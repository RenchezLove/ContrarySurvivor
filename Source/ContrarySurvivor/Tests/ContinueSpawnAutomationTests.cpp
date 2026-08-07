// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты фикса fix/spawn-at-playerstart (жалоба Рината 08-07: «игрок
// стартует хуй пойми где вместо player start» — «Продолжить» применяло испорченную точку
// слота старой сборки и роняло игрока посреди поля). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.ContinueSpawn; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE: LoadGameForContinue() лечит точку слота дальше
// RespawnNearCampfireRadius от ближайшего костра (перестановка к костру), не трогает
// настоящий сейв у костра и оставляет точку как есть в мире без костров (служебные/тестовые
// миры). Проверяются только X/Y: трасса пола (Z) вне PIE невалидна, урок 2026-06-16.
// НЕ покрывается headless: сам стартовый экран (виджет) и цепочка «кнопка -> контроллер».

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Tests/SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags ContinueSpawnTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки RespawnTestWorld — по установленному в проекте
// паттерну «свой мир на файл теста», чтобы не гоняться с чужими правками в одном файле).
namespace ContinueSpawnTestWorld
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

	// Именованный тестовый слот — ЗАВЕДОМО не боевой 'ContrarySave' (в нём живой прогресс
	// Рината, ADR-058). Подчищается до и после каждого теста.
	static FString TestSlot(const TCHAR* Tag)
	{
		return FString::Printf(TEXT("Test_ContinueSpawn_%s"), Tag);
	}

	// Дистанция по горизонтали (X/Y) между актором и точкой — Z в headless не проверяем.
	static float Dist2D(const AActor* Actor, const FVector& Point)
	{
		return Actor ? FVector::Dist2D(Actor->GetActorLocation(), Point) : TNumericLimits<float>::Max();
	}
}

// ===========================================================================
// Испорченный слот (точка чистого поля, закреплённая петлёй смерти старой сборки — ADR-061):
// «Продолжить» обязано вылечить точку перестановкой к костру, а не ронять игрока в поле.
// Именно этот случай Ринат увидел 08-07 как «стартую хуй пойми где вместо player start».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContinueHealsPoisonedSaveTest,
	"ContrarySurvivor.ContinueSpawn.HealsPoisonedSaveToCampfire", ContinueSpawnTestFlags)

bool FContinueHealsPoisonedSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = ContinueSpawnTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = ContinueSpawnTestWorld::TestSlot(TEXT("Poisoned"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	const FVector FieldLoc(8000.0f, 0.0f, 100.0f);   // «чистое поле», заведомо дальше радиуса 1000
	const FVector StartLoc(500.0f, 500.0f, 100.0f);  // где игрок стоит на стартовом экране

	ACampfire* Campfire = ContinueSpawnTestWorld::Spawn<ACampfire>(World, CampfireLoc);
	ASaveTestPlayerCharacter* Player = ContinueSpawnTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FieldLoc);
	if (Campfire && Player)
	{
		Player->UseTestSaveSlot(Slot);

		// Испорченный слот: настоящий сейв (bHasData=true), но точка — поле.
		TestTrue(TEXT("Сейв с точкой поля записан"), Player->SaveGame());

		// Игрок «перезапустил игру»: стоит на стартовой точке и жмёт «Продолжить».
		Player->SetActorLocation(StartLoc, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("«Продолжить» прошло по реальному сейву"), Player->LoadGameForContinue());

		// Игрок у костра (в его зоне), а не в поле из слота.
		TestTrue(FString::Printf(TEXT("После «Продолжить» игрок у костра (%.0f см от него)"),
			ContinueSpawnTestWorld::Dist2D(Player, CampfireLoc)),
			ContinueSpawnTestWorld::Dist2D(Player, CampfireLoc) <= 300.0f);
	}
	else
	{
		AddError(TEXT("Не заспавнились костёр или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	ContinueSpawnTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// Настоящий сейв у костра ЛЕЧИТЬ НЕЛЬЗЯ: «Продолжить» возвращает РОВНО в сохранённую точку
// (позиция из сейва легитимна — ADR-061; лечится только точка дальше радиуса от костра).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContinueKeepsValidCampfireSaveTest,
	"ContrarySurvivor.ContinueSpawn.KeepsValidCampfireSave", ContinueSpawnTestFlags)

bool FContinueKeepsValidCampfireSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = ContinueSpawnTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = ContinueSpawnTestWorld::TestSlot(TEXT("Valid"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	const FVector SavedLoc(250.0f, 0.0f, 100.0f); // у края зоны костра — как пишет автосейв входа

	ACampfire* Campfire = ContinueSpawnTestWorld::Spawn<ACampfire>(World, CampfireLoc);
	ASaveTestPlayerCharacter* Player = ContinueSpawnTestWorld::Spawn<ASaveTestPlayerCharacter>(World, SavedLoc);
	if (Campfire && Player)
	{
		Player->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Сейв у костра записан"), Player->SaveGame());

		// «Перезапуск»: игрок на другой точке жмёт «Продолжить».
		Player->SetActorLocation(FVector(6000.0f, 6000.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("«Продолжить» прошло по реальному сейву"), Player->LoadGameForContinue());

		TestTrue(FString::Printf(TEXT("«Продолжить» вернуло в сохранённую точку (отклонение %.0f см)"),
			ContinueSpawnTestWorld::Dist2D(Player, SavedLoc)),
			ContinueSpawnTestWorld::Dist2D(Player, SavedLoc) <= 1.0f);
	}
	else
	{
		AddError(TEXT("Не заспавнились костёр или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	ContinueSpawnTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// Мир без костра (служебный/тестовый): лечить некуда — «Продолжить» оставляет точку слота
// как есть (ранний выход RelocateToCampfireIfSavedPointFar, поведение прежних тестов сейва).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContinueWithoutCampfireKeepsSavedPointTest,
	"ContrarySurvivor.ContinueSpawn.NoCampfireKeepsSavedPoint", ContinueSpawnTestFlags)

bool FContinueWithoutCampfireKeepsSavedPointTest::RunTest(const FString& Parameters)
{
	UWorld* World = ContinueSpawnTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = ContinueSpawnTestWorld::TestSlot(TEXT("NoCampfire"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	const FVector SavedLoc(4000.0f, -2000.0f, 100.0f);

	ASaveTestPlayerCharacter* Player = ContinueSpawnTestWorld::Spawn<ASaveTestPlayerCharacter>(World, SavedLoc);
	if (Player)
	{
		Player->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Сейв записан"), Player->SaveGame());

		Player->SetActorLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("«Продолжить» прошло по реальному сейву"), Player->LoadGameForContinue());

		TestTrue(FString::Printf(TEXT("Без костров точка слота не тронута (отклонение %.0f см)"),
			ContinueSpawnTestWorld::Dist2D(Player, SavedLoc)),
			ContinueSpawnTestWorld::Dist2D(Player, SavedLoc) <= 1.0f);
	}
	else
	{
		AddError(TEXT("Не заспавнился тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	ContinueSpawnTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
