#include <Windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <string>
#include <random>
#include <filesystem>
#include <algorithm>
#include <array>
#include <atomic>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include <unordered_map>
#include <unordered_set>
#include <set>

#include "resource.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#include "inc/dependencies/json.hpp"
#include "inc/snowflake/Snowflake.hpp"
#include "inc/Renderer.hpp"
#include "inc/rbx.hpp"
#include "inc/utils.hpp"          // includes imgui_settings.h
#include "inc/offsets.hpp"

using json = nlohmann::json;

#define SNOW_LIMIT 200

std::vector<RBX::Instance> playersList;
std::vector<std::string> aimbotLockPartsR6{ "Head","Torso","Left Arm","Right Arm","Left Leg","Right Leg" };
std::vector<std::string> aimbotLockPartsR15{ "Head","UpperTorso","LeftUpperArm","RightUpperArm","LeftUpperLeg","RightUpperLeg" };
std::vector<std::string> tracerTypes{ "Mouse","Corner","Top","Bottom" };
std::vector<std::string> espTypes{ "Square","Skeleton","Corners" };
std::vector<Snowflake::Snowflake> snow;

// Cached player body parts (refreshed every 500 ms)
struct CachedPlayer
{
    RBX::Instance model{ nullptr };
    RBX::Instance playerNode{ nullptr };
    std::string   name;
    bool          isR6{ false };
    void* teamFolder{ nullptr }; // parent folder address (for CB team check)

    RBX::Instance head{ nullptr };
    RBX::Instance torso{ nullptr };
    RBX::Instance lowerTorso{ nullptr };
    RBX::Instance leftArm{ nullptr };
    RBX::Instance rightArm{ nullptr };
    RBX::Instance leftLeg{ nullptr };
    RBX::Instance rightLeg{ nullptr };
    RBX::Instance hrp{ nullptr };
    RBX::Instance humanoid{ nullptr };
};
std::vector<CachedPlayer> cachedPlayers;
std::mutex                g_cacheMutex;
std::vector<RBX::Instance> g_pendingPlayers;
std::vector<CachedPlayer>  g_pendingCache;

// Team check
inline bool isSameTeam(RBX::Instance localPlr, RBX::Instance otherPlr)
{
    if (!localPlr.address || !otherPlr.address) return false;

    // Method 1: Compare Team pointer (works on most games)
    void* localTeam = RBX::Memory::read<void*>((void*)((uintptr_t)localPlr.address + Offsets::Team));
    void* otherTeam = RBX::Memory::read<void*>((void*)((uintptr_t)otherPlr.address + Offsets::Team));
    if (localTeam && otherTeam) return localTeam == otherTeam;

    // Method 2: Compare TeamColor (works on CB/BloxStrike)
    // TeamColor is a BrickColor value — same team = same value
    int localColor = RBX::Memory::read<int>((void*)((uintptr_t)localPlr.address + Offsets::TeamColor));
    int otherColor = RBX::Memory::read<int>((void*)((uintptr_t)otherPlr.address + Offsets::TeamColor));
    if (localColor != 0 && otherColor != 0) return localColor == otherColor;

    // Method 3: Compare parent of character model (CB stores T/CT in separate folders)
    // localPlr/otherPlr here might be the character model — read its parent pointer
    void* localParent = RBX::Memory::read<void*>((void*)((uintptr_t)localPlr.address + Offsets::Parent));
    void* otherParent = RBX::Memory::read<void*>((void*)((uintptr_t)otherPlr.address + Offsets::Parent));
    if (localParent && otherParent && localParent == otherParent) return true;

    return false;
}

// Wall check
inline bool isPlayerVisible(const RBX::Vector2& screenPos, int monW, int monH, HDC screenDC)
{
    if (screenPos.x < -500.f || screenPos.y < -500.f) return false;
    if (screenPos.x < 0.f || screenPos.y < 0.f) return false;
    if (screenPos.x > static_cast<float>(monW) || screenPos.y > static_cast<float>(monH)) return false;
    if (!screenDC) return true;

    // Sample a 3x3 grid around the target point and check if it looks like
    // the player is occluded. We check for solid-color wall surfaces
    // (high color uniformity = likely behind a wall).
    int px = (int)screenPos.x;
    int py = (int)screenPos.y;

    int visCount = 0;
    static const int offsets[5][2] = { {0,0},{-3,-3},{3,-3},{-3,3},{3,3} };

    for (auto& off : offsets)
    {
        int sx = std::clamp(px + off[0], 0, monW - 1);
        int sy = std::clamp(py + off[1], 0, monH - 1);
        COLORREF col = GetPixel(screenDC, sx, sy);
        BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);

        // Not visible if pixel is pure black (void/unrendered) 
        bool isVoid = (r < 10 && g < 10 && b < 10);
        if (!isVoid) visCount++;
    }

    return visCount >= 3; // visible if most samples are non-void
}

namespace Settings
{
    bool imguiVisible{ true };
    bool mainMenuVisible{ true };
    bool explorerWinVisible{ false };
    bool keybindListVisible{ false };
    int  toggleGuiKey{ 45 };
    std::string currentTab{ "Aiming" };

    bool  aimbotEnabled{ false };
    bool  aimbotFOVEnabled{ false };
    bool  aimbotPredictionEnabled{ false };
    float aimbotFOVRadius{ 100.0f };
    float aimbotStrenght{ 0.2f };
    float aimbotSmoothness{ 5.0f };  // 1=instant, higher=smoother
    float aimbotPredictionX{ 10.0f };
    float aimbotPredictionY{ 10.0f };
    std::string aimbotLockPart{ "Torso" };
    int   aimbotKey{ 2 };
    bool  aimbotClosestPartEnabled{ true };
    bool  aimbotWallCheck{ false };
    bool  aimbotTeamCheck{ false };
    ImVec4 aimbotFovColor{ 1.0f, 0.0f, 0.0f, 1.0f };

    bool  silentAimEnabled{ false };
    float silentAimFOVRadius{ 500.0f };
    int   silentAimHitChance{ 100 };   // 1-100
    std::string silentAimLockPart{ "Head" };

    // Hitbox multiplier
    bool  hitboxEnabled{ false };
    float hitboxMultiplier{ 2.0f };

    bool  triggerbotEnabled{ false };
    bool  triggerbotIndicateClicking{ false };
    float triggerbotDetectionRadius{ 20.0f };
    std::string triggerbotTriggerPart{ "Torso" };
    int   triggerbotKey{ 0 };

    bool  espEnabled{ false };
    bool  espFilled{ false };
    bool  espShowDistance{ false };
    bool  espShowName{ false };
    bool  espShowHealth{ false };
    bool  espIgnoreDeadPlrs{ true };
    bool  espWallCheck{ false };
    bool  espTeamCheck{ false };
    bool  chamsEnabled{ false };
    ImVec4 chamsColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    float chamsPulseSpeed{ 2.0f };
    int   espDistance{ 0 };
    char  espTypeName[32]{ "Square" };
    std::string espType{ "Square" };
    ImVec4 espColor{ 0.902f, 0.525f, 0.878f, 1.0f }; // #E686E0 pink
    bool  tracersEnabled{ false };
    std::string tracerType{ "Mouse" };
    ImVec4 tracerColor{ 0.902f, 0.525f, 0.878f, 1.0f }; // pink

    char  configFileName[30]{};
    char  themeFileName[30]{};
    bool  streamproofEnabled{ false };
    bool  rbxWindowNeedsToBeSelected{ true };
    int   mainLoopDelay{ 1 };

    bool  noclipEnabled{ false };
    bool  flyEnabled{ false };
    int   flyKey{ 0 };
    bool  flyKeyToggled{ false };
    bool  orbitEnabled{ false };
    int   walkSpeedSet{};
    int   jumpPowerSet{};
    char  othersRobloxPlr[30]{};
    RBX::Vector3 othersTeleportPos{};

    bool espPreviewOpened{ false };
}

// Forward-declare for explorer
// ── Hitbox expander ─────────────────────────────────────────────────────────
// Stores original sizes by head address. All writes via WriteProcessMemory.
// Never crashes: validates every address before touching it.
static std::unordered_map<uintptr_t, RBX::Vector3> g_origSize;
static std::unordered_map<uintptr_t, float>         g_multApplied;

static bool hb_addrOk(void* addr)
{
    if (!addr) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQueryEx(RBX::Memory::handle, addr, &mbi, sizeof(mbi))) return false;
    return mbi.State == MEM_COMMIT
        && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
}

void doHitbox(float mult)
{
    // Snapshot players under lock
    std::vector<CachedPlayer> snap;
    { std::lock_guard<std::mutex> lk(g_cacheMutex); snap = cachedPlayers; }

    // Build set of active keys, prune stale
    std::unordered_set<uintptr_t> active;
    for (auto& cp : snap) if (cp.head.address) active.insert((uintptr_t)cp.head.address);
    for (auto it = g_origSize.begin(); it != g_origSize.end(); )
        it = active.count(it->first) ? ++it : g_origSize.erase(it);
    for (auto it = g_multApplied.begin(); it != g_multApplied.end(); )
        it = active.count(it->first) ? ++it : g_multApplied.erase(it);

    for (auto& cp : snap)
    {
        if (!cp.head.address) continue;
        void* headAddr = cp.head.address;
        if (!hb_addrOk(headAddr)) continue;

        // Re-read primitive fresh — never use cached getPrimitive()
        void* primAddr = nullptr;
        if (!ReadProcessMemory(RBX::Memory::handle,
            (void*)((uintptr_t)headAddr + Offsets::Primitive),
            &primAddr, sizeof(void*), nullptr)) continue;
        if (!primAddr || !hb_addrOk(primAddr)) continue;

        uintptr_t key = (uintptr_t)headAddr;

        // First time: read and store original size
        if (!g_origSize.count(key))
        {
            RBX::Vector3 orig{};
            if (!ReadProcessMemory(RBX::Memory::handle,
                (void*)((uintptr_t)primAddr + Offsets::PartSize),
                &orig, sizeof(orig), nullptr)) continue;
            if (orig.x < 0.1f || orig.x > 8.f ||
                orig.y < 0.1f || orig.y > 8.f ||
                orig.z < 0.1f || orig.z > 8.f) continue;
            g_origSize[key] = orig;
            g_multApplied[key] = 1.f;
        }

        if (g_multApplied[key] == mult) continue;

        RBX::Vector3 orig = g_origSize[key];
        RBX::Vector3 expanded{ orig.x * mult, orig.y * mult, orig.z * mult };

        // Write expanded to primitive (physics), original to instance (visual)
        WriteProcessMemory(RBX::Memory::handle,
            (void*)((uintptr_t)primAddr + Offsets::PartSize),
            &expanded, sizeof(expanded), nullptr);
        WriteProcessMemory(RBX::Memory::handle,
            (void*)((uintptr_t)headAddr + Offsets::PartSize),
            &orig, sizeof(orig), nullptr);

        g_multApplied[key] = mult;
    }
}

void undoHitbox()
{
    std::vector<CachedPlayer> snap;
    { std::lock_guard<std::mutex> lk(g_cacheMutex); snap = cachedPlayers; }

    for (auto& cp : snap)
    {
        if (!cp.head.address) continue;
        uintptr_t key = (uintptr_t)cp.head.address;
        auto it = g_origSize.find(key);
        if (it == g_origSize.end()) continue;
        if (!hb_addrOk(cp.head.address)) continue;

        void* primAddr = nullptr;
        if (!ReadProcessMemory(RBX::Memory::handle,
            (void*)((uintptr_t)cp.head.address + Offsets::Primitive),
            &primAddr, sizeof(void*), nullptr)) continue;
        if (!primAddr || !hb_addrOk(primAddr)) continue;

        WriteProcessMemory(RBX::Memory::handle,
            (void*)((uintptr_t)primAddr + Offsets::PartSize),
            &it->second, sizeof(it->second), nullptr);
        WriteProcessMemory(RBX::Memory::handle,
            (void*)((uintptr_t)cp.head.address + Offsets::PartSize),
            &it->second, sizeof(it->second), nullptr);
    }
    g_origSize.clear();
    g_multApplied.clear();
}

