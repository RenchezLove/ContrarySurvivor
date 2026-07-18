// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DailyRewardWidget.generated.h"

class UTextBlock;
class UButton;
class UBorder;

/**
 * Стиль окна «Ежедневная награда». Живёт EditAnywhere-полем на UDailyRewardComponent
 * (виджет строится из C++-класса и в Details не виден — паттерн FTouchControlsConfig;
 * директива Рината 07-18). Дефолты дословно повторяют прежние зашитые значения.
 */
USTRUCT(BlueprintType)
struct FDailyRewardStyle
{
	GENERATED_BODY()

	// Золотой кант и тёмный фон панели (палитра модалок HUD).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor FrameColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor PanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FString TitleText = TEXT("ЕЖЕДНЕВНАЯ НАГРАДА");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 TitleFontSize = 22;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	// Строка серии собирается кодом: Prefix + число = «День серии: 3»
	// (формат-строки в редактор не отдаём — решение game-lead 07-18).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FString StreakPrefix = TEXT("День серии: ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 StreakFontSize = 17;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor StreakColor = FLinearColor(0.95f, 0.96f, 1.0f, 1.0f);

	// Строка суммы собирается кодом: Prefix + число + Suffix = «+35 монет».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FString RewardPrefix = TEXT("+");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FString RewardSuffix = TEXT(" монет");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 RewardFontSize = 26;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor RewardColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FString TakeButtonText = TEXT("Забрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 TakeButtonFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor TakeButtonTextColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
};

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
	// Применяет стиль к уже построенному дереву. Звать после CreateWidget, ДО SetupContent
	// (строки серии/суммы собираются из Prefix/Suffix стиля).
	void ApplyStyle(const FDailyRewardStyle& Style);

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

	// Элементы, которые перекрашивает ApplyStyle, и текущий стиль (для SetupContent).
	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> TakeLabelBlock;

	UPROPERTY()
	FDailyRewardStyle CurrentStyle;
};
