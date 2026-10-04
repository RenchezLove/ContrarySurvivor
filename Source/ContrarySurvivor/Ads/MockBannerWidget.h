// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MockBannerWidget.generated.h"

class UBorder;
class UTextBlock;

/**
 * Заглушка маленького рекламного баннера для редактора и компьютера (решение Рината
 * 04.10.2026): серая полоса с подписью у нижнего края экрана — на том месте, где на
 * телефоне встанет настоящий баннер Яндекса. Игра при этом сжимается и освобождает под
 * полосу место (Ads/AdGameArea.h), поэтому полоса лежит на ПОЛНОМ экране, а не в области
 * игры. Нажатий не перехватывает. Никакого реального SDK.
 */
UCLASS()
class CONTRARYSURVIVOR_API UMockBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Поставить полосу у НИЖНЕГО края экрана: HeightFraction — высота
	// долей высоты экрана, Placement — имя точки в подписи.
	void Setup(FName Placement, float HeightFraction);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> Strip;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;
};
