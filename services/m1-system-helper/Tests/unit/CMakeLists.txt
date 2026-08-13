# Unit/regression tests for m1-system-helper internals.
# Enabled from the project root with -DBUILD_UNIT_TESTS=ON, run with ctest.

juce_add_console_app(m1-system-helper-tests
        PRODUCT_NAME "m1-system-helper-tests")

juce_generate_juce_header(m1-system-helper-tests)

target_sources(m1-system-helper-tests PRIVATE
        HelperServiceTests.cpp
        MemoryShareRingTests.cpp
        ExportEngineTests.cpp
        StorageGovernorTests.cpp
        SystemIntegrationTests.cpp
        TestStubs.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/ServiceManager.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/PluginManager.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/ClientManager.cpp
        # Real panner tracking stack: the integration tests exercise segment
        # discovery, connection, and per-instance data flow end to end.
        ${CMAKE_SOURCE_DIR}/Source/Managers/PannerTrackingManager.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/M1MemoryShareTracker.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/OSCPannerTracker.cpp
        ${CMAKE_SOURCE_DIR}/Source/Network/OSCHandler.cpp
        ${CMAKE_SOURCE_DIR}/Source/Common/M1MemoryShare.cpp
        ${CMAKE_SOURCE_DIR}/Source/Common/SharedPathUtils.cpp
        ${CMAKE_SOURCE_DIR}/Source/Core/CoverageModel.cpp
        ${CMAKE_SOURCE_DIR}/Source/Core/ExportEngine.cpp
        ${CMAKE_SOURCE_DIR}/Source/Core/StorageGovernor.cpp
        # Frozen-transport capture dedup: exercised by the capture integration test
        ${CMAKE_SOURCE_DIR}/Source/Core/CaptureEngine.cpp
        # Live mix render clock (P2): exercised by the MixBus integration test
        ${CMAKE_SOURCE_DIR}/Source/Core/MixEngine.cpp)

if(APPLE)
    target_sources(m1-system-helper-tests PRIVATE
            ${CMAKE_SOURCE_DIR}/Source/Common/SharedPathUtils.mm)
endif()

target_include_directories(m1-system-helper-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
        # Header-only usage via Core/ExternalMixerProcessor.h (never
        # constructed in tests; see TestStubs.cpp).
        ${CMAKE_SOURCE_DIR}/../../m1-player/Modules/m1-sdk/libmach1spatial/api_common/include
        ${CMAKE_SOURCE_DIR}/../../m1-player/Modules/m1-sdk/libmach1spatial/api_decode/include
        ${CMAKE_SOURCE_DIR}/../../m1-player/Modules/m1-sdk/libmach1spatial/api_encode/include
        ${CMAKE_SOURCE_DIR}/../../m1-player/Modules/m1-sdk/libmach1spatial/api_transcode/include
        ${CMAKE_SOURCE_DIR}/../../m1-player/Modules/m1-sdk/libmach1spatial/deps)

target_compile_definitions(m1-system-helper-tests PRIVATE
        JUCE_APPLICATION_NAME_STRING="m1-system-helper-tests"
        JUCE_APPLICATION_VERSION_STRING="1.0.0"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_compile_features(m1-system-helper-tests PRIVATE cxx_std_17)

target_link_libraries(m1-system-helper-tests PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_osc
        # juce::AudioBuffer is used in Common/M1MemoryShare.h declarations
        juce::juce_audio_basics
        # WAV writing/reading in Core/ExportEngine.cpp and its tests
        juce::juce_audio_formats
        # Offline re-encode of captured blocks in Core/ExportEngine.cpp
        M1Encode)

add_test(NAME m1-system-helper-unit COMMAND m1-system-helper-tests)
