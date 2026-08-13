/*
    SystemIntegrationTests.cpp
    --------------------------
    End-to-end self-test of the panner -> helper -> monitor comm chain,
    without a DAW:

      1. A simulated M1-Panner creates a shared-memory segment in the real
         shared directory using the exact naming scheme the plugin uses
         (M1SpatialSystem_M1Panner_PID<pid>_PTR<addr>_T<ts>) and streams
         audio blocks with a parameter payload.
      2. The real PannerTrackingManager / M1MemoryShareTracker must discover
         the segment by scanning the filesystem, connect as a consumer, and
         surface the panner's parameters and audio activity.
      3. TWO simulated instances share this test's process id - exactly like
         two panner plugin instances inside one DAW process - and must be
         tracked as two distinct panners (regression: tracking used to key
         memory-share panners by PID only, collapsing them into one).
      4. Capture-style sequential draining must deliver each instance's own
         blocks through the tracker's registered consumer cursor.
      5. The real OSCHandler must deliver "/m1-streaming-panners <count>" to
         a registered monitor client over real UDP - the message the
         M1-Monitor UI uses to show its STREAMING badge.
*/

#include <JuceHeader.h>
#include "Common/M1MemoryShare.h"
#include "Common/SharedPathUtils.h"
#include "Common/TypesForDataExchange.h"
#include "Core/CaptureEngine.h"
#include "Core/EventSystem.h"
#include "Core/MixEngine.h"
#include "Managers/ClientManager.h"
#include "Managers/PannerTrackingManager.h"
#include "Managers/PluginManager.h"
#include "Managers/ServiceManager.h"
#include "Network/OSCHandler.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

int integrationFailures = 0;

#define ICHECK(cond, message)                                                   \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ++integrationFailures;                                              \
            std::cout << "FAILED: " << message << " (" << #cond << ") at "      \
                      << __FILE__ << ":" << __LINE__ << std::endl;              \
        }                                                                       \
    } while (false)

uint32_t currentPid()
{
#ifdef _WIN32
    return static_cast<uint32_t>(GetCurrentProcessId());
#else
    return static_cast<uint32_t>(getpid());
#endif
}

juce::File sharedMemoryDirectory()
{
    const auto dirs = Mach1::SharedPathUtils::getAllSharedDirectories();
    juce::File dir = dirs.empty()
        ? juce::File::getSpecialLocation(juce::File::tempDirectory)
        : juce::File(juce::String(dirs.front()));
    dir.createDirectory();
    return dir;
}

/**
 * Stand-in for one M1-Panner plugin instance: same segment naming, same
 * shared-memory writer, same parameter payload as the real plugin.
 */
struct SimulatedPanner
{
    juce::File file;
    juce::String segmentName;
    std::unique_ptr<M1MemoryShare> share;
    uintptr_t fakeAddress = 0;
    float azimuth = 0.0f;
    std::string displayName;
    int oscPort = 9998;
    uint64_t blocksWritten = 0;
    uint64_t timelineBlocks = 0;  // transport position in blocks (frozen while "stopped")
    int32_t controlRevision = 0; // highest helper control revision applied
    bool externalActive = true;  // the plugin's own streaming-state report

    SimulatedPanner(uintptr_t address, float azimuthDeg, const std::string& name, int port = 9998)
        : fakeAddress(address), azimuth(azimuthDeg), displayName(name), oscPort(port)
    {
        segmentName = juce::String("M1SpatialSystem_M1Panner_PID") + juce::String(static_cast<int>(currentPid()))
                    + "_PTR" + juce::String::toHexString(static_cast<juce::int64>(address))
                    + "_T" + juce::String(juce::Time::currentTimeMillis());
        file = sharedMemoryDirectory().getChildFile(segmentName + ".mem");

        share = std::make_unique<M1MemoryShare>(segmentName.toStdString(), 1024 * 1024,
                                                /*persistent*/ true, /*createMode*/ true,
                                                file.getFullPathName().toStdString());
        if (share->isValid())
            share->initializeForAudio(48000, 2, 480);
    }

    ~SimulatedPanner()
    {
        share.reset();
        file.deleteFile();
    }

    /** Writes one block like the real plugin's processBlock: sample-accurate
        timeline position, and a frozen (non-advancing) playhead when the
        transport is "stopped" - exactly what hosts like Reaper produce. */
    uint64_t writeBlock(bool isPlaying = true, bool advanceTransport = true)
    {
        juce::AudioBuffer<float> audio(2, 480);
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 480; ++s)
                audio.setSample(ch, s, 0.25f + 0.25f * ch);

        ParameterMap params;
        params.addFloat(M1SystemHelperParameterIDs::AZIMUTH, azimuth);
        params.addFloat(M1SystemHelperParameterIDs::ELEVATION, -10.0f);
        params.addFloat(M1SystemHelperParameterIDs::DIVERGE, 60.0f);
        params.addFloat(M1SystemHelperParameterIDs::GAIN, -3.0f);
        params.addInt(M1SystemHelperParameterIDs::INPUT_MODE, 1);
        params.addInt(M1SystemHelperParameterIDs::OUTPUT_MODE, 1);
        params.addBool(M1SystemHelperParameterIDs::AUTO_ORBIT, true);
        params.addBool(M1SystemHelperParameterIDs::EXTERNAL_ACTIVE, externalActive);
        params.addInt(M1SystemHelperParameterIDs::PORT, oscPort);
        params.addString(M1SystemHelperParameterIDs::DISPLAY_NAME, displayName);
        params.addInt(M1SystemHelperParameterIDs::CONTROL_REVISION, controlRevision);

