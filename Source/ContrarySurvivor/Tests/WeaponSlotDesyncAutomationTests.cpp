// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты по спаму рассинхрона оружия (ТЗ Рината 07-08, пункт 4):
// на телефоне лог заваливали строки «CurrentWeapon 'BP_Pistol_C_0' is ARangedWeapon,
// but != RangedWeaponInstance ('null')» из PlayerStatsWidget и TouchControlsWidget.
//
// КОРЕНЬ (подтверждён журналом телефона phone-b6-log.txt, кадр [0]: «EquipWeapon:
// Equipped BP_Pistol_C_...» при старте, без строки «старт без огнестрела — в руках нож»):
// легаси-граф BP_PlayerCharacter на ReceiveBeginPlay спавнит BP_Pistol и зовёт
// EquipWeapon НАПРЯМУЮ, мимо слота RangedWeaponInstance. CurrentWeapon становится
// пистолетом, слот остаётся null — ровно наблюдаемый рассинхрон.
//
// Здесь проверяется:
//  1) легаси-путь (EquipWeapon дальнобоя БЕЗ слота) воспроизводит рассинхрон, а
//     APlayerCharacter::ReconcileOutOfSlotRangedWeapon() его лечит (пистолет снят и
//     уничтожен, в руках снова нож);
//  2) штатный путь покупки (TryAdoptRangedWeapon -> SwitchWeapon) страховкой НЕ задет;
//  3) дроссель Warning'а (WeaponUiSyncLog::ShouldLogDesyncOnce): одна строка на ЭПИЗОД
//     рассинхрона, не каждая проверка кадра.
//
// Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.WeaponSlotDesync; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// НЕ покрывается headless: сам видимый спам/иконка на экране (Slate требует PIE — паттерн
// проекта), и сработка страховки именно в BeginPlay при живом BP_PlayerCharacter (граф BP
// в C++-тесте не выполняется — проверяем функцию-страховку напрямую).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/UI/WeaponUiSyncLog.h"
#include "ARangedWeapon.h"
#include "APistol.h"
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags WeaponSlotDesyncTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки WeaponUiGatingTestWorld — каждый файл теста
// держит свою, паттерн проекта: свои тесты всегда в НОВОМ файле).
namespace WeaponSlotDesyncTestWorld
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
}

// ===========================================================================
// 1. Легаси-путь BP (EquipWeapon дальнобоя БЕЗ слота) воспроизводит рассинхрон
//    «в руках дальнобой, слот пуст»; ReconcileOutOfSlotRangedWeapon() лечит его:
//    пистолет снят и уничтожен, в руках нож, условие показа патронов снова ложно.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotDesyncLegacyEquipTest,
	"ContrarySurvivor.WeaponSlotDesync.LegacyEquipBypassesSlotAndIsHealed", WeaponSlotDesyncTestFlags)

bool FWeaponSlotDesyncLegacyEquipTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponSlotDesyncTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		APlayerCharacter* Player = WeaponSlotDesyncTestWorld::Spawn<APlayerCharacter>(World);
		if (!TestNotNull(TEXT("Игрок заспавнен"), Player))
		{
			WeaponSlotDesyncTestWorld::Destroy(World);
			return false;
		}

		// Старт чистый: нож в руках, слот огнестрела пуст, условие «патроны/иконка» ложно.
		TestNull(TEXT("Слот огнестрела на старте пуст"), Player->GetRangedWeaponInstance());
		TestTrue(TEXT("Старт: в руках нож (MeleeWeaponInstance)"),
			Player->GetCurrentWeapon() == Player->GetMeleeWeaponInstance()
			&& Player->GetCurrentWeapon() != nullptr);

		// Легаси-граф BP: спавн пистолета + EquipWeapon напрямую, слот не трогается.
		FActorSpawnParameters Sp;
		Sp.Owner = Player;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APistol* LegacyPistol = World->SpawnActor<APistol>(APistol::StaticClass(),
			FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Sp);
		if (!TestNotNull(TEXT("Пистолет легаси-пути заспавнен"), LegacyPistol))
		{
			WeaponSlotDesyncTestWorld::Destroy(World);
			return false;
		}
		Player->EquipWeapon(LegacyPistol);

		// Рассинхрон ДО лечения воспроизвёлся: ровно состояние из лога телефона.
		ARangedWeapon* InHands = Cast<ARangedWeapon>(Player->GetCurrentWeapon());
		TestNotNull(TEXT("ДО лечения: в руках дальнобой (как на телефоне)"), InHands);
		TestNull(TEXT("ДО лечения: слот по-прежнему пуст — рассинхрон"),
			Player->GetRangedWeaponInstance());
		TestTrue(TEXT("ДО лечения: условие Warning'а виджетов истинно"),
			InHands != nullptr && InHands != Player->GetRangedWeaponInstance());

		// Лечение (в игре зовётся в конце BeginPlay, после отработки графа BP).
		Player->ReconcileOutOfSlotRangedWeapon();

		TestTrue(TEXT("ПОСЛЕ: в руках снова нож"),
			Player->GetCurrentWeapon() == Player->GetMeleeWeaponInstance()
			&& Player->GetCurrentWeapon() != nullptr);
		TestNull(TEXT("ПОСЛЕ: слот пуст (старт без огнестрела, решение Рината 05-08)"),
			Player->GetRangedWeaponInstance());
		TestFalse(TEXT("ПОСЛЕ: пистолет-артефакт уничтожен"), IsValid(LegacyPistol));
		TestNull(TEXT("ПОСЛЕ: условие показа патронов/иконки ложно"),
			Cast<ARangedWeapon>(Player->GetCurrentWeapon()));
	}

	WeaponSlotDesyncTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 2. Штатный путь покупки не задет: TryAdoptRangedWeapon -> SwitchWeapon кладёт пистолет
