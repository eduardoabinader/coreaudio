cmake_minimum_required(VERSION 3.22)

project(
    GiganteLabsCore
    VERSION 0.1.0
    DESCRIPTION "Product-neutral realtime audio DSP core"
    LANGUAGES CXX
)

option(GIGANTE_BUILD_TESTS "Build Gigante Labs Core tests" ON)
option(GIGANTE_WARNINGS_AS_ERRORS "Treat Gigante Labs Core warnings as errors" ON)
set(GIGANTE_REVISION "unknown" CACHE STRING "Exact source revision for reproducible build identity")
set(GIGANTE_JUCE_TARGET "" CACHE STRING
    "Existing consumer-provided interface target for the pinned JUCE modules")
set(GIGANTE_LOG_COMPILED_LEVEL "INFO" CACHE STRING
    "Highest compiled log level: OFF, ERROR, WARN, INFO, DEBUG or TRACE")
set_property(CACHE GIGANTE_LOG_COMPILED_LEVEL PROPERTY STRINGS
    OFF ERROR WARN INFO DEBUG TRACE)

string(TOUPPER "${GIGANTE_LOG_COMPILED_LEVEL}" giganteLogCompiledLevel)
set(giganteLogLevels OFF ERROR WARN INFO DEBUG TRACE)
list(FIND giganteLogLevels "${giganteLogCompiledLevel}" giganteLogLevelIndex)
if(giganteLogLevelIndex EQUAL -1)
    message(FATAL_ERROR
        "GIGANTE_LOG_COMPILED_LEVEL must be OFF, ERROR, WARN, INFO, DEBUG or TRACE")
endif()

