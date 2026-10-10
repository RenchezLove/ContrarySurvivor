// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты карманного компьютера — КПК (поручение Рината 10.10.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Pda; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат:
//   - вид задания (основное или побочное) проставлен у всех существующих заданий: три задания
//     старосты — основные, задание охранника — побочное, строки боевой таблицы заданий;
//   - правила отслеживания: взятое основное становится отслеживаемым само, побочное — только
//     если ничего не отслеживается; сданное отслеживаемое уступает первому идущему основному,
//     иначе первому идущему; старое сохранение читается как «первое идущее»;
//   - отслеживаемое задание переживает сохранение и «Продолжить»;
//   - пересчёт «точка мира -> точка карты»: углы, центр, прижатие к краю, пропорции, и то,
//     что при границах по умолчанию верх карты совпадает с верхом экрана в игре;
//   - окно: журнал раскладывается по разделам, список и отметки карты собираются по журналу,
//     выбор и «Отслеживать» работают; готовый файл окна привязывается к коду всеми элементами.
//
// Окно проверяется теми же вызовами, что делают его кнопки (SelectQuest, TrackQuest,
// ToggleDoneSection) — живой Slate в прогонах проекта не поднимается, и открыть окно через
// контроллер без живого игрока нельзя; правило открытия проверяется отдельно (ShouldOpenPda).
// Слот сохранения ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter) — боевой слот не трогается.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Actors/TalkerNPC.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/Data/ContraryQuestLibrary.h"
#include "ContrarySurvivor/Data/ContraryQuestRow.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/PdaMapMath.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "ContrarySurvivor/UI/QuestTargets.h"
#include "ContrarySurvivor/UI/TouchControlsWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h" // GetTransientPackage
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags PdaTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace PdaTest
{
	// Транзиентный игровой мир (копия обвязки ScientistBaseAutomationTests.cpp).
	static UWorld* Create()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			return nullptr;
		}
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(World);

		const FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			WorldSettings->NotifyBeginPlay();
		}
		return World;
	}

	static void Destroy(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}

	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params);
	}

	// Таблица заданий на время теста подменяется ПУСТОЙ: задания берутся из кода, боевые
	// строки не мешают (CDO настроек переживает прогон — значение возвращается).
	struct FScopedEmptyQuestTable
	{
		UContraryDataSettings* Settings;
		TSoftObjectPtr<UDataTable> Saved;
		UDataTable* Table;

		FScopedEmptyQuestTable()
			: Settings(GetMutableDefault<UContraryDataSettings>())
		{
			Saved = Settings->QuestTable;
			Table = NewObject<UDataTable>(GetTransientPackage());
			Table->RowStruct = FContraryQuestRow::StaticStruct();
			Table->AddToRoot();
			Settings->QuestTable = Table;
		}
		~FScopedEmptyQuestTable()
		{
			Settings->QuestTable = Saved;
			Table->RemoveFromRoot();
		}
	};

	// Задание без целей и наград: после взятия сразу готово к сдаче тому, кто выдал.
	static FQuest MakeQuest(const TCHAR* Id, EQuestKind Kind, EQuestState State = EQuestState::NotStarted)
	{
		FQuest Quest;
		Quest.QuestId = FName(Id);
		Quest.Title = FText::FromString(FString::Printf(TEXT("QA %s"), Id));
		Quest.Kind = Kind;
		Quest.TargetCount = 0;
		Quest.RequiredItemCount = 0;
		Quest.RewardMoney = 0.0f;
		Quest.State = State;
		return Quest;
	}

	static FName TrackedId(const UQuestComponent* Quests)
	{
		const FQuest* Tracked = Quests ? Quests->GetTrackedQuest() : nullptr;
		return Tracked ? Tracked->QuestId : NAME_None;
	}

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}
}

// ===========================================================================
// 1. Вид задания проставлен у всех существующих заданий.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaQuestKindsTest,
	"ContrarySurvivor.Pda.QuestKindIsSetForEveryExistingQuest", PdaTestFlags)

