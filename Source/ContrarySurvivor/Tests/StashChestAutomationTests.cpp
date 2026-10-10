// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты личного сундука (Ринат 09.10.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Stash; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат:
//   - положенное в одном сундуке видно в другом (хранилище одно на мир);
//   - перенос стопки целиком и оружия с его магазином — туда и обратно; одинаковые стопки в
//     сундуке сливаются; надетое не переносится;
//   - сохранение и «Продолжить» возвращают содержимое сундука; сохранение, записанное до
//     сундука (без его записей), даёт пустой сундук; «Новая игра» сундук очищает;
//   - подсказка действия называет сундук, свечение усиливается, когда игрок рядом.
//
// Перенос идёт теми же вызовами, что нажатия на плитки (UCorpseLootWidget::TakeItemFromPlayer
// и TakeItemToBackpack) — живой Slate в прогонах проекта не поднимается.
// Слот сохранения ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter) — боевой слот не трогается.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/StashChest.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/CorpseLootWidget.h"
#include "UInventoryComponent.h"
#include "AAmmoItem.h"
#include "AHeadArmor.h"
#include "APistol.h"
#include "ARangedWeapon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h" // GetTransientPackage
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags StashTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки CombatAutomationTests.cpp — статики свои на файл).
namespace StashTestWorld
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

	// Окно обыска над контейнером сундука — без живого Slate, только его логика переноса.
	static UCorpseLootWidget* OpenWindow(UWorld* World, UCorpseLootComponent* Container, APlayerCharacter* Player)
	{
		UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
		if (Window)
		{
			Window->InitCorpseLoot(Container, Player);
		}
		return Window;
	}

	// Сколько патронов данного вида лежит в контейнере.
	static int32 CountAmmoIn(const UCorpseLootComponent* Container, FName AmmoType)
	{
		int32 Total = 0;
		if (Container)
		{
			for (const AMasterInventoryItem* Item : Container->GetLootItems())
			{
				const AAmmoItem* Ammo = Cast<AAmmoItem>(Item);
				if (Ammo && Ammo->GetAmmoType() == AmmoType)
				{
					Total += Ammo->GetStackCount();
				}
			}
		}
		return Total;
	}

	static AAmmoItem* FindAmmoPack(const APlayerCharacter* Player, FName AmmoType)
	{
		const UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (Inv)
		{
			for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
			{
				AAmmoItem* Ammo = Cast<AAmmoItem>(Item);
				if (Ammo && Ammo->GetAmmoType() == AmmoType)
				{
					return Ammo;
				}
			}
		}
		return nullptr;
	}
}

// ===========================================================================
// 1. Одно хранилище на все сундуки; перенос стопки и оружия с магазином; слияние стопок.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStashSharedBetweenChestsTest,
	"ContrarySurvivor.Stash.SharedContentAndTransfers", StashTestFlags)

