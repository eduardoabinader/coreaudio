#ifdef GIGANTE_LOG_COMPILED_LEVEL
#undef GIGANTE_LOG_COMPILED_LEVEL
#endif
#define GIGANTE_LOG_COMPILED_LEVEL GIGANTE_LOG_LEVEL_OFF

#include <gigante/Logger.h>

#include <cstdlib>
#include <iostream>

int main()
{
    int evaluated = 0;
    GIGANTE_LOG_ERROR(++evaluated, ++evaluated);
    GIGANTE_LOG_WARN(++evaluated, ++evaluated);
    GIGANTE_LOG_INFO(++evaluated, ++evaluated);
    GIGANTE_LOG_DEBUG(++evaluated, ++evaluated);
    GIGANTE_LOG_TRACE(++evaluated, ++evaluated);

    if (evaluated != 0)
    {
        std::cerr << "FAIL: compiled-out log argument was evaluated\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
