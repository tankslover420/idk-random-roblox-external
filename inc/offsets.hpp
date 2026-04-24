#pragma once

#include <Windows.h>
#include <wininet.h>
#include <string>

#include "dependencies/json.hpp"

#pragma comment(lib, "wininet.lib")

// https://offsets.ntgetwritewatch.workers.dev/offsets.hpp

namespace Offsets
{
    inline uintptr_t Adornee;
    inline uintptr_t Anchored;
    inline uintptr_t AnchoredMask;
    inline uintptr_t AnimationId;
    inline uintptr_t AttributeToNext;
    inline uintptr_t AttributeToValue;
    inline uintptr_t AutoJumpEnabled;
    inline uintptr_t BeamBrightness;
    inline uintptr_t BeamColor;
    inline uintptr_t BeamLightEmission;
    inline uintptr_t BeamLightInfuence;
    inline uintptr_t CFrame;
    inline uintptr_t Camera;
    inline uintptr_t CameraMaxZoomDistance;
    inline uintptr_t CameraMinZoomDistance;
    inline uintptr_t CameraMode;
    inline uintptr_t CameraPos;
    inline uintptr_t CameraRotation;
    inline uintptr_t CameraSubject;
    inline uintptr_t CameraType;
    inline uintptr_t CanCollide;
    inline uintptr_t CanCollideMask;
    inline uintptr_t CanTouch;
    inline uintptr_t CanTouchMask;
    inline uintptr_t CharacterAppearanceId;
    inline uintptr_t Children;
    inline uintptr_t ChildrenEnd;
    inline uintptr_t ClassDescriptor;
    inline uintptr_t ClassDescriptorToClassName;
    inline uintptr_t ClickDetectorMaxActivationDistance;
    inline uintptr_t ClockTime;
    inline uintptr_t CreatorId;
    inline uintptr_t DataModelDeleterPointer;
    inline uintptr_t DataModelPrimitiveCount;
    inline uintptr_t DataModelToRenderView1;
    inline uintptr_t DataModelToRenderView2;
    inline uintptr_t DataModelToRenderView3;
    inline uintptr_t DecalTexture;
    inline uintptr_t Deleter;
    inline uintptr_t DeleterBack;
    inline uintptr_t Dimensions;
    inline uintptr_t DisplayName;
    inline uintptr_t EvaluateStateMachine;
    inline uintptr_t FOV;
    inline uintptr_t FakeDataModelPointer;
    inline uintptr_t FakeDataModelToDataModel;
    inline uintptr_t FogColor;
    inline uintptr_t FogEnd;
    inline uintptr_t FogStart;
    inline uintptr_t ForceNewAFKDuration;
    inline uintptr_t FramePositionOffsetX;
    inline uintptr_t FramePositionOffsetY;
    inline uintptr_t FramePositionX;
    inline uintptr_t FramePositionY;
    inline uintptr_t FrameRotation;
    inline uintptr_t FrameSizeOffsetX;
    inline uintptr_t FrameSizeOffsetY;
    inline uintptr_t FrameSizeX;
    inline uintptr_t FrameSizeY;
    inline uintptr_t GameId;
    inline uintptr_t GameLoaded;
    inline uintptr_t Gravity;
    inline uintptr_t Health;
    inline uintptr_t HealthDisplayDistance;
    inline uintptr_t HipHeight;
    inline uintptr_t HumanoidDisplayName;
    inline uintptr_t HumanoidState;
    inline uintptr_t HumanoidStateId;
    inline uintptr_t InputObject;
    inline uintptr_t InsetMaxX;
    inline uintptr_t InsetMaxY;
    inline uintptr_t InsetMinX;
    inline uintptr_t InsetMinY;
    inline uintptr_t InstanceAttributePointer1;
    inline uintptr_t InstanceAttributePointer2;
    inline uintptr_t InstanceCapabilities;
    inline uintptr_t JobEnd;
    inline uintptr_t JobId;
    inline uintptr_t JobStart;
    inline uintptr_t Job_Name;
    inline uintptr_t JobsPointer;
    inline uintptr_t JumpPower;
    inline uintptr_t LocalPlayer;
    inline uintptr_t LocalScriptByteCode;
    inline uintptr_t LocalScriptBytecodePointer;
    inline uintptr_t LocalScriptHash;
    inline uintptr_t MaterialType;
    inline uintptr_t MaxHealth;
    inline uintptr_t MaxSlopeAngle;
    inline uintptr_t MeshPartColor3;
    inline uintptr_t MeshPartTexture;
    inline uintptr_t ModelInstance;
    inline uintptr_t ModuleScriptByteCode;
    inline uintptr_t ModuleScriptBytecodePointer;
    inline uintptr_t ModuleScriptHash;
    inline uintptr_t MoonTextureId;
    inline uintptr_t MousePosition;
    inline uintptr_t MouseSensitivity;
    inline uintptr_t MoveDirection;
    inline uintptr_t Name;
    inline uintptr_t NameDisplayDistance;
    inline uintptr_t NameSize;
    inline uintptr_t OnDemandInstance;
    inline uintptr_t OutdoorAmbient;
    inline uintptr_t Parent;
    inline uintptr_t PartSize;
    inline uintptr_t Ping;
    inline uintptr_t PlaceId;
    inline uintptr_t PlayerConfigurerPointer;
    inline uintptr_t PlayerMouse;
    inline uintptr_t Position;
    inline uintptr_t Primitive;
    inline uintptr_t PrimitiveValidateValue;
    inline uintptr_t PrimitivesPointer1;
    inline uintptr_t PrimitivesPointer2;
    inline uintptr_t ProximityPromptActionText;
    inline uintptr_t ProximityPromptEnabled;
    inline uintptr_t ProximityPromptGamepadKeyCode;
    inline uintptr_t ProximityPromptHoldDuraction;
    inline uintptr_t ProximityPromptMaxActivationDistance;
    inline uintptr_t ProximityPromptMaxObjectText;
    inline uintptr_t RenderJobToDataModel;
    inline uintptr_t RenderJobToFakeDataModel;
    inline uintptr_t RenderJobToRenderView;
    inline uintptr_t RequireBypass;
    inline uintptr_t RigType;
    inline uintptr_t Rotation;
    inline uintptr_t RunContext;
    inline uintptr_t ScriptContext;
    inline uintptr_t Sit;
    inline uintptr_t SkyboxBk;
    inline uintptr_t SkyboxDn;
    inline uintptr_t SkyboxFt;
    inline uintptr_t SkyboxLf;
    inline uintptr_t SkyboxRt;
    inline uintptr_t SkyboxUp;
    inline uintptr_t SoundId;
    inline uintptr_t StarCount;
    inline uintptr_t StringLength;
    inline uintptr_t SunTextureId;
    inline uintptr_t TagList;
    inline uintptr_t TaskSchedulerMaxFPS;
    inline uintptr_t TaskSchedulerPointer;
    inline uintptr_t Team;
    inline uintptr_t TeamColor;
    inline uintptr_t TextLabelText;
    inline uintptr_t TextLabelVisible;
    inline uintptr_t Tool_Grip_Position;
    inline uintptr_t Transparency;
    inline uintptr_t UserId;
    inline uintptr_t Value;
    inline uintptr_t Velocity;
    inline uintptr_t ViewportSize;
    inline uintptr_t VisualEngine;
    inline uintptr_t VisualEnginePointer;
    inline uintptr_t VisualEngineToDataModel1;
    inline uintptr_t VisualEngineToDataModel2;
    inline uintptr_t WalkSpeed;
    inline uintptr_t WalkSpeedCheck;
    inline uintptr_t Workspace;
    inline uintptr_t WorkspaceToWorld;
    inline uintptr_t viewmatrix;

