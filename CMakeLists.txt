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
set(GIGANTE_HAS_PLATFORM_ENDPOINT_SOURCES OFF)

if(GIGANTE_JUCE_TARGET AND NOT TARGET ${GIGANTE_JUCE_TARGET})
    message(FATAL_ERROR
        "GIGANTE_JUCE_TARGET names a target that does not exist: ${GIGANTE_JUCE_TARGET}")
endif()

add_library(giganteCore STATIC)
add_library(gigante::core ALIAS giganteCore)
add_library(gigantePlatform STATIC)
add_library(gigante::platform ALIAS gigantePlatform)

target_sources(
    giganteCore
    PRIVATE
        src/EffectsChain.cpp
        src/RealtimeMetrics.cpp
        include/gigante/AudioBufferView.h
        include/gigante/EffectsChain.h
        include/gigante/EffectsStageObserver.h
        include/gigante/RealtimeMetrics.h
        include/gigante/Revision.h
)

target_sources(
    gigantePlatform
    PRIVATE
        src/EndpointMonitor.cpp
        src/EndpointPolicy.cpp
        src/HostFacts.cpp
        src/PlatformEndpointSources.cpp
        include/gigante/EndpointMonitor.h
        include/gigante/EndpointPolicy.h
        include/gigante/HostFacts.h
        include/gigante/PlatformEndpointSources.h
)

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

if(ANDROID)
    target_sources(
        gigantePlatform
        PRIVATE
            src/AndroidHostFacts.cpp
            src/PosixHostFactsSupport.cpp
    )
    set(GIGANTE_PLATFORM_NAME "Android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_sources(
        gigantePlatform
        PRIVATE
            src/AppleHostFactsSupport.cpp
            src/IosHostFacts.cpp
    )
    set(GIGANTE_PLATFORM_NAME "iOS")
elseif(APPLE)
    target_sources(
        gigantePlatform
        PRIVATE
            src/AppleHostFactsSupport.cpp
            src/MacOsHostFacts.cpp
    )
    set(GIGANTE_PLATFORM_NAME "macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(
        gigantePlatform
        PRIVATE
            src/LinuxHostFacts.cpp
            src/PosixHostFactsSupport.cpp
    )
    set(GIGANTE_PLATFORM_NAME "Linux")
else()
    message(FATAL_ERROR
        "gigantePlatform does not support target platform '${CMAKE_SYSTEM_NAME}'")
endif()

if(GIGANTE_JUCE_TARGET)
    target_sources(giganteCore PRIVATE src/JuceEffectsChainImplementation.cpp)
    target_link_libraries(giganteCore PRIVATE ${GIGANTE_JUCE_TARGET})
    target_compile_definitions(giganteCore PRIVATE GIGANTE_HAS_JUCE_DSP=1)
    target_link_libraries(gigantePlatform PRIVATE ${GIGANTE_JUCE_TARGET})

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(
            gigantePlatform
            PRIVATE
                src/JuceEndpointSourcesSupport.cpp
                src/JuceEndpointSourcesSupport.h
                src/LinuxEndpointSources.cpp
        )
        set(GIGANTE_HAS_PLATFORM_ENDPOINT_SOURCES ON)
    else()
        target_sources(
            gigantePlatform
            PRIVATE src/UnavailablePlatformEndpointSources.cpp
        )
        target_compile_definitions(
            gigantePlatform
            PRIVATE GIGANTE_PLATFORM_ENDPOINT_BACKEND_NOT_IMPLEMENTED=1
        )
    endif()
else()
    target_sources(giganteCore PRIVATE src/NeutralEffectsChainImplementation.cpp)
    target_sources(
        gigantePlatform
        PRIVATE src/UnavailablePlatformEndpointSources.cpp
    )
endif()

target_compile_features(giganteCore PUBLIC cxx_std_17)
target_compile_features(gigantePlatform PUBLIC cxx_std_17)
target_compile_definitions(
    giganteCore
    PUBLIC GIGANTE_REVISION="${GIGANTE_REVISION}"
)
target_compile_definitions(
    gigantePlatform
    PUBLIC GIGANTE_REVISION="${GIGANTE_REVISION}"
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

if(GIGANTE_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
