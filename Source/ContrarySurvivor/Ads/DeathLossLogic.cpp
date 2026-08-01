// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/DeathLossLogic.h"

namespace DeathLoss
{
	FPlan Compute(int32 ItemCount, float Money, float ItemLossFrac, float MoneyLossFrac, float DropFrac)
	{
		FPlan Plan;

		const int32 Count = FMath::Max(0, ItemCount);
		const float Balance = FMath::Max(0.0f, Money);
		const float ItemFrac = FMath::Clamp(ItemLossFrac, 0.0f, 1.0f);
		const float MoneyFrac = FMath::Clamp(MoneyLossFrac, 0.0f, 1.0f);
		const float Drop = FMath::Clamp(DropFrac, 0.0f, 1.0f);

		Plan.LostItems = FMath::Clamp(
			FMath::RoundToInt32(static_cast<float>(Count) * ItemFrac), 0, Count);
		Plan.DroppedItems = FMath::Clamp(
			FMath::RoundToInt32(static_cast<float>(Plan.LostItems) * Drop), 0, Plan.LostItems);

		Plan.LostMoney = Balance * MoneyFrac;
		Plan.DroppedMoney = Plan.LostMoney * Drop;
		return Plan;
	}
}
