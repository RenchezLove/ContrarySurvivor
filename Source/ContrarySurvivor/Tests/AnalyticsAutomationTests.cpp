// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты пункта Б4 задания издателя (ADR-059): ключи аналитики внутри
// сборки + события первого запуска, шага обучения и завершения обучения. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Analytics; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * определения компилятора с ключами реально доехали до кода (проверяются ТОЛЬКО длины —
//     значения ключей не печатаются и в тест не попадают);
//   * имена новых событий совпадают с таблицей соответствия в шапке AnalyticsSubsystem.h;
//   * защита от повторной отправки за одну установку игры работает.
//
// НЕ покрывается headless: фактическая отправка событий на сервер GameAnalytics — это сеть
// и живое устройство (см. отчёт: НЕ ПРОВЕРЕНО на живом устройстве).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Retention/OnboardingComponent.h"

static constexpr EAutomationTestFlags AnalyticsTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Ключи вшиты компилятором ------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsCompiledKeysTest,
	"ContrarySurvivor.Analytics.CompiledKeysReachCode", AnalyticsTestFlags)

bool FAnalyticsCompiledKeysTest::RunTest(const FString& Parameters)
{
	if (!UAnalyticsSubsystem::AreKeysCompiledIn())
	{
		// Копия без папки ключей собираться обязана (сборка на чужой машине) — это не ошибка,
		// но и доказательством вшитых ключей такой прогон не является.
		AddInfo(TEXT("Ключи в этот двоичный файл не вшивались (CONTRARY_GA_KEYS_COMPILED_IN=0): "
			"сборка сделана без папки ключей, аналитика будет искать файлы в рантайме."));
		return true;
	}

	const int32 GameKeyLen = UAnalyticsSubsystem::GetCompiledGameKeyLength();
	const int32 SecretKeyLen = UAnalyticsSubsystem::GetCompiledSecretKeyLength();
	AddInfo(FString::Printf(
		TEXT("Ключи вшиты в сборку: длина игрового ключа %d, длина секретного ключа %d (значения не печатаются)."),
		GameKeyLen, SecretKeyLen));

	TestTrue(TEXT("Игровой ключ вшит и не пуст"), GameKeyLen > 0);
	TestTrue(TEXT("Секретный ключ вшит и не пуст"), SecretKeyLen > 0);
	// Формат GameAnalytics: игровой ключ 32 символа, секретный 40.
	TestEqual(TEXT("Длина игрового ключа соответствует формату GameAnalytics"), GameKeyLen, 32);
	TestEqual(TEXT("Длина секретного ключа соответствует формату GameAnalytics"), SecretKeyLen, 40);
	return true;
}

// --- 2. Имена новых событий ------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsNewEventIdsTest,
	"ContrarySurvivor.Analytics.NewEventIdsMatchTable", AnalyticsTestFlags)

bool FAnalyticsNewEventIdsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Первый запуск"), UAnalyticsSubsystem::MakeFirstLaunchEventId(),
		FString(TEXT("app:first_launch")));
	TestEqual(TEXT("Завершение обучения"), UAnalyticsSubsystem::MakeTutorialCompletedEventId(),
		FString(TEXT("tutorial:completed")));

	// Имя шага подставляется последним сегментом пятиуровневого имени GameAnalytics.
	TestEqual(TEXT("Шаг обучения «движение»"),
		UAnalyticsSubsystem::MakeTutorialStepEventId(TEXT("movement")),
		FString(TEXT("tutorial:step:movement")));
	TestEqual(TEXT("Шаг обучения «смерть»"),
		UAnalyticsSubsystem::MakeTutorialStepEventId(TEXT("death")),
		FString(TEXT("tutorial:step:death")));

	// GameAnalytics не принимает в именах событий ничего, кроме латиницы/цифр/нескольких
	// знаков — недопустимые символы заменяются, имя события остаётся валидным.
	const FString DirtyEventId = UAnalyticsSubsystem::MakeTutorialStepEventId(TEXT("шаг один"));
	TestTrue(TEXT("Имя события шага всегда начинается с «tutorial:step:»"),
		DirtyEventId.StartsWith(TEXT("tutorial:step:")));
	bool bOnlyAllowedChars = true;
	for (const TCHAR C : DirtyEventId)
	{
		const bool bAllowed = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z')
			|| (C >= '0' && C <= '9') || C == '_' || C == '-' || C == '.' || C == ':';
		bOnlyAllowedChars = bOnlyAllowedChars && bAllowed;
	}
	TestTrue(TEXT("Недопустимые для GameAnalytics символы в имени шага заменены"), bOnlyAllowedChars);
	return true;
}

// --- 3. Имена шагов обучения берутся из реальных подсказок -----------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsTutorialStepNamesTest,
	"ContrarySurvivor.Analytics.TutorialStepNamesAreStable", AnalyticsTestFlags)