include(FetchContent)
if(giganteLogLevelIndex GREATER 0 AND NOT TARGET spdlog::spdlog)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    set(SPDLOG_SYSTEM_INCLUDES ON CACHE BOOL "" FORCE)
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    set(SPDLOG_DISABLE_DEFAULT_LOGGER ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG 79524ddd08a4ec981b7fea76afd08ee05f83755d
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

if(GIGANTE_JUCE_TARGET AND NOT TARGET ${GIGANTE_JUCE_TARGET})
    message(FATAL_ERROR
        "GIGANTE_JUCE_TARGET names a target that does not exist: ${GIGANTE_JUCE_TARGET}")
endif()

add_library(giganteCore STATIC)
add_library(gigante::core ALIAS giganteCore)
add_library(gigantePlatform STATIC)
add_library(gigante::platform ALIAS gigantePlatform)
add_library(giganteLog STATIC)
add_library(gigante::log ALIAS giganteLog)

target_sources(
    giganteCore
    PRIVATE
        src/dsp/EffectsChain.cpp
        src/metrics/RealtimeMetrics.cpp
        include/gigante/AudioBufferView.h
        include/gigante/EffectsChain.h
        include/gigante/EffectsStageObserver.h
        include/gigante/RealtimeMetrics.h
        include/gigante/Revision.h
)

target_sources(
    gigantePlatform
    PRIVATE
        src/platform/AndroidSessionPreparation.cpp
        src/platform/AndroidAudioFocusDriver.h
        src/platform/SessionPreparation.cpp
        src/platform/SessionPreparationImplementation.h
        include/gigante/SessionPreparation.h
        src/platform/EndpointMonitor.cpp
        src/platform/EndpointPolicy.cpp
        src/platform/HostFacts.cpp
        include/gigante/EndpointMonitor.h
        include/gigante/EndpointPolicy.h
        include/gigante/HostFacts.h
)

target_sources(
    giganteLog
    PRIVATE
        src/logger/LoggerCommon.cpp
        include/gigante/Logger.h
)

if(giganteLogLevelIndex GREATER 0)
    target_sources(
        giganteLog
        PRIVATE
            src/logger/Logger.cpp
            src/logger/PlatformLogSink.h
    )
else()
    target_sources(giganteLog PRIVATE src/logger/DisabledLogger.cpp)
endif()

target_include_directories(
    giganteCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_include_directories(
    gigantePlatform
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_include_directories(
    giganteLog
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

if(ANDROID)
    target_sources(gigantePlatform PRIVATE
        src/platform/AndroidSessionJni.cpp
        include/gigante/AndroidSessionPreparation.h)
    target_sources(
        gigantePlatform
        PRIVATE
            src/platform/AndroidHostFacts.cpp
            src/platform/PosixHostFactsSupport.cpp
    )
    set(GIGANTE_PLATFORM_NAME "Android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    target_sources(gigantePlatform PRIVATE src/platform/IosSessionPreparation.mm)
    target_compile_definitions(gigantePlatform PRIVATE GIGANTE_IOS_SESSION=1)
    set_target_properties(gigantePlatform PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED YES)
    set_source_files_properties(src/platform/IosSessionPreparation.mm PROPERTIES COMPILE_OPTIONS "-fno-objc-arc")
    find_library(GIGANTE_AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
    find_library(GIGANTE_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(gigantePlatform PRIVATE
        ${GIGANTE_AVFOUNDATION_FRAMEWORK} ${GIGANTE_FOUNDATION_FRAMEWORK})
    target_sources(
        gigantePlatform
        PRIVATE
            src/platform/AppleHostFactsSupport.cpp
            src/platform/IosHostFacts.cpp
    )
    set(GIGANTE_PLATFORM_NAME "iOS")
elseif(APPLE)
    target_sources(
        gigantePlatform
        PRIVATE
            src/platform/AppleHostFactsSupport.cpp
            src/platform/MacOsHostFacts.cpp
    )
    find_library(GIGANTE_APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
    target_link_libraries(gigantePlatform PRIVATE ${GIGANTE_APPLICATION_SERVICES_FRAMEWORK})
    set(GIGANTE_PLATFORM_NAME "macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(
        gigantePlatform
        PRIVATE
            src/platform/LinuxHostFacts.cpp
            src/platform/PosixHostFactsSupport.cpp
    )
    set(GIGANTE_PLATFORM_NAME "Linux")
else()
    message(FATAL_ERROR
        "gigantePlatform does not support target platform '${CMAKE_SYSTEM_NAME}'")
endif()

if(giganteLogLevelIndex GREATER 0)
    if(ANDROID)
        target_sources(giganteLog PRIVATE src/logger/AndroidLogSink.cpp)
        target_link_libraries(giganteLog PRIVATE log)
    else()
        target_sources(giganteLog PRIVATE src/logger/TerminalLogSink.cpp)
    endif()
endif()
if(giganteLogLevelIndex GREATER 0)
    target_link_libraries(giganteLog PRIVATE spdlog::spdlog)
endif()

if(GIGANTE_JUCE_TARGET)
    target_sources(giganteCore PRIVATE src/dsp/JuceEffectsChainImplementation.cpp)
    target_link_libraries(giganteCore PRIVATE ${GIGANTE_JUCE_TARGET})
    target_compile_definitions(giganteCore PRIVATE GIGANTE_HAS_JUCE_DSP=1)
    target_link_libraries(gigantePlatform PRIVATE ${GIGANTE_JUCE_TARGET})
else()
    target_sources(giganteCore PRIVATE src/dsp/NeutralEffectsChainImplementation.cpp)
endif()

target_compile_features(giganteCore PUBLIC cxx_std_17)
target_compile_features(gigantePlatform PUBLIC cxx_std_17)
target_compile_features(giganteLog PUBLIC cxx_std_17)
target_compile_definitions(
    giganteCore
    PUBLIC GIGANTE_REVISION="${GIGANTE_REVISION}"
)
target_compile_definitions(
    gigantePlatform
    PUBLIC GIGANTE_REVISION="${GIGANTE_REVISION}"
)
target_compile_definitions(
    giganteLog
    PUBLIC GIGANTE_LOG_COMPILED_LEVEL=${giganteLogLevelIndex}
)

function(gigante_enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(GIGANTE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Wshadow
        )
        if(GIGANTE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

gigante_enable_project_warnings(giganteCore)
gigante_enable_project_warnings(gigantePlatform)
gigante_enable_project_warnings(giganteLog)

if(GIGANTE_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