        const int64_t playheadSamples = static_cast<int64_t>(timelineBlocks) * 480;
        const double playheadSeconds = static_cast<double>(playheadSamples) / 48000.0;
        const uint64_t id = share->writeAudioBufferWithGenericParameters(
            audio, params,
            static_cast<uint64_t>(juce::Time::currentTimeMillis()),
            playheadSeconds, isPlaying,
            /*blockWhenConsumersBehind*/ false, /*updateSource*/ 1, 48000,
            playheadSamples);
        if (id != 0)
        {
            ++blocksWritten;
            if (advanceTransport)
                ++timelineBlocks;
        }
        return id;
    }

    /** Writes a parameter-only keepalive (0 audio samples) like the real
        plugin's timer does when it is NOT streaming - e.g. sitting on a
        multichannel bus, where EXTERNAL_ACTIVE=false rides in the params. */
    uint64_t writeKeepalive()
    {
        juce::AudioBuffer<float> empty(2, 0);

        ParameterMap params;
        params.addFloat(M1SystemHelperParameterIDs::AZIMUTH, azimuth);
        params.addBool(M1SystemHelperParameterIDs::EXTERNAL_ACTIVE, externalActive);
        params.addInt(M1SystemHelperParameterIDs::PORT, oscPort);
        params.addString(M1SystemHelperParameterIDs::DISPLAY_NAME, displayName);

        return share->writeAudioBufferWithGenericParameters(
            empty, params,
            static_cast<uint64_t>(juce::Time::currentTimeMillis()),
            0.0, /*isPlaying*/ false,
            /*blockWhenConsumersBehind*/ false, /*updateSource*/ 1, 48000,
            /*playheadPositionSamples*/ -1);
    }

    /** Replaces the segment at the SAME path with a fresh inode and an empty
        consumer table - what old plugin builds did on every bus-width
        round-trip, and what a crashed/reloaded plugin still does. Any mapping
        a reader holds afterwards points at the orphaned old inode. */
    void recreateSegment()
    {
        share.reset();
        share = std::make_unique<M1MemoryShare>(segmentName.toStdString(), 1024 * 1024,
                                                /*persistent*/ true, /*createMode*/ true,
                                                file.getFullPathName().toStdString());
        if (share->isValid())
            share->initializeForAudio(48000, 2, 480);
    }

    /** Mirrors the plugin's processExternalControlMessages(): drain the
        control ring, apply edits, remember the newest revision for echoing. */
    int drainControls()
    {
        M1MemoryShare::ControlMessage message;
        int applied = 0;
        while (share->readControlMessage(message))
        {
            ++applied;
            if (message.parameterID == M1SystemHelperParameterIDs::AZIMUTH)
                azimuth = message.floatValue;
            controlRevision = std::max(controlRevision, message.intValue);
        }
        return applied;
    }
};

// Finds this test's simulated panners among whatever the tracker discovered
// (a developer machine may have a real DAW with real panners running).
std::vector<Mach1::PannerInfo> findOwnPanners(const std::vector<Mach1::PannerInfo>& panners)
{
    std::vector<Mach1::PannerInfo> own;
    for (const auto& panner : panners)
        if (panner.processId == currentPid() && panner.isMemoryShareBased)
            own.push_back(panner);
    return own;
}

//==============================================================================
void testPannerDiscoveryAndTwoInstanceDataShare()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    // Two instances inside ONE process, like two panner tracks in a DAW
    SimulatedPanner pannerA(0xA11CE000, 10.0f, "Integration Panner A");
    SimulatedPanner pannerB(0xB0BB0000, 75.0f, "Integration Panner B");
    ICHECK(pannerA.share->isValid() && pannerB.share->isValid(),
           "simulated panner segments created in the shared directory");

    // Drive discovery the same way the helper service does (periodic update()
    // calls) while the "plugins" keep streaming blocks.
    std::vector<Mach1::PannerInfo> own;
    const auto deadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < deadline)
    {
        pannerA.writeBlock();
        pannerB.writeBlock();
        manager.update();

        own = findOwnPanners(manager.getActivePanners());
        if (own.size() >= 2)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    ICHECK(own.size() == 2,
           "both same-process panner instances tracked separately (got "
           + std::to_string(own.size()) + ")");

    if (own.size() == 2)
    {
        // Identify by memory address and verify per-instance parameter flow
        const Mach1::PannerInfo* a = nullptr;
        const Mach1::PannerInfo* b = nullptr;
        for (const auto& panner : own)
        {
            if (panner.memoryAddress == pannerA.fakeAddress) a = &panner;
            if (panner.memoryAddress == pannerB.fakeAddress) b = &panner;
        }
        ICHECK(a != nullptr && b != nullptr, "instances distinguished by memory address");

        if (a != nullptr && b != nullptr)
        {
            ICHECK(std::abs(a->azimuth - 10.0f) < 0.01f, "instance A azimuth arrived via memory share");
            ICHECK(std::abs(b->azimuth - 75.0f) < 0.01f, "instance B azimuth arrived via memory share");
            ICHECK(a->name == "Integration Panner A", "instance A display name arrived");
            ICHECK(b->name == "Integration Panner B", "instance B display name arrived");
            ICHECK(a->channels == 2 && a->sampleRate == 48000, "audio format propagated");
            ICHECK(a->isPlaying, "playback state propagated");
        }
    }

    // Capture-style sequential drain: each instance's blocks must be readable
    // through the tracker's registered consumer, independently per instance.
    auto* tracker = manager.getMemoryShareTracker();
    ICHECK(tracker != nullptr, "memory share tracker available");
    if (tracker != nullptr && own.size() == 2)
    {
        const uint32_t consumerId = tracker->getConsumerId();

        auto drainAndCheck = [&](SimulatedPanner& simulated, const char* label)
        {
            auto* memPanner = tracker->findPanner(currentPid(), simulated.fakeAddress);
            auto share = memPanner != nullptr ? memPanner->getShare() : nullptr;
            ICHECK(memPanner != nullptr && share != nullptr,
                   std::string(label) + ": tracker connected to segment");
            if (share == nullptr)
                return;

            // Skip anything already published, then stream fresh blocks
            M1MemoryShare::SharedBlock block;
            while (share->readNextBlockForConsumer(consumerId, block)) {}

            const int freshBlocks = 5;
            for (int i = 0; i < freshBlocks; ++i)
                ICHECK(simulated.writeBlock() != 0, std::string(label) + ": block write accepted");

            int drained = 0;
            float lastAzimuth = -999.0f;
            while (share->readNextBlockForConsumer(consumerId, block))
            {
                ++drained;
                lastAzimuth = block.parameters.getFloat(M1SystemHelperParameterIDs::AZIMUTH, -999.0f);
            }
            ICHECK(drained == freshBlocks,
                   std::string(label) + ": capture drain sees every block (got "
                   + std::to_string(drained) + ")");
            ICHECK(std::abs(lastAzimuth - simulated.azimuth) < 0.01f,
                   std::string(label) + ": drained blocks carry the instance's own parameters");
        };

        drainAndCheck(pannerA, "panner A");
        drainAndCheck(pannerB, "panner B");
    }

    manager.stop();
}

