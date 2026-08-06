// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты контрактов баланса (решения Рината 08-06).
// Запуск: UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Balance; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Проверяются КЛАСС-ДЕФОЛТЫ C++ (значения по умолчанию новых объектов). Настройка из
// Blueprint может их перекрыть — финальную скорость на живом уровне судит Ринат вживую;
// тест держит сам ЗАМЫСЕЛ: от волка в чистом поле не убежать, от бандита спринтом — можно.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"

static constexpr EAutomationTestFlags BalanceTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Решение Рината 08-06 (живой прогон: «игрок может убежать от волков — выглядит странно»):
// скорость погони волка выше скорости спринта игрока. Внутри «поводка» базы (ADR-036) волк
// настигает; спасение игрока — покинуть зону погони, а не зажать бег.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceWolfCatchesSprinterTest,
	"ContrarySurvivor.Balance.WolfCatchesSprintingPlayer", BalanceTestFlags)

bool FBalanceWolfCatchesSprinterTest::RunTest(const FString& Parameters)
{
	const APlayerCharacter* Player = GetDefault<APlayerCharacter>();
	const AWolfCharacter* Wolf = GetDefault<AWolfCharacter>();
	if (!TestNotNull(TEXT("Класс-дефолт игрока доступен"), Player) ||
		!TestNotNull(TEXT("Класс-дефолт волка доступен"), Wolf))
	{
		return false;
	}

	TestTrue(TEXT("Волк на погоне быстрее спринта игрока (от волка не убежать)"),
		Wolf->GetChaseSpeed() > Player->GetSprintSpeed());

	// «Чуть» быстрее, а не вдвое: перевес не больше 10% — иначе погоня превращается в мгновенную
	// смерть без шанса дотянуть до края «поводка».
	TestTrue(TEXT("Перевес волка над спринтом — в пределах 10%"),
		Wolf->GetChaseSpeed() <= Player->GetSprintSpeed() * 1.1f);
	return true;
}

// Замысел бандита прежний (комментарий в EnemyCharacter.cpp): догоняет шагающего,
// но от него можно оторваться спринтом. Решение 08-06 бандитов не трогало.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceBanditChaseTest,
	"ContrarySurvivor.Balance.BanditCatchesWalkerButNotSprinter", BalanceTestFlags)

bool FBalanceBanditChaseTest::RunTest(const FString& Parameters)
{
	const APlayerCharacter* Player = GetDefault<APlayerCharacter>();
	const AEnemyCharacter* Bandit = GetDefault<AEnemyCharacter>();
	if (!TestNotNull(TEXT("Класс-дефолт игрока доступен"), Player) ||
		!TestNotNull(TEXT("Класс-дефолт бандита доступен"), Bandit))
	{
		return false;
	}

	TestTrue(TEXT("Бандит догоняет шагающего игрока"),
		Bandit->GetChaseSpeed() > Player->GetWalkSpeed());
	TestTrue(TEXT("От бандита можно оторваться спринтом"),
		Bandit->GetChaseSpeed() < Player->GetSprintSpeed());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
