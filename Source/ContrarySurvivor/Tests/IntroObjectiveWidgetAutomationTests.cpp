// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты переезда строки задачи вступления с холста на живое окно
// (просьба Рината 08-09: «все интерфейсы должны переехать на WBP»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.IntroObjective; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * правило показа: задачи нет — строки нет вовсе; меню на экране — строка молчит;
//   * окно прячет СОДЕРЖИМОЕ, а не себя: оно наследует базу самоскрытия, поэтому не может
//     потерять ежекадровый вызов (та самая грабля, на которой сегодня пропали полосы
//     состояния — прямой Collapsed на самом окне останавливает его тик навсегда);
//   * обрамление текста — редактируемое поле, и подстановка задачи в нём есть (без неё
//     игрок не увидел бы саму задачу).
//
// НЕ покрывается headless: как строка выглядит на устройстве — смотрит Ринат живьём.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/IntroObjectiveWidget.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"

static constexpr EAutomationTestFlags IntroObjectiveTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Правило показа строки ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntroObjectiveVisibilityRuleTest,
	"ContrarySurvivor.IntroObjective.ShowsOnlyWithLiveObjective", IntroObjectiveTestFlags)

bool FIntroObjectiveVisibilityRuleTest::RunTest(const FString& Parameters)
{
	const FText Objective = NSLOCTEXT("IntroObjectiveTest", "Sample", "Впереди деревня. Дойти до неё.");

	TestTrue(TEXT("Есть задача и меню закрыто — строка показана"),
		UIntroObjectiveWidget::ShouldShowObjective(Objective, /*bMainMenuOnScreen=*/false));
	TestFalse(TEXT("Задачи нет — пустой плашки вверху экрана не появляется"),
		UIntroObjectiveWidget::ShouldShowObjective(FText::GetEmpty(), false));
	TestFalse(TEXT("Главное меню на экране — игровая строка молчит"),
		UIntroObjectiveWidget::ShouldShowObjective(Objective, /*bMainMenuOnScreen=*/true));
	TestFalse(TEXT("Ни задачи, ни повода — строки нет"),
		UIntroObjectiveWidget::ShouldShowObjective(FText::GetEmpty(), true));
	return true;
}

// --- 2. Окно не прячет само себя ------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntroObjectiveHidesContentNotItselfTest,
	"ContrarySurvivor.IntroObjective.HidesContentNotItself", IntroObjectiveTestFlags)

bool FIntroObjectiveHidesContentNotItselfTest::RunTest(const FString& Parameters)
{
	// Контракт против грабли 07-18 и её повтора 08-09: окно, которое прячется само,
	// обязано делать это через базу USelfHidingWidget (она прячет корень ДЕРЕВА).
	// Прямой Collapsed на самом окне остановил бы его ежекадровый вызов навсегда.
	TestTrue(TEXT("Строка задачи прячется через базу самоскрытия, а не собой"),
		UIntroObjectiveWidget::StaticClass()->IsChildOf(USelfHidingWidget::StaticClass()));
	return true;
}

// --- 3. Обрамление текста — рабочее поле ----------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIntroObjectiveFormatIsUsableTest,
	"ContrarySurvivor.IntroObjective.FormatKeepsObjective", IntroObjectiveTestFlags)

bool FIntroObjectiveFormatIsUsableTest::RunTest(const FString& Parameters)
{
	const UIntroObjectiveWidget* Defaults = GetDefault<UIntroObjectiveWidget>();
	if (!TestNotNull(TEXT("Настройки строки доступны"), Defaults))
	{
		return false;
	}

	// В обрамлении обязана быть подстановка задачи — иначе строка покажет что угодно,
	// кроме самой задачи.
	TestTrue(TEXT("В обрамлении есть подстановка задачи"),
		Defaults->ObjectiveFormat.ToString().Contains(TEXT("{Objective}")));

	// Склейка действительно подставляет задачу.
	FFormatNamedArguments Args;
	Args.Add(TEXT("Objective"), NSLOCTEXT("IntroObjectiveTest", "Find", "Найти старосту и поговорить."));
	const FString Built = FText::Format(Defaults->ObjectiveFormat, Args).ToString();
	TestTrue(FString::Printf(TEXT("Собранная строка содержит задачу: «%s»"), *Built),
		Built.Contains(TEXT("Найти старосту")));

	// Вид по умолчанию — читаемый: кегль не микроскопический, плашка не прозрачная.
	TestTrue(TEXT("Кегль строки задан крупно"), Defaults->ObjectiveFontSize >= 20);
	TestTrue(TEXT("Плашка под строкой не полностью прозрачна"), Defaults->PlateColor.A > 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
