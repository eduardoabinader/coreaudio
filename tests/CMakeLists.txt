add_executable(
    giganteCoreTests
    EffectsChainTests.cpp
)

target_link_libraries(giganteCoreTests PRIVATE gigante::core)
target_compile_features(giganteCoreTests PRIVATE cxx_std_17)
if(GIGANTE_JUCE_TARGET)
    target_compile_definitions(giganteCoreTests PRIVATE GIGANTE_TESTS_WITH_JUCE_DSP=1)
endif()
gigante_enable_project_warnings(giganteCoreTests)

add_test(NAME giganteCoreTests COMMAND giganteCoreTests)

add_executable(giganteGoldenVectorTests GoldenVectorTests.cpp)
target_compile_features(giganteGoldenVectorTests PRIVATE cxx_std_17)
target_compile_definitions(
    giganteGoldenVectorTests
    PRIVATE GIGANTE_GOLDEN_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/golden/kuaru"
)
gigante_enable_project_warnings(giganteGoldenVectorTests)
add_test(NAME giganteGoldenVectorTests COMMAND giganteGoldenVectorTests)

add_test(
    NAME giganteGoldenHashVerification
    COMMAND
        ${CMAKE_COMMAND}
        -DGOLDEN_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/golden/kuaru
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/VerifyKuaruGoldens.cmake
)

add_executable(gigantePlatformTests HostFactsTests.cpp)
target_link_libraries(gigantePlatformTests PRIVATE gigante::platform)
target_compile_features(gigantePlatformTests PRIVATE cxx_std_17)
target_compile_definitions(
    gigantePlatformTests
    PRIVATE GIGANTE_EXPECTED_PLATFORM_NAME="${GIGANTE_PLATFORM_NAME}"
)
gigante_enable_project_warnings(gigantePlatformTests)
add_test(NAME gigantePlatformTests COMMAND gigantePlatformTests)

add_executable(giganteEndpointPolicyTests EndpointPolicyTests.cpp)
target_link_libraries(giganteEndpointPolicyTests PRIVATE gigante::platform)
target_compile_features(giganteEndpointPolicyTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteEndpointPolicyTests)
add_test(NAME giganteEndpointPolicyTests COMMAND giganteEndpointPolicyTests)

add_executable(giganteEndpointMonitorTests EndpointMonitorTests.cpp)
target_link_libraries(giganteEndpointMonitorTests PRIVATE gigante::platform)
target_compile_features(giganteEndpointMonitorTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteEndpointMonitorTests)
add_test(NAME giganteEndpointMonitorTests COMMAND giganteEndpointMonitorTests)

add_executable(
    gigantePlatformEndpointSourcesTests
    PlatformEndpointSourcesTests.cpp
)
target_link_libraries(
    gigantePlatformEndpointSourcesTests
    PRIVATE gigante::platform
)
target_compile_features(
    gigantePlatformEndpointSourcesTests
    PRIVATE cxx_std_17
)
if(GIGANTE_HAS_PLATFORM_ENDPOINT_SOURCES)
    target_compile_definitions(
        gigantePlatformEndpointSourcesTests
        PRIVATE GIGANTE_TEST_HAS_PLATFORM_ENDPOINT_SOURCES=1
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(
            gigantePlatformEndpointSourcesTests
            PRIVATE GIGANTE_TEST_LINUX_ENDPOINT_SOURCE=1
        )
    endif()
endif()
gigante_enable_project_warnings(gigantePlatformEndpointSourcesTests)
add_test(
    NAME gigantePlatformEndpointSourcesTests
    COMMAND gigantePlatformEndpointSourcesTests
)

add_library(gigantePublicHeaderIsolation OBJECT PublicHeaderIsolation.cpp)
target_link_libraries(gigantePublicHeaderIsolation PRIVATE gigante::core)
target_compile_features(gigantePublicHeaderIsolation PRIVATE cxx_std_17)
gigante_enable_project_warnings(gigantePublicHeaderIsolation)

add_library(
    gigantePlatformPublicHeaderIsolation
    OBJECT
    PlatformPublicHeaderIsolation.cpp
)
target_link_libraries(
    gigantePlatformPublicHeaderIsolation
    PRIVATE gigante::platform
)
target_compile_features(
    gigantePlatformPublicHeaderIsolation
    PRIVATE cxx_std_17
)
gigante_enable_project_warnings(gigantePlatformPublicHeaderIsolation)