bool FStashSharedBetweenChestsTest::RunTest(const FString& Parameters)
{
	UWorld* World = StashTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FName Type9 = ContraryAmmo::DefaultType();
	APlayerCharacter* Player = StashTestWorld::Spawn<APlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AStashChest* VillageChest = StashTestWorld::Spawn<AStashChest>(World, FVector(300.0f, 0.0f, 0.0f));
	AStashChest* BaseChest = StashTestWorld::Spawn<AStashChest>(World, FVector(9000.0f, 0.0f, 0.0f));
	UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
	if (!Player || !Inv || !VillageChest || !BaseChest)
	{
		AddError(TEXT("Не заспавнены игрок или сундуки"));
		StashTestWorld::Destroy(World);
		return false;
	}

	UCorpseLootComponent* StashA = VillageChest->GetStashContainer();
	UCorpseLootComponent* StashB = BaseChest->GetStashContainer();
	TestNotNull(TEXT("У сундука есть хранилище"), StashA);
	TestTrue(TEXT("Оба сундука открывают ОДНО хранилище"), StashA && StashA == StashB);
	if (!StashA)
	{
		StashTestWorld::Destroy(World);
		return false;
	}
	TestFalse(TEXT("Новый сундук пуст"), StashA->HasLoot());
	TestTrue(TEXT("Сундуки стоят в реестре"), AStashChest::GetActiveChests().Num() >= 2);

	// Зона взаимодействия: рядом — да, издалека — нет.
	TestTrue(TEXT("Игрок рядом с сундуком деревни"), VillageChest->IsWithinInteractRange(FVector(450.0f, 0.0f, 100.0f)));
	TestFalse(TEXT("Игрок далеко от сундука базы"), BaseChest->IsWithinInteractRange(Player->GetActorLocation()));

	// Свечение усиливается, когда игрок рядом, и возвращается обратно.
	const float IdleGlow = VillageChest->GetCurrentGlowStrength();
	VillageChest->SetPlayerNearby(true);
	TestTrue(TEXT("Рядом с игроком сундук светится сильнее"), VillageChest->GetCurrentGlowStrength() > IdleGlow);
	VillageChest->SetPlayerNearby(false);
	TestEqual(TEXT("Игрок ушёл — свечение прежнее"), VillageChest->GetCurrentGlowStrength(), IdleGlow);

	// --- В сумке: 30 патронов и пистолет с начатым магазином ---
	Player->AddAmmoToInventory(30);
	APistol* Pistol = StashTestWorld::Spawn<APistol>(World, FVector(0.0f, 0.0f, 100.0f));
	AHeadArmor* Helmet = StashTestWorld::Spawn<AHeadArmor>(World, FVector(0.0f, 0.0f, 100.0f));
	AActor* Dummy = StashTestWorld::Spawn<AActor>(World, FVector(500.0f, 0.0f, 100.0f));
	if (!Pistol || !Helmet || !Dummy)
	{
		AddError(TEXT("Не заспавнены предметы"));
		StashTestWorld::Destroy(World);
		return false;
	}
	Pistol->DrainReserveAmmo();
	Pistol->Fire(Dummy);
	Pistol->SetActorHiddenInGame(true);
	Pistol->SetActorEnableCollision(false);
	const int32 PistolClip = Pistol->GetCurrentAmmoInClip();
	TestTrue(TEXT("Магазин пистолета начат"), PistolClip < Pistol->GetMaxAmmoInClip());
	Inv->AddItem(Pistol);
	Helmet->SetActorHiddenInGame(true);
	Helmet->SetActorEnableCollision(false);
	Inv->AddItem(Helmet);
	Player->EquipArmor(Helmet);
	Inv->SetItemEquipped(Helmet, true);

	// --- Кладём в сундук ДЕРЕВНИ (теми же вызовами, что нажатия на плитки сумки) ---
	UCorpseLootWidget* WindowA = StashTestWorld::OpenWindow(World, StashA, Player);
	if (!TestNotNull(TEXT("Окно сундука создано"), WindowA))
	{
		StashTestWorld::Destroy(World);
		return false;
	}
	AAmmoItem* Pack = StashTestWorld::FindAmmoPack(Player, Type9);
	TestNotNull(TEXT("Пачка патронов в сумке"), Pack);
	TestTrue(TEXT("Стопка патронов ушла в сундук"), Pack && WindowA->TakeItemFromPlayer(Pack));
	TestEqual(TEXT("В сумке патронов не осталось (стопка ушла целиком)"), Player->GetReserveAmmoInInventory(), 0);
	TestEqual(TEXT("В сундуке вся стопка"), StashTestWorld::CountAmmoIn(StashA, Type9), 30);

	TestTrue(TEXT("Пистолет ушёл в сундук"), WindowA->TakeItemFromPlayer(Pistol));
	TestFalse(TEXT("Пистолета в сумке нет"), Inv->GetInventoryItems().Contains(Pistol));
	TestTrue(TEXT("Пистолет лежит в сундуке"), StashA->GetLootItems().Contains(Pistol));

	TestFalse(TEXT("Надетая броня в сундук не кладётся"), WindowA->TakeItemFromPlayer(Helmet));
	TestTrue(TEXT("Надетая броня осталась у игрока"), Inv->GetInventoryItems().Contains(Helmet));

	// Вторая стопка тех же патронов сливается с лежащей, а не ложится второй плиткой.
	Player->AddAmmoToInventory(12);
	AAmmoItem* SecondPack = StashTestWorld::FindAmmoPack(Player, Type9);
	TestTrue(TEXT("Вторая стопка ушла в сундук"), SecondPack && WindowA->TakeItemFromPlayer(SecondPack));
	TestEqual(TEXT("В сундуке патронов 42"), StashTestWorld::CountAmmoIn(StashA, Type9), 42);
	int32 AmmoTiles = 0;
	for (const AMasterInventoryItem* Item : StashA->GetLootItems())
	{
		AmmoTiles += Cast<AAmmoItem>(Item) ? 1 : 0;
	}
	TestEqual(TEXT("Патроны в сундуке одной стопкой"), AmmoTiles, 1);

	// --- Сундук БАЗЫ показывает то же самое; забираем оттуда ---
	UCorpseLootWidget* WindowB = StashTestWorld::OpenWindow(World, StashB, Player);
	if (!TestNotNull(TEXT("Окно второго сундука создано"), WindowB))
	{
		StashTestWorld::Destroy(World);
		return false;
	}
	TestTrue(TEXT("Во втором сундуке виден пистолет"), StashB->GetLootItems().Contains(Pistol));
	TestEqual(TEXT("Во втором сундуке видны патроны"), StashTestWorld::CountAmmoIn(StashB, Type9), 42);

	TestTrue(TEXT("Пистолет забран из второго сундука"), WindowB->TakeItemToBackpack(Pistol));
	TestTrue(TEXT("Пистолет снова в сумке"), Inv->GetInventoryItems().Contains(Pistol));
	TestEqual(TEXT("Магазин пистолета сохранился, пока он лежал в сундуке"), Pistol->GetCurrentAmmoInClip(), PistolClip);
	TestEqual(TEXT("Запас при пистолете не появился из воздуха"), Pistol->GetCurrentAmmoReserve(), 0);

	AMasterInventoryItem* StashedAmmo = nullptr;
	for (AMasterInventoryItem* Item : StashB->GetLootItems())
	{
		if (Cast<AAmmoItem>(Item))
		{
			StashedAmmo = Item;
		}
	}
	TestTrue(TEXT("Патроны забраны из второго сундука"), StashedAmmo && WindowB->TakeItemToBackpack(StashedAmmo));
	TestEqual(TEXT("В сумке снова 42 патрона"), Player->GetReserveAmmoInInventory(), 42);
	TestFalse(TEXT("Сундук опустел — и не исчез"), StashA->HasLoot());
	TestTrue(TEXT("Хранилище живо после опустошения"), IsValid(StashA->GetOwner()) && !StashA->IsSinking() && !StashA->IsDissolving());

	// --- Подсказка действия у сундука своя ---
	if (AContrarySurvivorPlayerController* PC = NewObject<AContrarySurvivorPlayerController>(GetTransientPackage()))
	{
		TestFalse(TEXT("У сундука есть своя подпись действия"),
			PC->GetInteractActionText(EInteractKind::Stash).IsEmpty());
		TestNotEqual(TEXT("Подпись сундука отличается от «Обыскать»"),
			PC->GetInteractActionText(EInteractKind::Stash).ToString(),
			PC->GetInteractActionText(EInteractKind::Corpse).ToString());
	}
	else
	{
		AddError(TEXT("Не заспавнен контроллер игрока"));
	}

	StashTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 2. Сохранение и «Продолжить»; сохранение без сундука; «Новая игра».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStashSaveLoadTest,
	"ContrarySurvivor.Stash.SaveLoadOldSaveAndNewGame", StashTestFlags)

