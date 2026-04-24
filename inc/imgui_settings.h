#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"

#include <string>
#include <map>

// ── Centrum color palette ─────────────────────────────────────────────────────
inline ImColor main_color(230, 134, 224, 255);          // accent (pink/purple)
inline ImColor winbg_color(15, 16, 18, 200);            // main window bg
inline ImColor child_color(19, 19, 19, 255);            // child panel bg
inline ImColor background_color(13, 14, 16, 200);       // sidebar bg
inline ImColor stroke_color(35, 35, 35, 255);           // borders
inline ImColor scroll_bg_col(24, 24, 24, 255);          // scrollbar bg
inline ImColor second_color(255, 255, 255, 20);         // subtle highlight

inline ImColor text_colors[3] = {
    ImColor(255, 255, 255, 255),
    ImColor(200, 200, 200, 255),
    ImColor(150, 150, 150, 255)
};

// ── Helpers ───────────────────────────────────────────────────────────────────
inline ImVec4 ImColorToImVec4(const ImColor& c)
{
    return ImVec4(c.Value.x, c.Value.y, c.Value.z, c.Value.w);
}

inline ImColor GetColorWithAlpha(ImColor color, float alpha)
{
    return ImColor(color.Value.x, color.Value.y, color.Value.z, alpha);
}

inline ImColor GetDarkColor(const ImColor& color)
{
    const float d = 0.4f;
    return ImColor(color.Value.x * d, color.Value.y * d, color.Value.z * d, 1.f);
}

inline ImVec2 center_text(ImVec2 min, ImVec2 max, const char* text)
{
    return min + (max - min) / 2 - ImGui::CalcTextSize(text) / 2;
}

// ── Sidebar Tab button ────────────────────────────────────────────────────────
// Usage: ImGui::Tab("Aimbot", &iTabs, 0);
namespace ImGui
{
    inline void Tab(const char* label, int* current, int index)
    {
        const ImGuiID id      = ImGui::GetID(label);
        const bool    active  = (*current == index);

        // Animated color lerp per tab
        struct TabAnim { ImVec4 bg; };
        static std::map<ImGuiID, TabAnim> s_anim;
        auto& anim = s_anim[id];

        ImVec4 target_bg = active
            ? ImColorToImVec4(GetColorWithAlpha(main_color, 0.18f))
            : ImVec4(0.f, 0.f, 0.f, 0.f);

        const float spd = ImGui::GetIO().DeltaTime * 12.f;
        anim.bg = ImLerp(anim.bg, target_bg, spd);

        ImGui::PushStyleColor(ImGuiCol_Button,        anim.bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColorToImVec4(GetColorWithAlpha(main_color, 0.12f)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImColorToImVec4(GetColorWithAlpha(main_color, 0.22f)));

        if (active)
            ImGui::PushStyleColor(ImGuiCol_Text, ImColorToImVec4(main_color));
        else
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 0.65f));

        if (ImGui::Button(label, ImVec2(167.f, 34.f)))
            *current = index;

        // Left accent bar when active
        if (active)
        {
            ImVec2 p = ImGui::GetItemRectMin();
            ImVec2 e = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, p.y + 4.f),
                ImVec2(p.x + 3.f, e.y - 4.f),
                ImGui::GetColorU32(ImColorToImVec4(main_color)),
                2.f
            );
        }

        ImGui::PopStyleColor(4);
    }
}

// ── Glow rect helper (kept for DrawGlow compatibility) ────────────────────────
inline void rect_glow(ImDrawList* draw, ImVec2 start, ImVec2 end,
                      ImColor col, float rounding, float intensity)
{
    while (col.Value.w > 0.0019f)
    {
        draw->AddRectFilled(start, end, (ImU32)col, rounding);
        col.Value.w -= col.Value.w / intensity;
        start = start - ImVec2(1, 1);
        end   = end   + ImVec2(1, 1);
    }
}
