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
#include "ContrarySurvivor/Ads/AdPointsLogic.h"
#include "ContrarySurvivor/Ads/AdService.h"
#include "ContrarySurvivor/Ads/DeathLossLogic.h"
#include "ContrarySurvivor/Ads/YandexAdService.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"

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

// Глобальный гейт игрового времени. ADR-063 п.3 (РИ-29): порог 300 с (5 мин, было 360 по
// В1); граница включительно; порог передаётся вторым аргументом (EditAnywhere на игроке),
// константа — только дефолт. Квестовый обход порога — Adr063AutomationTests (IsAdGatePassed).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdGatingPlaytimeTest,
	"ContrarySurvivor.Ads.Gating.PlaytimeGate5Min", AdsTestFlags)
bool FAdGatingPlaytimeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("дефолт порога = ровно 300 с (ADR-063, РИ-29)"), AdGating::MinPlaytimeSeconds, 300.0);
	TestFalse(TEXT("4:59 игрового времени: рекламы нет"), AdGating::IsPlaytimeGatePassed(299.0));
	TestTrue(TEXT("ровно 5:00: реклама разрешена"), AdGating::IsPlaytimeGatePassed(300.0));
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

// ===========================================================================
// Два новых формата (решение Рината 04.10.2026): полноэкранный показ без награды после
// сдачи задания про бандитов и маленький баннер. Правила точек (AdPointsLogic) гоняются на
// подставной рекламной службе, которая только считает вызовы. НЕ покрывается headless:
// сам вызов из контроллера при закрытии диалога и открытии экранов, запись признака в слот
// сохранения, настоящий показ на телефоне.
// ===========================================================================
namespace AdsTestLocal
{
	// Подставная рекламная служба: считает вызовы, ничего не показывает.
	struct FCountingAdService : public IAdService
	{
		bool bInterstitialReady = true;
		FDateTime LastRewardedUtc;
		int32 RewardedShows = 0;
		int32 InterstitialShows = 0;
		TArray<FName> BannerShows;
		TArray<FName> BannerHides;

		virtual bool IsRewardedReady() const override { return true; }
		virtual void ShowRewarded(FName Placement, FSimpleDelegate OnSuccess, FSimpleDelegate OnFail) override
		{
			++RewardedShows;
			OnSuccess.ExecuteIfBound();
		}
		virtual FDateTime GetLastRewardedShowTimeUtc() const override { return LastRewardedUtc; }
		virtual bool IsInterstitialReady(FName Placement) const override { return bInterstitialReady; }
		virtual void ShowInterstitial(FName Placement, FSimpleDelegate OnClosed) override
		{
			++InterstitialShows;
			OnClosed.ExecuteIfBound();
		}
		virtual void ShowBanner(FName Placement) override { BannerShows.Add(Placement); }
		virtual void HideBanner(FName Placement) override { BannerHides.Add(Placement); }
	};
}

// Полноэкранный показ вызывается ОДИН раз за сохранение: первый закрытый диалог сдачи
// показывает и поднимает признак, с поднятым признаком показа больше нет; диалог без сдачи
// точку не трогает вовсе.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdQuestInterstitialOncePerSaveTest,
	"ContrarySurvivor.Ads.Points.QuestInterstitialOncePerSave", AdsTestFlags)
