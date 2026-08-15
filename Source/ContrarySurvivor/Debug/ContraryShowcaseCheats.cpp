// Служебные консольные команды для съёмки витринных кадров магазина (2026-08-15).
// Реализация методов UContraryCheatManager (объявление — Debug/ContraryDebugCamera.h).
//
// ЗАЧЕМ. game-lead снимает шесть кадров для магазина в отдельном процессе игры
// (`UnrealEditor.exe <uproject> /Game/Maps/L_World_C -game -windowed …`) и управляет игрой
// текстовыми скриптами, которые игра сама подхватывает из папки Saved/Showcase/inbox/
// (ключ запуска -ShowcaseWatch). Слово Рината: «сделай команды на их расставление по уровню
// (кадру). Они (волки) будут не двигаться».
//
// РАМКИ. Всё под CONTRARY_WITH_QA_CHEATS — в публикационной сборке файл пуст. Геймплей,
// баланс, сейвы и интерфейс не меняются: команды зовут те же обработчики, что клавиши и
// кнопки (дружба с контроллером/волком даёт доступ к защищённым членам), либо только читают
// состояние. Клавиш не добавляется.
//
// СПИСОК КОМАНД — `QAHelp` (таблица GShowcaseCommands ниже, одна строка на команду).

#include "ContraryDebugCamera.h"

#if CONTRARY_WITH_QA_CHEATS

#include "ContrarySurvivor/ContrarySurvivor.h"                    // LogQA
#include "ContrarySurvivor/Debug/QADebug.h"                       // FQADebug::QA / флаги
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Actors/ShopVendor.h"                   // IShopVendor / UShopVendor
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"
#include "AArmorTiers.h"                                          // запасная броня Т1/Т2 в рюкзак
#include "AConsumableItem.h"                                      // EConsumableType
#include "APistol.h"
#include "AMeleeWeapon.h"
#include "ARangedWeapon.h"
#include "AMasterInventoryItem.h"
#include "UInventoryComponent.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"          // OnScreenshotCaptured, GetGameViewportWidget
#include "Engine/DebugCameraController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TargetPoint.h"                 // невидимая точка-цель для позы прицеливания
#include "EngineUtils.h"                        // TActorIterator
#include "GameFramework/HUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"                        // FScreenshotRequest
#include "Framework/Application/SlateApplication.h" // снимок только вьюпорта
#include "Widgets/SViewport.h"
#include "ImageUtils.h"                          // запись PNG
#include "UObject/UObjectGlobals.h"              // StaticLoadClass
#include "UObject/UnrealType.h"                  // FClassProperty (класс волка из спавнера)
#include "InputActionValue.h"                    // FInputActionValue() для Fire

namespace
{
	// Как часто крутится тикер скриптов (реальное время, работает и на паузе).
	constexpr float ShowcaseTickInterval = 0.1f;
	// Как часто заглядываем в папку входящих скриптов.
	constexpr double InboxScanInterval = 0.5;
	// Насколько выше/ниже заданной точки искать пол при телепорте/спавне (см).
	constexpr float FloorSearchAbove = 300.0f;
	constexpr float FloorSearchBelow = 600.0f;
	// Дальность невидимой точки-цели для позы прицеливания (см).
	constexpr float AimMarkerDistance = 600.0f;
	// Сколько строк максимум печатает QADumpActors (защита от заливки журнала).
	constexpr int32 DumpActorsMaxLines = 400;

	// Единый вывод: LogQA (с немедленной записью в файл) + консоль вызвавшего.
	void Say(FOutputDevice& Ar, const FString& Msg)
	{
		FQADebug::QA(nullptr, Msg, /*bScreen=*/false);
		if (&Ar != GLog)
		{
			Ar.Logf(TEXT("%s"), *Msg);
		}
	}

	// Разбить строку аргументов на токены (кавычки уважаются).
	TArray<FString> SplitArgs(const TCHAR* Args)
	{
		TArray<FString> Out;
		FString Token;
		while (FParse::Token(Args, Token, /*UseEscape=*/false))
		{
			Out.Add(Token);
			Token.Reset();
		}
		return Out;
	}

	// float для углов/долей, double для координат (FVector в UE5 — double).
	template <typename T>
	bool ParseFloatArg(const TArray<FString>& Args, int32 Index, T& Out)
	{
		if (!Args.IsValidIndex(Index) || !Args[Index].IsNumeric())
		{
			return false;
		}
		Out = static_cast<T>(FCString::Atod(*Args[Index]));
		return true;
	}

	bool ParseBoolArg(const TArray<FString>& Args, int32 Index, bool& Out)
	{
		if (!Args.IsValidIndex(Index))
		{
			return false;
		}
		const FString& S = Args[Index];
		if (S == TEXT("1") || S.Equals(TEXT("on"), ESearchCase::IgnoreCase) || S.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			Out = true;
			return true;
		}
		if (S == TEXT("0") || S.Equals(TEXT("off"), ESearchCase::IgnoreCase) || S.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			Out = false;
			return true;
		}
		return false;
	}

	FString VecStr(const FVector& V)
	{
		return FString::Printf(TEXT("%.1f %.1f %.1f"), V.X, V.Y, V.Z);
	}

	// Предмет рюкзака — данные, не объект сцены (тот же приём, что у покупки и лута).
	AMasterInventoryItem* SpawnHiddenItem(UWorld* World, TSubclassOf<AMasterInventoryItem> Cls, AActor* Owner)
	{
		if (!World || !Cls || !Owner)
		{
			return nullptr;
		}
		FActorSpawnParameters Sp;
		Sp.Owner = Owner;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMasterInventoryItem* Item = World->SpawnActor<AMasterInventoryItem>(Cls, Owner->GetActorLocation(), Owner->GetActorRotation(), Sp);
		if (Item)
		{
			Item->SetActorHiddenInGame(true);
			Item->SetActorEnableCollision(false);
		}
		return Item;
	}

	// Таблица команд: имя (без учёта регистра), обработчик, строка подсказки для QAHelp.
	struct FShowcaseCommand
	{
		const TCHAR* Name;
		void (UContraryCheatManager::*Handler)(const TCHAR* Args, FOutputDevice& Ar);
		const TCHAR* Help;
	};
}

