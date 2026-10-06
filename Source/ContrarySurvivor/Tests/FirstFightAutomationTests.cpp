// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты правок первого боя (решения Рината 05–06.10.2026, ветка
// feature/first-fight-1006). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.FirstFight; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Пять правок — пять тестов:
//   1) нож: герой доворачивается к ближайшему врагу в досягаемости, и удар приходится по цели;
//   2) волк замедлен, пока первое задание старосты не сдано;
//   3) торговец не выкупает шкуры, пока первое задание не сдано, и называет причину;
//   4) аптечка старосты: реплика называет её прямо, надпись о подарке ждёт закрытия диалога,
//      кнопка сумки мигает;
//   5) событие смерти игрока несёт тип убийцы.
//
// Паттерн мира — CombatTestWorld (CombatAutomationTests.cpp): транзиентный игровой мир, спавн
// реальных классов, вызов РЕАЛЬНОЙ логики. Числа настроек тесты не пришпиливают (их крутит
// Ринат) — держат правила: «замедлен/не замедлен», «берёт/не берёт».

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/TouchControlsWidget.h"
#include "AMeleeWeapon.h"
#include "AQuestItem.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h" // FURL
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h" // защищённые настройки читаем и ставим как дизайнер — через рефлексию

static constexpr EAutomationTestFlags FirstFightTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Локальная копия обвязки CombatTestWorld (у каждого файла тестов своя — правило проекта).
namespace FirstFightTestWorld
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

	// Задание с заданным идентификатором и состоянием — для подмены журнала игрока.
	static FQuest MakeQuest(FName QuestId, EQuestState State)
	{
		FQuest Quest;
		Quest.QuestId = QuestId;
		Quest.State = State;
		return Quest;
	}

	// Значение защищённой числовой настройки объекта (по имени поля, как его видит редактор).
	static bool ReadFloat(const UObject* Object, const TCHAR* PropertyName, float& OutValue)
	{
		const FFloatProperty* Prop = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), FName(PropertyName)) : nullptr;
		if (!Prop)
		{
			return false;
		}
		OutValue = Prop->GetPropertyValue_InContainer(Object);
		return true;
	}

	static bool WriteFloat(UObject* Object, const TCHAR* PropertyName, float Value)
	{
		const FFloatProperty* Prop = Object ? FindFProperty<FFloatProperty>(Object->GetClass(), FName(PropertyName)) : nullptr;
		if (!Prop)
		{
			return false;
		}
		Prop->SetPropertyValue_InContainer(Object, Value);
		return true;
	}

	// Идентификатор задания из настройки-имени (FName) по имени поля.
	static FName ReadName(const UObject* Object, const TCHAR* PropertyName)
	{
		const FNameProperty* Prop = Object ? FindFProperty<FNameProperty>(Object->GetClass(), FName(PropertyName)) : nullptr;
		return Prop ? Prop->GetPropertyValue_InContainer(Object) : NAME_None;
	}

	// Один замах ножом до конца: Fire запускает замах; если пошла анимация, урон ждёт метку
	// на её дорожке — в тесте кадров анимации нет, поэтому метку отыгрываем сами тем же
	// путём, что UAnimNotify_MeleeHit (ConsumePendingSwing -> ApplyMeleeDamage).
	static void SwingKnife(AMeleeWeapon* Knife)
	{
		Knife->Fire(nullptr);
		if (Knife->ConsumePendingSwing())
		{
			Knife->ApplyMeleeDamage();
		}
	}
}

// ===========================================================================
// 1. Доворот к ближайшему врагу при ударе ножом («чтобы удар приходился по цели»).
//    Враг стоит ЗА СПИНОЙ героя в досягаемости удара: без доворота сектор его не задевает,
//    замах с доворотом — попадает, герой оказывается к врагу лицом.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstFightKnifeTurnsToNearestEnemyTest,
	"ContrarySurvivor.FirstFight.KnifeTurnsToNearestEnemy", FirstFightTestFlags)
