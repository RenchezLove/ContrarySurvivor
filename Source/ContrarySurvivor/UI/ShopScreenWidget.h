// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContrarySurvivor/Actors/ShopVendor.h" // IShopVendor/TScriptInterface (источник каталога/цен)
#include "ShopScreenWidget.generated.h"

class UTextBlock;
class UButton;
class UScrollBox;
class USlider;
class UShopRowWidget;
class APlayerCharacter;
class AMasterInventoryItem;

/**
 * Экран магазина на UMG (ADR-048, этап 1). Логика — здесь; раскладку WBP_Shop строит
 * Ринат мышкой по схеме docs/contrary-survivor/umg-layout-guide.md. Кубики цепляются
 * по ТОЧНЫМ именам (BindWidgetOptional: нет кубика — предупреждение в лог, не краш).
 *
 * Владелец — AContrarySurvivorHUD: слот ShopWidgetClass назначен -> SetShopOpen создаёт
 * этот виджет вместо Canvas-отрисовки DrawShop (Canvas-путь живёт как запасной, пока
 * слот пуст — миграция без поломки игры, ADR-048).
 *
 * Данные — от вендора (IShopVendor) и игрока (APlayerCharacter), НЕ от HUD-класса:
 * покупка/продажа — СУЩЕСТВУЮЩИЕ методы Shop_BuyEntryQty / Shop_SellItemQty /
 * Shop_SellItem (расчёты не дублируются). Прокрутка списков — штатный ScrollBox
 * (свайп и колесо бесплатно), количество — штатный Slider + кнопки [-]/[+].
 *
 * Закрытие (кнопка Close) — делегат OnCloseRequested: мир закрывает контроллер
 * (CloseShop), сам виджет паузу/ввод не трогает.
 */
UCLASS()
class CONTRARYSURVIVOR_API UShopScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязка данных после CreateWidget (вендор + игрок) и первая сборка списков.
	void InitShop(TScriptInterface<IShopVendor> InTrader, APlayerCharacter* InPlayer);

	// Кнопка Close нажата — подписан контроллер (CloseShop).
	FSimpleMulticastDelegate OnCloseRequested;

	// Пересобрать списки и денежную строку (после покупки/продажи: составы и цены сменились).
	void RefreshAll();

	// --- Настройки (Class Defaults WBP_Shop; владение переехало из HUD — ADR-048/решение лида:
	// одно место правды; Canvas-путь использует прежние строки литералами до своего выпила) ---

	// Класс строки списков: Ринат назначает сюда WBP_ShopRow. Пусто — списки не строятся
	// (в лог уходит предупреждение), остальной экран работает.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "1"))
	TSubclassOf<UShopRowWidget> RowWidgetClass;

	// Перед числом денег: «Монеты 150».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "1"))
	FString MoneyPrefix = TEXT("Монеты ");

	// Подписи кнопок действия в строках списков.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "2"))
	FString BuyActionText = TEXT("Buy");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "3"))
	FString SellActionText = TEXT("Sell");

	// Заголовок панели количества: «КУПИТЬ: Аптечка» / «ПРОДАТЬ: Патроны 9мм».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "4"))
	FString BuyTitle = TEXT("КУПИТЬ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "5"))
	FString SellTitle = TEXT("ПРОДАТЬ");

	// Перед «N / max»: «Кол-во: 3 / 10».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "6"))
	FString QtyPrefix = TEXT("Кол-во: ");

	// Перед итогом покупки «Итого: 120» и выручкой продажи «Выручка: +40» (плюс — часть текста).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "7"))
	FString TotalPrefix = TEXT("Итого: ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "8"))
	FString RevenuePrefix = TEXT("Выручка: +");

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Обработчики кубиков (AddDynamic требует UFUNCTION) ---

	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandleSliderValueChanged(float NewValue);
	UFUNCTION() void HandleQtyMinusClicked();
	UFUNCTION() void HandleQtyPlusClicked();
	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCancelClicked();

	// Клик по кнопке строки (Buy/Sell) — payload в самой строке.
	void HandleRowAction(UShopRowWidget* Row);

	// --- Транзакция количества (та же модель, что Canvas-слайдер: BUY по индексу каталога,
	// SELL — стак патронов; прочие предметы продаются сразу без панели) ---

	void ArmBuyTransaction(int32 CatalogIndex);
	void ArmSellTransaction(AMasterInventoryItem* Item);
	void CloseTransaction();

	// Обновить строки панели количества (Кол-во/Итого) и позицию ползунка под текущее Qty.
	void UpdateTransactionTexts();

	// Установить Qty с клампом 1..Max и обновить панель.
	void SetTransactionQty(int32 NewQty);

	// Пересобрать один список; bBuyList: каталог вендора / продаваемое из рюкзака.
	void RebuildList(bool bBuyList);

	// Текущие деньги игрока (0 при отсутствии статов).
	float GetPlayerMoney() const;

	// --- Кубики WBP_Shop (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Строка денег игрока (обновляется каждый кадр).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyText;

	// Списки: каталог торговца и рюкзак игрока. Прокрутка — штатная (палец/колесо).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> BuyList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> SellList;

	// Кнопка закрытия магазина.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	// --- Панель количества (показывается на время транзакции, иначе спрятана) ---

	// Корень панели количества: любой контейнер (Border/CanvasPanel/...). Виджет сам
	// переключает ему видимость Visible/Collapsed.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SliderPanel;

	// «КУПИТЬ: Аптечка».
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SliderTitleText;

	// «Кол-во: 3 / 10 (= 30 ammo)».
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SliderQtyText;

	// Ползунок количества (шаг 1, диапазон 1..max — выставляет код).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> QtySlider;

	// Кнопки точной подстройки количества.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QtyMinusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QtyPlusButton;

	// «Итого: 120» / «Выручка: +40» (живая, меняется с ползунком).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SliderTotalText;

	// Подтвердить/отменить транзакцию.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SliderConfirmButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SliderCancelButton;

private:
	// --- Данные экрана ---

	UPROPERTY()
	TScriptInterface<IShopVendor> Trader;

	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// --- Состояние транзакции количества ---

	bool bTransactionActive = false;
	bool bTransactionIsBuy = false;
	int32 TransactionCatalogIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<AMasterInventoryItem> TransactionItem;

	int32 TransactionQty = 1;
	int32 TransactionQtyMax = 1;
	float TransactionUnitPrice = 0.0f;
	int32 TransactionUnitAmmo = 0; // патронов в одной единице покупки (0 — не патроны)
	FString TransactionTitle;

	// Защита от рекурсии: SetValue ползунка триггерит OnValueChanged — игнорируем свой же вызов.
	bool bUpdatingSliderFromCode = false;
};
