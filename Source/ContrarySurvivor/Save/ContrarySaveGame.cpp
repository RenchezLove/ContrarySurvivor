// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySaveGame.h"

void UContrarySaveGame::CopyRetentionData(const UContrarySaveGame* From, UContrarySaveGame* To)
{
	if (!From || !To)
	{
		return;
	}

	To->LastDailyRewardDate = From->LastDailyRewardDate;
	To->DailyStreakDays     = From->DailyStreakDays;
	To->bHintMovementShown  = From->bHintMovementShown;
	To->bHintPickupShown    = From->bHintPickupShown;
	To->bHintElderShown     = From->bHintElderShown;
	To->bHintInventoryShown = From->bHintInventoryShown;
	To->bHintDeathShown     = From->bHintDeathShown;
	To->bElderFirstGiftGiven = From->bElderFirstGiftGiven;
	To->bElderHookShown     = From->bElderHookShown;
	To->bElderNotebookHintShown = From->bElderNotebookHintShown;
	To->bEndOfStoryShown    = From->bEndOfStoryShown;

	// Build 1.2: накопитель игрового времени и счётчики rewarded-рекламы — без переноса
	// каждый автосейв костра обнулял бы 15-минутный гейт и суточные лимиты.
	To->TotalPlayTimeSeconds   = From->TotalPlayTimeSeconds;
	To->BackpackAdCounterDate  = From->BackpackAdCounterDate;
	To->BackpackAdUsesOnDate   = From->BackpackAdUsesOnDate;
	To->ShopAdCounterDate      = From->ShopAdCounterDate;
	To->ShopAdUsesOnDate       = From->ShopAdUsesOnDate;
	To->LastShopAdTime         = From->LastShopAdTime;
}
