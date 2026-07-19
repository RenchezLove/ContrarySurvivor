// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatsWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UWidget;

/**
 * Постоянная панель статов игрока на UMG (ADR-048, этап 3): HP/голод/жажда, патроны
 * экипированного дальнобоя, деньги. Раскладку WBP_PlayerStats строит Ринат по схеме
 * docs/contrary-survivor/umg-layout-guide.md. Живёт на экране всю игру (создаёт HUD
 * в BeginPlay при назначенном слоте); игрока берёт КАЖДЫЙ кадр у своего контроллера —
 * переживает респаун/смену пешки без устаревших указателей.
 *
 * Локализация (ADR-050): код пишет ТОЛЬКО значение («80/100»), подписи («Здоровье»)
 * — отдельные статичные TextBlock'и в дизайнере, код их не биндит и не трогает.
 * Формат значения — FText с ИМЕНОВАННЫМИ подстановками (порядок слов в языках разный),
 * склейка через FText::Format, числа через FText::AsNumber.
 */
UCLASS()
class CONTRARYSURVIVOR_API UPlayerStatsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Настройки (Class Defaults WBP_PlayerStats; владение переехало из HUD — ADR-048) ---

	// Форматы ЗНАЧЕНИЙ. Подстановки в фигурных скобках подставляет код, остальное — твой текст.
	// Доступные подстановки перечислены у каждого свойства; лишние можно не использовать.

	// Здоровье: {Current} — сейчас, {Max} — максимум.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "1"))
	FText HealthFormat = NSLOCTEXT("PlayerStatsWidget", "HealthFormat", "{Current}/{Max}");

	// Голод: {Current} — сейчас, {Max} — максимум шкалы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "2"))
	FText HungerFormat = NSLOCTEXT("PlayerStatsWidget", "HungerFormat", "{Current}");

	// Жажда: {Current} — сейчас, {Max} — максимум шкалы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "3"))
	FText ThirstFormat = NSLOCTEXT("PlayerStatsWidget", "ThirstFormat", "{Current}");

	// Патроны в кубике AmmoText: {InClip} — в магазине оружия, {Reserve} — запас при оружии,
	// {Bag} — патроны в рюкзаке. Если положишь отдельный кубик AmmoBagText под рюкзак —
	// убери отсюда «в рюкзаке {Bag}», иначе число покажется дважды.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "4"))
	FText AmmoFormat = NSLOCTEXT("PlayerStatsWidget", "AmmoFormat", "{InClip} / {Reserve}   в рюкзаке {Bag}");

	// Патроны в отдельном кубике AmmoBagText (если он есть): те же подстановки, что и выше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "5"))
	FText AmmoBagFormat = NSLOCTEXT("PlayerStatsWidget", "AmmoBagFormat", "{Bag}");

	// Деньги: {Amount} — сколько монет у игрока.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "6"))
	FText MoneyFormat = NSLOCTEXT("PlayerStatsWidget", "MoneyFormat", "{Amount}");

protected:
	// Всё обновляется каждый кадр (лёгкие SetText/SetPercent — как ежекадровый Canvas-путь).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Кубики WBP_PlayerStats (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---
	// Здесь только ЗНАЧЕНИЯ и полоски. Подписи («Здоровье», «Патроны») — обычные
	// TextBlock'и с любыми именами, код их не ищет и не переписывает.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;   // заполнение 0..1 ставит код

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;    // значение «80/100»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HungerBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HungerText;    // значение «64»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ThirstBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ThirstText;    // значение «71»

	// Вся строка патронов (подпись + число) — прячется целиком, когда в руках не дальнобой.
	// Положи сюда подпись «Патроны» и значения; без этого кубика код спрячет только числа.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AmmoRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;      // значение «7 / 21  (в рюкзаке 30)»

	// Необязательный отдельный кубик под запас в рюкзаке (если хочешь свою подпись рядом).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoBagText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyText;     // значение «150»
};
