// Experimental player-only ARMA selection. No shared form mutation or equipment calls.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <spdlog/spdlog.h>

#include "Experiment.h"
#include "SlotPolicy.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "../third_party/DDNG/API.h"

namespace DDAR::Experiment
{
    namespace
    {
        using Build = void (*)(RE::TESObjectARMA*, RE::TESObjectARMO*,
            RE::BSTSmartPointer<RE::BipedAnim>*, RE::SEX);
        using Config = std::map<std::string, std::string>;
        struct Mapping
        {
            RE::TESObjectARMO* source;
            RE::TESObjectARMO* inventory;
            RE::TESObjectARMO* rendered;
            RE::TESObjectARMA* sourceAddon;
            RE::TESObjectARMA* targetAddon;
        };
        std::atomic<std::shared_ptr<const Mapping>> active;
        std::atomic<std::uint64_t> dispatched{0};
        Build original = nullptr;
        bool hooked = false;
        std::function<void()> snapshotCallback;

        std::filesystem::path ModulePath(HMODULE module)
        {
            std::array<wchar_t, 32768> path{};
            const auto n = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
            if (!n || n >= path.size()) throw std::runtime_error("Module path unavailable");
            return std::filesystem::path(std::wstring(path.data(), n));
        }

        std::filesystem::path DataRoot() { return ModulePath(nullptr).parent_path() / L"Data"; }

