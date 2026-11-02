#include "Blocker.h"

#include "Settings.h"
#include "WeatherDB.h"

namespace WB::Blocker
{
    namespace
    {
        std::atomic_bool g_running{ false };
        std::atomic_bool g_pending{ false };
        std::atomic_int  g_pollMs{ 100 };

        // Estado usado so pela thread principal (dentro das tasks)
        std::uint64_t                s_version = ~0ull;
        Config                       s_cfg;
        std::vector<RE::TESWeather*> s_resolved;  // paralelo a s_cfg.rules
        RE::TESWeather*              s_fallback = nullptr;
        RE::TESWeather*              s_lastGood = nullptr;
        std::chrono::steady_clock::time_point s_lastAction{};

        void RefreshConfig()
        {
            const auto version = Settings::Version();
            if (version == s_version) {
                return;
            }
            s_version = version;
            s_cfg = Settings::GetActive();
            g_pollMs = std::clamp(s_cfg.pollMs, 50, 1000);

            auto* dataHandler = RE::TESDataHandler::GetSingleton();

            s_resolved.assign(s_cfg.rules.size(), nullptr);
            s_fallback = nullptr;
            if (!dataHandler) {
                return;
            }

            for (std::size_t i = 0; i < s_cfg.rules.size(); ++i) {
                const auto& r = s_cfg.rules[i];
                s_resolved[i] = dataHandler->LookupForm<RE::TESWeather>(r.localID, r.plugin);
                if (!s_resolved[i]) {
                    logger::warn("Rule '{}' ({}|{:06X}) not found in current load order", r.label, r.plugin, r.localID);
                }
            }

            if (s_cfg.fallbackMode == 1 && !s_cfg.fallbackPlugin.empty()) {
                s_fallback = dataHandler->LookupForm<RE::TESWeather>(s_cfg.fallbackLocalID, s_cfg.fallbackPlugin);
            }

            logger::info("Config applied: {} rules, enabled={}", s_cfg.rules.size(), s_cfg.modEnabled);
        }

        bool IsBlocked(const RE::TESWeather* weather, int nowMin)
        {
            if (!weather) {
                return false;
            }
            for (std::size_t i = 0; i < s_cfg.rules.size(); ++i) {
                if (s_resolved[i] == weather && Settings::RuleActiveAt(s_cfg.rules[i], nowMin)) {
                    return true;
                }
            }
            return false;
        }

        RE::TESWeather* PickReplacement(const RE::Sky* sky, const RE::TESWeather* current, int nowMin)
        {
            const auto usable = [&](RE::TESWeather* w) { return w && w != current && !IsBlocked(w, nowMin); };

            if (s_cfg.fallbackMode == 1 && usable(s_fallback)) {
                return s_fallback;
            }
            if (usable(sky->lastWeather)) {
                return sky->lastWeather;
            }
            if (usable(s_lastGood)) {
                return s_lastGood;
            }
            if (usable(sky->defaultWeather)) {
                return sky->defaultWeather;
            }
            return nullptr;
        }

        void Tick()
        {
            RefreshConfig();

            if (!s_cfg.modEnabled || s_cfg.rules.empty()) {
                return;
            }

            auto* sky = RE::Sky::GetSingleton();
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* calendar = RE::Calendar::GetSingleton();
            if (!sky || !player || !calendar || !player->Is3DLoaded()) {
                return;
            }

            // So em exterior com ceu completo
            if (sky->mode.get() != RE::Sky::Mode::kFull) {
                return;
            }

            // Respeita clima forcado por quest/script
            if (s_cfg.respectOverrides && sky->overrideWeather) {
                return;
            }

            auto* current = sky->currentWeather;
            if (!current) {
                return;
            }

            const int nowMin = static_cast<int>(calendar->GetHour() * 60.0f) % 1440;

            if (!IsBlocked(current, nowMin)) {
                s_lastGood = current;
                return;
            }

            // Evita repetir a acao a cada tick se o motor demorar a aplicar
            const auto now = std::chrono::steady_clock::now();
            if (now - s_lastAction < 1500ms) {
                return;
            }
            s_lastAction = now;

            auto* replacement = PickReplacement(sky, current, nowMin);
            if (!replacement) {
                logger::warn("Weather {} is blocked but no replacement was found", WeatherDB::LabelOf(current));
                return;
            }

            logger::info("Blocked {} at {:02}:{:02} -> switching to {}",
                WeatherDB::LabelOf(current), nowMin / 60, nowMin % 60, WeatherDB::LabelOf(replacement));

            if (s_cfg.instantSwitch) {
                sky->ForceWeather(replacement, false);   // troca imediata
            } else {
                sky->SetWeather(replacement, false, true);  // transicao acelerada do motor
            }

            if (s_cfg.showNotification) {
                const auto text = std::format("Weather Blocker: {} blocked", WeatherDB::ShortName(current));
                RE::DebugNotification(text.c_str());
            }
        }
    }

    void Start()
    {
        if (g_running.exchange(true)) {
            return;
        }

        Settings::Init();

        std::thread([] {
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(g_pollMs.load()));

                if (g_pending.exchange(true)) {
                    continue;  // tick anterior ainda nao rodou (ex.: loading screen)
                }

                SKSE::GetTaskInterface()->AddTask([] {
                    try {
                        Tick();
                    } catch (const std::exception& e) {
                        logger::error("Tick failed: {}", e.what());
                    }
                    g_pending = false;
                });
            }
        }).detach();

        logger::info("Blocker started");
    }
}
