// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты ТРЁХ ДАТЧИКОВ замера (сводное ТЗ издателя 13.08, задача 3).
// Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.AnalyticsSensors; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * имена событий совпадают с таблицей соответствия в шапке AnalyticsSubsystem.h:
//     quest:offered:{QuestId}, support:ad_offered, support:ad_not_shown:no_ad,
//     support:ad_dismissed;
//   * «квест предложен» уходит РОВНО ОДИН РАЗ на квест — даже когда OfferQuest зовут
//     повторно (в игре его зовёт каждый кадр отрисовки диалога) и даже когда журнал
//     очистили загрузкой сохранения;
//   * событие открытия окна поддержки выбирается ТЕМ ЖЕ условием, что и видимость кнопки
//     «Посмотреть рекламу»: кнопка показана — «ролик предложен», нет — «нечего показывать»;
//   * отказ игрока (support:ad_dismissed) не пересекается с семейством ad:*:failed, которым
//     рекламная служба сообщает о ТЕХНИЧЕСКОМ сбое.
//
// ⛔ НЕ ПРОВЕРЯЕТСЯ И НЕ ПРОВЕРЯЛОСЬ: фактическая отправка событий в GameAnalytics и их
// приход в кабинет — это сеть и живое устройство. Правило Рината на эту сессию: датчики
// только расставить, работоспособность в игре не проверять.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/UI/SupportAuthorWidget.h"

static constexpr EAutomationTestFlags AnalyticsSensorsTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Имена трёх новых событий -------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsSensorNamesTest,
	"ContrarySurvivor.AnalyticsSensors.EventNamesMatchTheBrief", AnalyticsSensorsTestFlags)

bool FAnalyticsSensorNamesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Квест предложен"),
		UAnalyticsSubsystem::MakeQuestOfferedEventId(FName(TEXT("KillWolves"))),
		FString(TEXT("quest:offered:KillWolves")));

	// Идентификатор квеста едет сегментом имени и чистится тем же правилом, что остальные
	// сегменты: GA принимает только латиницу, цифры и немного знаков.
	TestEqual(TEXT("Недопустимые знаки в идентификаторе квеста заменяются"),
		UAnalyticsSubsystem::MakeQuestOfferedEventId(FName(TEXT("Квест 1"))),
		FString(TEXT("quest:offered:______1")));

	TestEqual(TEXT("Ролик предложен"),
		UAnalyticsSubsystem::MakeSupportAdOfferedEventId(), FString(TEXT("support:ad_offered")));
	TestEqual(TEXT("Ролика нет — причина сегментом"),
		UAnalyticsSubsystem::MakeSupportAdNotShownEventId(UAnalyticsSubsystem::SupportAdReasonNoAd()),
		FString(TEXT("support:ad_not_shown:no_ad")));
	TestEqual(TEXT("Игрок закрыл ролик"),
		UAnalyticsSubsystem::MakeSupportAdDismissedEventId(), FString(TEXT("support:ad_dismissed")));

	return true;
}

// --- 2. «Квест предложен» — ровно один раз на квест ------------------------------------
//
// Ловушка, ради которой существует сторож: UQuestComponent::OfferQuest зовётся КАЖДЫЙ КАДР
// отрисовки диалога со старостой (ContrarySurvivorHUD.cpp), а журнал можно очистить
// загрузкой сохранения. Повторная отправка портит проценты воронки сильнее, чем пропуск.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsSensorQuestOfferedOnceTest,
	"ContrarySurvivor.AnalyticsSensors.QuestOfferedSentOncePerQuest", AnalyticsSensorsTestFlags)