bool FFirstFightKnifeTurnsToNearestEnemyTest::RunTest(const FString& Parameters)
{
	UWorld* World = FirstFightTestWorld::Create();
	TestNotNull(TEXT("Тестовый мир создан"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		// Герой смотрит вдоль +X; враг в 140 см ЗА спиной (зазор между капсулами меньше
		// дальности удара 90), второй враг — далеко сбоку, вне досягаемости.
		APlayerCharacter* Player = FirstFightTestWorld::Spawn<APlayerCharacter>(World, FVector(0.f, 0.f, 100.f));
		AEnemyCharacter* Behind = FirstFightTestWorld::Spawn<AEnemyCharacter>(World, FVector(-140.f, 0.f, 100.f));
		AEnemyCharacter* Far = FirstFightTestWorld::Spawn<AEnemyCharacter>(World, FVector(0.f, 600.f, 100.f));
		AMeleeWeapon* Knife = FirstFightTestWorld::Spawn<AMeleeWeapon>(World, FVector(0.f, 0.f, 50.f));
		TestNotNull(TEXT("Герой создан"), Player);
		TestNotNull(TEXT("Враг за спиной создан"), Behind);
		TestNotNull(TEXT("Дальний враг создан"), Far);
		TestNotNull(TEXT("Нож создан"), Knife);

		UStatsComponent* BehindStats = Behind ? Behind->FindComponentByClass<UStatsComponent>() : nullptr;
		UStatsComponent* FarStats = Far ? Far->FindComponentByClass<UStatsComponent>() : nullptr;
		TestNotNull(TEXT("У врага за спиной есть здоровье"), BehindStats);
		TestNotNull(TEXT("У дальнего врага есть здоровье"), FarStats);

		if (Player && Behind && Far && Knife && BehindStats && FarStats)
		{
			Knife->SetInstigator(Player);
			const float HP0 = BehindStats->GetHealth();
			const float FarHP0 = FarStats->GetHealth();

			// Настройки доворота существуют под теми именами, что видит редактор.
			float ExtraRange = 0.0f;
			float SearchAngle = 0.0f;
			TestTrue(TEXT("Настройка AutoFaceExtraRange есть у ножа"),
				FirstFightTestWorld::ReadFloat(Knife, TEXT("AutoFaceExtraRange"), ExtraRange));
			TestTrue(TEXT("Настройка AutoFaceHalfAngleDeg есть у ножа"),
				FirstFightTestWorld::ReadFloat(Knife, TEXT("AutoFaceHalfAngleDeg"), SearchAngle));
			TestNotNull(TEXT("Выключатель bAutoFaceNearestEnemy есть у ножа"),
				FindFProperty<FBoolProperty>(AMeleeWeapon::StaticClass(), TEXT("bAutoFaceNearestEnemy")));

			// Без доворота (голый расчёт урона, как было до правки): враг за спиной не задет.
			Knife->ApplyMeleeDamage();
			TestEqual(TEXT("Без доворота враг за спиной удара не получает"), BehindStats->GetHealth(), HP0);

			// Узкий угол поиска: враг за спиной целью доворота не становится.
			FirstFightTestWorld::WriteFloat(Knife, TEXT("AutoFaceHalfAngleDeg"), 90.0f);
			TestNull(TEXT("При угле поиска 90 враг за спиной не выбирается"), Knife->FindAutoFaceTarget());
			FirstFightTestWorld::WriteFloat(Knife, TEXT("AutoFaceHalfAngleDeg"), SearchAngle);

			// Цель доворота — ближайший враг в досягаемости, а не дальний.
			TestTrue(TEXT("Целью доворота выбран враг за спиной"), Knife->FindAutoFaceTarget() == Behind);

			// Замах: герой доворачивается, удар приходится по цели.
			FirstFightTestWorld::SwingKnife(Knife);
			TestTrue(TEXT("После замаха враг за спиной получил удар"), BehindStats->GetHealth() < HP0);
			TestEqual(TEXT("Дальний враг удара не получил"), FarStats->GetHealth(), FarHP0);

			const FVector ToEnemy = (Behind->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
			TestTrue(TEXT("Герой повёрнут лицом к врагу"),
				FVector::DotProduct(Player->GetActorForwardVector().GetSafeNormal2D(), ToEnemy) > 0.99f);

			// Врага в досягаемости нет — доворачиваться не к кому.
			Behind->SetActorLocation(FVector(-600.f, 0.f, 100.f));
			TestNull(TEXT("Вне досягаемости цель доворота не выбирается"), Knife->FindAutoFaceTarget());
		}
		else { bOk = false; }
	}

	FirstFightTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 2. Волк замедлен, пока первое задание старосты не сдано; после сдачи — полная скорость.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstFightWolfSlowedUntilQuestTurnedInTest,
	"ContrarySurvivor.FirstFight.WolfSlowedUntilFirstQuestTurnedIn", FirstFightTestFlags)
bool FFirstFightWolfSlowedUntilQuestTurnedInTest::RunTest(const FString& Parameters)
{
	// --- Формула на объекте-образце класса ---
	const AWolfCharacter* WolfDefaults = GetDefault<AWolfCharacter>();
	if (!TestNotNull(TEXT("Образец волка доступен"), WolfDefaults))
	{
		return false;
	}

	float Multiplier = 0.0f;
	TestTrue(TEXT("Настройка FirstFightSpeedMultiplier есть у волка"),
		FirstFightTestWorld::ReadFloat(WolfDefaults, TEXT("FirstFightSpeedMultiplier"), Multiplier));
	const FName QuestId = FirstFightTestWorld::ReadName(WolfDefaults, TEXT("FirstFightQuestId"));
	TestFalse(TEXT("Задание замедления у волка задано"), QuestId.IsNone());
	TestEqual(TEXT("Задание замедления — первое задание старосты"),
		QuestId, GetDefault<AElderNPC>()->GetOfferedQuest().QuestId);

	const float FullSpeed = WolfDefaults->GetChaseSpeed();
	TestTrue(TEXT("Множитель первого боя замедляет, а не ускоряет"), Multiplier > 0.0f && Multiplier < 1.0f);
	TestEqual(TEXT("До сдачи задания скорость умножена на множитель"),
		WolfDefaults->GetChaseSpeedForFirstFight(/*bFirstQuestTurnedIn=*/false), FullSpeed * Multiplier, 0.01f);
	TestEqual(TEXT("После сдачи задания скорость полная"),
		WolfDefaults->GetChaseSpeedForFirstFight(/*bFirstQuestTurnedIn=*/true), FullSpeed, 0.01f);

	// --- Живой волк и журнал заданий игрока ---
	UWorld* World = FirstFightTestWorld::Create();
	TestNotNull(TEXT("Тестовый мир создан"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		APlayerCharacter* Player = FirstFightTestWorld::Spawn<APlayerCharacter>(World, FVector(0.f, 0.f, 100.f));
		AWolfCharacter* Wolf = FirstFightTestWorld::Spawn<AWolfCharacter>(World, FVector(2000.f, 0.f, 100.f));
		TestNotNull(TEXT("Герой создан"), Player);
		TestNotNull(TEXT("Волк создан"), Wolf);

		UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
		UCharacterMovementComponent* Move = Wolf ? Wolf->GetCharacterMovement() : nullptr;
		TestNotNull(TEXT("У героя есть журнал заданий"), Quests);
		TestNotNull(TEXT("У волка есть движение"), Move);

		if (Player && Wolf && Quests && Move)
		{
			const float Slowed = Wolf->GetChaseSpeedForFirstFight(false);
			const float Full = Wolf->GetChaseSpeed();

			// Задания в журнале ещё нет (игрок не дошёл до старосты) — волк замедлен.
			Quests->RestoreQuests(TArray<FQuest>());
			Wolf->ApplyFirstFightSpeed(Quests);
			TestEqual(TEXT("Задания нет в журнале — волк замедлен"), Move->MaxWalkSpeed, Slowed, 0.01f);

			// Задание взято, но не сдано — всё ещё замедлен.
			Quests->RestoreQuests({ FirstFightTestWorld::MakeQuest(QuestId, EQuestState::Active) });
			Wolf->ApplyFirstFightSpeed(Quests);
			TestEqual(TEXT("Задание взято, не сдано — волк замедлен"), Move->MaxWalkSpeed, Slowed, 0.01f);

			// Задание сдано — замедление снято с уже живого волка.
			Quests->RestoreQuests({ FirstFightTestWorld::MakeQuest(QuestId, EQuestState::TurnedIn) });
			Wolf->ApplyFirstFightSpeed(Quests);
			TestEqual(TEXT("Задание сдано — волк бежит в полную силу"), Move->MaxWalkSpeed, Full, 0.01f);

			// Журнала нет вовсе (игрок ещё не появился) — считаем задание несданным.
			Wolf->ApplyFirstFightSpeed(nullptr);
			TestEqual(TEXT("Журнала нет — волк замедлен"), Move->MaxWalkSpeed, Slowed, 0.01f);
		}
		else { bOk = false; }
	}

	FirstFightTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 3. Торговец не выкупает шкуры, пока первое задание не сдано; после сдачи — выкупает.
//    Игрок получает короткую причину отказа.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstFightTraderRefusesPeltsTest,
	"ContrarySurvivor.FirstFight.TraderRefusesPeltsUntilQuestTurnedIn", FirstFightTestFlags)
bool FFirstFightTraderRefusesPeltsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FirstFightTestWorld::Create();
	TestNotNull(TEXT("Тестовый мир создан"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		APlayerCharacter* Player = FirstFightTestWorld::Spawn<APlayerCharacter>(World, FVector(0.f, 0.f, 100.f));
		AMasterTrader* Trader = FirstFightTestWorld::Spawn<AMasterTrader>(World, FVector(500.f, 0.f, 100.f));
		AQuestItem* Pelt = FirstFightTestWorld::Spawn<AQuestItem>(World, FVector(0.f, 500.f, 100.f));
		AQuestItem* Laptop = FirstFightTestWorld::Spawn<AQuestItem>(World, FVector(0.f, 700.f, 100.f));
		TestNotNull(TEXT("Герой создан"), Player);
		TestNotNull(TEXT("Торговец создан"), Trader);
		TestNotNull(TEXT("Шкура создана"), Pelt);
		TestNotNull(TEXT("Посторонний предмет создан"), Laptop);

		UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
		TestNotNull(TEXT("У героя есть журнал заданий"), Quests);

		if (Player && Trader && Pelt && Laptop && Quests)
		{
			// Служебный ключ шкуры — тот, с которым её роняет волк; задание — первое у старосты.
			const FStrProperty* PeltKeyProp =
				FindFProperty<FStrProperty>(AWolfCharacter::StaticClass(), TEXT("QuestLootItemName"));
			const FString PeltKey = PeltKeyProp
				? PeltKeyProp->GetPropertyValue_InContainer(GetDefault<AWolfCharacter>()) : FString();
			TestFalse(TEXT("Ключ шкуры прочитан у волка"), PeltKey.IsEmpty());
			const FName QuestId = GetDefault<AElderNPC>()->GetOfferedQuest().QuestId;

			Pelt->ItemName = PeltKey;
			Laptop->ItemName = TEXT("Ноутбук");

			const IShopVendor* Vendor = Trader;
			FText Reason;

			// Задание взято, не сдано: шкуру не берёт и объясняет почему.
			Quests->RestoreQuests({ FirstFightTestWorld::MakeQuest(QuestId, EQuestState::Active) });
			TestFalse(TEXT("До сдачи задания торговец шкуру не берёт"),
				Vendor->CanBuyFromPlayer(Pelt, Player, Reason));
			TestFalse(TEXT("Причина отказа не пустая"), Reason.IsEmpty());
			AddInfo(FString::Printf(TEXT("Причина отказа, которую видит игрок: «%s»"), *Reason.ToString()));

			// Задания в журнале нет вовсе (игрок миновал старосту) — тоже не берёт.
			Quests->RestoreQuests(TArray<FQuest>());
			TestTrue(TEXT("Задания нет в журнале — шкура под замком"),
				Trader->IsSellLockedByQuest(PeltKey, Quests));

			// Прочие предметы замок не трогает.
			Reason = FText::GetEmpty();
			TestTrue(TEXT("Посторонний предмет торговец берёт и до сдачи задания"),
				Vendor->CanBuyFromPlayer(Laptop, Player, Reason));
			TestTrue(TEXT("У разрешённой сделки причины отказа нет"), Reason.IsEmpty());

			// Задание сдано: шкуру берёт, цена выкупа на месте.
			Quests->RestoreQuests({ FirstFightTestWorld::MakeQuest(QuestId, EQuestState::TurnedIn) });
			TestTrue(TEXT("После сдачи задания торговец шкуру берёт"),
				Vendor->CanBuyFromPlayer(Pelt, Player, Reason));
			TestTrue(TEXT("Цена выкупа шкуры положительная"), Trader->GetSellValue(Pelt) > 0.0f);
		}
		else { bOk = false; }
	}

	FirstFightTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 4. Аптечка старосты заметна: реплика называет её прямо, надпись о подарке откладывается
//    до закрытия диалога, кнопка сумки мигает ограниченное время.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstFightElderGiftNoticeableTest,
	"ContrarySurvivor.FirstFight.ElderGiftIsNoticeable", FirstFightTestFlags)
bool FFirstFightElderGiftNoticeableTest::RunTest(const FString& Parameters)
{
	// --- (а) Реплика, по кнопке которой выдаётся подарок, называет аптечку ---
	const AElderNPC* Elder = GetDefault<AElderNPC>();
	if (!TestNotNull(TEXT("Образец старосты доступен"), Elder))
	{
		return false;
	}
	int32 GiftLines = 0;
	for (const FElderIntroLine& Line : Elder->GetIntroLines())
	{
		if (Line.Action == EElderIntroAction::GiveGift)
		{
			++GiftLines;
			TestTrue(FString::Printf(TEXT("Реплика с подарком называет аптечку: «%s»"), *Line.NPCText.ToString()),
				Line.NPCText.ToString().Contains(TEXT("аптечк")));
		}
	}
	TestEqual(TEXT("Реплика с подарком в интро ровно одна"), GiftLines, 1);

	// Надпись о подарке говорит, где его искать (поле защищённое — читаем через рефлексию).
	const FTextProperty* HintProp = FindFProperty<FTextProperty>(AElderNPC::StaticClass(), TEXT("FirstGiftHintFormat"));
	if (TestNotNull(TEXT("Настройка FirstGiftHintFormat есть у старосты"), HintProp))
	{
		const FString Hint = HintProp->GetPropertyValue_InContainer(Elder).ToString();
		TestTrue(FString::Printf(TEXT("Надпись о подарке называет сумку: «%s»"), *Hint), Hint.Contains(TEXT("сумк")));
	}

	// --- (б) Надпись о подарке ждёт закрытия диалога ---
	AContrarySurvivorPlayerController* PC = NewObject<AContrarySurvivorPlayerController>(GetTransientPackage());
	if (!TestNotNull(TEXT("Контроллер создан"), PC))
	{
		return false;
	}
	const FText Notice = FText::FromString(TEXT("Получено: Аптечка"));

	PC->SetDialogOpenForQA(true);
	PC->NotifyGiftReceived(Notice);
	TestTrue(TEXT("Пока идёт диалог, надпись отложена"), PC->HasPendingGiftNotice());

	PC->SetDialogOpenForQA(false);
	TestTrue(TEXT("При закрытии диалога отложенная надпись показывается"), PC->FlushPendingGiftNotice());
	TestFalse(TEXT("После показа отложенной надписи не остаётся"), PC->HasPendingGiftNotice());
	TestFalse(TEXT("Второй раз показывать нечего"), PC->FlushPendingGiftNotice());

	PC->NotifyGiftReceived(Notice);
	TestFalse(TEXT("Вне диалога надпись показывается сразу, не откладывается"), PC->HasPendingGiftNotice());

	PC->SetDialogOpenForQA(true);
	PC->NotifyGiftReceived(FText::GetEmpty());
	TestFalse(TEXT("Пустая надпись не откладывается"), PC->HasPendingGiftNotice());

	// --- (в) Мигание кнопки сумки ---
	const UTouchControlsWidget* LayerDefaults = GetDefault<UTouchControlsWidget>();
	if (!TestNotNull(TEXT("Образец слоя экранных кнопок доступен"), LayerDefaults))
	{
		return false;
	}
	TestTrue(TEXT("Мигание кнопки сумки включено (длительность больше нуля)"),
		LayerDefaults->InventoryBlinkDuration > 0.0f);
	TestFalse(TEXT("Сама по себе кнопка сумки не мигает"), LayerDefaults->IsInventoryAttentionBlinking());

	UTouchControlsWidget* Layer = NewObject<UTouchControlsWidget>(GetTransientPackage());
	if (TestNotNull(TEXT("Слой экранных кнопок создан"), Layer))
	{
		Layer->StartInventoryAttentionBlink();
		TestTrue(TEXT("После подарка кнопка сумки мигает"), Layer->IsInventoryAttentionBlinking());

		Layer->InventoryBlinkDuration = 0.0f;
		Layer->StartInventoryAttentionBlink();
		TestFalse(TEXT("Длительность 0 выключает мигание"), Layer->IsInventoryAttentionBlinking());
	}

	// Яркость мигания ходит между (1 - глубина) и 1.
	const float Period = 0.8f;
	const float Depth = 0.7f;
	TestEqual(TEXT("В верхней точке цикла яркость полная"),
		UTouchControlsWidget::ComputeBlinkBrightness(Period * 0.25f, Period, Depth), 1.0f, 0.001f);
	TestEqual(TEXT("В нижней точке цикла яркость снижена на глубину"),
		UTouchControlsWidget::ComputeBlinkBrightness(Period * 0.75f, Period, Depth), 1.0f - Depth, 0.001f);
	return true;
}

// ===========================================================================
// 5. Событие смерти игрока несёт тип убийцы (wolf / bandit / other).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstFightDeathEventNamesKillerTest,
	"ContrarySurvivor.FirstFight.DeathEventNamesKiller", FirstFightTestFlags)
bool FFirstFightDeathEventNamesKillerTest::RunTest(const FString& Parameters)
{
	// --- Имя события: три части через двоеточие, только латиница ---
	TestEqual(TEXT("Смерть от волка"),
		UAnalyticsSubsystem::MakePlayerDeathEventId(TEXT("wolf")), FString(TEXT("player:death:wolf")));
	TestEqual(TEXT("Смерть от бандита"),
		UAnalyticsSubsystem::MakePlayerDeathEventId(TEXT("bandit")), FString(TEXT("player:death:bandit")));
	TestEqual(TEXT("Смерть от иного"),
		UAnalyticsSubsystem::MakePlayerDeathEventId(TEXT("other")), FString(TEXT("player:death:other")));
	TestEqual(TEXT("Пустой тип не даёт пустой части имени"),
		UAnalyticsSubsystem::MakePlayerDeathEventId(FString()), FString(TEXT("player:death:unknown")));
	TestEqual(TEXT("Недопустимые знаки в типе заменяются"),
		UAnalyticsSubsystem::MakePlayerDeathEventId(TEXT("Волк:1")), FString(TEXT("player:death:_____1")));

	// --- Тип убийцы по источнику урона ---
	TestEqual(TEXT("Волк -> wolf"),
		APlayerCharacter::GetKillerAnalyticsType(GetDefault<AWolfCharacter>()), FString(TEXT("wolf")));
	TestEqual(TEXT("Бандит -> bandit"),
		APlayerCharacter::GetKillerAnalyticsType(GetDefault<AEnemyCharacter>()), FString(TEXT("bandit")));
	TestEqual(TEXT("Прочий источник -> other"),
		APlayerCharacter::GetKillerAnalyticsType(GetDefault<AMasterTrader>()), FString(TEXT("other")));
	TestEqual(TEXT("Источника нет -> other"),
		APlayerCharacter::GetKillerAnalyticsType(nullptr), FString(TEXT("other")));

	// --- Герой запоминает тип последнего нанёсшего урон ---
	UWorld* World = FirstFightTestWorld::Create();
	TestNotNull(TEXT("Тестовый мир создан"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		APlayerCharacter* Player = FirstFightTestWorld::Spawn<APlayerCharacter>(World, FVector(0.f, 0.f, 100.f));
		AWolfCharacter* Wolf = FirstFightTestWorld::Spawn<AWolfCharacter>(World, FVector(2000.f, 0.f, 100.f));
		AEnemyCharacter* Bandit = FirstFightTestWorld::Spawn<AEnemyCharacter>(World, FVector(-2000.f, 0.f, 100.f));
		TestNotNull(TEXT("Герой создан"), Player);
		TestNotNull(TEXT("Волк создан"), Wolf);
		TestNotNull(TEXT("Бандит создан"), Bandit);

		if (Player && Wolf && Bandit)
		{
			TestEqual(TEXT("До первого урона тип убийцы — other"),
				Player->GetLastDamagerAnalyticsType(), FString(TEXT("other")));

			const FDamageEvent DamageEvent;
			Player->TakeDamage(1.0f, DamageEvent, nullptr, Wolf);
			TestEqual(TEXT("После укуса волка тип убийцы — wolf"),
				Player->GetLastDamagerAnalyticsType(), FString(TEXT("wolf")));

			Player->TakeDamage(1.0f, DamageEvent, nullptr, Bandit);
			TestEqual(TEXT("После удара бандита тип убийцы — bandit"),
				Player->GetLastDamagerAnalyticsType(), FString(TEXT("bandit")));

			// Урон без источника (и урон самому себе) последнего врага не перетирает.
			Player->TakeDamage(1.0f, DamageEvent, nullptr, nullptr);
			TestEqual(TEXT("Урон без источника тип убийцы не меняет"),
				Player->GetLastDamagerAnalyticsType(), FString(TEXT("bandit")));
		}
		else { bOk = false; }
	}

	FirstFightTestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
