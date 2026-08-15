// Свободная камера для съёмки (клавиша F1) — тихая версия движковой debug-камеры.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DebugCameraController.h"
#include "GameFramework/CheatManager.h"
#include "Containers/Ticker.h"                  // FTSTicker (опрос папки скриптов съёмки)
#include "ContrarySurvivor/Debug/QADebug.h"      // CONTRARY_WITH_QA_CHEATS
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

class AContrarySurvivorPlayerController;
class APlayerCharacter;
class AWolfCharacter;

/**
 * Cheat-manager игры. Две задачи:
 *  1) подставить наш класс свободной камеры (UCheatManager::DebugCameraControllerClass,
 *     CheatManager.cpp:74 — движок ставит там ADebugCameraController). Назначается в
 *     конструкторе AContrarySurvivorPlayerController (CheatClass);
 *  2) служебные консольные команды для съёмки витринных кадров магазина (2026-08-15,
 *     реализация — Debug/ContraryShowcaseCheats.cpp): постановка сцены текстовыми
 *     скриптами — телепорт, обездвиженные волки, выдача брони/предметов, окна, камера, снимок.
 *     Список команд печатает `QAHelp`.
 *
 * Сам cheat-manager в Shipping движком не создаётся (UE_WITH_CHEAT_MANAGER), а всё ниже под
 * #if CONTRARY_WITH_QA_CHEATS в Shipping и не компилируется.
 *
 * ПОЧЕМУ НЕ UFUNCTION(Exec): UHT (UhtHeaderFileParser.cs:1044, UE 5.5) не знает проектных
 * условий препроцессора — блок под `#if CONTRARY_WITH_QA_CHEATS` он пропускает целиком, и
 * UFUNCTION внутри него отражения не получит; а снаружи блока команды попали бы в
 * публикационную сборку. Поэтому команды разбираются в переопределённом ProcessConsoleExec:
 * движок зовёт его для cheat-manager'а на каждую консольную строку (Player.cpp:141 —
 * цепочка ULocalPlayer::Exec), а неизвестное уходит в Super (штатные exec-функции
 * UCheatManager: ToggleDebugCamera, Teleport, …).
 */
UCLASS(NotBlueprintable)
class CONTRARYSURVIVOR_API UContraryCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UContraryCheatManager(const FObjectInitializer& ObjectInitializer);

#if CONTRARY_WITH_QA_CHEATS
	// --- Точки входа движка ---
	virtual void InitCheatManager() override;
	virtual void BeginDestroy() override;
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;

	// Снимок ТОЛЬКО игровой области (мир + UMG-интерфейс, без остального окна редактора при
	// PIE) в файл AbsPngPath (папка создаётся). Кадр берётся в конце текущего кадра, файл
	// пишется по готовности; итог — строкой «QA: <LogTag> saved <путь> (WxH, …)» или
	// «QA: <LogTag> FAILED …» в LogQA. Общий хелпер клавиши G (OnQAScreenshot) и команды QAShot.
	static void TakeViewportShot(UWorld* World, const FString& AbsPngPath, const TCHAR* LogTag = TEXT("SHOT"));

	// --- Команды (реализация — Debug/ContraryShowcaseCheats.cpp). Публичные, потому что их
	// адреса лежат в таблице команд на уровне файла (имя -> обработчик -> подсказка QAHelp). ---
	void CmdHelp(const TCHAR* Args, FOutputDevice& Ar);
	void CmdRunScript(const TCHAR* Args, FOutputDevice& Ar);
	void CmdShot(const TCHAR* Args, FOutputDevice& Ar);
	void CmdShotDir(const TCHAR* Args, FOutputDevice& Ar);
	void CmdTeleport(const TCHAR* Args, FOutputDevice& Ar);
	void CmdWhere(const TCHAR* Args, FOutputDevice& Ar);
	void CmdDumpActors(const TCHAR* Args, FOutputDevice& Ar);
	void CmdSpawnWolf(const TCHAR* Args, FOutputDevice& Ar);
	void CmdClearWolves(const TCHAR* Args, FOutputDevice& Ar);
	void CmdFreezeEnemies(const TCHAR* Args, FOutputDevice& Ar);
	void CmdGod(const TCHAR* Args, FOutputDevice& Ar);
	void CmdPlayerFace(const TCHAR* Args, FOutputDevice& Ar);
	void CmdFire(const TCHAR* Args, FOutputDevice& Ar);
	void CmdEquipPistol(const TCHAR* Args, FOutputDevice& Ar);
	void CmdGiveShowcaseLoot(const TCHAR* Args, FOutputDevice& Ar);
	void CmdGiveMoney(const TCHAR* Args, FOutputDevice& Ar);
	void CmdOpenInventory(const TCHAR* Args, FOutputDevice& Ar);
	void CmdOpenShop(const TCHAR* Args, FOutputDevice& Ar);
	void CmdCloseUI(const TCHAR* Args, FOutputDevice& Ar);
	void CmdFullStats(const TCHAR* Args, FOutputDevice& Ar);
	void CmdEndIntro(const TCHAR* Args, FOutputDevice& Ar);
	void CmdMenuContinue(const TCHAR* Args, FOutputDevice& Ar);
	void CmdMenuNewGame(const TCHAR* Args, FOutputDevice& Ar);
	void CmdHideHUD(const TCHAR* Args, FOutputDevice& Ar);
	void CmdCamZoom(const TCHAR* Args, FOutputDevice& Ar);
	void CmdCamReset(const TCHAR* Args, FOutputDevice& Ar);
	void CmdDebugCam(const TCHAR* Args, FOutputDevice& Ar);
	void CmdDebugCamOff(const TCHAR* Args, FOutputDevice& Ar);

