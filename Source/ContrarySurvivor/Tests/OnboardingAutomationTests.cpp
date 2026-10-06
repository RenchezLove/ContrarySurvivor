// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты разгрузки надписей начала игры (список Рината 07.10.2026, ветка
// feature/first-fight-1006). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Onboarding; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держим:
//   1) очередь подсказок: одна на экране, следующая ждёт и не накладывается; ввод не
//      пролистывает очередь мгновенно;
//   2) на старте — только про ходьбу; про удар и смену оружия — отдельные подсказки и их условия;
//   3) развёрнутая подсказка хромоты ждёт подарка старосты;
//   4) надпись о подарке — текст Рината дословно;
//   5) подсказка старосты и первое открытие сумки — шаги без текста, текст брони — отдельно;
//   6) окно ежедневной награды в первый заход переносится на следующий запуск, и в следующий
//      запуск награда первого дня выдаётся;
//   7) события обучения: прежние имена и четыре шага завершения (подробно —
//      AnalyticsAutomationTests.cpp).
//
// Показ самого виджета без окна и контроллера не проверить — тесты держат решения «что и
// когда показывать» (чистая логика) и тексты объектов-образцов классов.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Retention/DailyRewardLogic.h"
#include "ContrarySurvivor/Retention/OnboardingComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/Subsystems/GameFlowSubsystem.h"
#include "ContrarySurvivor/UI/LimpIndicatorWidget.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags OnboardingTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// ===========================================================================
// 1. «Одна подсказка за раз. Следующая ждёт, пока предыдущая уйдёт, и не накладывается.»
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnboardingHintQueueTest,
	"ContrarySurvivor.Onboarding.HintQueueShowsOneAtATime", OnboardingTestFlags)
