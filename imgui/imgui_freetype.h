// imgui_freetype.h
// FreeType font builder for Dear ImGui
// Compatible with imgui 1.89+ (including 1.92)
#pragma once
#include "imgui.h"

namespace ImGuiFreeType
{
    // Hinting flags (subset used in practice)
    enum FreeTypeBuilderFlags
    {
        NoHinting     = 1 << 0,
        NoAutoHint    = 1 << 1,
        ForceAutoHint = 1 << 2,
        LightHinting  = 1 << 3,
        MonoHinting   = 1 << 4,
        Bold          = 1 << 5,
        Oblique       = 1 << 6,
        Monochrome    = 1 << 7,
        LoadColor     = 1 << 8,
        Bitmap        = 1 << 9,
    };

    IMGUI_API bool BuildFontAtlas(ImFontAtlas* atlas, unsigned int extra_flags = 0);

    // Install FreeType as the default font builder.
    // Call once before any font is built (before first ImGui::NewFrame or ImGui::GetIO().Fonts->Build())
    IMGUI_API void SetAllocatorFunctions(void* (*alloc_func)(size_t sz, void* user_data),
                                         void  (*free_func)(void* ptr, void* user_data),
                                         void*   user_data = nullptr);

    struct FreeTypeBuilderData; // opaque

    // Per-font extra data — stored in ImFontConfig::FontBuilderIO (advanced)
    struct ImFontBuilderIO : public ::ImFontBuilderIO
    {
        IMGUI_API ImFontBuilderIO();
    };

    // Simple helper: call this once to replace the stb_truetype builder globally
    IMGUI_API const ImFontBuilderIO* GetBuilderForFreeType();
}