bool FPdaQuestKindsTest::RunTest(const FString& Parameters)
{
	// Боевая таблица заданий: строки, заведённые до появления поля, — сюжет старосты.
	if (UDataTable* LiveTable = ContraryQuests::GetQuestTable())
	{
		for (const FName& RowName : LiveTable->GetRowNames())
		{
			const FContraryQuestRow* Row = LiveTable->FindRow<FContraryQuestRow>(RowName, TEXT("PdaQA"), /*bWarnIfRowMissing=*/false);
			if (TestNotNull(*FString::Printf(TEXT("Строка таблицы заданий %s читается"), *RowName.ToString()), Row))
			{
				TestEqual(*FString::Printf(TEXT("Строка таблицы заданий %s — основное задание"), *RowName.ToString()),
					Row->Kind, EQuestKind::Main);
			}
		}
	}
	else
	{
		AddInfo(TEXT("Таблица заданий в настройках проекта не задана — проверены только задания из кода."));
	}

	// Поле строки доезжает до задания.
	{
		PdaTest::FScopedEmptyQuestTable Scoped;
		FContraryQuestRow SideRow;
		SideRow.Title = FText::FromString(TEXT("QA побочное"));
		SideRow.Kind = EQuestKind::Side;
		Scoped.Table->AddRow(FName(TEXT("PdaSideRow")), SideRow);
		FContraryQuestRow DefaultRow;
		DefaultRow.Title = FText::FromString(TEXT("QA по умолчанию"));
		Scoped.Table->AddRow(FName(TEXT("PdaDefaultRow")), DefaultRow);

		FQuest Built;
		if (TestTrue(TEXT("Задание из строки с побочным видом собралось"),
			ContraryQuests::BuildQuestFromRow(FName(TEXT("PdaSideRow")), Built)))
		{
			TestEqual(TEXT("Вид задания взят из строки"), Built.Kind, EQuestKind::Side);
		}
		if (TestTrue(TEXT("Задание из строки без указания вида собралось"),
			ContraryQuests::BuildQuestFromRow(FName(TEXT("PdaDefaultRow")), Built)))
		{
			TestEqual(TEXT("Строка без указания вида — основное задание"), Built.Kind, EQuestKind::Main);
		}
	}

	// Задания из кода: староста и охранник базы учёных.
	PdaTest::FScopedEmptyQuestTable Scoped;
	UWorld* World = PdaTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	AElderNPC* Elder = PdaTest::Spawn<AElderNPC>(World, FVector(3000.0f, 0.0f, 100.0f));
	AScientistGuardNPC* Guard = PdaTest::Spawn<AScientistGuardNPC>(World, FVector(6000.0f, 0.0f, 100.0f));
	if (TestNotNull(TEXT("Староста заспавнен"), Elder) && TestNotNull(TEXT("Охранник заспавнен"), Guard))
	{
		TestEqual(TEXT("Первое задание старосты — основное"), Elder->GetOfferedQuest().Kind, EQuestKind::Main);
		TestEqual(TEXT("Второе задание старосты — основное"), Elder->GetSecondQuest().Kind, EQuestKind::Main);
		TestEqual(TEXT("Третье задание старосты — основное"), Elder->GetThirdQuest().Kind, EQuestKind::Main);
		TestTrue(TEXT("У заданий старосты выдавший не записан (пусто = староста)"),
			Elder->GetOfferedQuest().GiverNpcTag.IsNone() && Elder->GetThirdQuest().GiverNpcTag.IsNone());

		TestEqual(TEXT("Задание охранника — побочное"), Guard->GetOfferedQuest().Kind, EQuestKind::Side);
		TestEqual(TEXT("Выдавший задание охранника — он сам"), Guard->GetOfferedQuest().GiverNpcTag, Guard->GetNpcTag());

		// Имена для строки «Выдал» и «Сдать» берутся у персонажей на уровне.
		TestEqual(TEXT("Имя выдавшего задание старосты"),
			QuestTargets::ResolveGiverName(World, Elder->GetOfferedQuest()).ToString(), Elder->GetNPCMarkerLabel().ToString());
		TestEqual(TEXT("Имя принимающего задание охранника"),
			QuestTargets::ResolveReceiverName(World, Guard->GetOfferedQuest()).ToString(), Guard->GetNPCMarkerLabel().ToString());
	}
	PdaTest::Destroy(World);
	return true;
}

// ===========================================================================
// 2. Правила отслеживания.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaTrackingRulesTest,
	"ContrarySurvivor.Pda.TrackingRules", PdaTestFlags)