bool FAdQuestInterstitialOncePerSaveTest::RunTest(const FString& Parameters)
{
	using namespace AdPoints;
	AdsTestLocal::FCountingAdService Ads;
	const FDateTime Now(2026, 10, 4, 12, 0, 0);
	int32 ClosedCalls = 0;
	const FSimpleDelegate OnClosed = FSimpleDelegate::CreateLambda([&ClosedCalls]() { ++ClosedCalls; });

	bool bUsed = false; // признак из сохранения

	// Обычный разговор со старостой (задание в этом диалоге не сдавали) — точка не наступила.
	TestTrue(TEXT("диалог без сдачи: точка не наступила"),
		RunQuestInterstitial(&Ads, AdPlacements::QuestBanditsDone, /*bTurnedInDuringDialog=*/false,
			bUsed, true, Now, 180.0, OnClosed) == EInterstitialDecision::NotThisDialog);
	TestFalse(TEXT("диалог без сдачи: признак не поднят"), bUsed);
	TestEqual(TEXT("диалог без сдачи: показа нет"), Ads.InterstitialShows, 0);

	// Диалог сдачи закрылся — показ один, признак поднят, «закрыто» пришло один раз.
	TestTrue(TEXT("диалог сдачи: показываем"),
		RunQuestInterstitial(&Ads, AdPlacements::QuestBanditsDone, true, bUsed, true, Now, 180.0, OnClosed)
			== EInterstitialDecision::Show);
	TestTrue(TEXT("диалог сдачи: признак поднят"), bUsed);
	TestEqual(TEXT("диалог сдачи: показ вызван один раз"), Ads.InterstitialShows, 1);
	TestEqual(TEXT("диалог сдачи: «закрыто» пришло один раз"), ClosedCalls, 1);

	// Повтор с поднятым признаком (то же сохранение) — показа больше нет.
	TestTrue(TEXT("повтор в том же сохранении: уже отработала"),
		RunQuestInterstitial(&Ads, AdPlacements::QuestBanditsDone, true, bUsed, true, Now, 180.0, OnClosed)
			== EInterstitialDecision::AlreadyUsed);
	TestEqual(TEXT("повтор в том же сохранении: показ по-прежнему один"), Ads.InterstitialShows, 1);

	// Новое сохранение (признак снова false) — точка работает заново.
	bool bUsedNewSave = false;
	RunQuestInterstitial(&Ads, AdPlacements::QuestBanditsDone, true, bUsedNewSave, true, Now, 180.0, OnClosed);
	TestEqual(TEXT("новое сохранение: показ вызван ещё раз"), Ads.InterstitialShows, 2);

	// Признак переживает автосохранение костра (перенос полей удержания).
	UContrarySaveGame* From = NewObject<UContrarySaveGame>();
	UContrarySaveGame* To = NewObject<UContrarySaveGame>();
	From->bQuestBanditsInterstitialUsed = true;
	UContrarySaveGame::CopyRetentionData(From, To);
	TestTrue(TEXT("признак точки переносится в свежий объект сохранения"), To->bQuestBanditsInterstitialUsed);
	return true;
}

// Полноэкранный показ НЕ вызывается, если игрок только что смотрел ролик с наградой, если нет
// согласия или показ не готов. Точка при этом считается отработавшей — позже не догоняется.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdQuestInterstitialSkipsTest,
	"ContrarySurvivor.Ads.Points.QuestInterstitialSkips", AdsTestFlags)
bool FAdQuestInterstitialSkipsTest::RunTest(const FString& Parameters)
{
	using namespace AdPoints;
	const FDateTime Now(2026, 10, 4, 12, 0, 0);
	const FSimpleDelegate NoOp;

	// Ролик с наградой закончился минуту назад — пропуск.
	{
		AdsTestLocal::FCountingAdService Ads;
		Ads.LastRewardedUtc = Now - FTimespan::FromSeconds(60.0);
		bool bUsed = false;
		const EInterstitialDecision Decision = RunQuestInterstitial(&Ads,
			AdPlacements::QuestBanditsDone, true, bUsed, true, Now, 180.0, NoOp);
		TestTrue(TEXT("ролик минуту назад: пропуск"), Decision == EInterstitialDecision::RecentRewarded);
		TestEqual(TEXT("ролик минуту назад: показа нет"), Ads.InterstitialShows, 0);
		TestTrue(TEXT("ролик минуту назад: точка отработала, позже не догоняется"), bUsed);
		TestEqual(TEXT("причина пропуска для аналитики"), InterstitialSkipReason(Decision), FString(TEXT("cooldown")));
	}

	// Ролик закончился ровно три минуты назад — граница включительно, показываем.
	{
		AdsTestLocal::FCountingAdService Ads;
		Ads.LastRewardedUtc = Now - FTimespan::FromSeconds(180.0);
		bool bUsed = false;
		RunQuestInterstitial(&Ads, AdPlacements::QuestBanditsDone, true, bUsed, true, Now, 180.0, NoOp);
		TestEqual(TEXT("ролик три минуты назад: показ есть"), Ads.InterstitialShows, 1);
	}

	// Порог берётся из переданного числа (настройка конфига), а не зашит.
	{
		AdsTestLocal::FCountingAdService Ads;
		Ads.LastRewardedUtc = Now - FTimespan::FromSeconds(60.0);
		bool bUsed = false;
		RunQuestInterstitial(&Ads, AdPlacements::QuestBanditsDone, true, bUsed, true, Now, 30.0, NoOp);
		TestEqual(TEXT("свой порог 30 с: ролик минуту назад не мешает"), Ads.InterstitialShows, 1);
	}

	// Нет согласия игрока.
	{
		AdsTestLocal::FCountingAdService Ads;
		bool bUsed = false;
		const EInterstitialDecision Decision = RunQuestInterstitial(&Ads,
			AdPlacements::QuestBanditsDone, true, bUsed, /*bConsentGranted=*/false, Now, 180.0, NoOp);
		TestTrue(TEXT("нет согласия: пропуск"), Decision == EInterstitialDecision::NoConsent);
		TestEqual(TEXT("нет согласия: показа нет"), Ads.InterstitialShows, 0);
	}

	// Показ не готов; рекламной службы нет вовсе — молча мимо, без падения.
	{
		AdsTestLocal::FCountingAdService Ads;
		Ads.bInterstitialReady = false;
		bool bUsed = false;
		TestTrue(TEXT("показ не готов: пропуск"),
			RunQuestInterstitial(&Ads, AdPlacements::QuestBanditsDone, true, bUsed, true, Now, 180.0, NoOp)
				== EInterstitialDecision::NoAd);
		TestEqual(TEXT("показ не готов: показа нет"), Ads.InterstitialShows, 0);

		bool bUsedNoService = false;
		TestTrue(TEXT("службы нет: пропуск"),
			RunQuestInterstitial(nullptr, AdPlacements::QuestBanditsDone, true, bUsedNoService, true, Now, 180.0, NoOp)
				== EInterstitialDecision::NoAd);
	}

	// Значение настройки по умолчанию — три минуты (решение Рината).
	TestEqual(TEXT("порог по умолчанию в настройках — 180 с"),
		UYandexAdService::GetSettings().QuestInterstitialRewardedCooldownSeconds, 180.0f);
	return true;
}