bool FOnboardingHintQueueTest::RunTest(const FString& Parameters)
{
	const FText A = FText::FromString(TEXT("Первая"));
	const FText B = FText::FromString(TEXT("Вторая"));
	const FText C = FText::FromString(TEXT("Третья"));

	FOnboardingHintQueue Queue;
	TestTrue(TEXT("Экран свободен — первая надпись показывается сразу"),
		Queue.Offer(A, 0.0) == EOnboardingHintOffer::ShowNow);
	TestTrue(TEXT("Первая надпись на экране"), Queue.IsShowing() && Queue.GetCurrent().EqualTo(A));

	// Вторая и третья не затирают первую, а ждут.
	TestTrue(TEXT("Вторая надпись встаёт в очередь"), Queue.Offer(B, 1.0) == EOnboardingHintOffer::Queued);
	TestTrue(TEXT("Третья надпись встаёт в очередь"), Queue.Offer(C, 1.5) == EOnboardingHintOffer::Queued);
	TestTrue(TEXT("На экране по-прежнему первая"), Queue.GetCurrent().EqualTo(A));
	TestEqual(TEXT("В очереди две надписи"), Queue.NumPending(), 2);

	// Повторы не копятся: та же надпись, что на экране, только продлевается; та же, что в
	// очереди, второй раз не встаёт.
	TestTrue(TEXT("Надпись, которая уже на экране, только продлевается"),
		Queue.Offer(A, 2.0) == EOnboardingHintOffer::RefreshCurrent);
	TestTrue(TEXT("Надпись, которая уже в очереди, не дублируется"),
		Queue.Offer(B, 2.0) == EOnboardingHintOffer::Queued);
	TestEqual(TEXT("В очереди по-прежнему две надписи"), Queue.NumPending(), 2);

	// Ввод игрока не пролистывает очередь мгновенно: первые секунды надпись погасить нельзя.
	const float MinShow = 2.0f;
	TestFalse(TEXT("Сразу после появления ввод надпись не гасит"), Queue.MayDismissByInput(0.5, MinShow));
	TestTrue(TEXT("Провисела свой минимум — ввод гасит"), Queue.MayDismissByInput(2.5, MinShow));

	// Первая ушла: начинается пауза, следующая не появляется встык.
	Queue.MarkHidden();
	TestFalse(TEXT("После ухода первой на экране пусто"), Queue.IsShowing());
	TestTrue(TEXT("Идёт пауза перед следующей"), Queue.IsWaitingGap());
	TestFalse(TEXT("Во время паузы гасить нечего"), Queue.MayDismissByInput(100.0, MinShow));

	// Надпись, пришедшая во время паузы, не лезет вперёд очереди.
	const FText D = FText::FromString(TEXT("Четвёртая"));
	TestTrue(TEXT("Надпись во время паузы встаёт в конец очереди"),
		Queue.Offer(D, 7.2) == EOnboardingHintOffer::Queued);

	FText Next;
	TestTrue(TEXT("После паузы показывается следующая"), Queue.TakeNext(8.0, Next));
	TestTrue(TEXT("Следующая — вторая, по порядку"), Next.EqualTo(B) && Queue.GetCurrent().EqualTo(B));
	TestFalse(TEXT("Только что показанную вторую ввод не гасит (отсчёт заново)"),
		Queue.MayDismissByInput(8.5, MinShow));
	TestFalse(TEXT("Пока вторая на экране, третью взять нельзя"), Queue.TakeNext(8.6, Next));

	Queue.MarkHidden();
	TestTrue(TEXT("Третья по порядку"), Queue.TakeNext(16.0, Next) && Next.EqualTo(C));
	Queue.MarkHidden();
	TestTrue(TEXT("Четвёртая по порядку"), Queue.TakeNext(24.0, Next) && Next.EqualTo(D));

	// Очередь опустела: после ухода последней паузы нет, новая надпись показывается сразу.
	Queue.MarkHidden();
	TestFalse(TEXT("Очередь пуста — паузы нет"), Queue.IsWaitingGap());
	TestFalse(TEXT("Очередь пуста — показывать нечего"), Queue.TakeNext(30.0, Next));
	TestTrue(TEXT("Новая надпись на свободный экран — сразу"),
		Queue.Offer(A, 31.0) == EOnboardingHintOffer::ShowNow);

	// Настройки очереди существуют и осмысленны (сами числа крутит Ринат).
	const UOnboardingComponent* Defaults = GetDefault<UOnboardingComponent>();
	if (TestNotNull(TEXT("Образец компонента подсказок доступен"), Defaults))
	{
		TestNotNull(TEXT("Настройка HintGapSeconds есть"),
			FindFProperty<FFloatProperty>(UOnboardingComponent::StaticClass(), TEXT("HintGapSeconds")));
		TestTrue(TEXT("Пауза между подсказками больше нуля"), Defaults->HintGapSeconds > 0.0f);
		TestTrue(TEXT("Минимум показа до гашения вводом больше нуля"), Defaults->MinShowBeforeInputDismiss > 0.0f);
		TestTrue(TEXT("Минимум показа короче полного времени показа"),
			Defaults->MinShowBeforeInputDismiss < Defaults->HintDuration);
	}
	return true;
}

// ===========================================================================
// 2. Стартовая подсказка — только про ходьбу; удар и смена оружия — отдельно, по условию.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnboardingStartHintOnlyMovementTest,
	"ContrarySurvivor.Onboarding.StartHintIsOnlyAboutMovement", OnboardingTestFlags)
