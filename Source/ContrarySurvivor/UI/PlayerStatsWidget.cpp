// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/PlayerStatsWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ARangedWeapon.h" // патроны только у дальнобоя (#5, как Canvas DrawPlayerStats)
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"

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
		FFormatNamedArguments Args;
		Args.Add(TEXT("Current"), FText::AsNumber(FMath::RoundToInt32(Stats->GetHealth())));
		Args.Add(TEXT("Max"), FText::AsNumber(FMath::RoundToInt32(Stats->GetMaxHealth())));
		HealthText->SetText(FText::Format(HealthFormat, Args));
	}
	if (HungerBar)
	{
		HungerBar->SetPercent(FMath::Clamp(Stats->GetHunger() / SurvivalMax, 0.0f, 1.0f));
	}
	if (HungerText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Current"), FText::AsNumber(FMath::RoundToInt32(Stats->GetHunger())));
		Args.Add(TEXT("Max"), FText::AsNumber(FMath::RoundToInt32(SurvivalMax)));
		HungerText->SetText(FText::Format(HungerFormat, Args));
	}
	if (ThirstBar)
	{
		ThirstBar->SetPercent(FMath::Clamp(Stats->GetThirst() / SurvivalMax, 0.0f, 1.0f));
	}
	if (ThirstText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Current"), FText::AsNumber(FMath::RoundToInt32(Stats->GetThirst())));
		Args.Add(TEXT("Max"), FText::AsNumber(FMath::RoundToInt32(SurvivalMax)));
		ThirstText->SetText(FText::Format(ThirstFormat, Args));
	}

	// Патроны — только с дальнобоем в руках (нож/пустые руки — строка прячется).
	ARangedWeapon* Ranged = Cast<ARangedWeapon>(Player->GetCurrentWeapon());

	// Находка лида 08-05: на устройстве патроны/иконка держались показанными при пустом слоте
	// огнестрела в рюкзаке (RangedWeaponInstance == nullptr). Причину в коде не нашли (оба
	// признака выводятся из одного CurrentWeapon и обязаны совпадать), но раз на устройстве они
	// разошлись — гейт делаем по ОБОИМ сразу: «в руках» обязано быть ИМЕННО тем стволом, что
	// отслеживается в слоте, а не просто любым ARangedWeapon. Несовпадение — громкий Warning,
	// чтобы следующий такой случай был виден в логе сразу, а не искался полчаса.
	if (Ranged && Ranged != Player->GetRangedWeaponInstance())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PlayerStatsWidget: CurrentWeapon '%s' is ARangedWeapon, but != RangedWeaponInstance ('%s') — ammo hidden defensively"),
			*Ranged->GetName(),
			Player->GetRangedWeaponInstance() ? *Player->GetRangedWeaponInstance()->GetName() : TEXT("null"));
		Ranged = nullptr;
	}

	const ESlateVisibility AmmoVisibility =
		Ranged ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

	// Видимость ставим И контейнеру, И содержимому. Контейнера одного НЕ ХВАТАЕТ: в ассете
	// строка патронов создана спрятанной, и спрятан там не только ряд, но и сам кубик с
	// числом. Показав только ряд, мы показывали пустое место — из-за этого патроны не
	// появлялись даже с пистолетом в руках (баг владельца 2026-07-20).
	if (AmmoRow)
	{
		// Контейнер прячет подпись и значение разом — подпись живёт в дизайнере, код её не знает.
		AmmoRow->SetVisibility(AmmoVisibility);
	}
	if (AmmoText)
	{
		AmmoText->SetVisibility(AmmoVisibility);
	}
	if (AmmoBagText)
	{
		AmmoBagText->SetVisibility(AmmoVisibility);
	}

	if (Ranged)
	{
		// «Всего у игрока» = запас при оружии + пачки в рюкзаке. Складывать честно:
		// перезарядка сама вливает рюкзак в запас (APlayerCharacter::ReloadCurrentWeapon,
		// единственное место забора из рюкзака во всём проекте), поэтому каждый патрон
		// из этой суммы игрок реально может расстрелять, ничего дополнительно не делая.
		const int32 ReserveAmmo = Ranged->GetCurrentAmmoReserve();
		const int32 BagAmmo = Player->GetReserveAmmoInInventory();

		FFormatNamedArguments Args;
		Args.Add(TEXT("InClip"), FText::AsNumber(Ranged->GetCurrentAmmoInClip()));
		Args.Add(TEXT("Total"), FText::AsNumber(ReserveAmmo + BagAmmo));
		Args.Add(TEXT("Reserve"), FText::AsNumber(ReserveAmmo));
		Args.Add(TEXT("Bag"), FText::AsNumber(BagAmmo));

		if (AmmoText)
		{
			AmmoText->SetText(FText::Format(AmmoFormat, Args));
		}
		if (AmmoBagText)
		{
			AmmoBagText->SetText(FText::Format(AmmoBagFormat, Args));
		}
	}

	if (MoneyText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FText::AsNumber(FMath::RoundToInt32(Stats->GetMoney())));
		MoneyText->SetText(FText::Format(MoneyFormat, Args));
	}
}
