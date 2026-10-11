// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты шлагбаума базы учёных (поручение Рината 10.10.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Barrier; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат:
//   - правило открытия во всех сочетаниях состояний двух заданий (ноутбук учёным и рация охранника);
//   - шлагбаум в мире: закрыт без героя и с пустым журналом, открывается по событию журнала при
//     взятии задания, закрывается после замены журнала пустым («Новая игра», кнопки сюжета меню
//     отладки), открыт сразу после загрузки журнала из сохранения; преграда не пускает только
//     персонажей и перестаёт влиять на навигацию, когда шлагбаум открыт; стрела поднимается;
//   - реплика охранника: героя с ноутбуком пропускает, своё задание про рацию при этом остаётся.
//
// Журнал заданий собирается в тесте руками (без выдачи предметов), поэтому таблицы предметов
// и заданий не читаются. Слот сохранения ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/QuestBarrier.h"
#include "ContrarySurvivor/Actors/QuestItemSpawner.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Actors/TalkerNPC.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags BarrierTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace BarrierTest
{
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

	// Кадр тестового мира (счётчик кадров двигаем сами — см. CombatAutomationTests.cpp).
	static void TickWorld(UWorld* World, float DeltaSeconds)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, DeltaSeconds);
	}

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}

	// Задание без целей, предметов и наград: берётся и сдаётся без таблицы предметов.
	static FQuest MakeQuest(FName QuestId, EQuestState State)
	{
		FQuest Quest;
		Quest.QuestId = QuestId;
		Quest.TargetCount = 0;
		Quest.RewardMoney = 0.0f;
		Quest.State = State;
		return Quest;
	}

	static FText GetTextField(const UObject* Object, const TCHAR* FieldName)
	{
		const FTextProperty* Prop = Object ? FindFProperty<FTextProperty>(Object->GetClass(), FieldName) : nullptr;
		return Prop ? Prop->GetPropertyValue_InContainer(Object) : FText::GetEmpty();
	}

	static FName GetNameField(const UObject* Object, const TCHAR* FieldName)
	{
		const FNameProperty* Prop = Object ? FindFProperty<FNameProperty>(Object->GetClass(), FieldName) : nullptr;
		return Prop ? Prop->GetPropertyValue_InContainer(Object) : NAME_None;
	}
}

// ===========================================================================
// 1. Правило открытия: все сочетания состояний двух заданий.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarrierOpenRuleTest,
	"ContrarySurvivor.Barrier.OpenRuleAllQuestCombinations", BarrierTestFlags)