// Баннер запрашивается при открытии экрана смерти и окна торговли и убирается при закрытии;
// без согласия не запрашивается, но убирается всегда.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdScreenBannerTest,
	"ContrarySurvivor.Ads.Points.BannerFollowsScreen", AdsTestFlags)
bool FAdScreenBannerTest::RunTest(const FString& Parameters)
{
	using namespace AdPoints;
	AdsTestLocal::FCountingAdService Ads;

	TestTrue(TEXT("экран смерти открылся: баннер заказан"),
		SetScreenBanner(&Ads, AdPlacements::BannerDeath, /*bScreenOpen=*/true, /*bConsentGranted=*/true));
	TestEqual(TEXT("экран смерти: один заказ"), Ads.BannerShows.Num(), 1);
	TestTrue(TEXT("экран смерти: заказана именно его точка"),
		Ads.BannerShows.Num() == 1 && Ads.BannerShows[0] == AdPlacements::BannerDeath);
	TestEqual(TEXT("экран смерти открыт: баннер не убирали"), Ads.BannerHides.Num(), 0);

	SetScreenBanner(&Ads, AdPlacements::BannerDeath, false, true);
	TestTrue(TEXT("экран смерти закрылся: баннер убран"),
		Ads.BannerHides.Num() == 1 && Ads.BannerHides[0] == AdPlacements::BannerDeath);

	TestTrue(TEXT("окно торговли открылось: баннер заказан"),
		SetScreenBanner(&Ads, AdPlacements::BannerShop, true, true));
	TestTrue(TEXT("окно торговли: заказана именно его точка"),
		Ads.BannerShows.Num() == 2 && Ads.BannerShows[1] == AdPlacements::BannerShop);
	SetScreenBanner(&Ads, AdPlacements::BannerShop, false, true);
	TestTrue(TEXT("окно торговли закрылось: баннер убран"),
		Ads.BannerHides.Num() == 2 && Ads.BannerHides[1] == AdPlacements::BannerShop);

	// Нет согласия: при открытии баннер не заказывается, при закрытии всё равно убирается.
	TestFalse(TEXT("нет согласия: баннер не заказан"),
		SetScreenBanner(&Ads, AdPlacements::BannerDeath, true, /*bConsentGranted=*/false));
	TestEqual(TEXT("нет согласия: заказов не прибавилось"), Ads.BannerShows.Num(), 2);
	SetScreenBanner(&Ads, AdPlacements::BannerDeath, false, false);
	TestEqual(TEXT("нет согласия: уборка всё равно прошла"), Ads.BannerHides.Num(), 3);

	// Службы нет — без падения.
	TestFalse(TEXT("службы нет: баннер не заказан"),
		SetScreenBanner(nullptr, AdPlacements::BannerShop, true, true));

	// Баннер и полноэкранный показ ролик с наградой не трогают.
	TestEqual(TEXT("ролик с наградой не вызывался"), Ads.RewardedShows, 0);
	return true;
}

// Старые точки ролика работают как раньше: ключи четырёх точек не изменились, каждая
// по-прежнему берёт свой идентификатор места (или общий, если свой пуст), а идентификаторы
// новых форматов в ролики не подмешаны.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdRewardedPointsUnchangedTest,
	"ContrarySurvivor.Ads.Points.RewardedPointsUnchanged", AdsTestFlags)
