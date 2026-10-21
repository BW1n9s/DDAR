#pragma once
namespace RE::BSScript { class IVirtualMachine; }
namespace DDAR::MCM
{
    bool Register(RE::BSScript::IVirtualMachine* vm);
    void GameLoaded();
    void BeforeLoad();
    void RegisterMenus();
}
