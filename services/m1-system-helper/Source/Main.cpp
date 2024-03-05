/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/
#include <JuceHeader.h>
#include <string>

#ifdef JUCE_WINDOWS

#else
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "m1_orientation_client/M1OrientationTypes.h"
#include "m1_orientation_client/M1OrientationSettings.h"

class M1RegisteredPlugin {
public:
    // At a minimum we should expect port and if applicable name
    // messages to ensure we do not delete this instance of a registered plugin
    int port;
    std::string name;
    bool isPannerPlugin = false;
    int input_mode;
    float azimuth, elevation, diverge; // values expected unnormalized
    float gain; // values expected unnormalized
    
    // pointer to store osc sender to communicate to registered plugin
    juce::OSCSender* messageSender;
};

// search plugin by registered port number
// TODO: potentially improve this with uuid concept
struct find_plugin {
    int port;
    find_plugin(int port) : port(port) {}
    bool operator () ( const M1RegisteredPlugin& p ) const
    {
        return p.port == port;
    }
};

struct M1OrientationClientConnection {
    int port;
    std::string type = "";
    bool active = false; // TODO: implement this to avoid using copies of specific type clients
    juce::int64 time;
};

struct find_client_by_type {
    std::string type;
    find_client_by_type(std::string type) : type(type) {}
    bool operator () ( const M1OrientationClientConnection& c ) const
    {
        return c.type == type;
    }
};

int serverPort, helperPort;
juce::int64 shutdownCounterTime = 0;
juce::int64 pingTime = 0;

// run process m1-orientationmanager.exe from the same folder
juce::ChildProcess orientationManagerProcess;

bool initFromSettings(std::string jsonSettingsFilePath) {
    juce::File settingsFile = juce::File(jsonSettingsFilePath);
    if (!settingsFile.exists()) {
        return false;
    } else {
        // Found the settings.json
        juce::var mainVar = juce::JSON::parse(juce::File(jsonSettingsFilePath));
        serverPort = mainVar["serverPort"];
        helperPort = mainVar["helperPort"];
    }
    return true;
}

#if defined(__APPLE__)
    uid_t uid = getuid();
    auto domain_target = (std::stringstream() << "gui/" << uid).str();
    auto service_target = (std::stringstream() << "gui/" << uid << "/com.mach1.spatial.orientationmanager").str();
    auto service_path = "/Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist";
#elif defined(_WIN32) || defined(_WIN64)
    auto domain_target = (std::stringstream() << "gui/").str();
    auto service_target = (std::stringstream() << "gui/" << "/com.mach1.spatial.orientationmanager").str();
    std::string service_path = "";
#endif

void killProcessByName(const char *name)
{
    std::string service_name;
    if (std::string(name) == "m1-orientationmanager") {
        // for launchctl we need to use the service name instead of the executable name
        service_name = "com.mach1.spatial.orientationmanager";
    }
    
    DBG("Killing " + std::string(name) + "...");
    std::string command;
    if ((juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_7) ||
        (juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_8) ||
        (juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_9)) {
        // MacOS 10.7-10.9, launchd v1.0
        command = std::string("launchctl stop ") + service_name;
        DBG("Executing: " + command);
        system(command.c_str());
    }
    else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
        // All newer MacOS, launchd v2.0
        command = std::string("launchctl kill 9 ") + service_target;
        DBG("Executing: " + command);
        system(command.c_str());
    }
    else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
        // Windows Service Manager
        DBG("Stopping m1-orientationmanager service");
        int res = system("sc stop M1-OrientationManager");
        if (res == 0) {
            DBG("Started m1-orientationmanager server");
        }
        else if (res == 1060) {
            DBG("Service not found");
            juce::JUCEApplicationBase::quit();
        }
        else if (res == 1053) {
            DBG("Failed to start service");
        }
        else if (res == 5) {
            DBG("Need to run as admin");
            juce::JUCEApplicationBase::quit();
        }
        else {
            DBG("Unknown Error");
        }
    }
    else {
        command = std::string("pkill ") + std::string(name);
        DBG("Executing: " + command);
        system(command.c_str());
    }
}

