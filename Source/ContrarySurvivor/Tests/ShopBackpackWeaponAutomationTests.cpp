// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны 08-08 (STALKER-поток покупки оружия). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.ShopPauseBones; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE (только логика персонажа, Slate не нужен):
//   - ТЗ Рината 08-08 задача 2: купленный у торговца огнестрел попадает В РЮКЗАК и НЕ
//     занимает слот оружия сам (автоэкип из покупки убран);
//   - тап по плитке огнестрела в инвентаре (его модель — APlayerCharacter::TryAdoptRangedWeapon,
//     вызывается из UInventoryScreenWidget::HandleTileUse по категории Weapon) переносит
//     ствол в пустой слот оружия и убирает его из рюкзака;
//   - второй купленный ствол тоже остаётся в рюкзаке (слот занят — не отбираем).
// Сам клик по плитке (Slate) headless не воспроизводим: проверяем категорию предмета
// (условие ветки HandleTileUse) и вызываем ту же модель, что дёргает обработчик.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "APistol.h"
#include "AMasterInventoryItem.h" // EItemCategory
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags ShopPauseBonesTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace ShopBackpackWeaponTestWorld
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

	// Позиция каталога «пистолет за 150» — как в прайс-листе торговца (Kind=Item, ItemClass=APistol).
	static FShopEntry PistolEntry()
	{
		FShopEntry E;
		E.DisplayName = TEXT("Pistol");
		E.Price = 150.0f;
		E.Kind = EShopEntryKind::Item;
		E.ItemClass = APistol::StaticClass();
		return E;
	}

	// Найти в рюкзаке первый огнестрел (не экипированный).
	static APistol* FindPistolInBackpack(APlayerCharacter* Player)
	{
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (!Inv)
		{
			return nullptr;
		}
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (IsValid(Item) && !Inv->IsItemEquipped(Item))
			{
				if (APistol* Pistol = Cast<APistol>(Item))
				{
					return Pistol;
				}
			}
		}
		return nullptr;
	}
}

// ===========================================================================
// Задача 2 (ТЗ Рината 08-08): покупка кладёт огнестрел в рюкзак, а не в слот;
// «тап» по нему в инвентаре переносит его в слот вручную.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopBackpackWeaponTest,
	"ContrarySurvivor.ShopPauseBones.PurchasedFirearmGoesToBackpackThenTapEquips",
	ShopPauseBonesTestFlags)

bool FShopBackpackWeaponTest::RunTest(const FString& Parameters)
{
	UWorld* World = ShopBackpackWeaponTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		APlayerCharacter* Player = ShopBackpackWeaponTestWorld::SpawnPlayer(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
		if (Player && Inv && Stats)
		{
			// На старте огнестрела нет вовсе (решение Рината 05-08).
			TestNull(TEXT("Слот огнестрела на старте пуст"), Player->GetRangedWeaponInstance());

			// 1) Покупка пистолета: денег хватает, сделка проходит.
			Stats->AddMoney(1000.0f);
			const FShopEntry Entry = ShopBackpackWeaponTestWorld::PistolEntry();
			const bool bBought = Player->Shop_BuyEntryQty(Entry, 1);
			TestTrue(TEXT("Покупка пистолета прошла"), bBought);

			// 2) ГЛАВНОЕ: купленный ствол лёг в РЮКЗАК и слот оружия НЕ занял (автоэкип убран).
			TestNull(TEXT("После покупки слот огнестрела всё ещё пуст (нет автоэкипа)"),
				Player->GetRangedWeaponInstance());
			APistol* Pistol = ShopBackpackWeaponTestWorld::FindPistolInBackpack(Player);
			TestNotNull(TEXT("Купленный пистолет лежит в рюкзаке"), Pistol);

			if (Pistol)
			{
				// Условие ветки UInventoryScreenWidget::HandleTileUse: тап действует по категории.
				TestEqual(TEXT("Категория купленного пистолета — Оружие (тап уйдёт в TryAdopt)"),
					Pistol->GetItemCategory(), EItemCategory::Weapon);

				// 3) «Тап» по плитке огнестрела в инвентаре — та же модель, что дёргает обработчик.
				const bool bAdopted = Player->TryAdoptRangedWeapon(Pistol);
				TestTrue(TEXT("Тап по пистолету занял пустой слот оружия"), bAdopted);
				TestEqual(TEXT("В слоте огнестрела теперь купленный пистолет"),
					Player->GetRangedWeaponInstance(), static_cast<AMasterWeapon*>(Pistol));
				TestFalse(TEXT("Из рюкзака пистолет ушёл в слот"),
					Inv->GetInventoryItems().Contains(Pistol));
			}

			// 4) Второй купленный ствол тоже остаётся в рюкзаке (слот занят — не отбираем).
			const bool bBoughtSecond = Player->Shop_BuyEntryQty(Entry, 1);
			TestTrue(TEXT("Покупка второго пистолета прошла"), bBoughtSecond);
			APistol* Second = ShopBackpackWeaponTestWorld::FindPistolInBackpack(Player);
			TestNotNull(TEXT("Второй пистолет лежит в рюкзаке"), Second);
			if (Second)
			{
				const bool bAdoptSecond = Player->TryAdoptRangedWeapon(Second);
				TestFalse(TEXT("Второй ствол в занятый слот не берётся"), bAdoptSecond);
				TestTrue(TEXT("Второй пистолет остался в рюкзаке"),
					Inv->GetInventoryItems().Contains(Second));
			}
		}
		else
		{
			bOk = false;
		}
	}
	ShopBackpackWeaponTestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
