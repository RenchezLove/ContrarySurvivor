// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны fix/respawn-at-campfire (баги дистрибуционной сборки
// с телефона, 08-06: петля смерти в поле + пропавший баннер задачи после «Продолжить»).
// Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.RespawnContinue; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE: чистое правило «восстанавливать ли интро-баннер после Продолжить»
// (AContrarySurvivorPlayerController::ShouldResumeIntroObjectiveAfterContinue) и поведение
// смертельного возрождения «всегда у костра» (вариант 1+3): испорченный сейв лечится,
// настоящий сейв у костра не трогается, пере-сейв смерти пишет уже точку костра.
// НЕ покрывается headless: фактическая отрисовка баннера/стрелки на экране, переход
// «вошёл в деревню -> найти старосту» и точный Z у костра (трасса пола вне PIE невалидна,
// урок 2026-06-16 — поэтому проверяются только координаты X/Y).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Tests/SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags RespawnContinueTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки SaveLoadTestWorld — она static в своём .cpp,
// дублируется по установленному в проекте паттерну «свой мир на файл теста»).
namespace RespawnTestWorld
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
		return FString::Printf(TEXT("Test_Respawn_%s"), Tag);
	}

	// Дистанция по горизонтали (X/Y) между актором и точкой — Z в headless не проверяем.
	static float Dist2D(const AActor* Actor, const FVector& Point)
	{
		return Actor ? FVector::Dist2D(Actor->GetActorLocation(), Point) : TNumericLimits<float>::Max();
	}
}

// «Продолжить» посреди интро-этапа: журнал квестов после загрузки пуст (до старосты игрок
// не дошёл) — баннер задачи и стрелка-указатель обязаны восстановиться. С непустым журналом
// или выключенным интро — нет (ориентиры даёт трекер квестов / отладочный режим).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContinueRestoresIntroObjectiveRuleTest,
	"ContrarySurvivor.RespawnContinue.Continue.RestoresIntroObjectiveRule", RespawnContinueTestFlags)

bool FContinueRestoresIntroObjectiveRuleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Интро включено + журнал пуст: баннер и стрелку восстанавливаем"),
		AContrarySurvivorPlayerController::ShouldResumeIntroObjectiveAfterContinue(
			/*bIntroEnabled=*/true, /*RestoredQuestCount=*/0));
	TestFalse(TEXT("Журнал не пуст (игрок дошёл до старосты): интро-баннер не нужен"),
		AContrarySurvivorPlayerController::ShouldResumeIntroObjectiveAfterContinue(true, 1));
	TestFalse(TEXT("Интро выключено (отладка): баннер не восстанавливаем"),
		AContrarySurvivorPlayerController::ShouldResumeIntroObjectiveAfterContinue(false, 0));
	return true;
}

// ===========================================================================
// Петля смерти (телефон, дистрибуционная сборка 08-06): в слоте закреплена точка ЧИСТОГО
// ПОЛЯ (пере-сейв смерти после фолбэка «сейва нет» писал стартовую точку). Лечение:
// смертельное возрождение с точкой дальше RespawnNearCampfireRadius от ближайшего костра
// переставляет игрока к костру, и пере-сейв смерти пишет уже точку костра (петля не
// закрепляется повторно).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathRespawnHealsPoisonedSaveTest,
	"ContrarySurvivor.RespawnContinue.DeathRespawn.HealsPoisonedSaveToCampfire", RespawnContinueTestFlags)

bool FDeathRespawnHealsPoisonedSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = RespawnTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = RespawnTestWorld::TestSlot(TEXT("Poisoned"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	const FVector FieldLoc(8000.0f, 0.0f, 100.0f); // «чистое поле», заведомо дальше радиуса 1000

	ACampfire* Campfire = RespawnTestWorld::Spawn<ACampfire>(World, CampfireLoc);
	ASaveTestPlayerCharacter* Player = RespawnTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FieldLoc);
	if (Campfire && Player)
	{
		Player->UseTestSaveSlot(Slot);

		// Испорченный слот: настоящий сейв (bHasData=true), но точка — поле (как пере-сейв
		// смерти дистрибуционной сборки закрепил стартовую точку с волком).
		TestTrue(TEXT("Сейв с точкой поля записан"), Player->SaveGame());

		Player->Respawn(/*bBackpackRescued=*/false);

		// Игрок у костра (в его зоне), а не в поле из слота.
		TestTrue(FString::Printf(TEXT("После возрождения игрок у костра (%.0f см от него)"),
			RespawnTestWorld::Dist2D(Player, CampfireLoc)),
			RespawnTestWorld::Dist2D(Player, CampfireLoc) <= 300.0f);

		// Пере-сейв смерти записал в слот уже точку костра — петля не закрепляется.
		const UContrarySaveGame* Save = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (TestNotNull(TEXT("Слот после возрождения читается"), Save))
		{
			TestTrue(TEXT("В слоте после возрождения — точка у костра, не поле"),
				FVector::Dist2D(Save->PlayerLocation, CampfireLoc) <= 300.0f);
		}
	}
	else
	{
		AddError(TEXT("Не заспавнились костёр или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	RespawnTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// Настоящий сейв у костра ЛЕЧИТЬ НЕЛЬЗЯ: точка в пределах RespawnNearCampfireRadius
// остаётся как есть (штатное «возрождение на последнем костре», GDD §7.8).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathRespawnKeepsValidCampfireSaveTest,
	"ContrarySurvivor.RespawnContinue.DeathRespawn.KeepsValidCampfireSave", RespawnContinueTestFlags)

bool FDeathRespawnKeepsValidCampfireSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = RespawnTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = RespawnTestWorld::TestSlot(TEXT("Valid"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	const FVector SavedLoc(250.0f, 0.0f, 100.0f); // у края зоны костра — как пишет автосейв входа

	ACampfire* Campfire = RespawnTestWorld::Spawn<ACampfire>(World, CampfireLoc);
	ASaveTestPlayerCharacter* Player = RespawnTestWorld::Spawn<ASaveTestPlayerCharacter>(World, SavedLoc);
	if (Campfire && Player)
	{
		Player->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Сейв у костра записан"), Player->SaveGame());

		// Игрок ушёл и погиб вдали — возрождение обязано вернуть РОВНО в сохранённую точку.
		Player->SetActorLocation(FVector(6000.0f, 6000.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Player->Respawn(/*bBackpackRescued=*/false);

		TestTrue(FString::Printf(TEXT("Возрождение в сохранённой точке (отклонение %.0f см)"),
			RespawnTestWorld::Dist2D(Player, SavedLoc)),
			RespawnTestWorld::Dist2D(Player, SavedLoc) <= 1.0f);
	}
	else
	{
		AddError(TEXT("Не заспавнились костёр или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	RespawnTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// Сейва нет вовсе (смерть до первого костра — худший сценарий новичка): фолбэк на
// стартовую точку теперь ДОЛЕЧИВАЕТСЯ до костра, а не оставляет игрока в поле с волком.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathRespawnNoSaveGoesToCampfireTest,
	"ContrarySurvivor.RespawnContinue.DeathRespawn.NoSaveGoesToCampfire", RespawnContinueTestFlags)

bool FDeathRespawnNoSaveGoesToCampfireTest::RunTest(const FString& Parameters)
{
	UWorld* World = RespawnTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = RespawnTestWorld::TestSlot(TEXT("NoSave"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	const FVector StartLoc(8000.0f, 0.0f, 100.0f); // стартовое поле (InitialSpawnTransform)

	ACampfire* Campfire = RespawnTestWorld::Spawn<ACampfire>(World, CampfireLoc);
	ASaveTestPlayerCharacter* Player = RespawnTestWorld::Spawn<ASaveTestPlayerCharacter>(World, StartLoc);
	if (Campfire && Player)
	{
		Player->UseTestSaveSlot(Slot);
		TestFalse(TEXT("Сейва нет"), Player->HasSaveGame());

		Player->Respawn(/*bBackpackRescued=*/false);

		TestTrue(FString::Printf(TEXT("Без сейва возрождение у костра (%.0f см от него)"),
			RespawnTestWorld::Dist2D(Player, CampfireLoc)),
			RespawnTestWorld::Dist2D(Player, CampfireLoc) <= 300.0f);
	}
	else
	{
		AddError(TEXT("Не заспавнились костёр или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	RespawnTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
