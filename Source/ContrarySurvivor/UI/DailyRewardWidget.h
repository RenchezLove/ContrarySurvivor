// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DailyRewardWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * Окно «Ежедневная награда» (Этап F2, ADR-044 п.4). Лёгкий UMG-виджет: дерево целиком
 * строится в C++ (WidgetTree) — BP-наследник не обязателен, создаётся напрямую
 * CreateWidget<UDailyRewardWidget>(PC, UDailyRewardWidget::StaticClass()).
 * По плану v2 новые экраны — UMG (старый Canvas-HUD не растим).
 */
UCLASS()
class CONTRARYSURVIVOR_API UDailyRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Заполняет строки окна (день серии + сумма). Звать после CreateWidget, до AddToViewport.
	void SetupContent(int32 StreakDays, float RewardAmount);

	// Окно закрыто кнопкой «Забрать» — владелец (UDailyRewardComponent) возвращает режим ввода.
	FSimpleMulticastDelegate OnClosed;

protected:
	// Строит дерево виджета в C++ (панель по центру: заголовок, день серии, сумма, кнопка).
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleTakeClicked();

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> StreakText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RewardText;
};