bool FAdRewardedPointsUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("ключ точки «Спасти рюкзак»"), AdPlacements::DeathBackpack, FName(TEXT("death_backpack")));
	TestEqual(TEXT("ключ точки «Продать дороже»"), AdPlacements::ShopSellBonus, FName(TEXT("shop_sell_bonus")));
	TestEqual(TEXT("ключ точки ежедневной награды"), AdPlacements::DailyDouble, FName(TEXT("daily_double")));
	TestEqual(TEXT("ключ точки «Поддержать автора»"), AdPlacements::SupportAuthor, FName(TEXT("support_author")));

	const UYandexAdService& Service = UYandexAdService::GetSettings();
	auto Expected = [&Service](const FString& OwnId)
	{
		return OwnId.IsEmpty() ? Service.RewardedAdUnitId : OwnId;
	};
	TestEqual(TEXT("место ролика «Спасти рюкзак»"),
		Service.GetAdUnitIdForPlacement(AdPlacements::DeathBackpack), Expected(Service.AdUnitIdDeathBackpack));
	TestEqual(TEXT("место ролика «Продать дороже»"),
		Service.GetAdUnitIdForPlacement(AdPlacements::ShopSellBonus), Expected(Service.AdUnitIdShopSellBonus));
	TestEqual(TEXT("место ролика ежедневной награды"),
		Service.GetAdUnitIdForPlacement(AdPlacements::DailyDouble), Expected(Service.AdUnitIdDailyDouble));
	TestEqual(TEXT("место ролика «Поддержать автора»"),
		Service.GetAdUnitIdForPlacement(AdPlacements::SupportAuthor), Expected(Service.AdUnitIdSupportAuthor));

	// Идентификаторы новых форматов (если заданы) не совпадают ни с одним местом ролика:
	// иначе служба приняла бы ролик за полноэкранный показ без награды.
	const FName RewardedPoints[] = { AdPlacements::DeathBackpack, AdPlacements::ShopSellBonus,
		AdPlacements::DailyDouble, AdPlacements::SupportAuthor };
	const FString NewIds[] = { Service.AdUnitIdQuestInterstitial, Service.AdUnitIdBannerDeath,
		Service.AdUnitIdBannerShop };
	for (const FName& Point : RewardedPoints)
	{
		for (const FString& NewId : NewIds)
		{
			TestTrue(FString::Printf(TEXT("место ролика '%s' не занято новым форматом"), *Point.ToString()),
				NewId.IsEmpty() || NewId != Service.GetAdUnitIdForPlacement(Point));
		}
	}

	// Имена точек в аналитике: старые прежние, новые — свои.
	TestEqual(TEXT("аналитика: рюкзак"), UYandexAdService::PlacementToAnalyticsPoint(AdPlacements::DeathBackpack), FString(TEXT("backpack")));
	TestEqual(TEXT("аналитика: магазин"), UYandexAdService::PlacementToAnalyticsPoint(AdPlacements::ShopSellBonus), FString(TEXT("shop")));
	TestEqual(TEXT("аналитика: ежедневная"), UYandexAdService::PlacementToAnalyticsPoint(AdPlacements::DailyDouble), FString(TEXT("daily")));
	TestEqual(TEXT("аналитика: поддержка"), UYandexAdService::PlacementToAnalyticsPoint(AdPlacements::SupportAuthor), FString(TEXT("support")));
	TestEqual(TEXT("аналитика: показ после задания"), UYandexAdService::PlacementToAnalyticsPoint(AdPlacements::QuestBanditsDone), FString(TEXT("quest_bandits")));
	TestEqual(TEXT("аналитика: баннер смерти"), UYandexAdService::PlacementToAnalyticsPoint(AdPlacements::BannerDeath), FString(TEXT("banner_death")));
	TestEqual(TEXT("аналитика: баннер торговли"), UYandexAdService::PlacementToAnalyticsPoint(AdPlacements::BannerShop), FString(TEXT("banner_shop")));

	// Подставная служба: ролик с наградой по-прежнему зовёт «успех» и не трогает новые форматы.
	AdsTestLocal::FCountingAdService Ads;
	int32 Successes = 0;
	Ads.ShowRewarded(AdPlacements::DeathBackpack,
		FSimpleDelegate::CreateLambda([&Successes]() { ++Successes; }), FSimpleDelegate());
	TestEqual(TEXT("ролик: успех пришёл один раз"), Successes, 1);
	TestEqual(TEXT("ролик: полноэкранный показ не вызывался"), Ads.InterstitialShows, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
