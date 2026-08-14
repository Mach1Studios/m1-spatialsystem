#pragma once

#include <cstddef>

namespace Mach1::ProjectSessionPolicy {

enum class Action
{
    None,
    AutoStart,
    Prompt
};

constexpr Action chooseAction(std::size_t streamingHostCount,
                              bool singleHostHasNamedBinding,
                              bool singleHostIsAmbiguous,
                              bool captureIsActive)
{
    if (captureIsActive || streamingHostCount == 0)
        return Action::None;
    if (streamingHostCount == 1
        && singleHostHasNamedBinding
        && !singleHostIsAmbiguous)
        return Action::AutoStart;
    return Action::Prompt;
}

} // namespace Mach1::ProjectSessionPolicy
