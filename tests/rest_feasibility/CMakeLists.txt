cmake_minimum_required(VERSION 3.22)

project(GiganteRestFeasibility LANGUAGES C CXX)

set(REST_FEASIBILITY_TLS_BACKEND "OPENSSL" CACHE STRING "TLS backend: OPENSSL, MBEDTLS or NONE")
set_property(CACHE REST_FEASIBILITY_TLS_BACKEND PROPERTY STRINGS OPENSSL MBEDTLS NONE)
set(REST_FEASIBILITY_SANITIZE "" CACHE STRING "Compiler sanitizers, for example address,undefined or thread")
option(REST_FEASIBILITY_LIBRARY_ONLY "Build only the library, for platforms that need signed executables" OFF)

if(NOT REST_FEASIBILITY_TLS_BACKEND MATCHES "^(OPENSSL|MBEDTLS|NONE)$")
    message(FATAL_ERROR "REST_FEASIBILITY_TLS_BACKEND must be OPENSSL, MBEDTLS or NONE")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(REST_FEASIBILITY_SANITIZE)
    add_compile_options(-fsanitize=${REST_FEASIBILITY_SANITIZE} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${REST_FEASIBILITY_SANITIZE})
endif()

include(FetchContent)

# Pinned to v0.64.0. The source subdirectory is unused so upstream's CMake, with
# its use-if-available detection of OpenSSL and compression, never runs.
FetchContent_Declare(
    cppHttplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG 24e58ce5b25d55045af4cd6f463e83a8082fbe6b
    SOURCE_SUBDIR notUsed
)
FetchContent_MakeAvailable(cppHttplib)

set(coreRoot "${CMAKE_CURRENT_LIST_DIR}/../..")

add_library(restFeasibilityCore STATIC
    "${coreRoot}/src/rest/CapacityGate.cpp"
    "${coreRoot}/src/rest/CppHttplibClientTransport.cpp"
    "${coreRoot}/src/rest/CppHttplibTransport.cpp"
    "${coreRoot}/src/rest/RestMetrics.cpp"
    "${coreRoot}/src/rest/Router.cpp"
    "${coreRoot}/src/rest/StreamHub.cpp"
    "${coreRoot}/src/rest/StreamSession.cpp"
)
target_include_directories(restFeasibilityCore PUBLIC "${coreRoot}/src/rest")
target_include_directories(restFeasibilityCore SYSTEM PUBLIC "${cpphttplib_SOURCE_DIR}")
target_compile_options(restFeasibilityCore PRIVATE -Wall -Wextra -Wpedantic -Werror)

find_package(Threads REQUIRED)
target_link_libraries(restFeasibilityCore PUBLIC Threads::Threads)

if(REST_FEASIBILITY_TLS_BACKEND STREQUAL "OPENSSL")
    find_package(OpenSSL 3.0 REQUIRED)
    target_compile_definitions(restFeasibilityCore PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(restFeasibilityCore PUBLIC OpenSSL::SSL OpenSSL::Crypto)
elseif(REST_FEASIBILITY_TLS_BACKEND STREQUAL "MBEDTLS")
    set(ENABLE_TESTING OFF)
    set(ENABLE_PROGRAMS OFF)
    set(MBEDTLS_FATAL_WARNINGS OFF)
    FetchContent_Declare(
        mbedtlsSource
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG 068ff080b369adfac81509f9b57b2afabaf82dc5 # v3.6.7
        GIT_SUBMODULES framework
    )
    add_compile_definitions(
        "MBEDTLS_USER_CONFIG_FILE=\"${coreRoot}/src/rest/mbedtlsRestConfig.h\"")
    FetchContent_MakeAvailable(mbedtlsSource)
    target_compile_definitions(restFeasibilityCore PUBLIC CPPHTTPLIB_MBEDTLS_SUPPORT)
    target_link_libraries(restFeasibilityCore PUBLIC mbedtls mbedx509 mbedcrypto)
endif()

if(APPLE AND NOT REST_FEASIBILITY_TLS_BACKEND STREQUAL "NONE" AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(restFeasibilityCore PUBLIC "-framework CoreFoundation" "-framework Security")
endif()

if(REST_FEASIBILITY_LIBRARY_ONLY)
    return()
endif()

add_library(restFeasibilityProbes STATIC
    LoadClient.cpp
    ProbeReport.cpp
    TransferProbe.cpp
)
target_include_directories(restFeasibilityProbes PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(restFeasibilityProbes PUBLIC restFeasibilityCore)
target_compile_options(restFeasibilityProbes PRIVATE -Wall -Wextra -Wpedantic -Werror)

foreach(probe tlsProbe capacityProbe lifetimeProbe clientProbe)
    add_executable(${probe} ${probe}.cpp)
    target_link_libraries(${probe} PRIVATE restFeasibilityProbes)
    target_compile_options(${probe} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endforeach()

if(NOT REST_FEASIBILITY_SANITIZE)
    return()
endif()

# Mutation fuzz targets replace libFuzzer, which Apple clang does not link here.
foreach(target server client header_parser multipart_parser url_parser)
    add_executable(fuzz_${target} mutationFuzzDriver.cpp "${cpphttplib_SOURCE_DIR}/test/fuzzing/${target}_fuzzer.cc")
    target_compile_options(fuzz_${target} PRIVATE -w)
    target_link_libraries(fuzz_${target} PRIVATE restFeasibilityCore)
endforeach()
add_executable(fuzz_sse mutationFuzzDriver.cpp sseParserFuzzer.cpp)
target_compile_options(fuzz_sse PRIVATE -w)
target_link_libraries(fuzz_sse PRIVATE restFeasibilityCore)
