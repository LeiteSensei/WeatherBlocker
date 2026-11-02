#pragma once

namespace WB
{
    // Faixa de horario em minutos desde a meia-noite [0, 1439].
    // Se start > end a faixa atravessa a meia-noite (ex.: 18:00 -> 05:00).
    // Se start == end a faixa cobre o dia inteiro.
    struct TimeWindow
    {
        int startMin = 18 * 60;
        int endMin = 5 * 60;
    };

    struct Rule
    {
        std::string   plugin;            // plugin que define o clima originalmente, ex.: "Skyrim.esm"
        std::uint32_t localID = 0;       // FormID sem o prefixo do load order
        std::string   label;             // texto exibido no menu
        bool          enabled = true;    // false = clima liberado de novo (regra mantida na lista)
        bool          allDay = true;     // true = bloqueado o dia todo
        std::vector<TimeWindow> windows; // usado quando allDay == false
    };

    struct Config
    {
        bool modEnabled = true;
        bool respectOverrides = true;    // nao mexer em clima forcado por quest/script/console
        int  pollMs = 100;
        bool instantSwitch = true;       // true = troca imediata, false = transicao rapida do motor
        bool showNotification = true;    // aviso na tela quando um clima e bloqueado

        int           fallbackMode = 0;  // 0 = automatico, 1 = clima escolhido
        std::string   fallbackPlugin;
        std::uint32_t fallbackLocalID = 0;
        std::string   fallbackLabel;

        std::vector<Rule> rules;
    };

    namespace Settings
    {
        std::filesystem::path Path();

        Config ReadFromDisk();                   // le o JSON (config padrao se nao existir)
        void   Init();                           // le o JSON e torna a config ativa
        bool   Apply(const Config& cfg);         // torna ativa e grava o JSON
        Config GetActive();                      // copia da config ativa
        std::uint64_t Version();                 // muda a cada Init/Apply

        bool InWindow(int nowMin, const TimeWindow& w);
        bool RuleActiveAt(const Rule& r, int nowMin);
    }
}
