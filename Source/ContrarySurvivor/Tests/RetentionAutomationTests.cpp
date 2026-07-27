// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты этапа F (удержание). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Retention; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывают ЧИСТУЮ логику серии ежедневной награды (DailyReward::Compute, ADR-044 п.4) —
// без мира/сейва/виджетов. Числа = дефолты UDailyRewardComponent (25 / +10 / потолок 75).
// Плюс логика «раз за сессию» разовой подсказки хромоты (FLimpFirstHintState, Build 1).
// НЕ покрывается headless: показ окна/тостов (нужен PIE), запись в слот сейва.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Retention/DailyRewardLogic.h"
#include "ContrarySurvivor/UI/LimpIndicatorWidget.h" // FLimpFirstHintState (разовая подсказка хромоты)

// Контекст всех приложений + продуктовый фильтр — как у боевых тестов (CombatAutomationTests).
static constexpr EAutomationTestFlags RetentionTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace
{
	// Числа ADR-044 (дефолты UDailyRewardComponent). Имя НЕ «Base»: короткое Base перекрывает
	// глобальное объявление из заголовков делегатов UE — пачка предупреждений C4459.
	constexpr float RewardBase = 25.0f;
	constexpr float Step = 10.0f;
	constexpr float Cap = 75.0f;

	// Фиксированная «сегодняшняя» дата (полдень — проверяем, что время суток отбрасывается).
	const FDateTime Today(2026, 7, 11, 12, 30, 0);
}

// Первый вход профиля (входов ещё не было) — день 1, базовая сумма.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyRewardFirstLoginTest,
	"ContrarySurvivor.Retention.DailyReward.FirstLogin", RetentionTestFlags)
bool FDailyRewardFirstLoginTest::RunTest(const FString& Parameters)
{
	const DailyReward::FComputeResult R = DailyReward::Compute(Today, FDateTime(), 0, RewardBase, Step, Cap);
	TestTrue(TEXT("первый вход: награда выдаётся"), R.bGrant);
	TestEqual(TEXT("первый вход: серия = 1"), R.NewStreak, 1);
	TestEqual(TEXT("первый вход: сумма = база (25)"), R.Reward, RewardBase);
	return true;
}

// Повторный запуск в тот же календарный день — без награды, серия не меняется.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyRewardSameDayTest,
	"ContrarySurvivor.Retention.DailyReward.SameDayNoReward", RetentionTestFlags)
bool FDailyRewardSameDayTest::RunTest(const FString& Parameters)
{
	// Вход был сегодня утром, серия 3; перезапуск днём.
	const FDateTime SameDayMorning(2026, 7, 11, 0, 0, 0);
	const DailyReward::FComputeResult R = DailyReward::Compute(Today, SameDayMorning, 3, RewardBase, Step, Cap);
	TestFalse(TEXT("тот же день: награды нет"), R.bGrant);
	TestEqual(TEXT("тот же день: серия сохранена (3)"), R.NewStreak, 3);
	TestEqual(TEXT("тот же день: сумма 0"), R.Reward, 0.0f);
	return true;
}

// Вход на следующий календарный день — серия растёт, сумма +шаг (вчера входил -> +10).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyRewardNextDayTest,
	"ContrarySurvivor.Retention.DailyReward.NextDayStreakGrows", RetentionTestFlags)
bool FDailyRewardNextDayTest::RunTest(const FString& Parameters)
{
	const FDateTime Yesterday(2026, 7, 10, 23, 59, 0); // поздний вечер: важна дата, не время
	const DailyReward::FComputeResult R = DailyReward::Compute(Today, Yesterday, 1, RewardBase, Step, Cap);
	TestTrue(TEXT("следующий день: награда выдаётся"), R.bGrant);
	TestEqual(TEXT("следующий день: серия 1 -> 2"), R.NewStreak, 2);
	TestEqual(TEXT("следующий день: сумма 25+10=35"), R.Reward, 35.0f);
	return true;
}

// Пропуск календарного дня — серия сбрасывается на день 1 с базовой суммой.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyRewardSkipResetsTest,
	"ContrarySurvivor.Retention.DailyReward.SkipDayResetsStreak", RetentionTestFlags)
