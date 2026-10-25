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
// Правки Рината 25.10.2026 вечер.
#include "ContrarySurvivor/Chapter2/Chapter2ScreenWidget.h"
#include "ContrarySurvivor/Chapter2/ScientistGearItem.h"
#include "ContrarySurvivor/Debug/QADebug.h"           // CONTRARY_WITH_QA_CHEATS
#include "ContrarySurvivor/Debug/ContraryDebugMenu.h" // точки главы 2 в меню отладки
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/UI/PlayerStatsWidget.h"
#include "ContrarySurvivor/UI/QuestTargets.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "UInventoryComponent.h"
#include "AMasterInventoryItem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"

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
		TestTrue(TEXT("[241] наёмник называет героя Сычом"), View.Replica.ToString().StartsWith(TEXT("Ну наконец-то, Сыч.")));
		TestFalse(TEXT("[241] прежнего «Седьмой» больше нет"), View.Replica.ToString().Contains(TEXT("Седьмой")));
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
	TestTrue(TEXT("[252] конец главы"), View.Replica.ToString().StartsWith(TEXT("Да, это я. Мы давно знакомы, только ты этого не помнишь.")));
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
	// Правка 25.10 вечер: при сдаче задания Вера забирает аппарат и снаряжение обратно.
	TestFalse(TEXT("Аппарат возвращён Вере — и в сохранении тоже"), C->HasApparatus());
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

// ===========================================================================
// 5. Правки Рината 25.10.2026 вечер: чистые правила — локатор, стрелка, фотография, вспышка
//    здоровья, тексты [230], [235], [236], [253], шкала воздуха выше.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2FixesRulesTest, "ContrarySurvivor.Chapter2.Fixes2510.Rules", Chapter2TestFlags)

