// Свободная камера для съёмки (клавиша F1) — тихая версия движковой debug-камеры.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DebugCameraController.h"
#include "GameFramework/CheatManager.h"
#include "ContraryDebugCamera.generated.h"

/**
 * Свободная камера F1 (Ринат, 2026-08-15: «по нажатию свободной камеры включается WireFrame,
 * подсвечивается полоса по центру кадра, debug-надписи слева — убери»).
 *
 * Движковая ADebugCameraController (UE 5.5, Engine/Private/DebugCameraController.cpp) сама
 * вешает клавиши: V — перебор вьюмодов (VIEWMODE ... → Wireframe и т.д., строка 98/146; при
 * выходе вьюмод НЕ возвращается — OnDeactivate его не трогает), B/Enter/стрелки — буферы GBuffer,
 * F — заморозка рендера, и рисует свой HUD (ADebugCameraHUD::PostRender: надписи слева на
 * 5 % ширины + белая линия 30 см в точке трассы по центру экрана, DebugCameraHUD.cpp:97,193).
 * Плюс рисует пирамиду обзора исходной камеры (DrawFrustum, «show camfrustums»).
 *
 * Здесь всё это выключено: HUD спрятан (bShowHUD=false — Backspace вернёт при нужде),
 * привязки V/B/Enter/стрелок/F сняты, пирамида скрыта, при входе вьюмод принудительно Lit
 * (лечит и застрявший с прошлой сессии Wireframe). Управление полётом (WASD/мышь/колесо =
 * скорость, запятая-точка = FOV, O = орбита) — как в движке.
 *
 * ВЫХОД: F1 (действие QAToggleDebugCam из Config/DefaultInput.ini) привязан прямо здесь и
 * зовёт ConsoleCommand("ToggleDebugCamera") — команда уходит в cheat-manager ЭТОГО контроллера
 * (DebugCameraController.cpp:294 — сначала свой Player->Exec), а UCheatManager::ToggleDebugCamera
 * при активной камере делает DisableDebugCamera. Без этой привязки из камеры не выйти клавишей:
 * пока она активна, ввод игрового контроллера не обрабатывается (Player->SwitchController).
 */
UCLASS(NotBlueprintable)
class CONTRARYSURVIVOR_API AContraryDebugCameraController : public ADebugCameraController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void PostInitializeComponents() override;
	virtual void OnActivate(APlayerController* OriginalPC) override;

private:
	// F1 внутри камеры — вернуться к игроку.
	void OnExitDebugCamera();
};

/**
 * Cheat-manager игры: единственная задача — подставить наш класс свободной камеры
 * (UCheatManager::DebugCameraControllerClass, CheatManager.cpp:74 — движок ставит там
 * ADebugCameraController). Назначается в конструкторе AContrarySurvivorPlayerController
 * (CheatClass). Сам cheat-manager в Shipping движком не создаётся (UE_WITH_CHEAT_MANAGER).
 */
UCLASS(NotBlueprintable)
class CONTRARYSURVIVOR_API UContraryCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UContraryCheatManager(const FObjectInitializer& ObjectInitializer);
};