void showExplorerChildren(RBX::Instance chld)
{
    if (ImGui::TreeNode(chld.name().c_str()))
    {
        for (RBX::Instance child : chld.getChildren())
            showExplorerChildren(child);
        ImGui::TreePop();
    }
}

int main()
{
    SetConsoleTitleA("barbecue");
    std::cout << "fetching offsets...\n";

    if (!Offsets::fetchOffsets())
    {
        std::cout << "your pc too trash, failed to fetch offsets!\n";
        system("pause");
        return 1;
    }
    std::cout << "done fetching offsets\n";

    std::cout << "Press ENTER to attach.";
    std::cin.get();
    system("cls");

    if (RBX::Memory::attach())
    {
        std::cout << "attached, pls follow @bar_becuee on roblox\n\n";
        Sleep(1000);
        system("cls");
    }
    else
    {
        std::cout << "your fatass pc too trash, failed to attach!!\n";
        system("pause");
        return 1;
    }

    void* dataModelAddr = RBX::getDataModel();

    if (RBX::Memory::read<int>((void*)((uintptr_t)dataModelAddr + Offsets::GameId)) == 0)
    {
        std::cout << "You need to join a game.\n";
        while (RBX::Memory::read<int>((void*)((uintptr_t)dataModelAddr + Offsets::GameId)) == 0)
        {
            dataModelAddr = RBX::getDataModel();
            Sleep(1000);
        }
        system("cls");
    }

    FreeConsole();

    json settingsJ;

    if (!std::filesystem::exists("settings.json"))
    {
        std::ofstream oF("settings.json");
        json s; s["theme"] = "default";
        oF << s.dump(4); oF.close();
    }
    if (!std::filesystem::exists("themes/"))
        std::filesystem::create_directory("themes");

    std::ifstream settingsIF("settings.json");
    std::string settingsContents((std::istreambuf_iterator<char>(settingsIF)),
        std::istreambuf_iterator<char>());
    settingsJ = json::parse(settingsContents);

    int monitorWidth = GetSystemMetrics(SM_CXSCREEN);
    int monitorHeight = GetSystemMetrics(SM_CYSCREEN);

    // Orbit vars
    int    radius{ 8 };
    int    eclipse{ 1 };
    double rotspeed{ 3.14159265358979323846 * 2 / 1 };
    eclipse = eclipse * radius;
    double rot{ 0 };

    Renderer renderer;
    renderer.Init();

    Snowflake::CreateSnowFlakes(snow, SNOW_LIMIT, 5.0f, 15.0f,
        0, 0, monitorWidth, monitorHeight,
        Snowflake::vec3(0.0f, 0.005f), IM_COL32(255, 255, 255, 100));

    ID3D11ShaderResourceView* avatarImg{ nullptr }; int avatarImgW, avatarImgH;
    avatarImg = renderer.LoadTextureFromResource(IDR_ESP_PREVIEW_IMG, avatarImgW, avatarImgH);

    ID3D11ShaderResourceView* logoImg{ nullptr }; int logoImgW, logoImgH;
    logoImg = renderer.LoadTextureFromResource(IDR_barbecue_LOGO, logoImgW, logoImgH);

    ID3D11ShaderResourceView* menuIconImg{ nullptr }; int menuIconImgW, menuIconImgH;
    menuIconImg = renderer.LoadTextureFromResource(IDR_MENU_ICON, menuIconImgW, menuIconImgH);

    ID3D11ShaderResourceView* explorerIconImg{ nullptr }; int explorerIconImgW, explorerIconImgH;
    explorerIconImg = renderer.LoadTextureFromResource(IDR_EXPLORER_ICON, explorerIconImgW, explorerIconImgH);

    ID3D11ShaderResourceView* aimingIconImg{ nullptr }; int aimingIconImgW, aimingIconImgH;
    aimingIconImg = renderer.LoadTextureFromResource(IDR_BULLSEYE_ICON, aimingIconImgW, aimingIconImgH);

    ID3D11ShaderResourceView* visualsIconImg{ nullptr }; int visualsIconImgW, visualsIconImgH;
    visualsIconImg = renderer.LoadTextureFromResource(IDR_ESP_PERSON_ICON, visualsIconImgW, visualsIconImgH);

    ID3D11ShaderResourceView* settingsIconImg{ nullptr }; int settingsIconImgW, settingsIconImgH;
    settingsIconImg = renderer.LoadTextureFromResource(IDR_GEAR_ICON, settingsIconImgW, settingsIconImgH);

    ID3D11ShaderResourceView* miscIconImg{ nullptr }; int miscIconImgW, miscIconImgH;
    miscIconImg = renderer.LoadTextureFromResource(IDR_DICE_ICON, miscIconImgW, miscIconImgH);

    ID3D11ShaderResourceView* keybindListIconImg{ nullptr }; int keybindListIconImgW, keybindListIconImgH;
    keybindListIconImg = renderer.LoadTextureFromResource(IDR_PAINT_BOARD_ICON, keybindListIconImgW, keybindListIconImgH);

    ID3D11ShaderResourceView* gamblingIconImg{ nullptr }; int gamblingIconImgW, gamblingIconImgH;
    gamblingIconImg = renderer.LoadTextureFromResource(IDR_CARDS_ICON, gamblingIconImgW, gamblingIconImgH);

    RBX::VisualEngine visualEngine{ RBX::Memory::read<void*>((void*)((uintptr_t)RBX::Memory::getRobloxBaseAddr() + Offsets::VisualEnginePointer)) };

    RBX::Instance dataModel{ dataModelAddr };
    RBX::Instance workspace{ dataModel.findFirstChildOfClass("Workspace") };
    RBX::Instance players{ dataModel.findFirstChildOfClass("Players") };

    RBX::Instance localPlayer{ RBX::Memory::read<void*>((void*)((uintptr_t)players.address + Offsets::LocalPlayer)) };
    RBX::Instance localPlayerModelInstance{ localPlayer.getModelInstance() };
    RBX::Instance humanoid{ localPlayerModelInstance.findFirstChild("Humanoid") };
    RBX::Instance hrp{ localPlayerModelInstance.findFirstChild("HumanoidRootPart") };
    RBX::Instance camera{ RBX::Memory::read<void*>((void*)((uintptr_t)workspace.address + Offsets::Camera)) };

    RBX::Instance lockedPlr{ nullptr };
    bool locked{ false };
    bool keybindPrevDown{ false };

    // ── Apply centrum style ───────────────────────────────────────────────────
    {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();

        style.FramePadding = ImVec2(4, 4);
        style.ItemSpacing = ImVec2(8, 4);
        style.FrameRounding = 5.f;
        style.WindowRounding = 15.f;
        style.WindowBorderSize = 0.f;
        style.PopupBorderSize = 0.f;
        style.WindowPadding = ImVec2(0, 0);
        style.ChildBorderSize = 1.f;
        style.PopupRounding = 5.f;
        style.ScrollbarSize = 4.f;
        style.ScrollbarRounding = 10.f;
        style.GrabMinSize = 6.f;
        style.GrabRounding = 3.f;
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.ChildRounding = 6.f;

        // Load custom theme on top if set
        ImColor featureBGColor{ 17, 17, 17, 204 };
        ImColor glowColor{ 1.0f, 0.0f, 0.0f, 0.8f };
        if (settingsJ["theme"] != "default")
            loadTheme(featureBGColor, glowColor, settingsJ["theme"].get<std::string>());

        // Centrum color overrides
        style.Colors[ImGuiCol_WindowBg] = ImColorToImVec4(winbg_color);
        style.Colors[ImGuiCol_ChildBg] = ImColorToImVec4(child_color);
        style.Colors[ImGuiCol_PopupBg] = ImColorToImVec4(background_color);
        style.Colors[ImGuiCol_Border] = ImVec4(0.13f, 0.13f, 0.13f, 1.f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 1.f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.f);
        style.Colors[ImGuiCol_ButtonActive] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 1.f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.f);
        style.Colors[ImGuiCol_HeaderActive] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_SliderGrab] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_SliderGrabActive] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_CheckMark] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.08f, 1.f);
        style.Colors[ImGuiCol_TabHovered] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_TabActive] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.14f, 0.14f, 1.f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_SeparatorActive] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_ScrollbarBg] = ImColorToImVec4(scroll_bg_col);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColorToImVec4(GetColorWithAlpha(main_color, 0.8f));
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.f);
        style.Colors[ImGuiCol_Text] = ImVec4(1.f, 1.f, 1.f, 0.9f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(1.f, 1.f, 1.f, 0.3f);
        style.Colors[ImGuiCol_ResizeGrip] = ImColorToImVec4(GetColorWithAlpha(main_color, 0.20f));
        style.Colors[ImGuiCol_ResizeGripHovered] = ImColorToImVec4(GetColorWithAlpha(main_color, 0.67f));
        style.Colors[ImGuiCol_ResizeGripActive] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_NavHighlight] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_PlotHistogram] = ImColorToImVec4(main_color);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImColorToImVec4(GetColorWithAlpha(main_color, 0.8f));
    }

    // ── Hitbox expander thread ───────────────────────────────────────────────────
    // Runs on its own thread to avoid racing with Roblox physics engine
    std::thread([&]() {
        while (true) {
            __try {
                if (Settings::hitboxEnabled) {
                    doHitbox(Settings::hitboxMultiplier);
                }
                else if (!g_origSize.empty()) {
                    undoHitbox();
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                // Catch any access violation from race — just skip this frame
                g_origSize.clear();
                g_multApplied.clear();
            }
            Sleep(33); // ~30fps is plenty for hitbox, reduces race risk
        }
        }).detach();

    // ── Background player cache thread ──────────────────────────────────────────
    std::thread([&]() {
        while (true)
        {
            std::vector<RBX::Instance> newPlayers;
            std::vector<CachedPlayer>  newCache;

            auto tryAdd = [&](RBX::Instance mi)
                {
                    if (!mi.address) return;
                    RBX::Instance humCheck = mi.findFirstChildOfClass("Humanoid");
                    if (!humCheck.address) return;
                    RBX::Instance hrpCheck = mi.findFirstChild("HumanoidRootPart");
                    if (!hrpCheck.address) return;
                    if (hrpCheck.address == hrp.address) return;
                    for (const auto& e : newCache)
                        if (e.model.address == mi.address) return;

                    void* mi_addr = mi.address; // capture before any modification
                    CachedPlayer cp;
                    cp.model = mi; cp.playerNode = mi;
                    // Capture parent folder for CB team check (T/CT folder address)
                    cp.teamFolder = RBX::Memory::read<void*>((void*)((uintptr_t)mi.address + Offsets::Parent));
                    cp.name = mi.name();
                    cp.head = mi.findFirstChild("Head");
                    cp.humanoid = humCheck;
                    cp.hrp = hrpCheck;

                    // Standard death check
                    if (cp.humanoid.address) {
                        float h = RBX::Memory::read<float>((void*)((uintptr_t)cp.humanoid.address + Offsets::Health));
                        if (h <= 0.f) return;
                    }

                    // CB dead body check: verify a Player in Players service owns this character
                    // Dead bodies are orphaned — no Player's ModelInstance points to them
                    bool hasOwner = false;
                    for (RBX::Instance plr : players.getChildren())
                    {
                        void* miPtr = RBX::Memory::read<void*>((void*)((uintptr_t)plr.address + Offsets::ModelInstance));
                        if (miPtr == mi_addr) { hasOwner = true; break; }
                    }
                    if (!hasOwner) return; // dead body or unowned character

                    RBX::Instance t = mi.findFirstChild("Torso");
                    cp.isR6 = (t.address != nullptr);
                    if (cp.isR6) {
                        cp.torso = t;
                        cp.leftArm = mi.findFirstChild("Left Arm");
                        cp.rightArm = mi.findFirstChild("Right Arm");
                        cp.leftLeg = mi.findFirstChild("Left Leg");
                        cp.rightLeg = mi.findFirstChild("Right Leg");
                    }
                    else {
                        cp.torso = mi.findFirstChild("UpperTorso");
                        cp.lowerTorso = mi.findFirstChild("LowerTorso");
                        cp.leftArm = mi.findFirstChild("LeftUpperArm");
                        cp.rightArm = mi.findFirstChild("RightUpperArm");
                        cp.leftLeg = mi.findFirstChild("LeftUpperLeg");
                        cp.rightLeg = mi.findFirstChild("RightUpperLeg");
                    }

                    // Match Player node for team check
                    // Try by name first (works on normal games)
                    for (RBX::Instance plr : players.getChildren())
                    {
                        if (plr.name() == mi.name()) { cp.playerNode = plr; break; }
                        // Fallback: check ModelInstance offset points to this character
                        void* miAddr = RBX::Memory::read<void*>((void*)((uintptr_t)plr.address + Offsets::ModelInstance));
                        if (miAddr == mi.address) { cp.playerNode = plr; break; }
                    }

                    newPlayers.push_back(mi);
                    newCache.push_back(cp);
                };

            // 3-level workspace scan
            for (RBX::Instance c0 : workspace.getChildren()) {
                if (!c0.address) continue;
                tryAdd(c0);
                for (RBX::Instance c1 : c0.getChildren()) {
                    if (!c1.address) continue;
                    tryAdd(c1);
                    for (RBX::Instance c2 : c1.getChildren()) {
                        if (!c2.address) continue;
                        tryAdd(c2);
                    }
                }
            }

            if (!newCache.empty()) {
                std::lock_guard<std::mutex> lock(g_cacheMutex);
                g_pendingPlayers = std::move(newPlayers);
                g_pendingCache = std::move(newCache);
            }

            Sleep(200);
        }
        }).detach();

    // ── Main loop ─────────────────────────────────────────────────────────────
    ULONGLONG lastPlrRefresh{ GetTickCount64() };

    while (true)
    {
        if (RBX::Memory::read<int>((void*)((uintptr_t)dataModelAddr + Offsets::GameId)) == 0)
        {
            while (RBX::Memory::read<int>((void*)((uintptr_t)dataModelAddr + Offsets::GameId)) == 0)
            {
                dataModelAddr = RBX::getDataModel(); Sleep(1000);
            }
        }

        HWND robloxHWND = FindWindow(NULL, L"Roblox");
        bool robloxFocused = GetForegroundWindow() == robloxHWND;

        if (GetAsyncKeyState(Settings::toggleGuiKey) & 1)
            Settings::imguiVisible = !Settings::imguiVisible;

        dataModel = RBX::Instance(RBX::getDataModel());
        workspace = dataModel.findFirstChildOfClass("Workspace");
        players = dataModel.findFirstChildOfClass("Players");
        localPlayer = RBX::Instance(RBX::Memory::read<void*>((void*)((uintptr_t)players.address + Offsets::LocalPlayer)));
        localPlayerModelInstance = RBX::Instance(RBX::Memory::read<void*>((void*)((uintptr_t)localPlayer.address + Offsets::ModelInstance)));
        humanoid = localPlayerModelInstance.findFirstChild("Humanoid");
        hrp = localPlayerModelInstance.findFirstChild("HumanoidRootPart");
        camera = RBX::Memory::read<void*>((void*)((uintptr_t)workspace.address + Offsets::Camera));

        // Swap latest player cache from background thread
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            if (!g_pendingCache.empty())
            {
                playersList = std::move(g_pendingPlayers);
                cachedPlayers = std::move(g_pendingCache);
                g_pendingPlayers.clear();
                g_pendingCache.clear();
            }
        }

        // Hitbox expander runs on its own thread (see below)

        renderer.StartRender();

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        // ── GUI ───────────────────────────────────────────────────────────────
        if (Settings::imguiVisible)
        {
            SetWindowLong(renderer.hwnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);

            ImGuiIO& imguiIo = ImGui::GetIO();
            ImVec2    mousePos = imguiIo.MousePos;

            // Dim background
            drawList->AddRectFilled({ 0.f, 0.f },
                { static_cast<float>(monitorWidth), static_cast<float>(monitorHeight) },
                IM_COL32(0, 0, 0, 120));

            // Bottom toolbar
            {
                const ImU32 accent = (ImU32)main_color;
                const ImU32 dim = IM_COL32(45, 45, 45, 255);

                float tbW = 200.f, tbH = 48.f;
                float tbX = (monitorWidth / 2.f) - tbW * 0.5f;
                float tbY = (float)monitorHeight - 125.f;

                // Shadow
                drawList->AddRectFilled({ tbX - 6.f, tbY - 6.f }, { tbX + tbW + 6.f, tbY + tbH + 6.f }, IM_COL32(0, 0, 0, 90), 16.f);
                // Body
                drawList->AddRectFilled({ tbX, tbY }, { tbX + tbW, tbY + tbH }, IM_COL32(10, 11, 13, 245), 12.f);
                // Border
                drawList->AddRect({ tbX, tbY }, { tbX + tbW, tbY + tbH }, IM_COL32(38, 38, 38, 255), 12.f, 0, 1.f);
                // Top accent
                drawList->AddRectFilled({ tbX + 12.f, tbY + 1.f }, { tbX + tbW - 12.f, tbY + 3.f }, accent, 2.f);
                // Logo + separator
                drawList->AddImage((void*)logoImg, { tbX + tbW - 36.f, tbY + 8.f }, { tbX + tbW - 8.f, tbY + tbH - 8.f });
                drawList->AddRectFilled({ tbX + tbW - 42.f, tbY + 10.f }, { tbX + tbW - 41.f, tbY + tbH - 10.f }, IM_COL32(50, 50, 50, 255));

                float btnY = tbY + 7.f, btnH = tbH - 14.f, btnW = 34.f;
                auto drawBtn = [&](float bx, void* icon, const char* tip) -> bool
                    {
                        bool hov = ImRect({ bx,btnY }, { bx + btnW,btnY + btnH }).Contains(mousePos);
                        if (hov) {
                            drawList->AddRectFilled({ bx,btnY }, { bx + btnW,btnY + btnH }, IM_COL32(230, 134, 224, 35), 6.f);
                            drawList->AddRect({ bx,btnY }, { bx + btnW,btnY + btnH }, accent, 6.f, 0, 1.3f);
                            drawList->AddText({ mousePos.x + 14.f,mousePos.y + 14.f }, IM_COL32_WHITE, tip);
                        }
                        else
                            drawList->AddRect({ bx,btnY }, { bx + btnW,btnY + btnH }, dim, 6.f, 0, 1.f);
                        ImU32 tint = hov ? IM_COL32(255, 200, 255, 255) : IM_COL32(180, 180, 180, 255);
                        drawList->AddImage((void*)icon, { bx + 5.f,btnY + 5.f }, { bx + btnW - 5.f,btnY + btnH - 5.f }, ImVec2(0, 0), ImVec2(1, 1), tint);
                        return hov && imguiIo.MouseClicked[0];
                    };

                if (drawBtn(tbX + 6.f, menuIconImg, "Menu"))         Settings::mainMenuVisible = !Settings::mainMenuVisible;
                if (drawBtn(tbX + 45.f, explorerIconImg, "Explorer"))     Settings::explorerWinVisible = !Settings::explorerWinVisible;
                if (drawBtn(tbX + 84.f, keybindListIconImg, "Keybind List")) Settings::keybindListVisible = !Settings::keybindListVisible;
            }

            // ── Main menu (centrum layout) ────────────────────────────────────
            if (Settings::mainMenuVisible)
            {
                static int iTabs = 0;

                const float WIN_W_DEFAULT = 827.f, WIN_H_DEFAULT = 604.f;

                ImGui::SetNextWindowSize(ImVec2(WIN_W_DEFAULT, WIN_H_DEFAULT), ImGuiCond_Once);
                ImGui::SetNextWindowSizeConstraints(ImVec2(600.f, 420.f), ImVec2(FLT_MAX, FLT_MAX));
                ImGui::Begin("barbecue_main", nullptr,
                    ImGuiWindowFlags_NoTitleBar |   // no title bar (we draw our own)
                    ImGuiWindowFlags_NoScrollbar |   // no scrollbar
                    ImGuiWindowFlags_NoCollapse |   // no collapse
                    ImGuiWindowFlags_NoSavedSettings);   // resizable — no NoResize flag
                {
                    auto* draw = ImGui::GetWindowDrawList();
                    const ImVec2 p = ImGui::GetWindowPos();

                    // ── Dynamic layout — recalculate each frame from actual window size ──
                    const ImVec2 winSz = ImGui::GetWindowSize();
                    const float  WIN_W = winSz.x;
                    const float  WIN_H = winSz.y;
                    const float  SIDE_W = 187.f;
                    const float  TOP_H = 55.f;
                    const float  CONTENT_W = WIN_W - SIDE_W - 30.f;
                    const float  PANEL_W = floorf(CONTENT_W * 0.5f);
                    const float  PANEL_H = WIN_H - TOP_H - 20.f;
                    // Window chrome
                    draw->AddRectFilled(p + ImVec2(0, TOP_H), p + ImVec2(WIN_W, WIN_H),
                        (ImU32)winbg_color, ImGui::GetStyle().WindowRounding,
                        ImDrawFlags_RoundCornersBottom);

                    // Sidebar bg
                    draw->AddRectFilled(p + ImVec2(0, TOP_H), p + ImVec2(SIDE_W, WIN_H),
                        IM_COL32(13, 14, 16, 229), ImGui::GetStyle().WindowRounding,
                        ImDrawFlags_RoundCornersBottomLeft);

                    // Top bar
                    draw->AddRectFilled(p, p + ImVec2(WIN_W, TOP_H),
                        IM_COL32(5, 5, 5, 45), ImGui::GetStyle().WindowRounding,
                        ImDrawFlags_RoundCornersTop);

                    // Accent line under top bar
                    draw->AddRectFilled(p + ImVec2(0, TOP_H - 3.f), p + ImVec2(WIN_W, TOP_H),
                        ImGui::GetColorU32(ImColorToImVec4(main_color)));

                    // Title — scaled up using explicit font size
                    {
                        ImFont* f = ImGui::GetFont();
                        const float ts = 20.f;
                        ImVec2 tsz = f->CalcTextSizeA(ts, FLT_MAX, 0.f, "Barbecue");
                        ImVec2 tpos = p + ImVec2((WIN_W - tsz.x) * 0.5f, (TOP_H - tsz.y) * 0.5f);
                        draw->AddText(f, ts, tpos, IM_COL32_WHITE, "Barbecue");
                    }

                    // ── Sidebar tabs ──────────────────────────────────────────
                    ImGui::SetCursorPos(ImVec2(10.f, TOP_H + 15.f));
                    ImGui::BeginGroup();

                    ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.3f), "Combat");
                    ImGui::Tab("Aiming", &iTabs, 0);
                    ImGui::Tab("Visuals", &iTabs, 1);

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.3f), "Misc");
                    ImGui::Tab("Misc", &iTabs, 2);

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.3f), "Settings");
                    ImGui::Tab("Settings", &iTabs, 3);

                    ImGui::EndGroup();

                    // ── Content area ──────────────────────────────────────────
                    ImGui::SetCursorPos(ImVec2(SIDE_W + 10.f, TOP_H + 10.f));
                    ImGui::BeginGroup();
                    ImGui::PushItemWidth(-1.f);

                    // ── TAB 0: AIMING ─────────────────────────────────────────
                    if (iTabs == 0)
                    {
                        // Left: Aimbot
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##aim_left", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Aimbot");
                            ImGui::Separator();
                            ImGui::Checkbox("Enable Aimbot", &Settings::aimbotEnabled);
                            ImGui::Checkbox("FOV Circle", &Settings::aimbotFOVEnabled);
                            ImGui::Checkbox("Prediction", &Settings::aimbotPredictionEnabled);
                            ImGui::Checkbox("Closest Part to Mouse", &Settings::aimbotClosestPartEnabled);
                            ImGui::Checkbox("Wall Check", &Settings::aimbotWallCheck);
                            ImGui::Checkbox("Team Check", &Settings::aimbotTeamCheck);

                            ImGui::Spacing();
                            ImGui::Text("Lock Part");
                            if (ImGui::BeginCombo("##aimLockPart", Settings::aimbotLockPart.c_str()))
                            {
                                for (const auto& part : aimbotLockPartsR6)
                                {
                                    bool sel = Settings::aimbotLockPart == part;
                                    if (ImGui::Selectable(part.c_str(), sel)) Settings::aimbotLockPart = part;
                                    if (sel) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::Text("FOV Radius");
                            ImGui::SliderFloat("##fovR", &Settings::aimbotFOVRadius, 1.f, 400.f, "%.1f");
                            ImGui::Text("Strength");
                            ImGui::SliderFloat("##aimStr", &Settings::aimbotStrenght, 0.1f, 1.0f, "%.2f");
                            ImGui::Text("Smoothness (1=instant, 20=smooth)");
                            ImGui::SliderFloat("##aimSmooth", &Settings::aimbotSmoothness, 1.f, 20.f, "%.1f");
                            ImGui::Text("Prediction X");
                            ImGui::SliderFloat("##predX", &Settings::aimbotPredictionX, 1.f, 50.f, "%.0f");
                            ImGui::Text("Prediction Y");
                            ImGui::SliderFloat("##predY", &Settings::aimbotPredictionY, 1.f, 50.f, "%.0f");
                            ImGui::Text("FOV Color");
                            ImGui::ColorEdit4("##fovCol", (float*)&Settings::aimbotFovColor);
                            ImGui::Text("Keybind");
                            ImGui::Hotkey(&Settings::aimbotKey, ImVec2(-1.f, 22.f));
                        }
                        ImGui::EndChild();

                        ImGui::SameLine();

                        // Right: Triggerbot + Silent Aim
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##aim_right", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Triggerbot");
                            ImGui::Separator();
                            ImGui::Checkbox("Enable Triggerbot", &Settings::triggerbotEnabled);
                            ImGui::Checkbox("Indicate Clicking", &Settings::triggerbotIndicateClicking);
                            ImGui::Text("Trigger Part");
                            if (ImGui::BeginCombo("##tbPart", Settings::triggerbotTriggerPart.c_str()))
                            {
                                for (const auto& part : aimbotLockPartsR6)
                                {
                                    bool sel = Settings::triggerbotTriggerPart == part;
                                    if (ImGui::Selectable(part.c_str(), sel)) Settings::triggerbotTriggerPart = part;
                                    if (sel) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::Text("Detection Radius");
                            ImGui::SliderFloat("##tbRad", &Settings::triggerbotDetectionRadius, 1.f, 100.f, "%.1f");
                            ImGui::Text("Keybind");
                            ImGui::Hotkey(&Settings::triggerbotKey, ImVec2(-1.f, 22.f));

                            ImGui::Spacing(); ImGui::Spacing();
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Silent Aim");
                            ImGui::Separator();
                            ImGui::Checkbox("Enable Silent Aim", &Settings::silentAimEnabled);
                            ImGui::Text("Lock Part");
                            if (ImGui::BeginCombo("##saLockPart", Settings::silentAimLockPart.c_str()))
                            {
                                for (const auto& part : aimbotLockPartsR6)
                                {
                                    bool sel = Settings::silentAimLockPart == part;
                                    if (ImGui::Selectable(part.c_str(), sel)) Settings::silentAimLockPart = part;
                                    if (sel) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::Text("FOV Radius");
                            ImGui::SliderFloat("##saFov", &Settings::silentAimFOVRadius, 1.f, 1000.f, "%.0f");
                            ImGui::Text("Hit Chance %%");
                            ImGui::SliderInt("##saHit", &Settings::silentAimHitChance, 1, 100);
                        }
                        ImGui::EndChild();
                    }

                    // ── TAB 1: VISUALS ────────────────────────────────────────
                    if (iTabs == 1)
                    {
                        // Left: ESP
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##vis_left", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "ESP");
                            ImGui::Separator();
                            ImGui::Checkbox("Enable ESP", &Settings::espEnabled);
                            ImGui::Checkbox("Filled", &Settings::espFilled);
                            ImGui::Checkbox("Show Name", &Settings::espShowName);
                            ImGui::Checkbox("Show Health", &Settings::espShowHealth);
                            ImGui::Checkbox("Show Distance", &Settings::espShowDistance);
                            ImGui::Checkbox("Ignore Dead Players", &Settings::espIgnoreDeadPlrs);
                            ImGui::Checkbox("Wall Check##esp", &Settings::espWallCheck);

                            ImGui::Spacing();
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Hitbox");
                            ImGui::Separator();
                            ImGui::Checkbox("Expand Hitbox (Head)", &Settings::hitboxEnabled);
                            if (Settings::hitboxEnabled)
                                ImGui::SliderFloat("Multiplier##hb", &Settings::hitboxMultiplier, 1.f, 5.f, "%.1fx");
                            ImGui::Checkbox("Team Check##esp", &Settings::espTeamCheck);
                            ImGui::Text("ESP Type  (Square / Corners / Skeleton)");
                            ImGui::InputText("##espType", Settings::espTypeName, IM_ARRAYSIZE(Settings::espTypeName));
                            ImGui::Text("Max Distance  (0 = unlimited)");
                            ImGui::SliderInt("##espDist", &Settings::espDistance, 0, 500);
                            ImGui::Text("ESP Color");
                            ImGui::ColorEdit4("##espCol", (float*)&Settings::espColor);

                            ImGui::Spacing();
                            if (ImGui::Button("Preview ESP", ImVec2(-1.f, 26.f)))
                                Settings::espPreviewOpened = !Settings::espPreviewOpened;
                        }
                        ImGui::EndChild();

                        ImGui::SameLine();

                        // Right: Tracers + Chams
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##vis_right", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Tracers");
                            ImGui::Separator();
                            ImGui::Checkbox("Enable Tracers", &Settings::tracersEnabled);
                            ImGui::Text("Tracer Type");
                            if (ImGui::BeginCombo("##tracerType", Settings::tracerType.c_str()))
                            {
                                for (const auto& t : tracerTypes)
                                {
                                    bool sel = Settings::tracerType == t;
                                    if (ImGui::Selectable(t.c_str(), sel)) Settings::tracerType = t;
                                    if (sel) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::Text("Tracer Color");
                            ImGui::ColorEdit4("##tracerCol", (float*)&Settings::tracerColor);

                            ImGui::Spacing(); ImGui::Spacing();
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Chams");
                            ImGui::Separator();
                            ImGui::Checkbox("Enable Chams", &Settings::chamsEnabled);
                            ImGui::Text("Chams Color");
                            ImGui::ColorEdit4("##chamsCol", (float*)&Settings::chamsColor);
                            ImGui::Text("Pulse Speed");
                            ImGui::SliderFloat("##pulseSpd", &Settings::chamsPulseSpeed, 0.1f, 10.f, "%.1f");
                        }
                        ImGui::EndChild();
                    }

                    // ── TAB 2: MISC ───────────────────────────────────────────
                    if (iTabs == 2)
                    {
                        // Left: Movement + Teleport
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##misc_left", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Movement");
                            ImGui::Separator();
                            ImGui::Checkbox("Noclip", &Settings::noclipEnabled);
                            ImGui::Checkbox("Fly", &Settings::flyEnabled);
                            ImGui::Text("Fly Keybind");
                            ImGui::Hotkey(&Settings::flyKey, ImVec2(-1.f, 22.f));

                            ImGui::Text("WalkSpeed");
                            ImGui::SliderInt("##ws", &Settings::walkSpeedSet, 0, 1000);
                            if (ImGui::Button("Set WalkSpeed", ImVec2(-1.f, 26.f)))
                                RBX::setWalkSpeed(humanoid, Settings::walkSpeedSet);

                            ImGui::Text("JumpPower");
                            ImGui::SliderInt("##jp", &Settings::jumpPowerSet, 0, 1000);
                            if (ImGui::Button("Set JumpPower", ImVec2(-1.f, 26.f)))
                                RBX::setJumpPower(humanoid, Settings::jumpPowerSet);

                            ImGui::Spacing(); ImGui::Spacing();
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Teleport");
                            ImGui::Separator();
                            ImGui::InputFloat("X##tp", &Settings::othersTeleportPos.x, 0.f, 0.f, "%.0f");
                            ImGui::InputFloat("Y##tp", &Settings::othersTeleportPos.y, 0.f, 0.f, "%.0f");
                            ImGui::InputFloat("Z##tp", &Settings::othersTeleportPos.z, 0.f, 0.f, "%.0f");
                            if (ImGui::Button("Teleport to Coords", ImVec2(-1.f, 26.f)))
                                RBX::Memory::write<RBX::Vector3>((void*)((uintptr_t)hrp.getPrimitive() + Offsets::Position), Settings::othersTeleportPos);
                        }
                        ImGui::EndChild();

                        ImGui::SameLine();

                        // Right: Player utilities
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##misc_right", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Player Utilities");
                            ImGui::Separator();
                            ImGui::Text("Player Name");
                            ImGui::InputText("##plrName", Settings::othersRobloxPlr, sizeof(Settings::othersRobloxPlr));

                            ImGui::Spacing();
                            if (ImGui::Button("Spectate Player", ImVec2(-1.f, 26.f)))
                                RBX::Memory::write<void*>((void*)((uintptr_t)camera.address + Offsets::CameraSubject),
                                    workspace.findFirstChild(std::string(Settings::othersRobloxPlr)).address);

                            if (ImGui::Button("Stop Spectating", ImVec2(-1.f, 26.f)))
                                RBX::Memory::write<void*>((void*)((uintptr_t)camera.address + Offsets::CameraSubject), humanoid.address);

                            if (ImGui::Button("Teleport to Player", ImVec2(-1.f, 26.f)))
                            {
                                RBX::Instance plr = players.findFirstChild(Settings::othersRobloxPlr);
                                RBX::Instance plrMi = plr.getModelInstance();
                                RBX::Instance plrHrp = plrMi.findFirstChild("HumanoidRootPart");
                                RBX::Memory::write<RBX::Vector3>((void*)((uintptr_t)hrp.getPrimitive() + Offsets::Position), plrHrp.getPosition());
                            }

                            if (ImGui::Button("Orbit Player", ImVec2(-1.f, 26.f)))
                                Settings::orbitEnabled = true;

                            if (ImGui::Button("Stop Orbiting", ImVec2(-1.f, 26.f)))
                                Settings::orbitEnabled = false;
                        }
                        ImGui::EndChild();
                    }

                    // ── TAB 3: SETTINGS ───────────────────────────────────────
                    if (iTabs == 3)
                    {
                        // Left: Config
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##set_left", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Config");
                            ImGui::Separator();

                            ImGui::Text("Config file name");
                            ImGui::InputText("##cfgName", Settings::configFileName, sizeof(Settings::configFileName));

                            if (ImGui::Button("Load Config", ImVec2(-1.f, 26.f)))
                            {
                                wchar_t fPath[MAX_PATH]{};
                                OPENFILENAME ofn; ZeroMemory(&ofn, sizeof(ofn));
                                ofn.lStructSize = sizeof(ofn);
                                ofn.hwndOwner = renderer.hwnd;
                                ofn.lpstrFile = fPath;
                                ofn.nMaxFile = MAX_PATH;
                                ofn.lpstrFilter = L"All Files\0";
                                ofn.nFilterIndex = 1;
                                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                                if (GetOpenFileNameW(&ofn))
                                {
                                    std::ifstream iF(ofn.lpstrFile);
                                    if (iF.is_open())
                                    {
                                        std::string contents((std::istreambuf_iterator<char>(iF)),
                                            std::istreambuf_iterator<char>());
                                        json config = json::parse(contents);

                                        Settings::silentAimEnabled = config["silentaim"]["enabled"].get<bool>();
                                        Settings::silentAimLockPart = config["silentaim"]["lockPart"].get<std::string>();
                                        Settings::silentAimFOVRadius = config["silentaim"]["FOVradius"].get<float>();
                                        Settings::aimbotEnabled = config["aimbot"]["enabled"].get<bool>();
                                        Settings::aimbotFOVEnabled = config["aimbot"]["FOVenabled"].get<bool>();
                                        Settings::aimbotFOVRadius = config["aimbot"]["FOVradius"].get<float>();
                                        Settings::aimbotStrenght = config["aimbot"]["strenght"].get<float>();
                                        Settings::aimbotLockPart = config["aimbot"]["lockPart"].get<std::string>();
                                        Settings::aimbotKey = config["aimbot"]["key"].get<int>();
                                        Settings::aimbotFovColor.x = config["aimbot"]["FOVcolor"][0].get<float>();
                                        Settings::aimbotFovColor.y = config["aimbot"]["FOVcolor"][1].get<float>();
                                        Settings::aimbotFovColor.z = config["aimbot"]["FOVcolor"][2].get<float>();
                                        Settings::aimbotFovColor.w = config["aimbot"]["FOVcolor"][3].get<float>();
                                        Settings::triggerbotEnabled = config["triggerbot"]["enabled"].get<bool>();
                                        Settings::triggerbotIndicateClicking = config["triggerbot"]["indicateClicking"].get<bool>();
                                        Settings::triggerbotDetectionRadius = config["triggerbot"]["detectionRadius"].get<float>();
                                        Settings::triggerbotTriggerPart = config["triggerbot"]["triggerPart"].get<std::string>();
                                        Settings::triggerbotKey = config["triggerbot"]["key"].get<int>();
                                        Settings::espEnabled = config["esp"]["enabled"].get<bool>();
                                        Settings::espFilled = config["esp"]["filled"].get<bool>();
                                        Settings::espShowDistance = config["esp"]["showDistance"].get<bool>();
                                        Settings::espShowName = config["esp"]["showName"].get<bool>();
                                        Settings::espShowHealth = config["esp"]["showHealth"].get<bool>();
                                        Settings::espIgnoreDeadPlrs = config["esp"]["ignoreDeadPlayers"].get<bool>();
                                        Settings::espWallCheck = config["esp"]["wallCheck"].get<bool>();
                                        Settings::espTeamCheck = config["esp"]["teamCheck"].get<bool>();
                                        Settings::chamsEnabled = config["chams"]["enabled"].get<bool>();
                                        Settings::chamsPulseSpeed = config["chams"]["pulseSpeed"].get<float>();
                                        Settings::chamsColor.x = config["chams"]["color"][0].get<float>();
                                        Settings::chamsColor.y = config["chams"]["color"][1].get<float>();
                                        Settings::chamsColor.z = config["chams"]["color"][2].get<float>();
                                        Settings::chamsColor.w = config["chams"]["color"][3].get<float>();
                                        Settings::aimbotWallCheck = config["aimbot"]["wallCheck"].get<bool>();
                                        Settings::aimbotTeamCheck = config["aimbot"]["teamCheck"].get<bool>();
                                        Settings::espDistance = config["esp"]["distance"].get<int>();
                                        Settings::espType = config["esp"]["type"].get<std::string>();
                                        strncpy_s(Settings::espTypeName, Settings::espType.c_str(), sizeof(Settings::espTypeName) - 1);
                                        Settings::espColor.x = config["esp"]["color"][0].get<float>();
                                        Settings::espColor.y = config["esp"]["color"][1].get<float>();
                                        Settings::espColor.z = config["esp"]["color"][2].get<float>();
                                        Settings::espColor.w = config["esp"]["color"][3].get<float>();
                                        Settings::tracersEnabled = config["tracers"]["enabled"].get<bool>();
                                        Settings::tracerType = config["tracers"]["type"].get<std::string>();
                                        Settings::tracerColor.x = config["tracers"]["color"][0].get<float>();
                                        Settings::tracerColor.y = config["tracers"]["color"][1].get<float>();
                                        Settings::tracerColor.z = config["tracers"]["color"][2].get<float>();
                                        Settings::tracerColor.w = config["tracers"]["color"][3].get<float>();
                                        Settings::rbxWindowNeedsToBeSelected = config["settings"]["rbxWindowNeedsToBeSelected"].get<bool>();
                                        Settings::mainLoopDelay = config["settings"]["mainLoopDelay"].get<int>();
                                        Settings::noclipEnabled = config["misc"]["noclipEnabled"].get<bool>();
                                        Settings::flyEnabled = config["misc"]["flyEnabled"].get<bool>();
                                        Settings::flyKey = config["misc"]["flyKey"].get<int>();
                                        iF.close();
                                    }
                                    else MessageBoxA(renderer.hwnd, "Failed to open config.", "Error", MB_ICONERROR | MB_OK);
                                }
                            }

                            if (ImGui::Button("Save Config", ImVec2(-1.f, 26.f)))
                            {
                                json config;
                                config["silentaim"]["enabled"] = Settings::silentAimEnabled;
                                config["silentaim"]["lockPart"] = Settings::silentAimLockPart;
                                config["silentaim"]["FOVradius"] = Settings::silentAimFOVRadius;
                                config["aimbot"]["enabled"] = Settings::aimbotEnabled;
                                config["aimbot"]["FOVenabled"] = Settings::aimbotFOVEnabled;
                                config["aimbot"]["FOVradius"] = Settings::aimbotFOVRadius;
                                config["aimbot"]["strenght"] = Settings::aimbotStrenght;
                                config["aimbot"]["lockPart"] = Settings::aimbotLockPart;
                                config["aimbot"]["key"] = Settings::aimbotKey;
                                config["aimbot"]["FOVcolor"] = { Settings::aimbotFovColor.x, Settings::aimbotFovColor.y, Settings::aimbotFovColor.z, Settings::aimbotFovColor.w };
                                config["aimbot"]["closestPart"] = Settings::aimbotClosestPartEnabled;
                                config["aimbot"]["wallCheck"] = Settings::aimbotWallCheck;
                                config["aimbot"]["teamCheck"] = Settings::aimbotTeamCheck;
                                config["triggerbot"]["enabled"] = Settings::triggerbotEnabled;
                                config["triggerbot"]["indicateClicking"] = Settings::triggerbotIndicateClicking;
                                config["triggerbot"]["detectionRadius"] = Settings::triggerbotDetectionRadius;
                                config["triggerbot"]["triggerPart"] = Settings::triggerbotTriggerPart;
                                config["triggerbot"]["key"] = Settings::triggerbotKey;
                                config["esp"]["enabled"] = Settings::espEnabled;
                                config["esp"]["filled"] = Settings::espFilled;
                                config["esp"]["showDistance"] = Settings::espShowDistance;
                                config["esp"]["showName"] = Settings::espShowName;
                                config["esp"]["showHealth"] = Settings::espShowHealth;
                                config["esp"]["ignoreDeadPlayers"] = Settings::espIgnoreDeadPlrs;
                                config["esp"]["wallCheck"] = Settings::espWallCheck;
                                config["esp"]["teamCheck"] = Settings::espTeamCheck;
                                config["esp"]["distance"] = Settings::espDistance;
                                config["esp"]["type"] = Settings::espType;
                                config["esp"]["color"] = { Settings::espColor.x, Settings::espColor.y, Settings::espColor.z, Settings::espColor.w };
                                config["chams"]["enabled"] = Settings::chamsEnabled;
                                config["chams"]["pulseSpeed"] = Settings::chamsPulseSpeed;
                                config["chams"]["color"] = { Settings::chamsColor.x, Settings::chamsColor.y, Settings::chamsColor.z, Settings::chamsColor.w };
                                config["tracers"]["enabled"] = Settings::tracersEnabled;
                                config["tracers"]["type"] = Settings::tracerType;
                                config["tracers"]["color"] = { Settings::tracerColor.x, Settings::tracerColor.y, Settings::tracerColor.z, Settings::tracerColor.w };
                                config["settings"]["rbxWindowNeedsToBeSelected"] = Settings::rbxWindowNeedsToBeSelected;
                                config["settings"]["mainLoopDelay"] = Settings::mainLoopDelay;
                                config["misc"]["noclipEnabled"] = Settings::noclipEnabled;
                                config["misc"]["flyEnabled"] = Settings::flyEnabled;
                                config["misc"]["flyKey"] = Settings::flyKey;

                                std::ofstream oF(Settings::configFileName);
                                if (oF.is_open()) { oF << config.dump(4); oF.close(); }
                                else MessageBoxA(renderer.hwnd, "Failed to write config.", "Error", MB_ICONERROR | MB_OK);
                            }
                        }
                        ImGui::EndChild();

                        ImGui::SameLine();

                        // Right: App settings + Theme
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
                        ImGui::BeginChild("##set_right", ImVec2(PANEL_W, PANEL_H), ImGuiChildFlags_Borders);
                        ImGui::PopStyleVar();
                        {
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Application");
                            ImGui::Separator();

                            if (ImGui::Checkbox("Streamproof", &Settings::streamproofEnabled))
                                SetWindowDisplayAffinity(renderer.hwnd, Settings::streamproofEnabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);

                            ImGui::Checkbox("Roblox window must be focused", &Settings::rbxWindowNeedsToBeSelected);
                            ImGui::Text("Main Loop Delay (ms)");
                            ImGui::InputInt("##loopDelay", &Settings::mainLoopDelay);
                            ImGui::Text("Toggle GUI Key");
                            ImGui::Hotkey(&Settings::toggleGuiKey, ImVec2(-1.f, 22.f));

                            ImGui::Spacing(); ImGui::Spacing();
                            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.45f), "Theme");
                            ImGui::Separator();
                            ImGui::Text("Theme Name");
                            ImGui::InputText("##themeName", Settings::themeFileName, sizeof(Settings::themeFileName));

                            if (ImGui::Button("Set Theme", ImVec2(-1.f, 26.f)))
                            {
                                ImColor dummy1, dummy2;
                                if (loadTheme(dummy1, dummy2, std::string(Settings::themeFileName)))
                                {
                                    std::ofstream oF("settings.json");
                                    if (oF.is_open())
                                    {
                                        json sj = settingsJ;
                                        sj["theme"] = Settings::themeFileName;
                                        oF << sj.dump(4); oF.close();
                                    }
                                }
                            }

                            if (ImGui::Button("Reset Theme", ImVec2(-1.f, 26.f)))
                            {
                                std::ofstream oF("settings.json");
                                if (oF.is_open())
                                {
                                    json sj = settingsJ;
                                    sj["theme"] = "default";
                                    oF << sj.dump(4); oF.close();
                                }
                            }

                            ImGui::Spacing(); ImGui::Spacing();
                            ImGui::Text("Accent Color");
                            ImGui::ColorEdit4("##accentCol", (float*)&main_color.Value);

                            ImGui::Spacing();
                            if (ImGui::Button("Exit", ImVec2(-1.f, 26.f)))
                            {
                                RBX::Memory::detach();
                                renderer.Shutdown();
                                return 0;
                            }
                        }
                        ImGui::EndChild();
                    }

                    ImGui::PopItemWidth();
                    ImGui::EndGroup();
                }
                ImGui::End(); // barbecue_main
            }

            // ── ESP Preview window ────────────────────────────────────────────
            if (Settings::espPreviewOpened)
            {
                ImGui::SetNextWindowSize({ 290, 380 });
                ImGui::Begin("barbecue - ESP Preview", nullptr,
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
                {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    auto* dl = ImGui::GetWindowDrawList();

                    if (avatarImg) ImGui::Image((ImTextureID)(uintptr_t)avatarImg, { (float)avatarImgW, (float)avatarImgH });

                    if (Settings::espEnabled)
                    {
                        Settings::espType = std::string(Settings::espTypeName);
                        ImVec2 pos1{ p.x + 50.f, p.y + 70.f };
                        ImVec2 pos2{ p.x + 220.f, p.y + 260.f };
                        ImColor espCol{ Settings::espColor };

                        if (Settings::espType == "Square")
                        {
                            if (!Settings::espFilled) dl->AddRect(pos1, pos2, espCol);
                            else                      dl->AddRectFilled(pos1, pos2, espCol);
                        }
                        else if (Settings::espType == "Skeleton")
                        {
                            dl->AddLine({ p.x + 132.f, p.y + 70.f }, { p.x + 132.f, p.y + 150.f }, espCol);
                            dl->AddLine({ p.x + 50.f,  p.y + 150.f }, { p.x + 132.f, p.y + 150.f }, espCol);
                            dl->AddLine({ p.x + 220.f, p.y + 150.f }, { p.x + 132.f, p.y + 150.f }, espCol);
                            dl->AddLine({ p.x + 105.f, p.y + 270.f }, { p.x + 132.f, p.y + 150.f }, espCol);
                            dl->AddLine({ p.x + 157.f, p.y + 270.f }, { p.x + 132.f, p.y + 150.f }, espCol);
                        }
                        else if (Settings::espType == "Corners")
                        {
                            float l = pos1.x, r = pos2.x, t = pos1.y, b = pos2.y;
                            dl->AddLine({ l, t }, { l + 10.f, t }, espCol); dl->AddLine({ l, t }, { l, t + 10.f }, espCol);
                            dl->AddLine({ r, t }, { r - 10.f, t }, espCol); dl->AddLine({ r, t }, { r, t + 10.f }, espCol);
                            dl->AddLine({ l, b }, { l + 10.f, b }, espCol); dl->AddLine({ l, b }, { l, b - 10.f }, espCol);
                            dl->AddLine({ r, b }, { r - 10.f, b }, espCol); dl->AddLine({ r, b }, { r, b - 10.f }, espCol);
                        }

                        if (Settings::espShowName)
                        {
                            ImVec2 ts = ImGui::CalcTextSize("abc123");
                            dl->AddText({ pos1.x + (pos2.x - pos1.x) * 0.5f - ts.x * 0.5f, pos1.y - ts.y - 4.f }, IM_COL32_WHITE, "abc123");
                        }
                        if (Settings::espShowHealth)
                            dl->AddRectFilled({ pos1.x - 6.f, pos1.y }, { pos1.x - 2.f, pos2.y }, IM_COL32(0, 255, 0, 255));
                        if (Settings::espShowDistance)
                            dl->AddText({ pos2.x - 20.f, pos2.y + 4.f }, IM_COL32_WHITE, "10");
                    }
                }
                ImGui::End();
            }

            // ── Explorer window ───────────────────────────────────────────────
            if (Settings::explorerWinVisible)
            {
                ImGui::SetNextWindowSize({ 500, 600 });
                ImGui::Begin("barbecue - Explorer", nullptr,
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
                for (RBX::Instance chld : dataModel.getChildren())
                    showExplorerChildren(chld);
                ImGui::End();
            }
        }
        else
        {
            SetWindowLong(renderer.hwnd, GWL_EXSTYLE,
                WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
        }

        // ── Keybind list ──────────────────────────────────────────────────────
        if (Settings::keybindListVisible)
        {
            ImGui::SetNextWindowSize({ 290, 320 });
            ImGui::Begin("barbecue - Keybind List", nullptr,
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);

            if (Settings::aimbotEnabled && (GetAsyncKeyState(Settings::aimbotKey) & 0x8000))
                ImGui::Text("[HOLD] Aimbot - %s", KeyNames[Settings::aimbotKey]);
            if (Settings::triggerbotEnabled && (GetAsyncKeyState(Settings::triggerbotKey) & 0x8000))
                ImGui::Text("[HOLD] Triggerbot - %s", KeyNames[Settings::triggerbotKey]);
            if (Settings::flyEnabled && Settings::flyKeyToggled)
                ImGui::Text("[TOGGLE] Fly - %s", KeyNames[Settings::flyKey]);

            ImGui::End();
        }

        // ── Aimbot ────────────────────────────────────────────────────────────
        if (Settings::aimbotEnabled && (!Settings::rbxWindowNeedsToBeSelected || robloxFocused))
        {
            static std::string lockedPartName{};
            bool keybindDown = (Settings::aimbotKey != 0 && (GetAsyncKeyState(Settings::aimbotKey) & 0x8000));

            auto isValidScreen = [&](const RBX::Vector2& p) { return p.x > -500.f && p.y > -500.f && p.x < monitorWidth + 500 && p.y < monitorHeight + 500; };

            if (keybindDown)
            {
                if (!locked)
                {
                    float closestDistance{ Settings::aimbotFOVRadius };
                    RBX::Instance closestPlr{ nullptr };
                    std::string chosenPart{};
                    RBX::Vector2 chosenScreen{};

                    POINT mousePos; GetCursorPos(&mousePos);
                    HDC aimDC = Settings::aimbotWallCheck ? GetDC(nullptr) : nullptr;

                    for (const CachedPlayer& cp : cachedPlayers)
                    {
                        if (cp.hrp.address == hrp.address) continue;

                        // Skip dead players
                        if (cp.humanoid.address) {
                            float h = RBX::Memory::read<float>((void*)((uintptr_t)cp.humanoid.address + Offsets::Health));
                            if (h <= 0.f) continue;
                        }

                        // 3D distance cull — skip far players before worldToScreen
                        if (cp.hrp.address && hrp.getDistance(cp.hrp.getPosition()) > 2000.f) continue;

                        // Team check
                        if (Settings::aimbotTeamCheck)
                        {
                            bool sameTeam = false;
                            if (cp.playerNode.address && cp.playerNode.address != cp.model.address)
                                sameTeam = isSameTeam(localPlayer, cp.playerNode);
                            void* localFolder = RBX::Memory::read<void*>((void*)((uintptr_t)localPlayerModelInstance.address + Offsets::Parent));
                            if (!sameTeam && localFolder && cp.teamFolder && localFolder == cp.teamFolder)
                                sameTeam = true;
                            if (sameTeam) continue;
                        }

                        const RBX::Instance* parts[6] = {
                            &cp.head,&cp.torso,&cp.leftArm,
                            &cp.rightArm,&cp.leftLeg,&cp.rightLeg
                        };
                        static const char* pnames[6] = {
                            "Head","Torso","Left Arm","Right Arm","Left Leg","Right Leg"
                        };

                        if (!Settings::aimbotClosestPartEnabled)
                        {
                            const RBX::Instance* lp = &cp.torso;
                            if (Settings::aimbotLockPart == "Head")           lp = &cp.head;
                            else if (Settings::aimbotLockPart == "Left Arm" ||
                                Settings::aimbotLockPart == "LeftUpperArm")   lp = &cp.leftArm;
                            else if (Settings::aimbotLockPart == "Right Arm" ||
                                Settings::aimbotLockPart == "RightUpperArm")  lp = &cp.rightArm;
                            else if (Settings::aimbotLockPart == "Left Leg" ||
                                Settings::aimbotLockPart == "LeftUpperLeg")   lp = &cp.leftLeg;
                            else if (Settings::aimbotLockPart == "Right Leg" ||
                                Settings::aimbotLockPart == "RightUpperLeg")  lp = &cp.rightLeg;
                            if (!lp->address) continue;

                            RBX::Vector2 sp = visualEngine.worldToScreen(lp->getPosition());
                            if (!isValidScreen(sp)) continue;
                            if (Settings::aimbotWallCheck && !isPlayerVisible(sp, monitorWidth, monitorHeight, aimDC)) continue;
                            float dx = sp.x - mousePos.x, dy = sp.y - mousePos.y;
                            float dist = sqrtf(dx * dx + dy * dy);
                            if (dist < Settings::aimbotFOVRadius && dist < closestDistance)
                            {
                                closestDistance = dist; closestPlr = cp.model; chosenPart = Settings::aimbotLockPart; chosenScreen = sp;
                            }
                            continue;
                        }

                        float pbDist = FLT_MAX; std::string pbPart; RBX::Vector2 pbScreen;
                        for (int i = 0; i < 6; i++) {
                            if (!parts[i]->address) continue;
                            RBX::Vector2 sp = visualEngine.worldToScreen(parts[i]->getPosition());
                            if (!isValidScreen(sp)) continue;
                            if (Settings::aimbotWallCheck && !isPlayerVisible(sp, monitorWidth, monitorHeight, aimDC)) continue;
                            float dx = sp.x - mousePos.x, dy = sp.y - mousePos.y;
                            float d = sqrtf(dx * dx + dy * dy);
                            if (d < pbDist) { pbDist = d; pbPart = pnames[i]; pbScreen = sp; }
                        }
                        if (pbDist < closestDistance && pbDist <= Settings::aimbotFOVRadius)
                        {
                            closestDistance = pbDist; closestPlr = cp.model; chosenPart = pbPart; chosenScreen = pbScreen;
                        }
                    }

                    if (aimDC) ReleaseDC(nullptr, aimDC);

                    if (closestPlr.address != nullptr)
                    {
                        lockedPlr = closestPlr; locked = true; lockedPartName = chosenPart;
                    }
                }
            }

            if (keybindDown && locked && lockedPlr.address != nullptr)
            {
                // Look up locked player in cache — avoids stale address reads
                const CachedPlayer* lcp = nullptr;
                for (const CachedPlayer& cp : cachedPlayers)
                    if (cp.model.address == lockedPlr.address) { lcp = &cp; break; }

                if (!lcp) { locked = false; lockedPlr = RBX::Instance(nullptr); lockedPartName.clear(); }
                else
                {
                    // Get part from cache — zero findFirstChild calls
                    const RBX::Instance* lpInst = &lcp->torso;
                    if (lockedPartName == "Head")                                    lpInst = &lcp->head;
                    else if (lockedPartName == "Left Arm" || lockedPartName == "LeftUpperArm")  lpInst = &lcp->leftArm;
                    else if (lockedPartName == "Right Arm" || lockedPartName == "RightUpperArm") lpInst = &lcp->rightArm;
                    else if (lockedPartName == "Left Leg" || lockedPartName == "LeftUpperLeg")  lpInst = &lcp->leftLeg;
                    else if (lockedPartName == "Right Leg" || lockedPartName == "RightUpperLeg") lpInst = &lcp->rightLeg;
                    if (!lpInst->address) lpInst = &lcp->hrp;

                    RBX::Instance lockPart = *lpInst; // copy for getPosition/getPrimitive

                    if (!lockPart.address) { locked = false; lockedPlr = RBX::Instance(nullptr); lockedPartName.clear(); }
                    else
                    {
                        RBX::Vector3 lpp = lockPart.getPosition();
                        if (Settings::aimbotPredictionEnabled)
                        {
                            void* prim = lockPart.getPrimitive();
                            if (prim)
                            {
                                RBX::Vector3 vel = RBX::Memory::read<RBX::Vector3>((void*)((uintptr_t)prim + Offsets::Velocity));
                                lpp.x += vel.x / Settings::aimbotPredictionX;
                                lpp.y += vel.y / Settings::aimbotPredictionY;
                                lpp.z += vel.z / Settings::aimbotPredictionX;
                            }
                        }

                        RBX::Vector2 sp = visualEngine.worldToScreen(lpp);
                        if (isValidScreen(sp))
                        {
                            POINT mp; GetCursorPos(&mp);
                            float rawDx = sp.x - mp.x;
                            float rawDy = sp.y - mp.y;
                            float dist = sqrtf(rawDx * rawDx + rawDy * rawDy);

                            float smooth = (std::max)(1.f, Settings::aimbotSmoothness);
                            float dx, dy;

                            if (dist < 3.f || smooth <= 1.f)
                            {
                                // Within 3px or smoothness=1 — snap directly
                                dx = rawDx;
                                dy = rawDy;
                            }
                            else
                            {
                                // Ease toward target
                                dx = rawDx / smooth * Settings::aimbotStrenght * 10.f;
                                dy = rawDy / smooth * Settings::aimbotStrenght * 10.f;
                                dx = std::clamp(dx, -fabsf(rawDx), fabsf(rawDx));
                                dy = std::clamp(dy, -fabsf(rawDy), fabsf(rawDy));
                            }

                            if (fabsf(dx) > 0.05f || fabsf(dy) > 0.05f) MoveMouse(dx, dy);
                        }
                    }
                }
            }

            if (!keybindDown && keybindPrevDown)
            {
                locked = false; lockedPlr = RBX::Instance(nullptr); lockedPartName.clear();
            }

            keybindPrevDown = keybindDown;
        }

        // FOV circle
        if (Settings::aimbotFOVEnabled && (!Settings::rbxWindowNeedsToBeSelected || robloxFocused))
        {
            POINT mp; GetCursorPos(&mp);
            drawList->AddCircle({ (float)mp.x, (float)mp.y },
                Settings::aimbotFOVRadius, ImColor(Settings::aimbotFovColor));
        }

        // Triggerbot
        if (Settings::triggerbotEnabled
            && (Settings::triggerbotKey != 0 && (GetAsyncKeyState(Settings::triggerbotKey) & 0x8000))
            && (!Settings::rbxWindowNeedsToBeSelected || robloxFocused))
        {
            POINT mp; GetCursorPos(&mp);
            for (RBX::Instance plr : playersList)
            {
                if (plr.name() == localPlayer.name()) continue;

                RBX::Instance tp{ plr.findFirstChild(Settings::triggerbotTriggerPart) };
                if (!plr.findFirstChild("Torso").address)
                {
                    auto it = std::find(aimbotLockPartsR6.begin(), aimbotLockPartsR6.end(), Settings::triggerbotTriggerPart);
                    tp = plr.findFirstChild(aimbotLockPartsR15[std::distance(aimbotLockPartsR6.begin(), it)]);
                }

                RBX::Vector2 sp = visualEngine.worldToScreen(tp.getPosition());
                float dx = sp.x - mp.x, dy = sp.y - mp.y;

                if (sqrtf(dx * dx + dy * dy) < Settings::triggerbotDetectionRadius)
                {
                    INPUT inp{ 0 };
                    inp.type = INPUT_MOUSE; inp.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                    SendInput(1, &inp, sizeof(inp));
                    ZeroMemory(&inp, sizeof(inp));
                    inp.type = INPUT_MOUSE; inp.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    SendInput(1, &inp, sizeof(inp));

                    if (Settings::triggerbotIndicateClicking)
                        drawList->AddCircleFilled({ (float)mp.x, (float)mp.y },
                            Settings::triggerbotDetectionRadius, IM_COL32(255, 0, 0, 255));
                }
            }
        }

        // ── ESP + Chams + Tracers — single combined pass ──────────────────────
        {
            Settings::espType = std::string(Settings::espTypeName);
            int espTypeId = 0;
            if (Settings::espType == "Corners")       espTypeId = 1;
            else if (Settings::espType == "Skeleton") espTypeId = 2;

            ImColor espCol{ Settings::espColor };
            ImColor tracerCol{ Settings::tracerColor };

            float pulse = (sinf(static_cast<float>(GetTickCount64()) * 0.001f * Settings::chamsPulseSpeed) + 1.f) * 0.5f;
            ImU32 chamsCol = IM_COL32(
                (int)(Settings::chamsColor.x * 255),
                (int)(Settings::chamsColor.y * 255),
                (int)(Settings::chamsColor.z * 255),
                (int)(40 + pulse * 215));
            float chamsThick = 1.f + pulse * 2.5f;

            POINT mousePos; GetCursorPos(&mousePos);
            bool needsWall = (Settings::espEnabled && Settings::espWallCheck) ||
                (Settings::aimbotEnabled && Settings::aimbotWallCheck);
            HDC frameDC = needsWall ? GetDC(nullptr) : nullptr;

            for (const CachedPlayer& cp : cachedPlayers)
            {
                if (cp.name == localPlayer.name()) continue;
                if (Settings::espTeamCheck)
                {
                    // Method 1: Player node Team/TeamColor comparison
                    if (cp.playerNode.address && cp.playerNode.address != cp.model.address)
                    {
                        if (isSameTeam(localPlayer, cp.playerNode)) continue;
                    }
                    // Method 2: CB folder comparison — same parent folder = same team
                    // (workspace.Characters.Terrorists vs workspace.Characters.Counter-Terrorists)
                    void* localFolder2 = RBX::Memory::read<void*>((void*)((uintptr_t)localPlayerModelInstance.address + Offsets::Parent));
                    if (localFolder2 && cp.teamFolder && localFolder2 == cp.teamFolder) continue;
                    // Also try via player node if we have one
                    else if (cp.playerNode.address && cp.playerNode.address != cp.model.address)
                    {
                        if (isSameTeam(localPlayer, cp.playerNode)) continue;
                    }
                }

                float health = 0.f, maxHealth = 100.f;
                if (cp.humanoid.address)
                {
                    health = RBX::Memory::read<float>((void*)((uintptr_t)cp.humanoid.address + Offsets::Health));
                    maxHealth = RBX::Memory::read<float>((void*)((uintptr_t)cp.humanoid.address + Offsets::MaxHealth));
                    if (maxHealth <= 0.f) maxHealth = 100.f;
                }
                // Standard death check
                if (health <= 0.f) continue;
                bool isDead = false; // player is alive if we reach here

                // CB dead body check: live players always have a Player node
                // whose ModelInstance offset points to their character.
                // Dead bodies are orphaned — no Player references them anymore.
                // Only apply this check when we couldn't match a Player node.
                if (cp.playerNode.address == cp.model.address) // no real Player node found
                {
                    bool hasOwner = false;
                    for (RBX::Instance plr : players.getChildren())
                    {
                        void* miPtr2 = RBX::Memory::read<void*>((void*)((uintptr_t)plr.address + Offsets::ModelInstance));
                        if (miPtr2 == cp.model.address) { hasOwner = true; break; }
                    }
                    if (!hasOwner) continue; // dead body — skip
                }

                // Fast 2-point box: head (top) + HRP (center) — only 2 worldToScreen calls
                if (!cp.hrp.address) continue;
                RBX::Vector2 hrpSP = visualEngine.worldToScreen(cp.hrp.getPosition());
                if (hrpSP.x < -500.f || hrpSP.y < -500.f) continue;
                if (hrpSP.x > monitorWidth + 500 || hrpSP.y > monitorHeight + 500) continue;

                RBX::Vector2 headSP = hrpSP; // fallback
                if (cp.head.address)
                {
                    RBX::Vector2 h = visualEngine.worldToScreen(cp.head.getPosition());
                    if (h.x > -500.f && h.y > -500.f) headSP = h;
                }

                // Estimate box width from height (characters are roughly 0.5:1 ratio)
                float boxH = fabsf(hrpSP.y - headSP.y) * 2.2f; // full height
                float boxW = boxH * 0.55f;
                float cx = hrpSP.x;
                float topY = (std::min)(headSP.y, hrpSP.y) - boxH * 0.05f;

                float minX = cx - boxW * 0.5f;
                float maxX = cx + boxW * 0.5f;
                float minY = topY;
                float maxY = topY + boxH;

                bool anyValid = (boxH > 5.f); // sanity check
                if (!anyValid) continue;

                // Keep pts array for skeleton drawing (only fill head/torso/limbs)
                struct Pt { const RBX::Instance* inst; RBX::Vector2 sp; };
                std::array<Pt, 8> pts = { {
                    { &cp.head,       headSP }, { &cp.torso,     {} },
                    { &cp.leftArm,    {} },      { &cp.rightArm,  {} },
                    { &cp.leftLeg,    {} },      { &cp.rightLeg,  {} },
                    { &cp.lowerTorso, {} },      { &cp.hrp,       hrpSP },
                } };
                // Only compute remaining parts if skeleton mode is active
                if (espTypeId == 2)
                {
                    for (int pi = 1; pi <= 5; pi++)
                    {
                        if (!pts[pi].inst->address) continue;
                        pts[pi].sp = visualEngine.worldToScreen(pts[pi].inst->getPosition());
                    }
                }

                bool visible = true;
                if (frameDC && pts[1].sp.x != 0.f)
                    visible = isPlayerVisible(pts[1].sp, monitorWidth, monitorHeight, frameDC);

                float dist = cp.hrp.address ? hrp.getDistance(cp.hrp.getPosition()) : 0.f;
                // Head pivot is at head center so push top up by ~head radius
                // to cover the full head. Estimate: the box top should be ~18px
                // above the topmost point we tracked.
                const float padX = 8.f, padTop = 22.f, padBottom = 8.f;
                float bMinX = minX - padX, bMinY = minY - padTop;
                float bMaxX = maxX + padX, bMaxY = maxY + padBottom;

                // ESP
                if (Settings::espEnabled && (!Settings::rbxWindowNeedsToBeSelected || robloxFocused))
                {
                    if (!(Settings::espIgnoreDeadPlrs && isDead) &&
                        !(Settings::espWallCheck && !visible) &&
                        !(Settings::espDistance != 0 && dist > Settings::espDistance))
                    {
                        ImVec2 p1{ bMinX, bMinY }, p2{ bMaxX, bMaxY };
                        float bW = bMaxX - bMinX, bH = bMaxY - bMinY;
                        float cornerLen = (std::max)(8.f, (std::min)(bW * 0.22f, 18.f));

                        // Dark fill + black outline on both sides for contrast
                        drawList->AddRectFilled(p1, p2, IM_COL32(0, 0, 0, 40), 2.f);
                        drawList->AddRect(p1 - ImVec2(1, 1), p2 + ImVec2(1, 1), IM_COL32(0, 0, 0, 180), 2.f, 0, 3.f);
                        drawList->AddRect(p1 + ImVec2(1, 1), p2 - ImVec2(1, 1), IM_COL32(0, 0, 0, 180), 2.f, 0, 3.f);

                        if (espTypeId == 0) // Square
                        {
                            drawList->AddRect(p1, p2, espCol, 2.f, 0, 1.8f);
                        }
                        else if (espTypeId == 1) // Corners
                        {
                            float l = bMinX, r = bMaxX, t = bMinY, b = bMaxY;
                            drawList->AddLine({ l,t }, { l + cornerLen,t }, espCol, 2.5f);
                            drawList->AddLine({ l,t }, { l,t + cornerLen }, espCol, 2.5f);
                            drawList->AddLine({ r - cornerLen,t }, { r,t }, espCol, 2.5f);
                            drawList->AddLine({ r,t }, { r,t + cornerLen }, espCol, 2.5f);
                            drawList->AddLine({ l,b }, { l + cornerLen,b }, espCol, 2.5f);
                            drawList->AddLine({ l,b - cornerLen }, { l,b }, espCol, 2.5f);
                            drawList->AddLine({ r - cornerLen,b }, { r,b }, espCol, 2.5f);
                            drawList->AddLine({ r,b - cornerLen }, { r,b }, espCol, 2.5f);
                        }
                        else // Skeleton
                        {
                            auto vld = [](const RBX::Vector2& v) { return v.x > -500.f && v.y > -500.f; };
                            const auto& hd = pts[0].sp, & tr = pts[1].sp, & la = pts[2].sp,
                                & ra = pts[3].sp, & ll = pts[4].sp, & rl = pts[5].sp;
                            auto sl = [&](const RBX::Vector2& a, const RBX::Vector2& b2) {
                                drawList->AddLine({ a.x + 1,a.y + 1 }, { b2.x + 1,b2.y + 1 }, IM_COL32(0, 0, 0, 150), 3.f);
                                drawList->AddLine({ a.x,a.y }, { b2.x,b2.y }, espCol, 2.f);
                                };
                            if (vld(hd) && vld(tr)) sl(hd, tr);
                            if (vld(la) && vld(tr)) sl(la, tr);
                            if (vld(ra) && vld(tr)) sl(ra, tr);
                            if (vld(ll) && vld(tr)) sl(ll, tr);
                            if (vld(rl) && vld(tr)) sl(rl, tr);
                        }

                        // Name tag — pill bg + accent border
                        if (Settings::espShowName && !cp.name.empty())
                        {
                            std::string n = cp.name.length() > 20 ? cp.name.substr(0, 17) + "..." : cp.name;
                            ImVec2 ts = ImGui::CalcTextSize(n.c_str());
                            float tx = bMinX + bW * 0.5f - ts.x * 0.5f;
                            float ty = bMinY - ts.y - 8.f;
                            float pad = 4.f;
                            drawList->AddRectFilled({ tx - pad,ty - pad }, { tx + ts.x + pad,ty + ts.y + pad }, IM_COL32(10, 10, 10, 180), 4.f);
                            drawList->AddRect({ tx - pad,ty - pad }, { tx + ts.x + pad,ty + ts.y + pad }, ImGui::GetColorU32(ImColorToImVec4(main_color)), 4.f, 0, 0.8f);
                            drawList->AddText({ tx + 1,ty + 1 }, IM_COL32(0, 0, 0, 200), n.c_str());
                            drawList->AddText({ tx,ty }, IM_COL32_WHITE, n.c_str());
                        }

                        // Health bar — left side, gradient green→red
                        if (Settings::espShowHealth && cp.humanoid.address)
                        {
                            float pct = std::clamp(health / maxHealth, 0.f, 1.f);
                            float hbL = bMinX - 8.f, hbR = bMinX - 4.f;
                            float filled = bH * pct;
                            drawList->AddRectFilled({ hbL,bMinY }, { hbR,bMaxY }, IM_COL32(20, 20, 20, 200), 2.f);
                            if (filled > 0.5f) {
                                int r = (int)((std::min)(1.f, (1.f - pct) * 2.f) * 255);
                                int g = (int)((std::min)(1.f, pct * 2.f) * 255);
                                drawList->AddRectFilled({ hbL,bMaxY - filled }, { hbR,bMaxY }, IM_COL32(r, g, 0, 220), 2.f);
                            }
                            drawList->AddRect({ hbL - 0.5f,bMinY - 0.5f }, { hbR + 0.5f,bMaxY + 0.5f }, IM_COL32(0, 0, 0, 180), 2.f, 0, 0.8f);
                            char hpBuf[8]; snprintf(hpBuf, sizeof(hpBuf), "%d%%", (int)(pct * 100));
                            ImVec2 hts = ImGui::CalcTextSize(hpBuf);
                            float htx = hbL + (hbR - hbL) * 0.5f - hts.x * 0.5f;
                            drawList->AddText({ htx + 1,bMinY - hts.y - 3.f }, IM_COL32(0, 0, 0, 180), hpBuf);
                            drawList->AddText({ htx,bMinY - hts.y - 4.f }, IM_COL32_WHITE, hpBuf);
                        }

                        // Distance — bottom right, accent colored
                        if (Settings::espShowDistance && cp.hrp.address)
                        {
                            char buf[16]; snprintf(buf, sizeof(buf), "%dm", (int)dist);
                            ImVec2 ts = ImGui::CalcTextSize(buf);
                            drawList->AddText({ bMaxX - ts.x + 1,bMaxY + 4.f }, IM_COL32(0, 0, 0, 160), buf);
                            drawList->AddText({ bMaxX - ts.x,bMaxY + 3.f }, ImGui::GetColorU32(ImColorToImVec4(main_color)), buf);
                        }
                    }

                    // Chams
                    if (Settings::chamsEnabled && (!Settings::rbxWindowNeedsToBeSelected || robloxFocused))
                    {
                        if (!(Settings::espIgnoreDeadPlrs && isDead))
                        {
                            const float cp2 = 8.f;
                            drawList->AddRectFilled({ minX - cp2,minY - cp2 }, { maxX + cp2,maxY + cp2 },
                                IM_COL32((int)(Settings::chamsColor.x * 255), (int)(Settings::chamsColor.y * 255), (int)(Settings::chamsColor.z * 255), (int)(pulse * 35)), 4.f);
                            drawList->AddRect({ minX - cp2,minY - cp2 }, { maxX + cp2,maxY + cp2 }, chamsCol, 4.f, 0, chamsThick);
                            drawList->AddRect({ minX - cp2 + 3,minY - cp2 + 3 }, { maxX + cp2 - 3,maxY + cp2 - 3 },
                                IM_COL32((int)(Settings::chamsColor.x * 255), (int)(Settings::chamsColor.y * 255), (int)(Settings::chamsColor.z * 255), (int)(20 + pulse * 80)), 2.f, 0, 0.5f);
                        }
                    }

                    // Tracers
                    if (Settings::tracersEnabled && (!Settings::rbxWindowNeedsToBeSelected || robloxFocused))
                    {
                        if (!(Settings::espIgnoreDeadPlrs && isDead) && pts[1].sp.x != 0.f)
                        {
                            const RBX::Vector2& tr = pts[1].sp;
                            if (Settings::tracerType == "Mouse")
                                drawList->AddLine({ tr.x,tr.y }, { (float)mousePos.x,(float)mousePos.y }, tracerCol);
                            else if (Settings::tracerType == "Corner")
                                drawList->AddLine({ tr.x,tr.y }, { 0.f,0.f }, tracerCol);
                            else if (Settings::tracerType == "Top")
                                drawList->AddLine({ tr.x,tr.y }, { (float)(monitorWidth / 2),0.f }, tracerCol);
                            else if (Settings::tracerType == "Bottom")
                                drawList->AddLine({ tr.x,tr.y }, { (float)(monitorWidth / 2),(float)monitorHeight }, tracerCol);
                        }
                    }
                }

                if (frameDC) ReleaseDC(nullptr, frameDC);
            }

        }
        // ── Fly ───────────────────────────────────────────────────────────────
        if (Settings::flyEnabled)
        {
            if (Settings::flyKey != 0 && (GetAsyncKeyState(Settings::flyKey) & 1))
                Settings::flyKeyToggled = !Settings::flyKeyToggled;

            if (Settings::flyKeyToggled)
            {
                float flySpeed = 5.f;
                void* primitive = hrp.address ? hrp.getPrimitive() : nullptr;
                if (!primitive) continue;

                RBX::Vector3 pos = RBX::Memory::read<RBX::Vector3>((void*)((uintptr_t)primitive + Offsets::Position));
                RBX::Matrix3 camRot{};
                bool camHasData = false;

                if (camera.address)
                {
                    camRot = RBX::Memory::read<RBX::Matrix3>((void*)((uintptr_t)camera.address + Offsets::CameraRotation));
                    float mag = 0.f;
                    for (int i = 0;i < 9;i++) mag += fabsf(camRot.data[i]);
                    camHasData = (mag > 1e-6f);
                }

                RBX::Vector3 look{ 0,0,-1 }, right{ 1,0,0 };
                if (camHasData)
                {
                    look = { -camRot.data[2], -camRot.data[5], -camRot.data[8] };
                    right = { camRot.data[0],  camRot.data[3],  camRot.data[6] };
                }

                RBX::Vector3 move{ 0,0,0 };
                if (GetAsyncKeyState('W') & 0x8000) { move.x += look.x;  move.y += look.y;  move.z += look.z; }
                if (GetAsyncKeyState('S') & 0x8000) { move.x -= look.x;  move.y -= look.y;  move.z -= look.z; }
                if (GetAsyncKeyState('A') & 0x8000) { move.x -= right.x; move.y -= right.y; move.z -= right.z; }
                if (GetAsyncKeyState('D') & 0x8000) { move.x += right.x; move.y += right.y; move.z += right.z; }
                if (GetAsyncKeyState(VK_SPACE) & 0x8000) move.y += 1.f;
                if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) move.y -= 1.f;

                float len = sqrtf(move.x * move.x + move.y * move.y + move.z * move.z);
                if (len > 1e-6f) { move.x = (move.x / len) * flySpeed; move.y = (move.y / len) * flySpeed; move.z = (move.z / len) * flySpeed; }
                else { move.x = move.y = move.z = 0.f; }

                RBX::Vector3 newPos{ pos.x + move.x, pos.y + move.y, pos.z + move.z };
                RBX::Memory::write<RBX::Vector3>((void*)((uintptr_t)primitive + Offsets::Position), newPos);
                RBX::Memory::write<RBX::Vector3>((void*)((uintptr_t)primitive + Offsets::Velocity), { 0,0,0 });
            }
        }

        // ── Noclip ────────────────────────────────────────────────────────────
        if (Settings::noclipEnabled)
        {
            RBX::Instance head = localPlayerModelInstance.findFirstChild("Head");
            RBX::Instance torso = localPlayerModelInstance.findFirstChild("Torso");
            RBX::Instance torso2{ nullptr };
            if (!torso.address)
            {
                torso = localPlayerModelInstance.findFirstChild("UpperTorso");
                torso2 = localPlayerModelInstance.findFirstChild("LowerTorso");
            }
            RBX::Memory::write<int>((void*)((uintptr_t)head.getPrimitive() + Offsets::CanCollide), 0);
            RBX::Memory::write<int>((void*)((uintptr_t)torso.getPrimitive() + Offsets::CanCollide), 0);
            if (torso2.address)
                RBX::Memory::write<int>((void*)((uintptr_t)torso2.getPrimitive() + Offsets::CanCollide), 0);
            RBX::Memory::write<int>((void*)((uintptr_t)hrp.getPrimitive() + Offsets::CanCollide), 0);
        }

        // ── Orbit ─────────────────────────────────────────────────────────────
        if (Settings::orbitEnabled)
        {
            RBX::Instance plr = players.findFirstChild(Settings::othersRobloxPlr);
            RBX::Instance plrMi = plr.getModelInstance();
            RBX::Instance plrHrp = plrMi.findFirstChild("HumanoidRootPart");
            void* primitive = hrp.getPrimitive();

            rot += 0.016f * rotspeed;
            double offsetAngle = rot;
            RBX::Vector3 offset{ (float)(sin(offsetAngle) * eclipse), 0.f, (float)(cos(offsetAngle) * radius) };
            RBX::Vector3 target = RBX::Memory::read<RBX::Vector3>((void*)((uintptr_t)plrHrp.getPrimitive() + Offsets::Position));
            RBX::Vector3 newPos{ target.x + offset.x, target.y + offset.y, target.z + offset.z };
            RBX::Memory::write<RBX::Vector3>((void*)((uintptr_t)primitive + Offsets::Position), newPos);
        }

        renderer.EndRender();
        Sleep(Settings::mainLoopDelay);
    }

    renderer.Shutdown();
    return 0;
}