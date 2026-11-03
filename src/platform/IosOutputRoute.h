#pragma once
#include "gigante/OutputRoute.h"
#include <optional>

namespace gigante
{
// Private iOS collection seam, independent of ObjC for deterministic tests.
// Only an actually observed native Speaker can populate knownSpeaker.
[[nodiscard]] OutputRoute observeIosOutputs(std::vector<AudioLink> currentOutputs,
    std::optional<AudioLink>& knownSpeaker);
}