//==============================================================================
// One plugin instance is visible through BOTH tracking paths: it registers
// over OSC (with its receiver port) and it streams audio via memory share
// (whose parameter payload carries that same port). The tracker must list it
// ONCE, keyed to the memory-share identity - regression for the helper UI
// showing every streaming panner twice ("OSC" + "MemoryShare" rows).
void testOscAndMemoryShareDedupe()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PluginManager pluginManager(eventSystem);
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.initializeOSCTracker(&pluginManager); // OSC path needs the plugin manager
    manager.start();

    const int port = 48777; // matches nothing else on a dev machine

    // Register through the PluginManager - the same path the real
    // "/m1-register-plugin" OSC handler uses (the OSC tracker mirrors the
    // plugin manager's list on every update).
    Mach1::M1RegisteredPlugin plugin;
    plugin.port = port;
    plugin.name = "Dedupe Panner";
    plugin.isPannerPlugin = true;
    plugin.azimuth = 20.0f;
    plugin.time = juce::Time::currentTimeMillis();
    ICHECK(pluginManager.registerPlugin(plugin).wasOk(), "OSC registration accepted");

    const auto countEntriesForPort = [&manager, port]()
    {
        int count = 0;
        for (const auto& panner : manager.getActivePanners())
            if (panner.port == port)
                ++count;
        return count;
    };

    // The manager rescans on an interval, so poll until the OSC entry lands
    const auto oscDeadline = juce::Time::currentTimeMillis() + 5000;
    while (juce::Time::currentTimeMillis() < oscDeadline && countEntriesForPort() == 0)
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    ICHECK(countEntriesForPort() == 1, "panner tracked once after OSC-only registration");

    // Same instance starts streaming: memory-share segment carrying the port
    SimulatedPanner simulated(0xDED00000, 20.0f, "Dedupe Panner", port);
    ICHECK(simulated.share->isValid(), "dedupe panner segment created");

    bool promoted = false;
    const auto deadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < deadline)
    {
        simulated.writeBlock();
        // Keep the OSC side alive too, like the plugin's ping replies
        pluginManager.updatePluginTime(port);
        manager.update();

        for (const auto& panner : manager.getActivePanners())
        {
            if (panner.port == port && panner.isMemoryShareBased
                && panner.memoryAddress == simulated.fakeAddress)
            {
                promoted = true;
                break;
            }
        }
        if (promoted)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    ICHECK(promoted, "entry promoted to memory-share identity (capture needs the address)");
    ICHECK(countEntriesForPort() == 1,
           "panner still tracked once after memory share appears (got "
           + std::to_string(countEntriesForPort()) + ")");

    if (promoted)
    {
        for (const auto& panner : manager.getActivePanners())
        {
            if (panner.port != port)
                continue;
            ICHECK(panner.channels == 2, "audio format flows into the merged entry");
            ICHECK(panner.inputMode == 1, "input mode (stereo) flows into the merged entry");
        }
    }

    manager.stop();
}

//==============================================================================
// The discovery race seen in live DAW testing: the memory-share segment is
// discovered while its parameter payload still carries port 0 (the plugin has
// not bound its OSC receiver yet), so one entry is created with no port. The
// OSC registration then creates a second entry. When the port finally shows
// up in the block parameters, the tracker must converge to ONE entry - the
// regression was two rows that both said "streaming" forever.
void testLatePortConvergesToOneEntry()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PluginManager pluginManager(eventSystem);
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.initializeOSCTracker(&pluginManager);
    manager.start();

    const int port = 48778;

    // 1. Memory share appears FIRST, with port 0 in its parameters
    SimulatedPanner simulated(0x1A7E0000, 40.0f, "Late Port Panner", /*port*/ 0);
    ICHECK(simulated.share->isValid(), "late-port panner segment created");

    const auto countOwnEntries = [&manager, &simulated, port]()
    {
        int count = 0;
        for (const auto& panner : manager.getActivePanners())
        {
            const bool viaMemory = panner.isMemoryShareBased
                && panner.processId == currentPid()
                && panner.memoryAddress == simulated.fakeAddress;
            const bool viaPort = panner.port == port;
            if (viaMemory || viaPort)
                ++count;
        }
        return count;
    };

    const auto memDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < memDeadline && countOwnEntries() == 0)
    {
        simulated.writeBlock();
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
    ICHECK(countOwnEntries() == 1, "memory-share entry tracked (port still unknown)");

    // 2. OSC registration lands while the memory-share entry has no port
    Mach1::M1RegisteredPlugin plugin;
    plugin.port = port;
    plugin.name = "Late Port Panner";
    plugin.isPannerPlugin = true;
    plugin.time = juce::Time::currentTimeMillis();
    ICHECK(pluginManager.registerPlugin(plugin).wasOk(), "OSC registration accepted");

    // 3. The plugin's blocks start carrying the bound port
    simulated.oscPort = port;

    bool converged = false;
    const auto deadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < deadline)
    {
        simulated.writeBlock();
        pluginManager.updatePluginTime(port);
        manager.update();

        if (countOwnEntries() == 1)
        {
            // Must be the memory-share row AND know its port now
            for (const auto& panner : manager.getActivePanners())
            {
                if (panner.isMemoryShareBased
                    && panner.memoryAddress == simulated.fakeAddress
                    && panner.port == port)
                {
                    converged = true;
                    break;
                }
            }
        }
        if (converged)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    ICHECK(converged,
           "one entry with memory-share identity and the late-arriving port (got "
           + std::to_string(countOwnEntries()) + " entries)");

    // Stays converged over further updates (no oscillation back to two rows)
    for (int i = 0; i < 10; ++i)
    {
        simulated.writeBlock();
        pluginManager.updatePluginTime(port);
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    ICHECK(countOwnEntries() == 1,
           "entry count stays at one after convergence (got "
           + std::to_string(countOwnEntries()) + ")");

    manager.stop();
}

//==============================================================================
class StreamingStatusListener : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
    std::atomic<int> receivedCount { 0 };
    std::atomic<int> lastCount { -1 };

    void oscMessageReceived(const juce::OSCMessage& msg) override
    {
        if (msg.getAddressPattern() == "/m1-streaming-panners" && msg.size() >= 1 && msg[0].isInt32())
        {
            lastCount = msg[0].getInt32();
            ++receivedCount;
        }
    }
};

