// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CorpseLootWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UScrollBox;
class UCorpseLootComponent;
class AMasterInventoryItem;
class APlayerCharacter;

// Клик «Забрать» строки обыска. Параметр — сама строка: подписчик (UCorpseLootWidget)
// читает из неё, предмет это или деньги (паттерн FOnInventoryRowAction).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCorpseLootRowTake, class UCorpseLootRowWidget*);

/**
 * Одна строка окна обыска трупа (Build 1.2.1, ТЗ А1): иконка + название + количество +
 * кнопка «Забрать». Деньги — такая же строка (дефолт Рината: предметы и деньги одним
 * списком). Раскладку WBP_CorpseLootRow может собрать Ринат (кубики по ТОЧНЫМ именам,
 * BindWidgetOptional); БЕЗ ассета строка строит себе кодовое дерево сама
 * (NativeOnInitialized, паттерн этапа F) — окно работает и до генерации WBP.
 */
UCLASS()
class CONTRARYSURVIVOR_API UCorpseLootRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Заполнить строку: иконка (nullptr — прячется), название, количество (<=1 — прячется),
	// подпись кнопки «Забрать».
	void SetupRow(UTexture2D* InIcon, const FText& InName, int32 InCount, const FText& InTakeCaption);

	FOnCorpseLootRowTake OnTakeClicked;

	// Полезная нагрузка строки: предмет трупа ЛИБО признак «это строка денег».
	TWeakObjectPtr<AMasterInventoryItem> Item;
	bool bMoneyRow = false;

	// Формат количества: {Count} — сколько штук в стаке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "1"))
	FText CountFormat = NSLOCTEXT("CorpseLoot", "CountFormat", "x{Count}");

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION() void HandleTakeClicked();

	// --- Кубики WBP_CorpseLootRow (имена ТОЧНЫЕ; нет ассета — кодовое дерево) ---

	// Иконка предмета (у денег и предметов без иконки — Collapsed).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> RowIcon;

	// Название предмета / «Деньги».
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RowNameText;

	// Количество «x3» (стак) или сумма денег; при 1 штуке — прячется.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RowCountText;

	// Кнопка «Забрать» и её подпись (подпись ВНУТРИ кнопки — прячется вместе с ней).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TakeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TakeText;

private:
	// Кодовое дерево-фолбэк, если строка создана без WBP (кубики пустые).
	void BuildFallbackTree();
};

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

	// Класс строки списка. Дефолт — C++-класс (строка строит кодовое дерево сама);
	// Ринат может назначить сюда WBP_CorpseLootRow, когда/если тот появится.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "1"))
	TSubclassOf<UCorpseLootRowWidget> RowWidgetClass;

	// Заголовок окна (в кодовом фолбэке; в WBP кубик TitleText может держать и статичный текст).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "1"))
	FText TitleLabel = NSLOCTEXT("CorpseLoot", "Title", "Обыск трупа");

	// Название строки денег в общем списке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "2"))
	FText MoneyRowLabel = NSLOCTEXT("CorpseLoot", "MoneyRow", "Деньги");

	// Подпись кнопки забора в строке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "3"))
	FText TakeCaption = NSLOCTEXT("CorpseLoot", "Take", "Забрать");

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

	// Клик «Забрать» строки (payload — в строке).
	void HandleRowTake(UCorpseLootRowWidget* Row);

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
