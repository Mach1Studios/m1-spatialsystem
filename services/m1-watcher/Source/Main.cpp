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

int serverPort, watcherPort;
int activeClients = 1; // default with 1 so we do not auto shutdown on launch
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
        watcherPort = mainVar["watcherPort"];
    }
    return true;
}

void killProcessByName(const char *name)
{
    std::string service_name;
    if (std::string(name) == "m1-orientationmanager") {
        // for launchctl we need to use the service name instead of the executable name
        service_name = "com.mach1.spatial.orientationmanager";
    }
    
	if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
		DBG("Killing " + std::string(name) + "...");

		std::string command;
		if ((juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_7) ||
			(juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_8) ||
			(juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX_10_9)) {
			// MacOS 10.7-10.9, launchd v1.0
			command = "launchctl stop /Library/LaunchDaemons/" + service_name + ".plist";
		}
		else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
			// All newer MacOS, launchd v2.0
			command = "launchctl kill 9 gui/$UID/" + service_name;
		}
		else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
			command = "taskkill /IM " + std::string(name) + " /F";
		}
		else {
			command = "pkill " + std::string(name);
		}
		DBG("Executing: " + command);
		system(command.c_str());
	}
	else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
		// Any windows OS
		// Windows Service Manager
		DBG("Stopping m1-orientationmanager service");
		int res = system("sc stop M1Service");
		if (res == 0) {
			DBG("Stopped m1-orientationmanager server");
		}
		else if (res == 1060) {
			DBG("not found service");
		}
		else if (res == 1062) {
			DBG("service not running");
		}
		else if (res == 1053) {
			DBG("start failed");
		}
		else if (res == 5) {
			DBG("need run from administartor");
		}
		else {
			DBG("some error");
		}
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
//            std::string load_command = "launchctl load -w /Library/LaunchDaemons/com.mach1.spatial.orientationmanager.plist";
//            DBG("Executing: " + load_command);
//            system(load_command.c_str());
            // start process m1-orientationmanager
            std::string command = "launchctl start com.mach1.spatial.orientationmanager";
            DBG("Executing: " + command);
            system(command.c_str());
        } else 
        if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
            // All newer MacOS, launchd v2.0
            // load process m1-orientationmanager
//            std::string load_command = "launchctl bootstrap gui/$UID /Library/LaunchDaemons/com.mach1.spatial.orientationmanager.plist";
//            DBG("Executing: " + load_command);
//            system(load_command.c_str());
            // start process m1-orientationmanager
            std::string command = "launchctl kickstart -p gui/$UID/com.mach1.spatial.orientationmanager";
            DBG("Executing: " + command);
            system(command.c_str());
        } else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
            // Any windows OS
            // Windows Service Manager
			DBG("Starting m1-orientationmanager service");
			int res = system("sc start M1Service");
			if (res == 0) {
				DBG("Started m1-orientationmanager server");
			}
			else if (res == 1060) {
				DBG("not found service");
				exit(1);
			}
			else if (res == 1053) {
				DBG("start failed");
			}
			else if (res == 5) {
				DBG("need run from administartor");
				exit(1);
			}
			else {
				DBG("some error");
			}
		} else {
            // TODO: factor out linux using systemd service
            orientationManagerExe = m1SupportDirectory.getChildFile("Mach1").getChildFile("m1-orientationmanager");
            juce::StringArray arguments;
            arguments.add(orientationManagerExe.getFullPathName().quoted());
            arguments.add("--no-gui");
            DBG("Starting m1-orientationmanager: " + orientationManagerExe.getFullPathName());
            if (orientationManagerProcess.start(arguments)) {
                DBG("Started m1-orientationmanager server");
            } else {
                // Failed to start the process
                DBG("Failed to start the m1-orientationmanager");
                exit(1);
            }
        }
	}
}

//==============================================================================
class M1SystemWatcherApplication : public juce::JUCEApplication,
    private juce::Timer,
    public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
    juce::OSCReceiver receiver;

    // TIMER = m1-orientationmanager ping timer
    // this is used to blindly check if the m1-orientationmanager has crashed and attempt to relaunch it
    
public:
    M1SystemWatcherApplication() {}
    ~M1SystemWatcherApplication() {}
    
	juce::int64 timeWhenWatcherLastSeenAClient = 0;
	juce::int64 timeWhenWeLastStartedAManager = -10000;
    bool clientRequestsServer = false;

    void oscMessageReceived(const juce::OSCMessage& message) override
    {
        // restart the ping timer because we received a ping
        pingTime = juce::Time::currentTimeMillis();

        if (message.getAddressPattern() == "/clientExists") {
            timeWhenWatcherLastSeenAClient = pingTime;
        }

        if (message.getAddressPattern() == "/clientRequestsServer") {
            clientRequestsServer = true;
        }
    }

    void initialise(const juce::String&) override
    {
        // We will assume the folders are properly created during the installation step
        juce::File settingsFile;
        // Using common support files installation location
        juce::File m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

        if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
            // test for any mac OS
            settingsFile = m1SupportDirectory.getChildFile("Application Support").getChildFile("Mach1");
        } else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
            // test for any windows OS
            settingsFile = m1SupportDirectory.getChildFile("Mach1");
        } else {
            settingsFile = m1SupportDirectory.getChildFile("Mach1");
        }
        settingsFile = settingsFile.getChildFile("settings.json");
        DBG("Opening settings file: " + settingsFile.getFullPathName().quoted());
        
        initFromSettings(settingsFile.getFullPathName().toStdString());
        
        juce::DatagramSocket socket(false); 
        socket.setEnablePortReuse(true);
        if (!socket.bindToPort(watcherPort)) {
            socket.shutdown();

            juce::String message = "Failed to bind to port " + std::to_string(watcherPort);
            juce::MessageManager::getInstance()->callFunctionOnMessageThread([](void* m) -> void* {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Alert Box",
                    static_cast<juce::String*>(m)->toRawUTF8(),
                    "OK",
                    nullptr,  // No associated component
                    juce::ModalCallbackFunction::create([m](int result) {
                        quit();
                    })
                );
                return nullptr;
                }, &message);
            DBG(message);
        }
        else {
            socket.shutdown();

            receiver.connect(watcherPort);
            receiver.addListener(this);

            shutdownCounterTime = juce::Time::currentTimeMillis();

            // starts the m1-orientationmanager ping timer
            startTimer(1000);
        }
    }

    void shutdown() override
    {
        stopTimer();
        receiver.removeListener(this);
        receiver.disconnect();
        DBG("m1-systemwatcher is shutting down...");
    }

    const juce::String getApplicationName() override
    {
        return ProjectInfo::projectName;
    }

    const juce::String getApplicationVersion() override
    {
        return ProjectInfo::versionString;
    }
    
    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }
    
    void timerCallback() override
    {
        juce::int64 currentTime = juce::Time::currentTimeMillis();

        if ((currentTime - timeWhenWatcherLastSeenAClient) > 20000) {
            // Killing server because we haven't seen a client in 20 seconds
            killProcessByName("m1-orientationmanager");
			timeWhenWatcherLastSeenAClient = currentTime;
        }
        
        if ((clientRequestsServer) && ((currentTime - timeWhenWeLastStartedAManager) > 10000)) {
            // Every 10 seconds we may restart or start a server if requested
			killProcessByName("m1-orientationmanager");
			juce::Thread::sleep(2000); // wait for stop
            startOrientationManager();
			juce::Thread::sleep(8000); // wait for start
			clientRequestsServer = false;
			timeWhenWeLastStartedAManager = currentTime;
		}
    }
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(M1SystemWatcherApplication)
