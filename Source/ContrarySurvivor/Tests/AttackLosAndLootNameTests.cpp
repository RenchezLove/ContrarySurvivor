// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты фиксов 08-07 (ветка fix/bandit-los-and-loot-names):
//   1) Прямая видимость атак: враг ЗА СТЕНОЙ не наносит урона (ни ближним ударом, ни
//      выстрелом), враг в прямой видимости — наносит. Нож игрока сквозь стену не бьёт.
//   2) Имена лагерного лута: расходник из размещённого пикапа (лагерь с палаткой) имеет
//      человеческое название и служебный ключ, а не заглушку «Предмет».
//
// Паттерн мира — CombatTestWorld (CombatAutomationTests.cpp): транзиентный игровой мир,
// спавн реальных классов, вызовы РЕАЛЬНОЙ боевой логики через QA-обёртки контроллера
// (анти-галлюцинация: тест не копирует math боя, а зовёт его). Стена = бокс BlockAll —
// профиль блокирует и канал Visibility, которым трассируется линия атаки.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Controllers/EnemyAIController.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "AConsumableItem.h"
#include "AAmmoItem.h"
#include "AMeleeWeapon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h" // FURL
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h" // «камера есть» для правила честности выстрела
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h" // FinishSpawningActor (deferred spawn пикапа)
#include "UObject/UnrealType.h"     // FClassProperty/FIntProperty: поля наполнения пикапа
                                    // protected — выставляем как дизайнер, через reflection

static constexpr EAutomationTestFlags LosLootTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Локальная копия обвязки CombatTestWorld (файл тестов у каждого исполнителя свой —
// правило ночной волны: не трогать чужие файлы, поэтому не выносим хелпер в общий заголовок).
namespace LosLootTestWorld
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

	// Стена: блокирующий бокс (профиль BlockAll блокирует канал Visibility трассы атаки).
	// Паттерн пола из Movement.TranslatesOnInput. Убирается из линии SetActorLocation'ом —
	// перенос обновляет сцену запросов синхронно, тик мира не нужен (и не желателен:
	// тик дал бы ИИ-контроллеру выстрелить самому и запустить кулдаун под ногами теста).
	static AActor* SpawnWall(UWorld* World, const FVector& Loc, const FVector& Extent)
	{
		AActor* Wall = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!Wall)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
		Box->InitBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Wall->SetRootComponent(Box);
		Box->SetWorldLocation(Loc);
		return Wall;
	}
}

// ===========================================================================
// 1. Враг за стеной НЕ наносит урона (удар и выстрел), в прямой видимости — наносит.
//    Дословное ТЗ Рината 08-07: «Убедись что бандиты не могут стрелять сквозь здание,
//    теревья и т д». Зовём реальные PerformAttack/PerformRangedAttack через QA-обёртки.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyNoAttackThroughWallTest,
	"ContrarySurvivor.Combat.EnemyLos.NoAttackThroughWall", LosLootTestFlags)
bool FEnemyNoAttackThroughWallTest::RunTest(const FString& Parameters)
{
	UWorld* World = LosLootTestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		// Игрок и бандит на одной оси X, дистанция 400 см: меньше дальности выстрела (1100),
		// больше ближней (90+капсулы). PlayerController нужен «правилу честности» выстрела:
		// без него PerformRangedAttack не стреляет вовсе («нет камеры игрока»); с ним, но без
		// вьюпорта (headless), правило честности не блокирует — как в существующем коде.
		APlayerCharacter* Player = LosLootTestWorld::Spawn<APlayerCharacter>(World, FVector(0.f, 0.f, 100.f));
		AEnemyCharacter* Enemy = LosLootTestWorld::Spawn<AEnemyCharacter>(World, FVector(400.f, 0.f, 100.f));
		APlayerController* PC = LosLootTestWorld::Spawn<APlayerController>(World, FVector::ZeroVector);
		TestNotNull(TEXT("Player spawned"), Player);
		TestNotNull(TEXT("Enemy spawned"), Enemy);
		TestNotNull(TEXT("PlayerController spawned"), PC);

		AEnemyAIController* AI = LosLootTestWorld::Spawn<AEnemyAIController>(World, FVector::ZeroVector);
		TestNotNull(TEXT("Enemy AI controller spawned"), AI);
		if (Player && Enemy && AI)
		{
			AI->Possess(Enemy);

			UStatsComponent* PlayerStats = Player->GetStats();
			TestNotNull(TEXT("Player stats"), PlayerStats);
			if (PlayerStats)
			{
				const float HP0 = PlayerStats->GetHealth();

				// Стена между ними: x=200, перекрывает линию по Y и Z с запасом.
				AActor* Wall = LosLootTestWorld::SpawnWall(World,
					FVector(200.f, 0.f, 100.f), FVector(20.f, 300.f, 300.f));
				TestNotNull(TEXT("Wall spawned"), Wall);

				// --- ЗА СТЕНОЙ: линии нет, атаки не проходят, урона нет ---
				TestFalse(TEXT("LOS blocked by wall"), AI->HasAttackLineOfSightForQA(Player));
				TestFalse(TEXT("Melee attack denied through wall"), AI->PerformAttackForQA(Player));
				TestFalse(TEXT("Ranged attack denied through wall"), AI->PerformRangedAttackForQA(Player));
				TestEqual(TEXT("Player HP unchanged behind wall"), PlayerStats->GetHealth(), HP0);

				// --- СТЕНА УБРАНА: линия свободна, атаки проходят, урон нанесён ---
				// Заблокированные попытки выше НЕ должны были потратить кулдауны
				// (гейт видимости стоит до отметки времени) — иначе эти вызовы легли бы false.
				if (Wall)
				{
					Wall->SetActorLocation(FVector(200.f, 10000.f, 100.f));
				}
				TestTrue(TEXT("LOS clear without wall"), AI->HasAttackLineOfSightForQA(Player));
				TestTrue(TEXT("Melee attack lands in clear LOS"), AI->PerformAttackForQA(Player));
				TestTrue(TEXT("Melee damage applied (HP dropped)"), PlayerStats->GetHealth() < HP0);

				// Выстрел состоялся (true = кулдаун/честность/видимость пройдены). Само
				// попадание вероятностное (RangedHitChance) — HP после выстрела не проверяем.
				TestTrue(TEXT("Ranged attack fires in clear LOS"), AI->PerformRangedAttackForQA(Player));
			}
			else { bOk = false; }
		}
		else { bOk = false; }
	}

	LosLootTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 2. Нож игрока сквозь стену не бьёт («Что в целом нельзя этого делать» — и игроку тоже).
