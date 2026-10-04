// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class IAdService;

/**
 * Правила двух новых рекламных точек (решение Рината 04.10.2026): полноэкранный показ без
 * награды после сдачи старосте задания про бандитов и маленький баннер на экране смерти и
 * в окне торговли. Мира и UObject здесь нет — правила гоняет headless-автотест
 * (Tests/AdsAutomationTests.cpp) на подставной рекламной службе, по образцу AdGating.
 *
 * Четыре прежние точки ролика с наградой сюда не входят и не меняются.
 */
namespace AdPoints
{
	// Что решила точка полноэкранного показа.
	enum class EInterstitialDecision : uint8
	{
		Show,            // показываем
		NotThisDialog,   // в этом диалоге задание не сдавали — точка не наступила
		AlreadyUsed,     // точка в этом сохранении уже отработала
		NoConsent,       // игрок не дал согласия на обработку данных
		RecentRewarded,  // игрок только что смотрел ролик с наградой
		NoAd             // рекламной службы нет или показ не готов
	};

	// Чистое решение. bTurnedInDuringDialog — задание сдано именно в том диалоге, который
	// сейчас закрылся; bAlreadyUsed — признак из сохранения; LastRewardedUtc с Ticks == 0 —
	// роликов не было. Порядок проверок = порядок значений перечисления.
	CONTRARYSURVIVOR_API EInterstitialDecision DecideQuestInterstitial(bool bTurnedInDuringDialog,
		bool bAlreadyUsed, bool bConsentGranted, const FDateTime& NowUtc,
		const FDateTime& LastRewardedUtc, double RewardedCooldownSeconds, bool bAdReady);

	// Отработать точку на рекламной службе. Возвращает решение; при Show зовёт
	// Ads->ShowInterstitial(Placement, OnClosed). bInOutUsed поднимается, как только точка
	// НАСТУПИЛА (любое решение, кроме NotThisDialog и AlreadyUsed): момент «диалог сдачи
	// закрылся» бывает один раз за сохранение, и пропущенный показ позже не догоняется.
	CONTRARYSURVIVOR_API EInterstitialDecision RunQuestInterstitial(IAdService* Ads, FName Placement,
		bool bTurnedInDuringDialog, bool& bInOutUsed, bool bConsentGranted, const FDateTime& NowUtc,
		double RewardedCooldownSeconds, FSimpleDelegate OnClosed);

	// Причина пропуска для аналитики (сегмент ad:<точка>:not_shown:<причина>); пусто — для
	// решений, о которых в аналитику не пишем (Show, NotThisDialog, AlreadyUsed).
	CONTRARYSURVIVOR_API FString InterstitialSkipReason(EInterstitialDecision Decision);

	// --- Когда баннер показывается (решение Рината 04.10.2026, дословно): «При первом, втором
	// и я думаю третьем открытии окна торговли... не стоит показывать этот банер»; «Шанс
	// показа рекламы при открытии окна торговли сделай 60%, в случае смерти также. Банер в
	// случае смерти показываем уже со второго раза». ---

	enum class EBannerDecision : uint8
	{
		Show,        // показываем
		FirstOpens,  // одно из первых открытий экрана — баннера ещё нет
		Chance       // не выпал шанс
	};

	// Чистое решение, принимается ОДИН раз при открытии экрана. OpenNumber — какое по счёту
	// это открытие за всю установку игры (1 = первое), SkipFirstOpens — сколько первых
	// открытий идут без баннера, ShowChance — шанс показа 0..1, Roll — случайное число
	// из [0, 1): показ при Roll < ShowChance.
	CONTRARYSURVIVOR_API EBannerDecision DecideScreenBanner(int32 OpenNumber, int32 SkipFirstOpens,
		float ShowChance, float Roll);

	// Причина непоказа для аналитики (ad:<точка>:not_shown:<причина>); пусто при Show.
	CONTRARYSURVIVOR_API FString BannerSkipReason(EBannerDecision Decision);

	// Баннер экрана: экран открылся (и согласие есть) — заказать баннер точки; экран
	// закрылся — убрать. Убирается баннер всегда, даже если согласие успели отозвать.
	// Возвращает true, если баннер был заказан.
	CONTRARYSURVIVOR_API bool SetScreenBanner(IAdService* Ads, FName Placement, bool bScreenOpen,
		bool bConsentGranted);
}
