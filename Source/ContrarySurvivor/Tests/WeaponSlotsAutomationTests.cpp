// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты двух слотов огнестрела и отдельной кнопки ножа (Ринат 10.10.2026:
// «чтобы игрок мог быстро переключаться между пистолетом и двуручным оружием… добавить
// дополнительный слот оружия, а удар ножом сделать отдельной кнопкой»).
//
// Проверяется:
//  1) оба ствола экипируются одновременно, каждый в слот своего вида; кнопка смены оружия
//     переключает короткое и длинное, нож в цикл не входит;
//  2) второй ствол того же вида вытесняет прежний в рюкзак и не трогает соседний слот;
//  3) один ствол — кнопка переключает его и нож; ни одного — в руках нож; снятие ствола в рюкзак;
//  4) удар ножом с отдельной кнопки при огнестреле в руках: урон прошёл, оружие в руках то же,
//     стрельба на время замаха заперта;
//  5) сохранение двух слотов и оружия в руках, запас патронов не дарится заново;
//  6) сохранение старого вида (один слот, без признака «что в руках») читается без потерь;
//  7) меню отладки кладёт каждый ствол в свой слот, не вытесняя второй.
//
// Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.WeaponSlots; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// НЕ покрывается headless: вид ножа в ладони на время замаха и экранная кнопка ножа (Slate и
// анимации требуют живой игры) — это смотрит Ринат.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Debug/ContraryDebugMenu.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "UInventoryComponent.h"
#include "AAmmoItem.h"
#include "AMeleeWeapon.h"
#include "APistol.h"
#include "AShotgun.h"
#include "ARangedWeapon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags WeaponSlotsTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (своя обвязка — паттерн проекта: свои тесты всегда в НОВОМ файле).
namespace WeaponSlotsTestWorld
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

	// Ствол в рюкзак игрока — так же, как его кладут покупка и добыча (скрыт, без коллизии).
	template <typename T>
	static T* GiveToBackpack(UWorld* World, APlayerCharacter* Player)
	{
		T* Weapon = Spawn<T>(World, Player ? Player->GetActorLocation() : FVector::ZeroVector);
		if (Weapon && Player && Player->GetInventory())
		{
			Weapon->SetActorHiddenInGame(true);
			Weapon->SetActorEnableCollision(false);
			Player->GetInventory()->AddItem(Weapon);
		}
		return Weapon;
	}

	static bool InBackpack(const APlayerCharacter* Player, const AMasterInventoryItem* Item)
	{
		return Player && Player->GetInventory() && Player->GetInventory()->GetInventoryItems().Contains(Item);
	}

	// Костёр с нулевым антиспамом автосохранения: «сессии» теста идут в одном времени мира.
	static void SpawnCampfire(UWorld* World, const FVector& Loc)
	{
		if (ACampfire* Campfire = Spawn<ACampfire>(World, Loc))
		{
			if (FFloatProperty* CooldownProp = FindFProperty<FFloatProperty>(ACampfire::StaticClass(), TEXT("AutoSaveCooldown")))
			{
				CooldownProp->SetPropertyValue_InContainer(Campfire, 0.0f);
			}
		}
	}
}

// ===========================================================================
// 1. Оба ствола экипированы одновременно; кнопка смены оружия ходит между ними без ножа.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotsBothEquippedAndSwitchTest,
	"ContrarySurvivor.WeaponSlots.BothFirearmsEquippedAndSwitchSkipsKnife", WeaponSlotsTestFlags)

