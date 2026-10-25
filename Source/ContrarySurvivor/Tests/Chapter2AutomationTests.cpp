// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-проверки главы 2 «Вера», задание «Образцы из аномалий» (25.10.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Chapter2; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат:
//   - правила: урон в низине без аппарата и с пустым баллоном, воздух тратится только в маске,
//     детектор — ближе образец, сильнее сигнал и чаще писк;
//   - порядок задания: Вера молчит до сдачи ноутбука, затем сделка [212] → задание [214] →
//     взятие (аппарат выдан) → образцы видны и подбираются → после третьего задание готово;
//   - наёмник появляется ТОЛЬКО после третьего образца и только вне низины; его гибель
//     записывается; Вера принимает образцы после боя: [228] → [229] → [250] с фотографией →
//     [251] → [252] → экран конца главы;
//   - строка цели [216] дословно с числами;
//   - сохранение: аппарат, воздух, подобранные образцы, наёмник и конец главы переживают запись.
//
// Разговоры проверяются теми же вызовами, что делает окно (BuildTalkView / HandleTalkPrimary).
// Слот сохранения ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter), в конце удаляется.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Chapter2/AnomalySample.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Chapter2/MercenaryEncounter.h"
#include "ContrarySurvivor/Chapter2/PoisonZone.h"
#include "ContrarySurvivor/Chapter2/VeraNPC.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

static constexpr EAutomationTestFlags Chapter2TestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace Chapter2Test
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

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}

	static EQuestState StateOf(const APlayerCharacter* Player, FName QuestId)
	{
		const FQuest* Q = (Player && Player->GetQuests()) ? Player->GetQuests()->FindQuest(QuestId) : nullptr;
		return Q ? Q->State : EQuestState::NotStarted;
	}

	// Журнал, в котором ноутбук уже сдан квестовому учёному (с этого места говорит Вера).
	static void GiveLaptopTurnedIn(APlayerCharacter* Player)
	{
		FQuest Laptop;
		Laptop.QuestId = ContraryQuestIds::DeliverLaptopToScientists;
		Laptop.TurnInNpcTag = ScientistBaseTags::QuestScientist;
		Laptop.TargetCount = 0;
		Laptop.State = EQuestState::TurnedIn;
		Player->GetQuests()->RestoreQuests({ Laptop });
	}

	// Окружение «герой стоит здесь» для шага правил.
	static UChapter2Component::FEnv Env(bool bInZone, bool bNearZone = false, bool bInRefill = false, float SampleDist = -1.0f)
	{
		UChapter2Component::FEnv E;
		E.bInZone = bInZone;
		E.bNearZone = bNearZone || bInZone;
		E.bInRefill = bInRefill;
		E.NearestSampleDistance = SampleDist;
		return E;
	}
}

// ===========================================================================
// 1. Чистые правила: урон, воздух, детектор, наёмник.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2RulesTest, "ContrarySurvivor.Chapter2.Rules", Chapter2TestFlags)