// ---------------------------------------------------------------------------
// Таблица команд. Имена — ровно те, что в постановке game-lead (2026-08-15).
// ---------------------------------------------------------------------------
static const FShowcaseCommand GShowcaseCommands[] =
{
	{ TEXT("QAHelp"),             &UContraryCheatManager::CmdHelp,             TEXT("QAHelp — этот список.") },
	{ TEXT("QARunScript"),        &UContraryCheatManager::CmdRunScript,        TEXT("QARunScript <имя> — выполнить <Project>/Saved/Showcase/<имя>.txt (строки: команда | wait <сек> | # комментарий).") },
	{ TEXT("QAShot"),             &UContraryCheatManager::CmdShot,             TEXT("QAShot [имя] — снимок игровой области с интерфейсом в <ShotDir>/<имя>.png (по умолчанию shot_<дата>).") },
	{ TEXT("QAShotDir"),          &UContraryCheatManager::CmdShotDir,          TEXT("QAShotDir <путь> — сменить папку снимков (по умолчанию <Project>/Saved/Showcase/out/).") },
	{ TEXT("QATeleport"),         &UContraryCheatManager::CmdTeleport,         TEXT("QATeleport X Y Z [Yaw] [nosnap] — телепорт игрока (Z подгоняется к полу рядом, nosnap — как задано).") },
	{ TEXT("QAWhere"),            &UContraryCheatManager::CmdWhere,            TEXT("QAWhere — позиция/поворот игрока и позиция/поворот/FOV камеры в лог.") },
	{ TEXT("QADumpActors"),       &UContraryCheatManager::CmdDumpActors,       TEXT("QADumpActors <подстрока> — акторы, у которых класс/имя/метка содержит подстроку: класс | имя | метка | X Y Z | Yaw.") },
	{ TEXT("QASpawnWolf"),        &UContraryCheatManager::CmdSpawnWolf,        TEXT("QASpawnWolf X Y Z Yaw [idle|run|attack|dead] [фаза 0..1] [nosnap] — обездвиженный волк в позе (мозгов нет).") },
	{ TEXT("QAClearWolves"),      &UContraryCheatManager::CmdClearWolves,      TEXT("QAClearWolves — убрать всех волков, поставленных QASpawnWolf.") },
	{ TEXT("QAFreezeEnemies"),    &UContraryCheatManager::CmdFreezeEnemies,    TEXT("QAFreezeEnemies 0|1 — явная версия клавиши U (заморозка всех врагов).") },
	{ TEXT("QAGod"),              &UContraryCheatManager::CmdGod,              TEXT("QAGod 0|1 — явная версия клавиши T (неуязвимость + стоп голода/жажды).") },
	{ TEXT("QAPlayerFace"),       &UContraryCheatManager::CmdPlayerFace,       TEXT("QAPlayerFace Yaw — развернуть игрока по азимуту и держать позу прицеливания (если в руках огнестрел).") },
	{ TEXT("QAFire"),             &UContraryCheatManager::CmdFire,             TEXT("QAFire — один выстрел/удар текущим оружием тем же путём, что ЛКМ/тап.") },
	{ TEXT("QAEquipPistol"),      &UContraryCheatManager::CmdEquipPistol,      TEXT("QAEquipPistol — выдать пистолет в слот огнестрела (если пуст) и взять в руки.") },
	{ TEXT("QAGiveShowcaseLoot"), &UContraryCheatManager::CmdGiveShowcaseLoot, TEXT("QAGiveShowcaseLoot — надеть полный комплект брони Т3 и набить рюкзак (еда/вода/бинты/патроны/шкуры/оружие/запасная броня).") },
	{ TEXT("QAGiveMoney"),        &UContraryCheatManager::CmdGiveMoney,        TEXT("QAGiveMoney [N] — выдать N денег (по умолчанию 500).") },
	{ TEXT("QAOpenInventory"),    &UContraryCheatManager::CmdOpenInventory,    TEXT("QAOpenInventory — открыть окно инвентаря (другие игровые окна закрываются).") },
	{ TEXT("QAOpenShop"),         &UContraryCheatManager::CmdOpenShop,         TEXT("QAOpenShop — открыть окно ближайшего торговца (дистанция не проверяется).") },
	{ TEXT("QACloseUI"),          &UContraryCheatManager::CmdCloseUI,          TEXT("QACloseUI — закрыть инвентарь/магазин/диалог/обыск/паузу (главное меню не трогает).") },
	{ TEXT("QAFullStats"),        &UContraryCheatManager::CmdFullStats,        TEXT("QAFullStats — явная версия клавиши Y (здоровье/голод/жажда на максимум).") },
	{ TEXT("QAEndIntro"),         &UContraryCheatManager::CmdEndIntro,         TEXT("QAEndIntro — завершить интро немедленно (управление игроку, задача «найти старосту»).") },
	{ TEXT("QAMenuContinue"),     &UContraryCheatManager::CmdMenuContinue,     TEXT("QAMenuContinue — нажать «Продолжить» в главном меню (если оно открыто).") },
	{ TEXT("QAMenuNewGame"),      &UContraryCheatManager::CmdMenuNewGame,      TEXT("QAMenuNewGame — нажать «Новая игра» в главном меню (если оно открыто).") },
	{ TEXT("QAHideHUD"),          &UContraryCheatManager::CmdHideHUD,          TEXT("QAHideHUD 0|1 — спрятать/показать весь игровой интерфейс (холст HUD + UMG-окна).") },
	{ TEXT("QACamZoom"),          &UContraryCheatManager::CmdCamZoom,          TEXT("QACamZoom <длина_штанги> [pitch] — дистанция (и наклон) камеры игрока; QACamReset вернёт.") },
	{ TEXT("QACamReset"),         &UContraryCheatManager::CmdCamReset,         TEXT("QACamReset — вернуть камеру игрока к значениям до QACamZoom.") },
	{ TEXT("QADebugCam"),         &UContraryCheatManager::CmdDebugCam,         TEXT("QADebugCam X Y Z Pitch Yaw [FOV] — включить свободную камеру (F1) и поставить её сюда.") },
	{ TEXT("QADebugCamOff"),      &UContraryCheatManager::CmdDebugCamOff,      TEXT("QADebugCamOff — выйти из свободной камеры к игроку.") },
};

// ---------------------------------------------------------------------------
// Точки входа движка
// ---------------------------------------------------------------------------

void UContraryCheatManager::InitCheatManager()
{
	Super::InitCheatManager();

	const FString ShowcaseRoot = FPaths::ProjectSavedDir() / TEXT("Showcase");
	ShotDir  = FPaths::ConvertRelativePathToFull(ShowcaseRoot / TEXT("out"));
	InboxDir = FPaths::ConvertRelativePathToFull(ShowcaseRoot / TEXT("inbox"));

	// Автозапуск опроса папки скриптов — только по ключу командной строки -ShowcaseWatch.
	bWatchInbox = FParse::Param(FCommandLine::Get(), TEXT("ShowcaseWatch"));
	if (bWatchInbox)
	{
		IFileManager::Get().MakeDirectory(*InboxDir, /*Tree=*/true);
		IFileManager::Get().MakeDirectory(*ShotDir, /*Tree=*/true);
	}

	// Тикер ядра, а не таймер мира: реальное время, крутится и на паузе (главное меню, пауза).
	TWeakObjectPtr<UContraryCheatManager> WeakThis(this);
	ShowcaseTickHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("ContraryShowcaseCheats"), ShowcaseTickInterval,
		[WeakThis](float DeltaSeconds) -> bool
		{
			UContraryCheatManager* Self = WeakThis.Get();
			return Self ? Self->TickShowcase(DeltaSeconds) : false;
		});

	FQADebug::QA(this, FString::Printf(TEXT("QA: SHOWCASE cheats ready (%d commands, QAHelp for list); watch=%s inbox=%s out=%s"),
		(int32)UE_ARRAY_COUNT(GShowcaseCommands), bWatchInbox ? TEXT("on") : TEXT("off"), *InboxDir, *ShotDir));
}

void UContraryCheatManager::BeginDestroy()
{
	if (ShowcaseTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ShowcaseTickHandle);
		ShowcaseTickHandle.Reset();
	}
	Super::BeginDestroy();
}

bool UContraryCheatManager::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	if (HandleShowcaseCommand(Cmd, Ar))
	{
		return true;
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);
}

bool UContraryCheatManager::HandleShowcaseCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (!Cmd)
	{
		return false;
	}
	// FParse::Command: без учёта регистра, только целое слово (QAShot не сматчится на QAShotDir).
	for (const FShowcaseCommand& Entry : GShowcaseCommands)
	{
		const TCHAR* Args = Cmd;
		if (FParse::Command(&Args, Entry.Name))
		{
			(this->*Entry.Handler)(Args, Ar);
			return true;
		}
	}
	return false;
}

void UContraryCheatManager::ExecuteShowcaseLine(const FString& Line)
{
	// 1) Наши команды и штатные exec-функции UCheatManager — напрямую в этот объект. Так они
	//    работают и при активной свободной камере: движок тогда обнуляет Player у контроллера
	//    игрока (UPlayer::SwitchController), и его ConsoleCommand молча ничего не делает.
	if (ProcessConsoleExec(*Line, *GLog, GetOuterAPlayerController()))
	{
		return;
	}
	// 2) Всё остальное (консольные переменные, stat fps, ShowFlag.* …) — штатный консольный
	//    путь текущего контроллера игрока (при свободной камере это она; её ConsoleCommand
	//    при промахе сам пробует исходный контроллер — DebugCameraController.cpp:294).
	UWorld* World = GetWorld();
	ULocalPlayer* LP = (GEngine && World) ? GEngine->GetFirstGamePlayer(World) : nullptr;
	APlayerController* Target = (LP && LP->PlayerController) ? ToRawPtr(LP->PlayerController) : GetOuterAPlayerController();
	if (Target)
	{
		Target->ConsoleCommand(Line, /*bWriteToLog=*/true);
	}
	else if (GEngine)
	{
		GEngine->Exec(World, *Line, *GLog);
	}
}