        std::string HashFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Required fingerprint file cannot be opened");
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            std::array<unsigned char, 32> digest{};
            auto cleanup = [&] {
                if (hash) BCryptDestroyHash(hash);
                if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
            };
            try {
                if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
                    BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
                    throw std::runtime_error("SHA256 initialization failed");
                std::array<char, 65536> buffer{};
                while (file.read(buffer.data(), buffer.size()) || file.gcount()) {
                    if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                        static_cast<ULONG>(file.gcount()), 0) < 0)
                        throw std::runtime_error("SHA256 update failed");
                }
                if (!file.eof() || BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
                    throw std::runtime_error("SHA256 read/finalization failed");
            } catch (...) { cleanup(); throw; }
            cleanup();
            constexpr char hex[] = "0123456789abcdef";
            std::string result;
            for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
            return result;
        }

        std::string Trim(std::string value)
        {
            auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return {};
            return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
        }

        Config ReadConfig()
        {
            std::ifstream file(DataRoot() / L"SKSE/Plugins/DDARExperiment.ini");
            if (!file) throw std::runtime_error("DDARExperiment.ini missing; no experiment enabled");
            Config result;
            std::string line;
            while (std::getline(file, line)) {
                line = Trim(line);
                if (line.empty() || line.front() == ';' || line.front() == '#' || line.front() == '[') continue;
                const auto at = line.find('=');
                if (at == std::string::npos) throw std::runtime_error("Invalid INI line");
                if (!result.emplace(Trim(line.substr(0, at)), Trim(line.substr(at + 1))).second)
                    throw std::runtime_error("Duplicate INI key");
            }
            if (!file.eof()) throw std::runtime_error("INI read failed");
            if (result["EnableExperiment"] != "1") throw std::runtime_error("Experiment is disabled in INI");
            return result;
        }

        std::string PluginName(const Config& config, const std::string& key)
        {
            const auto value = config.at(key);
            // The initial fixture intentionally supports ASCII plugin filenames only.
            if (value.empty() || value.find_first_of("/\\:") != std::string::npos || value.find("..") != std::string::npos ||
                std::any_of(value.begin(), value.end(), [](unsigned char c) { return c >= 128; }))
                throw std::runtime_error("Invalid or unsupported plugin filename");
            return value;
        }

        RE::TESObjectARMO* Armor(const Config& config, const std::string& prefix)
        {
            const auto plugin = PluginName(config, prefix + "Plugin");
            const auto text = config.at(prefix + "LocalID");
            std::uint32_t id = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id, 16);
            auto* handler = RE::TESDataHandler::GetSingleton();
            auto* file = handler ? handler->LookupModByName(plugin) : nullptr;
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
                !id || !file || id > (file->IsLight() ? 0xFFFu : 0xFFFFFFu))
                throw std::runtime_error("Invalid local FormID or unavailable plugin");
            auto* form = handler->LookupForm<RE::TESObjectARMO>(id, plugin);
            if (!form || form->IsDeleted() || form->IsDynamicForm()) throw std::runtime_error("Armor unavailable");
            return form;
        }

        void* VerifiedDDAPI()
        {
            const auto module = GetModuleHandleW(L"DeviousDevices.dll");
            if (!module) throw std::runtime_error("Loaded DD NG module missing");
            if (HashFile(ModulePath(module)) != "0d374dc7d6e2b1181795957edc19d6ad48dcd397afb6579d5a7ef511e185d274")
                throw std::runtime_error("DD NG differs from inspected local binary; experiment refused");
            const auto base = reinterpret_cast<std::uintptr_t>(module);
            const auto exportAddress = GetProcAddress(module, "GetAPI");
            if (reinterpret_cast<std::uintptr_t>(exportAddress) != base + 0x5F40)
                throw std::runtime_error("Unexpected local DD NG GetAPI export");
            auto* api = reinterpret_cast<void* (*)()>(exportAddress)();
            if (!api || *reinterpret_cast<std::uintptr_t*>(api) != base + 0x1FD410)
                throw std::runtime_error("DD NG API not initialized or vtable differs");
            const auto table = *reinterpret_cast<std::uintptr_t**>(api);
            constexpr std::array<std::uintptr_t, 4> slots{0xD8950, 0xD8960, 0xD8980, 0xD89A0};
            for (std::size_t i = 0; i < slots.size(); ++i)
                if (table[i] != base + slots[i]) throw std::runtime_error("DD NG API entry differs");
            if (reinterpret_cast<std::size_t (*)(const void*)>(table[0])(api) != 2)
                throw std::runtime_error("DD NG API version is not 2");
            return api;
        }

        bool ModelReadable(const char* path)
        {
            if (!path || !*path) return false;
            RE::BSResourceNiBinaryStream stream(std::string("meshes\\") + path);
            // Resource availability only. Full NIF/physics correctness remains a game test.
            return stream.good();
        }

        std::shared_ptr<const Mapping> Validate(RE::TESObjectARMO* selectedSource = nullptr,
            RE::TESObjectARMO* selectedTarget = nullptr, bool selectedAllow52 = false)
        {
            const bool fromMCM = selectedSource && selectedTarget;
            const auto config = fromMCM ? Config{} : ReadConfig();
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->Is3DLoaded() || !player->GetActorBase() ||
                player->GetActorBase()->GetSex() != RE::SEX::kFemale)
                throw std::runtime_error("Phase 0 requires a loaded female player");
            auto* source = fromMCM ? selectedSource : Armor(config, "Source");
            auto* inventory = fromMCM ? selectedTarget : Armor(config, "Target");
            RE::TESObjectARMO* rendered = nullptr;
            const auto pairs = DevicePairs();
            for (const auto& pair : pairs) if (pair.first == inventory) { rendered = pair.second; break; }
            if (!rendered) throw std::runtime_error("B is not registered in the actual DD NG device database");
            for (const auto& pair : pairs)
                if (pair.first == source || pair.second == source)
                    throw std::runtime_error("A must be ordinary armor, not a registered DD device");
            if (source->IsDeleted() || source->IsDynamicForm() || inventory->IsDeleted() || inventory->IsDynamicForm())
                throw std::runtime_error("Temporary/deleted armor cannot be mapped");
            std::string sourcePlugin;
            std::string winningPlugin;
            if (!fromMCM) {
                sourcePlugin = PluginName(config, "SourcePlugin");
                winningPlugin = PluginName(config, "TargetWinningPlugin");
                if (HashFile(DataRoot() / sourcePlugin) != config.at("SourceSHA256") ||
                    HashFile(DataRoot() / winningPlugin) != config.at("TargetWinningSHA256") ||
                    rendered != Armor(config, "Rendered"))
                    throw std::runtime_error("Armor plugin/pair changed from recorded fixture");
            }
            const auto lastIs = [](RE::TESForm* form, const std::string& plugin) {
                auto* file = form->GetFile(-1);
                return file && file->GetFilename() == plugin;
            };
            if (!fromMCM && (!lastIs(source, sourcePlugin) || !lastIs(inventory, winningPlugin) || !lastIs(rendered, winningPlugin)))
                throw std::runtime_error("Winning Armor override differs from the inspected fixture");
            if (source == inventory || source == rendered || inventory == rendered)
                throw std::runtime_error("Source, inventory and rendered forms must differ");
            if (source->armorAddons.size() != 1 || rendered->armorAddons.size() != 1)
                throw std::runtime_error("Phase 0 only supports one ARMA per Armor");
            auto* a = source->armorAddons[0];
            auto* b = rendered->armorAddons[0];
            if (!a || !b || (!fromMCM && (!lastIs(a, sourcePlugin) || !lastIs(b, winningPlugin))))
                throw std::runtime_error("Winning ARMA override differs from the inspected fixture");
            const bool allow52 = fromMCM ? selectedAllow52 :
                (config.contains("AllowExtraTargetSlot52") && config.at("AllowExtraTargetSlot52") == "1");
            if (!a || !b || a->IsDeleted() || b->IsDeleted() ||
                !BeltSlotsAllowed(static_cast<std::uint32_t>(source->GetSlotMask()),
                    static_cast<std::uint32_t>(a->GetSlotMask()), static_cast<std::uint32_t>(rendered->GetSlotMask()),
                    static_cast<std::uint32_t>(b->GetSlotMask()), allow52))
                throw std::runtime_error("Slot/ARMA policy rejected the mapping");
            if (!a->IsValidRace(player->GetRace()) || !b->IsValidRace(player->GetRace()))
                throw std::runtime_error("ARMA race compatibility failed");
            if (!ModelReadable(a->bipedModels[1].GetModel()) || !ModelReadable(b->bipedModels[1].GetModel()))
                throw std::runtime_error("Female model resource cannot be opened");
            const auto* first = a->bipedModel1stPersons[1].GetModel();
            if (first && *first && !ModelReadable(first)) throw std::runtime_error("First person resource unavailable");
            const auto counts = player->GetInventoryCounts();
            const auto has = [&](RE::TESBoundObject* form) { const auto i = counts.find(form); return i != counts.end() && i->second > 0; };
            if (!has(source)) throw std::runtime_error("A must be in player inventory; B may be absent and neither must be equipped to register");
            if (allow52) spdlog::warn("EXPERIMENT: target ARMA extra slot 52 explicitly allowed; NIF partitions and restoration are unverified");
            spdlog::info("validated DD pair inventory={:08X} rendered={:08X} source={:08X}",
                inventory->GetFormID(), rendered->GetFormID(), source->GetFormID());
            return std::make_shared<Mapping>(source, inventory, rendered, a, b);
        }

        void SelectAddon(RE::TESObjectARMA* addon, RE::TESObjectARMO* armor,
            RE::BSTSmartPointer<RE::BipedAnim>* biped, RE::SEX sex)
        {
            const auto mapping = active.load();
            if (mapping && armor == mapping->rendered && addon == mapping->targetAddon &&
                sex == RE::SEX::kFemale && biped && *biped) {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (player && ((*biped).get() == player->GetBiped(false).get() ||
                    (*biped).get() == player->GetBiped(true).get()) && mapping->sourceAddon->IsValidRace(player->GetRace())) {
                    dispatched.fetch_add(1);
                    spdlog::info("substitution DISPATCHED actor=00000014 armorB={:08X} armaA={:08X}; visibleResult=unverified",
                        armor->GetFormID(), mapping->sourceAddon->GetFormID());
                    original(mapping->sourceAddon, armor, biped, sex);
                    return;
                }
            }
            original(addon, armor, biped, sex);
        }

        void InstallHook()
        {
            if (hooked) return;
            if (GetModuleHandleW(L"DynamicArmorVariants.dll")) throw std::runtime_error("DynamicArmorVariants not supported by this experiment");
            if (HashFile(ModulePath(nullptr)) != "5666e1bddd01bcab31ecf11691ef1a3f22e1541af79f2bc0e55318533cfe5d12")
                throw std::runtime_error("Game executable differs from inspected local binary");
            const auto target = REL::Relocation<std::uintptr_t>(REL::ID(17361)).address();
            constexpr std::array<unsigned char, 24> expected{
                0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
                0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x30,0xFE,0xFF,0xFF};
            if (target != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + 0x227070 ||
                std::memcmp(reinterpret_cast<void*>(target), expected.data(), expected.size()) != 0)
                throw std::runtime_error("ARMA entry is modified or relocation differs; no hook installed");
            const auto init = MH_Initialize();
            if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) throw std::runtime_error("MinHook initialization failed");
            auto* address = reinterpret_cast<void*>(target);
            if (MH_CreateHook(address, reinterpret_cast<void*>(&SelectAddon), reinterpret_cast<void**>(&original)) != MH_OK)
                throw std::runtime_error("MinHook cannot create ARMA trampoline");
            if (MH_EnableHook(address) != MH_OK) {
                MH_RemoveHook(address);
                original = nullptr;
                throw std::runtime_error("MinHook cannot enable ARMA trampoline");
            }
            hooked = true;
            spdlog::info("Experimental downstream ARMA hook installed; DD Hider patch region untouched");
        }

        void Refresh()
        {
            if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->Is3DLoaded()) {
                // This is the equipment refresh entry used by the inspected DD Hider.
                using Update = bool (*)(RE::Actor*);
                REL::Relocation<Update> update{REL::ID(19316)};
                spdlog::info("refresh requested active={} dispatched={}", bool(active.load()), dispatched.load());
                const auto accepted = update(player);
                spdlog::info("equipment refresh returned={} visibleResult=unverified", accepted);
            }
            if (snapshotCallback) snapshotCallback();
        }

        void Command(std::uint32_t key)
        {
            try {
                if (key == 67 || (key == 68 && active.load())) {
                    active.store(nullptr);
                    spdlog::info("RESTORE requested; mapping disabled; original B ARMA selected on subsequent builds");
                    Refresh();
                } else if (key == 68) {
                    const auto mapping = Validate();
                    InstallHook();
                    active.store(mapping);
                    spdlog::info("Experiment armed; B identity retained; visual and DD-lock behavior still require game verification");
                    Refresh();
                } else if (key == 66 && snapshotCallback) snapshotCallback();
            } catch (const std::exception& error) {
                active.store(nullptr);
                spdlog::error("Experiment refused: {}", error.what());
                RE::DebugNotification("DDAR experiment refused; see DDARProbe.log");
            }
        }

        class InputObserver final : public RE::BSTEventSink<RE::InputEvent*>
        {
            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,
                RE::BSTEventSource<RE::InputEvent*>*) override
            {
                if (!events || !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) return RE::BSEventNotifyControl::kContinue;
                for (auto* event = *events; event; event = event->next) {
                    auto* button = event->AsButtonEvent();
                    if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown()) continue;
                    const auto key = button->GetIDCode();
                    if (key < 66 || key > 68) continue;
                    SKSE::GetTaskInterface()->AddTask([key] {
                        auto* ui = RE::UI::GetSingleton();
                        if (ui && !ui->GameIsPaused()) Command(key);
                    });
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        } input;
    }

    void RegisterInput(std::function<void()> snapshot)
    {
        snapshotCallback = std::move(snapshot);
        if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) manager->AddEventSink(&input);
        spdlog::info("Phase 0 controls Ctrl+F8 snapshot, Ctrl+F9 restore, Ctrl+F10 toggle; default DISABLED");
    }

    void BeforeLoad() { active.store(nullptr); dispatched.store(0); }
    bool IsArmed() { return bool(active.load()); }

    std::vector<std::pair<RE::TESObjectARMO*, RE::TESObjectARMO*>> DevicePairs()
    {
        // Published DD API v2: reference only, no foreign-container copies or mutation.
        // MSVC layout was checked against the inspected local DLL (node +0x28 +0xA0 = rendered +0xC8).
        static_assert(offsetof(DeviousDevicesAPI::DeviceUnitPrototype, deviceInventory) == 0x98);
        static_assert(offsetof(DeviousDevicesAPI::DeviceUnitPrototype, deviceRendered) == 0xA0);
        auto* api = static_cast<DeviousDevicesAPI::DeviousDevicesAPI*>(VerifiedDDAPI());
        std::vector<std::pair<RE::TESObjectARMO*, RE::TESObjectARMO*>> result;
        for (const auto& [inventory, unit] : api->GetDatabase()) {
            auto* rendered = unit.deviceRendered;
            if (inventory && rendered && unit.deviceInventory == inventory && !inventory->IsDeleted() &&
                !rendered->IsDeleted() && !inventory->IsDynamicForm() && !rendered->IsDynamicForm())
                result.emplace_back(inventory, rendered);
        }
        return result;
    }

    void ApplySelection(RE::TESObjectARMO* source, RE::TESObjectARMO* target, bool allowExtra52)
    {
        if (!source || !target) throw std::runtime_error("Select A and B in MCM first");
        const auto mapping = Validate(source, target, allowExtra52);
        InstallHook();
        active.store(mapping);
        spdlog::info("MCM mapping armed inventory={:08X} rendered={:08X}; visibleResult=unverified",
            mapping->inventory->GetFormID(), mapping->rendered->GetFormID());
        Refresh();
    }

    void RestoreSelection()
    {
        const bool hadMapping = bool(active.exchange(nullptr));
        spdlog::info("MCM restore requested; original B armatures will be selected");
        if (hadMapping) Refresh();
    }

    void RequestSnapshot() { if (snapshotCallback) snapshotCallback(); }
}
