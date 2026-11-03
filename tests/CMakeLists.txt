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

add_executable(giganteLogTests LoggerTests.cpp)
target_link_libraries(giganteLogTests PRIVATE gigante::log)
target_compile_features(giganteLogTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteLogTests)
add_test(NAME giganteLogTests COMMAND giganteLogTests)

add_executable(giganteLogCompiledOutTests LoggerCompiledOutTests.cpp)
target_link_libraries(giganteLogCompiledOutTests PRIVATE gigante::log)
target_compile_features(giganteLogCompiledOutTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteLogCompiledOutTests)
add_test(NAME giganteLogCompiledOutTests COMMAND giganteLogCompiledOutTests)

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

add_library(giganteLogPublicHeaderIsolation OBJECT LogPublicHeaderIsolation.cpp)
target_link_libraries(giganteLogPublicHeaderIsolation PRIVATE gigante::log)
target_compile_features(giganteLogPublicHeaderIsolation PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteLogPublicHeaderIsolation)

add_executable(giganteAudioSessionTests AudioSessionTests.cpp)
target_link_libraries(giganteAudioSessionTests PRIVATE gigante::platform)
target_include_directories(giganteAudioSessionTests PRIVATE ../src/platform)
target_compile_features(giganteAudioSessionTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteAudioSessionTests)
add_test(NAME giganteAudioSessionTests COMMAND giganteAudioSessionTests)
set_tests_properties(giganteAudioSessionTests PROPERTIES TIMEOUT 20)

add_executable(giganteAndroidAudioSessionTests AndroidAudioSessionTests.cpp)
target_link_libraries(giganteAndroidAudioSessionTests PRIVATE gigante::platform)
target_include_directories(giganteAndroidAudioSessionTests PRIVATE ../src/platform)
target_compile_features(giganteAndroidAudioSessionTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteAndroidAudioSessionTests)
add_test(NAME giganteAndroidAudioSessionTests COMMAND giganteAndroidAudioSessionTests)
set_tests_properties(giganteAndroidAudioSessionTests PROPERTIES TIMEOUT 20)

add_executable(giganteAndroidSessionResourceTests AndroidSessionResourceTests.cpp)
target_link_libraries(giganteAndroidSessionResourceTests PRIVATE gigante::platform)
target_include_directories(giganteAndroidSessionResourceTests PRIVATE ../src/platform)
target_compile_features(giganteAndroidSessionResourceTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteAndroidSessionResourceTests)
add_test(NAME giganteAndroidSessionResourceTests COMMAND giganteAndroidSessionResourceTests)
set_tests_properties(giganteAndroidSessionResourceTests PROPERTIES TIMEOUT 20)

if(GIGANTE_BUILD_REST)
    add_library(giganteRestPublicHeaderIsolation OBJECT RestPublicHeaderIsolation.cpp)
    target_include_directories(giganteRestPublicHeaderIsolation PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    set_target_properties(giganteRestPublicHeaderIsolation PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    gigante_enable_project_warnings(giganteRestPublicHeaderIsolation)

    find_program(GIGANTE_REST_OPENSSL_PROGRAM openssl)
    set(giganteRestCerts "${CMAKE_CURRENT_BINARY_DIR}/restTestCerts")
    add_test(
        NAME giganteRestCertificates
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/rest_feasibility/generateTestCerts.sh" "${giganteRestCerts}"
    )
    set_tests_properties(giganteRestCertificates PROPERTIES FIXTURES_SETUP giganteRestCertificates)

    foreach(restTest RestServerTests RestSecurityTests RestServiceLifecycleTests RestClientTests RestStreamTests RestCopyTests RestDiagnosticsTests)
        add_executable(gigante${restTest} ${restTest}.cpp)
        target_link_libraries(gigante${restTest} PRIVATE gigante::restcore)
        set_target_properties(gigante${restTest} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
        gigante_enable_project_warnings(gigante${restTest})
        add_test(NAME gigante${restTest} COMMAND gigante${restTest} "${giganteRestCerts}")
        set_tests_properties(gigante${restTest} PROPERTIES LABELS rest FIXTURES_REQUIRED giganteRestCertificates TIMEOUT 120)
    endforeach()
    # These two also check engine internals: delivery policies on the hub, and the digest.
    target_include_directories(giganteRestStreamTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/rest")
    target_include_directories(giganteRestCopyTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/rest")
    # The copy suite moves 1 GiB each way, which sanitizer builds make several times slower.
    set_tests_properties(giganteRestCopyTests PROPERTIES TIMEOUT 900)

    add_test(
        NAME giganteRestCurlInterop
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/restCurlInteropTests.sh" "$<TARGET_FILE:giganteRestServerTests>" "${giganteRestCerts}"
    )
    set_tests_properties(giganteRestCurlInterop PROPERTIES LABELS rest FIXTURES_REQUIRED giganteRestCertificates TIMEOUT 60)
endif()


add_executable(giganteOutputRouteTests OutputRouteTests.cpp)
target_link_libraries(giganteOutputRouteTests PRIVATE gigante::platform)
target_compile_features(giganteOutputRouteTests PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteOutputRouteTests)
add_test(NAME giganteOutputRouteTests COMMAND giganteOutputRouteTests)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/OutputRouteHeaderIsolation.cpp"
    "#include <gigante/OutputRoute.h>\n")
add_library(giganteOutputRouteHeaderIsolation OBJECT
    "${CMAKE_CURRENT_BINARY_DIR}/OutputRouteHeaderIsolation.cpp")
target_link_libraries(giganteOutputRouteHeaderIsolation PRIVATE gigante::platform)
target_compile_features(giganteOutputRouteHeaderIsolation PRIVATE cxx_std_17)
gigante_enable_project_warnings(giganteOutputRouteHeaderIsolation)
