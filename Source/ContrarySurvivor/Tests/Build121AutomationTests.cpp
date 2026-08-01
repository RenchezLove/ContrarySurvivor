// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны Build 1.2.1 (блоки А1-трупы и Г-стаки). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Build121; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE:
//   - слияние стаков в UInventoryComponent::AddItem (одна точка всех пополнений);
//   - лимит стака и разделение по служебному ключу ItemName;
//   - съедание ОДНОЙ штуки из стака (AConsumableItem::ApplyConsumeEffect);
//   - зачёт item-квеста ПО ШТУКАМ стака + частичное изъятие при сдаче (UQuestComponent);
//   - контейнер лута трупа (UCorpseLootComponent): Init/Take/HasLoot/реестр/EndPlay;
//   - смерть волка и бандита кладёт лут В ТРУП и НЕ спавнит мешок-пикап (ТЗ А1).
// НЕ покрывается headless (нужен PIE/Slate): окно UCorpseLootWidget (кнопки/списки),
//   подсказка «Обыскать [E]» (контроллер), вид маркеров с текстурой (Д3 — отрисовка).
//
// Числа порогов взяты из РЕАЛЬНОГО кода (AAmmoItem/AConsumableItem/AQuestItem ctor,
// WolfCharacter/EnemyCharacter Loot*), не выдуманы.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "AConsumableItem.h"
#include "AQuestItem.h"
#include "AAmmoItem.h"
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h" // FDamageEvent (смертельный урон волку/бандиту)
#include "Engine/EngineBaseTypes.h"
#include "EngineUtils.h" // TActorIterator (проверка «мешок-пикап не заспавнен»)
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags Build121TestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// ---------------------------------------------------------------------------
// Транзиентный игровой мир (копия обвязки CombatTestWorld — она static в своём .cpp).
// ---------------------------------------------------------------------------
namespace Build121TestWorld
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

	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc = FVector(0.f, 0.f, 100.f))
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params);
	}

	// Расходник с заданным типом/ключом/стаком (как его создаёт лут бандита/покупка).
	static AConsumableItem* SpawnConsumable(UWorld* World, EConsumableType Type, int32 Stack)
	{
		AConsumableItem* Item = Spawn<AConsumableItem>(World);
		if (Item)
		{
			Item->ConsumableType = Type;
			Item->ItemName = AConsumableItem::GetDefaultDisplayName(Type);
			Item->ItemDisplayText = AConsumableItem::GetDefaultDisplayText(Type);
			Item->StackCount = Stack;
			Item->SetActorHiddenInGame(true);
			Item->SetActorEnableCollision(false);
		}
		return Item;
	}

	// Живые (валидные) обыскиваемые трупы ИМЕННО этого мира — реестр статический,
	// в нём могут доживать невалидные записи прошлых тестов.
	static int32 CountValidCorpses(UWorld* World)
	{
		int32 Count = 0;
		for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : UCorpseLootComponent::GetSearchableCorpses())
		{
			if (Ptr.IsValid() && Ptr->GetWorld() == World)
			{
				++Count;
			}
		}
		return Count;
	}
}

// ===========================================================================
// 1. Стаки: слияние в AddItem — одинаковые предметы сливаются, разные ключи — нет,
//    лимит 999 соблюдается (остаток отдельной записью).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild121StackMergeTest,
	"ContrarySurvivor.Build121.Stacks.InventoryMerge", Build121TestFlags)