void startOrientationManager()
{
    // Create a DatagramSocket to check the availability of port serverPort
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    if (socket.bindToPort(serverPort)) {
        socket.shutdown();
        
        // We will assume the folders are properly created during the installation step
        // Using common support files installation location
        juce::File m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);
        juce::File orientationManagerExe; // for linux and win
        
        if ((juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_7) ||
            (juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_8) ||
            (juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_9)) {
            // MacOS 10.7-10.9, launchd v1.0

            // load process m1-orientationmanager
            /*
            std::string load_command = "launchctl load -w /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist";
            DBG("Executing: " + load_command);
            system(load_command.c_str());
            */

            // start process m1-orientationmanager
            // If service_path is already bootstrapped and disabled, launchctl bootstrap will fail until it is enabled again.
            // So we should enable it first, and then bootstrap and enable it.
            /*
            auto enable_command = std::string("/bin/launchctl enable ") + service_target;
            DBG("Executing: " + enable_command);
            system(enable_command.c_str());
            */
            std::string command = "/bin/launchctl start com.mach1.spatial.orientationmanager";
            DBG("Executing: " + command);
            system(command.c_str());
        } else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
            // All newer MacOS, launchd v2.0

            // load process m1-orientationmanager
            /*
            std::string load_command = "launchctl bootstrap gui/$UID /Library/LaunchAgents/com.mach1.spatial.orientationmanager.plist";
            DBG("Executing: " + load_command);
            system(load_command.c_str());
            */
            
            // start process m1-orientationmanager
            auto command = std::string("/bin/launchctl kickstart -p ") + service_target;
            DBG("Executing: " + command);
            system(command.c_str());
        } else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
            // Any windows OS
            // Windows Service Manager
            DBG("Starting m1-orientationmanager service");
            int res = system("sc start M1-OrientationManager");
            if (res == 0) {
                DBG("Started m1-orientationmanager server");
            }
            else if (res == 1060) {
                DBG("Service not found");
                juce::JUCEApplicationBase::quit();
            }
            else if (res == 1053) {
                DBG("Failed to start service");
            }
            else if (res == 5) {
                DBG("Need to run as admin");
                juce::JUCEApplicationBase::quit();
            }
            else {
                DBG("Unknown Error");
            }
        } else {
            // TODO: factor out linux using systemd service
            orientationManagerExe = m1SupportDirectory.getChildFile("Mach1").getChildFile("m1-orientationmanager");
            juce::StringArray arguments;
            arguments.add(orientationManagerExe.getFullPathName().quoted());
            DBG("Starting m1-orientationmanager: " + orientationManagerExe.getFullPathName());
            if (orientationManagerProcess.start(arguments)) {
                DBG("Started m1-orientationmanager server");
            } else {
                // Failed to start the process
                DBG("Failed to start the m1-orientationmanager");
                juce::JUCEApplicationBase::quit();
            }
        }
    }
}

//==============================================================================

bool send(const std::vector<M1OrientationClientConnection>& m1_clients, juce::OSCMessage& msg) {
    for (auto& m1_client : m1_clients) {
        juce::OSCSender sender;
        if (sender.connect("127.0.0.1", m1_client.port)) {
            if (sender.send(msg)) {
                return true;
                // TODO: we need some feedback because we can send messages without error even when there is no client receiving
            } else {
                // TODO: ERROR: Issue with sending OSC message on server side
                return false;
            }
        } else {
            // TODO: ERROR: Issue with sending OSC message on server side
            return false;
        }
    }
}

bool send(const std::vector<M1OrientationClientConnection>& m1_clients, std::string str) {
    juce::OSCMessage msg(str.c_str());
    return send(m1_clients, msg);
}

