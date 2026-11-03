#include <gigante/RestClient.h>
#include <gigante/RestServer.h>

#ifdef CPPHTTPLIB_VERSION
#error "public REST headers must not expose cpp-httplib"
#endif

static_assert(sizeof(gigante::RestResult) > 0, "public REST types must be complete");
