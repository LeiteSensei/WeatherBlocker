#include "UI.h"

#include "Settings.h"
#include "WeatherDB.h"

#include "SKSEMenuFramework.h"

namespace ImGui = ImGuiMCP;

namespace WB::UI
{
    namespace
    {
        Config      s_edit;               // copia editavel (so a thread de render mexe aqui)
        bool        s_loaded = false;
        bool        s_dirty = false;
        char        s_filter[128]{};
        int         s_selected = -1;      // indice em WeatherDB::All()
        bool        s_addTimed = false;
        std::string s_status;

        std::string ToLower(std::string s)
        {
            std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        std::string TimeStr(int minutes)
        {
            minutes = std::clamp(minutes, 0, 1439);
            return std::format("{:02}:{:02}", minutes / 60, minutes % 60);
        }

        void AddOrUpdateRule(const WeatherInfo& w, bool timed)
        {
            for (auto& r : s_edit.rules) {
                if (r.localID == w.localID && r.plugin == w.plugin) {
                    r.enabled = true;
                    r.allDay = !timed;
                    if (timed && r.windows.empty()) {
                        r.windows.push_back({});
                    }
                    s_dirty = true;
                    return;
                }
            }

            Rule r;
            r.plugin = w.plugin;
            r.localID = w.localID;
            r.label = w.label;
            r.enabled = true;
            r.allDay = !timed;
            if (timed) {
                r.windows.push_back({});
            }
            s_edit.rules.push_back(std::move(r));
            s_dirty = true;
        }

        void DrawWindowEditor(Rule& r)
        {
            int removeWindow = -1;

            for (std::size_t j = 0; j < r.windows.size(); ++j) {
                auto& w = r.windows[j];
                ImGui::PushID(static_cast<int>(j));

                int sh = w.startMin / 60, sm = w.startMin % 60;
                int eh = w.endMin / 60, em = w.endMin % 60;
                bool changed = false;

                ImGui::Text("From");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                changed |= ImGui::SliderInt("##sh", &sh, 0, 23, "%02d h");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                changed |= ImGui::SliderInt("##sm", &sm, 0, 59, "%02d m");
                ImGui::SameLine();
                ImGui::Text("to");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                changed |= ImGui::SliderInt("##eh", &eh, 0, 23, "%02d h");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                changed |= ImGui::SliderInt("##em", &em, 0, 59, "%02d m");
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    removeWindow = static_cast<int>(j);
                }

                if (changed) {
                    w.startMin = sh * 60 + sm;
                    w.endMin = eh * 60 + em;
                    s_dirty = true;
                }

                ImGui::TextDisabled("    = %s -> %s%s", TimeStr(w.startMin).c_str(), TimeStr(w.endMin).c_str(),
                    w.startMin > w.endMin ? "  (crosses midnight)" : (w.startMin == w.endMin ? "  (whole day)" : ""));

                ImGui::PopID();
            }

            if (removeWindow >= 0) {
                r.windows.erase(r.windows.begin() + removeWindow);
                s_dirty = true;
            }

            if (r.windows.empty()) {
                ImGui::TextColored(ImGui::ImVec4{ 1.0f, 0.7f, 0.2f, 1.0f }, "%s", "No time window: this rule never blocks anything.");
            }

            if (ImGui::Button("+ Add time window")) {
                r.windows.push_back({});
                s_dirty = true;
            }
        }

        void DrawRules()
        {
            if (s_edit.rules.empty()) {
                ImGui::TextDisabled("%s", "No rules yet. Pick a weather above and add one.");
                return;
            }

            int removeRule = -1;

            for (std::size_t i = 0; i < s_edit.rules.size(); ++i) {
                auto& r = s_edit.rules[i];
                ImGui::PushID(static_cast<int>(i));

                const bool missing = WeatherDB::Find(r.plugin, r.localID) == nullptr;

                s_dirty |= ImGui::Checkbox("##enabled", &r.enabled);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", "Checked = this weather is blocked. Uncheck to allow it again (rule is kept).");
                }
                ImGui::SameLine();
                if (missing) {
                    ImGui::TextDisabled("%s  [not in current load order]", r.label.c_str());
                } else {
                    ImGui::Text("%s", r.label.c_str());
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    removeRule = static_cast<int>(i);
                }

                ImGui::Indent();
                s_dirty |= ImGui::Checkbox("Block all day", &r.allDay);
                if (!r.allDay) {
                    DrawWindowEditor(r);
                }
                ImGui::Unindent();

                ImGui::Separator();
                ImGui::PopID();
            }

            if (removeRule >= 0) {
                s_edit.rules.erase(s_edit.rules.begin() + removeRule);
                s_dirty = true;
            }
        }