bool FChapter2RulesTest::RunTest(const FString& Parameters)
{
	// Урон в низине.
	TestEqual(TEXT("Вне низины воздух безопасен"), Chapter2Rules::ZoneDamagePerSecond(false, false, 0.0f, 8.0f, 6.0f), 0.0f);
	TestEqual(TEXT("В низине без аппарата — урон каждую секунду"), Chapter2Rules::ZoneDamagePerSecond(true, false, 100.0f, 8.0f, 6.0f), 8.0f);
	TestEqual(TEXT("В низине в аппарате с воздухом — урона нет"), Chapter2Rules::ZoneDamagePerSecond(true, true, 10.0f, 8.0f, 6.0f), 0.0f);
	TestEqual(TEXT("В низине в аппарате без воздуха — тоже урон"), Chapter2Rules::ZoneDamagePerSecond(true, true, 0.0f, 8.0f, 6.0f), 6.0f);

	// Воздух.
	TestEqual(TEXT("Воздух тратится, пока аппарат надет"), Chapter2Rules::DrainAir(180.0f, true, 1.5f), 178.5f);
	TestEqual(TEXT("Без аппарата воздух не тратится"), Chapter2Rules::DrainAir(180.0f, false, 1.5f), 180.0f);
	TestEqual(TEXT("Воздух не уходит в минус"), Chapter2Rules::DrainAir(0.5f, true, 3.0f), 0.0f);

	// Детектор: ближе — сильнее сигнал и короче пауза между писками.
	const float Range = 3000.0f;
	const float Far = Chapter2Rules::SignalStrength(2800.0f, Range);
	const float Mid = Chapter2Rules::SignalStrength(1500.0f, Range);
	const float Near = Chapter2Rules::SignalStrength(100.0f, Range);
	TestTrue(TEXT("Ближе к образцу сигнал сильнее"), Near > Mid && Mid > Far && Far > 0.0f);
	TestEqual(TEXT("За дальностью сигнала нет"), Chapter2Rules::SignalStrength(3500.0f, Range), 0.0f);
	TestEqual(TEXT("Образца нет — сигнала нет"), Chapter2Rules::SignalStrength(-1.0f, Range), 0.0f);
	const float FarPause = Chapter2Rules::BeepInterval(Far, 0.15f, 1.6f);
	const float NearPause = Chapter2Rules::BeepInterval(Near, 0.15f, 1.6f);
	TestTrue(TEXT("Ближе к образцу детектор пищит чаще"), NearPause > 0.0f && NearPause < FarPause);
	TestTrue(TEXT("Без сигнала детектор молчит"), Chapter2Rules::BeepInterval(0.0f, 0.15f, 1.6f) < 0.0f);

	// Наёмник: только после третьего образца и только вне низины.
	TestFalse(TEXT("Два образца из трёх — наёмника нет"), Chapter2Rules::ShouldSpawnMercenary(2, 3, false, false));
	TestFalse(TEXT("Три образца, но герой ещё в низине — наёмника нет"), Chapter2Rules::ShouldSpawnMercenary(3, 3, false, true));
	TestTrue(TEXT("Три образца и герой вышел из низины — наёмник"), Chapter2Rules::ShouldSpawnMercenary(3, 3, false, false));
	TestFalse(TEXT("Наёмник побеждён — второй раз не появляется"), Chapter2Rules::ShouldSpawnMercenary(3, 3, true, false));

	// Настройки: решение лида — воздуха на 3 минуты.
	TestEqual(TEXT("В полном баллоне 180 секунд воздуха"), UChapter2Settings::Get()->AirCapacitySeconds, 180.0f);

	// Картинок главы ещё может не быть в проекте: пустое поле и отсутствующий файл — не падение,
	// а «картинки нет».
	const TCHAR* Expected[] = { UChapter2Settings::ExpectedMaskGlassPath, UChapter2Settings::ExpectedPhotoPath,
		UChapter2Settings::ExpectedAirIconPath, UChapter2Settings::ExpectedDetectorIconPath };
	for (const TCHAR* Path : Expected)
	{
		UTexture2D* Tex = UChapter2Settings::ResolveTexture(TSoftObjectPtr<UTexture2D>(), Path);
		if (!FPackageName::DoesPackageExist(Path))
		{
			TestNull(FString::Printf(TEXT("Файла %s нет — картинки нет, без падения"), Path), Tex);
		}
	}
	TestNull(TEXT("Нет ни поля, ни пути — картинки нет"),
		UChapter2Settings::ResolveTexture(TSoftObjectPtr<UTexture2D>(), TEXT("/Game/UI/Chapter2/T_NoSuchPicture")));
	return true;
}

// ===========================================================================
// 2. Низина в мире: внутри/у края, урон без аппарата, аппарат, воздух, заправка.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2ZoneAndAirTest, "ContrarySurvivor.Chapter2.ZoneDamageAndAir", Chapter2TestFlags)

