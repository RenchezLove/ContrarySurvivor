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
}
