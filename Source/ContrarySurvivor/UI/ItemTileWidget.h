// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemTileWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class AMasterInventoryItem;

// Клики плитки: основное действие (использовать/надеть/купить/продать/забрать — решает
// экран-владелец) и выброс (мини-кнопка, только инвентарь). Параметр — сама плитка:
// подписчик читает из неё payload (Item / CatalogIndex / bMoneyTile).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnItemTileAction, class UItemTileWidget*);

/**
 * ПЛИТКА ПРЕДМЕТА (Build 1.2.2, решение Рината: «в инвентаре, магазине, окнах обыска и
 * вообще везде переходи уже на иконки в сетке. Как в сталкере или LDoE»). Одна плитка =
 * квадратная иконка + цифра количества в правом нижнем углу иконки (только у стака >1) +
 * постоянная подпись-название ПОД иконкой; в магазине под названием цена и строка
 * «не хватает монет».
 *
 * ОДИН класс на все три окна (инвентарь/магазин/обыск) — прежние три строковых класса
 * (UInventoryRowWidget/UShopRowWidget/UCorpseLootRowWidget) выведены из употребления.
 * Плитки — ДИНАМИКА списков (создаёт код экрана, как раньше строки): сетку
 * UUniformGridPanel экран строит внутри своего ScrollBox-кубика.
 *
 * Раскладку WBP_ItemTile генерирует GenerateWbpCommandlet, стиль правит Ринат; кубики
 * цепляются по ТОЧНЫМ именам (BindWidgetOptional). БЕЗ ассета плитка строит кодовое
 * дерево-фолбэк сама (NativeOnInitialized, паттерн UCorpseLootRowWidget) — окна работают
 * и до генерации/назначения WBP.
 */
UCLASS()
class CONTRARYSURVIVOR_API UItemTileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Основные данные плитки: иконка (nullptr — прячется, остаётся подпись), название,
	// количество (цифра в правом нижнем углу иконки; показывается только при >1).
	void SetTileData(UTexture2D* InIcon, const FText& InName, int32 InCount);

	// Цена под названием (магазин). Пусто — кубик прячется (инвентарь/обыск).
	void SetPriceText(const FText& InPrice);

	// Строка статуса под ценой («Не хватает монет», ADR-049). Пусто — прячется.
	void SetStatusText(const FText& InStatus);

	// Доступность основного действия (гаснет кнопка плитки — покупка не по карману).
	void SetActionEnabled(bool bEnabled);

	// Мини-кнопка выброса в углу плитки (только рюкзак; по умолчанию спрятана).
	void SetDropVisible(bool bVisible);

	// Габарит плитки и иконки — задаёт экран-владелец из своих настроек (размер и число
	// колонок настраиваемые — ТЗ). Работает и на WBP, и на кодовом дереве: пишет overrides
	// в SizeBox-кубики по именам.
	void SetTileSize(const FVector2D& InTileSize, float InIconSize);

	FOnItemTileAction OnTileClicked;
	FOnItemTileAction OnDropClicked;

	// --- Payload (читает экран-владелец в обработчиках кликов) ---

	// Предмет плитки (рюкзак/продажа/обыск); слабый — предмет может исчезнуть между кадрами.
	TWeakObjectPtr<AMasterInventoryItem> Item;

	// Покупка: индекс позиции каталога вендора; INDEX_NONE у прочих плиток.
	int32 CatalogIndex = INDEX_NONE;

	// Плитка денег в окне обыска (количество = сумма).
	bool bMoneyTile = false;

	// Формат цифры количества: {Count} — штук в стаке (у денег — сумма).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemTile|Texts", meta = (DisplayPriority = "1"))
	FText CountFormat = NSLOCTEXT("ItemTile", "CountFormat", "x{Count}");

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION() void HandleTileClicked();
	UFUNCTION() void HandleDropClicked();

	// --- Кубики WBP_ItemTile (имена ТОЧНЫЕ; нет ассета — кодовое дерево) ---

	// Габарит плитки целиком (корень).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> TileSizeBox;

	// Кнопка основного действия — вся плитка целиком.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TileButton;

	// Квадрат иконки (габарит иконочной зоны).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> TileIconBox;

	// Иконка предмета (нет текстуры — Collapsed, плитка живёт подписью).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TileIcon;

	// Цифра количества в правом нижнем углу иконки (только у стака >1).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TileCountText;

	// Подпись-название ПОД иконкой (постоянная — решение Рината).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TileNameText;

	// Цена (магазин) под названием.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TilePriceText;

	// «Не хватает монет» и подобное — код показывает только когда нужно.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TileStatusText;

	// Мини-кнопка выброса (верхний правый угол плитки; только рюкзак).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DropButton;

private:
	// Кодовое дерево-фолбэк, если плитка создана без WBP (кубики пустые).
	void BuildFallbackTree();
};
