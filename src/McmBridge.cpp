#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>
#include "McmBridge.h"
#include "Experiment.h"

namespace DDAR::MCM {
namespace {
std::mutex guard;
std::vector<RE::TESObjectARMO*> sources, targets;
std::vector<std::pair<RE::TESObjectARMO*, RE::TESObjectARMO*>> devices;
std::string status = "Disabled. Refresh catalog to begin.";
std::string catalogStatus;
struct Pending { int operation = 0; RE::TESObjectARMO* a = nullptr; RE::TESObjectARMO* b = nullptr; bool extra = false; } pending;
std::string Slots(std::uint32_t mask) {
    std::string text;
    for (unsigned i=0; i<32; ++i) if (mask & (1u<<i)) { if (!text.empty()) text += ","; text += std::to_string(30+i); }
    return text.empty() ? "none" : text;
}
std::string Label(RE::TESObjectARMO* a) {
    if (!a) return "None";
    auto* file = a->GetFile(0);
    return std::string(a->GetName()) + " [" + std::string(file ? file->GetFilename() : "dynamic") + "]";
}
void RunPending() {
    if (auto* ui=RE::UI::GetSingleton(); ui && ui->GameIsPaused()) return;
    Pending work;
    { std::lock_guard lock(guard); work=pending; pending={}; }
    if (!work.operation) return;
    try {
        if (work.operation==3) { Experiment::RequestSnapshot(); std::lock_guard lock(guard); status="Snapshot requested: SKSE/DDARProbe.log"; return; }
        if (work.operation==1) Experiment::ApplySelection(work.a,work.b,work.extra);
        else Experiment::RestoreSelection();
        std::lock_guard lock(guard);
        status=work.operation==1 ? "Applied; check appearance and DD behavior in game." : "Disabled; original DD armatures selected. Verify appearance.";
        RE::DebugNotification(work.operation==1 ? "DDAR: applied (verify visually)" : "DDAR: restored");
    } catch (const std::exception& e) {
        // A rejected replacement must not leave the previous mapping active.
        try { Experiment::RestoreSelection(); }
        catch (const std::exception& restoreError) { spdlog::error("MCM restore error: {}",restoreError.what()); }
        std::lock_guard lock(guard); status=std::string("Refused: ")+e.what(); spdlog::error("MCM {}",status);
        RE::DebugNotification("DDAR refused; reopen MCM for details");
    }
}
class Menus final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* e, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if (e && !e->opening) SKSE::GetTaskInterface()->AddTask(RunPending);
        return RE::BSEventNotifyControl::kContinue;
    }
} menus;
void Refresh(RE::StaticFunctionTag*, RE::TESObjectARMO* a, bool overlap) {
    SKSE::GetTaskInterface()->AddTask([a,overlap] {
        try {
            auto pairs=Experiment::DevicePairs();
            std::vector<RE::TESObjectARMO*> nextSources, nextTargets;
            if (auto* player=RE::PlayerCharacter::GetSingleton()) {
                for (const auto& [form,entry] : player->GetInventory()) {
                    auto* armor=form->As<RE::TESObjectARMO>();
                    if (!armor || entry.first<=0 || armor->IsDeleted() || armor->IsDynamicForm()) continue;
                    if (std::ranges::any_of(pairs,[armor](auto& p){return p.first==armor || p.second==armor;})) continue;
                    nextSources.push_back(armor);
                }
            }
            for (auto& [inv,render] : pairs) {
                auto mask=static_cast<std::uint32_t>(render->GetSlotMask());
                auto sourceMask=a ? static_cast<std::uint32_t>(a->GetSlotMask()) : 0u;
                if (!a || (overlap ? (mask & sourceMask)!=0 : mask==sourceMask)) nextTargets.push_back(inv);
            }
            auto less=[](auto* x,auto* y){return Label(x)<Label(y);};
            std::ranges::sort(nextSources,less); std::ranges::sort(nextTargets,less);
            std::lock_guard lock(guard); sources=std::move(nextSources); targets=std::move(nextTargets); devices=std::move(pairs);
            catalogStatus="Catalog ready: A="+std::to_string(sources.size())+"; B="+std::to_string(targets.size())+"; registered="+std::to_string(devices.size());
        } catch (const std::exception& e) { std::lock_guard lock(guard); sources.clear(); targets.clear(); devices.clear(); catalogStatus=std::string("Catalog failed: ")+e.what(); }
    });
}
std::vector<std::string> Labels(RE::StaticFunctionTag*, bool target, std::int32_t page) {
    std::lock_guard lock(guard); const auto& list=target ? targets:sources;
    std::vector<std::string> result;
    if (page<0) return result;
    for (std::size_t i=static_cast<std::size_t>(page)*20; i<list.size() && result.size()<20; ++i) result.push_back(Label(list[i]));
    if (result.empty()) result.push_back("(No entries - refresh catalog)");
    return result;
}
std::int32_t Count(RE::StaticFunctionTag*, bool target) { std::lock_guard lock(guard); return static_cast<std::int32_t>((target?targets:sources).size()); }
RE::TESObjectARMO* At(RE::StaticFunctionTag*, bool target, std::int32_t index) {
    std::lock_guard lock(guard); const auto& list=target?targets:sources;
    return index>=0 && static_cast<std::size_t>(index)<list.size() ? list[index]:nullptr;
}
std::string Describe(RE::StaticFunctionTag*, RE::TESObjectARMO* a, bool target) {
    std::lock_guard lock(guard);
    auto* render=a;
    if(target) { render=nullptr; for(auto& p:devices) if(p.first==a) {render=p.second;break;} }
    if(!render) return "None / not registered";
    auto result=Label(a)+fmt::format(" [{:08X}]",a->GetFormID());
    if(target) {
        result+=" | Rendered: "+Label(render)+fmt::format(" [{:08X}]",render->GetFormID());
        auto shared=std::ranges::count_if(devices,[render](auto& p){return p.second==render;});
        if(shared>1) result+=" | SHARED rendered: "+std::to_string(shared)+" inventory devices (mapping affects all)";
    }
    result+=" | ARMO slots: "+Slots(static_cast<std::uint32_t>(render->GetSlotMask()));
    for(auto* addon:render->armorAddons) if(addon) result+=" | ARMA: "+Slots(static_cast<std::uint32_t>(addon->GetSlotMask()));
    return result;
}
std::string Status(RE::StaticFunctionTag*) { std::lock_guard lock(guard); return status+"\n"+catalogStatus; }
bool Active(RE::StaticFunctionTag*) { return Experiment::IsArmed(); }
void Snapshot(RE::StaticFunctionTag*) {
    {std::lock_guard lock(guard); pending={3}; status="Snapshot queued; close menus.";}
    SKSE::GetTaskInterface()->AddTask(RunPending);
}
void Queue(RE::StaticFunctionTag*, RE::TESObjectARMO* a, RE::TESObjectARMO* b, bool extra, bool restore) {
    { std::lock_guard lock(guard); pending={restore?2:1,a,b,extra}; status="Queued. Close all paused menus to execute."; }
    SKSE::GetTaskInterface()->AddTask(RunPending);
}
}
bool Register(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("RefreshCatalog","DDAR_Native",Refresh);
    vm->RegisterFunction("GetLabels","DDAR_Native",Labels);
    vm->RegisterFunction("GetCount","DDAR_Native",Count);
    vm->RegisterFunction("GetAt","DDAR_Native",At);
    vm->RegisterFunction("Describe","DDAR_Native",Describe);
    vm->RegisterFunction("GetStatus","DDAR_Native",Status);
    vm->RegisterFunction("IsActive","DDAR_Native",Active);
    vm->RegisterFunction("QueueChange","DDAR_Native",Queue);
    vm->RegisterFunction("Snapshot","DDAR_Native",Snapshot);
    return true;
}
void RegisterMenus() { if(auto* ui=RE::UI::GetSingleton()) ui->AddEventSink(&menus); }
void BeforeLoad() { std::lock_guard lock(guard); pending={}; sources.clear(); targets.clear(); devices.clear(); catalogStatus.clear(); status="Disabled after load. Refresh catalog, then apply manually."; }
void GameLoaded() { BeforeLoad(); }
}