bool FDailyRewardSkipResetsTest::RunTest(const FString& Parameters)
{
	const FDateTime TwoDaysAgo(2026, 7, 9, 12, 0, 0);
	const DailyReward::FComputeResult R = DailyReward::Compute(Today, TwoDaysAgo, 5, RewardBase, Step, Cap);
	TestTrue(TEXT("после пропуска: награда выдаётся"), R.bGrant);
	TestEqual(TEXT("после пропуска: серия сброшена на 1"), R.NewStreak, 1);
	TestEqual(TEXT("после пропуска: сумма = база (25)"), R.Reward, RewardBase);
	return true;
}

// Потолок: день 6 серии = ровно 75; дальше серия растёт, но сумма остаётся 75.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyRewardCapTest,
	"ContrarySurvivor.Retention.DailyReward.CapAt75", RetentionTestFlags)
bool FDailyRewardCapTest::RunTest(const FString& Parameters)
{
	const FDateTime Yesterday(2026, 7, 10, 8, 0, 0);

	// День 6 (вчера была серия 5): 25 + 10*5 = 75 — ровно потолок.
	const DailyReward::FComputeResult Day6 = DailyReward::Compute(Today, Yesterday, 5, RewardBase, Step, Cap);
	TestTrue(TEXT("день 6: награда выдаётся"), Day6.bGrant);
	TestEqual(TEXT("день 6: серия 6"), Day6.NewStreak, 6);
	TestEqual(TEXT("день 6: сумма ровно потолок (75)"), Day6.Reward, Cap);

	// День 10 (вчера была серия 9): формула дала бы 115 — клампится в 75.
	const DailyReward::FComputeResult Day10 = DailyReward::Compute(Today, Yesterday, 9, RewardBase, Step, Cap);
	TestTrue(TEXT("день 10: награда выдаётся"), Day10.bGrant);
	TestEqual(TEXT("день 10: серия 10"), Day10.NewStreak, 10);
	TestEqual(TEXT("день 10: сумма клампится потолком (75)"), Day10.Reward, Cap);
	return true;
}

// Часы устройства перевели назад (дата «в будущем» относительно сегодня) — награды нет,
// серия не ломается (анти-абьюз/сбой часов).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDailyRewardClockRollbackTest,
	"ContrarySurvivor.Retention.DailyReward.ClockRollbackSafe", RetentionTestFlags)
bool FDailyRewardClockRollbackTest::RunTest(const FString& Parameters)
{
	const FDateTime Tomorrow(2026, 7, 12, 1, 0, 0); // «последний вход» позже текущей даты
	const DailyReward::FComputeResult R = DailyReward::Compute(Today, Tomorrow, 4, RewardBase, Step, Cap);
	TestFalse(TEXT("часы назад: награды нет"), R.bGrant);
	TestEqual(TEXT("часы назад: серия сохранена (4)"), R.NewStreak, 4);
	return true;
}

// Разовая подсказка хромоты (Build 1): срабатывает один раз за сессию, только когда игрок
// хромает и управление свободно; после Rearm (вылечился до показа) готова снова.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLimpFirstHintOnceTest,
	"ContrarySurvivor.Retention.LimpHint.FirstShowOnce", RetentionTestFlags)
bool FLimpFirstHintOnceTest::RunTest(const FString& Parameters)
{
	FLimpFirstHintState State;
	TestFalse(TEXT("не хромает: показа нет"), State.ShouldTrigger(/*bLimping=*/false, /*bControlFree=*/true));
	TestFalse(TEXT("хромает, но управление занято (интро/модалка): ждём"), State.ShouldTrigger(true, false));
	TestTrue(TEXT("хромает + управление свободно: показ"), State.ShouldTrigger(true, true));
	TestFalse(TEXT("повторный вызов: показа нет (раз за сессию)"), State.ShouldTrigger(true, true));
	State.Rearm();
	TestTrue(TEXT("после Rearm (вылечился до показа): подсказка снова готова"), State.ShouldTrigger(true, true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