bool FOnboardingStartHintOnlyMovementTest::RunTest(const FString& Parameters)
{
	const UOnboardingComponent* Hints = GetDefault<UOnboardingComponent>();
	if (!TestNotNull(TEXT("Образец компонента подсказок доступен"), Hints))
	{
		return false;
	}

	// Тексты Рината дословно.
	TestEqual(TEXT("Старт, телефон"),
		Hints->GetHintText(EOnboardingHint::Movement, /*bTouch=*/true).ToString(),
		FString(TEXT("Веди пальцем по левой части экрана, чтобы идти")));
	TestEqual(TEXT("Старт, ПК"),
		Hints->GetHintText(EOnboardingHint::Movement, /*bTouch=*/false).ToString(),
		FString(TEXT("Ходи на W, A, S, D")));
	TestEqual(TEXT("Удар, телефон"),
		Hints->GetHintText(EOnboardingHint::Attack, true).ToString(),
		FString(TEXT("Нажми на врага, чтобы ударить")));
	TestEqual(TEXT("Удар, ПК"),
		Hints->GetHintText(EOnboardingHint::Attack, false).ToString(),
		FString(TEXT("Нажми на врага, чтобы ударить")));
	TestEqual(TEXT("Смена оружия, телефон"),
		Hints->GetHintText(EOnboardingHint::WeaponSwitch, true).ToString(),
		FString(TEXT("Кнопка ОРУЖИЕ — сменить оружие")));
	TestEqual(TEXT("Смена оружия, ПК"),
		Hints->GetHintText(EOnboardingHint::WeaponSwitch, false).ToString(),
		FString(TEXT("Q — сменить оружие")));

	// Когда показывать.
	const float Trigger = Hints->AttackHintDistance;
	TestTrue(TEXT("Расстояние подсказки про удар больше нуля"), Trigger > 0.0f);
	TestTrue(TEXT("Враг рядом — пора говорить про удар"),
		UOnboardingComponent::ShouldShowAttackHint(Trigger * 0.5f, Trigger));
	TestFalse(TEXT("Враг далеко — про удар молчим"),
		UOnboardingComponent::ShouldShowAttackHint(Trigger * 2.0f, Trigger));

	TestFalse(TEXT("Только нож — про смену оружия молчим"),
		UOnboardingComponent::ShouldShowWeaponSwitchHint(/*bHasMelee=*/true, /*bHasRanged=*/false));
	TestFalse(TEXT("Только огнестрел — про смену оружия молчим"),
		UOnboardingComponent::ShouldShowWeaponSwitchHint(false, true));
	TestTrue(TEXT("Появилось второе оружие — пора говорить про смену"),
		UOnboardingComponent::ShouldShowWeaponSwitchHint(true, true));

	// Новые подсказки одноразовые: у каждой свой признак в сохранении, и он переживает
	// автосохранение у костра (перенос CopyRetentionData).
	UContrarySaveGame* From = NewObject<UContrarySaveGame>(GetTransientPackage());
	UContrarySaveGame* To = NewObject<UContrarySaveGame>(GetTransientPackage());
	if (TestNotNull(TEXT("Сохранение-источник создано"), From) && TestNotNull(TEXT("Сохранение-приёмник создано"), To))
	{
		TestFalse(TEXT("В новом профиле подсказка про удар не показана"), From->bHintAttackShown);
		TestFalse(TEXT("В новом профиле подсказка про смену оружия не показана"), From->bHintWeaponSwitchShown);
		TestFalse(TEXT("В новом профиле подсказка про броню не показана"), From->bHintArmorShown);

		From->bHintAttackShown = true;
		From->bHintWeaponSwitchShown = true;
		From->bHintArmorShown = true;
		UContrarySaveGame::CopyRetentionData(From, To);
		TestTrue(TEXT("Признак подсказки про удар переживает автосохранение"), To->bHintAttackShown);
		TestTrue(TEXT("Признак подсказки про смену оружия переживает автосохранение"), To->bHintWeaponSwitchShown);
		TestTrue(TEXT("Признак подсказки про броню переживает автосохранение"), To->bHintArmorShown);
	}
	return true;
}

// ===========================================================================
// 3 и 4. Хромота: на старте только короткая строка; объяснение — в надписи о подарке.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnboardingLimpHintWaitsForGiftTest,
	"ContrarySurvivor.Onboarding.LimpExplanationMovesToGiftNotice", OnboardingTestFlags)
