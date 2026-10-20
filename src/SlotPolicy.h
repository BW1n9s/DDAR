#pragma once
#include <cstdint>

namespace DDAR
{
    // Phase 0 scope is one belt, not a general cross-slot compatibility policy.
    constexpr bool BeltSlotsAllowed(std::uint32_t sourceArmor, std::uint32_t sourceAddon,
        std::uint32_t targetArmor, std::uint32_t targetAddon, bool allowExtra52)
    {
        constexpr auto belt = 1u << (49 - 30);
        constexpr auto extra = 1u << (52 - 30);
        return sourceArmor == belt && sourceAddon == belt && targetArmor == belt &&
            (targetAddon == belt || (allowExtra52 && targetAddon == (belt | extra)));
    }
}