// ---------------------------------------------------------------------------
// Опрос папки скриптов и исполнение скрипта
// ---------------------------------------------------------------------------

bool UContraryCheatManager::TickShowcase(float /*DeltaSeconds*/)
{
	if (bScriptActive)
	{
		PumpScript();
	}
	else if (bWatchInbox)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastInboxScanTime >= InboxScanInterval)
		{
			LastInboxScanTime = Now;
			ScanInbox();
		}
	}
	return true; // тикер живёт, пока жив cheat-manager (снимается в BeginDestroy)
}

void UContraryCheatManager::ScanInbox()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(InboxDir / TEXT("*.txt")), /*Files=*/true, /*Directories=*/false);
	if (Files.Num() == 0)
	{
		return;
	}
	Files.Sort(); // первый по имени

	const FString Name = FPaths::GetBaseFilename(Files[0]);
	const FString TxtPath     = InboxDir / Files[0];
	const FString RunningPath = InboxDir / (Name + TEXT(".running"));
	const FString DonePath    = InboxDir / (Name + TEXT(".done"));

	// Переименование — атомарная «заявка»: файл, который ещё дописывают, обычно не даёт
	// себя переименовать; тогда просто подождём следующего опроса.
	if (!IFileManager::Get().Move(*RunningPath, *TxtPath, /*Replace=*/true))
	{
		FQADebug::QA(this, FString::Printf(TEXT("QA: SCRIPT cannot claim %s (still being written?) - retry later"), *TxtPath));
		return;
	}
	if (!StartScript(RunningPath, RunningPath, DonePath))
	{
		IFileManager::Get().Move(*DonePath, *RunningPath, /*Replace=*/true);
	}
}

bool UContraryCheatManager::StartScript(const FString& ScriptPath, const FString& RunningPath, const FString& DonePath)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *ScriptPath))
	{
		FQADebug::QA(this, FString::Printf(TEXT("QA: SCRIPT FAILED to read %s"), *ScriptPath));
		return false;
	}
	ScriptLines = MoveTemp(Lines);
	ScriptNextLine = 0;
	ScriptWaitUntil = 0.0;
	ScriptName = FPaths::GetCleanFilename(ScriptPath);
	ScriptRunningPath = RunningPath;
	ScriptDonePath = DonePath;
	bScriptActive = true;
	FQADebug::QA(this, FString::Printf(TEXT("QA: SCRIPT start %s (%d lines)"), *ScriptName, ScriptLines.Num()));
	// Первые строки — сразу, не дожидаясь тика.
	PumpScript();
	return true;
}

void UContraryCheatManager::PumpScript()
{
	// Слабая ссылка на себя: строка скрипта может уронить мир (open <карта>), и тогда этот
	// объект уничтожается прямо внутри ExecuteShowcaseLine.
	TWeakObjectPtr<UContraryCheatManager> WeakThis(this);
	while (WeakThis.IsValid() && bScriptActive && ScriptNextLine < ScriptLines.Num())
	{
		const double Now = FPlatformTime::Seconds();
		if (Now < ScriptWaitUntil)
		{
			return; // идёт пауза wait
		}

		const int32 LineNo = ScriptNextLine + 1;
		const FString Line = ScriptLines[ScriptNextLine++].TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		FQADebug::QA(this, FString::Printf(TEXT("QA: SCRIPT line %d: %s"), LineNo, *Line));

		const TCHAR* Cursor = *Line;
		if (FParse::Command(&Cursor, TEXT("wait")))
		{
			const float Seconds = FMath::Max(0.0f, FCString::Atof(Cursor));
			ScriptWaitUntil = Now + Seconds;
			continue;
		}

		ExecuteShowcaseLine(Line);
	}

	if (WeakThis.IsValid() && bScriptActive && ScriptNextLine >= ScriptLines.Num())
	{
		FinishScript();
	}
}

void UContraryCheatManager::FinishScript()
{
	bScriptActive = false;
	if (!ScriptRunningPath.IsEmpty() && !ScriptDonePath.IsEmpty() && ScriptRunningPath != ScriptDonePath)
	{
		IFileManager::Get().Move(*ScriptDonePath, *ScriptRunningPath, /*Replace=*/true);
	}
	FQADebug::QA(this, FString::Printf(TEXT("QA: SCRIPT done %s"), *ScriptName));
	ScriptLines.Reset();
	ScriptName.Reset();
	ScriptRunningPath.Reset();
	ScriptDonePath.Reset();
}

// ---------------------------------------------------------------------------
// Снимок игровой области (общий хелпер клавиши G и команды QAShot)
// ---------------------------------------------------------------------------

void UContraryCheatManager::TakeViewportShot(UWorld* World, const FString& AbsPngPath, const TCHAR* LogTag)
{
	// Почему не просто RequestScreenshot(bShowUI=true): движок при bShowUI снимает ВСЁ ОКНО
	// (GameViewportClient.cpp:2160 — FSlateApplication::TakeScreenshot(WindowRef)), и в PIE
	// это окно редактора целиком (жалоба Рината 15.08). Поэтому: запрос оставляем (он даёт
	// правильный момент — конец кадра, после отрисовки), но подписываемся одноразово на
	// UGameViewportClient::OnScreenshotCaptured — при подписчике движок НЕ пишет файл сам,
	// а отдаёт картинку окна нам (GameViewportClient.cpp:2180, r.ScreenshotDelegate=1 по
	// умолчанию). Мы её игнорируем и в тот же момент снимаем ТОЛЬКО виджет вьюпорта
	// (перегрузка TakeScreenshot(Widget): SlateApplication.cpp:4217 сама считает прямоугольник
	// виджета внутри окна) — в него входят и мир, и UMG-интерфейс. Пишем PNG сами
	// (FImageUtils::SaveImageByExtension). Если виджета нет (не должно) — падаем на кадр окна.
	const FString Tag = LogTag ? FString(LogTag) : FString(TEXT("SHOT"));

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsPngPath), /*Tree=*/true);

	UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
	if (!ViewportClient)
	{
		FQADebug::QA(World, FString::Printf(TEXT("QA: %s FAILED %s (no game viewport)"), *Tag, *AbsPngPath), /*bScreen=*/true);
		return;
	}
	TWeakObjectPtr<UGameViewportClient> WeakViewport(ViewportClient);

	// Одноразовая подписка (снимает себя сама). Путь захвачен копией: к моменту вызова
	// FScreenshotRequest::Reset() уже стёр имя из запроса. Строка «saved» уходит в оверлей
	// уже СЛЕДУЮЩЕГО кадра и в снимок не попадает.
	//
	// ⚠ Remove() из тела лямбды УНИЧТОЖАЕТ саму лямбду вместе с захватами прямо во время
	// вызова (MulticastDelegateBase.h:309 → DelegateBase.Unbind() → ~IDelegateInstance):
	// после него захваты — висячие ссылки. Баг 15.08: путь превращался в мусор и снимок
	// ложился как «<мусор>.png» в рабочую папку движка (Engine/Binaries/Win64). Поэтому
	// СНАЧАЛА копируем захваты в локальные переменные, ПОТОМ отписываемся и дальше работаем
	// только с локальными копиями.
	TSharedRef<FDelegateHandle> HandleRef = MakeShared<FDelegateHandle>();
	*HandleRef = UGameViewportClient::OnScreenshotCaptured().AddLambda(
		[AbsPngPath, Tag, HandleRef, WeakViewport](int32 WindowW, int32 WindowH, const TArray<FColor>& WindowColors)
	{
		const FString LocalFilePath = AbsPngPath;
		const FString LocalTag = Tag;
		const TWeakObjectPtr<UGameViewportClient> LocalWeakViewport = WeakViewport;
		const FDelegateHandle LocalHandle = *HandleRef;
		UGameViewportClient::OnScreenshotCaptured().Remove(LocalHandle);
		// Ниже захваты лямбды трогать НЕЛЬЗЯ — они уже уничтожены.

		TArray<FColor> Pixels;
		FIntVector Size(0, 0, 0);
		bool bViewportOnly = false;
		TSharedPtr<SViewport> ViewportWidget = LocalWeakViewport.IsValid() ? LocalWeakViewport->GetGameViewportWidget() : nullptr;
		if (ViewportWidget.IsValid() && FSlateApplication::IsInitialized())
		{
			bViewportOnly = FSlateApplication::Get().TakeScreenshot(ViewportWidget.ToSharedRef(), Pixels, Size);
		}
		if (!bViewportOnly)
		{
			// Запасной путь — кадр всего окна, как отдал движок.
			Pixels = WindowColors;
			Size = FIntVector(WindowW, WindowH, 0);
		}
		for (FColor& Px : Pixels)
		{
			Px.A = 255; // без прозрачности (движок делает так же перед записью)
		}

		const bool bSaved = Size.X > 0 && Size.Y > 0 && Pixels.Num() >= Size.X * Size.Y
			&& FImageUtils::SaveImageByExtension(*LocalFilePath, FImageView(Pixels.GetData(), Size.X, Size.Y));
		FQADebug::QA(nullptr, FString::Printf(TEXT("QA: %s %s %s (%dx%d, %s)"),
			*LocalTag, bSaved ? TEXT("saved") : TEXT("FAILED"), *LocalFilePath, Size.X, Size.Y,
			bViewportOnly ? TEXT("viewport only") : TEXT("whole window fallback")), /*bScreen=*/true);
	});

	FScreenshotRequest::RequestScreenshot(AbsPngPath, /*bInShowUI=*/true, /*bAddFilenameSuffix=*/false);
	FQADebug::QA(World, FString::Printf(TEXT("QA: %s requested -> %s"), *Tag, *AbsPngPath));
}