    bool fetchOffsets()
    {
        HINTERNET hSession{ InternetOpenA("OffsetsFetcher", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0) };
        if (!hSession)
        {
            return false;
        }

        HINTERNET hUrl{ InternetOpenUrlA(hSession, "https://offsets.ntgetwritewatch.workers.dev/offsets.json", nullptr, 0, INTERNET_FLAG_RELOAD, 0) };
        if (!hUrl)
        {
            return false;
        };

        std::string content;
        char buffer[8192];
        DWORD bytesRead;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
        {
            content.append(buffer, bytesRead);
        }

        InternetCloseHandle(hSession);
        InternetCloseHandle(hUrl);

        nlohmann::json offsetsJ{ nlohmann::json::parse(content) };

#define ASSIGN_OFFSET(name) if (offsetsJ.contains(#name)) name = static_cast<uintptr_t>(std::stoull(offsetsJ[#name].get<std::string>(), nullptr, 0));

        ASSIGN_OFFSET(Adornee)
        ASSIGN_OFFSET(Anchored)
        ASSIGN_OFFSET(AnchoredMask)
        ASSIGN_OFFSET(AnimationId)
        ASSIGN_OFFSET(AttributeToNext)
        ASSIGN_OFFSET(AttributeToValue)
        ASSIGN_OFFSET(AutoJumpEnabled)
        ASSIGN_OFFSET(BeamBrightness)
        ASSIGN_OFFSET(BeamColor)
        ASSIGN_OFFSET(BeamLightEmission)
        ASSIGN_OFFSET(BeamLightInfuence)
        ASSIGN_OFFSET(CFrame)
        ASSIGN_OFFSET(Camera)
        ASSIGN_OFFSET(CameraMaxZoomDistance)
        ASSIGN_OFFSET(CameraMinZoomDistance)
        ASSIGN_OFFSET(CameraMode)
        ASSIGN_OFFSET(CameraPos)
        ASSIGN_OFFSET(CameraRotation)
        ASSIGN_OFFSET(CameraSubject)
        ASSIGN_OFFSET(CameraType)
        ASSIGN_OFFSET(CanCollide)
        ASSIGN_OFFSET(CanCollideMask)
        ASSIGN_OFFSET(CanTouch)
        ASSIGN_OFFSET(CanTouchMask)
        ASSIGN_OFFSET(CharacterAppearanceId)
        ASSIGN_OFFSET(Children)
        ASSIGN_OFFSET(ChildrenEnd)
        ASSIGN_OFFSET(ClassDescriptor)
        ASSIGN_OFFSET(ClassDescriptorToClassName)
        ASSIGN_OFFSET(ClickDetectorMaxActivationDistance)
        ASSIGN_OFFSET(ClockTime)
        ASSIGN_OFFSET(CreatorId)
        ASSIGN_OFFSET(DataModelDeleterPointer)
        ASSIGN_OFFSET(DataModelPrimitiveCount)
        ASSIGN_OFFSET(DataModelToRenderView1)
        ASSIGN_OFFSET(DataModelToRenderView2)
        ASSIGN_OFFSET(DataModelToRenderView3)
        ASSIGN_OFFSET(DecalTexture)
        ASSIGN_OFFSET(Deleter)
        ASSIGN_OFFSET(DeleterBack)
        ASSIGN_OFFSET(Dimensions)
        ASSIGN_OFFSET(DisplayName)
        ASSIGN_OFFSET(EvaluateStateMachine)
        ASSIGN_OFFSET(FOV)
        ASSIGN_OFFSET(FakeDataModelPointer)
        ASSIGN_OFFSET(FakeDataModelToDataModel)
        ASSIGN_OFFSET(FogColor)
        ASSIGN_OFFSET(FogEnd)
        ASSIGN_OFFSET(FogStart)
        ASSIGN_OFFSET(ForceNewAFKDuration)
        ASSIGN_OFFSET(FramePositionOffsetX)
        ASSIGN_OFFSET(FramePositionOffsetY)
        ASSIGN_OFFSET(FramePositionX)
        ASSIGN_OFFSET(FramePositionY)
        ASSIGN_OFFSET(FrameRotation)
        ASSIGN_OFFSET(FrameSizeOffsetX)
        ASSIGN_OFFSET(FrameSizeOffsetY)
        ASSIGN_OFFSET(FrameSizeX)
        ASSIGN_OFFSET(FrameSizeY)
        ASSIGN_OFFSET(GameId)
        ASSIGN_OFFSET(GameLoaded)
        ASSIGN_OFFSET(Gravity)
        ASSIGN_OFFSET(Health)
        ASSIGN_OFFSET(HealthDisplayDistance)
        ASSIGN_OFFSET(HipHeight)
        ASSIGN_OFFSET(HumanoidDisplayName)
        ASSIGN_OFFSET(HumanoidState)
        ASSIGN_OFFSET(HumanoidStateId)
        ASSIGN_OFFSET(InputObject)
        ASSIGN_OFFSET(InsetMaxX)
        ASSIGN_OFFSET(InsetMaxY)
        ASSIGN_OFFSET(InsetMinX)
        ASSIGN_OFFSET(InsetMinY)
        ASSIGN_OFFSET(InstanceAttributePointer1)
        ASSIGN_OFFSET(InstanceAttributePointer2)
        ASSIGN_OFFSET(InstanceCapabilities)
        ASSIGN_OFFSET(JobEnd)
        ASSIGN_OFFSET(JobId)
        ASSIGN_OFFSET(JobStart)
        ASSIGN_OFFSET(Job_Name)
        ASSIGN_OFFSET(JobsPointer)
        ASSIGN_OFFSET(JumpPower)
        ASSIGN_OFFSET(LocalPlayer)
        ASSIGN_OFFSET(LocalScriptByteCode)
        ASSIGN_OFFSET(LocalScriptBytecodePointer)
        ASSIGN_OFFSET(LocalScriptHash)
        ASSIGN_OFFSET(MaterialType)
        ASSIGN_OFFSET(MaxHealth)
        ASSIGN_OFFSET(MaxSlopeAngle)
        ASSIGN_OFFSET(MeshPartColor3)
        ASSIGN_OFFSET(MeshPartTexture)
        ASSIGN_OFFSET(ModelInstance)
        ASSIGN_OFFSET(ModuleScriptByteCode)
        ASSIGN_OFFSET(ModuleScriptBytecodePointer)
        ASSIGN_OFFSET(ModuleScriptHash)
        ASSIGN_OFFSET(MoonTextureId)
        ASSIGN_OFFSET(MousePosition)
        ASSIGN_OFFSET(MouseSensitivity)
        ASSIGN_OFFSET(MoveDirection)
        ASSIGN_OFFSET(Name)
        ASSIGN_OFFSET(NameDisplayDistance)
        ASSIGN_OFFSET(NameSize)
        ASSIGN_OFFSET(OnDemandInstance)
        ASSIGN_OFFSET(OutdoorAmbient)
        ASSIGN_OFFSET(Parent)
        ASSIGN_OFFSET(PartSize)
        ASSIGN_OFFSET(Ping)
        ASSIGN_OFFSET(PlaceId)
        ASSIGN_OFFSET(PlayerConfigurerPointer)
        ASSIGN_OFFSET(PlayerMouse)
        ASSIGN_OFFSET(Position)
        ASSIGN_OFFSET(Primitive)
        ASSIGN_OFFSET(PrimitiveValidateValue)
        ASSIGN_OFFSET(PrimitivesPointer1)
        ASSIGN_OFFSET(PrimitivesPointer2)
        ASSIGN_OFFSET(ProximityPromptActionText)
        ASSIGN_OFFSET(ProximityPromptEnabled)
        ASSIGN_OFFSET(ProximityPromptGamepadKeyCode)
        ASSIGN_OFFSET(ProximityPromptHoldDuraction)
        ASSIGN_OFFSET(ProximityPromptMaxActivationDistance)
        ASSIGN_OFFSET(ProximityPromptMaxObjectText)
        ASSIGN_OFFSET(RenderJobToDataModel)
        ASSIGN_OFFSET(RenderJobToFakeDataModel)
        ASSIGN_OFFSET(RenderJobToRenderView)
        ASSIGN_OFFSET(RequireBypass)
        ASSIGN_OFFSET(RigType)
        ASSIGN_OFFSET(Rotation)
        ASSIGN_OFFSET(RunContext)
        ASSIGN_OFFSET(ScriptContext)
        ASSIGN_OFFSET(Sit)
        ASSIGN_OFFSET(SkyboxBk)
        ASSIGN_OFFSET(SkyboxDn)
        ASSIGN_OFFSET(SkyboxFt)
        ASSIGN_OFFSET(SkyboxLf)
        ASSIGN_OFFSET(SkyboxRt)
        ASSIGN_OFFSET(SkyboxUp)
        ASSIGN_OFFSET(SoundId)
        ASSIGN_OFFSET(StarCount)
        ASSIGN_OFFSET(StringLength)
        ASSIGN_OFFSET(SunTextureId)
        ASSIGN_OFFSET(TagList)
        ASSIGN_OFFSET(TaskSchedulerMaxFPS)
        ASSIGN_OFFSET(TaskSchedulerPointer)
        ASSIGN_OFFSET(Team)
        ASSIGN_OFFSET(TeamColor)
        ASSIGN_OFFSET(TextLabelText)
        ASSIGN_OFFSET(TextLabelVisible)
        ASSIGN_OFFSET(Tool_Grip_Position)
        ASSIGN_OFFSET(Transparency)
        ASSIGN_OFFSET(UserId)
        ASSIGN_OFFSET(Value)
        ASSIGN_OFFSET(Velocity)
        ASSIGN_OFFSET(ViewportSize)
        ASSIGN_OFFSET(VisualEngine)
        ASSIGN_OFFSET(VisualEnginePointer)
        ASSIGN_OFFSET(VisualEngineToDataModel1)
        ASSIGN_OFFSET(VisualEngineToDataModel2)
        ASSIGN_OFFSET(WalkSpeed)
        ASSIGN_OFFSET(WalkSpeedCheck)
        ASSIGN_OFFSET(Workspace)
        ASSIGN_OFFSET(WorkspaceToWorld)
        ASSIGN_OFFSET(viewmatrix)

#undef ASSIGN_OFFSET

        return true;
    }
}