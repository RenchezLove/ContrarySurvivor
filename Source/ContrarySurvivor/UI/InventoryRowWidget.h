// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryRowWidget.generated.h"

class UTextBlock;
class UButton;
class AMasterInventoryItem;

// Клики строки рюкзака: использовать/надеть предмет либо выбросить. Параметр — сама
// строка: подписчик (UInventoryScreenWidget) читает из неё Item.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryRowAction, class UInventoryRowWidget*);

/**
 * Одна строка рюкзака в UMG-инвентаре (ADR-048, этап 2). Логика — C++, раскладку
 * WBP_InventoryRow строит Ринат по схеме docs/contrary-survivor/umg-layout-guide.md
 * (кубики по ТОЧНЫМ именам; BindWidgetOptional — предупреждение, не краш).
 */
UCLASS()
class CONTRARYSURVIVOR_API UInventoryRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Заполняет строку: имя предмета, подпись действия («использовать»/«надеть»;
	// пусто — кнопка действия прячется: предмет из строки не применяется).
	void SetupRow(const FText& InName, const FText& InUseCaption);

	FOnInventoryRowAction OnUseClicked;
	FOnInventoryRowAction OnDropClicked;

	// Предмет строки (слабый указатель — предмет может исчезнуть между кадрами).
	TWeakObjectPtr<AMasterInventoryItem> Item;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION() void HandleUseClicked();
	UFUNCTION() void HandleDropClicked();

	// --- Кубики WBP_InventoryRow (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Имя предмета.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	// Кнопка применения (использовать расходник / надеть броню). Прячется у прочих предметов.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> UseButton;

	// Подпись на кнопке применения (ВНУТРИ UseButton) — «использовать»/«надеть», ставит код.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UseText;

	// Кнопка выброса; подпись (например «X») Ринат пишет прямо в WBP.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DropButton;
};
