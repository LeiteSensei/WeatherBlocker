#include "Blocker.h"
#include "UI.h"
#include "WeatherDB.h"

namespace
{
    void SetupLog()
    {
        auto logsFolder = SKSE::log::log_directory();
        if (!logsFolder) {
            return;
        }

        auto logFilePath = *logsFolder / "WeatherBlocker.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));

        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    }

    void OnMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            WB::WeatherDB::Scan();
            WB::UI::Register();
            WB::Blocker::Start();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SetupLog();

    logger::info("Weather Blocker loading");
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}