void testStreamingStatusReachesMonitorClients()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    // One streaming "panner" so the broadcast carries a non-zero count.
    // Streamed continuously: the count now comes from the MixEngine's view of
    // feeds with fresh AUDIO (parameter-only keepalives must not count).
    SimulatedPanner panner(0xCAFE0000, 33.0f, "Integration Panner C");
    ICHECK(panner.share->isValid(), "simulated panner segment created");

    std::atomic<bool> keepStreaming { true };
    std::thread streamThread([&] {
        while (keepStreaming.load())
        {
            panner.writeBlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    const auto discoveryDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < discoveryDeadline
           && findOwnPanners(manager.getActivePanners()).empty())
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
    ICHECK(!findOwnPanners(manager.getActivePanners()).empty(), "panner discovered before broadcast test");

    // The broadcast's streaming count is sourced from the MixEngine
    const juce::String busName = "M1SpatialSystem_MixBus_status" + juce::String(static_cast<int>(currentPid()));
    Mach1::MixEngine engine(manager, busName);
    engine.setOutputFormat(8);
    engine.startEngine();

    const auto feedDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < feedDeadline
           && engine.getStatus().streamingFeeds < 1)
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ICHECK(engine.getStatus().streamingFeeds >= 1, "mix engine sees the streaming feed");

    Mach1::PluginManager pluginManager(eventSystem);
    Mach1::ClientManager clientManager(eventSystem);
    // Leaked deliberately: ~ServiceManager() issues real launchctl/sc kill
    // commands against any installed orientation manager.
    auto* serviceManager = new Mach1::ServiceManager(46347);

    Mach1::OSCHandler oscHandler(&clientManager, &pluginManager, serviceManager,
                                 &manager, /*externalMixer*/ nullptr, &engine);

    int helperPort = 0;
    for (int candidate = 48400; candidate < 48500; ++candidate)
    {
        if (oscHandler.startListening(candidate))
        {
            helperPort = candidate;
            break;
        }
    }
    ICHECK(helperPort != 0, "test helper OSC port bound");

    // Fake M1-Monitor client
    juce::OSCReceiver monitorReceiver;
    StreamingStatusListener monitorListener;
    int monitorPort = 0;
    for (int candidate = 48500; candidate < 48600; ++candidate)
    {
        if (monitorReceiver.connect(candidate))
        {
            monitorPort = candidate;
            break;
        }
    }
    ICHECK(monitorPort != 0, "fake monitor port bound");
    monitorReceiver.addListener(&monitorListener);

    // Register exactly like MonitorOSC::connectToHelper() does
    juce::OSCSender toHelper;
    ICHECK(toHelper.connect("127.0.0.1", helperPort), "sender connected to helper");
    juce::OSCMessage addClient("/m1-addClient");
    addClient.addInt32(monitorPort);
    addClient.addString("monitor");
    ICHECK(toHelper.send(addClient), "monitor registration sent");

    // Wait for registration to land, then trigger the keepalive broadcast
    for (int i = 0; i < 100 && clientManager.getClientCount() < 1; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ICHECK(clientManager.getClientCount() >= 1, "monitor client registered with helper");

    manager.update(); // refresh the streaming panner list
    oscHandler.broadcastStreamingStatusToMonitors();

    for (int i = 0; i < 200 && monitorListener.receivedCount.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ICHECK(monitorListener.receivedCount.load() >= 1,
           "monitor received /m1-streaming-panners heartbeat");
    ICHECK(monitorListener.lastCount.load() >= 1,
           "streaming panner count includes the active simulated panner (got "
           + std::to_string(monitorListener.lastCount.load()) + ")");

    keepStreaming.store(false);
    streamThread.join();
    monitorReceiver.removeListener(&monitorListener);
    monitorReceiver.disconnect();
    oscHandler.stopListening();
    engine.stopEngine();
    manager.stop();
    sharedMemoryDirectory().getChildFile(busName + ".mem").deleteFile();
}

//==============================================================================
// P2 live-mix chain: simulated panners stream into the tracker, the MixEngine
// render clock encodes/sums them and publishes the MixBus segment, and a
// simulated M1-Monitor opens that segment and reads the multichannel mix -
// exactly what the plugin's MixBusReader does inside processBlock.
void testMixEngineEndToEnd()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    // Isolated bus segment so a live helper on this machine can't interfere
    const juce::String busName = "M1SpatialSystem_MixBus_test" + juce::String(static_cast<int>(currentPid()));
    const juce::File busFile = sharedMemoryDirectory().getChildFile(busName + ".mem");

    Mach1::MixEngine engine(manager, busName);
    engine.setOutputFormat(8);
    engine.startEngine();

    // Two "plugin instances" in this process, azimuth 0 and 90 degrees
    SimulatedPanner pannerA(0x3117A000, 0.0f, "Mix Panner A");
    SimulatedPanner pannerB(0x3117B000, 90.0f, "Mix Panner B");
    ICHECK(pannerA.share->isValid() && pannerB.share->isValid(), "mix test segments created");

    std::atomic<bool> keepStreaming { true };
    std::atomic<bool> streamB { true };
    std::thread streamThread([&] {
        while (keepStreaming.load())
        {
            pannerA.writeBlock();
            if (streamB.load())
                pannerB.writeBlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 480 samples @ 48k
        }
    });

    // Drive discovery like the helper service timer does
    const auto discoveryDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < discoveryDeadline
           && findOwnPanners(manager.getActivePanners()).size() < 2)
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ICHECK(findOwnPanners(manager.getActivePanners()).size() >= 2, "panners discovered for mix test");

    // Wait for the engine to attach its feeds and publish bus blocks
    const auto busDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < busDeadline && !engine.getStatus().busActive)
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    const auto status = engine.getStatus();
    ICHECK(status.busActive, "MixBus segment is being written");
    ICHECK(status.busChannels == 8, "bus format follows the configured output format");
    ICHECK(status.sampleRate == 48000, "bus sample rate follows the panner feeds");
    ICHECK(status.liveFeeds >= 2, "both feeds counted as live (got "
           + std::to_string(status.liveFeeds) + ")");

    // Simulated monitor: open the segment, register a consumer, read blocks
    M1MemoryShare monitorSide(busName.toStdString(), 16 * 1024 * 1024,
                              /*persistent*/ true, /*createMode*/ false,
                              busFile.getFullPathName().toStdString());
    ICHECK(monitorSide.isValid() && monitorSide.isRingConfigured(),
           "monitor-side MixBus segment opened");

    if (monitorSide.isValid() && monitorSide.isRingConfigured())
    {
        const uint32_t monitorConsumer = 0x4D4F4E01;
        ICHECK(monitorSide.registerConsumer(monitorConsumer), "monitor consumer registered");

        int blocksRead = 0;
        float peakSample = 0.0f;
        int lastChannels = 0;
        const auto readDeadline = juce::Time::currentTimeMillis() + 6000;
        while (juce::Time::currentTimeMillis() < readDeadline && blocksRead < 20)
        {
            M1MemoryShare::SharedBlock block;
            while (monitorSide.readNextBlockForConsumer(monitorConsumer, block))
            {
                ++blocksRead;
                lastChannels = block.audio.getNumChannels();
                for (int ch = 0; ch < block.audio.getNumChannels(); ++ch)
                    peakSample = juce::jmax(peakSample,
                                            block.audio.getMagnitude(ch, 0, block.audio.getNumSamples()));
            }
            manager.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        ICHECK(blocksRead >= 20, "monitor read a steady stream of mix blocks (got "
               + std::to_string(blocksRead) + ")");
        ICHECK(lastChannels == 8, "mix blocks carry the 8-channel spatial bus (got "
               + std::to_string(lastChannels) + ")");
        ICHECK(peakSample > 0.01f, "mix audio is non-silent (encoded panner content arrived)");
    }

    ICHECK(engine.getFeedPeak(currentPid(), pannerA.fakeAddress) > 0.01f,
           "per-feed input meter is live for panner A");
    float masterPeak = 0.0f;
    for (int ch = 0; ch < 8; ++ch)
        masterPeak = juce::jmax(masterPeak, engine.getMasterPeak(ch));
    ICHECK(masterPeak > 0.01f, "master bus meters are live");

    // Stall behavior: panner B stops delivering; the bus must keep flowing,
    // paced by panner A alone (a silent/dead track must never mute the mix).
    streamB.store(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // > STALL_TIMEOUT_MS

    const uint64_t publishedBeforeStall = engine.getStatus().blocksPublished;
    const auto stallDeadline = juce::Time::currentTimeMillis() + 4000;
    while (juce::Time::currentTimeMillis() < stallDeadline
           && engine.getStatus().blocksPublished < publishedBeforeStall + 20)
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(engine.getStatus().blocksPublished >= publishedBeforeStall + 20,
           "bus keeps publishing while one panner is stalled (published "
           + std::to_string(engine.getStatus().blocksPublished - publishedBeforeStall)
           + " blocks after stall)");

    // Regression (crash): switching the spatial bus format while feeds are
    // live used to blow up inside Mach1Encode::encodeBuffer - its gain-
    // smoothing history keeps the OLD output channel count per point and is
    // only re-seeded when the POINT count changes, so an 8 -> 14 switch read
    // past the old inner vectors. The engine now rebuilds a feed's encoder on
    // any mode change instead.
    streamB.store(true);
    engine.setOutputFormat(14);

    const uint64_t publishedBeforeFormatChange = engine.getStatus().blocksPublished;
    const auto formatDeadline = juce::Time::currentTimeMillis() + 6000;
    while (juce::Time::currentTimeMillis() < formatDeadline
           && !(engine.getStatus().busChannels == 14
                && engine.getStatus().blocksPublished >= publishedBeforeFormatChange + 20))
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(engine.getStatus().busChannels == 14,
           "bus format switched to 14 channels mid-stream");
    ICHECK(engine.getStatus().blocksPublished >= publishedBeforeFormatChange + 20,
           "bus keeps publishing after a live 8 -> 14 format change (published "
           + std::to_string(engine.getStatus().blocksPublished - publishedBeforeFormatChange)
           + " blocks)");

    keepStreaming.store(false);
    streamThread.join();
    engine.stopEngine();
    manager.stop();
    busFile.deleteFile();
}

//==============================================================================
// P3 two-way control chain: an edit made in the helper's UI travels through
// sendParameterUpdate -> shared-memory control ring -> panner apply -> revision
// echo -> pending-overlay clear. Verifies:
//   1. edits reach the panner with the well-known parameter ID (regression:
//      IDs used to be hashString()'d and never matched, so panners silently
//      dropped every edit)
//   2. the tracked value shows the edit immediately (pending overlay), before
//      the panner has applied it - no UI snap-back
//   3. once the panner echoes the revision, the overlay clears and panner-side
//      changes flow through again
//   4. an edit the panner never applies expires and the tracked value reverts
void testControlRoundTrip()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    SimulatedPanner panner(0xC057C000, 0.0f, "Control Panner");
    ICHECK(panner.share->isValid(), "control test segment created");

    // Discover the panner
    std::vector<Mach1::PannerInfo> own;
    const auto discoveryDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < discoveryDeadline)
    {
        panner.writeBlock();
        manager.update();
        own = findOwnPanners(manager.getActivePanners());
        if (!own.empty() && own.front().memoryAddress == panner.fakeAddress)
            break;
        own.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ICHECK(!own.empty(), "panner discovered for control test");
    if (own.empty())
    {
        manager.stop();
        return;
    }

    // Unknown parameters must be refused, not sent with a garbage ID
    ICHECK(!manager.sendParameterUpdate(own.front(), "notARealParameter", 1.0f),
           "unknown parameter names are refused");

    // --- Edit azimuth from the helper side ---------------------------------
    ICHECK(manager.sendParameterUpdate(own.front(), "azimuth", 42.0f),
           "azimuth control message accepted");

    // Pending overlay: tracked value shows the edit right away, even though
    // the panner is still reporting azimuth 0 (it hasn't drained the ring yet)
    panner.writeBlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    manager.update();
    own = findOwnPanners(manager.getActivePanners());
    ICHECK(!own.empty() && std::abs(own.front().azimuth - 42.0f) < 0.01f,
           "pending overlay shows the edited azimuth before panner acknowledgment (got "
           + std::to_string(own.empty() ? -999.0f : own.front().azimuth) + ")");

    // --- Panner applies the edit and echoes the revision -------------------
    const int appliedCount = panner.drainControls();
    ICHECK(appliedCount >= 1, "panner received the control message");
    ICHECK(std::abs(panner.azimuth - 42.0f) < 0.01f,
           "panner applied azimuth 42 (well-known parameter ID matched)");
    ICHECK(panner.controlRevision >= 1, "revision travelled inside the control message");

    // After the echo, panner-side changes must win again (overlay cleared by
    // acknowledgment). The panner moves to 55; tracking must follow.
    panner.azimuth = 55.0f;
    const auto echoStartMs = juce::Time::currentTimeMillis();
    bool followed = false;
    while (juce::Time::currentTimeMillis() - echoStartMs < 3000)
    {
        panner.writeBlock();
        manager.update();
        own = findOwnPanners(manager.getActivePanners());
        if (!own.empty() && std::abs(own.front().azimuth - 55.0f) < 0.01f)
        {
            followed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(followed, "after revision echo, panner-side azimuth changes flow through again");
    ICHECK(!own.empty() && own.front().controlRevision >= 1,
           "tracked info carries the echoed control revision");

    // --- Unacknowledged edits must expire -----------------------------------
    // The panner never drains this one (plugin gone unresponsive / stale ring)
    ICHECK(manager.sendParameterUpdate(own.front(), "elevation", 33.0f),
           "elevation control message accepted");

    panner.writeBlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    manager.update();
    own = findOwnPanners(manager.getActivePanners());
    ICHECK(!own.empty() && std::abs(own.front().elevation - 33.0f) < 0.01f,
           "pending overlay shows the edited elevation");

    // Wait past PENDING_EDIT_TIMEOUT_MS while the panner keeps streaming its
    // real elevation (-10); the overlay must give up and stop lying.
    const auto expiryDeadline = juce::Time::currentTimeMillis() + 4000;
    bool reverted = false;
    while (juce::Time::currentTimeMillis() < expiryDeadline)
    {
        panner.writeBlock();
        manager.update();
        own = findOwnPanners(manager.getActivePanners());
        if (!own.empty() && std::abs(own.front().elevation - (-10.0f)) < 0.01f)
        {
            reverted = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ICHECK(reverted, "unacknowledged edit expired and tracked elevation reverted to panner value");

    manager.stop();
}

//==============================================================================
// P4 user-facing toggle: the helper's "Enable Audio Streaming" switch must
// (a) be pushed to every panner on registration, (b) broadcast changes to
// registered panners, (c) start/stop the live MixEngine, and (d) invoke the
// persistence hook exactly on user-driven changes.
class RendererToggleListener : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
    std::atomic<int> lastEnabledValue { -1 };
    std::atomic<int> receivedCount { 0 };

    void oscMessageReceived(const juce::OSCMessage& msg) override
    {
        if (msg.getAddressPattern() == "/m1-external-renderer-enabled" && msg.size() >= 1 && msg[0].isInt32())
        {
            lastEnabledValue = msg[0].getInt32();
            ++receivedCount;
        }
    }
};

void testExternalRendererToggle()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    Mach1::PluginManager pluginManager(eventSystem);
    Mach1::ClientManager clientManager(eventSystem);
    // Leaked deliberately: ~ServiceManager() issues real launchctl/sc kill
    // commands against any installed orientation manager.
    auto* serviceManager = new Mach1::ServiceManager(46348);

    const juce::String busName = "M1SpatialSystem_MixBus_toggle" + juce::String(static_cast<int>(currentPid()));
    Mach1::MixEngine engine(manager, busName);

    Mach1::OSCHandler oscHandler(&clientManager, &pluginManager, serviceManager,
                                 &manager, /*externalMixer*/ nullptr, &engine);

    std::atomic<int> persistCalls { 0 };
    std::atomic<int> lastPersistedValue { -1 };
    oscHandler.onExternalRendererChanged = [&](bool enabled) {
        ++persistCalls;
        lastPersistedValue = enabled ? 1 : 0;
    };

    int helperPort = 0;
    for (int candidate = 48600; candidate < 48700; ++candidate)
    {
        if (oscHandler.startListening(candidate))
        {
            helperPort = candidate;
            break;
        }
    }
    ICHECK(helperPort != 0, "toggle test helper OSC port bound");

    // Fake panner plugin endpoint
    juce::OSCReceiver pluginReceiver;
    RendererToggleListener pluginListener;
    int pluginPort = 0;
    for (int candidate = 48700; candidate < 48800; ++candidate)
    {
        if (pluginReceiver.connect(candidate))
        {
            pluginPort = candidate;
            break;
        }
    }
    ICHECK(pluginPort != 0, "fake plugin port bound");
    pluginReceiver.addListener(&pluginListener);

    ICHECK(oscHandler.isExternalRendererEnabled(), "renderer enabled by default");
    engine.startEngine();
    ICHECK(engine.isThreadRunning(), "mix engine running while enabled");

    // Register like the real plugin: the reply must carry the current toggle
    // so late-loading panners never stream into a disabled helper.
    juce::OSCSender toHelper;
    ICHECK(toHelper.connect("127.0.0.1", helperPort), "sender connected to helper");
    juce::OSCMessage registerMsg("/m1-register-plugin");
    registerMsg.addInt32(pluginPort);
    ICHECK(toHelper.send(registerMsg), "plugin registration sent");

    for (int i = 0; i < 200 && pluginListener.receivedCount.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ICHECK(pluginListener.receivedCount.load() >= 1, "registration reply carried renderer state");
    ICHECK(pluginListener.lastEnabledValue.load() == 1, "renderer reported enabled on registration");

    // Keep the fake plugin's registration fresh while we wait: the helper's
    // keepalive timer prunes plugins that stop pulsing (real plugins reply to
    // /m1-ping), and a pruned plugin would silently miss the broadcasts.
    const auto waitForEnabledValue = [&](int expected) {
        for (int i = 0; i < 200 && pluginListener.lastEnabledValue.load() != expected; ++i)
        {
            pluginManager.updatePluginTime(pluginPort);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return pluginListener.lastEnabledValue.load() == expected;
    };

    // Disable: plugin gets the broadcast, engine stops, persistence fires once
    oscHandler.setExternalRendererEnabled(false);
    ICHECK(!oscHandler.isExternalRendererEnabled(), "disabled state stored");
    ICHECK(!engine.isThreadRunning(), "mix engine stopped when disabled");
    ICHECK(waitForEnabledValue(0), "plugin told streaming is now disabled");
    ICHECK(persistCalls.load() == 1 && lastPersistedValue.load() == 0,
           "persistence hook fired once with the new value");

    // Redundant set must not re-fire persistence
    oscHandler.setExternalRendererEnabled(false);
    ICHECK(persistCalls.load() == 1, "redundant toggle does not re-persist");

    // Re-enable: engine restarts, plugin notified
    oscHandler.setExternalRendererEnabled(true);
    ICHECK(engine.isThreadRunning(), "mix engine restarted when re-enabled");
    ICHECK(waitForEnabledValue(1), "plugin told streaming is re-enabled");
    ICHECK(persistCalls.load() == 2 && lastPersistedValue.load() == 1,
           "persistence hook fired for re-enable");

    pluginReceiver.removeListener(&pluginListener);
    pluginReceiver.disconnect();
    oscHandler.stopListening();
    engine.stopEngine();
    manager.stop();
    sharedMemoryDirectory().getChildFile(busName + ".mem").deleteFile();
}

//==============================================================================
// Hosts like Reaper keep calling processBlock with a frozen playhead while the
// transport is stopped. Those blocks used to be captured over and over at the
// same timeline position (bloating sessions with idle audio that could
// overwrite real takes), and the seconds->samples truncation used to shift
// block positions by +/-1 sample, putting an audible click at nearly every
// block boundary of an export. This drives the real discovery -> capture
// pipeline and checks both fixes.
void testCaptureSkipsFrozenTransportAndKeepsSampleAccuracy()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    SimulatedPanner panner(0xF7023000, 15.0f, "Frozen Transport Panner");
    ICHECK(panner.share->isValid(), "simulated panner segment created");

    const auto discoveryDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < discoveryDeadline
           && findOwnPanners(manager.getActivePanners()).empty())
    {
        panner.writeBlock(/*isPlaying*/ false, /*advanceTransport*/ false);
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ICHECK(!findOwnPanners(manager.getActivePanners()).empty(),
           "panner discovered before capture test");

    Mach1::CaptureEngine engine(manager);
    const juce::File captureRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("m1-capture-freeze-" + juce::String(juce::Time::currentTimeMillis()));
    ICHECK(engine.startCapture("FreezeSession", captureRoot), "capture started");

    // The capture directory name is a stable instance id; locate ours through
    // the human-readable name.txt the engine writes next to the chunk data.
    auto ourChunkFile = [&]() -> juce::File
    {
        const juce::File sessionDir = captureRoot.getChildFile("FreezeSession");
        for (const auto& entry : juce::RangedDirectoryIterator(sessionDir, false, "*",
                                                               juce::File::findDirectories))
        {
            const juce::File nameFile = entry.getFile().getChildFile("name.txt");
            if (nameFile.existsAsFile()
                && nameFile.loadFileAsString().trim() == juce::String(panner.displayName))
                return entry.getFile().getChildFile("chunks.bin");
        }
        return {};
    };

    auto readChunkPositions = [](const juce::File& chunkFile) -> std::vector<juce::int64>
    {
        std::vector<juce::int64> positions;
        juce::FileInputStream in(chunkFile);
        if (!in.openedOk())
            return positions;
        while (in.getPosition() + static_cast<juce::int64>(Mach1::ChunkHeader::SIZE) <= in.getTotalLength())
        {
            Mach1::ChunkHeader header;
            if (in.read(&header, static_cast<int>(Mach1::ChunkHeader::SIZE))
                    != static_cast<int>(Mach1::ChunkHeader::SIZE)
                || header.magic != 0x4D314348)
                break;
            positions.push_back(header.startSample);
            in.setPosition(in.getPosition() + header.stateSize + header.audioDataSize);
        }
        return positions;
    };

    // Phase 1: transport stopped, playhead frozen at block 0; blocks keep flowing
    const auto frozenDeadline = juce::Time::currentTimeMillis() + 3000;
    while (juce::Time::currentTimeMillis() < frozenDeadline)
    {
        panner.writeBlock(/*isPlaying*/ false, /*advanceTransport*/ false);
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    const juce::File chunkFile = ourChunkFile();
    ICHECK(chunkFile.existsAsFile(), "capture wrote a chunk file for the panner");
    const int frozenChunks = static_cast<int>(readChunkPositions(chunkFile).size());
    ICHECK(frozenChunks == 1, "frozen transport captured exactly one chunk, not one per block");

    // Phase 2: transport rolling - every block must be captured, back to back
    constexpr int kPlayingBlocks = 30;
    for (int i = 0; i < kPlayingBlocks; ++i)
    {
        panner.writeBlock(/*isPlaying*/ true, /*advanceTransport*/ true);
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Let the capture thread drain the tail
    std::vector<juce::int64> positions;
    const auto drainDeadline = juce::Time::currentTimeMillis() + 6000;
    while (juce::Time::currentTimeMillis() < drainDeadline)
    {
        positions = readChunkPositions(chunkFile);
        if (static_cast<int>(positions.size()) >= frozenChunks + kPlayingBlocks)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ICHECK(static_cast<int>(positions.size()) == frozenChunks + kPlayingBlocks,
           "every rolling-transport block captured exactly once");

    // Skip the frozen->rolling seam (both start at position 0 by design);
    // afterwards every chunk must sit exactly one block after the previous.
    bool contiguous = true;
    for (size_t i = 2; i < positions.size(); ++i)
        if (positions[i] != positions[i - 1] + 480)
            contiguous = false;
    ICHECK(contiguous, "rolling-transport chunks are sample-contiguous (no +/-1 seams)");

    engine.stopCapture();
    manager.stop();
    captureRoot.deleteRecursively();
}

//==============================================================================
// A plugin that recreates its .mem segment at the same path (a crashed or
// reloaded plugin, or a bus-width round-trip on old builds) leaves the
// helper's mapping pointing at the orphaned inode: the NEW segment has an
// empty consumer table, so the panner shows "STREAMING (NO CONSUMER)" and no
// audio is captured. The tracker must notice the silent-mapping/fresh-file
// combination, remap, and re-register its consumer within a few seconds.
void testSegmentRecreateRemapsConsumer()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    SimulatedPanner panner(0x5EC0FDE0, 42.0f, "Remap Panner");
    ICHECK(panner.share->isValid(), "simulated panner segment created");

    // Phase 1: normal discovery - stream until the helper registers a consumer
    bool discovered = false;
    const auto discoveryDeadline = juce::Time::currentTimeMillis() + 10000;
    while (juce::Time::currentTimeMillis() < discoveryDeadline)
    {
        panner.writeBlock();
        manager.update();
        if (panner.share->getConsumerCount() > 0)
        {
            discovered = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(discovered, "helper registered a consumer on the initial segment");
    if (!discovered)
    {
        manager.stop();
        return;
    }

    // Phase 2: replace the segment behind the helper's back
    panner.recreateSegment();
    ICHECK(panner.share->isValid(), "recreated segment is valid");
    ICHECK(panner.share->getConsumerCount() == 0, "recreated segment starts with no consumers");
    panner.azimuth = 137.0f; // must arrive through the NEW mapping to count

    // Keep streaming; bump the file mtime like the real plugin does (its async
    // mtime updater rides the message thread, which this test doesn't pump).
    bool remapped = false;
    const auto remapDeadline = juce::Time::currentTimeMillis() + 15000;
    while (juce::Time::currentTimeMillis() < remapDeadline)
    {
        panner.writeBlock();
        panner.file.setLastModificationTime(juce::Time::getCurrentTime());
        manager.update();
        if (panner.share->getConsumerCount() > 0)
        {
            remapped = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(remapped, "tracker re-registered its consumer on the recreated segment");

    // The remapped connection must actually carry data: the tracker's view of
    // the panner has to converge on the post-recreate azimuth.
    bool dataFlows = false;
    const auto dataDeadline = juce::Time::currentTimeMillis() + 5000;
    while (juce::Time::currentTimeMillis() < dataDeadline && !dataFlows)
    {
        panner.writeBlock();
        manager.update();
        for (const auto& info : findOwnPanners(manager.getActivePanners()))
            if (info.memoryAddress == panner.fakeAddress && std::abs(info.azimuth - 137.0f) < 0.01f)
                dataFlows = true;
        if (!dataFlows)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(dataFlows, "post-remap parameters flow through the new mapping");

    manager.stop();
}

//==============================================================================
// Streaming-state reporting: the helper must be able to tell apart
//   (a) a panner streaming audio            (EXTERNAL_ACTIVE=true, blocks flow)
//   (b) a panner alive on a multichannel bus (EXTERNAL_ACTIVE=false keepalives)
//   (c) a panner that stopped writing entirely (msSinceLastBlock grows stale)
// so the UI can say "seen but no audio received" instead of a frozen
// "Streaming" label (the segment file existing keeps lastUpdateTime fresh).
void testStreamingStateReporting()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PannerTrackingManager manager(eventSystem);
    manager.start();

    SimulatedPanner panner(0x57A7E000, 42.0f, "State Panner");
    ICHECK(panner.share->isValid(), "state test segment created");

    auto findSelf = [&]() -> std::optional<Mach1::PannerInfo> {
        for (const auto& info : findOwnPanners(manager.getActivePanners()))
            if (info.memoryAddress == panner.fakeAddress)
                return info;
        return std::nullopt;
    };

    // (a) streaming: audio blocks with EXTERNAL_ACTIVE=true
    bool streamingSeen = false;
    const auto discoveryDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < discoveryDeadline && !streamingSeen)
    {
        panner.writeBlock();
        manager.update();
        if (const auto info = findSelf())
            streamingSeen = info->externalStreamingActive
                            && info->msSinceLastBlock >= 0 && info->msSinceLastBlock < 4000;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(streamingSeen, "streaming panner reports external-active with fresh blocks");

    // (b) multichannel: parameter-only keepalives with EXTERNAL_ACTIVE=false
    panner.externalActive = false;
    bool nativeSeen = false;
    const auto nativeDeadline = juce::Time::currentTimeMillis() + 8000;
    while (juce::Time::currentTimeMillis() < nativeDeadline && !nativeSeen)
    {
        panner.writeKeepalive();
        manager.update();
        if (const auto info = findSelf())
            nativeSeen = !info->externalStreamingActive
                         && info->msSinceLastBlock >= 0 && info->msSinceLastBlock < 4000;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ICHECK(nativeSeen, "keepalive-only panner reports external-inactive but alive");

    // (c) silent: no writes at all - freshness must go stale even though the
    // segment file still exists (which keeps lastUpdateTime bumping)
    bool staleSeen = false;
    const auto staleDeadline = juce::Time::currentTimeMillis() + 10000;
    while (juce::Time::currentTimeMillis() < staleDeadline && !staleSeen)
    {
        manager.update();
        if (const auto info = findSelf())
            staleSeen = info->msSinceLastBlock > 4000;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    ICHECK(staleSeen, "silent panner's block freshness goes stale within the timeout");

    manager.stop();
}

} // namespace

// Called from HelperServiceTests.cpp's main(); returns failed check count.
int runSystemIntegrationTests()
{
    testPannerDiscoveryAndTwoInstanceDataShare();
    testOscAndMemoryShareDedupe();
    testLatePortConvergesToOneEntry();
    testStreamingStatusReachesMonitorClients();
    testMixEngineEndToEnd();
    testControlRoundTrip();
    testExternalRendererToggle();
    testCaptureSkipsFrozenTransportAndKeepsSampleAccuracy();
    testSegmentRecreateRemapsConsumer();
    testStreamingStateReporting();

    if (integrationFailures == 0)
        std::cout << "All system integration tests passed" << std::endl;

    return integrationFailures;
}
