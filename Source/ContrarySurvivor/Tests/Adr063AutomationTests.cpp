// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты решений издателя ADR-063 (08-08 вечер): баланс выживания,
// единый поток огнестрела с трупа, порог рекламы по РИ-29. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Adr063; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * интервалы убыли замедлены вдвое: жажда -1/16 с, голод -1/24 с (шаг убыли и
//     критический порог 20 не тронуты); приглушение урона истощения включено, порог 15 мин;
//   * урон истощения приглушён у свежего игрока и снимается сдачей первого квеста
//     (существующий сигнал журнала GetTurnedInQuestCount);
//   * порог рекламы: 300 с СУММАРНОГО времени ИЛИ сдан первый квест — что раньше
//     (чистая логика IsAdGatePassed, её зовут все три точки показа);
//   * огнестрел с трупа ложится в РЮКЗАК и слот оружия сам НЕ занимает; в слот его
//     переносит явный вызов TryAdoptRangedWeapon («тап в инвентаре»).
//
// НЕ покрывается headless: сама всплывашка «Подобрано: … — наденьте в инвентаре»
// (тост онбординга требует контроллера и вьюпорта — только живой запуск) и реальный
// таймер урона истощения (гоняется гейт-условие, а не таймер).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "APistol.h"
#include "AMasterInventoryItem.h"
#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/UI/CorpseLootWidget.h"
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags Adr063TestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace Adr063TestWorld
{
	// Мир и игрок — тот же каркас, что ShopBackpackWeaponAutomationTests (логика без Slate).
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
			WorldSettings->NotifyBeginPlay(); // мир без GameMode сам begun-play не ставит
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

	static APlayerCharacter* SpawnPlayer(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<APlayerCharacter>(
			APlayerCharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
	}

	// Сданный квест для журнала: «первый квест выполнен» = State TurnedIn (сдан старосте).
	static FQuest TurnedInQuest()
	{
		FQuest Quest;
		Quest.QuestId = TEXT("Adr063TestQuest");
		Quest.State = EQuestState::TurnedIn;
		return Quest;
	}
}

// --- 1. Баланс: интервалы убыли замедлены вдвое, приглушение включено ---------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdr063SurvivalBalanceTest,
	"ContrarySurvivor.Adr063.SurvivalDrainSlowedTwofold", Adr063TestFlags)

bool FAdr063SurvivalBalanceTest::RunTest(const FString& Parameters)
{
	// Значения по умолчанию читаются с объекта-эталона класса через отражение (поля
	// защищённые; отражение — как читает их редактор). Контракт чисел — ADR-063 п.1.
	const UStatsComponent* Defaults = GetDefault<UStatsComponent>();
	if (!TestNotNull(TEXT("Эталон статов доступен"), Defaults))
	{
		return false;
	}

	auto ReadFloat = [Defaults](const TCHAR* Name, float& Out) -> bool
	{
		const FFloatProperty* Property =
			FindFProperty<FFloatProperty>(UStatsComponent::StaticClass(), Name);
		if (!Property)
		{
			return false;
		}
		Out = Property->GetPropertyValue_InContainer(Defaults);
		return true;
	};

	float Value = 0.0f;
	if (TestTrue(TEXT("Поле интервала жажды существует"), ReadFloat(TEXT("ThirstDrainInterval"), Value)))
	{
		TestEqual(TEXT("Жажда: -1 за 16 с (было 8, замедление вдвое)"), Value, 16.0f);
	}
	if (TestTrue(TEXT("Поле интервала голода существует"), ReadFloat(TEXT("HungerDrainInterval"), Value)))
	{
		TestEqual(TEXT("Голод: -1 за 24 с (было 12, замедление вдвое)"), Value, 24.0f);
	}
	if (TestTrue(TEXT("Поле шага убыли существует"), ReadFloat(TEXT("SurvivalDrainStep"), Value)))
	{
		TestEqual(TEXT("Шаг убыли не тронут"), Value, 1.0f);
	}
	if (TestTrue(TEXT("Поле критического порога существует"), ReadFloat(TEXT("CriticalThreshold"), Value)))
	{
		TestEqual(TEXT("Критический порог 20 не тронут"), Value, 20.0f);
	}
	if (TestTrue(TEXT("Поле порога приглушения существует"), ReadFloat(TEXT("StarvationGraceMinutes"), Value)))
	{
		TestEqual(TEXT("Страховка приглушения: 15 минут игры"), Value, 15.0f);
	}

	const FBoolProperty* GraceEnabled =
		FindFProperty<FBoolProperty>(UStatsComponent::StaticClass(), TEXT("bStarvationGraceEnabled"));
	if (TestNotNull(TEXT("Включатель приглушения существует"), GraceEnabled))
	{
		TestTrue(TEXT("Приглушение урона истощения включено по умолчанию"),
			GraceEnabled->GetPropertyValue_InContainer(Defaults));
	}
	return true;
}

