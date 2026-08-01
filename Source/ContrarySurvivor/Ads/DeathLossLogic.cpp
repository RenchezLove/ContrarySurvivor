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

	TArray<FStackLoss> SplitLossAcrossStacks(const TArray<int32>& StackSizes,
		int32 LostPieces, int32 DroppedPieces)
	{
		// Штуки снимаются с ПЕРВЫХ стаков по порядку (как раньше терялись первые акторы);
		// внутри потерянного сначала идёт «мешочная» часть, затем уничтожаемая — мешок
		// собирает первые DroppedPieces штук, ровно как FPlan обещает игроку в превью.
		TArray<FStackLoss> Result;
		Result.SetNum(StackSizes.Num());

		int32 LostLeft = FMath::Max(0, LostPieces);
		int32 DropLeft = FMath::Clamp(DroppedPieces, 0, LostLeft);
		for (int32 Index = 0; Index < StackSizes.Num() && LostLeft > 0; ++Index)
		{
			const int32 Size = FMath::Max(0, StackSizes[Index]);
			const int32 Loss = FMath::Min(Size, LostLeft);
			const int32 Dropped = FMath::Min(Loss, DropLeft);
			Result[Index].DroppedPieces = Dropped;
			Result[Index].DestroyedPieces = Loss - Dropped;
			LostLeft -= Loss;
			DropLeft -= Dropped;
		}
		return Result;
	}
}