// ---------------------------------------------------------------------------
// Вспомогательное
// ---------------------------------------------------------------------------

AContrarySurvivorPlayerController* UContraryCheatManager::GetContraryPC() const
{
	return Cast<AContrarySurvivorPlayerController>(GetOuterAPlayerController());
}

APlayerCharacter* UContraryCheatManager::GetPlayerChar() const
{
	APlayerController* PC = GetOuterAPlayerController();
	return PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
}

ADebugCameraController* UContraryCheatManager::GetActiveDebugCamera() const
{
	// Свободная камера активна, когда наш контроллер отдал ей Player (UPlayer::SwitchController
	// обнуляет Player у прежнего контроллера) и она помнит его как исходный.
	APlayerController* PC = GetOuterAPlayerController();
	ADebugCameraController* DCC = DebugCameraControllerRef;
	if (PC && DCC && PC->Player == nullptr && DCC->OriginalControllerRef == PC && DCC->Player != nullptr)
	{
		return DCC;
	}
	return nullptr;
}

bool UContraryCheatManager::FindFloorNear(const UWorld* World, const FVector& Around, float Above, float Below,
	const AActor* Ignore, float& OutFloorZ)
{
	if (!World)
	{
		return false;
	}
	const FVector Start(Around.X, Around.Y, Around.Z + Above);
	const FVector End(Around.X, Around.Y, Around.Z - Below);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShowcaseFloorTrace), /*bTraceComplex=*/false);
	if (Ignore)
	{
		Params.AddIgnoredActor(Ignore);
	}
	// Только геометрия мира (пол/дома/дороги); пешки и предметы в трассу не входят — как в
	// SpawnPlacement::TraceFloorZ (Subsystems/SpawnPlacementUtils.h).
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, Start, End, ObjParams, Params);
	for (const FHitResult& H : Hits)
	{
		if (!H.bStartPenetrating)
		{
			OutFloorZ = H.ImpactPoint.Z;
			return true;
		}
	}
	return false;
}

UClass* UContraryCheatManager::ResolveWolfClass(FString& OutSource) const
{
	UWorld* World = GetWorld();
	// 1) Живые спавнеры уровня: у бродячего спавнера поле WolfClass, у логова — EnemyClass.
	//    Читаем через отражение по имени поля: так cheat-код не лезет в защищённые члены
	//    геймплейных классов и не тянет их заголовки.
	if (World)
	{
		static const FName WolfClassFields[] = { TEXT("WolfClass"), TEXT("EnemyClass") };
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			for (const FName& FieldName : WolfClassFields)
			{
				const FClassProperty* Prop = FindFProperty<FClassProperty>(Actor->GetClass(), FieldName);
				if (!Prop)
				{
					continue;
				}
				UClass* Cls = Cast<UClass>(Prop->GetPropertyValue_InContainer(Actor));
				if (Cls && Cls->IsChildOf(AWolfCharacter::StaticClass()))
				{
					OutSource = FString::Printf(TEXT("%s.%s"), *Actor->GetName(), *FieldName.ToString());
					return Cls;
				}
			}
		}
	}
	// 2) BP_Wolf по адресу (Content/Characters/Wolf/BP_Wolf.uasset — тот класс, что стоит в
	//    логове и бродячем спавнере на карте).
	if (UClass* Loaded = StaticLoadClass(AWolfCharacter::StaticClass(), nullptr, TEXT("/Game/Characters/Wolf/BP_Wolf.BP_Wolf_C")))
	{
		OutSource = TEXT("/Game/Characters/Wolf/BP_Wolf");
		return Loaded;
	}
	// 3) Чистый C++-класс (меш и клипы он грузит сам в конструкторе).
	OutSource = TEXT("AWolfCharacter (C++)");
	return AWolfCharacter::StaticClass();
}

void UContraryCheatManager::CloseGameplayWindows(AContrarySurvivorPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	// Те же Close*, что зовут клавиши; каждый сам молчит, если окно не открыто.
	PC->CloseShop();
	PC->CloseDialog();
	PC->CloseCorpseLoot();
	PC->ClosePauseMenu();
	if (PC->bInventoryOpen)
	{
		PC->OnToggleInventory(); // тумблер: открыто -> закрыто, с возвратом режима ввода
	}
}

void UContraryCheatManager::ApplyHudHidden(bool bHidden)
{
	APlayerController* PC = GetOuterAPlayerController();
	// 1) Холст HUD (полосы здоровья над врагами, маркеры, цифры урона, Canvas-окна):
	//    AHUD::PostRender зовёт DrawHUD только при bShowHUD (HUD.cpp:173).
	if (AHUD* Hud = PC ? PC->GetHUD() : nullptr)
	{
		Hud->bShowHUD = !bHidden;
	}
	// 2) UMG-окна и панели, добавленные AddToViewport, живут в слоте вьюпорта
	//    SGameLayerManager (SGameLayerManager.cpp:130, ViewportSlotContainer); окна для
	//    конкретного игрока (AddToPlayerScreen) — в PlayerCanvas. Обе области движок умеет
	//    сворачивать консольными переменными Slate.GameLayer.* (SGameLayerManager.cpp:60-77,
	//    есть во всех сборках, кроме Shipping/Test). Сам мир рисует SViewport снаружи слоя —
	//    он остаётся.
	static const TCHAR* CVarNames[] = { TEXT("Slate.GameLayer.ViewportSlotVisible"), TEXT("Slate.GameLayer.PlayerCanvasVisible") };
	int32 Applied = 0;
	for (const TCHAR* Name : CVarNames)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(!bHidden, ECVF_SetByConsole);
			++Applied;
		}
	}
	bHudHidden = bHidden;
	FQADebug::QA(this, FString::Printf(TEXT("QA: HUD %s (canvas=%s, slate layers=%d/2)"),
		bHidden ? TEXT("hidden") : TEXT("shown"), (PC && PC->GetHUD()) ? TEXT("ok") : TEXT("no hud"), Applied));
}

// ---------------------------------------------------------------------------
// Команды
// ---------------------------------------------------------------------------

void UContraryCheatManager::CmdHelp(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	Say(Ar, TEXT("QA: SHOWCASE commands:"));
	for (const FShowcaseCommand& Entry : GShowcaseCommands)
	{
		Say(Ar, FString::Printf(TEXT("QA:   %s"), Entry.Help));
	}
	Say(Ar, FString::Printf(TEXT("QA:   (scripts: %s, shots: %s, watch=%s)"), *InboxDir, *ShotDir, bWatchInbox ? TEXT("on") : TEXT("off")));
}

