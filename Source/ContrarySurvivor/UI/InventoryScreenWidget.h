// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContrarySurvivor/UI/ItemPanelMode.h" // EItemPanelMode (ADR-082)
#include "InventoryScreenWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UScrollBox;
class UItemTileWidget;
class APlayerCharacter;
class AMasterWeapon;
class AMasterInventoryItem;
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

	// --- Режим панели (ADR-082): обыск/торговля/(позже) личный ящик — это РЕЖИМЫ этого же
	// окна, а не отдельные экраны. Пока никто не позвал SetPanelMode, окно работает ровно как
	// раньше (Normal) — поведение по умолчанию не меняется. ---

	EItemPanelMode GetPanelMode() const { return PanelMode; }

	// Переключает режим и напоминает, какое окно слева сейчас работает в паре (InPartner —
	// UCorpseLootWidget при Search; при Trade/Stash пока не используется, этап 3/задел).
	// НЕ Normal — кнопка закрытия рюкзака прячется (её роль в паре берёт окно-напарник слева,
	// ТЗ п.10); Normal — кнопка возвращается к видимости, снятой с кубика при инициализации.
	// Партнёра храним слабой ссылкой — окно-напарник может закрыться/умереть раньше рюкзака.
	void SetPanelMode(EItemPanelMode InMode, UObject* InPartner);

	// --- Настройки (Class Defaults WBP_Inventory; владение переехало из HUD — ADR-048) ---

	// Класс ПЛИТКИ рюкзака (Build 1.2.2, тайлы вместо строк): по умолчанию C++-плитка с
	// кодовым деревом; Ринат/генератор назначает сюда WBP_ItemTile для стилизации.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (DisplayPriority = "1",
		DisplayName = "Класс плитки предмета"))
	TSubclassOf<UItemTileWidget> TileWidgetClass;

	// Сетка рюкзака: число колонок и габариты плитки/иконки (настраиваемые — ТЗ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1", DisplayPriority = "2",
		DisplayName = "Колонок в сетке рюкзака"))
	int32 TileColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (DisplayPriority = "3",
		DisplayName = "Размер плитки"))
	FVector2D TileSize = FVector2D(110.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "16.0", DisplayPriority = "4",
		DisplayName = "Размер иконки в плитке"))
	float TileIconSize = 86.0f;

	// Зазоры между плитками (Build 1.2.2, приёмка Рината: ряды слипались по вертикали).
	// Сетку строит код — в дизайнере эти отступы не поменять, поэтому они параметры окна.
	// Значение — расстояние между СОСЕДНИМИ плитками; по краям сетки остаётся половина.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0.0", DisplayPriority = "5",
		DisplayName = "Зазор между плитками по горизонтали"))
	float TileSpacingX = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0.0", DisplayPriority = "6",
		DisplayName = "Зазор между плитками по вертикали"))
	float TileSpacingY = 8.0f;

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

	// Название оружия в слоте (Build 1.2.2 — слота ДВА: огнестрел и холодное, решение
	// Рината): {ItemName} — название предмета.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "6"))
	FText WeaponFormat = NSLOCTEXT("Inventory", "WeaponFormat", "{ItemName}");

	// То же для оружия, которое СЕЙЧАС в руках, — с пометкой (отличать активный слот).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Texts", meta = (DisplayPriority = "7"))
	FText WeaponInHandsFormat = NSLOCTEXT("Inventory", "WeaponInHandsFormat", "{ItemName} — в руках");

	// Подписи «Использовать»/«Надеть» и формат «{ItemName} x{Count}» строкового рюкзака
	// УДАЛЕНЫ (Build 1.2.2, тайлы): действие теперь — клик по самой плитке, а количество —
	// цифра в правом нижнем углу иконки (UItemTileWidget::CountFormat).

