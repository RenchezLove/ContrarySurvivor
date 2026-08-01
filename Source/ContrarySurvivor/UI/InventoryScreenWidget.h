// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UScrollBox;
class UInventoryRowWidget;
class APlayerCharacter;
enum class EArmorSlot : uint8;

/**
 * Экран инвентаря на UMG (ADR-048, этап 2). Логика — здесь; раскладку WBP_Inventory
 * строит Ринат по схеме docs/contrary-survivor/umg-layout-guide.md (кубики по ТОЧНЫМ
 * именам; BindWidgetOptional — предупреждение в лог, не краш).
 *
 * Содержимое — как Canvas DrawInventory: строка статов, paper-doll (3 слота брони,
 * клик по занятому = снять), «Защита: N%», слот оружия, рюкзак-список со «использовать/
 * надеть» и выбросом. Действия — существующие Inv_UnequipSlot / Inv_UseBackpackItem /
 * Inv_DropItem игрока (расчёты не дублируются). Закрытие — делегат OnCloseRequested
 * (подписан контроллер: тот же путь, что клавиша Tab).
 */
UCLASS()
class CONTRARYSURVIVOR_API UInventoryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязка данных после CreateWidget (игрок) и первая сборка.
	void InitInventory(APlayerCharacter* InPlayer);

	// Кнопка закрытия нажата — подписан контроллер (переключение инвентаря, как Tab).
	FSimpleMulticastDelegate OnCloseRequested;

	// --- Настройки (Class Defaults WBP_Inventory; владение переехало из HUD — ADR-048) ---

	// Класс строки рюкзака: Ринат назначает сюда WBP_InventoryRow.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (DisplayPriority = "1"))
	TSubclassOf<UInventoryRowWidget> RowWidgetClass;

	// Форматы ЗНАЧЕНИЙ. Подписи («Монеты», «Голод», «Жажда», «Защита», «Оружие») —
	// статичные кубики в дизайнере, код их НЕ пишет (ADR-050). Раньше все три стата
	// были слеплены в ОДИН кубик StatsText — теперь у каждого свой.

	// Монеты: {Amount} — сколько у игрока.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "1"))
	FText MoneyFormat = NSLOCTEXT("Inventory", "MoneyFormat", "{Amount}");

	// Голод: {Current} — сейчас, {Max} — максимум шкалы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "2"))
	FText HungerFormat = NSLOCTEXT("Inventory", "HungerFormat", "{Current} из {Max}");

	// Жажда: {Current} — сейчас, {Max} — максимум шкалы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "3"))
	FText ThirstFormat = NSLOCTEXT("Inventory", "ThirstFormat", "{Current} из {Max}");

	// Пустой слот брони и пустые руки — одно слово на всё (лист текстов).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "4"))
	FText EmptySlotText = NSLOCTEXT("Inventory", "EmptySlot", "Пусто");

	// Защита: {Percent} — процент снижения урона.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "5"))
	FText ProtectionFormat = NSLOCTEXT("Inventory", "ProtectionFormat", "{Percent}%");

	// Оружие в руках: {ItemName} — название предмета.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "6"))
	FText WeaponFormat = NSLOCTEXT("Inventory", "WeaponFormat", "{ItemName}");

	// Подписи кнопки применения в строках рюкзака. В дизайнер уйти НЕ могут: слово
	// зависит от предмета — расходник применяют, броню надевают.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "7"))
	FText UseHintConsumable = NSLOCTEXT("Inventory", "UseHintConsumable", "Использовать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "8"))
	FText UseHintArmor = NSLOCTEXT("Inventory", "UseHintArmor", "Надеть");

	// Название строки-СТАКА (Build 1.2.1, ТЗ Г): {ItemName} — название, {Count} — штук в
	// стаке. Применяется только при количестве больше 1 (одиночный предмет — как раньше).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "9"))
	FText StackNameFormat = NSLOCTEXT("Inventory", "StackNameFormat", "{ItemName} x{Count}");

protected:
	virtual void NativeOnInitialized() override;

	// Строка статов — каждый кадр (дёшево); paper-doll и рюкзак — при изменении
	// (дешёвая сигнатура состава: число предметов + защита + имя оружия).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleHeadSlotClicked();
	UFUNCTION() void HandleTorsoSlotClicked();
	UFUNCTION() void HandleLegsSlotClicked();
	UFUNCTION() void HandleCloseClicked();

	// Клики строк рюкзака (payload — в строке).
	void HandleRowUse(UInventoryRowWidget* Row);
	void HandleRowDrop(UInventoryRowWidget* Row);

	// Полная пересборка paper-doll + рюкзака.
	void RefreshAll();

	// Один слот paper-doll: текст (имя надетого / «(пусто)») + иконка надетого предмета
	// (Collapsed, когда пусто — под ней видна статичная подложка Рината из WBP).
	// Параметр НЕ «Slot»: имя шэдоуило бы член UWidget::Slot (C4458 при -WarningsAsErrors).
	void RefreshArmorSlot(EArmorSlot ArmorSlot, UTextBlock* SlotText, UImage* SlotIcon);

	// Снять броню слота (клик по занятому слоту; пустой — ничего).
	void UnequipSlot(EArmorSlot ArmorSlot);

	// --- Кубики WBP_Inventory (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Значения статов — ТРИ отдельных кубика (обновляются каждый кадр). Раньше был один
	// StatsText со всеми тремя парами «подпись плюс число» внутри — Ринату надо переложить
	// эту строку заново, схема в umg-layout-guide.md.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InvMoneyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InvHungerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InvThirstText;

	// Слоты paper-doll: кнопка (клик по занятому — снять), подпись, иконка надетого.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HeadSlotButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeadSlotText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeadSlotIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TorsoSlotButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TorsoSlotText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TorsoSlotIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LegsSlotButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LegsSlotText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LegsSlotIcon;

	// «Защита: 45%» — фактическое снижение урона (с капом), пересчитывает код.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProtectionText;

	// «Оружие: Пистолет».
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponText;

	// Список рюкзака (наполняется строками WBP_InventoryRow; прокрутка штатная).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> BackpackList;

	// Кнопка закрытия экрана.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

private:
	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// Сигнатура последней пересборки (число предметов рюкзака / процент защиты / имя оружия):
	// изменилась — пересобираем. Ловит и внешние изменения (QA-клавиши выдачи предметов).
	int32 LastBackpackCount = -1;
	int32 LastProtectionPct = -1;
	FString LastWeaponName;
};
