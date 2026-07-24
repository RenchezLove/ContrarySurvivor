// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Retention/DailyRewardComponent.h"
#include "ContrarySurvivor/Retention/DailyRewardLogic.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h" // F3: событие ежедневного входа
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/DailyRewardWidget.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Engine/World.h"

UDailyRewardComponent::UDailyRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDailyRewardComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EvaluateTimer, this,
			&UDailyRewardComponent::EvaluateOrDefer, FMath::Max(ShowWindowDelay, 0.01f), false);
	}
}

void UDailyRewardComponent::EvaluateOrDefer()
{
	// Build 1 (решение Рината 07-24): интро первой встречи ещё впереди (крючок не показан) —
	// окно награды портило бы атмосферу интро. Откладываем ВСЮ проверку (не только окно):
	// начислить молча и показать окно позже без начисления нельзя — начисление и окно идут
	// одним шагом EvaluateDailyReward.
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player)
	{
		if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			if (!Save->bElderHookShown)
			{
				bAwaitingElderDialog = true;
				UE_LOG(LogTemp, Log,
					TEXT("DailyReward: intro pending, window deferred until elder dialog closes"));
				return;
			}
		}
	}
	EvaluateDailyReward();
}

void UDailyRewardComponent::NotifyElderDialogClosed()
{
	if (!bAwaitingElderDialog)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EvaluateTimer, this,
			&UDailyRewardComponent::HandleDeferredShow, FMath::Max(PostIntroShowDelay, 0.01f), false);
	}
}

void UDailyRewardComponent::HandleDeferredShow()
{
	// За время задержки игрок успел открыть другое модальное окно (или умер — экран смерти) —
	// поверх не лезем. Ожидание остаётся: покажемся после следующего закрытия диалога старосты.
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	AContrarySurvivorPlayerController* PC = Player
		? Cast<AContrarySurvivorPlayerController>(Player->GetController()) : nullptr;
	if (PC && PC->IsAnyModalUIOpen())
	{
		UE_LOG(LogTemp, Log, TEXT("DailyReward: deferred window postponed (modal UI open)"));
		return;
	}
	bAwaitingElderDialog = false;
	EvaluateDailyReward();
}

void UDailyRewardComponent::EvaluateDailyReward()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !Player->GetStats())
	{
		return;
	}

	UContrarySaveGame* Save = Player->LoadOrCreateSaveObject();
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("DailyReward: save object unavailable, skip"));
		return;
	}

	const DailyReward::FComputeResult Result = DailyReward::Compute(FDateTime::Now(),
		Save->LastDailyRewardDate, Save->DailyStreakDays, BaseReward, RewardStepPerDay, MaxReward);

	if (!Result.bGrant)
	{
		UE_LOG(LogTemp, Log, TEXT("DailyReward: same calendar day (streak %d), no reward"),
			Save->DailyStreakDays);
		return;
	}

	// Начисляем в живые статы и зеркалим в сейв: загрузка сейва при смерти перетирает деньги
	// значением из слота — без зеркала награда терялась бы при первой же смерти.
	Player->GetStats()->AddMoney(Result.Reward);
	if (Save->bHasData)
	{
		Save->Money += Result.Reward;
	}
	Save->LastDailyRewardDate = FDateTime::Now().GetDate();
	Save->DailyStreakDays = Result.NewStreak;
	Player->WriteSaveObject(Save);

	UE_LOG(LogTemp, Log, TEXT("DailyReward: day %d of streak -> +%.0f coins (persisted)"),
		Result.NewStreak, Result.Reward);

	// F3 (ADR-038): событие «ежедневный вход» (value = день серии). Без ключей — no-op.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordDailyLogin(Result.NewStreak);
	}

	// Окно «Ежедневная награда». Курсор в игре и так виден; режим GameAndUI — чтобы кнопка
	// ловила клик (паттерн модалок контроллера: диалог/магазин).
	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController());
	if (!PC)
	{
		return;
	}

	ActiveWindow = CreateWidget<UDailyRewardWidget>(PC, UDailyRewardWidget::StaticClass());
	if (!ActiveWindow)
	{
		return;
	}
	ActiveWindow->ApplyStyle(WindowStyle); // стиль с компонента (EditAnywhere) поверх дефолтов
	ActiveWindow->SetupContent(Result.NewStreak, Result.Reward);
	ActiveWindow->OnClosed.AddUObject(this, &UDailyRewardComponent::HandleWindowClosed);
	ActiveWindow->AddToViewport(/*ZOrder=*/50);

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
}

void UDailyRewardComponent::HandleWindowClosed()
{
	ActiveWindow = nullptr;

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	AContrarySurvivorPlayerController* PC = Player
		? Cast<AContrarySurvivorPlayerController>(Player->GetController()) : nullptr;
	if (PC && !PC->IsAnyModalUIOpen())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = true; // курсор нужен и в игре (клик-таргетинг)
	}
}
