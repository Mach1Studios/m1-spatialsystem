cmake_minimum_required(VERSION 3.16)
file(STRINGS VERSION CURRENT_VERSION)
project(m1-system-helper VERSION ${CURRENT_VERSION})

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

#First, we'll add the CMake folder, incase we'll need to find_package later:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

### IDE Generator pre-config ###
# Xcode: Disable automatic build scheme generation globally.
#        Instead, we explicitely specify which targets require schemes.
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
# Enable to build universal binaries on macOS, increasing build time
# This only affects local builds, GitHub actions always builds Universals
set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum macOS suppored" FORCE)
set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET[arch=arm64] "11.0" CACHE STRING "arm 64 minimum deployment target" FORCE)
# IDEs:  Enable grouping of source files into folders in IDEs.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# IDEs:  Create a folder in the IDE with the JUCE Module code.
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

#static linking in Windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Adds the Mach1 math
# (Please ensure this is added before the `m1_orientation_client` JUCE module)
add_subdirectory(../../m1-orientationmanager/Modules/m1_orientation_client/libs/m1-mathematics "${CMAKE_CURRENT_BINARY_DIR}/m1-mathematics_build")

# add JUCE
add_subdirectory(../../m1-player/JUCE "${CMAKE_CURRENT_BINARY_DIR}/JUCE_build")
add_subdirectory(../../m1-orientationmanager/Modules "${CMAKE_CURRENT_BINARY_DIR}/Modules_build")

# Mach1 Spatial SDK
# Use these options to block dependencies we do not need
set(BUILD_M1DECODEPOSITIONAL OFF)
set(BUILD_MACH1SPATIAL_COMBINED_LIB OFF) # We do not need the combined lib
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/../../m1-player/Modules/m1-sdk/cmake) # Prepend the global CMake modules directory
add_subdirectory(../../m1-player/Modules/m1-sdk/libmach1spatial "${CMAKE_CURRENT_BINARY_DIR}/m1sdk_build")

# PLUGIN/APP NAME & ID  (for dynamic naming schemes)
set(CUSTOM_BUNDLE_ID "com.mach1.spatial.helper")
set(CUSTOM_APP_CODE "M1SW")

# add the plugin targets
juce_add_console_app(${CMAKE_PROJECT_NAME}
                VERSION ${CURRENT_VERSION}
                COMPANY_NAME "Mach1"
                COMPANY_WEBSITE "https://mach1.tech"
                COMPANY_EMAIL "whatsup@mach1.tech"
                ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../../installer/resources/icon.png"
                ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/../../installer/resources/icon.png"
                PLUGIN_MANUFACTURER_CODE "Mac1"
                PLUGIN_CODE ${CUSTOM_APP_CODE}
                BUNDLE_ID ${CUSTOM_BUNDLE_ID}
                PLUGIN_NAME ${CMAKE_PROJECT_NAME}
                PRODUCT_NAME ${CMAKE_PROJECT_NAME}
                IS_SYNTH FALSE)

# add required flags
juce_generate_juce_header(${CMAKE_PROJECT_NAME})

target_compile_definitions(${CMAKE_PROJECT_NAME}
    PUBLIC 
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${CMAKE_PROJECT_NAME},JUCE_PROJECT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${CMAKE_PROJECT_NAME},JUCE_VERSION>"
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Disable compiler extensions for the project targets (e.g. use -std=c++17 instead of -std=gnu++17).
get_property(project_targets DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
set_target_properties(${project_targets} PROPERTIES CXX_EXTENSIONS OFF)

# add the sources
add_subdirectory(Source)

# Configure source groups for Xcode organization to match folder structure
get_target_property(target_sources ${CMAKE_PROJECT_NAME} SOURCES)
foreach(source_file ${target_sources})
    if(source_file MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/Source/")
        # Get relative path from Source directory
        file(RELATIVE_PATH relative_path "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${source_file}")
        get_filename_component(dir_path "${relative_path}" DIRECTORY)
        if(dir_path)
            # Replace forward slashes with backslashes for Xcode groups
            string(REPLACE "/" "\\\\" group_name "${dir_path}")
            source_group("Source Files\\\\${group_name}" FILES "${source_file}")
        else()
            source_group("Source Files" FILES "${source_file}")
        endif()
    endif()
endforeach()

# definitions to replace the `JucePluginDefines.h`
set(JucePlugin_Name ${CMAKE_PROJECT_NAME})
set(JucePlugin_Desc ${CMAKE_PROJECT_NAME})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_opengl
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_osc
        m1_orientation_client
        m1_mathematics
        )

# link m1-sdk
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE M1Encode M1Decode M1Transcode)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ../../m1-player/Modules/m1-sdk/libmach1spatial/api_common/include ../../m1-player/Modules/m1-sdk/libmach1spatial/api_decode/include ../../m1-player/Modules/m1-sdk/libmach1spatial/api_encode/include ../../m1-player/Modules/m1-sdk/libmach1spatial/api_transcode/include ../../m1-player/Modules/m1-sdk/libmach1spatial/deps)

### setup info.plist and entitlements for macos
if(APPLE)
    set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Resources/macos/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME ${CMAKE_PROJECT_NAME}
        MACOSX_FRAMEWORK_IDENTIFIER ${CUSTOM_BUNDLE_ID}
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/macos/entitlements.mac.plist"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.utilities"
    )
    message(STATUS "Attaching Info.plist found at ${CMAKE_CURRENT_SOURCE_DIR}/Resources/macos/Info.plist")
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_FLAGS 
        "-sectcreate __TEXT __info_plist ${CMAKE_CURRENT_SOURCE_DIR}/Resources/macos/Info.plist"
    )
endif()

### IDE Generator post-config ###
# IDEs:  Move the "shared code" source group (main target) out of the "Targets" folder
#        to the top level.
set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES FOLDER "")
# IDEs:  Organise source file grouping.
get_target_property(project_sources "${CMAKE_PROJECT_NAME}" SOURCES)
#   If JuceHeader.h is generated, remove it from the source file list and handle it individually.
get_target_property(juce_library_code "${CMAKE_PROJECT_NAME}" JUCE_GENERATED_SOURCES_DIRECTORY)
set(juce_header "${juce_library_code}/JuceHeader.h")
list(REMOVE_ITEM project_sources "${juce_header}")

#   Place JuceHeader.h and the associated CMake Rule file into a "JUCE Library Code" folder.
source_group("JUCE Library Code" FILES "${juce_header}")
source_group("JUCE Library Code/CMake Rules" FILES "${juce_header}.rule")

#   Generate source groups that follow the organisation of source file directories.
#   Note: Source groups are configured in Source/CMakeLists.txt for better organization

#   Place Binary Data related source files into the root of their target folder.
if(TARGET Resources)
    get_target_property(project_assets Resources SOURCES)
    source_group("" FILES ${project_assets})
endif()

# Color our warnings and errors
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options(-fcolor-diagnostics)
endif()

install(TARGETS ${CMAKE_PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
