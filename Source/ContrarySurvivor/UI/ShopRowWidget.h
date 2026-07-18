// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopRowWidget.generated.h"

class UTextBlock;
class UButton;
class AMasterInventoryItem;

// Клик по кнопке действия строки (Buy/Sell). Параметр — сама строка: подписчик
// (UShopScreenWidget) читает из неё payload (CatalogIndex / SellItem).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShopRowAction, class UShopRowWidget*);

/**
 * Одна строка списка магазина (ADR-048, этап 1): и для каталога «FOR SALE», и для
 * рюкзака «SELL». Логика — C++, раскладку WBP_ShopRow строит Ринат мышкой по схеме
 * из docs/contrary-survivor/umg-layout-guide.md; «кубики» цепляются по ТОЧНЫМ именам
 * (BindWidgetOptional: отсутствие кубика — предупреждение в лог, не краш).
 *
 * Строки создаёт UShopScreenWidget (CreateWidget по классу RowWidgetClass из своих
 * настроек) и кладёт в ScrollBox'ы; данные приходят через SetupRow.
 */
UCLASS()
class CONTRARYSURVIVOR_API UShopRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Заполняет строку: имя товара/предмета, текст цены, подпись кнопки действия,
	// доступность кнопки (bEnabled=false — «не хватает денег», кнопка гаснет).
	// Если кубика PriceText в WBP нет — цена дописывается в NameText (информация не теряется).
	void SetupRow(const FString& InName, const FString& InPrice, const FString& InActionCaption,
		bool bActionEnabled);

	// Клик по кнопке действия (родитель решает: армировать слайдер покупки/продажи или продать сразу).
	FOnShopRowAction OnActionClicked;

	// --- Payload строки (читает родитель в обработчике OnActionClicked) ---

	// Покупка: индекс позиции в каталоге вендора; INDEX_NONE у строк продажи.
	int32 CatalogIndex = INDEX_NONE;

	// Продажа: предмет рюкзака; слабый указатель — предмет может исчезнуть между кадрами.
	TWeakObjectPtr<AMasterInventoryItem> SellItem;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleActionClicked();

	// --- Кубики WBP_ShopRow (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Имя товара/предмета.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	// Цена (покупки «120» / выкупа «+40»).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PriceText;

	// Кнопка действия строки (Buy/Sell).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ActionButton;

	// Подпись на кнопке действия (лежит ВНУТРИ ActionButton).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionText;
};