bool FPdaTrackingRulesTest::RunTest(const FString& Parameters)
{
	UWorld* World = PdaTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = PdaTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Quests)
	{
		AddError(TEXT("Не заспавнен игрок с журналом заданий"));
		PdaTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(TEXT("ContraryPdaTrackingQA"));

	const FName S1(TEXT("PdaS1")), S2(TEXT("PdaS2")), M1(TEXT("PdaM1")), M2(TEXT("PdaM2"));
	Quests->OfferQuest(PdaTest::MakeQuest(TEXT("PdaS1"), EQuestKind::Side));
	Quests->OfferQuest(PdaTest::MakeQuest(TEXT("PdaM1"), EQuestKind::Main));
	Quests->OfferQuest(PdaTest::MakeQuest(TEXT("PdaS2"), EQuestKind::Side));
	Quests->OfferQuest(PdaTest::MakeQuest(TEXT("PdaM2"), EQuestKind::Main));

	TestTrue(TEXT("Пока ничего не взято — отслеживать нечего"), PdaTest::TrackedId(Quests).IsNone());
	TestFalse(TEXT("Невзятое задание отслеживать нельзя"), Quests->SetTrackedQuest(S1));

	TestTrue(TEXT("Побочное задание взято"), Quests->AcceptQuest(S1));
	TestEqual(TEXT("Побочное при пустом отслеживании становится отслеживаемым"), PdaTest::TrackedId(Quests), S1);

	TestTrue(TEXT("Основное задание взято"), Quests->AcceptQuest(M1));
	TestEqual(TEXT("Взятое основное становится отслеживаемым само"), PdaTest::TrackedId(Quests), M1);

	TestTrue(TEXT("Второе побочное взято"), Quests->AcceptQuest(S2));
	TestEqual(TEXT("Побочное не перебивает отслеживаемое"), PdaTest::TrackedId(Quests), M1);

	TestTrue(TEXT("Игрок выбрал побочное для отслеживания"), Quests->SetTrackedQuest(S2));
	TestEqual(TEXT("Отслеживается выбранное игроком"), PdaTest::TrackedId(Quests), S2);
	TestFalse(TEXT("Несуществующее задание отслеживать нельзя"), Quests->SetTrackedQuest(FName(TEXT("PdaNoSuchQuest"))));
	TestEqual(TEXT("Отказ не меняет отслеживаемое"), PdaTest::TrackedId(Quests), S2);

	TestTrue(TEXT("Второе основное взято"), Quests->AcceptQuest(M2));
	TestEqual(TEXT("Новое основное снова становится отслеживаемым"), PdaTest::TrackedId(Quests), M2);

	TestTrue(TEXT("Отслеживаемое основное сдано"), Quests->TurnInQuest(M2));
	TestEqual(TEXT("После сдачи отслеживается первое идущее основное"), PdaTest::TrackedId(Quests), M1);

	TestTrue(TEXT("Последнее основное сдано"), Quests->TurnInQuest(M1));
	TestEqual(TEXT("Основных не осталось — отслеживается первое идущее"), PdaTest::TrackedId(Quests), S1);

	TestTrue(TEXT("Игрок переключился на второе побочное"), Quests->SetTrackedQuest(S2));
	TestTrue(TEXT("Сдано НЕ отслеживаемое задание"), Quests->TurnInQuest(S1));
	TestEqual(TEXT("Сдача чужого задания отслеживаемое не трогает"), PdaTest::TrackedId(Quests), S2);

	TestTrue(TEXT("Сдано последнее задание"), Quests->TurnInQuest(S2));
	TestTrue(TEXT("Идущих заданий нет — отслеживать нечего"), PdaTest::TrackedId(Quests).IsNone());
	TestTrue(TEXT("Выбор отслеживаемого сброшен"), Quests->GetTrackedQuestId().IsNone());

	// Старое сохранение: отслеживаемое не записано — действует «первое идущее задание журнала»,
	// даже если оно побочное и стоит раньше основного.
	const TArray<FQuest> OldJournal = {
		PdaTest::MakeQuest(TEXT("PdaDone"), EQuestKind::Main, EQuestState::TurnedIn),
		PdaTest::MakeQuest(TEXT("PdaS1"), EQuestKind::Side, EQuestState::Active),
		PdaTest::MakeQuest(TEXT("PdaM1"), EQuestKind::Main, EQuestState::Active) };
	Quests->RestoreQuests(OldJournal);
	TestTrue(TEXT("Старое сохранение: выбор отслеживаемого пуст"), Quests->GetTrackedQuestId().IsNone());
	TestEqual(TEXT("Старое сохранение: отслеживается первое идущее"), PdaTest::TrackedId(Quests), S1);

	Quests->RestoreQuests(OldJournal, M1);
	TestEqual(TEXT("Новое сохранение: отслеживается записанное задание"), PdaTest::TrackedId(Quests), M1);

	// Записанное задание в журнале уже сдано — выбирается первое идущее основное.
	Quests->RestoreQuests(OldJournal, FName(TEXT("PdaDone")));
	TestEqual(TEXT("Записанное задание сдано — отслеживается первое идущее основное"), PdaTest::TrackedId(Quests), M1);

	// Вид задания доезжает до журнала, записанного до появления поля: выдающий повторяет
	// предложение, и побочное задание перестаёт считаться основным.
	FQuest Stale = PdaTest::MakeQuest(TEXT("PdaS1"), EQuestKind::Main, EQuestState::Active);
	Quests->RestoreQuests({ Stale });
	FQuest Offered = PdaTest::MakeQuest(TEXT("PdaS1"), EQuestKind::Side);
	Offered.GiverNpcTag = FName(TEXT("PdaGiver"));
	Quests->OfferQuest(Offered);
	const FQuest* Healed = Quests->FindQuest(S1);
	if (TestNotNull(TEXT("Задание осталось в журнале"), Healed))
	{
		TestEqual(TEXT("Повторное предложение исправило вид задания"), Healed->Kind, EQuestKind::Side);
		TestEqual(TEXT("Повторное предложение записало выдавшего"), Healed->GiverNpcTag, FName(TEXT("PdaGiver")));
		TestEqual(TEXT("Повторное предложение не сбросило состояние"), Healed->State, EQuestState::Active);
	}

	PdaTest::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Отслеживаемое задание переживает сохранение и «Продолжить».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaTrackedQuestSaveTest,
	"ContrarySurvivor.Pda.TrackedQuestSurvivesSaveAndContinue", PdaTestFlags)

bool FPdaTrackedQuestSaveTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryPdaSaveQA");
	PdaTest::DeleteSlot(Slot);

	const FName S1(TEXT("PdaS1")), M1(TEXT("PdaM1"));
	const TArray<FQuest> Journal = {
		PdaTest::MakeQuest(TEXT("PdaM1"), EQuestKind::Main, EQuestState::Active),
		PdaTest::MakeQuest(TEXT("PdaS1"), EQuestKind::Side, EQuestState::Active) };

	// Сессия 1: игрок переключил отслеживание на побочное и сохранился.
	{
		UWorld* World = PdaTest::Create();
		if (!TestNotNull(TEXT("Мир сессии 1 создан"), World))
		{
			return false;
		}
		ASaveTestPlayerCharacter* P1 = PdaTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
		if (TestNotNull(TEXT("Игрок сессии 1 заспавнен"), P1) && P1->GetQuests())
		{
			P1->UseTestSaveSlot(Slot);
			P1->GetQuests()->RestoreQuests(Journal);
			TestTrue(TEXT("Побочное выбрано для отслеживания"), P1->GetQuests()->SetTrackedQuest(S1));
			TestTrue(TEXT("Сохранение сессии 1 прошло"), P1->SaveGame());
		}
		PdaTest::Destroy(World);
	}

	// Сессия 2: «Продолжить» возвращает то же отслеживаемое задание и вид каждого задания.
	{
		UWorld* World = PdaTest::Create();
		if (!TestNotNull(TEXT("Мир сессии 2 создан"), World))
		{
			return false;
		}
		ASaveTestPlayerCharacter* P2 = PdaTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
		if (TestNotNull(TEXT("Игрок сессии 2 заспавнен"), P2) && P2->GetQuests())
		{
			P2->UseTestSaveSlot(Slot);
			TestTrue(TEXT("«Продолжить» сессии 2 отработал"), P2->LoadGameForContinue());
			TestEqual(TEXT("Отслеживаемое задание вернулось из сохранения"), PdaTest::TrackedId(P2->GetQuests()), S1);
			const FQuest* Side = P2->GetQuests()->FindQuest(S1);
			const FQuest* Main = P2->GetQuests()->FindQuest(M1);
			if (TestNotNull(TEXT("Побочное задание в журнале"), Side) && TestNotNull(TEXT("Основное задание в журнале"), Main))
			{
				TestEqual(TEXT("Вид побочного задания сохранился"), Side->Kind, EQuestKind::Side);
				TestEqual(TEXT("Вид основного задания сохранился"), Main->Kind, EQuestKind::Main);
			}

			// Сохранение, записанное до появления поля: стираем его из файла и читаем заново.
			if (UContrarySaveGame* Save = P2->LoadOrCreateSaveObject())
			{
				Save->TrackedQuestId = NAME_None;
				P2->WriteSaveObject(Save);
			}
			TestTrue(TEXT("Старое сохранение загрузилось"), P2->LoadGameForContinue());
			TestTrue(TEXT("Старое сохранение: выбор отслеживаемого пуст"), P2->GetQuests()->GetTrackedQuestId().IsNone());
			TestEqual(TEXT("Старое сохранение: отслеживается первое идущее задание"), PdaTest::TrackedId(P2->GetQuests()), M1);
		}
		PdaTest::Destroy(World);
	}

	PdaTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 4. Пересчёт «точка мира -> точка карты».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaMapMathTest,
	"ContrarySurvivor.Pda.WorldPointToMapPoint", PdaTestFlags)