bool FBarrierOpenRuleTest::RunTest(const FString& Parameters)
{
	const FName Laptop = ContraryQuestIds::DeliverLaptopToScientists;
	const FName Radio = ContraryQuestIds::FetchGuardRadio;

	// Идентификаторы — те самые, что у заданий старосты и охранника, и те, что стоят в полях
	// шлагбаума и раскладчика рации по умолчанию.
	TestEqual(TEXT("Третье задание старосты носит общий идентификатор"),
		GetDefault<AElderNPC>()->GetThirdQuest().QuestId, Laptop);
	TestEqual(TEXT("Задание охранника носит общий идентификатор"),
		GetDefault<AScientistGuardNPC>()->GetOfferedQuest().QuestId, Radio);
	TestEqual(TEXT("Шлагбаум по умолчанию открывается взятием задания про ноутбук"),
		BarrierTest::GetNameField(GetDefault<AQuestBarrier>(), TEXT("OpenWhenTakenQuestId")), Laptop);
	TestEqual(TEXT("Шлагбаум по умолчанию открывается сдачей задания охранника"),
		BarrierTest::GetNameField(GetDefault<AQuestBarrier>(), TEXT("OpenWhenTurnedInQuestId")), Radio);
	TestEqual(TEXT("Раскладчик рации по умолчанию смотрит на задание охранника"),
		BarrierTest::GetNameField(GetDefault<AQuestItemSpawner>(), TEXT("QuestId")), Radio);
	TestEqual(TEXT("Охранник по умолчанию смотрит на задание про ноутбук"),
		BarrierTest::GetNameField(GetDefault<AScientistGuardNPC>(), TEXT("LaptopQuestId")), Laptop);

	TestFalse(TEXT("Пустой журнал — закрыт"), AQuestBarrier::ShouldBeOpen({}, Laptop, Radio));

	// Пять положений каждого задания: нет в журнале и четыре состояния. Ожидание записано
	// таблицей, а не выражением из самого правила.
	struct FCase
	{
		const TCHAR* Name;
		bool bInJournal;
		EQuestState State;
		bool bLaptopOpens; // это положение задания про ноутбук открывает шлагбаум
		bool bRadioOpens;  // это положение задания охранника открывает шлагбаум
	};
	const FCase Cases[] = {
		{ TEXT("нет в журнале"), false, EQuestState::NotStarted, false, false },
		{ TEXT("только предложено"), true, EQuestState::NotStarted, false, false },
		{ TEXT("идёт"), true, EQuestState::Active, true, false },
		{ TEXT("выполнено"), true, EQuestState::Completed, true, false },
		{ TEXT("сдано"), true, EQuestState::TurnedIn, true, true },
	};

	int32 Checked = 0;
	for (const FCase& LaptopCase : Cases)
	{
		for (const FCase& RadioCase : Cases)
		{
			TArray<FQuest> Journal;
			// Постороннее сданное задание в журнале шлагбаум не открывает.
			Journal.Add(BarrierTest::MakeQuest(FName(TEXT("QA_OtherQuest")), EQuestState::TurnedIn));
			if (LaptopCase.bInJournal)
			{
				Journal.Add(BarrierTest::MakeQuest(Laptop, LaptopCase.State));
			}
			if (RadioCase.bInJournal)
			{
				Journal.Add(BarrierTest::MakeQuest(Radio, RadioCase.State));
			}
			const bool bExpected = LaptopCase.bLaptopOpens || RadioCase.bRadioOpens;
			TestEqual(FString::Printf(TEXT("Ноутбук: %s; рация: %s"), LaptopCase.Name, RadioCase.Name),
				AQuestBarrier::ShouldBeOpen(Journal, Laptop, Radio), bExpected);
			++Checked;
		}
	}
	TestEqual(TEXT("Проверены все двадцать пять сочетаний"), Checked, 25);

	// Пустые поля шлагбаума ничего не открывают, даже если в журнале есть задание без имени.
	const TArray<FQuest> Nameless = { BarrierTest::MakeQuest(NAME_None, EQuestState::TurnedIn) };
	TestFalse(TEXT("Пустые поля — закрыт"), AQuestBarrier::ShouldBeOpen(Nameless, NAME_None, NAME_None));

	return true;
}

// ===========================================================================
// 2. Шлагбаум в мире: следует за журналом заданий героя.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarrierFollowsJournalTest,
	"ContrarySurvivor.Barrier.FollowsHeroJournal", BarrierTestFlags)

bool FBarrierFollowsJournalTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryBarrierQA");
	BarrierTest::DeleteSlot(Slot);
	UWorld* World = BarrierTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FName Laptop = ContraryQuestIds::DeliverLaptopToScientists;
	const FName Radio = ContraryQuestIds::FetchGuardRadio;

	// Шлагбаум появляется РАНЬШЕ героя — как на уровне, где порядок появления не задан.
	AQuestBarrier* Barrier = BarrierTest::Spawn<AQuestBarrier>(World, FVector(5000.0f, 0.0f, 0.0f));
	if (!Barrier || !Barrier->GetBlocker() || !Barrier->GetBoomMesh())
	{
		AddError(TEXT("Не заспавнен шлагбаум"));
		BarrierTest::Destroy(World);
		return false;
	}
	const UBoxComponent* Blocker = Barrier->GetBlocker();

	auto ExpectClosed = [&](const TCHAR* When)
	{
		TestFalse(FString::Printf(TEXT("%s: шлагбаум закрыт"), When), Barrier->IsOpen());
		TestTrue(FString::Printf(TEXT("%s: преграда включена"), When),
			Blocker->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
		TestTrue(FString::Printf(TEXT("%s: преграда вырезает проезд из навигации"), When),
			Blocker->CanEverAffectNavigation() && Blocker->IsNavigationRelevant());
	};
	auto ExpectOpen = [&](const TCHAR* When)
	{
		TestTrue(FString::Printf(TEXT("%s: шлагбаум открыт"), When), Barrier->IsOpen());
		TestTrue(FString::Printf(TEXT("%s: преграда выключена"), When),
			Blocker->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestFalse(FString::Printf(TEXT("%s: преграда на навигацию не влияет"), When),
			Blocker->CanEverAffectNavigation() || Blocker->IsNavigationRelevant());
	};

	ExpectClosed(TEXT("Героя ещё нет"));
	TestTrue(TEXT("Преграда не пускает персонажей"),
		Blocker->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
	TestTrue(TEXT("Преграда пропускает обзор и пули"),
		Blocker->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore);
	TestTrue(TEXT("Преграда пропускает камеру"),
		Blocker->GetCollisionResponseToChannel(ECC_Camera) == ECR_Ignore);
	TestEqual(TEXT("Ширина преграды — ширина проезда по умолчанию"),
		Blocker->GetUnscaledBoxExtent().Y * 2.0, 500.0);

	// ⛔ Игрок с изолированным слотом.
	ASaveTestPlayerCharacter* Player = BarrierTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Quests)
	{
		AddError(TEXT("Не заспавнен игрок"));
		BarrierTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	// Герой появился: шлагбаум находит его очередной сверкой (в игре её зовёт таймер).
	Barrier->RefreshNow();
	ExpectClosed(TEXT("Журнал пуст"));

	// Обычное взятие задания про ноутбук. Дальше сверку руками НЕ зовём — только события журнала.
	Quests->OfferQuest(BarrierTest::MakeQuest(Laptop, EQuestState::NotStarted));
	ExpectClosed(TEXT("Задание про ноутбук только предложено"));

	const float TipZClosed = Barrier->GetBoomMesh()->GetComponentLocation().Z;
	TestTrue(TEXT("Задание про ноутбук берётся"), Quests->AcceptQuest(Laptop));
	ExpectOpen(TEXT("Задание про ноутбук взято"));
	TestTrue(TEXT("Стрела поднимается плавно, а не рывком"), Barrier->GetOpenAlpha() < 1.0f);
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		BarrierTest::TickWorld(World, 0.1f);
	}
	TestEqual(TEXT("Стрела поднялась полностью"), Barrier->GetOpenAlpha(), 1.0f);
	TestTrue(TEXT("Конец стрелы ушёл вверх"),
		Barrier->GetBoomMesh()->GetComponentLocation().Z > TipZClosed + 100.0);

	// «Новая игра» и сброс: журнал заменён пустым — событий по отдельным заданиям нет вовсе.
	Quests->RestoreQuests(TArray<FQuest>());
	ExpectClosed(TEXT("Журнал заменён пустым"));
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		BarrierTest::TickWorld(World, 0.1f);
	}
	TestEqual(TEXT("Стрела опустилась полностью"), Barrier->GetOpenAlpha(), 0.0f);
	TestTrue(TEXT("Конец стрелы вернулся на место"),
		FMath::IsNearlyEqual(Barrier->GetBoomMesh()->GetComponentLocation().Z, TipZClosed, 0.5));

	// Загрузка сохранения, где ноутбук уже сдан учёному.
	Quests->RestoreQuests({ BarrierTest::MakeQuest(Laptop, EQuestState::TurnedIn) });
	ExpectOpen(TEXT("Загружен журнал со сданным ноутбуком"));

	// Кнопка сюжета меню отладки: журнал заменён на «первое задание старосты сдано».
	Quests->RestoreQuests({ BarrierTest::MakeQuest(GetDefault<AElderNPC>()->GetOfferedQuest().QuestId, EQuestState::TurnedIn) });
	ExpectClosed(TEXT("Журнал возвращён к началу сюжета"));

	// Задание охранника: открывает только сдача.
	Quests->OfferQuest(BarrierTest::MakeQuest(Radio, EQuestState::NotStarted));
	ExpectClosed(TEXT("Задание охранника предложено"));
	TestTrue(TEXT("Задание охранника берётся"), Quests->AcceptQuest(Radio));
	ExpectClosed(TEXT("Задание охранника взято и готово к сдаче"));
	TestTrue(TEXT("Задание охранника сдаётся"), Quests->TurnInQuest(Radio));
	ExpectOpen(TEXT("Задание охранника сдано"));

	// Шлагбаум, появившийся при уже открывающем журнале (загрузка уровня после сохранения):
	// открыт сразу, стрела уже поднята.
	AQuestBarrier* Late = BarrierTest::Spawn<AQuestBarrier>(World, FVector(9000.0f, 0.0f, 0.0f));
	if (TestNotNull(TEXT("Второй шлагбаум заспавнен"), Late))
	{
		TestTrue(TEXT("Появившийся позже шлагбаум открыт сразу"), Late->IsOpen());
		TestEqual(TEXT("У появившегося позже стрела уже поднята"), Late->GetOpenAlpha(), 1.0f);
		TestTrue(TEXT("У появившегося позже преграда выключена"),
			Late->GetBlocker() && Late->GetBlocker()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	}

	BarrierTest::Destroy(World);
	BarrierTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 3. Охранник: героя с ноутбуком пропускает, своё задание про рацию остаётся.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarrierGuardLaptopLineTest,
	"ContrarySurvivor.Barrier.GuardLetsLaptopCarrierThrough", BarrierTestFlags)

bool FBarrierGuardLaptopLineTest::RunTest(const FString& Parameters)
{
	const FName Laptop = ContraryQuestIds::DeliverLaptopToScientists;

	// Чистое правило «несёт ноутбук».
	auto Carrying = [Laptop](EQuestState State)
	{
		return AScientistGuardNPC::IsCarryingLaptop({ BarrierTest::MakeQuest(Laptop, State) }, Laptop);
	};
	TestFalse(TEXT("Журнал пуст — не несёт"), AScientistGuardNPC::IsCarryingLaptop({}, Laptop));
	TestFalse(TEXT("Задание только предложено — не несёт"), Carrying(EQuestState::NotStarted));
	TestTrue(TEXT("Задание идёт — несёт"), Carrying(EQuestState::Active));
	TestTrue(TEXT("Задание готово к сдаче — несёт"), Carrying(EQuestState::Completed));
	TestFalse(TEXT("Ноутбук сдан — уже не несёт"), Carrying(EQuestState::TurnedIn));
	TestFalse(TEXT("Чужое идущее задание — не несёт"),
		AScientistGuardNPC::IsCarryingLaptop({ BarrierTest::MakeQuest(FName(TEXT("QA_OtherQuest")), EQuestState::Active) }, Laptop));

	const FString Slot = TEXT("ContraryBarrierGuardQA");
	BarrierTest::DeleteSlot(Slot);
	UWorld* World = BarrierTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	ASaveTestPlayerCharacter* Player = BarrierTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AScientistGuardNPC* Guard = BarrierTest::Spawn<AScientistGuardNPC>(World, FVector(3000.0f, 0.0f, 100.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Guard || !Quests)
	{
		AddError(TEXT("Не заспавнены игрок или охранник"));
		BarrierTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	const FQuest GuardQuest = Guard->GetOfferedQuest();
	const FName Radio = GuardQuest.QuestId;
	const FString Pass = BarrierTest::GetTextField(Guard, TEXT("LaptopPassText")).ToString();
	const FString PassOffer = BarrierTest::GetTextField(Guard, TEXT("LaptopPassOfferText")).ToString();
	const FString ActiveLine = BarrierTest::GetTextField(Guard, TEXT("QuestActiveText")).ToString();
	const FString CompletedLine = BarrierTest::GetTextField(Guard, TEXT("QuestCompletedText")).ToString();
	const FString TurnedInLine = BarrierTest::GetTextField(Guard, TEXT("QuestTurnedInText")).ToString();
	TestFalse(TEXT("У охранника есть реплика пропуска"), Pass.IsEmpty());
	TestFalse(TEXT("У охранника есть просьба про рацию после пропуска"), PassOffer.IsEmpty());

	// Журнал героя: задание про ноутбук в заданном состоянии и задание охранника в заданном.
	auto SetJournal = [&](bool bLaptopInJournal, EQuestState LaptopState, bool bRadioInJournal, EQuestState RadioState)
	{
		TArray<FQuest> Journal;
		// Староста за героя поручился (задание на бандитов сдано) — иначе охранник о деле не говорит.
		Journal.Add(BarrierTest::MakeQuest(ContraryQuestIds::ClearBanditBase, EQuestState::TurnedIn));
		if (bLaptopInJournal)
		{
			Journal.Add(BarrierTest::MakeQuest(Laptop, LaptopState));
		}
		if (bRadioInJournal)
		{
			FQuest Own = GuardQuest;
			Own.State = RadioState;
			Journal.Add(Own);
		}
		Quests->RestoreQuests(Journal);
	};
	FNpcTalkView View;
	auto Talk = [&]()
	{
		Guard->OnTalkOpened(Player);
		Guard->BuildTalkView(Player, View);
	};

	// Без ноутбука — всё как раньше.
	SetJournal(false, EQuestState::NotStarted, false, EQuestState::NotStarted);
	Talk();
	TestEqual(TEXT("Без ноутбука охранник говорит описание задания"), View.Replica.ToString(), GuardQuest.Description.ToString());
	TestFalse(TEXT("Без ноутбука охранник предлагает задание"), View.PrimaryLabel.IsEmpty());

	// Задание про ноутбук только предложено старостой — ноутбука у героя ещё нет.
	SetJournal(true, EQuestState::NotStarted, false, EQuestState::NotStarted);
	Talk();
	TestEqual(TEXT("Ноутбук ещё не взят — охранник о нём молчит"), View.Replica.ToString(), GuardQuest.Description.ToString());

	// Герой несёт ноутбук, задание охранника не взято: пропускает и просит про рацию.
	SetJournal(true, EQuestState::Completed, false, EQuestState::NotStarted);
	Talk();
	TestTrue(TEXT("С ноутбуком охранник начинает с пропуска"), View.Replica.ToString().StartsWith(Pass));
	TestTrue(TEXT("С ноутбуком охранник всё равно просит про рацию"), View.Replica.ToString().EndsWith(PassOffer));
	TestFalse(TEXT("С ноутбуком задание охранника по-прежнему берётся кнопкой"), View.PrimaryLabel.IsEmpty());
	TestFalse(TEXT("Взятие задания разговор не закрывает"), Guard->HandleTalkPrimary(Player));
	const FQuest* OwnInLog = Quests->FindQuest(Radio);
	TestTrue(TEXT("Задание охранника взято при ноутбуке в сумке"),
		OwnInLog && OwnInLog->State == EQuestState::Active);

	// Несёт ноутбук, рацию ещё ищет.
	Guard->BuildTalkView(Player, View);
	TestTrue(TEXT("Рацию ищет: сначала пропуск"), View.Replica.ToString().StartsWith(Pass));
	TestTrue(TEXT("Рацию ищет: затем обычная реплика про рацию"), View.Replica.ToString().EndsWith(ActiveLine));
	TestTrue(TEXT("Рацию ищет: основной кнопки нет"), View.PrimaryLabel.IsEmpty());

	// Несёт ноутбук и рацию.
	SetJournal(true, EQuestState::Active, true, EQuestState::Completed);
	Talk();
	TestTrue(TEXT("С рацией: сначала пропуск"), View.Replica.ToString().StartsWith(Pass));
	TestTrue(TEXT("С рацией: затем обычная реплика сдачи"), View.Replica.ToString().EndsWith(CompletedLine));
	TestFalse(TEXT("С рацией: кнопка сдачи на месте"), View.PrimaryLabel.IsEmpty());

	// Рация сдана: его обычная реплика и так пропускает — второй раз не повторяем.
	SetJournal(true, EQuestState::Active, true, EQuestState::TurnedIn);
	Talk();
	TestEqual(TEXT("Рация сдана: реплика прежняя"), View.Replica.ToString(), TurnedInLine);

	// Ноутбук уже у учёных: охранник снова говорит только о своём.
	SetJournal(true, EQuestState::TurnedIn, false, EQuestState::NotStarted);
	Talk();
	TestEqual(TEXT("Ноутбук сдан: охранник говорит описание задания"), View.Replica.ToString(), GuardQuest.Description.ToString());

	// Пустая реплика пропуска выключает ветку целиком.
	if (FTextProperty* PassProp = FindFProperty<FTextProperty>(Guard->GetClass(), TEXT("LaptopPassText")))
	{
		PassProp->SetPropertyValue_InContainer(Guard, FText::GetEmpty());
		SetJournal(true, EQuestState::Active, false, EQuestState::NotStarted);
		Talk();
		TestEqual(TEXT("Реплика пропуска стёрта — охранник говорит описание задания"),
			View.Replica.ToString(), GuardQuest.Description.ToString());
	}
	else
	{
		AddError(TEXT("У охранника нет поля реплики пропуска"));
	}

	BarrierTest::Destroy(World);
	BarrierTest::DeleteSlot(Slot);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
