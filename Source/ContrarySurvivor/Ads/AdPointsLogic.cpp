// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/AdPointsLogic.h"
#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Ads/AdService.h"

namespace AdPoints
{
	EInterstitialDecision DecideQuestInterstitial(bool bTurnedInDuringDialog, bool bAlreadyUsed,
		bool bConsentGranted, const FDateTime& NowUtc, const FDateTime& LastRewardedUtc,
		double RewardedCooldownSeconds, bool bAdReady)
	{
		if (!bTurnedInDuringDialog)
		{
			return EInterstitialDecision::NotThisDialog;
		}
		if (bAlreadyUsed)
		{
			return EInterstitialDecision::AlreadyUsed;
		}
		if (!bConsentGranted)
		{
			return EInterstitialDecision::NoConsent;
		}
		// То же правило, что у кулдауна магазина: роликов не было — пройдено; часы перевели
		// назад — не пройдено, пока время не догонит метку.
		if (!AdGating::IsCooldownPassed(NowUtc, LastRewardedUtc, RewardedCooldownSeconds))
		{
			return EInterstitialDecision::RecentRewarded;
		}
		if (!bAdReady)
		{
			return EInterstitialDecision::NoAd;
		}
		return EInterstitialDecision::Show;
	}

	EInterstitialDecision RunQuestInterstitial(IAdService* Ads, FName Placement,
		bool bTurnedInDuringDialog, bool& bInOutUsed, bool bConsentGranted, const FDateTime& NowUtc,
		double RewardedCooldownSeconds, FSimpleDelegate OnClosed)
	{
		const EInterstitialDecision Decision = DecideQuestInterstitial(bTurnedInDuringDialog,
			bInOutUsed, bConsentGranted, NowUtc,
			Ads ? Ads->GetLastRewardedShowTimeUtc() : FDateTime(),
			RewardedCooldownSeconds, Ads && Ads->IsInterstitialReady(Placement));

		if (Decision == EInterstitialDecision::NotThisDialog || Decision == EInterstitialDecision::AlreadyUsed)
		{
			return Decision;
		}

		// Точка наступила — второй раз за сохранение не наступит.
		bInOutUsed = true;

		if (Decision == EInterstitialDecision::Show)
		{
			Ads->ShowInterstitial(Placement, OnClosed);
		}
		return Decision;
	}

	FString InterstitialSkipReason(EInterstitialDecision Decision)
	{
		switch (Decision)
		{
		case EInterstitialDecision::NoConsent:      return TEXT("no_consent");
		case EInterstitialDecision::RecentRewarded: return TEXT("cooldown");
		case EInterstitialDecision::NoAd:           return TEXT("no_ad");
		default:                                    return FString();
		}
	}

	bool SetScreenBanner(IAdService* Ads, FName Placement, bool bScreenOpen, bool bConsentGranted)
	{
		if (!Ads)
		{
			return false;
		}
		if (!bScreenOpen)
		{
			Ads->HideBanner(Placement);
			return false;
		}
		if (!bConsentGranted)
		{
			return false;
		}
		Ads->ShowBanner(Placement);
		return true;
	}
}
