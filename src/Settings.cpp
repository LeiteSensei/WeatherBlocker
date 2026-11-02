#include "Settings.h"

namespace WB::Settings
{
    namespace
    {
        std::mutex            g_mutex;
        Config                g_active;
        std::atomic_uint64_t  g_version{ 0 };

        std::string Hex(std::uint32_t v) { return std::format("0x{:06X}", v); }

        std::uint32_t ParseHex(const std::string& s)
        {
            try {
                return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16));
            } catch (...) {
                return 0;
            }
        }

        std::string TimeToString(int minutes)
        {
            minutes = std::clamp(minutes, 0, 1439);
            return std::format("{:02}:{:02}", minutes / 60, minutes % 60);
        }

        int StringToTime(const std::string& s, int fallback)
        {
            int h = 0, m = 0;
            if (std::sscanf(s.c_str(), "%d:%d", &h, &m) != 2) {
                return fallback;
            }
            return std::clamp(h, 0, 23) * 60 + std::clamp(m, 0, 59);
        }
    }

    std::filesystem::path Path()
    {
        return std::filesystem::path("Data/SKSE/Plugins/WeatherBlocker.json");
    }

    Config ReadFromDisk()
    {
        Config cfg;

        std::ifstream file(Path());
        if (!file) {
            logger::info("No config file found, using defaults");
            return cfg;
        }

        try {
            const auto j = nlohmann::json::parse(file, nullptr, true, true);

            cfg.modEnabled = j.value("enabled", true);
            cfg.respectOverrides = j.value("respectOverrides", true);
            cfg.pollMs = std::clamp(j.value("pollMs", 100), 50, 1000);
            cfg.instantSwitch = j.value("instantSwitch", true);
            cfg.showNotification = j.value("showNotification", true);

            if (j.contains("fallback") && j["fallback"].is_object()) {
                const auto& f = j["fallback"];
                cfg.fallbackMode = std::clamp(f.value("mode", 0), 0, 1);
                cfg.fallbackPlugin = f.value("plugin", ""s);
                cfg.fallbackLocalID = ParseHex(f.value("formID", "0x0"s));
                cfg.fallbackLabel = f.value("label", ""s);
            }

            if (j.contains("rules") && j["rules"].is_array()) {
                for (const auto& jr : j["rules"]) {
                    Rule r;
                    r.plugin = jr.value("plugin", ""s);
                    r.localID = ParseHex(jr.value("formID", "0x0"s));
                    r.label = jr.value("label", ""s);
                    r.enabled = jr.value("enabled", true);
                    r.allDay = jr.value("allDay", true);

                    if (jr.contains("windows") && jr["windows"].is_array()) {
                        for (const auto& jw : jr["windows"]) {
                            TimeWindow w;
                            w.startMin = StringToTime(jw.value("start", "18:00"s), 18 * 60);
                            w.endMin = StringToTime(jw.value("end", "05:00"s), 5 * 60);
                            r.windows.push_back(w);
                        }
                    }

                    if (r.plugin.empty()) {
                        continue;
                    }
                    if (r.label.empty()) {
                        r.label = std::format("{}|{}", r.plugin, Hex(r.localID));
                    }
                    cfg.rules.push_back(std::move(r));
                }
            }
        } catch (const std::exception& e) {
            logger::error("Failed to parse {}: {}", Path().string(), e.what());
        }

        return cfg;
    }

    void Init()
    {
        auto cfg = ReadFromDisk();
        std::scoped_lock lock(g_mutex);
        g_active = std::move(cfg);
        ++g_version;
    }

    bool Apply(const Config& cfg)
    {
        {
            std::scoped_lock lock(g_mutex);
            g_active = cfg;
            ++g_version;
        }

        nlohmann::json j;
        j["enabled"] = cfg.modEnabled;
        j["respectOverrides"] = cfg.respectOverrides;
        j["pollMs"] = cfg.pollMs;
        j["instantSwitch"] = cfg.instantSwitch;
        j["showNotification"] = cfg.showNotification;
        j["fallback"] = {
            { "mode", cfg.fallbackMode },
            { "plugin", cfg.fallbackPlugin },
            { "formID", Hex(cfg.fallbackLocalID) },
            { "label", cfg.fallbackLabel }
        };

        auto rules = nlohmann::json::array();
        for (const auto& r : cfg.rules) {
            nlohmann::json jr;
            jr["plugin"] = r.plugin;
            jr["formID"] = Hex(r.localID);
            jr["label"] = r.label;
            jr["enabled"] = r.enabled;
            jr["allDay"] = r.allDay;

            auto windows = nlohmann::json::array();
            for (const auto& w : r.windows) {
                windows.push_back({ { "start", TimeToString(w.startMin) }, { "end", TimeToString(w.endMin) } });
            }
            jr["windows"] = windows;
            rules.push_back(std::move(jr));
        }
        j["rules"] = std::move(rules);

        try {
            const auto path = Path();
            std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::trunc);
            if (!out) {
                logger::error("Could not open {} for writing", path.string());
                return false;
            }
            out << j.dump(2);
            return true;
        } catch (const std::exception& e) {
            logger::error("Failed to save config: {}", e.what());
            return false;
        }
    }

    Config GetActive()
    {
        std::scoped_lock lock(g_mutex);
        return g_active;
    }

    std::uint64_t Version()
    {
        return g_version.load();
    }

    bool InWindow(int nowMin, const TimeWindow& w)
    {
        if (w.startMin == w.endMin) {
            return true;  // faixa inteira
        }
        if (w.startMin < w.endMin) {
            return nowMin >= w.startMin && nowMin < w.endMin;
        }
        return nowMin >= w.startMin || nowMin < w.endMin;  // atravessa a meia-noite
    }

    bool RuleActiveAt(const Rule& r, int nowMin)
    {
        if (!r.enabled) {
            return false;
        }
        if (r.allDay) {
            return true;
        }
        return std::ranges::any_of(r.windows, [&](const TimeWindow& w) { return InWindow(nowMin, w); });
    }
}
