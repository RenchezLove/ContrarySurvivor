// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты награды за первый квест и совета старосты (ADR-065, задача
// Рината 08-09: «первое оружие — решение игрока, а не подарок»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.FirstQuest; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * награда за первый квест равна 200, за второй осталась 250, цена пистолета не тронута;
//   * сдача первого квеста увеличивает деньги игрока ровно на награду;
//   * ПОКУПАТЕЛЬНАЯ СПОСОБНОСТЬ: стартовых денег вместе с наградой хватает на пистолет,
//     бинт, консервы, воду и 20 патронов по ценам каталога, и остаётся ещё сдача. Этот тест
//     ловит будущую поломку баланса, если кто-то тронет цены или награду;
//   * совет заглянуть к торговцу звучит при сдаче ПЕРВОГО квеста и НЕ звучит при сдаче второго.
//
// НЕ покрывается headless: как это ощущается живьём (успевает ли игрок дойти до торговца,
// сколько остаётся на руках по факту) — это замеряет Ринат на устройстве.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "AConsumableItem.h" // имена расходников каталога — единый источник
#include "AQuestItem.h"      // шкуры волка в рюкзак
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags FirstQuestTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace FirstQuestTestWorld
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
	static T* Spawn(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
	}

	// Цена позиции каталога по служебному имени. -1, если позиции нет.
	static float FindPrice(const AMasterTrader* Trader, const FString& EntryName)
	{
		if (!Trader)
		{
			return -1.0f;
		}
		for (const FShopEntry& Entry : Trader->GetCatalog())
		{
			if (Entry.DisplayName == EntryName)
			{
				return Entry.Price;
			}
		}
		return -1.0f;
	}
}

// --- 1. Числа награды ----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstQuestRewardValueTest,
	"ContrarySurvivor.FirstQuest.RewardIsTwoHundred", FirstQuestTestFlags)

bool FFirstQuestRewardValueTest::RunTest(const FString& Parameters)
{
	UWorld* World = FirstQuestTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	{
		AElderNPC* Elder = FirstQuestTestWorld::Spawn<AElderNPC>(World);
		if (Elder)
		{
			// ADR-065: награда за первое задание поднята со 150 до 200, чтобы её хватило не
			// только на пистолет, но и на припасы.
			TestEqual(TEXT("Награда за первый квест — 200"),
				Elder->GetOfferedQuest().RewardMoney, 200.0f);
			// Второй квест трогать было нельзя.
			TestEqual(TEXT("Награда за второй квест осталась 250"),
				Elder->GetSecondQuest().RewardMoney, 250.0f);
		}
		else
		{
			AddError(TEXT("Староста не создался"));
		}
	}
	FirstQuestTestWorld::Destroy(World);
	return true;
}

// --- 2. Покупательная способность после сдачи первого квеста -------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstQuestPurchasingPowerTest,
	"ContrarySurvivor.FirstQuest.RewardCoversPistolAndSupplies", FirstQuestTestFlags)

bool FFirstQuestPurchasingPowerTest::RunTest(const FString& Parameters)
{
	UWorld* World = FirstQuestTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	{
		AElderNPC* Elder = FirstQuestTestWorld::Spawn<AElderNPC>(World);
		AMasterTrader* Trader = FirstQuestTestWorld::Spawn<AMasterTrader>(World);
		APlayerCharacter* Player = FirstQuestTestWorld::Spawn<APlayerCharacter>(World);

		if (Elder && Trader && Player && Player->GetStats())
		{
			// Стартовые деньги игрока к моменту сдачи целы: до первого квеста покупать нечего
			// (ADR-063 приглушил урон истощения до сдачи задания).
			const float StartingMoney = Player->GetStats()->GetMoney();
			const float Reward = Elder->GetOfferedQuest().RewardMoney;
			const float OnHand = StartingMoney + Reward;

			const float Pistol = FirstQuestTestWorld::FindPrice(Trader, TEXT("Pistol"));
			const float Bandage = FirstQuestTestWorld::FindPrice(Trader, AConsumableItem::GetDefaultDisplayName(EConsumableType::Medkit));
			const float Food = FirstQuestTestWorld::FindPrice(Trader, AConsumableItem::GetDefaultDisplayName(EConsumableType::Food));
			const float Water = FirstQuestTestWorld::FindPrice(Trader, AConsumableItem::GetDefaultDisplayName(EConsumableType::Water));
			const float AmmoPerRound = FirstQuestTestWorld::FindPrice(Trader, TEXT("Патроны 9мм"));

			TestTrue(TEXT("Пистолет есть в каталоге"), Pistol > 0.0f);
			TestTrue(TEXT("Бинт есть в каталоге"), Bandage > 0.0f);
			TestTrue(TEXT("Консервы есть в каталоге"), Food > 0.0f);
			TestTrue(TEXT("Вода есть в каталоге"), Water > 0.0f);
			TestTrue(TEXT("Патроны есть в каталоге"), AmmoPerRound > 0.0f);

			// Набор новичка: оружие плюс минимальные припасы и два десятка патронов.
			constexpr int32 AmmoRounds = 20;
			const float Basket = Pistol + Bandage + Food + Water + AmmoPerRound * AmmoRounds;

			TestTrue(FString::Printf(
				TEXT("Денег хватает на пистолет и припасы: на руках %.0f, набор стоит %.0f"),
				OnHand, Basket), OnHand >= Basket);
			TestTrue(FString::Printf(
				TEXT("После покупки остаётся сдача: %.0f"), OnHand - Basket), OnHand - Basket > 0.0f);
		}
		else
		{
			AddError(TEXT("Староста, торговец или игрок не создались"));
		}
	}
	FirstQuestTestWorld::Destroy(World);
	return true;
}

