// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты сюжетных сообщений (Build 1.2). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Story; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывают ЧИСТУЮ логику гейта «запланировать сообщение конца сюжета один раз»
// (FEndOfStoryGate, паттерн FLimpFirstHintState). НЕ покрывается headless: сам показ
// плашки, таймер 30 с и запись флага в слот сейва (нужен PIE).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/EndOfStoryWidget.h" // FEndOfStoryGate

// Контекст всех приложений + продуктовый фильтр — как у остальных тестов проекта.
static constexpr EAutomationTestFlags StoryTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Сценка не проиграна профилю — планировать нечего, гейт не расходуется.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndOfStoryGateNoEpilogueTest,
	"ContrarySurvivor.Story.EndOfStoryGate.NoEpilogueNoSchedule", StoryTestFlags)
bool FEndOfStoryGateNoEpilogueTest::RunTest(const FString& Parameters)
{
	FEndOfStoryGate Gate;
	TestFalse(TEXT("сценка не видена: показ не планируется"),
		Gate.ShouldSchedule(/*bEpilogueSeen=*/false, /*bAlreadyShownInSave=*/false));
	// Гейт не потрачен: сценка доиграна позже в той же сессии — показ планируется.
	TestTrue(TEXT("после сценки в той же сессии: показ планируется"),
		Gate.ShouldSchedule(true, false));
	return true;
}

// Сообщение уже показывалось этому сохранению — не планируем (один раз за сохранение).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndOfStoryGateAlreadyShownTest,
	"ContrarySurvivor.Story.EndOfStoryGate.AlreadyShownInSave", StoryTestFlags)
bool FEndOfStoryGateAlreadyShownTest::RunTest(const FString& Parameters)
{
	FEndOfStoryGate Gate;
	TestFalse(TEXT("флаг сейва стоит: показ не планируется"),
		Gate.ShouldSchedule(/*bEpilogueSeen=*/true, /*bAlreadyShownInSave=*/true));
	return true;
}

// Нормальный путь: планируется РОВНО один раз; повторные вызовы (триггер диалога +
// страховочная проверка сейва зовут гейт независимо) — false, второй таймер не заводится.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndOfStoryGateOnceTest,
	"ContrarySurvivor.Story.EndOfStoryGate.SchedulesOnce", StoryTestFlags)
bool FEndOfStoryGateOnceTest::RunTest(const FString& Parameters)
{
	FEndOfStoryGate Gate;
	TestTrue(TEXT("первый вызов: показ планируется"), Gate.ShouldSchedule(true, false));
	TestFalse(TEXT("повторный вызов: второй таймер не заводится"), Gate.ShouldSchedule(true, false));
	TestFalse(TEXT("повтор с любыми аргументами: тоже нет"), Gate.ShouldSchedule(true, true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
