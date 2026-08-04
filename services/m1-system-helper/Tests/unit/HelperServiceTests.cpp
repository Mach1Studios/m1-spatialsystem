// Regression tests for m1-system-helper internals.
//
// Guards against field-reported failure modes:
//  1. ServiceManager::isOrientationManagerRunning() used a UDP port-bind
//     check against the orientation manager's TCP (HTTP) server. A UDP bind
//     succeeds even while a TCP listener owns the port, so the helper always
//     believed the manager was down and kept kill/restarting a healthy
//     service (dropping any connected head-tracking device).
//  2. Plugin registration used to broadcast monitor state to every registered
//     plugin, making session loads O(N^2). Registration replies must go only
//     to the plugin that registered (PluginManager::sendToPlugin).
//  3. Every "/setMasterYPR" from a streaming head-tracker was re-broadcast
//     immediately to every registered plugin (rate * N messages per second),
//     starving the host's message thread so panner editors never finished
//     opening (grey windows). The fan-out must be rate limited and deduped
//     (MonitorBroadcastThrottle).

#include <JuceHeader.h>
#include "Common/MonitorBroadcastThrottle.h"
#include "Managers/ClientManager.h"
#include "Managers/ServiceManager.h"
#include "Managers/PluginManager.h"
#include "Network/OSCHandler.h"
#include "Core/EventSystem.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cout << "FAILED: " #cond " (" << __FILE__ << ":" << __LINE__ << ")"  \
                      << std::endl;                                                   \
        }                                                                             \
    } while (0)

namespace {

void testOrientationManagerRunningDetection()
{
    const int testPort = 46345; // stay clear of the real port (6345)

    // Heap-allocate and deliberately leak: ~ServiceManager() issues real
    // launchctl/sc kill commands against any installed orientation manager,
    // which a unit test must never do on a developer machine.
    auto* serviceManager = new Mach1::ServiceManager(testPort);

    // Nothing listening: must report not running.
    CHECK(!serviceManager->isOrientationManagerRunning());

    // TCP listener present (like the orientation manager's HTTP server):
    // must report running.
    {
        juce::StreamingSocket tcpListener;
        CHECK(tcpListener.createListener(testPort, "127.0.0.1"));
        CHECK(serviceManager->isOrientationManagerRunning());
    }

    // Regression: a UDP socket on the same port must NOT count as running.
    // The old check ("can we bind UDP on this port?") conflated the two.
    {
        juce::DatagramSocket udpSocket(false);
        CHECK(udpSocket.bindToPort(testPort));
        CHECK(!serviceManager->isOrientationManagerRunning());
    }
}

class CountingOSCListener : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback> {
public:
    std::atomic<int> receivedCount { 0 };

    void oscMessageReceived(const juce::OSCMessage&) override
    {
        ++receivedCount;
    }
};

void testSendToPluginTargetsSinglePlugin()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PluginManager pluginManager(eventSystem);

    // Two fake "plugin" OSC receivers.
    juce::OSCReceiver receiverA, receiverB;
    CountingOSCListener listenerA, listenerB;

    int portA = 0, portB = 0;
    for (int candidate = 46500; candidate < 46600 && (portA == 0 || portB == 0); ++candidate) {
        if (portA == 0 && receiverA.connect(candidate)) {
            portA = candidate;
            continue;
        }
        if (portB == 0 && receiverB.connect(candidate)) {
            portB = candidate;
        }
    }
    CHECK(portA != 0 && portB != 0);

    receiverA.addListener(&listenerA);
    receiverB.addListener(&listenerB);

    Mach1::M1RegisteredPlugin pluginA;
    pluginA.port = portA;
    pluginA.time = juce::Time::currentTimeMillis();
    CHECK(pluginManager.registerPlugin(pluginA).wasOk());

    Mach1::M1RegisteredPlugin pluginB;
    pluginB.port = portB;
    pluginB.time = juce::Time::currentTimeMillis();
    CHECK(pluginManager.registerPlugin(pluginB).wasOk());

    // Simulate the registration reply the OSC handler sends: monitor settings
    // plus channel config, addressed to the registering plugin only.
    juce::OSCMessage settingsMsg("/monitor-settings");
    settingsMsg.addInt32(0);
    settingsMsg.addFloat32(0.0f);
    settingsMsg.addFloat32(0.0f);
    settingsMsg.addFloat32(0.0f);
    CHECK(pluginManager.sendToPlugin(portB, settingsMsg));

    juce::OSCMessage channelConfigMsg("/m1-channel-config");
    channelConfigMsg.addInt32(8);
    CHECK(pluginManager.sendToPlugin(portB, channelConfigMsg));

    // Unknown port must fail without sending anywhere.
    juce::OSCMessage strayMsg("/monitor-settings");
    strayMsg.addInt32(0);
    CHECK(!pluginManager.sendToPlugin(59999, strayMsg));

