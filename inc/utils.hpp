#pragma once

#include <Windows.h>
#include <algorithm>
#include <string>
#include <map>
#include <unordered_map>
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "dependencies/json.hpp"
#include "imgui_settings.h"

// Windows.h defines max/min as macros; kill them so std::max/std::min/std::tolower compile
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#ifdef tolower
#undef tolower
#endif

using json = nlohmann::json;

// ── Key name table (centrum style, indexed by VK code 0x00-0xA5) ─────────────
static const char* KeyNames[256] = {
    "Unknown","LBUTTON","RBUTTON","CANCEL","MBUTTON","XB1","XB2","Unknown",
    "BACK","TAB","Unknown","Unknown","CLEAR","RETURN","Unknown","Unknown",
    "SHIFT","CONTROL","MENU","PAUSE","CAPITAL","KANA","Unknown","JUNJA",
    "FINAL","KANJI","Unknown","ESCAPE","CONVERT","NONCONVERT","ACCEPT","MODECHANGE",
    "SPACE","PRIOR","NEXT","END","HOME","LEFT","UP","RIGHT",
    "DOWN","SELECT","PRINT","EXECUTE","SNAPSHOT","INSERT","DELETE","HELP",
    "0","1","2","3","4","5","6","7","8","9",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "A","B","C","D","E","F","G","H","I","J","K","L","M",
    "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    "LWIN","RWIN","APPS","Unknown","SLEEP",
    "NUMPAD0","NUMPAD1","NUMPAD2","NUMPAD3","NUMPAD4",
    "NUMPAD5","NUMPAD6","NUMPAD7","NUMPAD8","NUMPAD9",
    "MULTIPLY","ADD","SEPARATOR","SUBTRACT","DECIMAL","DIVIDE",
    "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
    "F13","F14","F15","F16","F17","F18","F19","F20","F21","F22","F23","F24",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "NUMLOCK","SCROLL","OEM_NEC_EQUAL","OEM_FJ_MASSHOU","OEM_FJ_TOUROKU",
    "OEM_FJ_LOYA","OEM_FJ_ROYA",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "LSHIFT","RSHIFT","LCONTROL","RCONTROL","LMENU","RMENU",
    // Pad remaining entries
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
    "Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
};

namespace ImGui
{
    // ── Hotkey widget (centrum ImAdd::KeyBind style, drop-in for int*) ──────────
    // Click to listen, any key/mouse binds it, ESC clears to 0 (None).
    inline void Hotkey(int* k, const ImVec2& size_arg = ImVec2(0, 0))
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiContext& g = *GImGui;
        ImGuiIO& io = g.IO;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(k);

        // Label: lowercase bracketed key name, e.g. "[lshift]"
        char buf[32] = "none";
        if (*k > 0 && *k < 256 && strcmp(KeyNames[*k], "Unknown") != 0)
        {
            std::string s = std::string("[") + KeyNames[*k] + "]";
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            strncpy_s(buf, s.c_str(), sizeof(buf) - 1);
        }
        else if (g.ActiveId == id)
        {
            strncpy_s(buf, "[...]", sizeof(buf) - 1);
        }

        ImVec2 pos = window->DC.CursorPos;
        float  w = (size_arg.x <= 0) ? ImGui::CalcItemSize(ImVec2(-0.1f, 0), 0, 0).x : size_arg.x;
        float  h = (size_arg.y <= 0) ? ImGui::GetFontSize() : size_arg.y;

        ImRect frame_bb(pos, pos + ImVec2(w, h));
        ImRect total_bb(pos, pos + ImVec2(w, h));

        ImGui::ItemSize(total_bb);
        if (!ImGui::ItemAdd(total_bb, id)) return;

        const bool hovered = ImGui::ItemHoverable(frame_bb, id, 0);

        if (hovered)
        {
            ImGui::SetHoveredID(id);
            g.MouseCursor = ImGuiMouseCursor_Hand;
        }

        // Click to activate
        if (hovered && io.MouseClicked[0])
        {
            if (g.ActiveId != id)
            {
                *k = 0;
            }
            ImGui::SetActiveID(id, window);
            ImGui::FocusWindow(window);
        }
        else if (io.MouseClicked[0])
        {
            if (g.ActiveId == id) ImGui::ClearActiveID();
        }