bool FBuild121StackMergeTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build121TestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		APlayerCharacter* Player = Build121TestWorld::Spawn<APlayerCharacter>(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		TestNotNull(TEXT("Inventory"), Inv);
		if (Inv)
		{
			// (a) Две тушёнки -> один стак x2, второй актор уничтожен (слился целиком).
			AConsumableItem* FoodA = Build121TestWorld::SpawnConsumable(World, EConsumableType::Food, 1);
			AConsumableItem* FoodB = Build121TestWorld::SpawnConsumable(World, EConsumableType::Food, 1);
			TestTrue(TEXT("Add food A"), Inv->AddItem(FoodA));
			TestTrue(TEXT("Add food B (merges)"), Inv->AddItem(FoodB));
			TestEqual(TEXT("One inventory entry after merge"), Inv->GetInventoryItems().Num(), 1);
			TestEqual(TEXT("Stack A = 2"), FoodA->GetStackCount(), 2);
			TestFalse(TEXT("Merged actor B destroyed (pending kill)"), IsValid(FoodB));

			// (b) Вода — ДРУГОЙ ключ ItemName при том же классе: отдельная запись.
			AConsumableItem* Water = Build121TestWorld::SpawnConsumable(World, EConsumableType::Water, 1);
			TestTrue(TEXT("Add water"), Inv->AddItem(Water));
			TestEqual(TEXT("Water is separate entry"), Inv->GetInventoryItems().Num(), 2);
			TestEqual(TEXT("Food stack untouched"), FoodA->GetStackCount(), 2);

			// (c) Лимит 999 (как у патронов): доливка до потолка, остаток — новой записью.
			FoodA->StackCount = 998;
			AConsumableItem* FoodC = Build121TestWorld::SpawnConsumable(World, EConsumableType::Food, 3);
			TestTrue(TEXT("Add food C (partial merge)"), Inv->AddItem(FoodC));
			TestEqual(TEXT("Stack A capped at 999"), FoodA->GetStackCount(), 999);
			TestTrue(TEXT("Remainder actor kept"), IsValid(FoodC));
			TestEqual(TEXT("Remainder stack = 2"), FoodC->GetStackCount(), 2);
			TestEqual(TEXT("Three entries now (food full + water + remainder)"),
				Inv->GetInventoryItems().Num(), 3);

			// (d) Шкура волка (AQuestItem) стакается тем же механизмом.
			AQuestItem* PeltA = Build121TestWorld::Spawn<AQuestItem>(World);
			AQuestItem* PeltB = Build121TestWorld::Spawn<AQuestItem>(World);
			if (PeltA && PeltB)
			{
				PeltA->ItemName = TEXT("Шкура волка");
				PeltB->ItemName = TEXT("Шкура волка");
				Inv->AddItem(PeltA);
				Inv->AddItem(PeltB);
				TestEqual(TEXT("Pelt stack = 2"), PeltA->GetStackCount(), 2);
				TestFalse(TEXT("Second pelt merged away"), IsValid(PeltB));
			}
			else { bOk = false; }

			// (e) Дефолты классов: расходник/квест-предмет стакаемы (999), пачка патронов
			// прежняя (0/999), база нестакаема (1/1) — числа из конструкторов.
			TestEqual(TEXT("Consumable MaxStack 999"), GetDefault<AConsumableItem>()->MaxStackCount, 999);
			TestEqual(TEXT("QuestItem MaxStack 999"), GetDefault<AQuestItem>()->MaxStackCount, 999);
			TestEqual(TEXT("Ammo default stack 0"), GetDefault<AAmmoItem>()->StackCount, 0);
			TestEqual(TEXT("Ammo MaxStack 999"), GetDefault<AAmmoItem>()->MaxStackCount, 999);
		}
		else { bOk = false; }
	}
	Build121TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 2. Стаки: ApplyConsumeEffect съедает ОДНУ штуку — false (актор жив), пока стак >1;
//    последняя штука возвращает true (прежний путь «убрать и уничтожить»).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild121ConsumeFromStackTest,
	"ContrarySurvivor.Build121.Stacks.ConsumeOneFromStack", Build121TestFlags)
bool FBuild121ConsumeFromStackTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build121TestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		UStatsComponent* Stats = NewObject<UStatsComponent>();
		AConsumableItem* Food = Build121TestWorld::SpawnConsumable(World, EConsumableType::Food, 3);
		TestNotNull(TEXT("Stats"), Stats);
		TestNotNull(TEXT("Food"), Food);
		if (Stats && Food)
		{
			TestFalse(TEXT("Eat 1 of 3: item NOT fully consumed"), Food->ApplyConsumeEffect(Stats));
			TestEqual(TEXT("Stack 2 left"), Food->GetStackCount(), 2);
			TestFalse(TEXT("Eat 2 of 3: item NOT fully consumed"), Food->ApplyConsumeEffect(Stats));
			TestEqual(TEXT("Stack 1 left"), Food->GetStackCount(), 1);
			TestTrue(TEXT("Last one: item fully consumed (caller destroys)"), Food->ApplyConsumeEffect(Stats));

			// Null-статы — эффект не применён и стак не тронут.
			Food->StackCount = 2;
			TestFalse(TEXT("Null stats -> no effect"), Food->ApplyConsumeEffect(nullptr));
			TestEqual(TEXT("Stack untouched on failure"), Food->GetStackCount(), 2);
		}
		else { bOk = false; }
	}
	Build121TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 3. Стаки: квест «3 шкуры» — зачёт ПО ШТУКАМ одного стака и ЧАСТИЧНОЕ изъятие при
//    сдаче (5 шкур в стаке -> сдали 3 -> в стаке осталось 2). ТЗ Г: «зачёт квеста
//    "3 шкуры" НЕ сломать».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild121QuestStackTest,
	"ContrarySurvivor.Build121.Stacks.QuestCountAndPartialTurnIn", Build121TestFlags)
