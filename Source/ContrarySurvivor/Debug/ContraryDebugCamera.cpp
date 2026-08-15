// Свободная камера для съёмки (F1) — тихая версия движковой debug-камеры. Обоснование — в заголовке.

#include "ContraryDebugCamera.h"
#include "Components/InputComponent.h"
#include "Components/DrawFrustumComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"

DEFINE_LOG_CATEGORY_STATIC(LogContraryDebugCam, Log, All);

void AContraryDebugCameraController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}

	// Снимаем движковые привязки, которые портят кадр: перебор вьюмодов (V → Wireframe и
	// далее по кругу), буферы GBuffer (B / Enter / стрелки), заморозка рендера (F).
	// Имена действий — DebugCameraController.cpp:80-107 (UE 5.5).
	static const FName BindingsToDrop[] = {
		TEXT("DebugCamera_CycleViewMode"),
		TEXT("DebugCamera_ToggleBufferVisualizationOverview"),
		TEXT("DebugCamera_ToggleBufferVisualizationFull"),
		TEXT("DebugCamera_BufferVisualizationUp"),
		TEXT("DebugCamera_BufferVisualizationDown"),
		TEXT("DebugCamera_BufferVisualizationLeft"),
		TEXT("DebugCamera_BufferVisualizationRight"),
		TEXT("DebugCamera_FreezeRendering"),
	};
	for (int32 i = InputComponent->GetNumActionBindings() - 1; i >= 0; --i)
	{
		const FName Name = InputComponent->GetActionBinding(i).GetActionName();
		for (const FName& Drop : BindingsToDrop)
		{
			if (Name == Drop)
			{
				InputComponent->RemoveActionBinding(i);
				break;
			}
		}
	}

	// F1 (действие QAToggleDebugCam, Config/DefaultInput.ini) — выход из камеры к игроку.
	InputComponent->BindAction(TEXT("QAToggleDebugCam"), IE_Pressed, this, &AContraryDebugCameraController::OnExitDebugCamera);
}

void AContraryDebugCameraController::PostInitializeComponents()
{
	Super::PostInitializeComponents(); // здесь движок спавнит ADebugCameraHUD в MyHUD

	// Надписи слева и белая линия в центре кадра рисуются в ADebugCameraHUD::PostRender
	// только при bShowHUD (DebugCameraHUD.cpp:67). Прячем; Backspace (DebugCamera_ToggleDisplay)
	// при нужде вернёт.
	if (AHUD* Hud = GetHUD())
	{
		Hud->bShowHUD = false;
	}
}

void AContraryDebugCameraController::OnActivate(APlayerController* OriginalPC)
{
	Super::OnActivate(OriginalPC);

	// Пирамида обзора исходной камеры (жёлто-белые линии от точки отрыва) на кадре не нужна.
	if (DrawFrustum)
	{
		DrawFrustum->SetVisibility(false);
	}

	// Вьюмод — всегда Lit. Лечит Wireframe, застрявший с прошлого раза (движковая камера при
	// выходе вьюмод не возвращает: DebugCameraController.cpp:471-510).
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			if (Viewport->ViewModeIndex != VMI_Lit)
			{
				Viewport->ConsoleCommand(TEXT("VIEWMODE Lit"));
				UE_LOG(LogContraryDebugCam, Display, TEXT("QA: F1 free camera — view mode forced back to Lit (was %d)"), Viewport->ViewModeIndex);
			}
		}
	}
}

void AContraryDebugCameraController::OnExitDebugCamera()
{
	// Уходит в cheat-manager ЭТОГО контроллера (DebugCameraController.cpp:294: сначала свой
	// Player->Exec) → UCheatManager::ToggleDebugCamera → IsDebugCameraActive → DisableDebugCamera.
	ConsoleCommand(TEXT("ToggleDebugCamera"), /*bWriteToLog=*/true);
	UE_LOG(LogContraryDebugCam, Display, TEXT("QA: F1 free camera off (back to player)"));
}

UContraryCheatManager::UContraryCheatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DebugCameraControllerClass = AContraryDebugCameraController::StaticClass();
}
