// Regression tests for m1-system-helper internals.
//
// Guards against two field-reported failure modes:
//  1. ServiceManager::isOrientationManagerRunning() used a UDP port-bind
//     check against the orientation manager's TCP (HTTP) server. A UDP bind
//     succeeds even while a TCP listener owns the port, so the helper always
//     believed the manager was down and kept kill/restarting a healthy
//     service (dropping any connected head-tracking device).
//  2. Plugin registration used to broadcast monitor state to every registered
//     plugin, making session loads O(N^2). Registration replies must go only
//     to the plugin that registered (PluginManager::sendToPlugin).

#include <JuceHeader.h>
#include "Managers/ServiceManager.h"
#include "Managers/PluginManager.h"
#include "Core/EventSystem.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

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

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testOrientationManagerRunningDetection();
    testSendToPluginTargetsSinglePlugin();

    if (failures == 0) {
        std::cout << "All m1-system-helper unit tests passed" << std::endl;
        return 0;
    }

    std::cout << failures << " check(s) failed" << std::endl;
    return 1;
}