bool FBuild121QuestStackTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build121TestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		APlayerCharacter* Player = Build121TestWorld::Spawn<APlayerCharacter>(World);
		UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		TestNotNull(TEXT("Quests"), Quests);
		TestNotNull(TEXT("Inventory"), Inv);
		if (Quests && Inv)
		{
			// Item-квест как у старосты: 3 шкуры (значение ключа — как в ElderNPC.cpp).
			FQuest Q;
			Q.QuestId = FName(TEXT("TestPelts"));
			Q.Title = FText::FromString(TEXT("Test pelts"));
			Q.Type = EQuestType::Collect;
			Q.TargetCount = 0;
			Q.RequiredItemName = TEXT("Шкура волка");
			Q.RequiredItemCount = 3;
			Q.RewardMoney = 100.0f;
			Quests->OfferQuest(Q);
			TestTrue(TEXT("Accept"), Quests->AcceptQuest(Q.QuestId));

			// ОДИН актор-стак из 5 шкур (волки убиты, всё слилось при заборе из трупов).
			AQuestItem* Pelts = Build121TestWorld::Spawn<AQuestItem>(World);
			if (Pelts)
			{
				Pelts->ItemName = TEXT("Шкура волка");
				Pelts->StackCount = 5;
				Inv->AddItem(Pelts);

				Quests->SyncInventoryQuests(Inv);
				const FQuest* Synced = Quests->FindQuest(Q.QuestId);
				TestNotNull(TEXT("Quest found"), Synced);
				if (Synced)
				{
					TestEqual(TEXT("Item progress = 3 (по штукам, кламп целью)"), Synced->ItemProgress, 3);
					TestTrue(TEXT("Completed by one stack"), Synced->State == EQuestState::Completed);
				}

				TestTrue(TEXT("Turn-in succeeds"), Quests->TurnInQuest(Q.QuestId));
				TestTrue(TEXT("Stack actor survives partial take"), IsValid(Pelts));
				TestEqual(TEXT("2 pelts left in stack after turning in 3"), Pelts->GetStackCount(), 2);
				TestEqual(TEXT("Stack entry still in inventory"), Inv->GetInventoryItems().Num(), 1);
			}
			else { bOk = false; }
		}
		else { bOk = false; }
	}
	Build121TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 4. Труп: контейнер UCorpseLootComponent — Init/Take/HasLoot/реестр/EndPlay.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild121CorpseContainerTest,
	"ContrarySurvivor.Build121.Corpse.ContainerTakeAndExpire", Build121TestFlags)
bool FBuild121CorpseContainerTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build121TestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		AActor* Corpse = Build121TestWorld::Spawn<AActor>(World);
		TestNotNull(TEXT("Corpse actor"), Corpse);
		UCorpseLootComponent* Loot = Corpse ? NewObject<UCorpseLootComponent>(Corpse) : nullptr;
		if (Loot)
		{
			Loot->RegisterComponent();

			AConsumableItem* Item = Build121TestWorld::SpawnConsumable(World, EConsumableType::Medkit, 1);
			TArray<AMasterInventoryItem*> Items;
			Items.Add(Item);
			Loot->InitLoot(25.0f, Items);

			TestEqual(TEXT("Registered as searchable"), Build121TestWorld::CountValidCorpses(World), 1);
			TestTrue(TEXT("Has loot"), Loot->HasLoot());

			// Деньги забираются один раз.
			TestEqual(TEXT("Take money 25"), Loot->TakeMoney(), 25.0f);
			TestEqual(TEXT("Second take = 0"), Loot->TakeMoney(), 0.0f);
			TestTrue(TEXT("Still has loot (item left)"), Loot->HasLoot());

			// Предмет изымается один раз (двойной клик по устаревшей строке = false).
			TestTrue(TEXT("Take item"), Loot->TakeItem(Item));
			TestFalse(TEXT("Item cannot be taken twice"), Loot->TakeItem(Item));
			TestFalse(TEXT("Empty after full search"), Loot->HasLoot());
			TestTrue(TEXT("Taken item stays alive (goes to backpack)"), IsValid(Item));

			// Полный обыск НЕ убирает труп из реестра — он лежит до таймера (дефолт Рината);
			// подсказку «Обыскать» гасит HasLoot()=false на стороне контроллера.
			TestEqual(TEXT("Corpse still registered until expire"), Build121TestWorld::CountValidCorpses(World), 1);

			// Второй труп с НЕ забранным предметом: исчез по «таймеру» (DestroyActor) —
			// снят с реестра, остаток уничтожен (не висит в мире).
			AActor* Corpse2 = Build121TestWorld::Spawn<AActor>(World);
			UCorpseLootComponent* Loot2 = Corpse2 ? NewObject<UCorpseLootComponent>(Corpse2) : nullptr;
			AConsumableItem* Leftover = Build121TestWorld::SpawnConsumable(World, EConsumableType::Food, 2);
			if (Loot2 && Leftover)
			{
				Loot2->RegisterComponent();
				TArray<AMasterInventoryItem*> Items2;
				Items2.Add(Leftover);
				Loot2->InitLoot(10.0f, Items2);
				TestEqual(TEXT("Two corpses registered"), Build121TestWorld::CountValidCorpses(World), 2);

				World->DestroyActor(Corpse2);
				TestEqual(TEXT("Expired corpse unregistered"), Build121TestWorld::CountValidCorpses(World), 1);
				TestFalse(TEXT("Leftover item destroyed with corpse"), IsValid(Leftover));
			}
			else { bOk = false; }
		}
		else { bOk = false; }
	}
	Build121TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 5. Труп: смерть ВОЛКА кладёт шкуру и деньги в труп и НЕ спавнит мешок-пикап (ТЗ А1);