        // While active: scan for any key
        if (g.ActiveId == id)
        {
            // ESC = clear
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                *k = 0;
                ImGui::ClearActiveID();
            }
            else
            {
                // Mouse buttons 1-4 (skip LMB so we don't grab our own click)
                for (int i = 1; i < 5; i++)
                {
                    if (io.MouseDown[i])
                    {
                        switch (i)
                        {
                        case 1: *k = VK_RBUTTON;  break;
                        case 2: *k = VK_MBUTTON;  break;
                        case 3: *k = VK_XBUTTON1; break;
                        case 4: *k = VK_XBUTTON2; break;
                        }
                        ImGui::ClearActiveID();
                        goto done;
                    }
                }

                // Keyboard range VK_BACK (0x08) through VK_RMENU (0xA5)
                for (int i = 0x08; i <= 0xA5; i++)
                {
                    if (i == VK_LBUTTON) continue;
                    if (GetAsyncKeyState(i) & 0x8000)
                    {
                        *k = i;
                        ImGui::ClearActiveID();
                        goto done;
                    }
                }

                // Extra keys centrum also checks
                static const int extra[] = {
                    VK_LWIN,VK_RWIN,VK_APPS,VK_SLEEP,
                    VK_NUMPAD0,VK_NUMPAD1,VK_NUMPAD2,VK_NUMPAD3,VK_NUMPAD4,
                    VK_NUMPAD5,VK_NUMPAD6,VK_NUMPAD7,VK_NUMPAD8,VK_NUMPAD9,
                    VK_MULTIPLY,VK_ADD,VK_SEPARATOR,VK_SUBTRACT,VK_DECIMAL,VK_DIVIDE,
                    VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,
                    VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12,
                    VK_F13,VK_F14,VK_F15,VK_F16,VK_F17,VK_F18,
                    VK_F19,VK_F20,VK_F21,VK_F22,VK_F23,VK_F24,
                };
                for (int vk : extra)
                {
                    if (GetAsyncKeyState(vk) & 0x8000)
                    {
                        *k = vk;
                        ImGui::ClearActiveID();
                        goto done;
                    }
                }
            }
        }
    done:;

        // Draw frame (matches centrum's flat FrameBg style)
        const bool is_listening = (g.ActiveId == id);
        ImU32 frame_col = is_listening
            ? ImGui::GetColorU32(ImColorToImVec4(GetColorWithAlpha(main_color, 0.25f)))
            : ImGui::GetColorU32(ImGuiCol_FrameBg);

        window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, frame_col, style.FrameRounding);

        if (style.FrameBorderSize > 0.f)
        {
            ImU32 border_col = is_listening
                ? ImGui::GetColorU32(ImColorToImVec4(main_color))
                : ImGui::GetColorU32(ImGuiCol_Border);
            window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, border_col, style.FrameRounding, 0, style.FrameBorderSize);
        }

        // Text with shadow (centrum style)
        ImVec2 text_sz = ImGui::CalcTextSize(buf);
        ImVec2 text_pos = frame_bb.Min + (frame_bb.Max - frame_bb.Min) / 2 - text_sz / 2;

        window->DrawList->AddText(text_pos + ImVec2(1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), buf);
        window->DrawList->AddText(text_pos + ImVec2(-1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), buf);
        window->DrawList->AddText(text_pos + ImVec2(-1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), buf);
        window->DrawList->AddText(text_pos + ImVec2(1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), buf);
        window->DrawList->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), buf);
    }
}

// ── DrawGlow ──────────────────────────────────────────────────────────────────
void DrawGlow(ImDrawList* drawList, ImVec2 start, ImVec2 end,
    ImVec4 color, int layers, float fadingPerLayer, float rounding)
{
    ImVec4 cur = color;
    for (int i = 0; i < layers; i++)
    {
        drawList->AddRect(start, end, ImColor(cur), rounding);
        drawList->AddRect({ start.x - i, start.y - i }, { end.x + i, end.y + i }, ImColor(cur), rounding + i);
        cur.w = (std::max)(0.0f, cur.w - fadingPerLayer);
    }
}

// ── DrawButtonWithImage ───────────────────────────────────────────────────────
bool DrawButtonWithImage(ImTextureRef img, ImVec2 imgSize, ImVec2 bgSize, ImVec2 pos,
    const char* text, ImU32 bgColor, ImU32 hoveredColor,
    ImU32 heldColor, float rounding)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 pos2 = ImVec2(winPos.x + pos.x, winPos.y + pos.y);
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImRect bb{ pos2, { pos2.x + bgSize.x, pos2.y + bgSize.y } };

    bool hovering = ImGui::IsMouseHoveringRect(bb.Min, bb.Max);
    bool held = hovering && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool clicked = hovering && ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    drawList->AddRectFilled(bb.Min, { bb.Max.x + 5.0f, bb.Max.y }, bgColor, rounding);
    if (hovering || held)
        drawList->AddRectFilled(bb.Min, { bb.Max.x + 5.0f, bb.Max.y }, held ? heldColor : hoveredColor, rounding);

    float imgX = pos2.x + (bgSize.x - (imgSize.x + 5.0f + textSize.x)) * 0.5f;
    float imgY = pos2.y + (bgSize.y - (std::max)(imgSize.y, textSize.y)) * 0.5f;
    drawList->AddImage(img, { imgX, imgY }, { imgX + imgSize.x, imgY + imgSize.y });
    drawList->AddText({ imgX + imgSize.x + 5.0f, pos2.y + (bgSize.y - textSize.y) * 0.5f },
        ImGui::GetColorU32(ImGuiCol_Text), text);
    return clicked;
}

// ── MoveMouse ─────────────────────────────────────────────────────────────────
void MoveMouse(float x, float y)
{
    INPUT input{ 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = static_cast<LONG>(x);
    input.mi.dy = static_cast<LONG>(y);
    SendInput(1, &input, sizeof(input));
}


// ── Config helpers ────────────────────────────────────────────────────────────
bool loadTheme(ImColor& featureBGColor, ImColor& glowColor, const std::string& themeName)
{
    std::string path = "themes/" + themeName + ".json";

    std::ifstream iF(path);
    if (!iF.is_open()) return false;

    std::string contents((std::istreambuf_iterator<char>(iF)),
        std::istreambuf_iterator<char>());
    iF.close();

    json j = json::parse(contents, nullptr, false);
    if (j.is_discarded()) return false;

    featureBGColor = ImColor(
        j["featureBGColor"][0].get<int>(),
        j["featureBGColor"][1].get<int>(),
        j["featureBGColor"][2].get<int>(),
        j["featureBGColor"][3].get<int>()
    );
    glowColor = ImColor(
        j["glowColor"][0].get<float>(),
        j["glowColor"][1].get<float>(),
        j["glowColor"][2].get<float>(),
        j["glowColor"][3].get<float>()
    );
    return true;
}

int getRandomNumber(int min, int max)
{
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}