void UContraryCheatManager::CmdRunScript(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	if (A.Num() < 1)
	{
		Say(Ar, TEXT("QA: usage: QARunScript <имя>  (файл <Project>/Saved/Showcase/<имя>.txt)"));
		return;
	}
	if (bScriptActive)
	{
		Say(Ar, FString::Printf(TEXT("QA: SCRIPT busy (%s running) - QARunScript ignored"), *ScriptName));
		return;
	}
	FString Name = A[0];
	if (!Name.EndsWith(TEXT(".txt")))
	{
		Name += TEXT(".txt");
	}
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Showcase") / Name);
	if (!IFileManager::Get().FileExists(*Path))
	{
		Say(Ar, FString::Printf(TEXT("QA: SCRIPT not found: %s"), *Path));
		return;
	}
	// Ручной запуск: файл не переименовываем (Running == Done == сам файл).
	StartScript(Path, Path, Path);
}

void UContraryCheatManager::CmdShot(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	FString Name = A.Num() > 0 ? A[0] : FString::Printf(TEXT("shot_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	if (!Name.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
	{
		Name += TEXT(".png");
	}
	const FString Path = FPaths::IsRelative(Name) ? (ShotDir / Name) : Name;
	Say(Ar, FString::Printf(TEXT("QA: SHOT -> %s"), *Path));
	TakeViewportShot(GetWorld(), Path, TEXT("SHOT"));
}

void UContraryCheatManager::CmdShotDir(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	if (A.Num() < 1)
	{
		Say(Ar, FString::Printf(TEXT("QA: SHOTDIR is %s (usage: QAShotDir <путь>)"), *ShotDir));
		return;
	}
	ShotDir = FPaths::ConvertRelativePathToFull(A[0]);
	IFileManager::Get().MakeDirectory(*ShotDir, /*Tree=*/true);
	Say(Ar, FString::Printf(TEXT("QA: SHOTDIR = %s (%s)"), *ShotDir,
		IFileManager::Get().DirectoryExists(*ShotDir) ? TEXT("ok") : TEXT("cannot create!")));
}

void UContraryCheatManager::CmdTeleport(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	FVector Loc;
	if (!ParseFloatArg(A, 0, Loc.X) || !ParseFloatArg(A, 1, Loc.Y) || !ParseFloatArg(A, 2, Loc.Z))
	{
		Say(Ar, TEXT("QA: usage: QATeleport X Y Z [Yaw] [nosnap]"));
		return;
	}
	ACharacter* Pawn = Cast<ACharacter>(GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr);
	if (!Pawn)
	{
		Say(Ar, TEXT("QA: TELEPORT skipped - no player character"));
		return;
	}
	float Yaw = Pawn->GetActorRotation().Yaw;
	ParseFloatArg(A, 3, Yaw);
	const bool bSnap = !A.ContainsByPredicate([](const FString& S) { return S.Equals(TEXT("nosnap"), ESearchCase::IgnoreCase); });

	const FVector Requested = Loc;
	if (bSnap)
	{
		float FloorZ = 0.0f;
		if (FindFloorNear(GetWorld(), Loc, FloorSearchAbove, FloorSearchBelow, Pawn, FloorZ))
		{
			const float HalfHeight = Pawn->GetCapsuleComponent() ? Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
			Loc.Z = FloorZ + HalfHeight + 2.0f;
		}
	}

	if (UCharacterMovementComponent* Move = Pawn->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	const bool bOk = Pawn->SetActorLocationAndRotation(Loc, FRotator(0.0f, Yaw, 0.0f), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		PC->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
	Say(Ar, FString::Printf(TEXT("QA: TELEPORT %s -> %s yaw %.1f (requested %s, %s)"),
		bOk ? TEXT("ok") : TEXT("FAILED"), *VecStr(Pawn->GetActorLocation()), Pawn->GetActorRotation().Yaw,
		*VecStr(Requested), bSnap ? TEXT("snapped to floor") : TEXT("as given")));
}

void UContraryCheatManager::CmdWhere(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		return;
	}
	if (APawn* Pawn = PC->GetPawn())
	{
		Say(Ar, FString::Printf(TEXT("QA: WHERE player %s rot %s (yaw %.1f)"),
			*VecStr(Pawn->GetActorLocation()), *Pawn->GetActorRotation().ToCompactString(), Pawn->GetActorRotation().Yaw));
	}
	else
	{
		Say(Ar, TEXT("QA: WHERE player: no pawn"));
	}
	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const float Fov = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 0.0f;
	Say(Ar, FString::Printf(TEXT("QA: WHERE camera %s rot %s fov %.1f"), *VecStr(CamLoc), *CamRot.ToCompactString(), Fov));
	if (ADebugCameraController* DCC = GetActiveDebugCamera())
	{
		DCC->GetPlayerViewPoint(CamLoc, CamRot);
		const float DFov = DCC->PlayerCameraManager ? DCC->PlayerCameraManager->GetFOVAngle() : 0.0f;
		Say(Ar, FString::Printf(TEXT("QA: WHERE debugcam %s rot %s fov %.1f (ACTIVE)"), *VecStr(CamLoc), *CamRot.ToCompactString(), DFov));
	}
}

void UContraryCheatManager::CmdDumpActors(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	if (A.Num() < 1 || A[0].IsEmpty())
	{
		Say(Ar, TEXT("QA: usage: QADumpActors <подстрока класса/имени/метки>"));
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FString& Needle = A[0];
	int32 Matched = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		const FString ClassName = Actor->GetClass()->GetName();
		const FString ActorName = Actor->GetName();
		FString Label;
#if WITH_EDITOR
		Label = Actor->GetActorLabel(/*bCreateIfNone=*/false); // в -game из редакторной сборки метки уровня доступны
#endif
		if (!ClassName.Contains(Needle) && !ActorName.Contains(Needle) && !Label.Contains(Needle))
		{
			continue;
		}
		++Matched;
		if (Matched > DumpActorsMaxLines)
		{
			continue;
		}
		// Префикс «QA: DUMP» у каждой строки — по нему фильтрует журнал скрипт отправки game-lead.
		Say(Ar, FString::Printf(TEXT("QA: DUMP %s | %s | %s | %s | %.1f"),
			*ClassName, *ActorName, Label.IsEmpty() ? TEXT("-") : *Label,
			*VecStr(Actor->GetActorLocation()), Actor->GetActorRotation().Yaw));
	}
	Say(Ar, FString::Printf(TEXT("QA: DUMP '%s': %d actors total%s"), *Needle, Matched,
		Matched > DumpActorsMaxLines ? *FString::Printf(TEXT(" (printed first %d)"), DumpActorsMaxLines) : TEXT("")));
}

void UContraryCheatManager::CmdSpawnWolf(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	FVector Loc;
	float Yaw = 0.0f;
	if (!ParseFloatArg(A, 0, Loc.X) || !ParseFloatArg(A, 1, Loc.Y) || !ParseFloatArg(A, 2, Loc.Z) || !ParseFloatArg(A, 3, Yaw))
	{
		Say(Ar, TEXT("QA: usage: QASpawnWolf X Y Z Yaw [idle|run|attack|dead] [фаза 0..1] [nosnap]"));
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Поза и фаза (доля клипа, на которой замереть). Для «мёртвого» по умолчанию конец клипа —
	// волк уже лежит на боку.
	FString Pose = TEXT("idle");
	float Phase = 0.5f;
	bool bPhaseGiven = false;
	bool bSnap = true;
	for (int32 i = 4; i < A.Num(); ++i)
	{
		if (A[i].Equals(TEXT("nosnap"), ESearchCase::IgnoreCase))
		{
			bSnap = false;
		}
		else if (A[i].IsNumeric())
		{
			Phase = FMath::Clamp(FCString::Atof(*A[i]), 0.0f, 1.0f);
			bPhaseGiven = true;
		}
		else
		{
			Pose = A[i].ToLower();
		}
	}
	const bool bDead = (Pose == TEXT("dead") || Pose == TEXT("death"));
	if (bDead && !bPhaseGiven)
	{
		Phase = 1.0f;
	}

	FString ClassSource;
	UClass* WolfClass = ResolveWolfClass(ClassSource);

	// Отложенный спавн: до FinishSpawning выключаем авто-поссесс ИИ — тогда
	// APawn::PostInitializeComponents (Pawn.cpp:145) контроллер НЕ создаёт вовсе: мозгов нет,
	// в реестр AEnemyAIController::GetActiveControllers волк не попадает, ни к кому не
	// аггрится и не поворачивается. Это чище, чем спавнить с ИИ и потом его глушить.
	FTransform SpawnTM(FRotator(0.0f, Yaw, 0.0f), Loc);
	AWolfCharacter* Wolf = World->SpawnActorDeferred<AWolfCharacter>(WolfClass, SpawnTM, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Wolf)
	{
		Say(Ar, FString::Printf(TEXT("QA: WOLF spawn FAILED (class %s from %s)"), *GetNameSafe(WolfClass), *ClassSource));
		return;
	}
	Wolf->AutoPossessAI = EAutoPossessAI::Disabled;

	// Высота: капсула на пол рядом с заданной точкой (как ставит спавнер), либо ровно как задано.
	const float HalfHeight = Wolf->GetCapsuleComponent() ? Wolf->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 40.0f;
	if (bSnap)
	{
		float FloorZ = 0.0f;
		if (FindFloorNear(World, Loc, FloorSearchAbove, FloorSearchBelow, Wolf, FloorZ))
		{
			Loc.Z = FloorZ + HalfHeight + 1.0f;
			SpawnTM.SetLocation(Loc);
		}
	}
	Wolf->FinishSpawning(SpawnTM); // здесь проходят PostInitializeComponents и BeginPlay волка

	// Ноги: MOVE_None глушит и path-following, и прямой ход, и гравитацию (стоит как вкопанный).
	if (UCharacterMovementComponent* Move = Wolf->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	// Тик актора волка — только смена Idle/Run по скорости (WolfCharacter.cpp:Tick); он бы
	// перебил нашу позу — выключаем.
	Wolf->SetActorTickEnabled(false);

	// Поза: клипы волка (Single Node, без AnimBP — так устроен сам волк). Проиграть нужный клип,
	// поставить на долю Phase и остановить: у остановленного Single Node время не идёт, но поза
	// на этом времени продолжает вычисляться каждый кадр (AnimSingleNodeInstanceProxy.cpp:570:
	// «we still have to tick animation when bPlaying is false»).
	UAnimSequence* Clip = Wolf->IdleAnim;
	if (Pose == TEXT("run"))          { Clip = Wolf->RunAnim; }
	else if (Pose == TEXT("attack") || Pose == TEXT("bite")) { Clip = Wolf->BiteAnim; }
	else if (bDead)                   { Clip = Wolf->DeathAnim ? Wolf->DeathAnim : Wolf->IdleAnim; }
	else if (Pose != TEXT("idle"))    { Say(Ar, FString::Printf(TEXT("QA: WOLF unknown pose '%s' -> idle"), *Pose)); Pose = TEXT("idle"); }

	FString PoseNote;
	if (USkeletalMeshComponent* Mesh = Wolf->GetMesh())
	{
		if (Clip)
		{
			Mesh->PlayAnimation(Clip, /*bLooping=*/false);
			Mesh->SetPosition(Phase * Clip->GetPlayLength(), /*bFireNotifies=*/false);
			Mesh->Stop();
			PoseNote = FString::Printf(TEXT("%s@%.2f (%s)"), *Pose, Phase, *Clip->GetName());
		}
		else
		{
			Mesh->bPauseAnims = true; // клипа нет — хотя бы не дёргается
			PoseNote = FString::Printf(TEXT("%s (no clip, pose frozen)"), *Pose);
		}
	}

	if (bDead)
	{
		// «Мёртвый» без настоящей смерти: SetHealth(0) выставляет только признак смерти и
		// OnHealthChanged (StatsComponent.cpp:153) — OnDeath НЕ летит, значит ни лута, ни
		// зачёта в квест/счётчик убийств. По признаку смерти HUD не рисует полосу здоровья, а
		// прицел игрока его не выбирает. Капсулу гасим, как у настоящего трупа.
		if (Wolf->Stats)
		{
			Wolf->Stats->SetHealth(0.0f);
		}
		if (UCapsuleComponent* Capsule = Wolf->GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	SpawnedWolves.Add(Wolf);
	Say(Ar, FString::Printf(TEXT("QA: WOLF spawned #%d at %s yaw %.1f pose %s, class %s (from %s), controller=%s"),
		SpawnedWolves.Num(), *VecStr(Wolf->GetActorLocation()), Wolf->GetActorRotation().Yaw, *PoseNote,
		*WolfClass->GetName(), *ClassSource, Wolf->GetController() ? TEXT("YES (unexpected)") : TEXT("none")));
}

void UContraryCheatManager::CmdClearWolves(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	int32 Removed = 0;
	for (const TWeakObjectPtr<AWolfCharacter>& Weak : SpawnedWolves)
	{
		if (AWolfCharacter* Wolf = Weak.Get())
		{
			Wolf->Destroy();
			++Removed;
		}
	}
	SpawnedWolves.Reset();
	Say(Ar, FString::Printf(TEXT("QA: WOLVES cleared (%d)"), Removed));
}

void UContraryCheatManager::CmdFreezeEnemies(const TCHAR* Args, FOutputDevice& Ar)
{
	bool bOn = true;
	if (!ParseBoolArg(SplitArgs(Args), 0, bOn))
	{
		Say(Ar, TEXT("QA: usage: QAFreezeEnemies 0|1"));
		return;
	}
	if (AContrarySurvivorPlayerController* PC = GetContraryPC())
	{
		PC->ApplyQAFreezeEnemies(bOn); // сам пишет «QA: FREEZE on/off (enemies: N)»
	}
}

void UContraryCheatManager::CmdGod(const TCHAR* Args, FOutputDevice& Ar)
{
	bool bOn = true;
	if (!ParseBoolArg(SplitArgs(Args), 0, bOn))
	{
		Say(Ar, TEXT("QA: usage: QAGod 0|1"));
		return;
	}
	FQADebug::bGodMode = bOn; // тот же флаг, что клавиша T; дебаунс тумблера здесь не нужен
	Say(Ar, FString::Printf(TEXT("QA: GODMODE %s"), bOn ? TEXT("on") : TEXT("off")));
}

void UContraryCheatManager::CmdPlayerFace(const TCHAR* Args, FOutputDevice& Ar)
{
	float Yaw = 0.0f;
	if (!ParseFloatArg(SplitArgs(Args), 0, Yaw))
	{
		Say(Ar, TEXT("QA: usage: QAPlayerFace Yaw"));
		return;
	}
	APlayerCharacter* Player = GetPlayerChar();
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		Say(Ar, TEXT("QA: FACE skipped - no player character"));
		return;
	}
	const FRotator Rot(0.0f, Yaw, 0.0f);
	Player->SetActorRotation(Rot);
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		PC->SetControlRotation(Rot);
	}

	// Поза прицеливания: анимация игрока накладывает её, пока у ОРУЖИЯ есть цель
	// (AMasterHumanoidCharacter::IsAimingAtTarget -> ARangedWeapon::HasTarget). Ставим оружию
	// невидимую точку-цель по азимуту; настоящий выстрел (QAFire/ЛКМ) заменит её боевой целью.
	FString AimNote = TEXT("no aim pose (no ranged weapon in hands)");
	if (ARangedWeapon* Ranged = Cast<ARangedWeapon>(Player->GetCurrentWeapon()))
	{
		const FVector MarkerLoc = Player->GetActorLocation() + Rot.Vector() * AimMarkerDistance;
		AActor* Marker = AimMarker.Get();
		if (!Marker)
		{
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Marker = World->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), MarkerLoc, FRotator::ZeroRotator, Sp);
			if (Marker)
			{
				Marker->SetActorHiddenInGame(true);
				Marker->SetActorEnableCollision(false);
				AimMarker = Marker;
			}
		}
		if (Marker)
		{
			Marker->SetActorLocation(MarkerLoc);
			Ranged->SetTarget(Marker);
			Player->StartAimTurnTo(Marker); // тот же плавный доворот корпуса, что при выстреле
			AimNote = TEXT("aim pose held via hidden target");
		}
	}
	Say(Ar, FString::Printf(TEXT("QA: FACE yaw %.1f -> actor yaw %.1f, %s"), Yaw, Player->GetActorRotation().Yaw, *AimNote));
}

void UContraryCheatManager::CmdFire(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	AContrarySurvivorPlayerController* PC = GetContraryPC();
	if (!PC)
	{
		Say(Ar, TEXT("QA: FIRE skipped - no player controller"));
		return;
	}
	// Тот же обработчик, что у ЛКМ/тапа (Enhanced Input IA_Fire -> Fire): выбор цели, доворот,
	// вспышка/звук/отдача — всё как в игре. Значение действия здесь не читается.
	PC->Fire(FInputActionValue());
	Say(Ar, TEXT("QA: FIRE (via Fire())"));
}

void UContraryCheatManager::CmdEquipPistol(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	APlayerCharacter* Player = GetPlayerChar();
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		Say(Ar, TEXT("QA: PISTOL skipped - no player character"));
		return;
	}
	FString Note;
	if (!Player->GetRangedWeaponInstance())
	{
		// Единый поток огнестрела (ADR-063): предмет в рюкзак -> в слот его переносит
		// TryAdoptRangedWeapon (тот же вызов, что тап по плитке в окне инвентаря). Класс — тот,
		// которым торгует торговец (MasterTrader.cpp: APistol).
		AMasterInventoryItem* Item = SpawnHiddenItem(World, APistol::StaticClass(), Player);
		if (!Item)
		{
			Say(Ar, TEXT("QA: PISTOL spawn FAILED"));
			return;
		}
		if (UInventoryComponent* Inv = Player->GetInventory())
		{
			Inv->AddItem(Item);
		}
		const bool bAdopted = Player->TryAdoptRangedWeapon(Item);
		Note = bAdopted ? TEXT("pistol given to slot") : TEXT("pistol left in backpack (slot busy?)");
	}
	else
	{
		Note = TEXT("ranged slot already filled");
	}
	// В руки: SwitchWeapon — тумблер нож<->огнестрел (кнопка «Оружие»).
	if (Player->GetRangedWeaponInstance() && Player->GetCurrentWeapon() != Player->GetRangedWeaponInstance())
	{
		Player->SwitchWeapon();
	}
	Say(Ar, FString::Printf(TEXT("QA: PISTOL %s; in hands: %s"), *Note, *GetNameSafe(Player->GetCurrentWeapon())));
}

void UContraryCheatManager::CmdGiveShowcaseLoot(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	APlayerCharacter* Player = GetPlayerChar();
	UWorld* World = GetWorld();
	UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
	if (!Player || !World || !Inv)
	{
		Say(Ar, TEXT("QA: LOOT skipped - no player/inventory"));
		return;
	}

	// 1) Броня на теле: полный комплект тест-набора (по умолчанию Т3 — верх прогрессии,
	//    PlayerCharacter.cpp:148) тем же путём, что консольная команда EquipTestArmor.
	Player->EquipTestArmor();

	// 2) Рюкзак. Предметы — реальные классы игры (те, что в каталоге торговца, MasterTrader.cpp:
	//    RebuildCatalog): расходники через штатную выдачу, патроны стаком, шкуры волка с
	//    ключом/названием прямо из класса волка, оружие и запасная броня Т1/Т2 как отдельные плитки.
	int32 Given = 0;
	Given += Player->GiveConsumableToBackpack(EConsumableType::Water, 6);
	Given += Player->GiveConsumableToBackpack(EConsumableType::Food, 4);
	Given += Player->GiveConsumableToBackpack(EConsumableType::Medkit, 3);
	Player->AddAmmoToInventory(48);

	// Шкуры: класс/ключ/название — с умолчаний волка (AWolfCharacter: QuestLootItem*), как в
	// его DropLoot. Стакаются в одну плитку «Шкура волка x3».
	{
		FString Src;
		UClass* WolfClass = ResolveWolfClass(Src);
		const AWolfCharacter* WolfCDO = WolfClass ? WolfClass->GetDefaultObject<AWolfCharacter>() : nullptr;
		if (WolfCDO && WolfCDO->QuestLootItemClass)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				if (AMasterInventoryItem* Pelt = SpawnHiddenItem(World, WolfCDO->QuestLootItemClass, Player))
				{
					Pelt->ItemName = WolfCDO->QuestLootItemName;
					Pelt->ItemDisplayText = WolfCDO->QuestLootItemText;
					if (Inv->AddItem(Pelt)) { ++Given; } else { Pelt->Destroy(); }
				}
			}
		}
	}

	// Оружие и запасная броня — по одному экземпляру (не стакаются).
	const TSubclassOf<AMasterInventoryItem> Singles[] =
	{
		APistol::StaticClass(), AMeleeWeapon::StaticClass(),
		AHeadArmorT1::StaticClass(), ATorsoArmorT1::StaticClass(), APantsArmorT1::StaticClass(),
		AHeadArmorT2::StaticClass(), ATorsoArmorT2::StaticClass(), APantsArmorT2::StaticClass(),
	};
	for (const TSubclassOf<AMasterInventoryItem>& Cls : Singles)
	{
		if (AMasterInventoryItem* Item = SpawnHiddenItem(World, Cls, Player))
		{
			if (Inv->AddItem(Item)) { ++Given; } else { Item->Destroy(); }
		}
	}

	Say(Ar, FString::Printf(TEXT("QA: LOOT given: armor T3 equipped (protection %.2f), backpack items %d, tiles now %d, ammo %d"),
		Player->GetTotalArmorProtection(), Given, Inv->GetInventoryItems().Num(), Player->GetReserveAmmoInInventory()));
}