//    таймер трупа = 60 с (дефолт волны у волка и бандита).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild121WolfCorpseTest,
	"ContrarySurvivor.Build121.Corpse.WolfDeathLootStaysInCorpse", Build121TestFlags)
bool FBuild121WolfCorpseTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build121TestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		AWolfCharacter* Wolf = Build121TestWorld::Spawn<AWolfCharacter>(World);
		TestNotNull(TEXT("Wolf"), Wolf);
		if (Wolf)
		{
			// Смертельный урон штатным путём (TakeDamage -> UStatsComponent -> HandleDeath).
			Wolf->TakeDamage(1000.0f, FDamageEvent(), nullptr, nullptr);
			TestTrue(TEXT("Wolf dead"), Wolf->GetStats() && Wolf->GetStats()->IsDead());

			UCorpseLootComponent* Loot = Wolf->FindComponentByClass<UCorpseLootComponent>();
			TestNotNull(TEXT("Wolf has corpse loot component"), Loot);
			if (Loot)
			{
				TestTrue(TEXT("Corpse has loot"), Loot->HasLoot());
				TestTrue(TEXT("Money in wolf range 5..15"), Loot->GetMoney() >= 5.0f && Loot->GetMoney() <= 15.0f);

				const TArray<AMasterInventoryItem*> Items = Loot->GetLootItems();
				TestEqual(TEXT("Exactly one item (pelt)"), Items.Num(), 1);
				if (Items.Num() == 1)
				{
					TestEqual(TEXT("Pelt key intact (quest!)"), Items[0]->ItemName, FString(TEXT("Шкура волка")));
					TestEqual(TEXT("Pelt stack = 1"), Items[0]->GetStackCount(), 1);
					TestTrue(TEXT("Pelt is stackable"), Items[0]->IsStackable());
				}
			}

			// Мешок-пикап больше НЕ спавнится (ТЗ А1: «мешок с трупов убрать»).
			int32 Pickups = 0;
			for (TActorIterator<APickup> It(World); It; ++It) { ++Pickups; }
			TestEqual(TEXT("No pickup spawned on wolf death"), Pickups, 0);

			// Таймер трупа: дефолт волны 60 с (ТЗ А1), сам труп ещё жив.
			TestEqual(TEXT("Wolf corpse lifespan 60s"), Wolf->GetLifeSpan(), 60.0f);
			TestTrue(TEXT("Corpse actor alive until timer"), IsValid(Wolf));
		}
		else { bOk = false; }
	}
	Build121TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 6. Труп: смерть БАНДИТА — деньги в трупе (10..30), мешок-пикап не спавнится,
//    таймер 60 с. Состав предметов вероятностный (35%) — количество не проверяем.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild121BanditCorpseTest,
	"ContrarySurvivor.Build121.Corpse.BanditDeathLootStaysInCorpse", Build121TestFlags)
bool FBuild121BanditCorpseTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build121TestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		AEnemyCharacter* Bandit = Build121TestWorld::Spawn<AEnemyCharacter>(World);
		TestNotNull(TEXT("Bandit"), Bandit);
		if (Bandit)
		{
			Bandit->TakeDamage(1000.0f, FDamageEvent(), nullptr, nullptr);
			TestTrue(TEXT("Bandit dead"), Bandit->GetStats() && Bandit->GetStats()->IsDead());

			UCorpseLootComponent* Loot = Bandit->FindComponentByClass<UCorpseLootComponent>();
			TestNotNull(TEXT("Bandit has corpse loot component"), Loot);
			if (Loot)
			{
				TestTrue(TEXT("Corpse has loot"), Loot->HasLoot());
				TestTrue(TEXT("Money in bandit range 10..30"), Loot->GetMoney() >= 10.0f && Loot->GetMoney() <= 30.0f);
			}

			int32 Pickups = 0;
			for (TActorIterator<APickup> It(World); It; ++It) { ++Pickups; }
			TestEqual(TEXT("No pickup spawned on bandit death"), Pickups, 0);

			TestEqual(TEXT("Bandit corpse lifespan 60s"), Bandit->GetLifeSpan(), 60.0f);
		}
		else { bOk = false; }
	}
	Build121TestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