bool FWeaponSlotsBothEquippedAndSwitchTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponSlotsTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	APlayerCharacter* Player = WeaponSlotsTestWorld::Spawn<APlayerCharacter>(World);
	APistol* Pistol = WeaponSlotsTestWorld::GiveToBackpack<APistol>(World, Player);
	AShotgun* Shotgun = WeaponSlotsTestWorld::GiveToBackpack<AShotgun>(World, Player);
	if (Player && Pistol && Shotgun)
	{
		AMasterWeapon* Knife = Player->GetMeleeWeaponInstance();
		TestNotNull(TEXT("Нож у игрока есть"), Knife);
		TestTrue(TEXT("Пистолет — короткое оружие"), Pistol->GetFirearmSlot() == EFirearmSlot::Sidearm);
		TestTrue(TEXT("Двустволка — длинное оружие"), Shotgun->GetFirearmSlot() == EFirearmSlot::Long);
		TestEqual(TEXT("На старте стволов в слотах нет"), Player->GetEquippedFirearmCount(), 0);

		TestTrue(TEXT("Пистолет встал в свой слот"), Player->TryAdoptRangedWeapon(Pistol));
		TestTrue(TEXT("Двустволка встала в свой слот, пистолет ей не мешает"), Player->TryAdoptRangedWeapon(Shotgun));
		TestTrue(TEXT("В слоте короткого — пистолет"), Player->GetFirearmInSlot(EFirearmSlot::Sidearm) == Pistol);
		TestTrue(TEXT("В слоте длинного — двустволка"), Player->GetFirearmInSlot(EFirearmSlot::Long) == Shotgun);
		TestEqual(TEXT("Заняты оба слота"), Player->GetEquippedFirearmCount(), 2);
		TestFalse(TEXT("Пистолет ушёл из рюкзака в слот"), WeaponSlotsTestWorld::InBackpack(Player, Pistol));
		TestFalse(TEXT("Двустволка ушла из рюкзака в слот"), WeaponSlotsTestWorld::InBackpack(Player, Shotgun));
		TestTrue(TEXT("Экипировка в слот оружие в руки не берёт: в руках нож"), Player->GetCurrentWeapon() == Knife);

		// Кнопка смены оружия: с ножа — к стволу, дальше только между стволами.
		Player->SwitchWeapon();
		TestTrue(TEXT("Первое нажатие: в руках пистолет"), Player->GetCurrentWeapon() == Pistol);
		Player->SwitchWeapon();
		TestTrue(TEXT("Второе нажатие: в руках двустволка"), Player->GetCurrentWeapon() == Shotgun);
		Player->SwitchWeapon();
		TestTrue(TEXT("Третье нажатие: снова пистолет, нож в цикл не входит"), Player->GetCurrentWeapon() == Pistol);
		TestTrue(TEXT("Убранная двустволка спрятана"), Shotgun->IsHidden());
		TestFalse(TEXT("Пистолет в руках виден"), Pistol->IsHidden());

		// Прямой выбор слота.
		TestTrue(TEXT("Прямой выбор длинного оружия"), Player->SelectFirearmSlot(EFirearmSlot::Long));
		TestTrue(TEXT("После прямого выбора в руках двустволка"), Player->GetCurrentWeapon() == Shotgun);
		TestTrue(TEXT("«Огнестрел игрока» — ствол в руках"), Player->GetRangedWeaponInstance() == Shotgun);

		// С ножа кнопка возвращает к последнему стволу.
		TestTrue(TEXT("Нож взят в руки"), Player->TakeKnifeInHands());
		Player->SwitchWeapon();
		TestTrue(TEXT("С ножа кнопка возвращает к последнему стволу"), Player->GetCurrentWeapon() == Shotgun);
	}
	else
	{
		AddError(TEXT("Не заспавнены игрок или стволы"));
	}

	WeaponSlotsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 2. Второй ствол того же вида вытесняет прежний в рюкзак; соседний слот не трогается.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotsSameKindDisplacesTest,
	"ContrarySurvivor.WeaponSlots.SameKindDisplacesToBackpack", WeaponSlotsTestFlags)

bool FWeaponSlotsSameKindDisplacesTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponSlotsTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	APlayerCharacter* Player = WeaponSlotsTestWorld::Spawn<APlayerCharacter>(World);
	APistol* PistolA = WeaponSlotsTestWorld::GiveToBackpack<APistol>(World, Player);
	APistol* PistolB = WeaponSlotsTestWorld::GiveToBackpack<APistol>(World, Player);
	AShotgun* Shotgun = WeaponSlotsTestWorld::GiveToBackpack<AShotgun>(World, Player);
	if (Player && PistolA && PistolB && Shotgun)
	{
		TestTrue(TEXT("Первый пистолет в слоте"), Player->TryAdoptRangedWeapon(PistolA));
		TestTrue(TEXT("Двустволка в слоте"), Player->TryAdoptRangedWeapon(Shotgun));
		TestTrue(TEXT("Пистолет взят в руки"), Player->SelectFirearmSlot(EFirearmSlot::Sidearm));

		TestFalse(TEXT("Занятый слот короткого второй пистолет сам не занимает"), Player->TryAdoptRangedWeapon(PistolB));
		TestTrue(TEXT("Второй пистолет остался в рюкзаке"), WeaponSlotsTestWorld::InBackpack(Player, PistolB));

		TestTrue(TEXT("Замена пистолета пистолетом прошла"), Player->SwapRangedWeaponFromBackpack(PistolB));
		TestTrue(TEXT("В слоте короткого — второй пистолет"), Player->GetFirearmInSlot(EFirearmSlot::Sidearm) == PistolB);
		TestTrue(TEXT("Прежний пистолет ушёл в рюкзак"), WeaponSlotsTestWorld::InBackpack(Player, PistolA));
		TestTrue(TEXT("Прежний был в руках — в руках новый"), Player->GetCurrentWeapon() == PistolB);
		TestTrue(TEXT("Слот длинного не тронут"), Player->GetFirearmInSlot(EFirearmSlot::Long) == Shotgun);
		TestFalse(TEXT("Двустволка в рюкзак не попала"), WeaponSlotsTestWorld::InBackpack(Player, Shotgun));
	}
	else
	{
		AddError(TEXT("Не заспавнены игрок или стволы"));
	}

	WeaponSlotsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Ни одного ствола, один ствол, снятие ствола в рюкзак — игрок не в тупике.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotsOneOrNoneTest,
	"ContrarySurvivor.WeaponSlots.OneFirearmTogglesWithKnifeAndUnequipWorks", WeaponSlotsTestFlags)

bool FWeaponSlotsOneOrNoneTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponSlotsTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	APlayerCharacter* Player = WeaponSlotsTestWorld::Spawn<APlayerCharacter>(World);
	AShotgun* Shotgun = WeaponSlotsTestWorld::GiveToBackpack<AShotgun>(World, Player);
	if (Player && Shotgun)
	{
		AMasterWeapon* Knife = Player->GetMeleeWeaponInstance();

		// Ни одного ствола: кнопка ничего не ломает, в руках нож.
		Player->SwitchWeapon();
		TestTrue(TEXT("Без стволов в руках остаётся нож"), Player->GetCurrentWeapon() == Knife && Knife != nullptr);
		TestNull(TEXT("Без стволов «огнестрела игрока» нет"), Player->GetRangedWeaponInstance());
		TestFalse(TEXT("Прямой выбор пустого слота ничего не берёт"), Player->SelectFirearmSlot(EFirearmSlot::Sidearm));
		TestFalse(TEXT("Снятие из пустого слота — отказ"), Player->UnequipFirearmToBackpack(EFirearmSlot::Long));

		// Один ствол (только длинное): кнопка переключает его и нож.
		TestTrue(TEXT("Двустволка в слоте"), Player->TryAdoptRangedWeapon(Shotgun));
		Player->SwitchWeapon();
		TestTrue(TEXT("Один ствол: нажатие берёт его в руки"), Player->GetCurrentWeapon() == Shotgun);
		Player->SwitchWeapon();
		TestTrue(TEXT("Один ствол: следующее нажатие возвращает нож"), Player->GetCurrentWeapon() == Knife);
		Player->SwitchWeapon();
		TestTrue(TEXT("Один ствол: и снова ствол"), Player->GetCurrentWeapon() == Shotgun);

		// Снятие ствола из рук в рюкзак: в руках нож, вещь не потеряна.
		const int32 ClipBefore = Shotgun->GetCurrentAmmoInClip();
		TestTrue(TEXT("Двустволка снята в рюкзак"), Player->UnequipFirearmToBackpack(EFirearmSlot::Long));
		TestNull(TEXT("Слот длинного пуст"), Player->GetFirearmInSlot(EFirearmSlot::Long));
		TestTrue(TEXT("Двустволка лежит в рюкзаке"), WeaponSlotsTestWorld::InBackpack(Player, Shotgun));
		TestEqual(TEXT("Патроны в стволах остались при ружье"), Shotgun->GetCurrentAmmoInClip(), ClipBefore);
		TestTrue(TEXT("После снятия в руках нож"), Player->GetCurrentWeapon() == Knife);
		TestTrue(TEXT("Снятую двустволку можно экипировать снова"), Player->TryAdoptRangedWeapon(Shotgun));
	}
	else
	{
		AddError(TEXT("Не заспавнены игрок или двустволка"));
	}

	WeaponSlotsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 4. Удар ножом с отдельной кнопки при огнестреле в руках.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotsQuickMeleeTest,
	"ContrarySurvivor.WeaponSlots.KnifeButtonHitsAndKeepsFirearmInHands", WeaponSlotsTestFlags)

bool FWeaponSlotsQuickMeleeTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponSlotsTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Герой смотрит вдоль +X, враг в 120 см перед ним (зазор между капсулами меньше дальности удара).
	APlayerCharacter* Player = WeaponSlotsTestWorld::Spawn<APlayerCharacter>(World, FVector(0.f, 0.f, 100.f));
	AEnemyCharacter* Enemy = WeaponSlotsTestWorld::Spawn<AEnemyCharacter>(World, FVector(120.f, 0.f, 100.f));
	APistol* Pistol = WeaponSlotsTestWorld::GiveToBackpack<APistol>(World, Player);
	UStatsComponent* EnemyStats = Enemy ? Enemy->FindComponentByClass<UStatsComponent>() : nullptr;
	AMeleeWeapon* Knife = Player ? Cast<AMeleeWeapon>(Player->GetMeleeWeaponInstance()) : nullptr;
	if (Player && Enemy && Pistol && EnemyStats && Knife)
	{
		TestTrue(TEXT("Пистолет в слоте"), Player->TryAdoptRangedWeapon(Pistol));
		TestTrue(TEXT("Пистолет в руках"), Player->SelectFirearmSlot(EFirearmSlot::Sidearm));
		TestFalse(TEXT("До удара замаха нет"), Player->IsQuickMeleeInProgress());

		const float HP0 = EnemyStats->GetHealth();
		const int32 Clip0 = Pistol->GetCurrentAmmoInClip();

		TestTrue(TEXT("Удар ножом с кнопки начался"), Player->QuickMeleeAttack());
		// В тесте кадров анимации нет: если замах ждёт метку, отыгрываем её тем же путём,
		// что UAnimNotify_MeleeHit (нож спрашивается у носителя, а не берётся из рук).
		if (AMeleeWeapon* Swinging = Player->GetSwingMeleeWeapon())
		{
			if (Swinging->ConsumePendingSwing())
			{
				Swinging->ApplyMeleeDamage();
			}
		}
		TestTrue(TEXT("Метке удара отдаётся нож игрока, хотя в руках пистолет"), Player->GetSwingMeleeWeapon() == Knife);
		TestEqual(TEXT("Враг получил урон ножа"), EnemyStats->GetHealth(), FMath::Max(0.0f, HP0 - Knife->GetDamage()));
		TestTrue(TEXT("После удара в руках тот же пистолет"), Player->GetCurrentWeapon() == Pistol);
		TestTrue(TEXT("Пистолет остался в своём слоте"), Player->GetFirearmInSlot(EFirearmSlot::Sidearm) == Pistol);

		// На время замаха нож показан в ладони вместо ствола, стрельба заперта.
		TestTrue(TEXT("Идёт замах"), Player->IsQuickMeleeInProgress());
		TestTrue(TEXT("Ствол на время замаха спрятан"), Pistol->IsHidden());
		TestFalse(TEXT("Нож на время замаха виден"), Knife->IsHidden());
		Pistol->SetTarget(Enemy);
		Player->FireCurrentWeapon(Enemy);
		TestEqual(TEXT("Во время замаха выстрела нет: патрон не потрачен"), Pistol->GetCurrentAmmoInClip(), Clip0);
		TestFalse(TEXT("Повторный удар до конца кулдауна ножа не начинается"), Player->QuickMeleeAttack());

		// Смена оружия завершает показ ножа; в руках снова обычный вид.
		Player->TakeKnifeInHands();
		Player->SelectFirearmSlot(EFirearmSlot::Sidearm);
		TestFalse(TEXT("После смены оружия замаха нет"), Player->IsQuickMeleeInProgress());
		TestFalse(TEXT("Пистолет в руках снова виден"), Pistol->IsHidden());
		TestTrue(TEXT("Нож убран"), Knife->IsHidden());
	}
	else
	{
		AddError(TEXT("Не заспавнены игрок, враг, пистолет или нож"));
	}

	WeaponSlotsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 5. Сохранение двух слотов и оружия в руках; патроны при загрузке не дарятся.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotsSaveLoadTest,
	"ContrarySurvivor.WeaponSlots.SaveLoadKeepsBothSlotsAndWeaponInHands", WeaponSlotsTestFlags)

bool FWeaponSlotsSaveLoadTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryWeaponSlotsQA");
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0); // хвост прошлого прогона
	}

	UWorld* World = WeaponSlotsTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FName Type9 = ContraryAmmo::DefaultType();
	const FName Type12 = ContraryAmmo::Shotgun12gType();
	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	// ⛔ Игрок появляется ВДАЛИ от костра: рядом с костром автосохранение сработало бы раньше,
	// чем тест переключит игрока на свой слот, то есть в боевой слот.
	const FVector PlayerLoc(4000.0f, 0.0f, 100.0f);
	const FVector NearCampfireLoc = CampfireLoc + FVector(100.0f, 0.0f, 0.0f);
	WeaponSlotsTestWorld::SpawnCampfire(World, CampfireLoc);

	int32 Ammo9Saved = 0;
	int32 Ammo12Saved = 0;
	{
		ASaveTestPlayerCharacter* P1 = WeaponSlotsTestWorld::Spawn<ASaveTestPlayerCharacter>(World, PlayerLoc);
		APistol* Pistol = WeaponSlotsTestWorld::GiveToBackpack<APistol>(World, P1);
		AShotgun* Shotgun = WeaponSlotsTestWorld::GiveToBackpack<AShotgun>(World, P1);
		if (!P1 || !Pistol || !Shotgun)
		{
			AddError(TEXT("Не заспавнены игрок или стволы сессии 1"));
			WeaponSlotsTestWorld::Destroy(World);
			return false;
		}
		P1->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Пистолет в слоте"), P1->TryAdoptRangedWeapon(Pistol));
		TestTrue(TEXT("Двустволка в слоте"), P1->TryAdoptRangedWeapon(Shotgun));
		TestTrue(TEXT("В руках двустволка"), P1->SelectFirearmSlot(EFirearmSlot::Long));
		Ammo9Saved = P1->GetReserveAmmoInInventory(Type9);
		Ammo12Saved = P1->GetReserveAmmoInInventory(Type12);
		TestTrue(TEXT("Запас пистолета перелит пачкой в рюкзак"), Ammo9Saved > 0);
		TestTrue(TEXT("Запас двустволки перелит пачкой в рюкзак"), Ammo12Saved > 0);
		P1->SetActorLocation(NearCampfireLoc, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Сохранение прошло"), P1->SaveGame());
		P1->Destroy();
	}

	// В записи — обе записи стволов слотов и признак «в руках длинное».
	if (const UContrarySaveGame* Saved = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		int32 EquippedFirearms = 0;
		for (const FSavedInventoryEntry& Entry : Saved->InventoryEntries)
		{
			if (Entry.bEquipped && (Entry.ClassPath.Contains(TEXT("Pistol")) || Entry.ClassPath.Contains(TEXT("Shotgun"))))
			{
				++EquippedFirearms;
				TestEqual(TEXT("Запас при стволе записан числом (ноль: он уже в рюкзаке)"), Entry.WeaponReserveAmmo, 0);
			}
		}
		TestEqual(TEXT("В записи два ствола слотов"), EquippedFirearms, 2);
		TestTrue(TEXT("В записи отмечено: в руках длинное оружие"), Saved->WeaponInHands == ESavedWeaponInHands::Long);
	}
	else
	{
		AddError(TEXT("Запись не прочиталась"));
	}

	{
		ASaveTestPlayerCharacter* P2 = WeaponSlotsTestWorld::Spawn<ASaveTestPlayerCharacter>(World, PlayerLoc);
		if (!P2)
		{
			AddError(TEXT("Не заспавнен игрок сессии 2"));
			WeaponSlotsTestWorld::Destroy(World);
			return false;
		}
		P2->UseTestSaveSlot(Slot);
		TestTrue(TEXT("«Продолжить» отработал"), P2->LoadGameForContinue());

		const ARangedWeapon* Sidearm = P2->GetFirearmInSlot(EFirearmSlot::Sidearm);
		const ARangedWeapon* Long = P2->GetFirearmInSlot(EFirearmSlot::Long);
		TestTrue(TEXT("Пистолет вернулся в слот короткого"), Sidearm && Sidearm->IsA(APistol::StaticClass()));
		TestTrue(TEXT("Двустволка вернулась в слот длинного"), Long && Long->IsA(AShotgun::StaticClass()));
		TestTrue(TEXT("В руках снова двустволка"), Long && P2->GetCurrentWeapon() == Long);
		TestEqual(TEXT("Пистолетных патронов ровно столько же (не подарены заново)"), P2->GetReserveAmmoInInventory(Type9), Ammo9Saved);
		TestEqual(TEXT("Дробовых патронов ровно столько же (не подарены заново)"), P2->GetReserveAmmoInInventory(Type12), Ammo12Saved);
		if (Sidearm && Long)
		{
			TestEqual(TEXT("Запас при пистолете пуст"), Sidearm->GetCurrentAmmoReserve(), 0);
			TestEqual(TEXT("Запас при двустволке пуст"), Long->GetCurrentAmmoReserve(), 0);
		}
		int32 FirearmsInBackpack = 0;
		if (P2->GetInventory())
		{
			for (const AMasterInventoryItem* Item : P2->GetInventory()->GetInventoryItems())
			{
				FirearmsInBackpack += (Cast<ARangedWeapon>(Item) != nullptr) ? 1 : 0;
			}
		}
		TestEqual(TEXT("Стволы слотов в рюкзаке не задвоились"), FirearmsInBackpack, 0);
		P2->Destroy();
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	WeaponSlotsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 6. Сохранение старого вида: один ствол слота, запас не записан, «что в руках» не записано.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotsOldSaveTest,
	"ContrarySurvivor.WeaponSlots.OldSingleSlotSaveLoadsWithoutLoss", WeaponSlotsTestFlags)

bool FWeaponSlotsOldSaveTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryWeaponSlotsOldQA");
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}

	UWorld* World = WeaponSlotsTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	const FVector PlayerLoc(4000.0f, 0.0f, 100.0f);
	WeaponSlotsTestWorld::SpawnCampfire(World, CampfireLoc);

	// Запись «как раньше»: двустволка в единственном слоте (признак bEquipped), запас при
	// стволе неизвестен (меньше нуля), поля «что в руках» нет (значение по умолчанию), второй
	// ствол — пистолет — лежит в рюкзаке.
	UContrarySaveGame* Old = Cast<UContrarySaveGame>(
		UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
	if (!Old)
	{
		AddError(TEXT("Не создан объект сохранения"));
		WeaponSlotsTestWorld::Destroy(World);
		return false;
	}
	Old->bHasData = true;
	Old->PlayerLocation = CampfireLoc + FVector(100.0f, 0.0f, 0.0f);
	{
		FSavedInventoryEntry ShotgunEntry;
		ShotgunEntry.ClassPath = AShotgun::StaticClass()->GetPathName();
		ShotgunEntry.ItemName = GetDefault<AShotgun>()->ItemName;
		ShotgunEntry.bEquipped = true;
		Old->InventoryEntries.Add(ShotgunEntry);

		FSavedInventoryEntry PistolEntry;
		PistolEntry.ClassPath = APistol::StaticClass()->GetPathName();
		PistolEntry.ItemName = GetDefault<APistol>()->ItemName;
		PistolEntry.bEquipped = false;
		Old->InventoryEntries.Add(PistolEntry);
	}
	TestTrue(TEXT("У старой записи признак «что в руках» неизвестен"), Old->WeaponInHands == ESavedWeaponInHands::Unknown);
	TestTrue(TEXT("Старая запись легла в слот"), UGameplayStatics::SaveGameToSlot(Old, Slot, 0));

	{
		ASaveTestPlayerCharacter* Player = WeaponSlotsTestWorld::Spawn<ASaveTestPlayerCharacter>(World, PlayerLoc);
		if (!Player)
		{
			AddError(TEXT("Не заспавнен игрок"));
			WeaponSlotsTestWorld::Destroy(World);
			return false;
		}
		Player->UseTestSaveSlot(Slot);
		TestTrue(TEXT("«Продолжить» по старой записи отработал"), Player->LoadGameForContinue());

		const ARangedWeapon* Long = Player->GetFirearmInSlot(EFirearmSlot::Long);
		TestTrue(TEXT("Двустволка из единственного слота встала в слот длинного"), Long && Long->IsA(AShotgun::StaticClass()));
		TestNull(TEXT("Слот короткого пуст: пистолет лежал в рюкзаке"), Player->GetFirearmInSlot(EFirearmSlot::Sidearm));

		int32 PistolsInBackpack = 0;
		if (Player->GetInventory())
		{
			for (const AMasterInventoryItem* Item : Player->GetInventory()->GetInventoryItems())
			{
				PistolsInBackpack += Item && Item->IsA(APistol::StaticClass()) ? 1 : 0;
			}
		}
		TestEqual(TEXT("Пистолет из рюкзака не потерян"), PistolsInBackpack, 1);
		TestTrue(TEXT("В руках нож, как было до двух слотов"),
			Player->GetCurrentWeapon() == Player->GetMeleeWeaponInstance() && Player->GetCurrentWeapon() != nullptr);

		// Запас старой записи переносится в рюкзак один раз (прежнее правило) — патроны не пропали.
		TestEqual(TEXT("Дробь старой записи — запас класса, пачкой в рюкзаке"),
			Player->GetReserveAmmoInInventory(ContraryAmmo::Shotgun12gType()), GetDefault<AShotgun>()->GetCurrentAmmoReserve());
		TestEqual(TEXT("Пистолетные патроны старой записи — запас класса, пачкой в рюкзаке"),
			Player->GetReserveAmmoInInventory(ContraryAmmo::DefaultType()), GetDefault<APistol>()->GetCurrentAmmoReserve());

		// Пистолет из рюкзака экипируется рядом с двустволкой.
		APistol* BackpackPistol = nullptr;
		if (Player->GetInventory())
		{
			for (AMasterInventoryItem* Item : Player->GetInventory()->GetInventoryItems())
			{
				if (APistol* AsPistol = Cast<APistol>(Item))
				{
					BackpackPistol = AsPistol;
					break;
				}
			}
		}
		TestTrue(TEXT("Пистолет из рюкзака встаёт во второй слот"), BackpackPistol && Player->TryAdoptRangedWeapon(BackpackPistol));
		TestEqual(TEXT("Заняты оба слота"), Player->GetEquippedFirearmCount(), 2);
		Player->Destroy();
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	WeaponSlotsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 7. Меню отладки: каждый ствол в свой слот, второй не вытесняется.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotsDebugMenuGiveTest,
	"ContrarySurvivor.WeaponSlots.DebugMenuGivesEachFirearmToItsOwnSlot", WeaponSlotsTestFlags)

bool FWeaponSlotsDebugMenuGiveTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponSlotsTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	APlayerCharacter* Player = WeaponSlotsTestWorld::Spawn<APlayerCharacter>(World);
	if (Player)
	{
		ARangedWeapon* Shotgun = ContraryDebugTools::GiveFirearm(
			Player, FName(TEXT("shotgun_toz34")), AShotgun::StaticClass(), 20);
		ARangedWeapon* Pistol = ContraryDebugTools::GiveFirearm(
			Player, FName(TEXT("pistol")), APistol::StaticClass(), 24);
		TestNotNull(TEXT("Двустволка выдана"), Shotgun);
		TestNotNull(TEXT("Пистолет выдан"), Pistol);
		if (Shotgun && Pistol)
		{
			TestTrue(TEXT("Двустволка осталась в слоте длинного"), Player->GetFirearmInSlot(EFirearmSlot::Long) == Shotgun);
			TestTrue(TEXT("Пистолет встал в слот короткого"), Player->GetFirearmInSlot(EFirearmSlot::Sidearm) == Pistol);
			TestFalse(TEXT("Двустволка в рюкзак не вытеснена"), WeaponSlotsTestWorld::InBackpack(Player, Shotgun));
			TestTrue(TEXT("В руках последний выданный ствол"), Player->GetCurrentWeapon() == Pistol);
			TestTrue(TEXT("Патроны двустволки выданы"), Player->GetReserveAmmoInInventory(Shotgun->GetAmmoType()) >= 20);
			TestTrue(TEXT("Патроны пистолета выданы"), Player->GetReserveAmmoInInventory(Pistol->GetAmmoType()) >= 24);
		}
	}
	else
	{
		AddError(TEXT("Не заспавнен игрок"));
	}

	WeaponSlotsTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
