// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты Build 1.2 (rewarded-реклама + потери смерти). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Ads; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывают ЧИСТУЮ логику: арифметику потерь при смерти (DeathLoss::Compute — 70/50 без
// рекламы, 10/10 со «Спасти рюкзак», половина потерянного падает мешком; переопределение
// Рината поверх ТЗ №1) и условия показа rewarded-кнопок (AdGating — 15-минутный гейт,
// суточные счётчики по календарной дате, кулдаун). НЕ покрывается headless: показ
// заглушки ролика, снятие предметов из живого рюкзака, запись счётчиков в слот сейва.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Ads/DeathLossLogic.h"

static constexpr EAutomationTestFlags AdsTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Обычная смерть с дефолтами Рината (70% предметов, 50% денег, половина падает):
// 10 расходников и 100 монет -> теряется 7 и 50; мешок: 4 предмета (3.5 вверх) и 25 монет.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathLossNormalTest,
	"ContrarySurvivor.Ads.DeathLoss.NormalDeath70And50", AdsTestFlags)
bool FDeathLossNormalTest::RunTest(const FString& Parameters)
{
	const DeathLoss::FPlan Plan = DeathLoss::Compute(10, 100.0f, 0.70f, 0.50f, 0.50f);
	TestEqual(TEXT("обычная смерть: теряется 7 из 10 расходников"), Plan.LostItems, 7);
	TestEqual(TEXT("обычная смерть: в мешок падает 4 (половина от 7, вверх)"), Plan.DroppedItems, 4);
	TestEqual(TEXT("обычная смерть: теряется 50 монет из 100"), Plan.LostMoney, 50.0f);
	TestEqual(TEXT("обычная смерть: в мешок падает 25 монет"), Plan.DroppedMoney, 25.0f);
	return true;
}

// Смерть после «Спасти рюкзак» (по 10%): 10 расходников и 100 монет -> теряется 1 и 10;
// мешок: 1 предмет (0.5 вверх) и 5 монет.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathLossRescuedTest,
	"ContrarySurvivor.Ads.DeathLoss.RescuedDeath10", AdsTestFlags)
bool FDeathLossRescuedTest::RunTest(const FString& Parameters)
{
	const DeathLoss::FPlan Plan = DeathLoss::Compute(10, 100.0f, 0.10f, 0.10f, 0.50f);
	TestEqual(TEXT("спасённый рюкзак: теряется 1 из 10 расходников"), Plan.LostItems, 1);
	TestEqual(TEXT("спасённый рюкзак: единственный потерянный падает мешком"), Plan.DroppedItems, 1);
	TestEqual(TEXT("спасённый рюкзак: теряется 10 монет"), Plan.LostMoney, 10.0f);
	TestEqual(TEXT("спасённый рюкзак: в мешок падает 5 монет"), Plan.DroppedMoney, 5.0f);
	return true;
}

// Края: один предмет теряется целиком (0.7 округляется вверх); пустой рюкзак и ноль
// денег дают нулевой план; доли вне [0..1] клампятся, отрицательные входы не ломают.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathLossEdgesTest,
	"ContrarySurvivor.Ads.DeathLoss.EdgesAndClamps", AdsTestFlags)
bool FDeathLossEdgesTest::RunTest(const FString& Parameters)
{
	const DeathLoss::FPlan Single = DeathLoss::Compute(1, 0.0f, 0.70f, 0.50f, 0.50f);
	TestEqual(TEXT("один расходник: теряется (0.7 -> 1)"), Single.LostItems, 1);
	TestEqual(TEXT("один расходник: он же падает мешком (0.5 -> 1)"), Single.DroppedItems, 1);
	TestEqual(TEXT("денег нет: потеря 0"), Single.LostMoney, 0.0f);

	const DeathLoss::FPlan Empty = DeathLoss::Compute(0, 0.0f, 0.70f, 0.50f, 0.50f);
	TestEqual(TEXT("пусто: предметов 0"), Empty.LostItems, 0);
	TestEqual(TEXT("пусто: мешок 0"), Empty.DroppedItems, 0);
	TestEqual(TEXT("пусто: монет 0"), Empty.LostMoney, 0.0f);

	const DeathLoss::FPlan Clamped = DeathLoss::Compute(4, 50.0f, 1.5f, -0.2f, 2.0f);
	TestEqual(TEXT("кламп: доля >1 теряет все 4"), Clamped.LostItems, 4);
	TestEqual(TEXT("кламп: доля падения >1 роняет все потерянные"), Clamped.DroppedItems, 4);
	TestEqual(TEXT("кламп: отрицательная доля денег даёт 0"), Clamped.LostMoney, 0.0f);

	const DeathLoss::FPlan Negative = DeathLoss::Compute(-5, -20.0f, 0.7f, 0.5f, 0.5f);
	TestEqual(TEXT("мусорный вход: предметов 0"), Negative.LostItems, 0);
	TestEqual(TEXT("мусорный вход: монет 0"), Negative.LostMoney, 0.0f);
	return true;
}

