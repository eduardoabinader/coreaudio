# Opt-in REST engine. Nothing here runs, and nothing is fetched, unless
# GIGANTE_BUILD_REST is on. core, platform and log gain no dependency on it.
include(FetchContent)

string(TOUPPER "${GIGANTE_REST_TLS_BACKEND}" giganteRestTlsBackend)
if(NOT giganteRestTlsBackend MATCHES "^(AUTO|OPENSSL|MBEDTLS|NONE)$")
    message(FATAL_ERROR "GIGANTE_REST_TLS_BACKEND must be AUTO, OPENSSL, MBEDTLS or NONE")
endif()
if(giganteRestTlsBackend STREQUAL "AUTO")
    if(ANDROID OR IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_CROSSCOMPILING)
        set(giganteRestTlsBackend "MBEDTLS")
    else()
        set(giganteRestTlsBackend "OPENSSL")
    endif()
endif()
if(giganteRestTlsBackend STREQUAL "NONE" AND NOT GIGANTE_REST_ALLOW_PLAINTEXT)
    message(FATAL_ERROR "A build with no TLS backend and no plaintext could not serve anything")
endif()

# Pinned to v0.64.0. The unused source subdirectory keeps upstream's CMake, with its
# use-if-available detection of OpenSSL and compression, from running.
FetchContent_Declare(
    gigantecpphttplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG 24e58ce5b25d55045af4cd6f463e83a8082fbe6b
    SOURCE_SUBDIR notUsed
)
FetchContent_MakeAvailable(gigantecpphttplib)

add_library(giganteRestCore STATIC
    src/rest/CommonEndpoints.cpp
    src/rest/CppHttplibClientTransport.cpp
    src/rest/CppHttplibTransport.cpp
    src/rest/MiddlewareChain.cpp
    src/rest/RestClient.cpp
    src/rest/RestMetrics.cpp
    src/rest/RestServer.cpp
    src/rest/Router.cpp
    src/rest/SecurityPolicy.cpp
    src/rest/ServiceRegistry.cpp
)
add_library(gigante::restcore ALIAS giganteRestCore)
set_target_properties(giganteRestCore PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(giganteRestCore
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/rest"
)
target_include_directories(giganteRestCore SYSTEM PRIVATE "${gigantecpphttplib_SOURCE_DIR}")
target_compile_definitions(giganteRestCore PRIVATE
    GIGANTE_REST_ALLOW_PLAINTEXT=$<BOOL:${GIGANTE_REST_ALLOW_PLAINTEXT}>
)
find_package(Threads REQUIRED)
target_link_libraries(giganteRestCore PUBLIC Threads::Threads)
gigante_enable_project_warnings(giganteRestCore)

if(giganteRestTlsBackend STREQUAL "OPENSSL")
    find_package(OpenSSL 3.0 REQUIRED)
    target_compile_definitions(giganteRestCore PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT GIGANTE_REST_HAS_TLS=1)
    target_link_libraries(giganteRestCore PRIVATE OpenSSL::SSL OpenSSL::Crypto)
elseif(giganteRestTlsBackend STREQUAL "MBEDTLS")
    enable_language(C)
    set(giganteRestMbedtlsConfig "${CMAKE_CURRENT_SOURCE_DIR}/src/rest/mbedtlsRestConfig.h")
    if(NOT TARGET mbedtls)
        set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
        set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
        add_compile_definitions("MBEDTLS_USER_CONFIG_FILE=\"${giganteRestMbedtlsConfig}\"")
        FetchContent_Declare(
            gigantembedtls
            GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
            GIT_TAG 068ff080b369adfac81509f9b57b2afabaf82dc5 # v3.6.7
            GIT_SUBMODULES framework
        )
        FetchContent_MakeAvailable(gigantembedtls)
    endif()
    target_compile_definitions(giganteRestCore PRIVATE
        CPPHTTPLIB_MBEDTLS_SUPPORT
        GIGANTE_REST_HAS_TLS=1
        "MBEDTLS_USER_CONFIG_FILE=\"${giganteRestMbedtlsConfig}\""
    )
    target_link_libraries(giganteRestCore PRIVATE mbedtls mbedx509 mbedcrypto)
else()
    target_compile_definitions(giganteRestCore PRIVATE GIGANTE_REST_HAS_TLS=0)
endif()

if(APPLE AND NOT giganteRestTlsBackend STREQUAL "NONE" AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(giganteRestCore PRIVATE "-framework CoreFoundation" "-framework Security")
endif()