bool FChapter2FixesRulesTest::RunTest(const FString& Parameters)
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	auto Same = [](const FVector2D& A, const FVector2D& B) { return A.Equals(B, 0.01); };

	// Локатор: верх круга — верх экрана игры (камера сверху, смотрит по своему рысканию).
	TestTrue(TEXT("Камера смотрит на +X: образец впереди — вверху локатора"),
		Same(Chapter2Rules::LocatorScreenDir(FVector2D(1000.0, 0.0), 0.0f), FVector2D(0.0, -1.0)));
	TestTrue(TEXT("Камера смотрит на +X: образец по +Y — справа"),
		Same(Chapter2Rules::LocatorScreenDir(FVector2D(0.0, 1000.0), 0.0f), FVector2D(1.0, 0.0)));
	TestTrue(TEXT("Камера повёрнута на 90°: образец по +Y — вверху"),
		Same(Chapter2Rules::LocatorScreenDir(FVector2D(0.0, 1000.0), 90.0f), FVector2D(0.0, -1.0)));
	TestTrue(TEXT("Камера повёрнута на 90°: образец по +X — слева"),
		Same(Chapter2Rules::LocatorScreenDir(FVector2D(1000.0, 0.0), 90.0f), FVector2D(-1.0, 0.0)));
	TestTrue(TEXT("Камера повёрнута на 180°: образец по +X — внизу"),
		Same(Chapter2Rules::LocatorScreenDir(FVector2D(1000.0, 0.0), 180.0f), FVector2D(0.0, 1.0)));
	TestTrue(TEXT("Образец под ногами — направление вверх, без деления на ноль"),
		Same(Chapter2Rules::LocatorScreenDir(FVector2D::ZeroVector, 37.0f), FVector2D(0.0, -1.0)));

	// Когда локатор на экране.
	TestTrue(TEXT("Задание идёт, герой в низине — локатор"), Chapter2Rules::ShouldShowLocator(true, 3, true, 0.0f, 1500.0f));
	TestTrue(TEXT("Рядом с низиной (ближе порога) — локатор"), Chapter2Rules::ShouldShowLocator(true, 3, false, 1200.0f, 1500.0f));
	TestFalse(TEXT("Дальше порога от низины — обычный стик"), Chapter2Rules::ShouldShowLocator(true, 3, false, 1600.0f, 1500.0f));
	TestFalse(TEXT("Низины на уровне нет — обычный стик"), Chapter2Rules::ShouldShowLocator(true, 3, false, -1.0f, 1500.0f));
	TestFalse(TEXT("Задание не взято — обычный стик"), Chapter2Rules::ShouldShowLocator(false, 3, true, 0.0f, 1500.0f));
	TestFalse(TEXT("Все образцы собраны — обычный стик"), Chapter2Rules::ShouldShowLocator(true, 0, true, 0.0f, 1500.0f));

	// Стрелка-подсказка: по умолчанию через 60 секунд без находки.
	TestEqual(TEXT("Стрелка по умолчанию через 60 с"), S->LocatorHintArrowDelay, 60.0f);
	TestFalse(TEXT("До срока стрелки нет"), Chapter2Rules::ShouldShowHintArrow(59.9f, 60.0f));
	TestTrue(TEXT("В срок стрелка появляется"), Chapter2Rules::ShouldShowHintArrow(60.0f, 60.0f));
	TestTrue(TEXT("Стрелка полупрозрачная"), S->LocatorArrowOpacity > 0.0f && S->LocatorArrowOpacity < 1.0f);
	TestTrue(TEXT("Стик-локатор чуть больше обычного"), S->LocatorStickScale > 1.0f && S->LocatorStickScale <= 1.5f);

	// Фотография не заходит на окно разговора ни на ПК (16:9), ни на вытянутом телефоне (20:9).
	struct FPhotoCase { const TCHAR* Name; FVector2D Screen; FBox2D Dialog; };
	const FVector2D Pc(1920.0, 1080.0);
	const FVector2D Phone(2400.0, 1080.0);
	const FPhotoCase Cases[] = {
		{ TEXT("ПК, окно разговора снизу"), Pc, FBox2D(FVector2D(192.0, 590.0), FVector2D(1728.0, 1080.0)) },
		{ TEXT("Телефон, окно разговора снизу"), Phone, FBox2D(FVector2D(240.0, 590.0), FVector2D(2160.0, 1080.0)) },
		{ TEXT("ПК, высокое окно разговора по центру"), Pc, FBox2D(FVector2D(560.0, 120.0), FVector2D(1360.0, 1040.0)) },
		{ TEXT("Телефон, окно разговора справа"), Phone, FBox2D(FVector2D(1100.0, 60.0), FVector2D(2400.0, 1060.0)) } };
	for (const FPhotoCase& Case : Cases)
	{
		const FBox2D Fit = Chapter2Rules::FitPhoto(Case.Screen, Case.Dialog, 1.5f, S->PhotoSize, S->PhotoMargin);
		if (!TestTrue(FString::Printf(TEXT("%s: фотографии нашлось место"), Case.Name), Fit.bIsValid))
		{
			continue;
		}
		const bool bApart = Fit.Max.X <= Case.Dialog.Min.X + 0.01 || Fit.Min.X >= Case.Dialog.Max.X - 0.01
			|| Fit.Max.Y <= Case.Dialog.Min.Y + 0.01 || Fit.Min.Y >= Case.Dialog.Max.Y - 0.01;
		TestTrue(FString::Printf(TEXT("%s: фотография не заходит на реплику и кнопки"), Case.Name), bApart);
		TestTrue(FString::Printf(TEXT("%s: фотография целиком на экране"), Case.Name),
			Fit.Min.X >= -0.01 && Fit.Min.Y >= -0.01 && Fit.Max.X <= Case.Screen.X + 0.01 && Fit.Max.Y <= Case.Screen.Y + 0.01);
		const FVector2D Size = Fit.GetSize();
		TestTrue(FString::Printf(TEXT("%s: пропорции фотографии сохранены"), Case.Name),
			Size.Y > 1.0 && FMath::IsNearlyEqual(Size.X / Size.Y, 1.5, 0.01));
		TestTrue(FString::Printf(TEXT("%s: фотография не больше заданного"), Case.Name),
			Size.X <= S->PhotoSize.X + 0.01 && Size.Y <= S->PhotoSize.Y + 0.01);
	}

	// Вспышка здоровья: любое уменьшение.
	TestTrue(TEXT("Здоровье уменьшилось — вспышка"), UPlayerStatsWidget::IsHealthLoss(100.0f, 95.0f));
	TestFalse(TEXT("Здоровье не менялось — вспышки нет"), UPlayerStatsWidget::IsHealthLoss(100.0f, 100.0f));
	TestFalse(TEXT("Лечение — вспышки нет"), UPlayerStatsWidget::IsHealthLoss(50.0f, 60.0f));
	TestFalse(TEXT("Первое значение — вспышки нет"), UPlayerStatsWidget::IsHealthLoss(-1.0f, 50.0f));
	if (UPlayerStatsWidget* Stats = NewObject<UPlayerStatsWidget>(GetTransientPackage()))
	{
		Stats->NoteHealthForDamageFlash(100.0f);
		TestFalse(TEXT("Полоска здоровья в покое"), Stats->IsHealthDamageFlashing());
		Stats->NoteHealthForDamageFlash(97.0f);
		TestTrue(TEXT("Урон — полоска здоровья вспыхивает"), Stats->IsHealthDamageFlashing());
		TestTrue(TEXT("Вспышка короткая (до полсекунды)"), Stats->HealthDamageFlashDuration > 0.0f && Stats->HealthDamageFlashDuration <= 0.5f);
	}

	// Тексты дословно из РЕПЛИКИ.txt.
	TestEqual(TEXT("[230]"), S->TalkToVeraText.ToString(), FString(TEXT("Поговорить с Верой")));
	TestEqual(TEXT("[231]"), S->AirTankNameText.ToString(), FString(TEXT("Баллон с воздухом")));
	TestEqual(TEXT("[233]"), S->DetectorNameText.ToString(), FString(TEXT("Детектор")));
	TestEqual(TEXT("[235]"), S->StoryItemLockedText.ToString(), FString(TEXT("Это снаряжение учёных: его нельзя продать или выбросить.")));
	TestTrue(TEXT("[236] начало"), S->SamplesGuideText.ToString().StartsWith(TEXT("Отравленная низина лежит к востоку от лагеря учёных.")));
	TestTrue(TEXT("[236] про детектор"), S->SamplesGuideText.ToString().Contains(TEXT("В низине левый стик превращается в детектор.")));
	TestEqual(TEXT("[253]"), S->ChapterEndText.ToString(), FString(TEXT("Конец первой главы. Продолжение следует…")));
	TestEqual(TEXT("[225] надпись над локатором"), S->DetectorLabelText.ToString(), FString(TEXT("Детектор")));

	// «Интерфейс балона подними чуть выше»: было 170 точек от верха экрана.
	TestTrue(TEXT("Шкала воздуха выше прежнего"), S->BarsOffset.Y < 170.0f);

	// Стекло маски нажатий не ловит: слой целиком пропускает касания к кнопкам.
	UChapter2ScreenWidget* Glass = NewObject<UChapter2ScreenWidget>(GetTransientPackage());
	if (Glass)
	{
		Glass->WidgetTree = NewObject<UWidgetTree>(Glass, TEXT("WidgetTree"));
		Glass->InitLayer(EChapter2ScreenLayer::MaskGlass);
		const UWidget* Root = Glass->GetRootForTest();
		TestTrue(TEXT("Корень слоя стекла пропускает касания"),
			Root && Root->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
		const UImage* GlassImage = Glass->GetGlassImageForTest();
		TestTrue(TEXT("Стекло не перехватывает касания"),
			GlassImage && GlassImage->GetVisibility() != ESlateVisibility::Visible);
	}
	return true;
}