bool FOnboardingLimpHintWaitsForGiftTest::RunTest(const FString& Parameters)
{
	// До подарка старосты развёрнутая подсказка не запускается, сколько бы игрок ни хромал.
	FLimpFirstHintState State;
	TestFalse(TEXT("До подарка: хромает, управление свободно — развёрнутой подсказки нет"),
		State.ShouldTrigger(/*bLimping=*/true,
			FLimpFirstHintState::IsExpandedHintAllowed(/*bControlFree=*/true, /*bElderGiftGiven=*/false)));
	TestFalse(TEXT("До подарка подсказка не потрачена"), State.bConsumed);

	// После подарка — прежнее поведение: один раз при хромоте со свободным управлением.
	TestFalse(TEXT("После подарка, но управление занято — ждём"),
		State.ShouldTrigger(true, FLimpFirstHintState::IsExpandedHintAllowed(false, true)));
	TestTrue(TEXT("После подарка: хромает, управление свободно — показ"),
		State.ShouldTrigger(true, FLimpFirstHintState::IsExpandedHintAllowed(true, true)));
	TestFalse(TEXT("Повторно не показывается"),
		State.ShouldTrigger(true, FLimpFirstHintState::IsExpandedHintAllowed(true, true)));

	// Короткая строка — та самая, что остаётся на старте.
	const ULimpIndicatorWidget* Limp = GetDefault<ULimpIndicatorWidget>();
	if (TestNotNull(TEXT("Образец индикатора хромоты доступен"), Limp))
	{
		TestEqual(TEXT("Короткая строка о ранении"),
			Limp->WoundedText.ToString(), FString(TEXT("Ранен: скорость снижена")));
	}

	// Надпись о подарке — текст Рината дословно (поле защищённое, читаем через рефлексию).
	const FTextProperty* GiftProp = FindFProperty<FTextProperty>(AElderNPC::StaticClass(), TEXT("FirstGiftHintFormat"));
	if (TestNotNull(TEXT("Настройка FirstGiftHintFormat есть у старосты"), GiftProp))
	{
		TestEqual(TEXT("Надпись о подарке"),
			GiftProp->GetPropertyValue_InContainer(GetDefault<AElderNPC>()).ToString(),
			FString(TEXT("Получена аптечка — она в сумке. Подлечись, и хромота пройдёт")));
	}
	return true;
}

// ===========================================================================
// 5 и 6. Подсказка старосты и первое открытие сумки — без текста; текст брони — отдельный шаг.
//        События обучения при этом прежние.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnboardingSilentStepsTest,
	"ContrarySurvivor.Onboarding.ElderAndBagStepsAreSilent", OnboardingTestFlags)
bool FOnboardingSilentStepsTest::RunTest(const FString& Parameters)
{
	const UOnboardingComponent* Hints = GetDefault<UOnboardingComponent>();
	if (!TestNotNull(TEXT("Образец компонента подсказок доступен"), Hints))
	{
		return false;
	}

	for (const bool bTouch : { false, true })
	{
		TestTrue(TEXT("Подсказка «Поговори со старостой» на экран не выводится"),
			Hints->GetHintText(EOnboardingHint::Elder, bTouch).IsEmpty());
		TestTrue(TEXT("Первое открытие сумки текста не показывает"),
			Hints->GetHintText(EOnboardingHint::Inventory, bTouch).IsEmpty());
		TestTrue(TEXT("Текст про броню — у отдельной подсказки"),
			Hints->GetHintText(EOnboardingHint::Armor, bTouch).ToString().Contains(TEXT("Брон"))
			|| Hints->GetHintText(EOnboardingHint::Armor, bTouch).ToString().Contains(TEXT("брон")));
		// Остальные подсказки на месте.
		TestFalse(TEXT("Подсказка подбора на месте"), Hints->GetHintText(EOnboardingHint::Pickup, bTouch).IsEmpty());
		TestFalse(TEXT("Подсказка смерти на месте"), Hints->GetHintText(EOnboardingHint::Death, bTouch).IsEmpty());
		// Напоминание про костёр при выходе из деревни — без изменений.
		TestEqual(TEXT("Напоминание про костёр прежнее"),
			Hints->GetHintText(EOnboardingHint::LeaveVillage, bTouch).ToString(),
			FString(TEXT("Уходишь из деревни. Не забывай: прогресс сохраняется только у костра")));
	}

	// Шаги elder и inventory остаются шагами обучения с прежними именами и по-прежнему входят
	// в «обучение пройдено»; новые подсказки в него не входят — шагов четыре.
	TestEqual(TEXT("Имя шага старосты прежнее"),
		FString(UOnboardingComponent::GetHintAnalyticsId(EOnboardingHint::Elder)), FString(TEXT("elder")));
	TestEqual(TEXT("Имя шага сумки прежнее"),
		FString(UOnboardingComponent::GetHintAnalyticsId(EOnboardingHint::Inventory)), FString(TEXT("inventory")));
	TestTrue(TEXT("Шаг старосты входит в «обучение пройдено»"),
		UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint::Elder));
	TestTrue(TEXT("Шаг сумки входит в «обучение пройдено»"),
		UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint::Inventory));
	TestFalse(TEXT("Подсказка про удар в «обучение пройдено» не входит"),
		UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint::Attack));
	TestFalse(TEXT("Подсказка про смену оружия в «обучение пройдено» не входит"),
		UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint::WeaponSwitch));
	TestFalse(TEXT("Подсказка про броню в «обучение пройдено» не входит"),
		UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint::Armor));
	TestEqual(TEXT("«Обучение пройдено» — по-прежнему четыре шага"),
		UOnboardingComponent::GetTutorialCompletionStepCount(), 4);
	return true;
}