bool FPdaMapMathTest::RunTest(const FString& Parameters)
{
	// Прямоугольник с осями, как у игровой камеры: левый-верхний угол — наибольшие X и Y.
	const FVector2D TopLeft(1000.0, 2000.0);
	const FVector2D BottomRight(-3000.0, -6000.0);
	const double Tolerance = 1.e-6;

	auto Check = [this, &TopLeft, &BottomRight, Tolerance](const TCHAR* What, const FVector2D& World, double U, double V)
	{
		const FVector2D UV = PdaMap::WorldToMapUV(World, TopLeft, BottomRight);
		TestTrue(*FString::Printf(TEXT("%s: получено (%.4f, %.4f), ждали (%.4f, %.4f)"), What, UV.X, UV.Y, U, V),
			FMath::IsNearlyEqual(UV.X, U, Tolerance) && FMath::IsNearlyEqual(UV.Y, V, Tolerance));
	};
	Check(TEXT("Левый-верхний угол"), TopLeft, 0.0, 0.0);
	Check(TEXT("Правый-нижний угол"), BottomRight, 1.0, 1.0);
	Check(TEXT("Правый-верхний угол"), FVector2D(BottomRight.X, TopLeft.Y), 1.0, 0.0);
	Check(TEXT("Левый-нижний угол"), FVector2D(TopLeft.X, BottomRight.Y), 0.0, 1.0);
	Check(TEXT("Центр"), FVector2D(-1000.0, -2000.0), 0.5, 0.5);
	Check(TEXT("Четверть по обеим осям"), FVector2D(0.0, 0.0), 0.25, 0.25);
	Check(TEXT("Точка левее и выше карты прижата к углу"), FVector2D(9000.0, 9000.0), 0.0, 0.0);
	Check(TEXT("Точка правее карты прижата к правому краю"), FVector2D(-9000.0, -2000.0), 1.0, 0.5);
	Check(TEXT("Точка ниже карты прижата к нижнему краю"), FVector2D(-1000.0, -90000.0), 0.5, 1.0);

	// Вырожденный прямоугольник не делит на ноль.
	const FVector2D Flat = PdaMap::WorldToMapUV(FVector2D(5.0, 5.0), FVector2D(0.0, 0.0), FVector2D(0.0, 10.0));
	TestTrue(TEXT("Нулевая ширина прямоугольника даёт середину по горизонтали"), FMath::IsNearlyEqual(Flat.X, 0.5, Tolerance));
	TestTrue(TEXT("Высота при этом считается как обычно"), FMath::IsNearlyEqual(Flat.Y, 0.5, Tolerance));

	// Направление: «вверх по экрану» (+Y мира) на карте смотрит вверх, +X мира — влево.
	const FVector2D Up = PdaMap::WorldDirToMapDir(FVector2D(0.0, 1.0), TopLeft, BottomRight);
	TestTrue(TEXT("+Y мира на карте — вверх"), Up.Y < 0.0 && FMath::IsNearlyZero(Up.X));
	const FVector2D PlusX = PdaMap::WorldDirToMapDir(FVector2D(1.0, 0.0), TopLeft, BottomRight);
	TestTrue(TEXT("+X мира на карте — влево"), PlusX.X < 0.0 && FMath::IsNearlyZero(PlusX.Y));

	// Поле карты вписывается с пропорциями мира (мир 4000 на 8000 — карта вдвое выше, чем шире).
	const FVector2D Fitted = PdaMap::FitMapSize(FVector2D(1000.0, 600.0), TopLeft, BottomRight);
	TestTrue(TEXT("Карта вписана по высоте"), FMath::IsNearlyEqual(Fitted.Y, 600.0, 1.e-3));
	TestTrue(TEXT("Ширина карты — половина высоты"), FMath::IsNearlyEqual(Fitted.X, 300.0, 1.e-3));

	// Границы по умолчанию: верх карты совпадает с верхом экрана в игре. Камера смотрит вдоль
	// +Y мира, поэтому точка с большим Y обязана оказаться выше, а с большим X — левее.
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	TestTrue(TEXT("Границы карты по умолчанию: левый-верхний угол — наибольшие X и Y"),
		Settings->MapWorldTopLeft.X > Settings->MapWorldBottomRight.X
		&& Settings->MapWorldTopLeft.Y > Settings->MapWorldBottomRight.Y);

	// Известные точки уровня (деревня, вход базы учёных, южное логово) лежат внутри карты,
	// не на её краю, и стоят друг относительно друга как на экране игры.
	const FVector2D Village = PdaMap::WorldToMapUV(FVector2D(-4415.0, -3901.0), Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	const FVector2D Scientists = PdaMap::WorldToMapUV(FVector2D(7379.0, -16041.0), Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	const FVector2D SouthDen = PdaMap::WorldToMapUV(FVector2D(-5280.0, -13440.0), Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	auto Inside = [](const FVector2D& UV) { return UV.X > 0.02 && UV.X < 0.98 && UV.Y > 0.02 && UV.Y < 0.98; };
	TestTrue(TEXT("Деревня внутри карты"), Inside(Village));
	TestTrue(TEXT("База учёных внутри карты"), Inside(Scientists));
	TestTrue(TEXT("Южное логово внутри карты"), Inside(SouthDen));
	TestTrue(TEXT("Южное логово на карте ниже деревни"), SouthDen.Y > Village.Y);
	TestTrue(TEXT("База учёных на карте левее деревни (у неё больший X мира)"), Scientists.X < Village.X);
	return true;
}

// ===========================================================================
// 5. Журнал раскладывается по разделам окна; правило открытия; событие статистики.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaJournalViewTest,
	"ContrarySurvivor.Pda.JournalViewAndOpenRule", PdaTestFlags)

bool FPdaJournalViewTest::RunTest(const FString& Parameters)
{
	const TArray<FQuest> Journal = {
		PdaTest::MakeQuest(TEXT("PdaDone"), EQuestKind::Main, EQuestState::TurnedIn),
		PdaTest::MakeQuest(TEXT("PdaS1"), EQuestKind::Side, EQuestState::Active),
		PdaTest::MakeQuest(TEXT("PdaM1"), EQuestKind::Main, EQuestState::Completed),
		PdaTest::MakeQuest(TEXT("PdaOffered"), EQuestKind::Main, EQuestState::NotStarted),
		PdaTest::MakeQuest(TEXT("PdaM2"), EQuestKind::Main, EQuestState::Active) };

	FPdaJournalView View;
	UPdaScreenWidget::BuildJournalView(Journal, FName(TEXT("PdaS1")), View);
	TestEqual(TEXT("Основных идущих два"), View.Main.Num(), 2);
	if (View.Main.Num() == 2)
	{
		TestEqual(TEXT("Основные идут в порядке журнала (первое)"), View.Main[0], FName(TEXT("PdaM1")));
		TestEqual(TEXT("Основные идут в порядке журнала (второе)"), View.Main[1], FName(TEXT("PdaM2")));
	}
	TestEqual(TEXT("Побочное одно"), View.Side.Num(), 1);
	TestEqual(TEXT("Выполненное одно"), View.Done.Num(), 1);
	TestEqual(TEXT("Отслеживаемое отмечено"), View.Tracked, FName(TEXT("PdaS1")));
	TestFalse(TEXT("Предложенное, но не взятое задание в окно не попадает"),
		View.Main.Contains(FName(TEXT("PdaOffered"))) || View.Side.Contains(FName(TEXT("PdaOffered"))));

	UPdaScreenWidget::BuildJournalView(Journal, FName(TEXT("PdaDone")), View);
	TestTrue(TEXT("Сданное задание отслеживаемым не показывается"), View.Tracked.IsNone());

	UPdaScreenWidget::BuildJournalView(TArray<FQuest>(), NAME_None, View);
	TestTrue(TEXT("Пустой журнал — пустое окно"), View.IsEmpty());

	// Правило открытия окна.
	TestTrue(TEXT("В игре без других окон КПК открывается"),
		AContrarySurvivorPlayerController::ShouldOpenPda(true, false, false, false));
	TestFalse(TEXT("Без героя КПК не открывается"),
		AContrarySurvivorPlayerController::ShouldOpenPda(false, false, false, false));
	TestFalse(TEXT("На загрузочном уровне КПК не открывается"),
		AContrarySurvivorPlayerController::ShouldOpenPda(true, true, false, false));
	TestFalse(TEXT("Во вступлении КПК не открывается"),
		AContrarySurvivorPlayerController::ShouldOpenPda(true, false, true, false));
	TestFalse(TEXT("Поверх другого окна КПК не открывается"),
		AContrarySurvivorPlayerController::ShouldOpenPda(true, false, false, true));

	TestEqual(TEXT("Имя события «открыл КПК»"), UAnalyticsSubsystem::MakePdaOpenedEventId(), FString(TEXT("pda:opened")));

	// Цвета вида задания различаются и не прозрачны.
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	TestFalse(TEXT("Основные и побочные задания красятся по-разному"),
		Settings->GetQuestKindColor(EQuestKind::Main).Equals(Settings->GetQuestKindColor(EQuestKind::Side), 0.05f));
	TestTrue(TEXT("Основное — тёплый цвет (красного больше, чем синего)"),
		Settings->MainQuestColor.R > Settings->MainQuestColor.B);
	TestTrue(TEXT("Побочное — холодный цвет (синего больше, чем красного)"),
		Settings->SideQuestColor.B > Settings->SideQuestColor.R);
	return true;
}

// ===========================================================================
// 6. Окно: список и отметки карты собираются по журналу, выбор и «Отслеживать» работают.
//    Гоняется на обоих деревьях: построенном кодом и взятом из готового файла окна.
// ===========================================================================
namespace PdaTest
{
	static void RunWindowChecks(FAutomationTestBase& Test, UWorld* World, UClass* WidgetClass, const TCHAR* Path)
	{
		ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
		UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
		// Цели заданий: настоящие акторы с обычной меткой (костёр — готовый актор с корнем).
		ACampfire* TargetA = Spawn<ACampfire>(World, FVector(1000.0f, 2000.0f, 0.0f));
		ACampfire* TargetB = Spawn<ACampfire>(World, FVector(-3000.0f, -5000.0f, 0.0f));
		if (!Player || !Quests || !TargetA || !TargetB)
		{
			Test.AddError(FString::Printf(TEXT("%s: не заспавнены игрок и цели заданий"), Path));
			return;
		}
		Player->UseTestSaveSlot(TEXT("ContraryPdaWindowQA"));
		TargetA->Tags.Add(FName(TEXT("PdaTargetA")));
		TargetB->Tags.Add(FName(TEXT("PdaTargetB")));

		FQuest Main = MakeQuest(TEXT("PdaM1"), EQuestKind::Main, EQuestState::Active);
		Main.TargetCount = 3; // цель не выполнена — задание остаётся «идёт» и ведёт к цели
		Main.MapMarkerTag = FName(TEXT("PdaTargetA"));
		FQuest Side = MakeQuest(TEXT("PdaS1"), EQuestKind::Side, EQuestState::Active);
		Side.TargetCount = 2;
		Side.MapMarkerTag = FName(TEXT("PdaTargetB"));
		FQuest NoTarget = MakeQuest(TEXT("PdaS2"), EQuestKind::Side, EQuestState::Active);
		NoTarget.TargetCount = 1; // без метки цели — в списке есть, на карте нет
		FQuest Done = MakeQuest(TEXT("PdaDone"), EQuestKind::Main, EQuestState::TurnedIn);
		FQuest Offered = MakeQuest(TEXT("PdaOffered"), EQuestKind::Main, EQuestState::NotStarted);
		Quests->RestoreQuests({ Done, Side, Main, NoTarget, Offered }, Main.QuestId);

		UPdaScreenWidget* Window = CreateWidget<UPdaScreenWidget>(World, WidgetClass);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: окно создалось"), Path), Window))
		{
			return;
		}

		// «Открытие»: то же, что делает контроллер перед показом окна.
		Window->InitPda(Player);
		Test.TestEqual(*FString::Printf(TEXT("%s: в списке три идущих задания"), Path), Window->GetQuestRowCount(), 3);
		Test.TestEqual(*FString::Printf(TEXT("%s: выбрано отслеживаемое задание"), Path), Window->GetSelectedQuestId(), Main.QuestId);
		Test.TestEqual(*FString::Printf(TEXT("%s: на карте отметки двух заданий с целью"), Path), Window->GetQuestMapMarkerCount(), 2);
		Test.TestTrue(*FString::Printf(TEXT("%s: на карте есть известные места (костры)"), Path), Window->GetPlaceMapMarkerCount() >= 2);
		Test.TestFalse(*FString::Printf(TEXT("%s: открывается вкладка заданий"), Path), Window->IsMapTabShown());
		Test.TestFalse(*FString::Printf(TEXT("%s: раздел выполненных свёрнут"), Path), Window->IsDoneSectionExpanded());

		Window->ShowTab(true);
		Test.TestTrue(*FString::Printf(TEXT("%s: вкладка карты показана"), Path), Window->IsMapTabShown());

		Window->ToggleDoneSection();
		Window->RefreshNow();
		Test.TestTrue(*FString::Printf(TEXT("%s: раздел выполненных развёрнут"), Path), Window->IsDoneSectionExpanded());
		Test.TestEqual(*FString::Printf(TEXT("%s: выполненные в строки заданий не попадают"), Path), Window->GetQuestRowCount(), 3);

		// Выбор строки и кнопка «Отслеживать» (то же делает нажатие на отметку карты).
		Window->SelectQuest(Side.QuestId);
		Test.TestEqual(*FString::Printf(TEXT("%s: выбрано побочное задание"), Path), Window->GetSelectedQuestId(), Side.QuestId);
		Test.TestEqual(*FString::Printf(TEXT("%s: выбор строки отслеживаемое не меняет"), Path), TrackedId(Quests), Main.QuestId);
		Test.TestTrue(*FString::Printf(TEXT("%s: «Отслеживать» сработало"), Path), Window->TrackQuest(Side.QuestId));
		Test.TestEqual(*FString::Printf(TEXT("%s: журнал отслеживает побочное"), Path), TrackedId(Quests), Side.QuestId);
		Test.TestFalse(*FString::Printf(TEXT("%s: сданное задание отслеживать нельзя"), Path), Window->TrackQuest(Done.QuestId));
		Test.TestEqual(*FString::Printf(TEXT("%s: отказ отслеживаемое не меняет"), Path), TrackedId(Quests), Side.QuestId);

		// Метка-указатель в мире и отметка карты берут цель из одного источника.
		const FQuest* SideInJournal = Quests->FindQuest(Side.QuestId);
		Test.TestTrue(*FString::Printf(TEXT("%s: цель побочного задания — свой актор"), Path),
			SideInJournal && QuestTargets::FindTargetActor(World, *SideInJournal) == TargetB);

		// Журнал изменился при открытом окне — окно перечитывает его.
		Quests->RestoreQuests(TArray<FQuest>());
		Window->RefreshNow();
		Test.TestEqual(*FString::Printf(TEXT("%s: пустой журнал — строк нет"), Path), Window->GetQuestRowCount(), 0);
		Test.TestEqual(*FString::Printf(TEXT("%s: пустой журнал — отметок заданий нет"), Path), Window->GetQuestMapMarkerCount(), 0);
		Test.TestTrue(*FString::Printf(TEXT("%s: пустой журнал — выбор сброшен"), Path), Window->GetSelectedQuestId().IsNone());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaWindowCodeTreeTest,
	"ContrarySurvivor.Pda.WindowBuildsListAndMapFromJournal", PdaTestFlags)

bool FPdaWindowCodeTreeTest::RunTest(const FString& Parameters)
{
	UWorld* World = PdaTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	PdaTest::RunWindowChecks(*this, World, UPdaScreenWidget::StaticClass(), TEXT("Окно, построенное кодом"));
	PdaTest::Destroy(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaWindowAssetTest,
	"ContrarySurvivor.Pda.DesignerAssetBindsToCode", PdaTestFlags)

bool FPdaWindowAssetTest::RunTest(const FString& Parameters)
{
	UClass* AssetClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	if (!TestNotNull(TEXT("Готовый файл окна /Game/UI/WBP_Pda загрузился"), AssetClass))
	{
		return false;
	}
	TestTrue(TEXT("Контроллер по умолчанию берёт окно из готового файла"),
		GetDefault<AContrarySurvivorPlayerController>()->GetPdaWidgetClass().Get() == AssetClass);

	UWorld* World = PdaTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Каждое поле-элемент окна обязано найти свой элемент в файле: разойдись имена — окно
	// приехало бы игроку пустым без единой жалобы в журнале. Перебор самих полей класса:
	// заведут новый элемент — он попадёт под проверку сам.
	if (UPdaScreenWidget* Probe = CreateWidget<UPdaScreenWidget>(World, AssetClass))
	{
		int32 Checked = 0;
		for (TFieldIterator<FObjectPropertyBase> It(UPdaScreenWidget::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			const FObjectPropertyBase* Prop = *It;
			if (!Prop->PropertyClass || !Prop->PropertyClass->IsChildOf(UWidget::StaticClass()))
			{
				continue;
			}
			++Checked;
			TestNotNull(*FString::Printf(TEXT("Элемент %s привязан к файлу окна"), *Prop->GetName()),
				Prop->GetObjectPropertyValue_InContainer(Probe));
		}
		TestTrue(TEXT("Проверены все элементы окна"), Checked >= 20);
	}
	else
	{
		AddError(TEXT("Окно из готового файла не создалось"));
	}

	PdaTest::RunWindowChecks(*this, World, AssetClass, TEXT("Окно из готового файла"));
	PdaTest::Destroy(World);
	return true;
}

// ===========================================================================
// Кнопка «КПК» стоит рядом с кнопкой сумки из готового файла экранных кнопок: не наезжает
// на неё даже на пике её мигания и не задевает остальные кнопки слоя на экранах от 16:9
// до 20:9 (высота экрана в единицах раскладки всегда 1080).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaTouchButtonBesideBagTest,
	"ContrarySurvivor.Pda.TouchButtonStandsBesideBagButton", PdaTestFlags)

bool FPdaTouchButtonBesideBagTest::RunTest(const FString& Parameters)
{
	// Сама формула: сосед 100 на 100 с центром в нуле вырастает в полтора раза (до 75 от
	// центра), просвет 10, радиус 20 — центр на 105 левее либо правее, высота та же.
	const FVector2D Left = UPdaTouchButtonWidget::ComputeCenterBeside(FVector2D(0.0, 30.0), FVector2D(100.0, 100.0), 1.5f, 20.0f, 10.0f, true);
	const FVector2D Right = UPdaTouchButtonWidget::ComputeCenterBeside(FVector2D(0.0, 30.0), FVector2D(100.0, 100.0), 1.5f, 20.0f, 10.0f, false);
	TestEqual(TEXT("Слева от соседа: центр по горизонтали"), Left.X, -105.0);
	TestEqual(TEXT("Справа от соседа: центр по горизонтали"), Right.X, 105.0);
	TestEqual(TEXT("Высота — как у соседа"), Left.Y, 30.0);

	UClass* LayerClass = StaticLoadClass(UTouchControlsWidget::StaticClass(), nullptr,
		TEXT("/Game/UI/WBP_TouchControls.WBP_TouchControls_C"));
	if (!TestNotNull(TEXT("Готовый файл экранных кнопок загружен"), LayerClass))
	{
		return false;
	}
	UWorld* World = PdaTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	UTouchControlsWidget* Layer = CreateWidget<UTouchControlsWidget>(World, LayerClass);
	FAnchors BagAnchors;
	FVector2D BagCenter, BagSize;
	if (!TestNotNull(TEXT("Слой экранных кнопок создан"), Layer)
		|| !TestTrue(TEXT("Кнопка сумки найдена в слое"), Layer->GetInventoryButtonPlacement(BagAnchors, BagCenter, BagSize)))
	{
		PdaTest::Destroy(World);
		return false;
	}

	const AContrarySurvivorPlayerController* Defaults = GetDefault<AContrarySurvivorPlayerController>();
	const float Radius = Defaults->GetPdaTouchButtonRadius();
	const float Gap = Defaults->GetPdaTouchButtonGap();
	const bool bLeft = BagAnchors.Minimum.X >= 0.5;
	const FVector2D PdaCenter = UPdaTouchButtonWidget::ComputeCenterBeside(BagCenter, BagSize,
		Layer->GetInventoryBlinkScale(), Radius, Gap, bLeft);

	// Просвет между кнопкой «КПК» и сумкой в покое — не меньше 12 (требование постановки).
	const double RestGap = FMath::Abs(PdaCenter.X - BagCenter.X) - BagSize.X * 0.5 - Radius;
	TestTrue(*FString::Printf(TEXT("Просвет до сумки в покое не меньше 12 (вышло %.1f)"), RestGap), RestGap >= 12.0);
	AddInfo(FString::Printf(TEXT("Сумка: центр (%.1f, %.1f), размер %.1f на %.1f. КПК: центр (%.1f, %.1f), радиус %.1f, просвет в покое %.1f."),
		BagCenter.X, BagCenter.Y, BagSize.X, BagSize.Y, PdaCenter.X, PdaCenter.Y, Radius, RestGap));

	// Остальные кнопки слоя: прямоугольники на экранах 1920 и 2400 в ширину при высоте 1080.
	const UCanvasPanel* Root = Layer->WidgetTree ? Cast<UCanvasPanel>(Layer->WidgetTree->RootWidget) : nullptr;
	if (TestNotNull(TEXT("Корень слоя — канва"), Root))
	{
		for (const double ScreenWidth : { 1920.0, 2400.0 })
		{
			const FVector2D Screen(ScreenWidth, 1080.0);
			const FVector2D PdaOnScreen = FVector2D(BagAnchors.Minimum) * Screen + PdaCenter;
			const FBox2D PdaBox(PdaOnScreen - FVector2D(Radius), PdaOnScreen + FVector2D(Radius));
			TestTrue(*FString::Printf(TEXT("Кнопка «КПК» целиком на экране шириной %.0f"), ScreenWidth),
				PdaBox.Min.X >= 0.0 && PdaBox.Min.Y >= 0.0 && PdaBox.Max.X <= Screen.X && PdaBox.Max.Y <= Screen.Y);

			for (int32 Index = 0; Index < Root->GetChildrenCount(); ++Index)
			{
				const UWidget* Child = Root->GetChildAt(Index);
				const UCanvasPanelSlot* ChildSlot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
				if (!ChildSlot || ChildSlot->GetAutoSize() || ChildSlot->GetAnchors().Minimum != ChildSlot->GetAnchors().Maximum)
				{
					continue;
				}
				const FVector2D Size = ChildSlot->GetSize();
				const FVector2D Min = FVector2D(ChildSlot->GetAnchors().Minimum) * Screen
					+ ChildSlot->GetPosition() - Size * ChildSlot->GetAlignment();
				TestFalse(*FString::Printf(TEXT("Кнопка «КПК» не задевает %s на экране шириной %.0f"), *Child->GetName(), ScreenWidth),
					PdaBox.Intersect(FBox2D(Min, Min + Size)));
			}
		}
	}

	PdaTest::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
