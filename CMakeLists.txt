cmake_minimum_required(VERSION 3.23)

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

if(GIGANTE_JUCE_TARGET AND NOT TARGET ${GIGANTE_JUCE_TARGET})
    message(FATAL_ERROR
        "GIGANTE_JUCE_TARGET names a target that does not exist: ${GIGANTE_JUCE_TARGET}")
endif()

add_library(giganteCore STATIC)
add_library(gigante::core ALIAS giganteCore)

target_sources(
    giganteCore
    PRIVATE
        src/EffectsChain.cpp
        src/RealtimeMetrics.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/gigante/AudioBufferView.h
            include/gigante/EffectsChain.h
            include/gigante/EffectsStageObserver.h
            include/gigante/RealtimeMetrics.h
            include/gigante/Revision.h
)

if(GIGANTE_JUCE_TARGET)
    target_sources(giganteCore PRIVATE src/JuceEffectsChainImplementation.cpp)
    target_link_libraries(giganteCore PRIVATE ${GIGANTE_JUCE_TARGET})
    target_compile_definitions(giganteCore PRIVATE GIGANTE_HAS_JUCE_DSP=1)
else()
    target_sources(giganteCore PRIVATE src/NeutralEffectsChainImplementation.cpp)
endif()

target_compile_features(giganteCore PUBLIC cxx_std_17)
target_compile_definitions(
    giganteCore
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

if(GIGANTE_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
