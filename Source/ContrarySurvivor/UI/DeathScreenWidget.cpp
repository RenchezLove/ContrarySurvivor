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
		// Минуты и секунды — с ведущим нулём, поэтому формат числа задаём явно
		// (по умолчанию FText::AsNumber ведущий ноль не рисует).
		FNumberFormattingOptions TwoDigits;
		TwoDigits.SetMinimumIntegralDigits(2);
		TwoDigits.SetUseGrouping(false);

		const float LifeSec = Player->GetLastLifeDuration();
		FFormatNamedArguments Args;
		Args.Add(TEXT("Minutes"), FText::AsNumber(FMath::FloorToInt32(LifeSec / 60.0f), &TwoDigits));
		Args.Add(TEXT("Seconds"), FText::AsNumber(FMath::FloorToInt32(LifeSec) % 60, &TwoDigits));
		LifetimeText->SetText(FText::Format(LifetimeFormat, Args));
	}
	if (KillerText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Name"), Player->GetLastDamagerName());
		KillerText->SetText(FText::Format(KillerFormat, Args));
	}
	if (MoneyText)
	{
		const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FText::AsNumber(FMath::RoundToInt32(Money)));
		MoneyText->SetText(FText::Format(MoneyFormat, Args));
	}
	if (QuestsText)
	{
		const int32 QuestsDone = Player->GetQuests() ? Player->GetQuests()->GetTurnedInQuestCount() : 0;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), FText::AsNumber(QuestsDone));
		QuestsText->SetText(FText::Format(QuestsFormat, Args));
	}
	if (KillsText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), FText::AsNumber(Player->GetEnemyKillCount()));
		KillsText->SetText(FText::Format(KillsFormat, Args));
	}
	if (MoneyLossText)
	{
		// Процент — живой из игрока (DeathMoneyLossFraction), текст не разойдётся с BP-настройкой.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Percent"),
			FText::AsNumber(FMath::RoundToInt32(Player->GetDeathMoneyLossFraction() * 100.0f)));
		MoneyLossText->SetText(FText::Format(MoneyLossFormat, Args));
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
