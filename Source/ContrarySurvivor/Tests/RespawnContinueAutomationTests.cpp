// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны fix/respawn-at-campfire (баги дистрибуционной сборки
// с телефона, 08-06: петля смерти в поле + пропавший баннер задачи после «Продолжить»).
// Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.RespawnContinue; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE: чистое правило «восстанавливать ли интро-баннер после Продолжить»
// (AContrarySurvivorPlayerController::ShouldResumeIntroObjectiveAfterContinue).
// НЕ покрывается headless: фактическая отрисовка баннера/стрелки на экране и переход
// «вошёл в деревню -> найти старосту» (нужен живой PIE/осмотр).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"

static constexpr EAutomationTestFlags RespawnContinueTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

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

#endif // WITH_DEV_AUTOMATION_TESTS
