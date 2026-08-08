// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnboardingHintWidget.generated.h"

class UTextBlock;
class UBorder;

/**
 * Стиль тоста-подсказки онбординга. Живёт EditAnywhere-полем на UOnboardingComponent
 * (виджет строится из C++-класса и в Details не виден — настройка на компоненте, паттерн
 * FTouchControlsConfig; решение game-lead 07-18: без BP-наследника виджета). Дефолты
 * дословно повторяют прежние зашитые значения.
 */
USTRUCT(BlueprintType)
struct FOnboardingHintStyle
{
	GENERATED_BODY()

	// Цвет плашки-подложки (тёмная полупрозрачная).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	FLinearColor PlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	// Внутренние отступы плашки: X — по горизонтали, Y — по вертикали.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	FVector2D PlatePadding = FVector2D(18.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "8"))
	int32 FontSize = 18;

	// Цвет текста (тёплый жёлтый — палитра подсказки взаимодействия Canvas-HUD).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	FLinearColor TextColor = FLinearColor(1.0f, 0.95f, 0.5f, 1.0f);

	// Якорь на экране в долях (0.5 / 0.10 = верх-центр) и размер бокса плашки, px.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	FVector2D ScreenAnchor = FVector2D(0.5f, 0.10f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding")
	FVector2D BoxSize = FVector2D(820.0f, 96.0f);
};

/**
 * Всплывающая подсказка онбординга (Этап F1): тёмная плашка с текстом сверху по центру
 * экрана. Показ/скрытие управляет UOnboardingComponent (таймер/любой ввод).
 * Верх-центр выбран, чтобы не спорить с подсказкой взаимодействия «E — подобрать»
 * (низ-центр) и статами игрока (верх-лево)/трекером квеста (верх-право).
 *
 * ТЗ Рината 08-07 — два пути, как у UEndOfStoryWidget (архитектура ADR-048):
 *  - создан из WBP_OnboardingHint (слот OnboardingHintWidgetClass на HUD) → дерево
 *    владельца из дизайнера (плашка и шрифт правятся мышкой), стиль код не перекрашивает;
 *    сам текст подсказки ставит код (SetHintText) — он зависит от шага обучения;
 *  - ассета нет / слот пуст → прежний кодовый вид (BuildCodeTree + FOnboardingHintStyle).
 */
UCLASS()
class CONTRARYSURVIVOR_API UOnboardingHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Текст подсказки — уже переводимый FText (собирает UOnboardingComponent, ADR-050).
	void SetHintText(const FText& Text);

	// Применяет стиль к уже построенному дереву (NativeOnInitialized отработал в CreateWidget
	// с дефолтами). Зовёт UOnboardingComponent сразу после создания виджета.
	// При дизайнер-дереве не делает ничего — вид целиком в ассете.
	void ApplyStyle(const FOnboardingHintStyle& Style);

protected:
	virtual void NativeOnInitialized() override;

private:
	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_OnboardingHint.
	void BuildCodeTree();

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> HintPlate;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;
};