// --- 2. Приглушение урона истощения: активно у новичка, снимается первым квестом ----------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdr063StarvationGraceTest,
	"ContrarySurvivor.Adr063.StarvationGraceUntilFirstQuest", Adr063TestFlags)

bool FAdr063StarvationGraceTest::RunTest(const FString& Parameters)
{
	UWorld* World = Adr063TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		APlayerCharacter* Player = Adr063TestWorld::SpawnPlayer(World);
		UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
		UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
		if (Player && Stats && Quests)
		{
			// Наигрыш НЕ детерминирован: игрок в BeginPlay поднимает НАСТОЯЩИЙ сейв машины,
			// и на машине разработчика суммарное время давно больше 15 минут — страховка
			// честно снимала бы приглушение (первый прогон это и поймал). Здесь проверяется
			// ветка квеста, поэтому порог минут поднимается за горизонт через отражение
			// (поле защищённое); само значение по умолчанию закреплено тестом №1.
			if (const FFloatProperty* GraceMinutes = FindFProperty<FFloatProperty>(
				UStatsComponent::StaticClass(), TEXT("StarvationGraceMinutes")))
			{
				GraceMinutes->SetPropertyValue_InContainer(Stats, 1.0e9f);
			}

			// Свежий игрок: квест не сдан, порог времени не достигнут — урон приглушён.
			TestFalse(TEXT("Свежий игрок: первый квест не сдан"), Player->HasTurnedInFirstQuest());
			TestTrue(TEXT("Свежий игрок: урон истощения приглушён"),
				Stats->IsStarvationDamageSuppressed());

			// Первый квест сдан (журнал восстановлен со сданным квестом, как из сейва) —
			// приглушение снято, истощение бьёт как обычно.
			Quests->RestoreQuests({ Adr063TestWorld::TurnedInQuest() });
			TestTrue(TEXT("После сдачи: сигнал журнала видит сданный квест"),
				Player->HasTurnedInFirstQuest());
			TestFalse(TEXT("После сдачи первого квеста приглушение снято"),
				Stats->IsStarvationDamageSuppressed());
		}
		else
		{
			AddError(TEXT("Игрок/статы/журнал не создались в тестовом мире"));
		}
	}

	Adr063TestWorld::Destroy(World);
	return true;
}

// --- 3. Порог рекламы: 300 с ИЛИ первый квест, что раньше (РИ-29) -------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdr063AdGateTest,
	"ContrarySurvivor.Adr063.AdGatePassedBy300sOrFirstQuest", Adr063TestFlags)