void UContraryCheatManager::CmdGiveMoney(const TCHAR* Args, FOutputDevice& Ar)
{
	float Amount = 500.0f;
	ParseFloatArg(SplitArgs(Args), 0, Amount);
	APlayerCharacter* Player = GetPlayerChar();
	UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	if (!Stats)
	{
		Say(Ar, TEXT("QA: MONEY skipped - no player stats"));
		return;
	}
	Stats->AddMoney(Amount);
	Say(Ar, FString::Printf(TEXT("QA: MONEY +%.0f, balance %.0f"), Amount, Stats->GetMoney()));
}

void UContraryCheatManager::CmdOpenInventory(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	AContrarySurvivorPlayerController* PC = GetContraryPC();
	if (!PC)
	{
		return;
	}
	if (PC->bInventoryOpen)
	{
		Say(Ar, TEXT("QA: INVENTORY already open"));
		return;
	}
	CloseGameplayWindows(PC);
	PC->OnToggleInventory(); // клавиша Tab / кнопка «Сумка»
	Say(Ar, FString::Printf(TEXT("QA: INVENTORY %s"), PC->bInventoryOpen ? TEXT("opened") : TEXT("FAILED to open")));
}

void UContraryCheatManager::CmdOpenShop(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	AContrarySurvivorPlayerController* PC = GetContraryPC();
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	// Ближайший (или единственный) торговец — любой актор с интерфейсом IShopVendor.
	const FVector From = PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
	AActor* Best = nullptr;
	float BestSq = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->Implements<UShopVendor>())
		{
			continue;
		}
		const float DSq = FVector::DistSquared(From, It->GetActorLocation());
		if (DSq < BestSq)
		{
			BestSq = DSq;
			Best = *It;
		}
	}
	if (!Best)
	{
		Say(Ar, TEXT("QA: SHOP no vendor (IShopVendor) in world"));
		return;
	}
	CloseGameplayWindows(PC);
	PC->OpenShop(TScriptInterface<IShopVendor>(Best)); // тот же путь, что E у торговца
	Say(Ar, FString::Printf(TEXT("QA: SHOP %s with %s (%.0f cm away)"),
		PC->bShopOpen ? TEXT("opened") : TEXT("FAILED to open"), *Best->GetName(), FMath::Sqrt(BestSq)));
}

