#pragma once

// cpp-httplib shares one Mbed TLS context, including its random generator, across
// its server threads. The default Mbed TLS configuration has no locking.
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_PTHREAD

// Default-on features that write unsynchronized globals during handshakes:
// self-test operation counters, and AES tables generated on first use.
#undef MBEDTLS_SELF_TEST
#define MBEDTLS_AES_ROM_TABLES