private:
	// Одна строка скрипта: сперва наши QA-команды (этот cheat-manager напрямую — работает и
	// при активной свободной камере, когда у контроллера игрока Player == null), затем
	// штатный консольный путь текущего контроллера игрока.
	void ExecuteShowcaseLine(const FString& Line);

	// Разбор одной команды QA*. true — команда наша (при неверных аргументах в лог уходит
	// подсказка, но true всё равно).
	bool HandleShowcaseCommand(const TCHAR* Cmd, FOutputDevice& Ar);

	// --- Опрос папки скриптов (-ShowcaseWatch) и исполнение скрипта ---
	bool TickShowcase(float DeltaSeconds);
	void ScanInbox();
	bool StartScript(const FString& ScriptPath, const FString& RunningPath, const FString& DonePath);
	void PumpScript();
	void FinishScript();

	// --- Вспомогательное ---
	AContrarySurvivorPlayerController* GetContraryPC() const;
	APlayerCharacter* GetPlayerChar() const;
	// Класс волка «как в игре»: сначала из живого спавнера уровня (WolfClass бродячего
	// спавнера / EnemyClass логова), потом BP_Wolf по адресу, потом C++-класс.
	UClass* ResolveWolfClass(FString& OutSource) const;
	// Пол под точкой (X,Y) рядом с Z: трасса по статике мира вниз от Z+Above до Z-Below.
	static bool FindFloorNear(const UWorld* World, const FVector& Around, float Above, float Below,
		const AActor* Ignore, float& OutFloorZ);
	// Закрыть игровые окна (инвентарь/магазин/диалог/обыск/пауза), главное меню не трогая.
	void CloseGameplayWindows(AContrarySurvivorPlayerController* PC);
	// Спрятать/показать весь игровой интерфейс (холст HUD + UMG-слои вьюпорта).
	void ApplyHudHidden(bool bHidden);
	// Активна ли свободная камера (наш DebugCameraControllerRef управляет игроком).
	class ADebugCameraController* GetActiveDebugCamera() const;

	// Папка снимков QAShot (абсолютная). По умолчанию <Project>/Saved/Showcase/out/.
	FString ShotDir;
	// Папка входящих скриптов (-ShowcaseWatch): <Project>/Saved/Showcase/inbox/.
	FString InboxDir;
	// Тикер реального времени (опрос папки + исполнение скрипта; работает и на паузе).
	FTSTicker::FDelegateHandle ShowcaseTickHandle;
	bool bWatchInbox = false;
	double LastInboxScanTime = 0.0;

	// Активный скрипт.
	TArray<FString> ScriptLines;
	int32 ScriptNextLine = 0;
	double ScriptWaitUntil = 0.0;
	FString ScriptName;
	FString ScriptRunningPath;
	FString ScriptDonePath;
	bool bScriptActive = false;

	// Волки, поставленные QASpawnWolf (слабые ссылки: их держит уровень).
	TArray<TWeakObjectPtr<AWolfCharacter>> SpawnedWolves;
	// Невидимая точка-цель для позы прицеливания QAPlayerFace.
	TWeakObjectPtr<AActor> AimMarker;
	// Сохранённые значения камеры для QACamReset (валидны, пока bCamSaved).
	bool bCamSaved = false;
	float SavedArmLength = 0.0f;
	FRotator SavedArmRotation = FRotator::ZeroRotator;
	// Спрятан ли интерфейс командой QAHideHUD.
	bool bHudHidden = false;
#endif // CONTRARY_WITH_QA_CHEATS
};
