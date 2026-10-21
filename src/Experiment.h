#pragma once
#include <functional>
#include <utility>
#include <vector>

namespace RE { class TESObjectARMO; }

namespace DDAR::Experiment
{
    void RegisterInput(std::function<void()> snapshot);
    void BeforeLoad();
    bool IsArmed();
    std::vector<std::pair<RE::TESObjectARMO*, RE::TESObjectARMO*>> DevicePairs();
    void ApplySelection(RE::TESObjectARMO* source, RE::TESObjectARMO* inventory, bool allowExtra52);
    void RestoreSelection();
    void RequestSnapshot();
}