//    в слот и в руки; страховка ничего не меняет и не уничтожает.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotDesyncLegitPathUntouchedTest,
	"ContrarySurvivor.WeaponSlotDesync.LegitAdoptPathUntouched", WeaponSlotDesyncTestFlags)

bool FWeaponSlotDesyncLegitPathUntouchedTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponSlotDesyncTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		APlayerCharacter* Player = WeaponSlotDesyncTestWorld::Spawn<APlayerCharacter>(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (!TestNotNull(TEXT("Игрок заспавнен"), Player) || !TestNotNull(TEXT("Рюкзак есть"), Inv))
		{
			WeaponSlotDesyncTestWorld::Destroy(World);
			return false;
		}

		// Покупка у торговца: предмет в рюкзак -> усыновление в слот -> игрок берёт в руки.
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APistol* Bought = World->SpawnActor<APistol>(APistol::StaticClass(),
			FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Sp);
		if (!TestNotNull(TEXT("Пистолет покупки заспавнен"), Bought))
		{
			WeaponSlotDesyncTestWorld::Destroy(World);
			return false;
		}
		Inv->AddItem(Bought);
		TestTrue(TEXT("Пистолет занял слот огнестрела"), Player->TryAdoptRangedWeapon(Bought));
		Player->SwitchWeapon();
		TestTrue(TEXT("Пистолет в руках и совпадает со слотом"),
			Player->GetCurrentWeapon() == Bought
			&& Player->GetRangedWeaponInstance() == Bought);

		// Страховка на согласованном состоянии — no-op.
		Player->ReconcileOutOfSlotRangedWeapon();

		TestTrue(TEXT("После страховки пистолет остался в руках"),
			Player->GetCurrentWeapon() == Bought);
		TestTrue(TEXT("После страховки слот не тронут"),
			Player->GetRangedWeaponInstance() == Bought);
		TestTrue(TEXT("Пистолет жив"), IsValid(Bought));
	}

	WeaponSlotDesyncTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Дроссель Warning'а: одна строка при ВХОДЕ в рассинхрон, молчание пока он длится,
//    после выхода и нового входа — снова ровно одна строка.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSlotDesyncLogThrottleTest,
	"ContrarySurvivor.WeaponSlotDesync.WarningLoggedOncePerEpisode", WeaponSlotDesyncTestFlags)

bool FWeaponSlotDesyncLogThrottleTest::RunTest(const FString& Parameters)
{
	bool bLogged = false;

	// Нет рассинхрона — не пишем и не взводим флаг.
	TestFalse(TEXT("Кадр без рассинхрона: молчание"),
		WeaponUiSyncLog::ShouldLogDesyncOnce(false, bLogged));
	TestFalse(TEXT("Флаг не взведён"), bLogged);

	// Эпизод 1: первый кадр рассинхрона — пишем, дальше молчим (раньше спамило каждый кадр).
	TestTrue(TEXT("Вход в рассинхрон: одна строка"),
		WeaponUiSyncLog::ShouldLogDesyncOnce(true, bLogged));
	TestFalse(TEXT("Второй кадр эпизода: молчание"),
		WeaponUiSyncLog::ShouldLogDesyncOnce(true, bLogged));
	TestFalse(TEXT("Сотый кадр эпизода: молчание"),
		WeaponUiSyncLog::ShouldLogDesyncOnce(true, bLogged));

	// Выход из рассинхрона сбрасывает флаг.
	TestFalse(TEXT("Выход из рассинхрона: молчание"),
		WeaponUiSyncLog::ShouldLogDesyncOnce(false, bLogged));
	TestFalse(TEXT("Флаг сброшен"), bLogged);

	// Эпизод 2: новый вход — снова ровно одна строка.
	TestTrue(TEXT("Новый вход: снова одна строка"),
		WeaponUiSyncLog::ShouldLogDesyncOnce(true, bLogged));
	TestFalse(TEXT("И снова молчание"),
		WeaponUiSyncLog::ShouldLogDesyncOnce(true, bLogged));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
