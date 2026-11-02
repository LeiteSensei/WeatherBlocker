#include "WeatherDB.h"

namespace WB::WeatherDB
{
    namespace
    {
        std::vector<WeatherInfo> g_all;

        std::string ToLower(std::string s)
        {
            std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        std::string BuildLabel(const std::string& editorID, const std::string& plugin, std::uint32_t localID)
        {
            const std::string name = editorID.empty() ? "(no EditorID)"s : editorID;
            return std::format("{}  [{} | {:06X}]", name, plugin, localID);
        }
    }

    bool Describe(const RE::TESWeather* weather, std::string& plugin, std::uint32_t& localID)
    {
        if (!weather) {
            return false;
        }
        const auto* file = weather->GetFile(0);
        plugin = file ? std::string(file->GetFilename()) : std::string{};
        localID = weather->GetLocalFormID();
        return file != nullptr;
    }

    std::string LabelOf(const RE::TESWeather* weather)
    {
        if (!weather) {
            return "(none)";
        }
        std::string plugin;
        std::uint32_t local = 0;
        Describe(weather, plugin, local);
        const char* edid = weather->GetFormEditorID();
        return BuildLabel(edid ? edid : "", plugin, local);
    }

    std::string ShortName(const RE::TESWeather* weather)
    {
        if (!weather) {
            return "(none)";
        }
        const char* edid = weather->GetFormEditorID();
        if (edid && *edid) {
            return edid;
        }
        std::string plugin;
        std::uint32_t local = 0;
        Describe(weather, plugin, local);
        return std::format("{}|{:06X}", plugin, local);
    }

    void Scan()
    {
        g_all.clear();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            logger::error("TESDataHandler not available, cannot scan weathers");
            return;
        }

        for (auto* weather : dataHandler->GetFormArray<RE::TESWeather>()) {
            if (!weather) {
                continue;
            }

            WeatherInfo info;
            if (!Describe(weather, info.plugin, info.localID)) {
                continue;
            }
            const char* edid = weather->GetFormEditorID();
            info.editorID = edid ? edid : "";
            info.label = BuildLabel(info.editorID, info.plugin, info.localID);
            info.labelLower = ToLower(info.label);
            g_all.push_back(std::move(info));
        }

        std::ranges::sort(g_all, [](const WeatherInfo& a, const WeatherInfo& b) { return a.labelLower < b.labelLower; });

        const auto withEditorID = std::ranges::count_if(g_all, [](const WeatherInfo& w) { return !w.editorID.empty(); });
        logger::info("Scanned {} weathers ({} with EditorID)", g_all.size(), withEditorID);
    }

    const std::vector<WeatherInfo>& All()
    {
        return g_all;
    }

    const WeatherInfo* Find(const std::string& plugin, std::uint32_t localID)
    {
        for (const auto& w : g_all) {
            if (w.localID == localID && w.plugin == plugin) {
                return &w;
            }
        }
        return nullptr;
    }
}