bool FAnalyticsSensorQuestOfferedOnceTest::RunTest(const FString& Parameters)
{
	UQuestComponent* Journal = NewObject<UQuestComponent>(GetTransientPackage());
	if (!TestNotNull(TEXT("Журнал квестов создан"), Journal))
	{
		return false;
	}

	// Сам сторож: первый раз — да, дальше — нет.
	TestTrue(TEXT("Первое предложение квеста отмечается"),
		Journal->MarkQuestOfferedForAnalytics(FName(TEXT("KillWolves"))));
	TestFalse(TEXT("Повторное предложение того же квеста не отмечается"),
		Journal->MarkQuestOfferedForAnalytics(FName(TEXT("KillWolves"))));
	TestFalse(TEXT("Пустой идентификатор не отмечается"),
		Journal->MarkQuestOfferedForAnalytics(NAME_None));
	TestEqual(TEXT("Отмечен ровно один квест"), Journal->GetOfferedAnalyticsCount(), 1);

	// Живой путь: тот же квест предлагается много раз подряд (так и делает диалог).
	UQuestComponent* Live = NewObject<UQuestComponent>(GetTransientPackage());
	if (!TestNotNull(TEXT("Второй журнал создан"), Live))
	{
		return false;
	}

	FQuest Pelts;
	Pelts.QuestId = FName(TEXT("BringPelts"));
	Pelts.RequiredItemName = TEXT("Шкура волка");
	Pelts.RequiredItemCount = 3;

	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		Live->OfferQuest(Pelts);
	}
	TestEqual(TEXT("Десять кадров диалога дали одну отметку"), Live->GetOfferedAnalyticsCount(), 1);
	TestEqual(TEXT("В журнале один квест"), Live->GetQuests().Num(), 1);

	// Журнал очистили (загрузка сохранения / новая игра) и тот же квест предложили снова:
	// в журнал он вернётся, а вот событие второй раз уйти НЕ должно.
	Live->RestoreQuests(TArray<FQuest>());
	TestEqual(TEXT("Журнал очищен"), Live->GetQuests().Num(), 0);
	Live->OfferQuest(Pelts);
	TestEqual(TEXT("Квест снова в журнале"), Live->GetQuests().Num(), 1);
	TestEqual(TEXT("Отметка осталась одна — второго события нет"), Live->GetOfferedAnalyticsCount(), 1);

	// Другой квест — своя отметка (сторож не глушит всю воронку целиком).
	FQuest Second;
	Second.QuestId = FName(TEXT("FindLaptop"));
	Live->OfferQuest(Second);
	TestEqual(TEXT("Второй квест отмечен отдельно"), Live->GetOfferedAnalyticsCount(), 2);

	return true;
}

// --- 3. «Ролик предложен» решается тем же условием, что и видимость кнопки --------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsSensorSupportAdOfferTest,
	"ContrarySurvivor.AnalyticsSensors.SupportAdOfferFollowsButton", AnalyticsSensorsTestFlags)

bool FAnalyticsSensorSupportAdOfferTest::RunTest(const FString& Parameters)
{
	// Ролик готов: кнопка показана — и событие говорит «предложен».
	TestTrue(TEXT("Готовый ролик показывает кнопку"),
		USupportAuthorWidget::ShouldShowWatchAdButton(/*bAdReady=*/true));
	TestEqual(TEXT("Готовый ролик — событие «предложен»"),
		UAnalyticsSubsystem::MakeSupportAdOfferEventId(/*bAdReady=*/true),
		UAnalyticsSubsystem::MakeSupportAdOfferedEventId());

	// Ролика нет: кнопки нет — и событие говорит «показывать было нечего».
	TestFalse(TEXT("Без ролика кнопки нет"),
		USupportAuthorWidget::ShouldShowWatchAdButton(/*bAdReady=*/false));
	TestEqual(TEXT("Без ролика — событие «нечего показывать»"),
		UAnalyticsSubsystem::MakeSupportAdOfferEventId(/*bAdReady=*/false),
		UAnalyticsSubsystem::MakeSupportAdNotShownEventId(UAnalyticsSubsystem::SupportAdReasonNoAd()));

	// Два исхода не путаются между собой.
	TestNotEqual(TEXT("Исходы различаются"),
		UAnalyticsSubsystem::MakeSupportAdOfferEventId(true),
		UAnalyticsSubsystem::MakeSupportAdOfferEventId(false));

	return true;
}

// --- 4. Отказ игрока не смешивается с техническим сбоем --------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnalyticsSensorDismissedIsNotFailureTest,
	"ContrarySurvivor.AnalyticsSensors.DismissedIsNotTechnicalFailure", AnalyticsSensorsTestFlags)

bool FAnalyticsSensorDismissedIsNotFailureTest::RunTest(const FString& Parameters)
{
	const FString Dismissed = UAnalyticsSubsystem::MakeSupportAdDismissedEventId();

	// Семейство ad:*:failed принадлежит рекламной службе (UYandexAdService шлёт его при
	// технической ошибке показа) — наш датчик отказа обязан жить отдельно и в это семейство
	// не попадать, иначе издатель посчитает сбои сети как отказы игроков.
	TestFalse(TEXT("Отказ игрока не из семейства рекламной службы"), Dismissed.StartsWith(TEXT("ad:")));
	TestFalse(TEXT("В имени отказа нет слова про сбой"), Dismissed.Contains(TEXT("failed")));
	TestTrue(TEXT("Отказ живёт в воронке окна поддержки"), Dismissed.StartsWith(TEXT("support:")));

	// И не совпадает с событием досмотра — иначе досмотры и отказы слились бы в одно число.
	TestNotEqual(TEXT("Отказ и досмотр — разные события"),
		Dismissed, UAnalyticsSubsystem::MakeSupportAdCompletedEventId());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
