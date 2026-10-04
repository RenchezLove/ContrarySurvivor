// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MockBannerWidget.generated.h"

class UBorder;
class UTextBlock;

/**
 * Заглушка маленького рекламного баннера для редактора и компьютера (решение Рината
 * 04.10.2026): серая полоса с подписью у верхнего или нижнего края экрана — на том месте
 * и той высоты, где на телефоне встанет настоящий баннер Яндекса. Нужна, чтобы видеть в
 * редакторе, что баннер накрывает. Нажатий не перехватывает. Никакого реального SDK.
 */
UCLASS()
class CONTRARYSURVIVOR_API UMockBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Поставить полосу: bAtTop — у верхнего края (иначе у нижнего), HeightFraction — высота
	// долей высоты экрана, Placement — имя точки в подписи.
	void Setup(FName Placement, bool bAtTop, float HeightFraction);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> Strip;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;
};