// ===========================================================================
// 6. Правки Рината 25.10.2026 вечер: цель «Поговорить с Верой» [230] с меткой на Вере;
//    снаряжение учёных в сумке — нельзя продать и выбросить, выдаётся и старому сохранению,
//    Вера забирает его при сдаче; локатор и стрелка-подсказка по времени.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2TalkAndGearTest, "ContrarySurvivor.Chapter2.Fixes2510.TalkToVeraAndGear", Chapter2TestFlags)

bool FChapter2TalkAndGearTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryChapter2GearQA");
	Chapter2Test::DeleteSlot(Slot);
	UWorld* World = Chapter2Test::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Chapter2Test::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AVeraNPC* Vera = Chapter2Test::Spawn<AVeraNPC>(World, FVector(500.0f, 0.0f, 100.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Player || !Vera || !C || !Quests || !Inventory)
	{
		AddError(TEXT("Не заспавнены герой или Вера"));
		Chapter2Test::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	C->ResetForNewGame();
	Quests->RestoreQuests({});
	const UChapter2Settings* S = UChapter2Settings::Get();
	const FName QId = Chapter2Ids::SamplesQuest;

	auto FindGear = [Inventory](const TCHAR* Key) -> AMasterInventoryItem*
	{
		for (AMasterInventoryItem* Item : Inventory->GetInventoryItems())
		{
			if (IsValid(Item) && Item->ItemName.Equals(Key, ESearchCase::CaseSensitive))
			{
				return Item;
			}
		}
		return nullptr;
	};

	// --- Цель [230] ---
	C->SyncStoryState();
	TestNull(TEXT("До сдачи ноутбука цели «Поговорить с Верой» нет"), Quests->FindQuest(Chapter2Ids::TalkToVeraQuest));
	Chapter2Test::GiveLaptopTurnedIn(Player);
	C->SyncStoryState();
	C->SyncStoryState(); // второй раз ничего не добавляет
	const FQuest* Talk = Quests->FindQuest(Chapter2Ids::TalkToVeraQuest);
	if (TestNotNull(TEXT("Ноутбук сдан учёному — в списке заданий «Поговорить с Верой»"), Talk))
	{
		TestEqual(TEXT("Цель [230] активна"), Talk->State, EQuestState::Active);
		TestEqual(TEXT("Название цели — [230]"), Talk->Title.ToString(), FString(TEXT("Поговорить с Верой")));
		TestEqual(TEXT("Цель — основное задание (оранжевая метка)"), Talk->Kind, EQuestKind::Main);
		TestTrue(TEXT("Метка цели стоит на Вере (экран и карта смартфона)"), QuestTargets::FindTargetActor(World, *Talk) == Vera);
		TestEqual(TEXT("Подпись метки — [230]"), QuestTargets::BuildMarkerLabel(*Talk, TEXT("Сдать: ")), FString(TEXT("Поговорить с Верой")));
		const FText Line = QuestObjectiveText::BuildObjectives(*Talk, INVTEXT("{Objective} {Done} из {Total}"), INVTEXT(", "));
		TestEqual(TEXT("Строка цели на экране — [230]"), Line.ToString(), FString(TEXT("Поговорить с Верой")));
	}
	int32 TalkCount = 0;
	for (const FQuest& Q : Quests->GetQuests())
	{
		TalkCount += (Q.QuestId == Chapter2Ids::TalkToVeraQuest) ? 1 : 0;
	}
	TestEqual(TEXT("Цель «Поговорить с Верой» в журнале одна"), TalkCount, 1);
	TestEqual(TEXT("До взятия задания снаряжения нет"), C->CountGearInBag(), 0);

	// --- Вера выдаёт задание и снаряжение ---
	FNpcTalkView View;
	Vera->OnTalkOpened(Player);
	Vera->BuildTalkView(Player, View);
	Vera->HandleTalkPrimary(Player); // [213] сделка
	Vera->BuildTalkView(Player, View);
	Vera->HandleTalkPrimary(Player); // [215] взять
	Vera->OnTalkClosed(Player);
	TestEqual(TEXT("Задание взято"), Chapter2Test::StateOf(Player, QId), EQuestState::Active);
	TestNull(TEXT("Взято задание — цель «Поговорить с Верой» ушла"), Quests->FindQuest(Chapter2Ids::TalkToVeraQuest));
	TestEqual(TEXT("В сумке баллон и детектор"), C->CountGearInBag(), 2);
	AMasterInventoryItem* Air = FindGear(Chapter2Ids::AirTankKey);
	AMasterInventoryItem* Detector = FindGear(Chapter2Ids::DetectorKey);
	if (!TestNotNull(TEXT("Баллон в сумке"), Air) || !TestNotNull(TEXT("Детектор в сумке"), Detector))
	{
		Chapter2Test::Destroy(World);
		Chapter2Test::DeleteSlot(Slot);
		return false;
	}
	TestTrue(TEXT("Баллон — сюжетный предмет"), Air->bStoryItem);
	TestTrue(TEXT("Детектор — сюжетный предмет"), Detector->bStoryItem);
	TestEqual(TEXT("[231] название баллона"), Air->ItemDisplayText.ToString(), FString(TEXT("Баллон с воздухом")));
	TestEqual(TEXT("[232] описание баллона"), Air->ItemDescription, S->AirTankDescriptionText.ToString());
	TestEqual(TEXT("[233] название детектора"), Detector->ItemDisplayText.ToString(), FString(TEXT("Детектор")));
	TestEqual(TEXT("[234] описание детектора"), Detector->ItemDescription, S->DetectorDescriptionText.ToString());
	TestTrue(TEXT("Значок баллона — T_IconAir"), Air->ItemIcon.ToSoftObjectPath().ToString().Contains(TEXT("T_IconAir")));
	TestTrue(TEXT("Значок детектора — T_IconDetector"), Detector->ItemIcon.ToSoftObjectPath().ToString().Contains(TEXT("T_IconDetector")));

	// --- Продать и выбросить нельзя ---
	const float MoneyBefore = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
	Player->Inv_DropItem(Air);
	TestTrue(TEXT("Баллон не выбрасывается"), Inventory->GetInventoryItems().Contains(Air));
	Player->Shop_SellItem(Air, 50.0f);
	Player->Shop_SellItemQty(Detector, 50.0f, 1);
	TestTrue(TEXT("Баллон не продаётся"), Inventory->GetInventoryItems().Contains(Air));
	TestTrue(TEXT("Детектор не продаётся"), Inventory->GetInventoryItems().Contains(Detector));
	TestEqual(TEXT("Денег не прибавилось"), Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f, MoneyBefore);
	FText Reason;
	TestFalse(TEXT("Торговец снаряжение не берёт"), GetDefault<AMasterTrader>()->CanBuyFromPlayer(Air, Player, Reason));
	TestEqual(TEXT("Торговец объясняет словами [235]"), Reason.ToString(), S->StoryItemLockedText.ToString());
	TestTrue(TEXT("Отказ для сюжетного предмета"), Player->RefuseStoryItem(Detector));
	TestFalse(TEXT("Пустой предмет — не сюжетный"), Player->RefuseStoryItem(nullptr));

	// --- Сохранение, записанное до появления предметов: задание взято, а в сумке пусто ---
	for (AMasterInventoryItem* Item : { Air, Detector })
	{
		Inventory->RemoveItem(Item);
		Item->Destroy();
	}
	TestEqual(TEXT("Как в старом сохранении — сумка без снаряжения"), C->CountGearInBag(), 0);
	C->Step(0.1f, Chapter2Test::Env(false));
	TestEqual(TEXT("Снаряжение выдано само"), C->CountGearInBag(), 2);

	// --- Локатор и стрелка-подсказка ---
	UChapter2Component::FEnv Near = Chapter2Test::Env(false, /*bNearZone=*/true, false, 600.0f);
	Near.ZoneDistance = 100.0f;
	Near.HeroLocation = FVector::ZeroVector;
	Near.SampleLocations = { FVector(600.0f, 0.0f, 0.0f), FVector(0.0f, 2000.0f, 0.0f) };
	C->Step(1.0f, Near);
	TestTrue(TEXT("У края низины стик стал локатором"), C->IsDetectorShown());
	TestFalse(TEXT("Сразу стрелки нет"), C->IsHintArrowShown());
	C->Step(FMath::Max(0.0f, S->LocatorHintArrowDelay - 2.0f), Near);
	TestFalse(TEXT("За секунду до срока стрелки нет"), C->IsHintArrowShown());
	C->Step(2.0f, Near);
	TestTrue(TEXT("Не нашёл за отведённое время — стрелка"), C->IsHintArrowShown());
	FVector Nearest;
	TestTrue(TEXT("Есть ближайший образец"), C->GetNearestSampleLocation(Nearest));
	TestTrue(TEXT("Стрелка указывает на ближайший образец"), Nearest.Equals(FVector(600.0f, 0.0f, 0.0f), 1.0f));
	const FVector2D ToNearest(Nearest.X - Near.HeroLocation.X, Nearest.Y - Near.HeroLocation.Y);
	TestTrue(TEXT("Камера на +X: ближайший образец — вверху локатора"),
		Chapter2Rules::LocatorScreenDir(ToNearest, 0.0f).Equals(FVector2D(0.0, -1.0), 0.01));
	TestTrue(TEXT("Камера повёрнута на 90°: тот же образец — слева"),
		Chapter2Rules::LocatorScreenDir(ToNearest, 90.0f).Equals(FVector2D(-1.0, 0.0), 0.01));
	TestTrue(TEXT("Образец найден"), C->CollectSample(TEXT("GearQA_A")));
	TestEqual(TEXT("Находка начинает отсчёт заново"), C->GetLocatorSecondsWithoutPickup(), 0.0f);
	TestFalse(TEXT("После находки стрелки нет"), C->IsHintArrowShown());

	UChapter2Component::FEnv Far = Chapter2Test::Env(false, false, false, 5000.0f);
	Far.ZoneDistance = S->LocatorShowDistance + 500.0f;
	C->Step(0.1f, Far);
	TestFalse(TEXT("Далеко от низины — обычный стик"), C->IsDetectorShown());
	Far.ZoneDistance = S->LocatorShowDistance * 0.5f;
	C->Step(0.1f, Far);
	TestTrue(TEXT("Ближе порога к низине — локатор"), C->IsDetectorShown());

	// --- Сдача: Вера забирает снаряжение обратно ---
	TestTrue(TEXT("Второй образец"), C->CollectSample(TEXT("GearQA_B")));
	TestTrue(TEXT("Третий образец"), C->CollectSample(TEXT("GearQA_C")));
	C->Step(0.1f, Chapter2Test::Env(false));
	TestFalse(TEXT("Все образцы собраны — обычный стик"), C->IsDetectorShown());
	C->NotifyMercenaryDefeated();
	TestTrue(TEXT("Задание сдаётся Вере"), Quests->TurnInQuest(QId, Chapter2Ids::VeraTag));
	C->SyncStoryState();
	TestEqual(TEXT("После сдачи снаряжения в сумке нет"), C->CountGearInBag(), 0);
	TestFalse(TEXT("После сдачи аппарата нет"), C->HasApparatus());
	TestNull(TEXT("После сдачи цели «Поговорить с Верой» нет"), Quests->FindQuest(Chapter2Ids::TalkToVeraQuest));

	Chapter2Test::Destroy(World);
	Chapter2Test::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 8. Правка Рината 25.10.2026 вечер: в смартфоне у задания «Образцы из аномалий» инструкция
//    [236] и картинки баллона и детектора; у других заданий — их обычное описание.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2PdaGuideTest, "ContrarySurvivor.Chapter2.Fixes2510.PdaSamplesGuide", Chapter2TestFlags)

bool FChapter2PdaGuideTest::RunTest(const FString& Parameters)
{
	UClass* AssetClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	if (!TestNotNull(TEXT("Окно смартфона /Game/UI/WBP_Pda загрузилось"), AssetClass))
	{
		return false;
	}
	const FString Slot = TEXT("ContraryChapter2PdaQA");
	Chapter2Test::DeleteSlot(Slot);
	UWorld* World = Chapter2Test::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Chapter2Test::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Quests)
	{
		AddError(TEXT("Не заспавнен герой"));
		Chapter2Test::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	FQuest Samples = GetDefault<AVeraNPC>()->GetSamplesQuest();
	Samples.State = EQuestState::Active;
	FQuest Other;
	Other.QuestId = FName(TEXT("Chapter2PdaOther"));
	Other.Title = INVTEXT("Другое задание");
	Other.Description = INVTEXT("Обычное описание задания.");
	Other.Kind = EQuestKind::Side;
	Other.TargetCount = 1;
	Other.State = EQuestState::Active;
	Quests->RestoreQuests({ Samples, Other }, Samples.QuestId);

	UPdaScreenWidget* Window = CreateWidget<UPdaScreenWidget>(World, AssetClass);
	if (TestNotNull(TEXT("Окно смартфона создалось"), Window))
	{
		Window->InitPda(Player);
		Window->SelectQuest(Samples.QuestId);
		Window->RefreshNow();
		TestEqual(TEXT("У задания учёных — инструкция [236]"), Window->GetDetailDescriptionForTest().ToString(),
			UChapter2Settings::Get()->SamplesGuideText.ToString());
		TestEqual(TEXT("Под инструкцией картинки баллона и детектора"), Window->GetQuestGuideImageCountForTest(), 2);

		Window->SelectQuest(Other.QuestId);
		Window->RefreshNow();
		TestEqual(TEXT("У другого задания — его описание"), Window->GetDetailDescriptionForTest().ToString(),
			FString(TEXT("Обычное описание задания.")));
		TestEqual(TEXT("У другого задания картинок снаряжения нет"), Window->GetQuestGuideImageCountForTest(), 0);
	}

	Chapter2Test::Destroy(World);
	Chapter2Test::DeleteSlot(Slot);
	return true;
}

#if CONTRARY_WITH_QA_CHEATS
// ===========================================================================
// 7. Правка Рината 25.10.2026 вечер: точки главы 2 в меню отладки ставят верное состояние из
//    любого исходного и пишут его в сохранение.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2DebugPointsTest, "ContrarySurvivor.Chapter2.Fixes2510.DebugMenuPoints", Chapter2TestFlags)

bool FChapter2DebugPointsTest::RunTest(const FString& Parameters)
{
	using namespace ContraryDebugTools;
	const FString Slot = TEXT("ContraryChapter2DebugQA");
	Chapter2Test::DeleteSlot(Slot);
	UWorld* World = Chapter2Test::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Chapter2Test::Spawn<ASaveTestPlayerCharacter>(World, FVector(-20000.0f, 0.0f, 100.0f));
	AElderNPC* Elder = Chapter2Test::Spawn<AElderNPC>(World, FVector(0.0f, 0.0f, 100.0f));
	AScientistQuestNPC* Scientist = Chapter2Test::Spawn<AScientistQuestNPC>(World, FVector(3000.0f, 0.0f, 100.0f));
	AScientistGuardNPC* Guard = Chapter2Test::Spawn<AScientistGuardNPC>(World, FVector(2500.0f, 800.0f, 100.0f));
	AVeraNPC* Vera = Chapter2Test::Spawn<AVeraNPC>(World, FVector(3500.0f, 0.0f, 100.0f));
	APoisonZone* Zone = Chapter2Test::Spawn<APoisonZone>(World, FVector(12000.0f, 0.0f, 300.0f));
	AMercenaryEncounter* Encounter = Chapter2Test::Spawn<AMercenaryEncounter>(World, FVector(6000.0f, 0.0f, 100.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Elder || !Scientist || !Guard || !Vera || !Zone || !Encounter || !C || !Quests)
	{
		AddError(TEXT("Не заспавнены герой, староста, учёные, Вера, низина или наёмник"));
		Chapter2Test::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	Player->ReloadStoryStateFromSave();
	C->ResetForNewGame();
	Quests->RestoreQuests({});
	Encounter->SetMercenaryClassForTest(AEnemyCharacter::StaticClass());
	const FName QId = Chapter2Ids::SamplesQuest;
	const float EdgeDistance = UChapter2Settings::Get()->MaskButtonEdgeDistance;

	// Расстояние до низины в её плоскости (высота героя в пустом тестовом мире условна).
	auto ZoneDist = [Zone, Player]()
	{
		FVector Loc = Player->GetActorLocation();
		Loc.Z = Zone->GetActorLocation().Z;
		return Zone->DistanceTo(Loc);
	};
	TestEqual(TEXT("Низина сама поставила три образца на свои места"), Zone->GetSpawnedSamples().Num(), 3);

	// --- Точка 1: ноутбук сдан учёному — идти к Вере ---
	TestTrue(TEXT("Точка «ноутбук сдан — к Вере» отработала"), SetStoryChapter2(Player, EChapter2DebugPoint::TalkToVera));
	TestTrue(TEXT("Ноутбук сдан учёному"), Quests->IsQuestTurnedIn(ContraryQuestIds::DeliverLaptopToScientists));
	TestTrue(TEXT("Рация охранника сдана"), Quests->IsQuestTurnedIn(Guard->GetOfferedQuest().QuestId));
	TestNull(TEXT("Задание Веры ещё не предложено"), Quests->FindQuest(QId));
	TestNotNull(TEXT("В списке заданий «Поговорить с Верой»"), Quests->FindQuest(Chapter2Ids::TalkToVeraQuest));
	TestEqual(TEXT("Снаряжения в сумке нет"), C->CountGearInBag(), 0);
	TestTrue(TEXT("Герой у учёного"), FVector::Dist2D(Player->GetActorLocation(), Scientist->GetActorLocation()) <= 700.0f);
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		TestTrue(TEXT("Точка записана в сохранение"), Save->bHasData
			&& Save->Quests.ContainsByPredicate([](const FQuest& Q) { return Q.QuestId == Chapter2Ids::TalkToVeraQuest; }));
	}

	// --- Точка 2: задание взято, герой у края низины ---
	TestTrue(TEXT("Точка «задание взято» отработала"), SetStoryChapter2(Player, EChapter2DebugPoint::SamplesTaken));
	TestEqual(TEXT("Задание Веры взято"), Chapter2Test::StateOf(Player, QId), EQuestState::Active);
	TestTrue(TEXT("Аппарат выдан"), C->HasApparatus());
	TestEqual(TEXT("Баллон и детектор в сумке"), C->CountGearInBag(), 2);
	TestEqual(TEXT("Образцы не собраны"), C->GetSamplesDone(), 0);
	TestNull(TEXT("Цель «Поговорить с Верой» ушла"), Quests->FindQuest(Chapter2Ids::TalkToVeraQuest));
	TestTrue(TEXT("Герой снаружи низины"), ZoneDist() > 0.0f);
	TestTrue(TEXT("Герой у края низины (видна кнопка аппарата)"), ZoneDist() <= EdgeDistance);

	// --- Точка 3: три образца, сразу перед наёмником ---
	TestTrue(TEXT("Точка «три образца» отработала"), SetStoryChapter2(Player, EChapter2DebugPoint::SamplesCollected));
	TestEqual(TEXT("Задание готово к сдаче"), Chapter2Test::StateOf(Player, QId), EQuestState::Completed);
	TestEqual(TEXT("Собраны три образца"), C->GetSamplesDone(), 3);
	bool bAllLevelSamplesPicked = true;
	for (const TWeakObjectPtr<AAnomalySample>& Sample : Zone->GetSpawnedSamples())
	{
		bAllLevelSamplesPicked &= Sample.IsValid() && C->IsSamplePicked(Sample->GetSampleId());
	}
	TestTrue(TEXT("Подобраны именно образцы уровня"), bAllLevelSamplesPicked);
	TestFalse(TEXT("Наёмник ещё не побеждён"), C->IsMercenaryDefeated());
	TestEqual(TEXT("Снаряжение ещё в сумке"), C->CountGearInBag(), 2);
	TestTrue(TEXT("Герой вне низины"), ZoneDist() > 0.0f);
	const float ToEncounter = FVector::Dist2D(Player->GetActorLocation(), Encounter->GetActorLocation());
	TestTrue(TEXT("Герой рядом с местом наёмника, но разговор ещё не начат"),
		ToEncounter > Encounter->GetTalkDistance() && ToEncounter <= Encounter->GetTalkDistance() + 1200.0f);
	C->Step(0.1f, Chapter2Test::Env(false));
	Encounter->UpdateEncounter(Player);
	TestNotNull(TEXT("Наёмник появился сразу"), Encounter->GetMercenary());

	// --- Точка 4: наёмник побеждён, герой у Веры ---
	TestTrue(TEXT("Точка «наёмник побеждён» отработала"), SetStoryChapter2(Player, EChapter2DebugPoint::MercenaryDefeated));
	TestTrue(TEXT("Наёмник побеждён"), C->IsMercenaryDefeated());
	TestNull(TEXT("Наёмника прошлой точки на уровне нет"), Encounter->GetMercenary());
	TestEqual(TEXT("Задание готово к сдаче"), Chapter2Test::StateOf(Player, QId), EQuestState::Completed);
	TestTrue(TEXT("Герой у Веры"), FVector::Dist2D(Player->GetActorLocation(), Vera->GetActorLocation()) <= 700.0f);
	Vera->OnTalkOpened(Player);
	TestEqual(TEXT("Вера принимает образцы"), Vera->GetStage(), EVeraTalkStage::Samples);
	Vera->OnTalkClosed(Player);
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		TestTrue(TEXT("В сохранении наёмник побеждён"), Save->bChapter2MercenaryDefeated);
		TestEqual(TEXT("В сохранении три образца"), Save->Chapter2PickedSamples.Num(), 3);
	}

	// --- Назад к старосте: глава 2 сбрасывается ---
	TestTrue(TEXT("Точка старосты отработала"), SetStoryElderOffersBanditQuest(Player));
	TestFalse(TEXT("Аппарата нет"), C->HasApparatus());
	TestEqual(TEXT("Снаряжения нет"), C->CountGearInBag(), 0);
	TestEqual(TEXT("Образцы не подобраны"), C->GetPickedCount(), 0);
	TestFalse(TEXT("Наёмник не побеждён"), C->IsMercenaryDefeated());
	TestNull(TEXT("Цели «Поговорить с Верой» нет"), Quests->FindQuest(Chapter2Ids::TalkToVeraQuest));

	Chapter2Test::Destroy(World);
	Chapter2Test::DeleteSlot(Slot);
	return true;
}
#endif // CONTRARY_WITH_QA_CHEATS

#endif // WITH_DEV_AUTOMATION_TESTS