void UContraryCheatManager::CmdCloseUI(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	if (AContrarySurvivorPlayerController* PC = GetContraryPC())
	{
		CloseGameplayWindows(PC);
		Say(Ar, TEXT("QA: UI closed (inventory/shop/dialog/corpse/pause)"));
	}
}

void UContraryCheatManager::CmdFullStats(const TCHAR* /*Args*/, FOutputDevice& /*Ar*/)
{
	if (AContrarySurvivorPlayerController* PC = GetContraryPC())
	{
		PC->OnQAFullStats(); // сам пишет «QA: FULL STATS …»
	}
}

void UContraryCheatManager::CmdEndIntro(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	AContrarySurvivorPlayerController* PC = GetContraryPC();
	if (!PC)
	{
		return;
	}
	if (PC->IntroPhase == EIntroPhase::None)
	{
		Say(Ar, TEXT("QA: INTRO not running"));
		return;
	}
	// Как штатный пропуск + вход в деревню: грейд в норму, управление игроку (снимает чёрный
	// экран и блок ввода), затем EndIntro — задача «найти старосту» и маркер на него.
	if (APlayerCharacter* Player = GetPlayerChar())
	{
		Player->SetIntroGradeAlpha(1.0f);
	}
	if (PC->IntroPhase != EIntroPhase::HandOff)
	{
		PC->IntroHandOverControl();
	}
	PC->EndIntro();
	Say(Ar, TEXT("QA: INTRO ended by command"));
}

