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
 *
 * ⚠ Стиль действует ТОЛЬКО для кодового дерева-фолбэка. Если окну назначен ассет
 * WBP_DailyReward (слот DailyRewardWidgetClass на HUD, ТЗ Рината 08-07), шрифты/цвета/
 * раскладку владелец правит мышкой в дизайнере, и код их не перекрашивает; из стиля
 * продолжают действовать только ФОРМАТЫ строк с числами (StreakFormat/RewardFormat/
 * DoubleSubFormat/DoubledRewardFormat/AdNotFinishedText) — это данные, а не вид.
 *
 * Локализация (ADR-050): подписи — FText с дефолтами через NSLOCTEXT (LOCTEXT в значении
 * по умолчанию UHT запрещает — UhtTextProperty.cs:104). Строки с числами собираются
 * FText::Format с ИМЕНОВАННЫМИ подстановками: прежняя пара «приставка + окончание» на
 * другом языке дала бы неверный порядок слов.
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
	FText TitleText = NSLOCTEXT("DailyRewardWidget", "TitleText", "ЕЖЕДНЕВНАЯ НАГРАДА");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 TitleFontSize = 22;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	// Строка серии: {Days} — какой день подряд игрок заходит.
	// (Прежняя приставка StreakPrefix заменена форматом — ADR-050 отменил склейку строк.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FText StreakFormat = NSLOCTEXT("DailyRewardWidget", "StreakFormat", "День серии: {Days}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 StreakFontSize = 17;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor StreakColor = FLinearColor(0.95f, 0.96f, 1.0f, 1.0f);

	// Строка суммы: {Amount} — сколько монет начислено. Знак «+» — часть формата,
	// а не приклеенный кодом символ (ADR-050).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FText RewardFormat = NSLOCTEXT("DailyRewardWidget", "RewardFormat", "+{Amount} монет");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 RewardFontSize = 26;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor RewardColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FText TakeButtonText = NSLOCTEXT("DailyRewardWidget", "TakeButtonText", "Забрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "8"))
	int32 TakeButtonFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward")
	FLinearColor TakeButtonTextColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	// --- Build 1.2: золотая кнопка «Забрать вдвое больше» (ТЗ издателя №3) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward|Ads")
	FText DoubleButtonText = NSLOCTEXT("DailyRewardWidget", "DoubleButtonText", "Забрать вдвое больше");

	// Вторая строка мелко — КОНКРЕТНЫЕ числа, не «×2» (ТЗ №3 раздел 3):
	// {Double} — удвоенная сумма, {Base} — обычная.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward|Ads")
	FText DoubleSubFormat = NSLOCTEXT("DailyRewardWidget", "DoubleSubFormat",
		"{Double} монет вместо {Base} за просмотр ролика");

	// Строка суммы после удвоения: {Amount} — итог (ТЗ №3 п.5: подтверждение с итогом).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward|Ads")
	FText DoubledRewardFormat = NSLOCTEXT("DailyRewardWidget", "DoubledRewardFormat",
		"+{Amount} монет — удвоено!");

	// Строка после досрочного закрытия ролика (у заглушки не случается).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward|Ads")
	FText AdNotFinishedText = NSLOCTEXT("DailyRewardWidget", "AdNotFinishedText",
		"Награда даётся за полный просмотр");

	// Тёплое золото — единый цвет rewarded-кнопок (ТЗ раздел 0 п.9).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward|Ads")
	FLinearColor DoubleButtonColor = FLinearColor(0.85f, 0.62f, 0.14f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward|Ads")
	FLinearColor DoubleButtonTextColor = FLinearColor(0.1f, 0.08f, 0.03f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward|Ads", meta = (ClampMin = "8"))
	int32 DoubleButtonFontSize = 17;
};

/**
 * Окно «Ежедневная награда» (Этап F2, ADR-044 п.4).
 *
 * ТЗ Рината 08-07 («хочу редактировать окно ежедневной награды мышкой») — два пути,
 * как у UEndOfStoryWidget/UTouchControlsWidget (архитектура ADR-048):
 *  - создан из WBP_DailyReward (родитель этот класс; слот DailyRewardWidgetClass на HUD
 *    заполняет режим генератора -hudslots) → дерево владельца из дизайнера, кубики приходят
 *    по BindWidgetOptional-именам, код их НЕ перекрашивает — тексты, кнопки, картинки и
 *    раскладку Ринат правит мышкой; замков дизайнера в ассете НЕТ вовсе;
 *  - ассета нет / слот пуст → прежний кодовый вид: дерево строится в C++ (BuildCodeTree),
 *    стиль — FDailyRewardStyle с компонента.
 */
UCLASS()
class CONTRARYSURVIVOR_API UDailyRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Применяет стиль к уже построенному дереву. Звать после CreateWidget, ДО SetupContent
	// (строки серии/суммы собираются по форматам стиля). При дизайнер-дереве меняет только
	// запомненные форматы строк — вид владельца из ассета не трогается.
	void ApplyStyle(const FDailyRewardStyle& Style);

	// Заполняет строки окна (день серии + сумма). Звать после CreateWidget, до AddToViewport.
	void SetupContent(int32 StreakDays, float RewardAmount);

	// Окно закрыто кнопкой «Забрать» — владелец (UDailyRewardComponent) возвращает режим ввода.
	FSimpleMulticastDelegate OnClosed;

	// --- Build 1.2: удвоение за просмотр (ТЗ №3). Условия показа решает владелец
	// (UDailyRewardComponent) — виджет только показывает/прячет и рисует числа. ---

	// Игрок нажал «Забрать вдвое больше» — владелец крутит ролик и начисляет.
	FSimpleMulticastDelegate OnDoubleRequested;

	// Показ/скрытие золотой кнопки. BaseReward — обычная награда дня (для чисел подстроки).
	void SetupDoubleOffer(float BaseReward, bool bVisible);

	// После досмотра: строка суммы = итог с удвоением, золотая кнопка прячется
	// (обычная «Забрать» остаётся — ей баннер и закрывают).
	void ShowDoubledResult(float TotalAmount);

	// Досрочное закрытие ролика: спокойная строка, кнопка остаётся (повтор разрешён, ТЗ №3 п.5).
	void ShowAdNotFinished();

protected:
	// Детект дизайнер-дерева (WBP) либо сборка кодового фолбэка + подписка кнопок.
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleTakeClicked();

	UFUNCTION()
	void HandleDoubleClicked();

private:
	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_DailyReward (BindWidgetOptional биндит по имени).
	void BuildCodeTree();

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StreakText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RewardText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TakeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TakeText;

	// --- Build 1.2: кубики золотой кнопки удвоения (ТЗ №3) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DoubleButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DoubleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DoubleSubText;

	// Двойная рамка кодового фолбэка (в WBP её нет — там одна плашка PanelPlate с кантом).
	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;

	UPROPERTY()
	FDailyRewardStyle CurrentStyle;
};
