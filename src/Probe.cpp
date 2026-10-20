// Phase 0 diagnostics plus an opt-in, local-binary-gated rendering experiment.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <exception>
#include <iterator>
#include <memory>
#include "Experiment.h"

namespace
{
    std::atomic_bool pending{false};

    void Describe(RE::TESForm* form)
    {
        if (!form) {
            return;
        }
        auto* owner = form->GetFile(0);
        const auto id = form->GetFormID();
        const auto local = owner ? id & (owner->IsLight() ? 0xFFFu : 0xFFFFFFu) : 0;
        spdlog::info("form={:08X} owner={} local={:06X} name={}", id,
            owner ? owner->GetFilename() : "<dynamic-or-unresolved>", local, form->GetName());
        if (auto* armor = form->As<RE::TESObjectARMO>()) {
            spdlog::info("armor slots={:08X} addons={}", static_cast<std::uint32_t>(armor->GetSlotMask()),
                armor->armorAddons.size());
            for (auto* addon : armor->armorAddons) {
                if (!addon) {
                    continue;
                }
                spdlog::info("arma={:08X} slots={:08X} male={} female={}",
                    addon->GetFormID(), static_cast<std::uint32_t>(addon->GetSlotMask()),
                    addon->bipedModels[0].GetModel(), addon->bipedModels[1].GetModel());
            }
        }
    }

    void Snapshot()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) {
            spdlog::info("snapshot skipped: player 3D unavailable");
            return;
        }
        spdlog::info("snapshot actor={:08X} experimentArmed={} visibleResult=unverified",
            player->GetFormID(), DDAR::Experiment::IsArmed());
        auto inventory = player->GetInventory([](RE::TESBoundObject& object) {
            return object.IsArmor();
        });
        for (const auto& [form, entry] : inventory) {
            if (entry.first <= 0) {
                continue;
            }
            spdlog::info("inventory count={} worn={}", entry.first,
                entry.second && entry.second->IsWorn());
            Describe(form);
        }
        for (bool firstPerson : {false, true}) {
            const auto& biped = player->GetBiped(firstPerson);
            if (!biped) {
                continue;
            }
            for (std::size_t i = 0; i < std::size(biped->objects); ++i) {
                const auto& object = biped->objects[i];
                if (!object.item) {
                    continue;
                }
                spdlog::info("biped firstPerson={} index={} item={:08X} addon={:08X} clonePresent={}",
                    firstPerson, i, object.item->GetFormID(),
                    object.addon ? object.addon->GetFormID() : 0, bool(object.partClone));
            }
        }
        // clonePresent only means a pointer exists; it does not prove visible geometry.
        spdlog::default_logger()->flush();
    }

    void RequestSnapshot()
    {
        if (pending.exchange(true)) {
            return;
        }
        SKSE::GetTaskInterface()->AddTask([] {
            pending.store(false);
            Snapshot();
        });
    }

    class EquipObserver final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event,
            RE::BSTEventSource<RE::TESEquipEvent>*) override
        {
            if (event && event->actor.get() == RE::PlayerCharacter::GetSingleton()) {
                // Do not infer which mod caused an equip event.
                spdlog::info("equip actor=00000014 form={:08X} equipped={}",
                    event->baseObject, event->equipped);
                RequestSnapshot();
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    } observer;

    void OnMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type == SKSE::MessagingInterface::kInputLoaded) {
            DDAR::Experiment::RegisterInput(RequestSnapshot);
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
            DDAR::Experiment::BeforeLoad();
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) {
                source->AddEventSink<RE::TESEquipEvent>(&observer);
            }
            spdlog::info("data loaded: experiment remains disabled until explicit validated toggle");
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame) {
            DDAR::Experiment::BeforeLoad();
            RequestSnapshot();
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEAPI SKSEPlugin_Query(
    const SKSE::QueryInterface* skse, SKSE::PluginInfo* info)
{
    info->infoVersion = SKSE::PluginInfo::kVersion;
    info->name = "DDARProbe";
    info->version = 2;
    // AE support is intentionally not claimed by this SE-only experiment.
    return !skse->IsEditor() && skse->RuntimeVersion() == SKSE::RUNTIME_SSE_1_5_97;
}

extern "C" __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    if (skse->RuntimeVersion() != SKSE::RUNTIME_SSE_1_5_97) {
        return false;
    }
    SKSE::Init(skse);
    auto directory = SKSE::log::log_directory();
    if (!directory) {
        return false;
    }
    try {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            (*directory / "DDARProbe.log").string(), false);
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("DDARProbe", sink));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("DDARProbe 0.0.2 runtime={} appearanceBackend=experimental-default-disabled",
            skse->RuntimeVersion().string());
        return SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    } catch (const std::exception&) {
        return false;
    }
}