bool FChapter2ZoneAndAirTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryChapter2ZoneQA");
	Chapter2Test::DeleteSlot(Slot);
	UWorld* World = Chapter2Test::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Chapter2Test::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	APoisonZone* Zone = Chapter2Test::Spawn<APoisonZone>(World, FVector(10000.0f, 0.0f, 0.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	if (!Player || !Zone || !C || !Stats)
	{
		AddError(TEXT("Не заспавнены герой или низина"));
		Chapter2Test::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	C->ResetForNewGame();
	const UChapter2Settings* S = UChapter2Settings::Get();

	// Геометрия низины: центр внутри, у края — рядом, далеко — снаружи.
	const FVector Extent = Zone->GetVolume()->GetScaledBoxExtent();
	TestTrue(TEXT("Центр низины — внутри"), Zone->IsInside(Zone->GetActorLocation()));
	const FVector NearEdge = Zone->GetActorLocation() + FVector(Extent.X + 300.0f, 0.0f, 0.0f);
	TestFalse(TEXT("За краем — снаружи"), Zone->IsInside(NearEdge));
	TestTrue(TEXT("У края расстояние до низины небольшое"), FMath::IsNearlyEqual(Zone->DistanceTo(NearEdge), 300.0f, 1.0f));
	Player->SetActorLocation(NearEdge);
	TestTrue(TEXT("Окружение: у края низины"), C->GatherEnv().bNearZone && !C->GatherEnv().bInZone);
	Player->SetActorLocation(Zone->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f));
	TestTrue(TEXT("Окружение: в низине"), C->GatherEnv().bInZone);

	// Без аппарата кнопки нет, в низине урон раз в секунду.
	Stats->SetHealth(Stats->GetMaxHealth());
	TestFalse(TEXT("Без аппарата кнопки аппарата нет"), (C->Step(0.1f, Chapter2Test::Env(true)), C->CanToggleMask()));
	const float H0 = Stats->GetHealth();
	C->Step(0.5f, Chapter2Test::Env(true));
	TestEqual(TEXT("Полсекунды в низине — урона ещё нет"), Stats->GetHealth(), H0);
	C->Step(0.6f, Chapter2Test::Env(true));
	TestEqual(TEXT("Секунда в низине без аппарата — урон"), Stats->GetHealth(), H0 - S->ZoneDamageNoMaskPerSecond);

	// Аппарат выдан: у края можно надеть, в низине урона нет, воздух тратится.
	C->GiveApparatus();
	TestEqual(TEXT("Выданный баллон полон"), C->GetAirSeconds(), S->AirCapacitySeconds);
	C->Step(0.1f, Chapter2Test::Env(false, true));
	TestTrue(TEXT("У края с аппаратом есть кнопка «Надеть»"), C->CanToggleMask());
	TestTrue(TEXT("Кнопка подписана [218]"), C->GetMaskActionText().EqualTo(S->PutOnMaskText));
	TestTrue(TEXT("Аппарат надевается"), C->ToggleMask());
	const float H1 = Stats->GetHealth();
	C->Step(2.0f, Chapter2Test::Env(true));
	TestEqual(TEXT("В аппарате с воздухом урона нет"), Stats->GetHealth(), H1);
	TestTrue(TEXT("Воздух тратится"), C->GetAirSeconds() < S->AirCapacitySeconds);
	TestFalse(TEXT("В низине снять аппарат нельзя"), C->CanToggleMask());

	// Воздух кончается — снова урон.
	C->Step(S->AirCapacitySeconds - 2.5f, Chapter2Test::Env(false, true)); // у края, без урона
	TestTrue(TEXT("Почти пустой баллон"), C->GetAirSeconds() > 0.0f && C->GetAirSeconds() < 1.0f);
	const float H2 = Stats->GetHealth();
	C->Step(1.0f, Chapter2Test::Env(true));
	TestEqual(TEXT("Воздух кончился"), C->GetAirSeconds(), 0.0f);
	TestEqual(TEXT("С пустым баллоном в низине — урон"), Stats->GetHealth(), H2 - S->ZoneDamageNoAirPerSecond);

	// Вне низины аппарат снимается ([219]); на базе учёных баллон заправляется.
	C->Step(0.1f, Chapter2Test::Env(false));
	TestTrue(TEXT("Вне низины кнопка подписана [219]"), C->GetMaskActionText().EqualTo(S->TakeOffMaskText));
	C->Step(0.1f, Chapter2Test::Env(false, false, /*bInRefill=*/true));
	TestEqual(TEXT("На базе учёных баллон полон"), C->GetAirSeconds(), S->AirCapacitySeconds);
	for (int32 i = 0; i < 20; ++i)
	{
		C->Step(0.1f, Chapter2Test::Env(false, false, /*bInRefill=*/true));
	}
	TestEqual(TEXT("На базе учёных в маске воздух не тратится"), C->GetAirSeconds(), S->AirCapacitySeconds);
	TestTrue(TEXT("Аппарат снимается вне низины"), C->ToggleMask());
	TestFalse(TEXT("Аппарат снят"), C->IsMaskOn());

	Chapter2Test::Destroy(World);
	Chapter2Test::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 3. Порядок задания целиком: Вера → образцы → наёмник → сдача → конец главы, и сохранение.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2QuestFlowTest, "ContrarySurvivor.Chapter2.QuestOrderAndMercenary", Chapter2TestFlags)

bool FChapter2QuestFlowTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryChapter2FlowQA");
	Chapter2Test::DeleteSlot(Slot);
	UWorld* World = Chapter2Test::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Chapter2Test::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AVeraNPC* Vera = Chapter2Test::Spawn<AVeraNPC>(World, FVector(500.0f, 0.0f, 100.0f));
	AScientistQuestNPC* Scientist = Chapter2Test::Spawn<AScientistQuestNPC>(World, FVector(800.0f, 0.0f, 100.0f));
	AMercenaryEncounter* Encounter = Chapter2Test::Spawn<AMercenaryEncounter>(World, FVector(3000.0f, 0.0f, 100.0f));
	TArray<AAnomalySample*> Samples;
	for (int32 i = 0; i < 3; ++i)
	{
		Samples.Add(Chapter2Test::Spawn<AAnomalySample>(World, FVector(10000.0f + i * 500.0f, 0.0f, 50.0f)));
	}
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Vera || !Scientist || !Encounter || !C || !Quests || Samples.Contains(nullptr))
	{
		AddError(TEXT("Не заспавнены герой, Вера, учёный, точка наёмника или образцы"));
		Chapter2Test::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	C->ResetForNewGame();
	Quests->RestoreQuests({});
	Encounter->SetMercenaryClassForTest(AEnemyCharacter::StaticClass());
	const FName QId = Chapter2Ids::SamplesQuest;

	// Учёный после ноутбука отсылает к Вере ([037], [038]).
	Chapter2Test::GiveLaptopTurnedIn(Player);
	FNpcTalkView View;
	Scientist->OnTalkOpened(Player);
	Scientist->BuildTalkView(Player, View);
	TestTrue(TEXT("Учёный после ноутбука говорит о Вере [038]"), View.Replica.ToString().Contains(TEXT("Вера")));
	TestFalse(TEXT("Сдача ноутбука больше не объявляет конец сюжета"), Scientist->ShouldAnnounceStoryEndOnClose());

	// До ноутбука Вера занята и задания не даёт.
	Quests->RestoreQuests({});
	Vera->OnTalkOpened(Player);
	Vera->BuildTalkView(Player, View);
	TestEqual(TEXT("До сдачи ноутбука Вера занята"), Vera->GetStage(), EVeraTalkStage::Idle);
	TestTrue(TEXT("До сдачи ноутбука у Веры нет кнопки"), View.PrimaryLabel.IsEmpty());
	TestNull(TEXT("До сдачи ноутбука задания в журнале нет"), Quests->FindQuest(QId));
	Vera->OnTalkClosed(Player);

	// После ноутбука: сделка [212] → задание [214] → взятие.
	Chapter2Test::GiveLaptopTurnedIn(Player);
	Vera->OnTalkOpened(Player);
	Vera->BuildTalkView(Player, View);
	TestEqual(TEXT("Первая встреча — сделка"), Vera->GetStage(), EVeraTalkStage::Deal);
	TestTrue(TEXT("[212] начинается со слов про ноутбук"), View.Replica.ToString().StartsWith(TEXT("Так это вы принесли ноутбук.")));
	TestEqual(TEXT("[213] на кнопке"), View.PrimaryLabel.ToString(), FString(TEXT("Договорились.")));
	TestEqual(TEXT("Задание предложено, но не взято"), Chapter2Test::StateOf(Player, QId), EQuestState::NotStarted);
	TestFalse(TEXT("Сделка не закрывает разговор"), Vera->HandleTalkPrimary(Player));
	Vera->BuildTalkView(Player, View);
	TestTrue(TEXT("[214] задание про низину"), View.Replica.ToString().StartsWith(TEXT("К востоку от лагеря есть отравленная низина")));
	TestEqual(TEXT("[215] на кнопке"), View.PrimaryLabel.ToString(), FString(TEXT("Принесу.")));

	// До взятия образцов не видно и подобрать их нельзя.
	C->RefreshSampleActors();
	TestFalse(TEXT("До взятия задания образцов не видно"), Samples[0]->IsSampleVisible());
	TestFalse(TEXT("До взятия задания образец не подбирается"), Samples[0]->TryCollect(Player));

	TestFalse(TEXT("Взятие не закрывает разговор"), Vera->HandleTalkPrimary(Player));
	TestEqual(TEXT("Задание взято"), Chapter2Test::StateOf(Player, QId), EQuestState::Active);
	TestTrue(TEXT("При взятии выдан дыхательный аппарат"), C->HasApparatus());
	Vera->BuildTalkView(Player, View);
	TestEqual(TEXT("Пока образцы не собраны — [227]"), Vera->GetStage(), EVeraTalkStage::Active);
	Vera->OnTalkClosed(Player);

	// Строка цели [216] дословно.
	if (const FQuest* InLog = Quests->FindQuest(QId))
	{
		const FText Line = QuestObjectiveText::BuildObjectives(*InLog, INVTEXT("{Objective} {Done} из {Total}"), INVTEXT(", "));
		TestEqual(TEXT("Строка цели [216]"), Line.ToString(), FString(TEXT("Найти в отравленной низине образцы (0 из 3)")));
		TestEqual(TEXT("[217] — куда нести собранное"), InLog->TurnInHintText.ToString(), FString(TEXT("Отнести образцы Вере")));
	}

	// Образцы видны и подбираются касанием; детектор слышит ближайший.
	C->RefreshSampleActors();
	TestTrue(TEXT("После взятия образцы видны"), Samples[0]->IsSampleVisible());
	C->Step(0.1f, Chapter2Test::Env(true, true, false, 400.0f));
	TestTrue(TEXT("Рядом с образцом детектор на экране"), C->IsDetectorShown());
	TestTrue(TEXT("Рядом с образцом есть сигнал"), C->GetSignalStrength() > 0.0f);

	// Чаще ближе: считаем писки за 10 секунд вдали и вблизи.
	auto CountBeeps = [&](float Dist)
	{
		const int32 Before = C->GetBeepCount();
		for (int32 i = 0; i < 100; ++i)
		{
			C->Step(0.1f, Chapter2Test::Env(false, true, false, Dist));
		}
		return C->GetBeepCount() - Before;
	};
	const int32 FarBeeps = CountBeeps(2700.0f);
	const int32 NearBeeps = CountBeeps(150.0f);
	TestTrue(TEXT("Вдали детектор пищит"), FarBeeps > 0);
	TestTrue(TEXT("Вблизи детектор пищит чаще, чем вдали"), NearBeeps > FarBeeps);

	TestTrue(TEXT("Первый образец подобран"), Samples[0]->TryCollect(Player));
	TestFalse(TEXT("Подобранный образец второй раз не считается"), Samples[0]->TryCollect(Player));
	TestFalse(TEXT("Подобранный образец спрятан"), Samples[0]->IsSampleVisible());
	TestTrue(TEXT("Второй образец подобран"), Samples[1]->TryCollect(Player));
	TestEqual(TEXT("Два из трёх"), C->GetSamplesDone(), 2);

	// После двух образцов наёмника нет, даже вне низины.
	C->Step(0.1f, Chapter2Test::Env(false));
	Encounter->UpdateEncounter(Player);
	TestNull(TEXT("Два образца — наёмника нет"), Encounter->GetMercenary());

	// Третий образец — в низине: наёмник ещё не появляется.
	C->Step(0.1f, Chapter2Test::Env(true));
	TestTrue(TEXT("Третий образец подобран"), Samples[2]->TryCollect(Player));
	TestEqual(TEXT("Три образца — задание готово к сдаче"), Chapter2Test::StateOf(Player, QId), EQuestState::Completed);
	Encounter->UpdateEncounter(Player);
	TestNull(TEXT("Герой ещё в низине — наёмника нет"), Encounter->GetMercenary());

	// Вера до боя образцы не принимает.
	Vera->OnTalkOpened(Player);
	TestEqual(TEXT("Наёмник жив — Вера образцы не принимает"), Vera->GetStage(), EVeraTalkStage::WaitMerc);
	Vera->OnTalkClosed(Player);

	// Вышел из низины — наёмник появляется, он сильнее бандита.
	C->Step(0.1f, Chapter2Test::Env(false));
	Encounter->UpdateEncounter(Player);
	AEnemyCharacter* Merc = Encounter->GetMercenary();
	if (TestNotNull(TEXT("На обратном пути появился наёмник"), Merc) && Merc->GetStats())
	{
		TestTrue(TEXT("У наёмника больше здоровья, чем у бандита (80)"), Merc->GetStats()->GetMaxHealth() > 80.0f);
		TestFalse(TEXT("До разговора бой не начат"), Encounter->IsFightStarted());

		// Разговор [241]–[244].
		Encounter->OnTalkOpened(Player);
		Encounter->BuildTalkView(Player, View);
		TestTrue(TEXT("[241] наёмник называет героя Седьмым"), View.Replica.ToString().Contains(TEXT("Седьмой")));
		TestEqual(TEXT("[242] на кнопке"), View.PrimaryLabel.ToString(), FString(TEXT("Я тебя не знаю.")));
		TestFalse(TEXT("Первый ответ не закрывает разговор"), Encounter->HandleTalkPrimary(Player));
		Encounter->BuildTalkView(Player, View);
		TestTrue(TEXT("[243] перед боем"), View.Replica.ToString().StartsWith(TEXT("Зато мы тебя знаем.")));
		TestEqual(TEXT("[244] закрывает разговор"), View.CloseLabel.ToString(), FString(TEXT("Попробуй.")));
		Encounter->OnTalkClosed(Player);
		TestTrue(TEXT("После разговора начинается бой"), Encounter->IsFightStarted());

		// Наёмник убит.
		Merc->GetStats()->ApplyDamage(10000.0f);
		Encounter->UpdateEncounter(Player);
		TestTrue(TEXT("Гибель наёмника записана"), C->IsMercenaryDefeated());
	}

	// Сдача Вере: [228] → [229] → [250] + фото → [251] → [252] → конец главы.
	Vera->OnTalkOpened(Player);
	Vera->BuildTalkView(Player, View);
	TestEqual(TEXT("После боя Вера принимает образцы"), Vera->GetStage(), EVeraTalkStage::Samples);
	TestTrue(TEXT("[228]"), View.Replica.ToString().StartsWith(TEXT("Три образца… Отлично.")));
	TestEqual(TEXT("[229] на кнопке"), View.PrimaryLabel.ToString(), FString(TEXT("По дороге сюда меня ждал человек Куратора. Он знал меня.")));
	TestFalse(TEXT("Сдача не закрывает разговор"), Vera->HandleTalkPrimary(Player));
	TestEqual(TEXT("Задание сдано"), Chapter2Test::StateOf(Player, QId), EQuestState::TurnedIn);
	Vera->BuildTalkView(Player, View);
	TestTrue(TEXT("[250] последний файл"), View.Replica.ToString().StartsWith(TEXT("Значит, они знают вас лучше")));
	TestTrue(TEXT("Фотография на экране"), C->IsPhotoShown());
	TestEqual(TEXT("[251] на кнопке"), View.PrimaryLabel.ToString(), FString(TEXT("Это же я. А рядом… ты?")));
	TestFalse(TEXT("Ответ героя не закрывает разговор"), Vera->HandleTalkPrimary(Player));
	Vera->BuildTalkView(Player, View);
	TestTrue(TEXT("[252] конец главы"), View.Replica.ToString().StartsWith(TEXT("Да. Мы были знакомы")));
	TestTrue(TEXT("На [252] у Веры нет основной кнопки"), View.PrimaryLabel.IsEmpty());
	TestFalse(TEXT("Конец главы ещё не показан до закрытия"), C->IsChapterEndShown());
	Vera->OnTalkClosed(Player);
	TestTrue(TEXT("Закрытие разговора показывает конец главы"), C->IsChapterEndShown());
	TestTrue(TEXT("Экран конца главы на экране"), C->GetChapterEndCardRemaining() > 0.0f);
	TestFalse(TEXT("Фотография убрана"), C->IsPhotoShown());
	Vera->OnTalkOpened(Player);
	TestEqual(TEXT("Следующий разговор — после главы"), Vera->GetStage(), EVeraTalkStage::After);
	Vera->OnTalkClosed(Player);

	// Сохранение: всё переживает запись и чтение.
	UContrarySaveGame* Save = NewObject<UContrarySaveGame>(GetTransientPackage());
	C->WriteToSave(Save);
	const float Air = C->GetAirSeconds();
	C->ResetForNewGame();
	TestFalse(TEXT("Новая игра: аппарата нет"), C->HasApparatus());
	TestFalse(TEXT("Новая игра: наёмник не побеждён"), C->IsMercenaryDefeated());
	C->RestoreFromSave(Save);
	TestTrue(TEXT("Аппарат пережил сохранение"), C->HasApparatus());
	TestEqual(TEXT("Воздух пережил сохранение"), C->GetAirSeconds(), Air);
	TestEqual(TEXT("Подобранные образцы пережили сохранение"), C->GetPickedCount(), 3);
	TestTrue(TEXT("Образец по имени помнится подобранным"), C->IsSamplePicked(Samples[1]->GetSampleId()));
	TestTrue(TEXT("Победа над наёмником пережила сохранение"), C->IsMercenaryDefeated());
	TestTrue(TEXT("Конец главы пережил сохранение"), C->IsChapterEndShown());

	Chapter2Test::Destroy(World);
	Chapter2Test::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 4. Сохранение середины задания и полный SaveGame героя (поля в слоте).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2SaveMidQuestTest, "ContrarySurvivor.Chapter2.SaveMidQuest", Chapter2TestFlags)