bool FAnalyticsTutorialStepNamesTest::RunTest(const FString& Parameters)
{
	const int32 StepCount = static_cast<int32>(EOnboardingHint::Count);
	// ADR-074 (16.08): шестая подсказка — выход из деревни (leave_village).
	TestEqual(TEXT("Шагов обучения в игре ровно шесть (подсказки UOnboardingComponent)"), StepCount, 6);

	TSet<FString> SeenNames;
	for (int32 i = 0; i < StepCount; ++i)
	{
		const FString Name = UOnboardingComponent::GetHintAnalyticsId(static_cast<EOnboardingHint>(i));
		TestFalse(FString::Printf(TEXT("Имя шага %d не пустое"), i + 1), Name.IsEmpty());
		TestNotEqual(FString::Printf(TEXT("Имя шага %d определено, а не заглушка"), i + 1),
			Name, FString(TEXT("unknown")));
		TestFalse(FString::Printf(TEXT("Имя шага %d не повторяет предыдущие"), i + 1),
			SeenNames.Contains(Name));
		SeenNames.Add(Name);

		// Имя обязано пережить санитайзер без изменений, иначе событие приедет издателю
		// не под тем именем, что записано в таблице соответствия.
		TestEqual(FString::Printf(TEXT("Имя шага %d пригодно для GameAnalytics без замен"), i + 1),
			UAnalyticsSubsystem::SanitizeEventPart(Name), Name);
	}

	TestEqual(TEXT("Первый шаг — подсказка движения"),
		FString(UOnboardingComponent::GetHintAnalyticsId(EOnboardingHint::Movement)),
		FString(TEXT("movement")));
	return true;
}

// --- 3а. Правило «обучение пройдено» (решение лида 08-06; ADR-074 16.08) ----------------
// Завершение обучения считается по всем шагам, КРОМЕ подсказки смерти: иначе метрика
// издателя «доля прошедших обучение» требовала бы первой гибели и меряла смертность.
// Событие самого шага у подсказки смерти остаётся — правило касается только завершения.
// ADR-074: подсказка выхода из деревни (leave_village) тоже НЕ входит — условие завершения
// обязано остаться теми же четырьмя шагами, что в выпущенной 0.1.0 (1), иначе показатель
// издателя перестанет быть сравнимым между версиями.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsTutorialCompletionRuleTest,
	"ContrarySurvivor.Analytics.TutorialCompletionSkipsDeathHint", AnalyticsTestFlags)

bool FAnalyticsTutorialCompletionRuleTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Подсказка смерти в условие завершения НЕ входит"),
		UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint::Death));
	TestFalse(TEXT("Подсказка выхода из деревни в условие завершения НЕ входит (ADR-074)"),
		UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint::LeaveVillage));

	int32 Counted = 0;
	for (int32 i = 0; i < static_cast<int32>(EOnboardingHint::Count); ++i)
	{
		const EOnboardingHint Hint = static_cast<EOnboardingHint>(i);
		if (Hint != EOnboardingHint::Death && Hint != EOnboardingHint::LeaveVillage)
		{
			TestTrue(FString::Printf(TEXT("Шаг %d входит в условие завершения"), i + 1),
				UOnboardingComponent::CountsTowardTutorialCompletion(Hint));
		}
		Counted += UOnboardingComponent::CountsTowardTutorialCompletion(Hint) ? 1 : 0;
	}

	// Число шагов в событии «обучение пройдено» считается тем же правилом, а не руками.
	TestEqual(TEXT("Число шагов завершения = все шаги минус подсказки смерти и выхода из деревни"),
		UOnboardingComponent::GetTutorialCompletionStepCount(), Counted);
	// Число пришпилено НАМЕРЕННО (ADR-074): четыре шага — как в выпущенной 0.1.0 (1). Новая
	// подсказка это число менять не вправе; кто добавит седьмую и захочет её засчитывать —
	// сначала решение с издателем, потом правка этой строки.
	TestEqual(TEXT("Условие завершения — ровно четыре шага, как в 0.1.0 (1)"),
		UOnboardingComponent::GetTutorialCompletionStepCount(), 4);
	return true;
}

// --- 4. Защита от повторной отправки за установку --------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsProfileSaveDedupeTest,
	"ContrarySurvivor.Analytics.ProfileSaveReportsEventsOnce", AnalyticsTestFlags)

bool FAnalyticsProfileSaveDedupeTest::RunTest(const FString& Parameters)
{
	// Служебная память проверяется объектом в оперативной памяти — слот на диске
	// (ни боевой 'ContrarySave', ни 'ContraryAnalytics') тест не трогает.
	UAnalyticsProfileSave* Save = NewObject<UAnalyticsProfileSave>();
	if (!TestNotNull(TEXT("Объект служебной памяти аналитики создан"), Save))
	{
		return false;
	}

	TestTrue(TEXT("Первый запуск отмечается один раз"), Save->MarkFirstLaunchReported());
	TestFalse(TEXT("Повторный запуск события первого запуска не даёт"), Save->MarkFirstLaunchReported());

	TestTrue(TEXT("Шаг «движение» отмечается"), Save->MarkTutorialStepReported(TEXT("movement")));
	TestFalse(TEXT("Тот же шаг второй раз не отправляется"), Save->MarkTutorialStepReported(TEXT("movement")));
	TestTrue(TEXT("Другой шаг отмечается отдельно"), Save->MarkTutorialStepReported(TEXT("pickup")));
	TestEqual(TEXT("Отмечено ровно два разных шага"), Save->GetReportedTutorialStepCount(), 2);
	TestFalse(TEXT("Пустое имя шага не отмечается"), Save->MarkTutorialStepReported(FString()));

	TestTrue(TEXT("Завершение обучения отмечается один раз"), Save->MarkTutorialCompletedReported());
	TestFalse(TEXT("Повторное завершение обучения не отправляется"), Save->MarkTutorialCompletedReported());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