protected:
	virtual void NativeOnInitialized() override;

	// Строка статов — каждый кадр (дёшево); paper-doll и рюкзак — при изменении
	// (дешёвая сигнатура состава: число предметов + защита + имя оружия).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleHeadSlotClicked();
	UFUNCTION() void HandleTorsoSlotClicked();
	UFUNCTION() void HandleLegsSlotClicked();
	UFUNCTION() void HandleCloseClicked();

	// Клики плиток рюкзака (payload — в плитке): клик по плитке = использовать/надеть,
	// мини-кнопка в углу = выбросить.
	void HandleTileUse(UItemTileWidget* Tile);
	void HandleTileDrop(UItemTileWidget* Tile);

	// Полная пересборка paper-doll + рюкзака.
	void RefreshAll();

	// Один слот paper-doll: текст (имя надетого / «(пусто)») + иконка надетого предмета
	// (Collapsed, когда пусто — под ней видна статичная подложка Рината из WBP).
	// РАЗМЕР иконки и кегль подписи слота код НЕ задаёт: они живут в самом WBP_Inventory
	// (Brush.ImageSize у картинки, Font у текста) и принадлежат Ринату — с Build 1.2.2 эти
	// кубики разомкнуты в дизайнере, он крутит их сам, а пересборка окна их сохраняет.
	// Параметр НЕ «Slot»: имя шэдоуило бы член UWidget::Slot (C4458 при -WarningsAsErrors).
	void RefreshArmorSlot(EArmorSlot ArmorSlot, UTextBlock* SlotText, UImage* SlotIcon);

	// Один слот оружия (Build 1.2.2): название по WeaponFormat/WeaponInHandsFormat
	// (активный помечается), иконка через GetItemIcon; нет оружия — слово пустого слота.
	void RefreshWeaponSlot(const AMasterWeapon* Weapon, bool bInHands,
		UTextBlock* SlotText, UImage* SlotIcon);

	// Снять броню слота (клик по занятому слоту; пустой — ничего).
	void UnequipSlot(EArmorSlot ArmorSlot);

	// Состав обоих слотов оружия + какое в руках, одной строкой — сигнатура для тика.
	FString MakeWeaponSignature() const;

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

	// Слоты оружия (Build 1.2.2, Ринат: «один слот под холодное оружие и один слот под
	// огнестрельное»): текст названия + иконка на каждый. Экземпляры живут оба
	// (GetRangedWeaponInstance/GetMeleeWeaponInstance), активный помечается
	// WeaponInHandsFormat. Пустой слот — то же слово, что у пустого слота брони.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RangedSlotText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> RangedSlotIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MeleeSlotText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MeleeSlotIcon;

	// Список рюкзака (ScrollBox из WBP; Build 1.2.2 — код кладёт внутрь СЕТКУ плиток).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> BackpackList;

	// Кнопка закрытия экрана.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

private:
	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// Сигнатура последней пересборки (число предметов рюкзака / процент защиты / состав
	// обоих слотов оружия + какой в руках): изменилась — пересобираем. Ловит и внешние
	// изменения (QA-клавиши выдачи предметов).
	int32 LastBackpackCount = -1;
	int32 LastProtectionPct = -1;
	FString LastWeaponName;

	// --- Режим панели (ADR-082) ---

	EItemPanelMode PanelMode = EItemPanelMode::Normal;

	// Окно-напарник текущего режима (UCorpseLootWidget при Search) — слабая ссылка, оно может
	// закрыться/умереть раньше рюкзака.
	TWeakObjectPtr<UObject> PanelPartner;

	// Видимость CloseButton «как нарисовал Ринат», снятая с кубика ОДИН РАЗ при инициализации
	// (тот же приём, что цвет покоя кнопки БЕГ в TouchControlsWidget::SprintIdleColor) —
	// возвращаем её при выходе обратно в Normal, а не жёсткий Visible.
	ESlateVisibility CloseButtonShownVisibility = ESlateVisibility::Visible;

	// Защита от двойного клика по одной и той же плитке, пока перенос предмета (режим Search)
	// не завершён: повтор по тому же предмету, пока эта ссылка на него ещё держится, игнорируем.
	TWeakObjectPtr<AMasterInventoryItem> PendingTransferItem;
};
