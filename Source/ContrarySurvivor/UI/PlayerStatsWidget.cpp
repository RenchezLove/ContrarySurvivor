// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/PlayerStatsWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ARangedWeapon.h" // патроны только у дальнобоя (#5, как Canvas DrawPlayerStats)
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UPlayerStatsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Игрок — каждый кадр от владельца (переживает респаун без устаревших указателей).
	APlayerController* PC = GetOwningPlayer();
	APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	if (!Stats)
	{
		return;
	}

	const float SurvivalMax = FMath::Max(Stats->GetSurvivalMax(), 1.0f);

	if (HealthBar)
	{
		HealthBar->SetPercent(FMath::Clamp(Stats->GetHealthPercent(), 0.0f, 1.0f));
	}
	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("%s%.0f/%.0f"),
			*HpPrefix, Stats->GetHealth(), Stats->GetMaxHealth())));
	}
	if (HungerBar)
	{
		HungerBar->SetPercent(FMath::Clamp(Stats->GetHunger() / SurvivalMax, 0.0f, 1.0f));
	}
	if (HungerText)
	{
		HungerText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%.0f"), *HungerPrefix, Stats->GetHunger())));
	}
	if (ThirstBar)
	{
		ThirstBar->SetPercent(FMath::Clamp(Stats->GetThirst() / SurvivalMax, 0.0f, 1.0f));
	}
	if (ThirstText)
	{
		ThirstText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%.0f"), *ThirstPrefix, Stats->GetThirst())));
	}

	if (AmmoText)
	{
		// Патроны — только с дальнобоем в руках (нож/пустые руки — строка прячется).
		if (ARangedWeapon* Ranged = Cast<ARangedWeapon>(Player->GetCurrentWeapon()))
		{
			AmmoText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%s%d / %d  (bag %d)"),
				*AmmoPrefix, Ranged->GetCurrentAmmoInClip(), Ranged->GetCurrentAmmoReserve(),
				Player->GetReserveAmmoInInventory())));
		}
		else
		{
			AmmoText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (MoneyText)
	{
		MoneyText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%.0f"), *MoneyPrefix, Stats->GetMoney())));
	}
}
