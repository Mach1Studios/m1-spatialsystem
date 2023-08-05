/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/
#include <JuceHeader.h>

#ifndef JUCE_WINDOWS
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

int serverPort = 6345;
int watcherPort = 6346;

int activeClients = 1; // default with 1 so we do not auto shutdown on launch
juce::int64 pingTime = 0;

// run process M1-OrientationManager.exe from the same folder
juce::ChildProcess orientationManagerProcess;

void killProcessByName(const char *name)
{
    DBG("Killing M1-Orientation-Server...");
#ifdef JUCE_WINDOWS
	std::string command = "taskkill /IM " + std::string(name) + " /F";
	system(command.c_str());
#else
    std::string command = "pkill " + std::string(name);
    system(command.c_str());
#endif
}

void startOrientationManager()
{
    // Create a DatagramSocket to check the availability of port serverPort
    juce::DatagramSocket socket(false);
    socket.setEnablePortReuse(false);
    if (socket.bindToPort(serverPort)) {
        socket.shutdown();

        // TODO: make this file path search for `Mach1` dir
        // We will assume the folders are properly created during the installation step
    
        // Using common support files installation location
        juce::File m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

        // run process M1-OrientationManager
        std::string orientationManagerExe;
        if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
            // test for any windows OS
            orientationManagerExe = (m1SupportDirectory.getFullPathName()+"/Mach1/M1-OrientationManager.exe").toStdString();
        } else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
            // test for any mac OS
            orientationManagerExe = (m1SupportDirectory.getFullPathName()+"/Application Support/Mach1/M1-OrientationManager").toStdString();
        } else {
            orientationManagerExe = (m1SupportDirectory.getFullPathName()+"/Mach1/M1-OrientationManager").toStdString();
        }
        DBG("Starting M1-OrientationManager: " + orientationManagerExe);

        // TODO: Check if string path above is a juce::File safe directory
        //??????.setAsCurrentWorkingDirectory();

        juce::StringArray arguments;
        arguments.add(orientationManagerExe);
        arguments.add("--no-gui");

		if (orientationManagerProcess.start(arguments)) {
            DBG("Started M1-OrientationManager server");
        } else {
            // Failed to start the process
            DBG("Failed to start the M1-OrientationManager");
            exit(1);
        }
	}
}

//==============================================================================
class M1SystemWatcherApplication : public juce::JUCEApplication,
    private juce::Timer,
    public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
    juce::OSCReceiver receiver;

public:
    M1SystemWatcherApplication() {}
    ~M1SystemWatcherApplication() {}

    void oscMessageReceived(const juce::OSCMessage& message) override
    {
        if (message.getAddressPattern() == "/Mach1/ActiveClients") {
            activeClients = message[0].getInt32();
        } else {
            DBG("WARNING: Missing number of active clients in ping!");
        }
        pingTime = juce::Time::currentTimeMillis();
        DBG("Received message from " + message.getAddressPattern().toString() + ", with " + std::to_string(message[0].getInt32()) + " active clients");
    }

    void initialise(const juce::String&) override
    {
        juce::DatagramSocket socket(false); 
        socket.setEnablePortReuse(false);
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

            startOrientationManager();
            pingTime = juce::Time::currentTimeMillis();

            startTimer(100);
        }
    }

    void shutdown() override
    {
        stopTimer();
        receiver.removeListener(this);
        receiver.disconnect();
        DBG("M1-SystemWatcher is shutting down...");
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
        DBG("time: " + std::to_string(currentTime - pingTime));
        if (currentTime > pingTime && currentTime - pingTime > 1000) {
            pingTime = juce::Time::currentTimeMillis() + 10000; // wait 10 seconds

            killProcessByName("M1-OrientationManager");
            startOrientationManager();
        }
        
        // shutdown if active clients are 0 or less
        if (activeClients == 0) {
            shutdown();
            JUCEApplicationBase::quit();
        }
    }
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(M1SystemWatcherApplication)