bool FAdr063AdGateTest::RunTest(const FString& Parameters)
{
	// Обе части условия и их «что раньше» — чистой функцией, которую зовут все три точки.
	TestFalse(TEXT("Свежая установка без квеста: рекламы нет"),
		AdGating::IsAdGatePassed(0.0, false));
	TestFalse(TEXT("4:59 без квеста: рекламы нет"), AdGating::IsAdGatePassed(299.0, false));
	TestTrue(TEXT("Ровно 5:00 без квеста: реклама разрешена"), AdGating::IsAdGatePassed(300.0, false));
	TestTrue(TEXT("Первый квест сдан на 0-й секунде: реклама разрешена (квест раньше времени)"),
		AdGating::IsAdGatePassed(0.0, true));
	TestTrue(TEXT("4:59 и квест сдан: разрешена"), AdGating::IsAdGatePassed(299.0, true));

	// Порог настраиваемый (EditAnywhere на игроке) — функция уважает переданное значение.
	TestFalse(TEXT("Свой порог 120 с: 100 с без квеста не проходит"),
		AdGating::IsAdGatePassed(100.0, false, 120.0));
	TestTrue(TEXT("Свой порог 120 с: 150 с без квеста проходит"),
		AdGating::IsAdGatePassed(150.0, false, 120.0));
	return true;
}

// --- 4. Огнестрел с трупа: в рюкзак, не в слот; в слот — явным «тапом» --------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdr063CorpseFirearmTest,
	"ContrarySurvivor.Adr063.CorpseFirearmGoesToBackpackNotSlot", Adr063TestFlags)

bool FAdr063CorpseFirearmTest::RunTest(const FString& Parameters)
{
	UWorld* World = Adr063TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		APlayerCharacter* Player = Adr063TestWorld::SpawnPlayer(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;

		// Труп: голый актор с контейнером лута, внутри пистолет. В реестр обыскиваемых не
		// встаёт (bRegisterSearchable=false) — реестр статический, чужие тесты не трогаем.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* CorpseActor = World->SpawnActor<AActor>(
			AActor::StaticClass(), FVector(100.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		APistol* Pistol = World->SpawnActor<APistol>(
			APistol::StaticClass(), FVector(100.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		UCorpseLootComponent* Corpse = CorpseActor
			? NewObject<UCorpseLootComponent>(CorpseActor) : nullptr;

		if (Player && Inv && Corpse && Pistol)
		{
			Corpse->RegisterComponent();
			Corpse->InitLoot(0.0f, { Pistol }, /*bRegisterSearchable=*/false);

			// Окно обыска без Slate: дерева нет (кубики пустые — методы это переживают),
			// модель забора зовём напрямую, как это делает клик по плитке.
			UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
			if (TestNotNull(TEXT("Окно обыска создано"), Window))
			{
				Window->InitCorpseLoot(Corpse, Player);

				TestNull(TEXT("Слот огнестрела до обыска пуст"), Player->GetRangedWeaponInstance());
				TestTrue(TEXT("Пистолет забран из трупа"), Window->TakeItemToBackpack(Pistol));

				// ГЛАВНОЕ (ADR-063 п.2): ствол в РЮКЗАКЕ, слот оружия сам НЕ занят.
				TestTrue(TEXT("Пистолет лежит в рюкзаке"),
					Inv->GetInventoryItems().Contains(Pistol));
				TestNull(TEXT("Слот огнестрела после обыска ВСЁ ЕЩЁ пуст (автоэкип убран)"),
					Player->GetRangedWeaponInstance());

				// «Наденьте в инвентаре»: явный перенос (модель тапа по плитке) работает.
				TestTrue(TEXT("Тап в инвентаре надевает ствол"), Player->TryAdoptRangedWeapon(Pistol));
				TestTrue(TEXT("Ствол занял слот"), Player->GetRangedWeaponInstance() == Pistol);
				TestFalse(TEXT("Из рюкзака ствол ушёл (слот — не рюкзак)"),
					Inv->GetInventoryItems().Contains(Pistol));
			}
		}
		else
		{
			AddError(TEXT("Игрок/труп/пистолет не создались в тестовом мире"));
		}
	}

	Adr063TestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
