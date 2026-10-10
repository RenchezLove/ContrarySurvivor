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
#include "ContrarySurvivor/Actors/ScientistNPCs.h"  // метка квестового учёного
#include "ContrarySurvivor/Actors/TalkerNPC.h"      // правило «чем кончается сюжет»
#include "ContrarySurvivor/Components/QuestComponent.h"

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

// 10.10.2026: сообщение выходит только после сдачи ноутбука квестовому учёному. Сданное
// старосте задание на бандитов (после него идёт сценка про ноутбук) сюжет НЕ заканчивает —
// ни в живой игре, ни по страховке при загрузке сохранения: обе дороги спрашивают журнал.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndOfStoryOnlyAfterScientistTest,
	"ContrarySurvivor.Story.EndOfStory.OnlyAfterLaptopGivenToScientist", StoryTestFlags)
bool FEndOfStoryOnlyAfterScientistTest::RunTest(const FString& Parameters)
{
	FQuest First;
	First.QuestId = FName(TEXT("QA_First"));
	First.State = EQuestState::TurnedIn;
	FQuest Bandits; // сдаётся старосте: метки получателя нет
	Bandits.QuestId = FName(TEXT("QA_Bandits"));
	Bandits.State = EQuestState::TurnedIn;
	FQuest Laptop; // сдаётся квестовому учёному
	Laptop.QuestId = FName(TEXT("QA_Laptop"));
	Laptop.TurnInNpcTag = ScientistBaseTags::QuestScientist;

	// Сценка старосты только что сыграла: первые два задания сданы, третьего ещё нет.
	TArray<FQuest> Journal = { First, Bandits };
	TestFalse(TEXT("После сценки старосты сюжет не считается пройденным"),
		AQuestGiverNPC::IsStoryEndingQuestTurnedIn(Journal));
	FEndOfStoryGate Gate;
	TestFalse(TEXT("После сценки старосты сообщение не планируется"),
		Gate.ShouldSchedule(AQuestGiverNPC::IsStoryEndingQuestTurnedIn(Journal), /*bAlreadyShownInSave=*/false));

	// Ноутбук несут учёным: предложено, взято, готово к сдаче — всё ещё не конец.
	for (const EQuestState State : { EQuestState::NotStarted, EQuestState::Active, EQuestState::Completed })
	{
		Laptop.State = State;
		Journal = { First, Bandits, Laptop };
		TestFalse(TEXT("Пока ноутбук не сдан учёному, сообщение не планируется"),
			Gate.ShouldSchedule(AQuestGiverNPC::IsStoryEndingQuestTurnedIn(Journal), false));
		TestEqual(TEXT("«Задание взято» — всё, кроме «только предложено»"),
			AQuestGiverNPC::IsStoryEndingQuestTaken(Journal), State != EQuestState::NotStarted);
	}

	// Сдан учёному — планируется, один раз; уже показанное в сохранении не повторяется.
	Laptop.State = EQuestState::TurnedIn;
	Journal = { First, Bandits, Laptop };
	TestTrue(TEXT("Ноутбук сдан учёному — сюжет пройден"), AQuestGiverNPC::IsStoryEndingQuestTurnedIn(Journal));
	FEndOfStoryGate ShownGate;
	TestFalse(TEXT("Сообщение уже показано в этом сохранении — второй раз не планируется"),
		ShownGate.ShouldSchedule(AQuestGiverNPC::IsStoryEndingQuestTurnedIn(Journal), /*bAlreadyShownInSave=*/true));
	TestTrue(TEXT("Ноутбук сдан учёному — сообщение планируется"),
		Gate.ShouldSchedule(AQuestGiverNPC::IsStoryEndingQuestTurnedIn(Journal), false));
	TestFalse(TEXT("Второй раз за запуск не планируется"),
		Gate.ShouldSchedule(AQuestGiverNPC::IsStoryEndingQuestTurnedIn(Journal), false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