        void __stdcall Render()
        {
            if (!s_loaded) {
                s_edit = Settings::GetActive();
                s_loaded = true;
            }

            const auto& all = WeatherDB::All();

            // ---------------------------------------------------------------- General
            ImGui::SeparatorText("General");
            s_dirty |= ImGui::Checkbox("Enable Weather Blocker", &s_edit.modEnabled);
            s_dirty |= ImGui::Checkbox("Respect weather forced by quests / scripts / console", &s_edit.respectOverrides);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", "When ticked, nothing is changed while a script or quest holds the weather.\nUntick it to test with console commands.");
            }
            ImGui::SetNextItemWidth(200.0f);
            s_dirty |= ImGui::SliderInt("Check interval (ms)", &s_edit.pollMs, 50, 1000);

            // ---------------------------------------------------------------- Add rule
            ImGui::SeparatorText("Add a weather rule");
            ImGui::Text("%d weathers found in the load order", static_cast<int>(all.size()));

            ImGui::SetNextItemWidth(300.0f);
            ImGui::InputText("Filter", s_filter, sizeof(s_filter));
            const std::string filter = ToLower(s_filter);

            if (ImGui::BeginListBox("##weathers", ImGui::ImVec2{ -FLT_MIN, 200.0f })) {
                int shown = 0;
                for (int i = 0; i < static_cast<int>(all.size()); ++i) {
                    if (!filter.empty() && all[i].labelLower.find(filter) == std::string::npos) {
                        continue;
                    }
                    if (shown++ >= 300) {
                        ImGui::TextDisabled("%s", "... type in the filter to narrow the list");
                        break;
                    }
                    ImGui::PushID(i);
                    if (ImGui::Selectable(all[i].label.c_str(), s_selected == i)) {
                        s_selected = i;
                    }
                    ImGui::PopID();
                }
                ImGui::EndListBox();
            }

            const bool hasSelection = s_selected >= 0 && s_selected < static_cast<int>(all.size());
            if (hasSelection) {
                ImGui::Text("Selected: %s", all[s_selected].label.c_str());
            } else {
                ImGui::TextDisabled("%s", "Selected: (none)");
            }

            ImGui::Checkbox("Only during a time window", &s_addTimed);

            ImGui::BeginDisabled(!hasSelection);
            if (ImGui::Button("Block selected weather")) {
                AddOrUpdateRule(all[s_selected], s_addTimed);
            }
            ImGui::SameLine();
            if (ImGui::Button("Use selected as fallback weather")) {
                s_edit.fallbackMode = 1;
                s_edit.fallbackPlugin = all[s_selected].plugin;
                s_edit.fallbackLocalID = all[s_selected].localID;
                s_edit.fallbackLabel = all[s_selected].label;
                s_dirty = true;
            }
            ImGui::EndDisabled();

            // ---------------------------------------------------------------- Rules
            ImGui::SeparatorText("Rules");
            DrawRules();

            // ---------------------------------------------------------------- Fallback
            ImGui::SeparatorText("Replacement weather");
            ImGui::TextWrapped("%s", "What to switch to when a blocked weather starts.");
            s_dirty |= ImGui::RadioButton("Automatic (previous weather, then default)", &s_edit.fallbackMode, 0);
            s_dirty |= ImGui::RadioButton("Always use this weather:", &s_edit.fallbackMode, 1);
            ImGui::SameLine();
            if (s_edit.fallbackLabel.empty()) {
                ImGui::TextDisabled("%s", "(none chosen)");
            } else {
                ImGui::Text("%s", s_edit.fallbackLabel.c_str());
            }

            // ---------------------------------------------------------------- Save
            ImGui::SeparatorText("Save");
            if (ImGui::Button("Save all")) {
                const bool ok = Settings::Apply(s_edit);
                s_status = ok ? "Saved and applied: " + Settings::Path().string()
                              : "Applied in memory, but writing the file FAILED (see WeatherBlocker.log)";
                s_dirty = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload from file")) {
                Settings::Init();
                s_edit = Settings::GetActive();
                s_status = "Reloaded from file";
                s_dirty = false;
            }
            if (s_dirty) {
                ImGui::SameLine();
                ImGui::TextColored(ImGui::ImVec4{ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", "Unsaved changes");
            }
            if (!s_status.empty()) {
                ImGui::TextWrapped("%s", s_status.c_str());
            }
        }
    }

    void Register()
    {
        if (!GetMenuFrameworkModule()) {
            logger::warn("SKSE Menu Framework not found - the in-game menu is disabled (rules still load from the JSON file)");
            return;
        }

        SKSEMenuFramework::SetSection("Weather Blocker");
        SKSEMenuFramework::AddSectionItem("Settings", Render);
        logger::info("Menu registered in SKSE Menu Framework");
    }
}
