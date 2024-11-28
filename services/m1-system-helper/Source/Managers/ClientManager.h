/*
    ClientManager.h
    -----------------
    Manages clients connected to the system helper.
    Tracks client state and pings them to keep them alive.

    Designed for critical clients that are required for the system to function.
*/

#pragma once

#include "../Common/Common.h"
#include "../Core/EventSystem.h"

namespace Mach1 {

class ClientManager {
public:
    explicit ClientManager(std::shared_ptr<EventSystem> events);
    
    juce::Result addClient(const M1OrientationClientConnection& client);
    void removeClient(int port);
    bool updateClientTime(int port);
    void cleanupInactiveClients();
    
    std::vector<M1OrientationClientConnection> getClientsByType(ClientType type) const;
    const std::vector<M1OrientationClientConnection>& getAllClients() const;
    
    bool sendToAllClients(const juce::OSCMessage& msg);
    bool sendToClientsOfType(const juce::OSCMessage& msg, ClientType type);
    void activateClients();
    
    bool hasActiveClientOfType(int port, const juce::String& type) const;
    bool rotateMonitorToActive(int port);
    size_t getClientCount() const;

private:
    std::vector<M1OrientationClientConnection> clients;
    std::vector<M1OrientationClientConnection> monitors;
    std::vector<M1OrientationClientConnection> players;
    std::shared_ptr<EventSystem> eventSystem;
    
    juce::CriticalSection mutex;
};

} // namespace Mach1
