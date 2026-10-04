// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ContrarySurvivor/Ads/AdService.h"
#include "MockAdService.generated.h"

class UMockAdWidget;
class UMockBannerWidget;

/**
 * Заглушка слоя рекламы (Build 1.2, указание Рината): IsRewardedReady() всегда true,
 * ShowRewarded() показывает экран «здесь будет ролик» (UMockAdWidget) с кнопкой закрытия
 * через 3 секунды и по закрытию зовёт onSuccess. Никакого реального SDK и зависимостей.
 * onFail у заглушки зовётся только если показать экран вообще не вышло (нет контроллера/
 * ролик уже идёт) — «закрыл досрочно» у заглушки не бывает, закрытие = досмотр.
 *
 * На время «ролика» игра ставится на паузу и снимается по закрытию (ТЗ раздел 0 п.7);
 * прежнее состояние паузы восстанавливается как было.
 */
UCLASS()
class CONTRARYSURVIVOR_API UMockAdService : public UGameInstanceSubsystem, public IAdService
{
	GENERATED_BODY()

public:
	// --- IAdService ---
	virtual bool IsRewardedReady() const override { return true; }
	virtual void ShowRewarded(FName Placement, FSimpleDelegate OnSuccess, FSimpleDelegate OnFail) override;
	virtual FDateTime GetLastRewardedShowTimeUtc() const override { return LastRewardedShowUtc; }

	// Полноэкранный показ без награды: тот же экран-заглушка, по закрытию зовётся OnClosed.
	virtual bool IsInterstitialReady(FName Placement) const override { return true; }
	virtual void ShowInterstitial(FName Placement, FSimpleDelegate OnClosed) override;

	// Баннер-заглушка: серая полоса с подписью на месте будущего баннера (верх или низ экрана
	// и высота — из тех же настроек, что у боевой рекламы), чтобы в редакторе было видно место.
	virtual void ShowBanner(FName Placement) override;
	virtual void HideBanner(FName Placement) override;

private:
	void HandleAdClosed();

	UPROPERTY()
	TObjectPtr<UMockAdWidget> ActiveWidget;

	UPROPERTY()
	TObjectPtr<UMockBannerWidget> BannerWidget;

	// Баннер какой точки сейчас стоит (NAME_None — никакого).
	FName ActiveBannerPlacement;

	// На экране полноэкранный показ без награды, а не ролик.
	bool bActiveIsInterstitial = false;

	// Когда закрылся последний «ролик с наградой» (UTC).
	FDateTime LastRewardedShowUtc;

	FSimpleDelegate PendingSuccess;

	// Пауза стояла ещё ДО «ролика» (например, меню) — тогда по закрытию её не снимаем.
	bool bWasPausedBefore = false;
};