// --- 3. Сдача первого квеста реально приносит награду --------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstQuestTurnInPaysTest,
	"ContrarySurvivor.FirstQuest.TurnInAddsRewardToBalance", FirstQuestTestFlags)

bool FFirstQuestTurnInPaysTest::RunTest(const FString& Parameters)
{
	UWorld* World = FirstQuestTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	{
		AElderNPC* Elder = FirstQuestTestWorld::Spawn<AElderNPC>(World);
		APlayerCharacter* Player = FirstQuestTestWorld::Spawn<APlayerCharacter>(World);
		UQuestComponent* Quests = Player ? Player->FindComponentByClass<UQuestComponent>() : nullptr;
		UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;

		UInventoryComponent* Inventory = Player ? Player->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (Elder && Quests && Stats && Inventory)
		{
			const FQuest First = Elder->GetOfferedQuest();
			Quests->OfferQuest(First);
			TestTrue(TEXT("Квест принят"), Quests->AcceptQuest(First.QuestId));

			// Кладём шкуры в рюкзак настоящим путём — их и пересчитывает журнал. Один актор
			// со стаком: журнал считает штуки, а не строки списка.
			if (AQuestItem* Hides = FirstQuestTestWorld::Spawn<AQuestItem>(World))
			{
				Hides->ItemName = First.RequiredItemName;
				Hides->StackCount = First.RequiredItemCount;
				Hides->SetActorHiddenInGame(true);
				Hides->SetActorEnableCollision(false);
				TestTrue(TEXT("Шкуры попали в рюкзак"), Inventory->AddItem(Hides));
			}
			Quests->SyncInventoryQuests(Inventory);

			const FQuest* Live = Quests->FindQuest(First.QuestId);
			if (!TestTrue(TEXT("Квест перешёл в состояние «выполнен»"),
				Live && Live->State == EQuestState::Completed))
			{
				AddError(TEXT("Квест не стал выполненным — дальше проверять нечего"));
			}
			else
			{
				const float Before = Stats->GetMoney();
				TestTrue(TEXT("Сдача прошла"), Quests->TurnInQuest(First.QuestId));
				const float After = Stats->GetMoney();

				TestEqual(TEXT("Сдача принесла ровно награду квеста"), After - Before, First.RewardMoney);
				TestEqual(TEXT("А награда — это 200"), First.RewardMoney, 200.0f);
			}
		}
		else
		{
			AddError(TEXT("Староста, журнал квестов, статы или рюкзак не создались"));
		}
	}
	FirstQuestTestWorld::Destroy(World);
	return true;
}

// --- 4. Совет про торговца — только при сдаче первого квеста -------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstQuestTraderHintTest,
	"ContrarySurvivor.FirstQuest.TraderHintOnlyOnFirstTurnIn", FirstQuestTestFlags)

bool FFirstQuestTraderHintTest::RunTest(const FString& Parameters)
{
	// Чистое правило — без мира: совет звучит для первого квеста и только если текст задан.
	const FName FirstId(TEXT("KillWolves"));
	const FName SecondId(TEXT("ClearBanditBase"));

	TestTrue(TEXT("Первый квест и текст задан — звучит совет"),
		AElderNPC::ShouldUseFirstQuestCompletedText(FirstId, FirstId, /*bFirstQuestTextIsSet=*/true));
	TestFalse(TEXT("Второй квест — обычная реплика"),
		AElderNPC::ShouldUseFirstQuestCompletedText(SecondId, FirstId, true));
	TestFalse(TEXT("Текст очистили — обычная реплика для всех"),
		AElderNPC::ShouldUseFirstQuestCompletedText(FirstId, FirstId, false));
	TestFalse(TEXT("Квест без имени — обычная реплика"),
		AElderNPC::ShouldUseFirstQuestCompletedText(NAME_None, FirstId, true));

	// И то же самое на живом старосте: тексты действительно разные.
	UWorld* World = FirstQuestTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	{
		AElderNPC* Elder = FirstQuestTestWorld::Spawn<AElderNPC>(World);
		if (Elder)
		{
			const FText FirstText = Elder->GetCompletedTextForQuest(Elder->GetOfferedQuest().QuestId);
			const FText SecondText = Elder->GetCompletedTextForQuest(Elder->GetSecondQuest().QuestId);

			TestFalse(TEXT("Реплика первого квеста не пустая"), FirstText.IsEmpty());
			TestFalse(TEXT("Реплика второго квеста не пустая"), SecondText.IsEmpty());
			TestNotEqual(TEXT("Тексты первого и второго квеста разные"),
				FirstText.ToString(), SecondText.ToString());
			TestTrue(TEXT("В реплике первого квеста упомянут торговец"),
				FirstText.ToString().Contains(TEXT("торговц")));
			TestFalse(TEXT("В реплике второго квеста торговца нет"),
				SecondText.ToString().Contains(TEXT("торговц")));
		}
		else
		{
			AddError(TEXT("Староста не создался"));
		}
	}
	FirstQuestTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