//    Реальный AMeleeWeapon::ApplyMeleeDamage: цель в секторе и в радиусе, но за стеной.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnifeNoMeleeThroughWallTest,
	"ContrarySurvivor.Combat.KnifeLos.NoMeleeThroughWall", LosLootTestFlags)
bool FKnifeNoMeleeThroughWallTest::RunTest(const FString& Parameters)
{
	UWorld* World = LosLootTestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		// Игрок смотрит вдоль +X (нулевой поворот), враг впереди на 140 см: зазор капсул
		// ~70-80 см < MeleeRange 90 — враг в радиусе и в переднем секторе.
		APlayerCharacter* Player = LosLootTestWorld::Spawn<APlayerCharacter>(World, FVector(0.f, 0.f, 100.f));
		AEnemyCharacter* Enemy = LosLootTestWorld::Spawn<AEnemyCharacter>(World, FVector(140.f, 0.f, 100.f));
		AMeleeWeapon* Knife = LosLootTestWorld::Spawn<AMeleeWeapon>(World, FVector(0.f, 0.f, 50.f));
		TestNotNull(TEXT("Player spawned"), Player);
		TestNotNull(TEXT("Enemy spawned"), Enemy);
		TestNotNull(TEXT("Knife spawned"), Knife);

		UStatsComponent* EnemyStats = Enemy ? Enemy->FindComponentByClass<UStatsComponent>() : nullptr;
		TestNotNull(TEXT("Enemy stats"), EnemyStats);

		if (Player && Enemy && Knife && EnemyStats)
		{
			Knife->SetInstigator(Player); // носитель ножа — игрок (ApplyMeleeDamage бьёт от него)
			const float HP0 = EnemyStats->GetHealth();

			AActor* Wall = LosLootTestWorld::SpawnWall(World,
				FVector(70.f, 0.f, 100.f), FVector(10.f, 300.f, 300.f));
			TestNotNull(TEXT("Wall spawned"), Wall);

			// За стеной: взмах есть, урона нет (кандидат отсечён проверкой видимости).
			Knife->ApplyMeleeDamage();
			TestEqual(TEXT("Enemy HP unchanged: knife blocked by wall"), EnemyStats->GetHealth(), HP0);

			// Стена убрана: тот же взмах наносит урон ножа (35, враг без брони).
			if (Wall)
			{
				Wall->SetActorLocation(FVector(70.f, 10000.f, 100.f));
			}
			Knife->ApplyMeleeDamage();
			TestTrue(TEXT("Enemy HP dropped: knife lands without wall"), EnemyStats->GetHealth() < HP0);
		}
		else { bOk = false; }
	}

	LosLootTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 3. Лут лагеря (размещённый пикап): расходник, положенный ГОЛЫМ классом без полей имени —
//    ровно конфигурация «тушёнка называется "Предмет"» из прогона Рината 08-07
//    (лог: ConsumableItem_3 «не заданы ни ItemDisplayText, ни ItemName») — получает
//    человеческое название и служебный ключ. Пачка патронов пикапа — своё штатное имя.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacedPickupLootNamesTest,
	"ContrarySurvivor.Loot.PlacedPickup.ConsumableGetsHumanName", LosLootTestFlags)