// Глобальный гейт игрового времени. Build 1.2.1 (ТЗ В1): порог 360 с (6 мин, Ринат
// утвердил ровно 360) вместо прежних 15 минут; граница включительно; порог передаётся
// вторым аргументом (EditAnywhere на игроке), константа — только дефолт.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdGatingPlaytimeTest,
	"ContrarySurvivor.Ads.Gating.PlaytimeGate6Min", AdsTestFlags)
bool FAdGatingPlaytimeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("дефолт порога = ровно 360 с (ТЗ В1)"), AdGating::MinPlaytimeSeconds, 360.0);
	TestFalse(TEXT("5:59 игрового времени: рекламы нет"), AdGating::IsPlaytimeGatePassed(359.0));
	TestTrue(TEXT("ровно 6:00: реклама разрешена"), AdGating::IsPlaytimeGatePassed(360.0));
	TestTrue(TEXT("больше порога: разрешена"), AdGating::IsPlaytimeGatePassed(5000.0));
	TestFalse(TEXT("свежая установка (0 сек): нет"), AdGating::IsPlaytimeGatePassed(0.0));
	// Порог настраиваемый: функция обязана уважать переданное значение, а не константу.
	TestTrue(TEXT("свой порог 120 с: 150 с проходит"), AdGating::IsPlaytimeGatePassed(150.0, 120.0));
	TestFalse(TEXT("свой порог 120 с: 100 с не проходит"), AdGating::IsPlaytimeGatePassed(100.0, 120.0));
	return true;
}

// Суточный счётчик: живёт ровно в свою календарную дату, полночь обнуляет.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdGatingUsesTodayTest,
	"ContrarySurvivor.Ads.Gating.DailyCounterResetsAtMidnight", AdsTestFlags)
bool FAdGatingUsesTodayTest::RunTest(const FString& Parameters)
{
	const FDateTime Today(2026, 7, 31, 23, 50, 0);
	TestEqual(TEXT("счётчика ещё не было: 0"),
		AdGating::UsesToday(Today, FDateTime(), 5), 0);
	TestEqual(TEXT("счётчик сегодняшний: возвращается как есть"),
		AdGating::UsesToday(Today, FDateTime(2026, 7, 31), 3), 3);
	TestEqual(TEXT("счётчик вчерашний: после полуночи 0"),
		AdGating::UsesToday(FDateTime(2026, 8, 1, 0, 5, 0), FDateTime(2026, 7, 31), 3), 0);
	TestEqual(TEXT("отрицательный мусор клампится в 0"),
		AdGating::UsesToday(Today, FDateTime(2026, 7, 31), -2), 0);
	return true;
}

// Кулдаун точки магазина (3 минуты): граница включительно; часы назад — не прошёл.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdGatingCooldownTest,
	"ContrarySurvivor.Ads.Gating.ShopCooldown3Min", AdsTestFlags)
bool FAdGatingCooldownTest::RunTest(const FString& Parameters)
{
	const FDateTime Now(2026, 7, 31, 12, 10, 0);
	TestTrue(TEXT("просмотров ещё не было: кулдаун пройден"),
		AdGating::IsCooldownPassed(Now, FDateTime(), 180.0));
	TestTrue(TEXT("ровно 3 минуты назад: пройден"),
		AdGating::IsCooldownPassed(Now, FDateTime(2026, 7, 31, 12, 7, 0), 180.0));
	TestFalse(TEXT("2:59 назад: не пройден"),
		AdGating::IsCooldownPassed(Now, FDateTime(2026, 7, 31, 12, 7, 1), 180.0));
	TestFalse(TEXT("метка в будущем (часы назад): не пройден"),
		AdGating::IsCooldownPassed(Now, FDateTime(2026, 7, 31, 12, 30, 0), 180.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