void UContraryCheatManager::CmdMenuContinue(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	AContrarySurvivorPlayerController* PC = GetContraryPC();
	if (!PC)
	{
		return;
	}
	if (PC->bStartScreenOpen)
	{
		PC->IsOnBootLevel() ? PC->HandleBootContinue() : PC->HandleStartScreenContinue();
		Say(Ar, TEXT("QA: MENU continue pressed"));
	}
	else
	{
		Say(Ar, TEXT("QA: MENU not open - continue ignored"));
	}
}

void UContraryCheatManager::CmdMenuNewGame(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	AContrarySurvivorPlayerController* PC = GetContraryPC();
	if (!PC)
	{
		return;
	}
	if (PC->bStartScreenOpen)
	{
		PC->IsOnBootLevel() ? PC->HandleBootNewGame() : PC->HandleStartScreenNewGame();
		Say(Ar, TEXT("QA: MENU new game pressed"));
	}
	else
	{
		Say(Ar, TEXT("QA: MENU not open - new game ignored"));
	}
}

void UContraryCheatManager::CmdHideHUD(const TCHAR* Args, FOutputDevice& Ar)
{
	bool bHide = true;
	if (!ParseBoolArg(SplitArgs(Args), 0, bHide))
	{
		Say(Ar, TEXT("QA: usage: QAHideHUD 0|1"));
		return;
	}
	ApplyHudHidden(bHide); // сам пишет «QA: HUD hidden/shown …»
}

void UContraryCheatManager::CmdCamZoom(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	float ArmLength = 0.0f;
	if (!ParseFloatArg(A, 0, ArmLength))
	{
		Say(Ar, TEXT("QA: usage: QACamZoom <длина_штанги> [pitch]"));
		return;
	}
	APlayerCharacter* Player = GetPlayerChar();
	USpringArmComponent* Arm = Player ? Player->FindComponentByClass<USpringArmComponent>() : nullptr;
	if (!Arm)
	{
		Say(Ar, TEXT("QA: CAMZOOM skipped - no spring arm on player"));
		return;
	}
	if (!bCamSaved)
	{
		SavedArmLength = Arm->TargetArmLength;
		SavedArmRotation = Arm->GetRelativeRotation();
		bCamSaved = true;
	}
	Arm->TargetArmLength = ArmLength;
	float Pitch = 0.0f;
	if (ParseFloatArg(A, 1, Pitch))
	{
		FRotator R = Arm->GetRelativeRotation();
		R.Pitch = Pitch;
		Arm->SetRelativeRotation(R);
	}
	Say(Ar, FString::Printf(TEXT("QA: CAMZOOM arm %.0f pitch %.1f (saved %.0f / %.1f)"),
		Arm->TargetArmLength, Arm->GetRelativeRotation().Pitch, SavedArmLength, SavedArmRotation.Pitch));
}

void UContraryCheatManager::CmdCamReset(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	APlayerCharacter* Player = GetPlayerChar();
	USpringArmComponent* Arm = Player ? Player->FindComponentByClass<USpringArmComponent>() : nullptr;
	if (!Arm || !bCamSaved)
	{
		Say(Ar, TEXT("QA: CAMRESET nothing to reset"));
		return;
	}
	Arm->TargetArmLength = SavedArmLength;
	Arm->SetRelativeRotation(SavedArmRotation);
	bCamSaved = false;
	Say(Ar, FString::Printf(TEXT("QA: CAMRESET arm %.0f pitch %.1f"), Arm->TargetArmLength, Arm->GetRelativeRotation().Pitch));
}

void UContraryCheatManager::CmdDebugCam(const TCHAR* Args, FOutputDevice& Ar)
{
	const TArray<FString> A = SplitArgs(Args);
	FVector Loc;
	float Pitch = 0.0f, Yaw = 0.0f, Fov = 0.0f;
	if (!ParseFloatArg(A, 0, Loc.X) || !ParseFloatArg(A, 1, Loc.Y) || !ParseFloatArg(A, 2, Loc.Z)
		|| !ParseFloatArg(A, 3, Pitch) || !ParseFloatArg(A, 4, Yaw))
	{
		Say(Ar, TEXT("QA: usage: QADebugCam X Y Z Pitch Yaw [FOV]"));
		return;
	}
	const bool bFovGiven = ParseFloatArg(A, 5, Fov);

	ADebugCameraController* DCC = GetActiveDebugCamera();
	if (!DCC)
	{
		EnableDebugCamera(); // штатно: спавнит наш AContraryDebugCameraController и переключает игрока
		DCC = GetActiveDebugCamera();
	}
	if (!DCC)
	{
		Say(Ar, TEXT("QA: DEBUGCAM FAILED to activate (no Player on controller?)"));
		return;
	}
	const FRotator Rot(Pitch, Yaw, 0.0f);
	// Как при активации движком (DebugCameraController.cpp:427): поворот управления + телепорт
	// пешки-наблюдателя.
	DCC->SetInitialLocationAndRotation(Loc, Rot);
	if (APawn* Spectator = DCC->GetPawnOrSpectator())
	{
		Spectator->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (bFovGiven && DCC->PlayerCameraManager)
	{
		DCC->PlayerCameraManager->SetFOV(Fov);
	}
	FVector CamLoc; FRotator CamRot;
	DCC->GetPlayerViewPoint(CamLoc, CamRot);
	Say(Ar, FString::Printf(TEXT("QA: DEBUGCAM at %s rot %s fov %.1f"), *VecStr(CamLoc), *CamRot.ToCompactString(),
		DCC->PlayerCameraManager ? DCC->PlayerCameraManager->GetFOVAngle() : 0.0f));
}

void UContraryCheatManager::CmdDebugCamOff(const TCHAR* /*Args*/, FOutputDevice& Ar)
{
	ADebugCameraController* DCC = GetActiveDebugCamera();
	if (!DCC)
	{
		Say(Ar, TEXT("QA: DEBUGCAM not active"));
		return;
	}
	// Ровно то, что делает UCheatManager::DisableDebugCamera (CheatManager.cpp:683) — но тот
	// работает только из cheat-manager'а самой камеры (IsDebugCameraActive смотрит на Outer).
	if (DCC->OriginalPlayer && DCC->OriginalControllerRef)
	{
		DCC->OriginalPlayer->SwitchController(DCC->OriginalControllerRef);
		DCC->OnDeactivate(DCC->OriginalControllerRef);
	}
	Say(Ar, TEXT("QA: DEBUGCAM off (back to player)"));
}

#endif // CONTRARY_WITH_QA_CHEATS