    // Give the UDP datagrams time to arrive.
    for (int i = 0; i < 100 && listenerB.receivedCount.load() < 2; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    CHECK(listenerB.receivedCount.load() == 2);
    // Regression: the non-registering plugin must receive nothing.
    CHECK(listenerA.receivedCount.load() == 0);

    receiverA.removeListener(&listenerA);
    receiverB.removeListener(&listenerB);
    receiverA.disconnect();
    receiverB.disconnect();
}

void testMonitorBroadcastThrottleRateLimitsStreams()
{
    Mach1::MonitorBroadcastThrottle throttle; // 50ms interval, 0.01 epsilon

    // Regression: a head-tracker streaming a new orientation every 5ms must
    // not produce one plugin broadcast per message.
    int sent = 0;
    std::int64_t now = 1000;
    for (int i = 0; i < 100; ++i, now += 5) {
        Mach1::MonitorBroadcastThrottle::Values v { 0, (float)i, 0.0f, 0.0f };
        if (throttle.shouldBroadcast(v, now))
            ++sent;
    }
    // 100 updates over 500ms at a 50ms interval: at most ~11 sends.
    CHECK(sent <= 11);
    CHECK(sent >= 9);

    // The newest suppressed value must flush once the interval has passed.
    Mach1::MonitorBroadcastThrottle::Values flushed;
    CHECK(throttle.takePendingFlush(now + 100, flushed));
    CHECK(flushed.yaw == 99.0f);

    // Nothing further pending.
    CHECK(!throttle.takePendingFlush(now + 200, flushed));
}

// Counts "/monitor-settings" deliveries to one fake panner instance.
struct MonitorSettingsCountingListener : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback> {
    std::atomic<int> monitorSettingsCount { 0 };