bool FPlacedPickupLootNamesTest::RunTest(const FString& Parameters)
{
	UWorld* World = LosLootTestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		// Поля наполнения пикапа protected (их заполняет дизайнер в Details) — выставляем
		// их до BeginPlay так же, как сериализация экземпляра с карты: deferred spawn +
		// reflection по именам свойств. FinishSpawningActor прогоняет BeginPlay ->
		// SpawnPlacedLoot, как у настоящего пикапа лагеря.
		const FTransform T(FRotator::ZeroRotator, FVector(0.f, 0.f, 100.f));
		APickup* Pickup = World->SpawnActorDeferred<APickup>(
			APickup::StaticClass(), T, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		TestNotNull(TEXT("Pickup deferred-spawned"), Pickup);

		if (Pickup)
		{
			FClassProperty* ClassProp = FindFProperty<FClassProperty>(APickup::StaticClass(), TEXT("PlacedItemClass"));
			FIntProperty* CountProp = FindFProperty<FIntProperty>(APickup::StaticClass(), TEXT("PlacedItemCount"));
			FIntProperty* AmmoProp = FindFProperty<FIntProperty>(APickup::StaticClass(), TEXT("PlacedAmmoAmount"));
			TestNotNull(TEXT("PlacedItemClass property found"), ClassProp);
			TestNotNull(TEXT("PlacedItemCount property found"), CountProp);
			TestNotNull(TEXT("PlacedAmmoAmount property found"), AmmoProp);

			if (ClassProp && CountProp && AmmoProp)
			{
				ClassProp->SetPropertyValue_InContainer(Pickup, AConsumableItem::StaticClass());
				CountProp->SetPropertyValue_InContainer(Pickup, 2);
				AmmoProp->SetPropertyValue_InContainer(Pickup, 3);

				UGameplayStatics::FinishSpawningActor(Pickup, T);

				UCorpseLootComponent* Loot = Pickup->GetLootContainer();
				TestNotNull(TEXT("Loot container"), Loot);
				if (Loot)
				{
					TArray<AMasterInventoryItem*> Items = Loot->GetLootItems();
					TestEqual(TEXT("Loot holds 2 consumables + 1 ammo pack"), Items.Num(), 3);

					const FString Stub = TEXT("Предмет");
					int32 Consumables = 0;
					for (AMasterInventoryItem* Item : Items)
					{
						if (!Item)
						{
							AddError(TEXT("Null item in loot container"));
							bOk = false;
							continue;
						}
						const FString Shown = Item->GetItemDisplayText().ToString();
						TestFalse(FString::Printf(TEXT("'%s' display name not empty"), *Item->GetName()), Shown.IsEmpty());
						TestNotEqual(*FString::Printf(TEXT("'%s' is not the stub"), *Item->GetName()), *Shown, *Stub);
						TestFalse(FString::Printf(TEXT("'%s' has non-empty key ItemName"), *Item->GetName()), Item->ItemName.IsEmpty());

						if (AConsumableItem* Cons = Cast<AConsumableItem>(Item))
						{
							++Consumables;
							// Ключ — штатный ключ типа (по нему стак сольётся с купленной
							// такой же тушёнкой; квестовые ключи ADR-050 не задеты — это
							// значения расходника, а не «Шкура волка»/«Ноутбук»).
							TestEqual(FString::Printf(TEXT("'%s' key matches type default"), *Item->GetName()),
								Cons->ItemName, AConsumableItem::GetDefaultDisplayName(Cons->ConsumableType));
						}
					}
					TestEqual(TEXT("Exactly 2 consumables in loot"), Consumables, 2);
				}
			}
			else { bOk = false; }
		}
		else { bOk = false; }
	}

	LosLootTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 4. Страховка отображения: расходник ВООБЩЕ без имён (любой будущий забытый спавнер)
//    показывает штатное название своего типа, а не «Предмет»; заполненные поля главнее.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsumableDisplayNameFallbackTest,
	"ContrarySurvivor.Loot.ConsumableDisplayNameFallback", LosLootTestFlags)
bool FConsumableDisplayNameFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = LosLootTestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		AConsumableItem* Item = LosLootTestWorld::Spawn<AConsumableItem>(World, FVector(0.f, 0.f, 100.f));
		TestNotNull(TEXT("Consumable spawned"), Item);
		if (Item)
		{
			TestTrue(TEXT("Precondition: no explicit names"), Item->ItemName.IsEmpty() && Item->ItemDisplayText.IsEmpty());

			// Пустые поля -> название выводится из типа (та же таблица, что у лута бандита).
			const EConsumableType Types[] = { EConsumableType::Food, EConsumableType::Water, EConsumableType::Medkit };
			for (EConsumableType Type : Types)
			{
				Item->ConsumableType = Type;
				const FString Shown = Item->GetItemDisplayText().ToString();
				TestEqual(FString::Printf(TEXT("Type %d shows its default name"), (int32)Type),
					Shown, AConsumableItem::GetDefaultDisplayText(Type).ToString());
				TestNotEqual(*FString::Printf(TEXT("Type %d not the stub"), (int32)Type),
					*Shown, TEXT("Предмет"));
			}

			// Заполненное поле главнее вычисленного по типу.
			Item->ItemDisplayText = FText::FromString(TEXT("Тестовое имя"));
			TestEqual(TEXT("Explicit display text wins over type default"),
				Item->GetItemDisplayText().ToString(), FString(TEXT("Тестовое имя")));
		}
		else { bOk = false; }
	}

	LosLootTestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
