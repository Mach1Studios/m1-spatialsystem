file(GLOB source_files 
        Main.cpp
        M1SystemHelperService.h
        M1SystemHelperService.cpp
        Common/Common.h
        Common/ServiceLocator.h
        Core/EventSystem.h
        Core/ConfigManager.h
        Core/ConfigManager.cpp
        Network/OSCHandler.h
        Network/OSCHandler.cpp
        Managers/ClientManager.h
        Managers/ClientManager.cpp
        Managers/PluginManager.h
        Managers/PluginManager.cpp
        Managers/ServiceManager.h
        Managers/ServiceManager.cpp
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${source_files})
