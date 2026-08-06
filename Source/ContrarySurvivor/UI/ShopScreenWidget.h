// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"        // FAnchorData: снимок авторской геометрии списка
#include "ContrarySurvivor/Actors/ShopVendor.h" // IShopVendor/TScriptInterface (источник каталога/цен)
#include "ShopScreenWidget.generated.h"

class UTextBlock;
class UButton;
class UScrollBox;
class USlider;
class UItemTileWidget;
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

	// Б8 (издатель 08-05, п.5): сколько плиток шириной TileWidth с зазором Spacing помещается
	// в ряд шириной InnerWidth. Ряд из N плиток занимает N ширин и (N-1) зазоров.
	// InnerWidth <= 0 — ширина ещё не измерена (окно не рисовалось), тогда FallbackColumns.
	// Чистая арифметика отдельной функцией, чтобы её гонял headless-автотест: живой виджет
	// магазина в тесте без окна и контроллера не создать (тот же приём, что у GetSellQtyMax).
	static int32 ComputeColumnsForWidth(float InnerWidth, float TileWidth, float Spacing,
		int32 FallbackColumns);

	// Предел количества в сделке ПРОДАЖИ = размер стопки предмета (нестакающийся даёт 1).
	// Build 1.2.2, приёмка Рината 05-08: раньше стопку видели только патроны, поэтому стопка
	// «Вода х3» открывалась окном «Количество 1 из 1» и кнопки плюс/минус упирались в кламп.
	// Вынесено отдельной чистой функцией, чтобы её гонял headless-автотест (тот же приём, что
	// у AdGating/DailyReward): сам виджет без окна и контроллера в тесте не создать.
	static int32 GetSellQtyMax(const AMasterInventoryItem* Item);

	// --- Настройки (Class Defaults WBP_Shop; владение переехало из HUD — ADR-048/решение лида:
	// одно место правды; Canvas-путь использует прежние строки литералами до своего выпила) ---

	// Класс ПЛИТКИ списков (Build 1.2.2, тайлы вместо строк): по умолчанию C++-плитка с
	// кодовым деревом; Ринат/генератор назначает сюда WBP_ItemTile для стилизации.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "1",
		DisplayName = "Класс плитки товара"))
	TSubclassOf<UItemTileWidget> TileWidgetClass;

	// Б8 (издатель 08-05, п.5): сколько колонок ставить, ПОКА ширина списка не измерена.
	// Обычно число колонок считает сам код — по живой ширине списка и ширине плитки
	// (ComputeColumns): сколько плиток помещается в ряд, столько и колонок. Но в самый
	// первый кадр окна ширины ещё нет (геометрия появляется только после первой отрисовки),
	// и тогда берётся это число. Со второго кадра оно уже не используется.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "1", DisplayPriority = "2",
		DisplayName = "Колонок до первого замера ширины"))
	int32 TileColumns = 3;

	// Б8 (издатель 08-05, п.5, дословно «растянуть сетку на пустующую правую половину»):
	// пока продавать нечего, список товаров занимает и половину рюкзака, а сама половина
	// рюкзака вместе с её заголовком прячется; появился первый предмет на продажу — окно
	// возвращается к двум половинам. Авторская расстановка в ассете при этом НЕ переписывается:
	// код берёт снимок геометрии слота при запуске и возвращает её число в число.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "4",
		DisplayName = "Растягивать товары, когда рюкзак пуст"))
	bool bExpandBuyListWhenBackpackEmpty = true;

	// Б8 (издатель 08-05, п.5): было 110 — узкие плитки, из-за которых длинное название
	// брони переносилось на четыре строки. Ширина увеличена до 180: замер живого ассета
	// (срез -dumpslots 08-06) даёт под каталог 652 единицы раскладки, три плитки по 180
	// с зазорами занимают 556 — помещаются с запасом, а строк переноса становится вдвое
	// меньше. Высота — МИНИМУМ, не потолок (см. UItemTileWidget::SetTileSize): длинному
	// названию и красной строке «Не хватает монет» разрешено растить плитку вниз.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "5",
		DisplayName = "Размер плитки"))
	FVector2D TileSize = FVector2D(180.0f, 165.0f);

	// Б8, п.5: иконка пропорционально крупнее под расширенную плитку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "16.0", DisplayPriority = "6",
		DisplayName = "Размер иконки в плитке"))
	float TileIconSize = 84.0f;

	// Зазоры между плитками — как в инвентаре (приёмка Рината: ряды слипались по вертикали).
	// Сетку строит код при каждом обновлении, поэтому в дизайнере эти отступы не поменять:
	// они параметры окна. Значение — расстояние между СОСЕДНИМИ плитками, по краям сетки
	// остаётся половина.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "0.0", DisplayPriority = "7",
		DisplayName = "Зазор между плитками по горизонтали"))
	float TileSpacingX = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "0.0", DisplayPriority = "8",
		DisplayName = "Зазор между плитками по вертикали"))
	float TileSpacingY = 8.0f;

	// Б8 (издатель 08-05, п.4): нижний предел размера нажимаемой кнопки магазина по каждой
	// стороне. Кнопка мельче — код увеличивает её при открытии окна; кнопка крупнее —
	// НЕ трогает (уменьшать расстановку Рината код не имеет права). Ноль выключает проверку.
	// Значение задано в единицах раскладки интерфейса, не в пикселях устройства.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "0.0", DisplayPriority = "9",
		DisplayName = "Наименьший размер кнопки под палец"))
	float MinTouchSize = 48.0f;

	// Тексты магазина. Подстановки в фигурных скобках подставляет код, остальное — твой
	// текст. Статичные подписи («Монеты», «Количество») — отдельные кубики в дизайнере,
	// код их не пишет; здесь только то, что меняется по ходу сделки (ADR-050).

	// Деньги игрока: {Amount} — сколько монет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "1"))
	FText MoneyFormat = NSLOCTEXT("Shop", "MoneyFormat", "{Amount}");

	// Подписи кнопок в строках списков. В дизайнер уйти НЕ могут: одна и та же строка
	// служит и списку товаров, и списку рюкзака, слово меняется в зависимости от списка.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "2"))
	FText BuyActionText = NSLOCTEXT("Shop", "BuyAction", "Купить");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "3"))
	FText SellActionText = NSLOCTEXT("Shop", "SellAction", "Продать");

	// Цена в плитке: {Price} — число. Отдельные форматы, потому что у выкупа знак плюс.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "4"))
	FText BuyPriceFormat = NSLOCTEXT("Shop", "BuyPriceFormat", "{Price}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "5"))
	FText SellPriceFormat = NSLOCTEXT("Shop", "SellPriceFormat", "+{Price}");

	// «Не хватает монет» в плитке недоступной покупки (ADR-049: одним потухшим цветом
	// кнопки не обойтись, нужен текст). Переехало со строкового UShopRowWidget (Build 1.2.2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "6"))
	FText NotEnoughMoneyText = NSLOCTEXT("Shop", "NotEnoughMoney", "Не хватает монет");

	// Название брони с прибавкой защиты: {ItemName} — название, {Percent} — прибавка.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "6"))
	FText ArmorBonusFormat = NSLOCTEXT("Shop", "ArmorBonusFormat", "{ItemName} (+{Percent}% защиты)");

	// Заголовок панели количества: {ItemName} — название товара. Слово «Купить»/«Продать»
	// меняется по типу сделки, поэтому в дизайнер уйти не может и живёт здесь.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "7"))
	FText BuyTitleFormat = NSLOCTEXT("Shop", "BuyTitleFormat", "Купить: {ItemName}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "8"))
	FText SellTitleFormat = NSLOCTEXT("Shop", "SellTitleFormat", "Продать: {ItemName}");

	// Количество в сделке: {Qty} — выбрано, {Max} — потолок.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "9"))
	FText QtyFormat = NSLOCTEXT("Shop", "QtyFormat", "{Qty} из {Max}");

	// Пересчёт пачек в патроны: {Rounds} — сколько патронов выйдет всего. Показывается
	// ТОЛЬКО при покупке патронов, в остальных сделках строка прячется целиком.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "10"))
	FText QtyAmmoFormat = NSLOCTEXT("Shop", "QtyAmmoFormat", "всего {Rounds} патронов");

	// Итог сделки: {Total} — сумма. Слово меняется по типу сделки — живёт здесь.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "11"))
	FText TotalFormat = NSLOCTEXT("Shop", "TotalFormat", "Итого: {Total}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Texts", meta = (DisplayPriority = "12"))
	FText RevenueFormat = NSLOCTEXT("Shop", "RevenueFormat", "Выручка: +{Total}");

	// --- Build 1.2: «Продать дороже» за просмотр ролика (ТЗ издателя №2) ---

	// Множитель выручки за просмотр (ТЗ: +50% -> 1.5; удвоение сознательно НЕ берём —
	// оно ломает петлю накопления, ТЗ №2 раздел 2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Ads", meta = (ClampMin = "1.0", DisplayPriority = "1"))
	float SellAdBonusMultiplier = 1.5f;

	// Порог суммы сделки для показа золотой кнопки (ТЗ: 50 монет — защита от «рекламы
	// ради трёх монет»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Ads", meta = (ClampMin = "0.0", DisplayPriority = "2"))
	float SellAdMinTotal = 50.0f;

	// Лимит просмотров точки в календарные сутки (ТЗ: 4).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Ads", meta = (ClampMin = "0", DisplayPriority = "3"))
	int32 SellAdDailyLimit = 4;

	// Кулдаун между просмотрами точки, сек (ТЗ: 3 минуты — защита от «продал по одной
	// шкуре пять раз подряд»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Ads", meta = (ClampMin = "0.0", DisplayPriority = "4"))
	float SellAdCooldownSeconds = 180.0f;

	// Надпись золотой кнопки — КОНКРЕТНЫЕ числа, не проценты (ТЗ №2 раздел 3):
	// {Bonus} — сумма с надбавкой, {Base} — обычная сумма.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Ads", meta = (DisplayPriority = "5"))
	FText SellAdPriceFormat = NSLOCTEXT("Shop", "SellAdPriceFormat", "Продать за {Bonus} вместо {Base}");

	// Вторая строка мелким шрифтом: {Percent} — размер надбавки в процентах.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Ads", meta = (DisplayPriority = "6"))
	FText SellAdSubFormat = NSLOCTEXT("Shop", "SellAdSubFormat", "на {Percent}% больше за просмотр ролика");

	// Строка после досрочного закрытия ролика (ТЗ №2 п.5; у заглушки не случается).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop|Ads", meta = (DisplayPriority = "7"))
	FText AdNotFinishedText = NSLOCTEXT("Shop", "AdNotFinishedText", "Награда даётся за полный просмотр");

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
	UFUNCTION() void HandleSellAdClicked();

	// Результат «ролика» точки «Продать дороже» (IAdService::ShowRewarded).
	void HandleShopAdSuccess();
	void HandleShopAdFail();

	// Клик по плитке (Buy/Sell) — payload в самой плитке.
	void HandleTileAction(UItemTileWidget* Tile);

	// --- Транзакция количества (та же модель, что Canvas-слайдер: BUY по индексу каталога,
	// SELL — стак патронов; прочие предметы продаются сразу без панели) ---

	void ArmBuyTransaction(int32 CatalogIndex);
	void ArmSellTransaction(AMasterInventoryItem* Item);
	void CloseTransaction();

	// Обновить строки панели количества (Кол-во/Итого) и позицию ползунка под текущее Qty.
	void UpdateTransactionTexts();

	// Видимость и надписи золотой кнопки «Продать дороже» по условиям ТЗ №2 п.4 (15 минут,
	// порог суммы, готовность ролика, лимит 4/сутки, кулдаун 3 минуты). Числа живые — от
	// текущей суммы сделки; условия не выполнены — кнопка прячется целиком. События
	// показа/непоказа шлются один раз на транзакцию.
	void UpdateSellAdButton();

	// Установить Qty с клампом 1..Max и обновить панель.
	void SetTransactionQty(int32 NewQty);

	// Пересобрать один список; bBuyList: каталог вендора / продаваемое из рюкзака.
	// Columns — колонок в сетке этого списка. Возвращает число построенных плиток
	// (по нему решается, пуст ли рюкзак — Б8, п.5).
	int32 RebuildList(bool bBuyList, int32 Columns);

	// Б8, п.5. Разложить половины окна под наполнение рюкзака: продавать нечего — товары
	// занимают окно целиком, есть что продать — прежние две половины.
	void UpdateListsLayout(bool bBackpackEmpty);

	// Б8, п.5. Ширина, которая реально достаётся сетке внутри списка, в единицах раскладки
	// интерфейса. Ноль — список ещё ни разу не рисовался (до первого кадра живой геометрии
	// нет). Запас под полосу прокрутки вычтен: она стоит РЯДОМ с содержимым и отъедает ширину.
	float GetListInnerWidth(const UScrollBox* List) const;

	// Б8, п.5. Сколько плиток помещается в ряд при такой ширине. Ширина неизвестна (ноль) —
	// откат на настройку «Колонок до первого замера ширины».
	int32 ComputeColumns(float InnerWidth) const;

	// Б8, п.4. Поднять до MinTouchSize кнопки окна, которые мельче предела. Уменьшать
	// ничего не может — только увеличивает.
	void ApplyMinTouchSize();

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

	// Заголовок половины рюкзака («Рюкзак»). Слово принадлежит Ринату — код его НЕ пишет,
	// а только прячет вместе с самим списком, когда товары растянуты на всё окно (иначе
	// заголовок висел бы над чужими плитками). Нет кубика с таким именем — просто не прячем.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SellHeaderText;

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

	// Значение количества «3 из 10» (подпись «Количество» — твой кубик в дизайнере).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SliderQtyText;

	// Строка пересчёта пачек в патроны ЦЕЛИКОМ: положи внутрь и свою подпись, и значение.
	// Код прячет этот контейнер, когда покупаются не патроны — подпись пропадает вместе
	// с числом. Без контейнера прячется только само число, а подпись осталась бы висеть.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SliderQtyAmmoRow;

	// Значение пересчёта «всего 30 патронов».
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SliderQtyAmmoText;

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

	// --- Build 1.2: золотая кнопка «Продать дороже» (в панели сделки; ТЗ №2) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SellAdButton;

	// «Продать за 225 вместо 150» — живая надпись (код пишет по SellAdPriceFormat).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SellAdText;

	// Вторая строка мелко: «на 50% больше за просмотр ролика» / строка про полный просмотр.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SellAdSubText;

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
	FText TransactionTitle;

	// Защита от рекурсии: SetValue ползунка триггерит OnValueChanged — игнорируем свой же вызов.
	bool bUpdatingSliderFromCode = false;

	// --- Б8, п.5: снимок авторской раскладки половин окна (снимается один раз при
	// инициализации, ДО первой растяжки). Возврат к двум половинам идёт только из него,
	// поэтому расстановка Рината в ассете остаётся единственным источником правды. ---

	FAnchorData BuyListLayout;
	bool bBuyListLayoutCaptured = false;
	ESlateVisibility SellListVisibility = ESlateVisibility::Visible;
	ESlateVisibility SellHeaderVisibility = ESlateVisibility::Visible;

	// Сколько колонок стояло в сетках на момент последней пересборки. Каждый кадр сверяем
	// с тем, сколько помещается СЕЙЧАС: в первом кадре живой ширины ещё нет, а после
	// растяжки товаров на всё окно она меняется. Разошлось — пересобираем один раз,
	// дальше числа совпадают и лишних пересборок не происходит.
	int32 LastBuyColumns = 0;
	int32 LastSellColumns = 0;

	// --- Build 1.2: состояние точки «Продать дороже» ---

	// «Ролик» идёт — защита от двойного клика и от закрытия панели под ним.
	bool bAdInProgress = false;

	// События button_shown / not_shown уже отправлены для ЭТОЙ транзакции (ползунок
	// дёргает UpdateSellAdButton на каждое движение — не спамим аналитику).
	bool bAdShownLogged = false;
	bool bAdNotShownLogged = false;

	// Лимит 4/сутки и кулдаун 3 минуты — снимок на момент открытия сделки (читаются из
	// слота сейва; на каждое движение ползунка диск не дёргаем — меняются они только
	// нашим же просмотром, а он закрывает сделку).
	bool bAdLimitOk = false;
	bool bAdCooldownOk = false;
};