    void oscMessageReceived(const juce::OSCMessage& msg) override
    {
        if (msg.getAddressPattern() == "/monitor-settings")
            ++monitorSettingsCount;
    }
};

// End-to-end stress test against the real OSCHandler over real UDP: many
// registered panner instances while a monitor streams head-tracker
// orientation. Guards the failure mode where every "/setMasterYPR" was
// re-broadcast to every plugin (rate * N messages per second), which starved
// the host's message thread and left newly opened panner editors grey.
void testManyPannersUnderOrientationStorm()
{
    auto eventSystem = std::make_shared<Mach1::EventSystem>();
    Mach1::PluginManager pluginManager(eventSystem);
    Mach1::ClientManager clientManager(eventSystem);
    // Leaked deliberately: ~ServiceManager() issues real launchctl/sc kill
    // commands against any installed orientation manager.
    auto* serviceManager = new Mach1::ServiceManager(46346);

    Mach1::OSCHandler oscHandler(&clientManager, &pluginManager, serviceManager,
                                 /*pannerTrackingManager*/ nullptr, /*externalMixer*/ nullptr);

    int helperPort = 0;
    for (int candidate = 46800; candidate < 46900; ++candidate) {
        if (oscHandler.startListening(candidate)) {
            helperPort = candidate;
            break;
        }
    }
    CHECK(helperPort != 0);

    constexpr int kOpenEditorPanners = 24;
    constexpr int kClosedEditorPanners = 16;
    constexpr int kTotalPanners = kOpenEditorPanners + kClosedEditorPanners + 1; // last joins mid-storm

    std::vector<std::unique_ptr<juce::OSCReceiver>> receivers(kTotalPanners);
    std::vector<std::unique_ptr<MonitorSettingsCountingListener>> listeners(kTotalPanners);
    std::vector<int> ports(kTotalPanners, 0);

    int candidate = 47000;
    for (int i = 0; i < kTotalPanners; ++i) {
        receivers[i] = std::make_unique<juce::OSCReceiver>();
        listeners[i] = std::make_unique<MonitorSettingsCountingListener>();
        while (candidate < 48000 && !receivers[i]->connect(candidate))
            ++candidate;
        CHECK(candidate < 48000);
        ports[i] = candidate++;
        receivers[i]->addListener(listeners[i].get());
    }

    juce::OSCSender toHelper;
    CHECK(toHelper.connect("127.0.0.1", helperPort));

    // Register everything except the mid-storm panner.
    for (int i = 0; i < kTotalPanners - 1; ++i) {
        juce::OSCMessage reg("/m1-register-plugin");
        reg.addInt32(ports[i]);
        CHECK(toHelper.send(reg));
    }
    for (int i = 0; i < 200 && pluginManager.getPluginCount() < (size_t)(kTotalPanners - 1); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(pluginManager.getPluginCount() == (size_t)(kTotalPanners - 1));

    // The "closed editor" group reports its state, like real panners do in
    // every ping reply and on editor close.
    for (int i = kOpenEditorPanners; i < kOpenEditorPanners + kClosedEditorPanners; ++i) {
        juce::OSCMessage pulse("/m1-status-plugin");
        pulse.addInt32(ports[i]);
        pulse.addInt32(0);
        CHECK(toHelper.send(pulse));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // let registration replies land

    for (auto& listener : listeners)
        listener->monitorSettingsCount = 0;

    // Storm: a monitor with a streaming head-tracker, ~200 updates/sec.
    const auto stormStart = juce::Time::currentTimeMillis();
    for (int i = 0; i < 300; ++i) {
        juce::OSCMessage ypr("/setMasterYPR");
        ypr.addFloat32((float)(i % 360));
        ypr.addFloat32(0.0f);
        ypr.addFloat32(0.0f);
        toHelper.send(ypr);

        // A new panner instance must be able to join mid-storm (a user
        // opening a session / adding a track while the head-tracker runs).
        if (i == 150) {
            juce::OSCMessage reg("/m1-register-plugin");
            reg.addInt32(ports[kTotalPanners - 1]);
            CHECK(toHelper.send(reg));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const auto stormElapsedMs = juce::Time::currentTimeMillis() - stormStart;

    // Open-editor panners receive the stream, but rate limited (~20/sec) -
    // NOT one delivery per incoming update.
    const int maxExpectedPerPanner = (int)(stormElapsedMs / 50) + 3;
    for (int i = 0; i < kOpenEditorPanners; ++i) {
        const int received = listeners[i]->monitorSettingsCount.load();
        CHECK(received >= 5);
        CHECK(received <= maxExpectedPerPanner);
    }

    // Closed-editor panners receive none of the (UI-only) orientation stream.
    for (int i = kOpenEditorPanners; i < kOpenEditorPanners + kClosedEditorPanners; ++i)
        CHECK(listeners[i]->monitorSettingsCount.load() == 0);

    // The mid-storm registrant got its targeted registration reply.
    CHECK(pluginManager.getPluginCount() == (size_t)kTotalPanners);
    CHECK(listeners[kTotalPanners - 1]->monitorSettingsCount.load() >= 1);

    // Reopening an editor triggers an immediate targeted state refresh.
    {
        const int reopenIndex = kOpenEditorPanners; // first closed-editor panner
        juce::OSCMessage pulse("/m1-status-plugin");
        pulse.addInt32(ports[reopenIndex]);
        pulse.addInt32(1);
        CHECK(toHelper.send(pulse));

        for (int i = 0; i < 100 && listeners[reopenIndex]->monitorSettingsCount.load() < 1; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        CHECK(listeners[reopenIndex]->monitorSettingsCount.load() >= 1);
    }

    for (int i = 0; i < kTotalPanners; ++i) {
        receivers[i]->removeListener(listeners[i].get());
        receivers[i]->disconnect();
    }
}

void testMonitorBroadcastThrottleDedupesAndForces()
{
    Mach1::MonitorBroadcastThrottle throttle;

    Mach1::MonitorBroadcastThrottle::Values v { 0, 10.0f, 0.0f, 0.0f };
    CHECK(throttle.shouldBroadcast(v, 1000));

    // Identical values must not re-broadcast, even after the interval.
    CHECK(!throttle.shouldBroadcast(v, 2000));

    // A mode change with force must always go through immediately.
    Mach1::MonitorBroadcastThrottle::Values modeChange { 1, 10.0f, 0.0f, 0.0f };
    CHECK(throttle.shouldBroadcast(modeChange, 2001, true));

    // Sub-epsilon jitter must not re-broadcast.
    Mach1::MonitorBroadcastThrottle::Values jitter { 1, 10.005f, 0.0f, 0.0f };
    CHECK(!throttle.shouldBroadcast(jitter, 3000));
}

} // namespace

// Defined in MemoryShareRingTests.cpp; returns the number of failed checks.
int runMemoryShareRingTests();
// Defined in ExportEngineTests.cpp; returns the number of failed checks.
int runExportEngineTests();
// Defined in SystemIntegrationTests.cpp; returns the number of failed checks.
int runSystemIntegrationTests();

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testOrientationManagerRunningDetection();
    testSendToPluginTargetsSinglePlugin();
    testMonitorBroadcastThrottleRateLimitsStreams();
    testMonitorBroadcastThrottleDedupesAndForces();
    testManyPannersUnderOrientationStorm();

    failures += runMemoryShareRingTests();
    failures += runExportEngineTests();
    failures += runSystemIntegrationTests();

    if (failures == 0) {
        std::cout << "All m1-system-helper unit tests passed" << std::endl;
        return 0;
    }

    std::cout << failures << " check(s) failed" << std::endl;
    return 1;
}
