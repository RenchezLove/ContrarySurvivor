// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DeathScreenWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UDeathScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RespawnButton)
	{
		RespawnButton->OnClicked.AddDynamic(this, &UDeathScreenWidget::HandleRespawnClicked);
	}
	else
	{
		// Возрождение остаётся доступным клавишами Enter/Пробел (путь контроллера) — не тупик.
		UE_LOG(LogQA, Warning, TEXT("DeathScreenWidget: кубик RespawnButton не найден в WBP_Death"));
	}
}

void UDeathScreenWidget::InitDeath(APlayerCharacter* InPlayer)
{
	Player = InPlayer;
}

void UDeathScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Player)
	{
		return;
	}

	// Статистика последней жизни — те же источники, что Canvas DrawDeathScreen.
	if (LifetimeText)
	{
		const float LifeSec = Player->GetLastLifeDuration();
		LifetimeText->SetText(FText::FromString(FString::Printf(TEXT("%s%02d:%02d"),
			*LifetimePrefix, FMath::FloorToInt(LifeSec / 60.0f), FMath::FloorToInt(LifeSec) % 60)));
	}
	if (KillerText)
	{
		KillerText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%s"), *KillerPrefix, *Player->GetLastDamagerName())));
	}
	if (MoneyText)
	{
		const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
		MoneyText->SetText(FText::FromString(FString::Printf(TEXT("%s%.0f"), *MoneyPrefix, Money)));
	}
	if (QuestsText)
	{
		const int32 QuestsDone = Player->GetQuests() ? Player->GetQuests()->GetTurnedInQuestCount() : 0;
		QuestsText->SetText(FText::FromString(FString::Printf(TEXT("%s%d"), *QuestsPrefix, QuestsDone)));
	}
	if (KillsText)
	{
		KillsText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%d"), *KillsPrefix, Player->GetEnemyKillCount())));
	}
	if (MoneyLossText)
	{
		// Процент — живой из игрока (DeathMoneyLossFraction), текст не разойдётся с BP-настройкой.
		const int32 MoneyLossPct = FMath::RoundToInt(Player->GetDeathMoneyLossFraction() * 100.0f);
		MoneyLossText->SetText(FText::FromString(
			FString::Printf(TEXT("%s%d%s"), *MoneyLossPrefix, MoneyLossPct, *MoneyLossSuffix)));
	}
}

void UDeathScreenWidget::HandleRespawnClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: respawn button clicked (UMG)"));
	if (Player)
	{
		Player->Respawn(); // тот же вызов, что кнопка Canvas-экрана и клавиши Enter/Пробел
	}
}
