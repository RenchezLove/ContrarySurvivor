// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnboardingHintWidget.generated.h"

class UTextBlock;

/**
 * Всплывающая подсказка онбординга (Этап F1): тёмная плашка с текстом сверху по центру
 * экрана. Дерево целиком строится в C++ (WidgetTree), создаётся напрямую из класса —
 * BP-наследник не нужен. Показ/скрытие управляет UOnboardingComponent (таймер/любой ввод).
 * Верх-центр выбран, чтобы не спорить с подсказкой взаимодействия «E — подобрать»
 * (низ-центр) и статами игрока (верх-лево)/трекером квеста (верх-право).
 */
UCLASS()
class CONTRARYSURVIVOR_API UOnboardingHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHintText(const FString& Text);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;
};
