// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CorpseLootWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UScrollBox;
class UTexture2D;
class UCorpseLootComponent;
class AMasterInventoryItem;
class APlayerCharacter;
class UItemTileWidget;

// Строковый UCorpseLootRowWidget УДАЛЁН (Build 1.2.2): обыск, как инвентарь и магазин,
// перешёл на общую плитку UItemTileWidget («иконки в сетке, как в сталкере или LDoE»);
// клик по плитке = прежняя кнопка «Забрать», деньги — плитка с иконкой T_Item_Money.

/**
 * Окно обыска трупа (Build 1.2.1, ТЗ А1; Ринат: «Открывается окно (похожее немного на
 * экран торговли) и игрок выбирает что из лута трупа себе в инвентарь добавить. Как в
 * сталкере, LDoE»).
 *
 * Содержимое: список предметов трупа (иконка+название+количество) И деньги ОДНИМ списком
 * (дефолт Рината), забор кликом по строке («Забрать»), кнопка «Забрать всё», закрытие —
 * крестик/Esc (Esc ведёт контроллер). Частичный обыск штатен: остаток лежит в трупе до
 * таймера исчезновения.
 *
 * Build 1.2.2 (Ринат про BP_Picup: «механика похожая на ту, что я обыскиваю ящик или труп
 * и выбираю что себе положить в инвентарь»): это же окно показывает содержимое мешка-пикапа
 * (APickup несёт такой же UCorpseLootComponent). Отличие только в поведении источника:
 * обысканный до конца мешок исчезает, и окно закрывается само (контейнер умер — NativeTick
 * просит закрытия). Заголовок берётся у контейнера (SearchTitle), поэтому над мешком не
 * висит надпись про труп.
 *
 * Архитектура — ADR-048, по образцу UShopScreenWidget: логика здесь, раскладку
 * WBP_CorpseLoot (канвас-схема) строит Ринат/генератор; кубики цепляются по ТОЧНЫМ
 * именам через BindWidgetOptional. ЗАПАСНОЙ ВИД: если виджет создан БЕЗ ассета (слот
 * класса на HUD пуст), NativeOnInitialized строит дерево кодом (паттерн этапа F /
 * UEndOfStoryWidget) — механика работает и до генерации/назначения WBP.
 *
 * Закрытие — делегат OnCloseRequested: мир/режим ввода возвращает контроллер
 * (CloseCorpseLoot), сам виджет их не трогает (как магазин).
 */
UCLASS()
class CONTRARYSURVIVOR_API UCorpseLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязка данных после CreateWidget (труп + игрок) и первая сборка списка.
	void InitCorpseLoot(UCorpseLootComponent* InCorpse, APlayerCharacter* InPlayer);

	// Крестик нажат / труп исчез под открытым окном — подписан контроллер (CloseCorpseLoot).
	FSimpleMulticastDelegate OnCloseRequested;

	// --- Настройки (Class Defaults WBP_CorpseLoot) ---

	// Класс ПЛИТКИ списка (Build 1.2.2, тайлы): дефолт — C++-плитка с кодовым деревом;
	// Ринат/генератор назначает сюда WBP_ItemTile для стилизации.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "1",
		DisplayName = "Класс плитки предмета"))
	TSubclassOf<UItemTileWidget> TileWidgetClass;

	// Сетка лута: число колонок и габариты плитки/иконки (настраиваемые — ТЗ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (ClampMin = "1", DisplayPriority = "2",
		DisplayName = "Колонок в сетке лута"))
	int32 TileColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "3",
		DisplayName = "Размер плитки"))
	FVector2D TileSize = FVector2D(110.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (ClampMin = "16.0", DisplayPriority = "4",
		DisplayName = "Размер иконки в плитке"))
	float TileIconSize = 86.0f;

	// Иконка плитки денег (у денег нет класса-предмета — иконку задаёт окно).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "5",
		DisplayName = "Иконка плитки денег"))
	TSoftObjectPtr<UTexture2D> MoneyRowIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/UI/Icons/Items/T_Item_Money.T_Item_Money")));

	// Заголовок окна (в кодовом фолбэке; в WBP кубик TitleText может держать и статичный текст).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "1"))
	FText TitleLabel = NSLOCTEXT("CorpseLoot", "Title", "Обыск трупа");

	// Название строки денег в общем списке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "2"))
	FText MoneyRowLabel = NSLOCTEXT("CorpseLoot", "MoneyRow", "Деньги");

	// Подпись «Забрать» строкового списка УДАЛЕНА (Build 1.2.2): забор — клик по плитке.

	// Подпись кнопки «Забрать всё».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "4"))
	FText TakeAllCaption = NSLOCTEXT("CorpseLoot", "TakeAll", "Забрать всё");

	// Подпись крестика (кодовый фолбэк; в WBP Ринат волен нарисовать свой крестик).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "5"))
	FText CloseCaption = NSLOCTEXT("CorpseLoot", "Close", "X");

	// Строка-заглушка пустого трупа (всё забрано, труп лежит до таймера).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "6"))
	FText EmptyLabel = NSLOCTEXT("CorpseLoot", "Empty", "Пусто");

protected:
	virtual void NativeOnInitialized() override;

	// Труп исчез по таймеру под открытым окном — окно честно просит закрытия (один раз).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleTakeAllClicked();
	UFUNCTION() void HandleCloseClicked();

	// Клик по плитке = забрать (payload — в плитке).
	void HandleTileTake(UItemTileWidget* Tile);

	// Полная пересборка списка по текущему содержимому трупа.
	void RefreshList();

	// --- Кубики WBP_CorpseLoot (имена ТОЧНЫЕ; нет ассета — кодовое дерево) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	// Список лута (деньги + предметы одним списком, прокрутка штатная).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> LootList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TakeAllButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TakeAllText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseText;

private:
	// Кодовое дерево-фолбэк, если окно создано без WBP.
	void BuildFallbackTree();

	// Забрать один предмет трупа в рюкзак игрока. true — предмет ушёл игроку.
	bool TakeItemToBackpack(AMasterInventoryItem* TakenItem);

	// Забрать деньги трупа на баланс игрока.
	void TakeMoneyToPlayer();

	// Труп (слабая ссылка: исчезает по таймеру независимо от окна) и игрок.
	TWeakObjectPtr<UCorpseLootComponent> Corpse;

	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// Закрытие уже запрошено (труп исчез) — не спамим делегат каждый тик.
	bool bCloseRequested = false;
};
