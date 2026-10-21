// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты ловушки «заперт на базе учёных» (отзыв игрока на 0.4.0 из RuStore, 21.10.2026):
// «после убоя волков шёл на базу ниже по карте, волк загрыз, нажал сохранить рюкзак, меня выкинуло
// на базу учёных, выйти с неё не могу, шлагбаум закрыт, взял задание на рацию у охранника, но всё
// равно не открывается». Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.BaseTrap; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат:
//   - смерть и «Продолжить» возвращают героя к ПОСЛЕДНЕМУ костру, у которого он сохранялся
//     (GDD §7.8 «респаун на последней точке сейва (костёр)»), а не к костру, ближайшему к точке
//     последнего автосохранения в поле (автосохранение по ходу задания пишет точку где угодно);
//   - пока герой ни у одного костра не сохранялся (и в сохранениях старых версий) — костёр,
//     ближайший к месту появления героя в начале игры (деревня);
//   - закрытый шлагбаум выпускает героя, оказавшегося внутри базы, и не впускает снаружи.
// Слоты сохранения ИЗОЛИРОВАННЫЕ (ASaveTestPlayerCharacter), боевой слот не трогается.
// Z в мире без игровой сессии не проверяется (трасса пола невалидна) — только X/Y.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/QuestBarrier.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

static constexpr EAutomationTestFlags BaseTrapTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace BaseTrapTest
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

	static FString TestSlot(const TCHAR* Tag)
	{
		return FString::Printf(TEXT("Test_BaseTrap_%s"), Tag);
	}

	static float Dist2D(const AActor* Actor, const FVector& Point)
	{
		return Actor ? FVector::Dist2D(Actor->GetActorLocation(), Point) : TNumericLimits<float>::Max();
	}

	static void MoveTo(AActor* Actor, const FVector& Loc)
	{
		Actor->SetActorLocation(Loc, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Расклад уровня L_World_C в масштабе (дамп logs/trap_level_dump_2110.txt): костёр деревни
	// рядом со стартом, костёр базы учёных далеко, логово волков первого задания ближе к базе,
	// чем к деревне.
	static const FVector StartLoc(-600.0f, 0.0f, 100.0f);
	static const FVector VillageFireLoc(0.0f, 0.0f, 100.0f);
	static const FVector BaseFireLoc(12000.0f, 0.0f, 100.0f);
	static const FVector WolfFieldLoc(9000.0f, 3000.0f, 100.0f);
}

// ===========================================================================
// 1. Точное воспроизведение отзыва: герой сохранялся только у костра деревни, задание по
//    волкам записало автосохранение в поле у логова (ближе к базе учёных), герой погиб.
//    Возрождение — у костра деревни, а не у костра внутри закрытой базы.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaseTrapDeathRespawnAtLastCampfireTest,
	"ContrarySurvivor.BaseTrap.DeathRespawnAtLastUsedCampfire", BaseTrapTestFlags)