//==============================================================================
class M1SystemHelperService :
    public juce::Timer, 
    public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback> {

    juce::OSCReceiver receiver;

    std::vector<M1OrientationClientConnection> m1_clients;
    // TODO: refactor these so they arent copies of the `m1_clients`
    std::vector<M1OrientationClientConnection> players; // track all the player instances
    std::vector<M1OrientationClientConnection> monitors; // track all the monitor instances
    float master_yaw = 0; float master_pitch = 0; float master_roll = 0;
    int master_mode = 0;
    float prev_master_yaw = 0; float prev_master_pitch = 0; float prev_master_roll = 0;
    int prev_master_mode = 0;
    // Tracking for any plugin that does not need an m1_orientation_client but still needs feedback of orientation for UI purposes such as the M1-Panner plugin
    std::vector<M1RegisteredPlugin> registeredPlugins;
    bool bTimerActive = false;

    juce::int64 timeWhenHelperLastSeenAClient = 0;
    juce::int64 timeWhenWeLastStartedAManager = -10000;
    bool clientRequestsServer = false;

    void send_getConnectedClients(const std::vector<M1OrientationClientConnection>& clients) {
        juce::OSCMessage msg("/getConnectedClients");

        for (int i = 0; i < clients.size(); i++) {
            msg.addInt32(i); // client index
            msg.addInt32(clients[i].port);
            msg.addString(clients[i].type);
        }
        send(clients, msg);
    }

    void command_activateClients() {
        /// MONITORS
        if (monitors.size() > 0) {
            // send activate message to 1st index
            juce::OSCSender sender;
            if (sender.connect("127.0.0.1", monitors[0].port)) {
                juce::OSCMessage msg("/m1-activate-client");
                msg.addInt32(1); // send true / activate message
                sender.send(msg);
            }
            // from 2nd index onward send a de-activate message
            if (monitors.size() > 1) {
                for (int i = 1; i < monitors.size(); i++) {
                    if (sender.connect("127.0.0.1", monitors[i].port)) {
                        juce::OSCMessage msg("/m1-activate-client");
                        msg.addInt32(0); // send false / de-activate message
                        sender.send(msg);
                    }
                }
            }
        }
        /// PLAYERS
        if (players.size() > 0) {
            // send activate message to 1st index
            juce::OSCSender sender;
            if (sender.connect("127.0.0.1", players[0].port)) {
                juce::OSCMessage msg("/m1-activate-client");
                msg.addInt32(1); // send true / activate message
                sender.send(msg);
            }
            // from 2nd index onward send a de-activate message
            if (players.size() > 1) {
                for (int i = 1; i < players.size(); i++) {
                    if (sender.connect("127.0.0.1", players[i].port)) {
                        juce::OSCMessage msg("/m1-activate-client");
                        msg.addInt32(0); // send false / de-activate message
                        sender.send(msg);
                    }
                }
            }
        }
    }

    void oscMessageReceived(const juce::OSCMessage& message) override
    {
        // restart the ping timer because we received a ping
        pingTime = juce::Time::currentTimeMillis();

        if (message.getAddressPattern() == "/clientExists") {
            timeWhenHelperLastSeenAClient = pingTime;
        }
        else if (message.getAddressPattern() == "/clientRequestsServer") {
            clientRequestsServer = true;
        }
        else if (message.getAddressPattern() == "/m1-addClient") {
            // add client to clients list
            int port = message[0].getInt32();
            std::string type = message[1].getString().toStdString();
            bool found = false;

            for (auto& client : m1_clients) {
                if (client.port == port) {
                    client.time = juce::Time::currentTimeMillis();
                    found = true;
                }
            }

            if (!found) {
                M1OrientationClientConnection m1_client;
                m1_client.port = port;
                m1_client.type = type;
                m1_client.time = juce::Time::currentTimeMillis();
                m1_clients.push_back(m1_client);

                juce::OSCSender sender;
                if (sender.connect("127.0.0.1", port)) {
                    juce::OSCMessage msg("/connectedToServer");
                    if (m1_client.type == "monitor") {
                        monitors.push_back(m1_client);
                    }
                    if (m1_client.type == "player") {
                        players.push_back(m1_client);
                    }
                    command_activateClients();
                    msg.addInt32(m1_clients.size() - 1); // send ID for multiple clients to send commands
                    sender.send(msg);
                    DBG("Number of mach1 clients registered: " + std::to_string(m1_clients.size()) + " | monitors:" + std::to_string(monitors.size()) + " | players:" + std::to_string(players.size()));
                }
            }

            if (!bTimerActive && m1_clients.size() > 0) {
                startTimer(20);
                bTimerActive = true;
            }
        }
        else if (message.getAddressPattern() == "/m1-removeClient") {
            // remove client from clients list
            int search_port = message[0].getInt32();
            for (int index = 0; index < m1_clients.size(); index++) {
                if (m1_clients[index].port == search_port) {
                    if (m1_clients[index].type == "monitor") {
                        // if the removed type is monitor
                        for (int m_index = 0; m_index < monitors.size(); m_index++) {
                            // search monitors and remove the same matching port
                            if (monitors[m_index].port == search_port) {
                                monitors.erase(monitors.begin() + m_index);
                                command_activateClients();
                            }
                        }
                    }
                    if (m1_clients[index].type == "player") {
                        // if the removed type is player
                        for (int p_index = 0; p_index < players.size(); p_index++) {
                            // search players and remove the same matching port
                            if (players[p_index].port == search_port) {
                                players.erase(players.begin() + p_index);
                                command_activateClients();
                            }
                        }
                    }
                    // remove the client from the list
                    m1_clients.erase(m1_clients.begin() + index);
                    DBG("Number of mach1 clients registered: " + std::to_string(m1_clients.size()) + " | monitors:" + std::to_string(monitors.size()) + " | players:" + std::to_string(players.size()));
                }
            }
            send_getConnectedClients(m1_clients);

            if (m1_clients.size() == 0 && registeredPlugins.size() == 0) {
                stopTimer();
                bTimerActive = false;
            }
        }
        else if (message.getAddressPattern() == "/m1-status") {
            // checking on active clients connection to remove any dead instances
            int search_port = message[0].getInt32();
            bool bFound = false;
            for (int index = 0; index < m1_clients.size(); index++) {
                if (m1_clients[index].port == search_port) {
                    // udpate ping time
                    m1_clients[index].time = pingTime;
                    bFound = true;
                    juce::OSCSender sender;
                    if (sender.connect("127.0.0.1", m1_clients[index].port)) {
                        juce::OSCMessage msg("/m1-response");
                        sender.send(msg);
                    }
                }
            }
            if (!bFound) {
                // received status update from a client no longer in our list
                // prompting client to re-add itself
                juce::OSCSender sender;
                if (sender.connect("127.0.0.1", search_port)) {
                    juce::OSCMessage msg("/m1-reconnect-req");
                    sender.send(msg);
                    DBG("Attempting to re-register client");
                }
            }
        }
        else if (message.getAddressPattern() == "/setMonitoringMode") {
            // receiving updated monitoring mode or other misc settings for clients
            master_mode = message[0].getInt32();
            DBG("[Monitor] Mode: " + std::to_string(master_mode));
        }
        else if (message.getAddressPattern() == "/setPlayerYPR") {
            // Used for relaying the active player offset to calculate the orientation in the active monitor instance
            DBG("[Player] Yaw Offset: "+std::to_string(message[0].getFloat32()));
            if (monitors.size() > 0) {
                for (int index = 0; index < m1_clients.size(); index++) {
                    // TODO: refactor using callback to send a message when an update is received
                    if (m1_clients[index].type == "monitor") {
                        juce::OSCSender sender;
                        if (sender.connect("127.0.0.1", m1_clients[index].port)) {
                            juce::OSCMessage msg("/YPR-Offset");
                            msg.addFloat32(message[0].getFloat32());
                            msg.addFloat32(message[1].getFloat32());
                            //msg.addFloat32(message[2].getFloat32());
                            sender.send(msg);

                        }
                    }
                }
            }
        }
        else if (message.getAddressPattern() == "/setMasterYPR") {
            // Used for relaying a master calculated orientation to registered plugins that require this for GUI systems
            DBG("[Monitor] Yaw: "+std::to_string(message[0].getFloat32()));
            master_yaw = message[0].getFloat32();
            master_pitch = message[1].getFloat32();
            master_roll = message[2].getFloat32();
        }
        else if (message.getAddressPattern() == "/m1-register-plugin") {
            // registering new panner instance
            auto port = message[0].getInt32();
            // protect port creation to only messages from registered plugin (example: an m1-panner)
            if (std::find_if(registeredPlugins.begin(), registeredPlugins.end(), find_plugin(port)) == registeredPlugins.end()) {
                M1RegisteredPlugin foundPlugin;
                foundPlugin.port = port;
                foundPlugin.messageSender = new juce::OSCSender();
                foundPlugin.messageSender->connect("127.0.0.1", port); // connect to that newly discovered panner locally
                registeredPlugins.push_back(foundPlugin);
                DBG("Plugin registered: " + std::to_string(port));
            }
            else {
                DBG("Plugin port already registered: " + std::to_string(port));
            }
            if (!bTimerActive && registeredPlugins.size() > 0) {
                startTimer(20);
                bTimerActive = true;
            }
            else {
                if (registeredPlugins.size() == 0 && m1_clients.size() == 0) {
                    // TODO: setup logic for deleting from `registeredPlugins`
                    stopTimer();
                    bTimerActive = false;
                }
            }
        }
        else if (message.getAddressPattern() == "/panner-settings") {
            if (message.size() > 0) { // check message size
                int plugin_port = message[0].getInt32();
                if (message.size() >= 6) {
                    int input_mode = message[1].getInt32();
                    float azi = message[2].getFloat32();
                    float ele = message[3].getFloat32();
                    float div = message[4].getFloat32();
                    float gain = message[5].getFloat32();
                    DBG("[OSC] Panner: port=" + std::to_string(plugin_port) + ", in=" + std::to_string(input_mode) + ", az=" + std::to_string(azi) + ", el=" + std::to_string(ele) + ", di=" + std::to_string(div) + ", gain=" + std::to_string(gain));
                    // get optional messages
                    std::string displayName;
                    if (message.size() >= 7 && message[6].isString()) {
                        displayName = message[6].getString().toStdString();
                    }
                    juce::OSCColour colour;
                    if (message.size() >= 8 && message[7].isColour()) {
                        colour = message[7].getColour();
                    }
                    // Check if port matches expected registered-plugin port
                    if (registeredPlugins.size() > 0) {
                        auto it = std::find_if(registeredPlugins.begin(), registeredPlugins.end(), find_plugin(plugin_port));
                        auto index = it - registeredPlugins.begin(); // find the index from the found plugin
                        registeredPlugins[index].isPannerPlugin = true;
                        registeredPlugins[index].input_mode = input_mode;
                        registeredPlugins[index].azimuth = azi;
                        registeredPlugins[index].elevation = ele;
                        registeredPlugins[index].diverge = div;
                        registeredPlugins[index].gain = gain;
                        registeredPlugins[index].name = displayName;
                    
                        if (m1_clients.size() > 0) {
                            for (int client = 0; client < m1_clients.size(); client++) {
                                if (m1_clients[client].type == "player") {
                                    // relay the panner-settings message to the players
                                    juce::OSCSender sender;
                                    if (sender.connect("127.0.0.1", m1_clients[client].port)) {
                                        juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/panner-settings"));
                                        m.addInt32(registeredPlugins[index].port);
                                        m.addInt32(registeredPlugins[index].input_mode);
                                        m.addFloat32(registeredPlugins[index].azimuth);
                                        m.addFloat32(registeredPlugins[index].elevation);
                                        m.addFloat32(registeredPlugins[index].diverge);
                                        m.addFloat32(registeredPlugins[index].gain);
                                        // send optional messages
                                        m.addString(registeredPlugins[index].name);
                                        if (message.size() >= 8 && message[7].isColour()) {
                                            m.addColour(colour);
                                        }
                                        
                                        sender.send(m);
                                        DBG("[OSC] Panner: port=" + std::to_string(registeredPlugins[index].port) + " | Relayed to Player: " + std::to_string(m1_clients[client].port));
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else {
                // port not found, error here
            }
        }
        else {
            DBG("OSC Message not implemented: " + message.getAddressPattern().toString());
        }
    }

    void initialise()
    {
        // We will assume the folders are properly created during the installation step
        juce::File settingsFile;
        // Using common support files installation location
        juce::File m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

        if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
            // test for any mac OS
            settingsFile = m1SupportDirectory.getChildFile("Application Support").getChildFile("Mach1");
        }
        else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
            // test for any windows OS
            settingsFile = m1SupportDirectory.getChildFile("Mach1");
        }
        else {
            settingsFile = m1SupportDirectory.getChildFile("Mach1");
        }
        settingsFile = settingsFile.getChildFile("settings.json");
        DBG("Opening settings file: " + settingsFile.getFullPathName().quoted());

        initFromSettings(settingsFile.getFullPathName().toStdString());

        juce::DatagramSocket socket(false);
        socket.setEnablePortReuse(true);
        if (!socket.bindToPort(helperPort)) {
            socket.shutdown();

            juce::String message = "Failed to bind to port " + std::to_string(helperPort);
            DBG(message);
            juce::JUCEApplicationBase::quit();
        }
        else {
            socket.shutdown();

            receiver.connect(helperPort);
            receiver.addListener(this);

            shutdownCounterTime = juce::Time::currentTimeMillis();

            // starts the m1-orientationmanager ping timer
            startTimer(20);
        }
    }

    //==============================================================================
    void shutdown()
    {
        stopTimer();
        receiver.removeListener(this);
        receiver.disconnect();
        DBG("m1-system-helper is shutting down...");
    }

    //==============================================================================
    void timerCallback() override
    {
        juce::int64 currentTime = juce::Time::currentTimeMillis();

        if ((currentTime - timeWhenHelperLastSeenAClient) > 20000) {
            // Killing server because we haven't seen a client in 20 seconds
            killProcessByName("m1-orientationmanager");
            timeWhenHelperLastSeenAClient = currentTime;
        }
        
        // this is used for when a client has lost connection to the orientationmanager and signals here for the orientationmanager to be restarted (should ideally be avoided)
        if ((clientRequestsServer) && ((currentTime - timeWhenWeLastStartedAManager) > 10000)) {
            // Every 10 seconds we may restart or start a server if requested
            killProcessByName("m1-orientationmanager");
            juce::Thread::sleep(2000); // wait for stop
            startOrientationManager();
            juce::Thread::sleep(8000); // wait for start
            clientRequestsServer = false;
            timeWhenWeLastStartedAManager = currentTime;
        }

        if (registeredPlugins.size() > 0) {
            // check if any values have changed since last loop
            if (master_mode != prev_master_mode || master_yaw != prev_master_yaw || master_pitch != prev_master_pitch || master_roll != prev_master_roll) {
                for (auto &i: registeredPlugins) {
                    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/monitor-settings"));
                    m.addInt32(master_mode);
                    m.addFloat32(master_yaw);   // expected normalised
                    m.addFloat32(master_pitch); // expected normalised
                    m.addFloat32(master_roll);  // expected normalised
                    //m.addInt32(monitor_output_mode); // TODO: add the output configuration to sync plugins when requested
                    i.messageSender->send(m);
                }
                // update stored monitor values for checking changes in next loop
                prev_master_mode = master_mode;
                prev_master_yaw = master_yaw;
                prev_master_pitch = master_pitch;
                prev_master_roll = master_roll;
            }
            
            // send a ping to all registered plugins every 2 seconds
            if ((currentTime - timeWhenWeLastStartedAManager) > 2000) {
                for (auto &i: registeredPlugins) {
                    juce::OSCMessage m = juce::OSCMessage(juce::OSCAddressPattern("/m1-ping"));
                    i.messageSender->send(m);
                }
            }
            
            // TODO: check if any registered plugins closed
            //for (auto &i: registeredPlugins) {
            //}
        }

        // client specific messages and cleanup
        if (m1_clients.size() > 0) {
            for (int index = 0; index < m1_clients.size(); index++) {
                // cleanup any zombie clients
                if ((currentTime - m1_clients[index].time) > 10000) {

                    // if the removed type is monitor
                    if (m1_clients[index].type == "monitor") {
                        for (int m_index = 0; m_index < monitors.size(); m_index++) {
                            // search monitors and remove the same matching port
                            if (monitors[m_index].port == m1_clients[index].port) {
                                monitors.erase(monitors.begin() + m_index);
                                command_activateClients();
                            }
                        }
                    }

                    // if the removed type is player
                    if (m1_clients[index].type == "player") {
                        for (int p_index = 0; p_index < players.size(); p_index++) {
                            // search players and remove the same matching port
                            if (players[p_index].port == m1_clients[index].port) {
                                players.erase(players.begin() + p_index);
                                command_activateClients();
                            }
                        }
                    }

                    // remove the client from the list
                    m1_clients.erase(m1_clients.begin() + index);
                    DBG("Number of mach1 clients registered: " + std::to_string(m1_clients.size()) + " | monitors:" + std::to_string(monitors.size()) + " | players:" + std::to_string(players.size()));
                }
            }
        }
    }

public:
    static M1SystemHelperService& getInstance() {
        static M1SystemHelperService instance; // Singleton instance
        return instance;
    }

    void start() {
        initialise();

        while (!juce::MessageManager::getInstance()->hasStopMessageBeenSent()) {

            juce::Thread::sleep(100);
        }

        shutdown();
    }

};

#if !defined(GUI_APP) && defined(JUCE_WINDOWS)
#include <Windows.h>
#include <iostream>
#include <thread>

SERVICE_STATUS          g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE   g_StatusHandle = NULL;
HANDLE                  g_ServiceStopEvent = INVALID_HANDLE_VALUE;

// Service control handler function
VOID WINAPI ServiceControlHandler(DWORD dwControl) {
    switch (dwControl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        // Report the service status as STOP_PENDING.
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

        juce::MessageManager::getInstance()->stopDispatchLoop();

        // Signal the service to stop.
        SetEvent(g_ServiceStopEvent);
        return;

    default:
        break;
    }
}

// Service entry point
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler("M1-System-Helper", ServiceControlHandler);
    if (!g_StatusHandle) {
        std::cerr << "RegisterServiceCtrlHandler failed: " << GetLastError() << std::endl;
        return;
    }

    // Report the service status as RUNNING.
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Create an event to signal the service to stop.
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        // Handle error.
        return;
    }

    // Start a worker thread to perform the service's workand wait for the worker thread to complete.
    std::thread([&] { juce::MessageManager::getInstance()->runDispatchLoop(); }).detach();
    std::thread([]() { M1SystemHelperService::getInstance().start(); }).join();

    // Cleanup and report the service status as STOPPED.
    CloseHandle(g_ServiceStopEvent);
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwControlsAccepted = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

int main(int argc, char* argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        { "M1-System-Helper", (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        { NULL, NULL }
    };

    // This is where the service starts.
    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        std::cerr << "StartServiceCtrlDispatcher failed: " << GetLastError() << std::endl;
    }

    return 0;
}

#else

class M1SystemHelperApplication : public juce::JUCEApplication
{
public:
    //==============================================================================
    M1SystemHelperApplication() {
    }

    const juce::String getApplicationName() override { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override { return false; }

    //==============================================================================
    void initialise(const juce::String& commandLine) override
    {
        // This method is where you should put your application's initialisation code..
        std::thread([]() { M1SystemHelperService::getInstance().start(); }).detach();

#if defined(GUI_APP)
        mainWindow.reset(new MainWindow(getApplicationName()));
#else
        juce::MessageManager::getInstance()->runDispatchLoop();
#endif
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

#if defined(GUI_APP)
        mainWindow = nullptr; // (deletes our window)
#endif
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
        quit();
    }

#if defined(GUI_APP)
    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                juce::Desktop::getInstance().getDefaultLookAndFeel()
                .findColour(juce::ResizableWindow::backgroundColourId),
                DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
#endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };
#endif

private:
#if defined(GUI_APP)
    std::unique_ptr<MainWindow> mainWindow;
#endif

};

//==============================================================================
// This macro generates the main() routine that launches the app.
//==============================================================================

START_JUCE_APPLICATION(M1SystemHelperApplication)

#endif