bool FChapter2SaveMidQuestTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryChapter2SaveQA");
	Chapter2Test::DeleteSlot(Slot);
	UWorld* World = Chapter2Test::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Chapter2Test::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	if (!Player || !C)
	{
		AddError(TEXT("Не заспавнен герой"));
		Chapter2Test::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	C->ResetForNewGame();

	C->GiveApparatus();
	C->Step(0.1f, Chapter2Test::Env(false, true));
	C->ToggleMask();
	C->Step(40.0f, Chapter2Test::Env(false, true));
	const float Air = C->GetAirSeconds();
	TestTrue(TEXT("Воздуха стало меньше"), Air < UChapter2Settings::Get()->AirCapacitySeconds);

	TestTrue(TEXT("Сохранение героя записано"), Player->SaveGame());
	const UContrarySaveGame* Saved = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (TestNotNull(TEXT("Слот читается"), Saved))
	{
		TestTrue(TEXT("В слоте: аппарат выдан"), Saved->bChapter2ApparatusGiven);
		TestTrue(TEXT("В слоте: аппарат надет"), Saved->bChapter2MaskOn);
		TestEqual(TEXT("В слоте: воздух"), Saved->Chapter2AirSeconds, Air);
		C->ResetForNewGame();
		C->RestoreFromSave(Saved);
		TestTrue(TEXT("После чтения аппарат надет"), C->IsMaskOn());
		TestEqual(TEXT("После чтения воздуха столько же"), C->GetAirSeconds(), Air);
	}

	Chapter2Test::Destroy(World);
	Chapter2Test::DeleteSlot(Slot);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
