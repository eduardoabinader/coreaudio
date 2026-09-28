#include <gigante/Logger.h>

namespace
{

static_assert(gigante::Logger::noSessionId == 0);
static_assert(GIGANTE_LOG_LEVEL_OFF < GIGANTE_LOG_LEVEL_ERROR);
static_assert(GIGANTE_LOG_LEVEL_ERROR < GIGANTE_LOG_LEVEL_WARN);
static_assert(GIGANTE_LOG_LEVEL_WARN < GIGANTE_LOG_LEVEL_INFO);
static_assert(GIGANTE_LOG_LEVEL_INFO < GIGANTE_LOG_LEVEL_DEBUG);
static_assert(GIGANTE_LOG_LEVEL_DEBUG < GIGANTE_LOG_LEVEL_TRACE);

} // namespace