bool FBaseTrapDeathRespawnAtLastCampfireTest::RunTest(const FString& Parameters)
{
	UWorld* World = BaseTrapTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	const FString Slot = BaseTrapTest::TestSlot(TEXT("Death"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	ACampfire* VillageFire = BaseTrapTest::Spawn<ACampfire>(World, BaseTrapTest::VillageFireLoc);
	ACampfire* BaseFire = BaseTrapTest::Spawn<ACampfire>(World, BaseTrapTest::BaseFireLoc);
	ASaveTestPlayerCharacter* Player = BaseTrapTest::Spawn<ASaveTestPlayerCharacter>(World, BaseTrapTest::StartLoc);
	if (VillageFire && BaseFire && Player)
	{
		Player->UseTestSaveSlot(Slot);

		// Сохранение у костра деревни.
		BaseTrapTest::MoveTo(Player, BaseTrapTest::VillageFireLoc + FVector(150.0f, 0.0f, 0.0f));
		TestTrue(TEXT("Сохранение у костра деревни прошло"), VillageFire->SaveAtCampfire(Player));

		// Автосохранение по ходу задания у логова волков (точка в поле, ближе к базе учёных).
		BaseTrapTest::MoveTo(Player, BaseTrapTest::WolfFieldLoc);
		TestTrue(TEXT("Автосохранение в поле записано"), Player->SaveGame());

		// Волк догнал на пути; «Сохранить рюкзак» после ролика зовёт то же возрождение.
		BaseTrapTest::MoveTo(Player, BaseTrapTest::WolfFieldLoc + FVector(1500.0f, -1500.0f, 0.0f));
		Player->Respawn(/*bBackpackRescued=*/true);

		const float ToVillage = BaseTrapTest::Dist2D(Player, BaseTrapTest::VillageFireLoc);
		const float ToBase = BaseTrapTest::Dist2D(Player, BaseTrapTest::BaseFireLoc);
		TestTrue(FString::Printf(TEXT("Возрождение у костра деревни, где герой сохранялся (до него %.0f см, до костра базы %.0f см)"),
			ToVillage, ToBase), ToVillage <= 300.0f);

		// Пере-сохранение смерти закрепило костёр деревни, а не базу.
		const UContrarySaveGame* Save = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (TestNotNull(TEXT("Слот после возрождения читается"), Save))
		{
			TestTrue(TEXT("В слоте после возрождения — точка у костра деревни"),
				FVector::Dist2D(Save->PlayerLocation, BaseTrapTest::VillageFireLoc) <= 300.0f);
		}
	}
	else
	{
		AddError(TEXT("Не заспавнились костры или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	BaseTrapTest::Destroy(World);
	return true;
}

// ===========================================================================
// 2. Сохранение без единого посещённого костра (так выглядит и сохранение старой версии
//    0.4.0): «Продолжить» и смерть — к костру, ближайшему к месту появления в начале игры.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaseTrapNoCampfireYetUsesStartCampfireTest,
	"ContrarySurvivor.BaseTrap.NoCampfireYetUsesStartCampfire", BaseTrapTestFlags)

bool FBaseTrapNoCampfireYetUsesStartCampfireTest::RunTest(const FString& Parameters)
{
	UWorld* World = BaseTrapTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	const FString Slot = BaseTrapTest::TestSlot(TEXT("NoCampfire"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	ACampfire* VillageFire = BaseTrapTest::Spawn<ACampfire>(World, BaseTrapTest::VillageFireLoc);
	ACampfire* BaseFire = BaseTrapTest::Spawn<ACampfire>(World, BaseTrapTest::BaseFireLoc);
	ASaveTestPlayerCharacter* Player = BaseTrapTest::Spawn<ASaveTestPlayerCharacter>(World, BaseTrapTest::StartLoc);
	if (VillageFire && BaseFire && Player)
	{
		Player->UseTestSaveSlot(Slot);

		BaseTrapTest::MoveTo(Player, BaseTrapTest::WolfFieldLoc);
		TestTrue(TEXT("Автосохранение в поле записано"), Player->SaveGame());

		// «Продолжить» после перезапуска.
		BaseTrapTest::MoveTo(Player, BaseTrapTest::StartLoc);
		TestTrue(TEXT("«Продолжить» прошло"), Player->LoadGameForContinue());
		TestTrue(FString::Printf(TEXT("После «Продолжить» герой у костра деревни (%.0f см)"),
			BaseTrapTest::Dist2D(Player, BaseTrapTest::VillageFireLoc)),
			BaseTrapTest::Dist2D(Player, BaseTrapTest::VillageFireLoc) <= 300.0f);

		// Смерть с тем же сохранением в поле.
		BaseTrapTest::MoveTo(Player, BaseTrapTest::WolfFieldLoc);
		TestTrue(TEXT("Автосохранение в поле записано повторно"), Player->SaveGame());
		Player->Respawn(/*bBackpackRescued=*/false);
		TestTrue(FString::Printf(TEXT("После смерти герой у костра деревни (%.0f см)"),
			BaseTrapTest::Dist2D(Player, BaseTrapTest::VillageFireLoc)),
			BaseTrapTest::Dist2D(Player, BaseTrapTest::VillageFireLoc) <= 300.0f);
	}
	else
	{
		AddError(TEXT("Не заспавнились костры или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	BaseTrapTest::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Обратная сторона правила: сохранялся у костра базы (шлагбаум тогда был открыт) —
//    возрождение там же, даже если автосохранение в поле ближе к деревне.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaseTrapRespawnAtBaseWhenSavedThereTest,
	"ContrarySurvivor.BaseTrap.RespawnAtBaseCampfireWhenSavedThere", BaseTrapTestFlags)

bool FBaseTrapRespawnAtBaseWhenSavedThereTest::RunTest(const FString& Parameters)
{
	UWorld* World = BaseTrapTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	const FString Slot = BaseTrapTest::TestSlot(TEXT("SavedAtBase"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	ACampfire* VillageFire = BaseTrapTest::Spawn<ACampfire>(World, BaseTrapTest::VillageFireLoc);
	ACampfire* BaseFire = BaseTrapTest::Spawn<ACampfire>(World, BaseTrapTest::BaseFireLoc);
	ASaveTestPlayerCharacter* Player = BaseTrapTest::Spawn<ASaveTestPlayerCharacter>(World, BaseTrapTest::StartLoc);
	if (VillageFire && BaseFire && Player)
	{
		Player->UseTestSaveSlot(Slot);

		BaseTrapTest::MoveTo(Player, BaseTrapTest::BaseFireLoc + FVector(150.0f, 0.0f, 0.0f));
		TestTrue(TEXT("Сохранение у костра базы прошло"), BaseFire->SaveAtCampfire(Player));

		BaseTrapTest::MoveTo(Player, FVector(2500.0f, 0.0f, 100.0f));
		TestTrue(TEXT("Автосохранение в поле у деревни записано"), Player->SaveGame());

		Player->Respawn(/*bBackpackRescued=*/false);
		TestTrue(FString::Printf(TEXT("Возрождение у костра базы, где герой сохранялся последним (%.0f см)"),
			BaseTrapTest::Dist2D(Player, BaseTrapTest::BaseFireLoc)),
			BaseTrapTest::Dist2D(Player, BaseTrapTest::BaseFireLoc) <= 300.0f);
	}
	else
	{
		AddError(TEXT("Не заспавнились костры или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	BaseTrapTest::Destroy(World);
	return true;
}

// ===========================================================================
// 4. Закрытый шлагбаум выпускает героя изнутри базы и не впускает снаружи. Задания, которые
//    открывают шлагбаум, не взяты (журнал пуст) — как у игрока из отзыва.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBaseTrapBarrierLetsHeroOutTest,
	"ContrarySurvivor.BaseTrap.ClosedBarrierLetsHeroOut", BaseTrapTestFlags)

bool FBaseTrapBarrierLetsHeroOutTest::RunTest(const FString& Parameters)
{
	UWorld* World = BaseTrapTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	const FString Slot = BaseTrapTest::TestSlot(TEXT("Barrier"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	// Как на уровне: шлагбаум смотрит вперёд (+X) внутрь базы.
	const FVector BarrierLoc(5000.0f, 0.0f, 0.0f);
	AQuestBarrier* Barrier = BaseTrapTest::Spawn<AQuestBarrier>(World, BarrierLoc);
	ASaveTestPlayerCharacter* Player = BaseTrapTest::Spawn<ASaveTestPlayerCharacter>(World, BarrierLoc + FVector(-3000.0f, 0.0f, 100.0f));
	if (Barrier && Barrier->GetBlocker() && Player)
	{
		Player->UseTestSaveSlot(Slot);
		const FVector Up(0.0f, 0.0f, 100.0f);

		Barrier->RefreshNow();
		TestFalse(TEXT("Герой далеко снаружи: шлагбаум закрыт"), Barrier->IsOpen());

		BaseTrapTest::MoveTo(Player, BarrierLoc + FVector(-60.0f, 0.0f, 0.0f) + Up);
		Barrier->RefreshNow();
		TestFalse(TEXT("Герой подошёл к шлагбауму снаружи: шлагбаум закрыт"), Barrier->IsOpen());

		// Герой внутри базы (возродился у костра базы) идёт к выезду.
		BaseTrapTest::MoveTo(Player, BarrierLoc + FVector(900.0f, 0.0f, 0.0f) + Up);
		Barrier->RefreshNow();
		TestTrue(TEXT("Герой внутри базы у выезда: шлагбаум открыт"), Barrier->IsOpen());
		TestTrue(TEXT("Герой внутри базы у выезда: преграда выключена"),
			Barrier->GetBlocker()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);

		// Проходит через проезд — не закрывается у него на голове.
		BaseTrapTest::MoveTo(Player, BarrierLoc + FVector(0.0f, 0.0f, 0.0f) + Up);
		Barrier->RefreshNow();
		TestTrue(TEXT("Герой в проезде: шлагбаум открыт"), Barrier->IsOpen());
		BaseTrapTest::MoveTo(Player, BarrierLoc + FVector(-80.0f, 0.0f, 0.0f) + Up);
		Barrier->RefreshNow();
		TestTrue(TEXT("Герой только что вышел: шлагбаум ещё открыт"), Barrier->IsOpen());

		// Отошёл наружу — шлагбаум снова закрыт, задание по-прежнему нужно для входа.
		BaseTrapTest::MoveTo(Player, BarrierLoc + FVector(-500.0f, 0.0f, 0.0f) + Up);
		Barrier->RefreshNow();
		TestFalse(TEXT("Герой вышел и отошёл: шлагбаум закрыт"), Barrier->IsOpen());
		BaseTrapTest::MoveTo(Player, BarrierLoc + FVector(-60.0f, 0.0f, 0.0f) + Up);
		Barrier->RefreshNow();
		TestFalse(TEXT("Вернулся к шлагбауму снаружи: шлагбаум закрыт"), Barrier->IsOpen());
	}
	else
	{
		AddError(TEXT("Не заспавнились шлагбаум или тестовый игрок"));
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	BaseTrapTest::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