// ===========================================================================
// 7. Ежедневная награда: в первый заход окно переносится на следующий запуск игры, и в
//    следующий запуск награда первого дня выдаётся (начисления без окна нет: при переносе
//    не делается ничего).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnboardingDailyRewardPostponedTest,
	"ContrarySurvivor.Onboarding.DailyRewardMovesToNextLaunch", OnboardingTestFlags)
bool FOnboardingDailyRewardPostponedTest::RunTest(const FString& Parameters)
{
	using DailyReward::EStartDecision;

	// Первый заход: первая встреча со старостой ещё впереди — в этом запуске ничего.
	TestTrue(TEXT("Первый заход — перенос на следующий запуск"),
		DailyReward::DecideOnLevelStart(/*bIntroStillAhead=*/true, /*bPostponedThisRun=*/false,
			/*bPostponeEnabled=*/true) == EStartDecision::PostponeToNextLaunch);

	// Тот же запуск, уровень перезагрузился уже после диалога — всё ещё ничего.
	TestTrue(TEXT("Тот же запуск после диалога — окно не возвращается"),
		DailyReward::DecideOnLevelStart(false, true, true) == EStartDecision::PostponeToNextLaunch);

	// Следующий запуск: встреча позади, переноса в этом запуске не было — обычная проверка.
	TestTrue(TEXT("Следующий запуск — обычная проверка награды"),
		DailyReward::DecideOnLevelStart(false, false, true) == EStartDecision::Evaluate);

	// Настройка выключена — прежнее поведение (окно после диалога старосты).
	TestTrue(TEXT("Настройка выключена, встреча впереди — окно после диалога, как раньше"),
		DailyReward::DecideOnLevelStart(true, false, false) == EStartDecision::DeferUntilElderDialog);
	TestTrue(TEXT("Настройка выключена, встреча позади — обычная проверка"),
		DailyReward::DecideOnLevelStart(false, true, false) == EStartDecision::Evaluate);

	// При переносе день в сохранении не помечается (дата пустая, серия 0), поэтому в следующий
	// запуск обычная проверка выдаёт награду ПЕРВОГО дня — вместе с окном, одним шагом.
	const UContrarySaveGame* FreshSave = NewObject<UContrarySaveGame>(GetTransientPackage());
	if (TestNotNull(TEXT("Свежее сохранение создано"), FreshSave))
	{
		TestEqual(TEXT("В свежем профиле даты входа нет"), FreshSave->LastDailyRewardDate.GetTicks(), (int64)0);
		const DailyReward::FComputeResult Next = DailyReward::Compute(FDateTime(2026, 10, 8, 12, 0, 0),
			FreshSave->LastDailyRewardDate, FreshSave->DailyStreakDays, 25.0f, 10.0f, 75.0f);
		TestTrue(TEXT("В следующий запуск награда выдаётся"), Next.bGrant);
		TestEqual(TEXT("Это первый день серии"), Next.NewStreak, 1);
		TestEqual(TEXT("Сумма первого дня"), Next.Reward, 25.0f);
	}

	// Признак переноса живёт на подсистеме экземпляра игры и по умолчанию снят.
	const UGameFlowSubsystem* FlowDefaults = GetDefault<UGameFlowSubsystem>();
	if (TestNotNull(TEXT("Образец подсистемы переходов доступен"), FlowDefaults))
	{
		TestFalse(TEXT("В новом запуске признак переноса снят"), FlowDefaults->IsDailyRewardPostponedThisRun());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