bool FStashSaveLoadTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryStashQA");
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0); // хвост прошлого прогона
	}

	const FName Type9 = ContraryAmmo::DefaultType();
	const FName Type12 = ContraryAmmo::Shotgun12gType();
	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	// ⛔ Игрок появляется ВДАЛИ от костра и только потом получает свой слот (иначе
	// автосохранение костра при появлении ушло бы в боевой слот).
	const FVector FarLoc(4000.0f, 0.0f, 100.0f);
	const FVector NearCampfireLoc = CampfireLoc + FVector(100.0f, 0.0f, 0.0f);

	auto PrepareCampfire = [&](UWorld* World)
	{
		if (ACampfire* Campfire = StashTestWorld::Spawn<ACampfire>(World, CampfireLoc))
		{
			if (FFloatProperty* CooldownProp = FindFProperty<FFloatProperty>(ACampfire::StaticClass(), TEXT("AutoSaveCooldown")))
			{
				CooldownProp->SetPropertyValue_InContainer(Campfire, 0.0f);
			}
		}
	};

	// --- Сессия 1: кладём в сундук патроны двух видов и пистолет, сохраняемся ---
	{
		UWorld* World = StashTestWorld::Create();
		if (!World)
		{
			AddError(TEXT("Не создан мир сессии 1"));
			return false;
		}
		PrepareCampfire(World);
		ASaveTestPlayerCharacter* P1 = StashTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		AStashChest* Chest = StashTestWorld::Spawn<AStashChest>(World, FVector(6000.0f, 0.0f, 0.0f));
		UCorpseLootComponent* Stash = Chest ? Chest->GetStashContainer() : nullptr;
		if (!P1 || !Stash)
		{
			AddError(TEXT("Не заспавнены игрок или сундук сессии 1"));
			StashTestWorld::Destroy(World);
			return false;
		}
		P1->UseTestSaveSlot(Slot);
		P1->AddAmmoToInventory(25, Type9);
		P1->AddAmmoToInventory(9, Type12);
		P1->AddAmmoToInventory(5, Type9); // это останется в сумке

		UCorpseLootWidget* Window = StashTestWorld::OpenWindow(World, Stash, P1);
		APistol* Pistol = StashTestWorld::Spawn<APistol>(World, FarLoc);
		if (!Window || !Pistol)
		{
			AddError(TEXT("Не созданы окно сундука или пистолет"));
			StashTestWorld::Destroy(World);
			return false;
		}
		Pistol->DrainReserveAmmo();
		Pistol->SetActorHiddenInGame(true);
		Pistol->SetActorEnableCollision(false);
		P1->GetInventory()->AddItem(Pistol);

		AAmmoItem* Shells = StashTestWorld::FindAmmoPack(P1, Type12);
		TestTrue(TEXT("Дробь ушла в сундук"), Shells && Window->TakeItemFromPlayer(Shells));
		TestTrue(TEXT("Пистолет ушёл в сундук"), Window->TakeItemFromPlayer(Pistol));
		TestEqual(TEXT("В сумке осталось 30 пистолетных патронов"), P1->GetReserveAmmoInInventory(Type9), 30);

		P1->SetActorLocation(NearCampfireLoc, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Сохранение сессии 1 прошло"), P1->SaveGame());

		if (const UContrarySaveGame* Disk = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
		{
			TestEqual(TEXT("В сохранении две записи сундука"), Disk->StashEntries.Num(), 2);
		}
		else
		{
			AddError(TEXT("Слот сессии 1 не читается"));
		}
		StashTestWorld::Destroy(World);
	}

	// --- Сессия 2 (новый запуск, новый мир): «Продолжить» возвращает содержимое сундука ---
	{
		UWorld* World = StashTestWorld::Create();
		if (!World)
		{
			AddError(TEXT("Не создан мир сессии 2"));
			return false;
		}
		PrepareCampfire(World);
		ASaveTestPlayerCharacter* P2 = StashTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		AStashChest* Chest = StashTestWorld::Spawn<AStashChest>(World, FVector(6000.0f, 0.0f, 0.0f));
		if (!P2 || !Chest)
		{
			AddError(TEXT("Не заспавнены игрок или сундук сессии 2"));
			StashTestWorld::Destroy(World);
			return false;
		}
		P2->UseTestSaveSlot(Slot);
		TestTrue(TEXT("«Продолжить» сессии 2 отработал"), P2->LoadGameForContinue());

		UCorpseLootComponent* Stash = Chest->GetStashContainer();
		TestEqual(TEXT("Дробь вернулась в сундук"), StashTestWorld::CountAmmoIn(Stash, Type12), 9);
		TestEqual(TEXT("Пистолетных патронов в сундуке нет"), StashTestWorld::CountAmmoIn(Stash, Type9), 0);
		const ARangedWeapon* StashedGun = nullptr;
		for (const AMasterInventoryItem* Item : Stash ? Stash->GetLootItems() : TArray<AMasterInventoryItem*>())
		{
			if (const ARangedWeapon* Gun = Cast<ARangedWeapon>(Item))
			{
				StashedGun = Gun;
			}
		}
		TestNotNull(TEXT("Пистолет вернулся в сундук"), StashedGun);
		if (StashedGun)
		{
			TestTrue(TEXT("В сундуке именно пистолет"), StashedGun->IsA(APistol::StaticClass()));
			TestEqual(TEXT("Ствол из сундука не приносит бесплатных патронов"), StashedGun->GetCurrentAmmoReserve(), 0);
		}
		TestEqual(TEXT("Патроны сумки на месте и не задвоились"), P2->GetReserveAmmoInInventory(Type9), 30);
		TestEqual(TEXT("Дроби в сумке нет — она в сундуке"), P2->GetReserveAmmoInInventory(Type12), 0);

		// «Новая игра» очищает сундук.
		P2->ResetToNewGame();
		TestFalse(TEXT("После новой игры сундук пуст"), Stash && Stash->HasLoot());
		StashTestWorld::Destroy(World);
	}

	// --- Сессия 3: сохранение, записанное до сундука (без его записей), даёт пустой сундук ---
	{
		UContrarySaveGame* OldSave = Cast<UContrarySaveGame>(
			UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
		if (OldSave)
		{
			OldSave->bHasData = true;
			OldSave->PlayerLocation = NearCampfireLoc;
			FSavedInventoryEntry AmmoEntry;
			AmmoEntry.ClassPath = AAmmoItem::StaticClass()->GetPathName();
			AmmoEntry.ItemName = GetDefault<AAmmoItem>()->ItemName;
			AmmoEntry.StackCount = 17;
			OldSave->InventoryEntries.Add(AmmoEntry);
			TestTrue(TEXT("Старое сохранение записано"), UGameplayStatics::SaveGameToSlot(OldSave, Slot, 0));
		}

		UWorld* World = StashTestWorld::Create();
		if (!World)
		{
			AddError(TEXT("Не создан мир сессии 3"));
			return false;
		}
		PrepareCampfire(World);
		ASaveTestPlayerCharacter* P3 = StashTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		AStashChest* Chest = StashTestWorld::Spawn<AStashChest>(World, FVector(6000.0f, 0.0f, 0.0f));
		if (!P3 || !Chest)
		{
			AddError(TEXT("Не заспавнены игрок или сундук сессии 3"));
			StashTestWorld::Destroy(World);
			return false;
		}
		P3->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Старое сохранение загрузилось"), P3->LoadGameForContinue());
		TestEqual(TEXT("Рюкзак старого сохранения на месте"), P3->GetReserveAmmoInInventory(Type9), 17);
		UCorpseLootComponent* Stash = Chest->GetStashContainer();
		TestNotNull(TEXT("Сундук открывается и со старым сохранением"), Stash);
		TestFalse(TEXT("Со старым сохранением сундук пуст"), Stash && Stash->HasLoot());
		StashTestWorld::Destroy(World);
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
