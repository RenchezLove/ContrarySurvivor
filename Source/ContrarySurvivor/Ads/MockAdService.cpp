// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/MockAdService.h"
#include "ContrarySurvivor/Ads/MockAdWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UMockAdService::ShowRewarded(FName Placement, FSimpleDelegate OnSuccess, FSimpleDelegate OnFail)
{
	// Один «ролик» за раз: повторный вызов до закрытия — отказ показа, награды нет.
	if (ActiveWidget)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: MOCK-AD '%s' rejected - another ad is already showing"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: MOCK-AD '%s' rejected - no local player controller"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	ActiveWidget = CreateWidget<UMockAdWidget>(PC, UMockAdWidget::StaticClass());
	if (!ActiveWidget)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: MOCK-AD '%s' rejected - widget creation failed"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	PendingSuccess = OnSuccess;

	ActiveWidget->SetPlacement(Placement);
	ActiveWidget->OnClosed.AddUObject(this, &UMockAdService::HandleAdClosed);
	// Поверх любых модалок (экран смерти/магазин живут на ZOrder <= 100).
	ActiveWidget->AddToViewport(/*ZOrder=*/300);

	// Пауза на время «ролика» (ТЗ раздел 0 п.7). Прежнее состояние запоминаем и вернём.
	bWasPausedBefore = UGameplayStatics::IsGamePaused(PC);
	if (!bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, true);
	}

	UE_LOG(LogQA, Display, TEXT("QA: MOCK-AD '%s' shown (paused=%s)"),
		*Placement.ToString(), bWasPausedBefore ? TEXT("already") : TEXT("by ad"));
}

void UMockAdService::HandleAdClosed()
{
	// Виджет уже снял себя с экрана (RemoveFromParent в HandleCloseClicked).
	APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (PC && !bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, false);
	}

	ActiveWidget = nullptr;

	// Делегат зовём ПОСЛЕДНИМ: обработчик успеха может открыть следующий экран/новый показ.
	FSimpleDelegate Success = PendingSuccess;
	PendingSuccess.Unbind();
	Success.ExecuteIfBound();
}

IAdService* AdService::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMockAdService>() : nullptr;
}
