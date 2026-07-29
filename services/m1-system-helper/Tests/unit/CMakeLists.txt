# Unit/regression tests for m1-system-helper internals.
# Enabled from the project root with -DBUILD_UNIT_TESTS=ON, run with ctest.

juce_add_console_app(m1-system-helper-tests
        PRODUCT_NAME "m1-system-helper-tests")

juce_generate_juce_header(m1-system-helper-tests)

target_sources(m1-system-helper-tests PRIVATE
        HelperServiceTests.cpp
        TestStubs.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/ServiceManager.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/PluginManager.cpp
        ${CMAKE_SOURCE_DIR}/Source/Managers/ClientManager.cpp
        ${CMAKE_SOURCE_DIR}/Source/Network/OSCHandler.cpp)

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
        juce::juce_audio_basics)

add_test(NAME m1-system-helper-unit COMMAND m1-system-helper-tests)
