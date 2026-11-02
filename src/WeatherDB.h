#pragma once

namespace WB
{
    struct WeatherInfo
    {
        std::string   plugin;
        std::uint32_t localID = 0;
        std::string   editorID;
        std::string   label;       // texto mostrado no menu
        std::string   labelLower;  // para o filtro
    };

    namespace WeatherDB
    {
        // Deve ser chamado depois de kDataLoaded (thread principal)
        void Scan();

        const std::vector<WeatherInfo>& All();
        const WeatherInfo* Find(const std::string& plugin, std::uint32_t localID);

        // Plugin de origem + FormID local de um TESWeather
        bool Describe(const RE::TESWeather* weather, std::string& plugin, std::uint32_t& localID);
        std::string LabelOf(const RE::TESWeather* weather);
        std::string ShortName(const RE::TESWeather* weather);  // EditorID ou plugin|FormID
    }
}
