# Organize source files by directory for better Xcode project structure

# Main source files
set(MAIN_SOURCES
    Main.cpp
    M1SystemHelperService.h
    M1SystemHelperService.cpp
)

# Common files
set(COMMON_SOURCES
    Common/Common.h
    Common/ServiceLocator.h
    Common/TypesForDataExchange.h
    Common/M1MemoryShare.h
    Common/M1MemoryShare.cpp
    Common/SharedPathUtils.h
    Common/SharedPathUtils.cpp
)

# Platform-specific files
if(APPLE)
    set(COMMON_SOURCES ${COMMON_SOURCES}
        Common/SharedPathUtils.mm
    )
endif()

# Core files
set(CORE_SOURCES
    Core/EventSystem.h
    Core/ConfigManager.h
    Core/ConfigManager.cpp
    Core/AudioStreaming.h
    Core/AudioStreaming.cpp
    Core/ExternalMixerProcessor.h
    Core/ExternalMixerProcessor.cpp
    Core/CoverageModel.h
    Core/CoverageModel.cpp
    Core/CaptureEngine.h
    Core/CaptureEngine.cpp
)

# Network files
set(NETWORK_SOURCES
    Network/OSCHandler.h
    Network/OSCHandler.cpp
)

# Manager files
set(MANAGER_SOURCES
    Managers/ClientManager.h
    Managers/ClientManager.cpp
    Managers/PluginManager.h
    Managers/PluginManager.cpp
    Managers/ServiceManager.h
    Managers/ServiceManager.cpp
    Managers/PannerTrackingManager.h
    Managers/PannerTrackingManager.cpp
    Managers/M1MemoryShareTracker.h
    Managers/M1MemoryShareTracker.cpp
    Managers/OSCPannerTracker.h
    Managers/OSCPannerTracker.cpp
)

# UI files
set(UI_SOURCES
    UI/SessionUI.h
    UI/SessionUI.cpp
)

# UI Component files
set(UI_COMPONENT_SOURCES
    UI/Components/InputTracklistComponent.h
    UI/Components/InputTracklistComponent.cpp
    UI/Components/InputMixerComponent.h
    UI/Components/InputMixerComponent.cpp
    UI/Components/InputPanelContainer.h
    UI/Components/InputPanelContainer.cpp
    UI/Components/TimelineComponent.h
    UI/Components/TimelineComponent.cpp
    UI/Components/CaptureTimelinePanel.h
    UI/Components/CaptureTimelinePanel.cpp
    UI/Components/Panner3DViewPanel.h
    UI/Components/Panner3DViewPanel.cpp
    UI/Components/MonitorPanel.h
    UI/Components/MonitorPanel.cpp
)

# Combine all source files
set(ALL_SOURCES
    ${MAIN_SOURCES}
    ${COMMON_SOURCES}
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${MANAGER_SOURCES}
    ${UI_SOURCES}
    ${UI_COMPONENT_SOURCES}
)

# Add all sources to the target
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${ALL_SOURCES})

# Source groups will be configured in the main CMakeLists.txt to avoid conflicts
