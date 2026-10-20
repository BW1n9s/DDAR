#pragma once
#include <functional>

namespace DDAR::Experiment
{
    void RegisterInput(std::function<void()> snapshot);
    void BeforeLoad();
    bool IsArmed();
}
