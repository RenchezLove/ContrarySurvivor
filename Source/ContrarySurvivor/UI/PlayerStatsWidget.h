// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
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
 *
 * Скрытие на время главного меню — через базу USelfHidingWidget и ТОЛЬКО извне
 * (AContrarySurvivorHUD::ApplyMainMenuGateToStatsPanel). Почему не сама из NativeTick —
 * см. комментарий у ApplyMainMenuGate: панель, свернувшая саму себя, теряет тик навсегда.
 */
UCLASS()
class CONTRARYSURVIVOR_API UPlayerStatsWidget : public USelfHidingWidget
{
	GENERATED_BODY()

public:
	// Показать/спрятать панель на время главного меню (дефект с телефона 08-09: поверх меню
	// оставались полосы и деньги).
	//
	// ⛔ Зовётся ТОЛЬКО ИЗВНЕ — каждый кадр из AContrarySurvivorHUD::DrawHUD и разом в момент
	// открытия/закрытия меню из контроллера. Сама панель отвечать за это НЕ МОЖЕТ: Slate
	// тикает только отрисовываемые виджеты (UE 5.5: SWidget::Paint — единственное место
	// вызова Tick; в режиме инвалидации FSlateInvalidationRoot::PaintFastPath_UpdateNextWidget
	// пропускает невидимые), поэтому SetVisibility(Collapsed) на самом UUserWidget убивает
	// его собственный NativeTick и панель уже никогда не разворачивается обратно. Именно так
	// панель статов пропала с телефона 08-09 (регресс правки ee20959): в журнале живой сессии
	// диагностика панели напечаталась ровно один раз — в кадре 0, до открытия меню.
	// Прячется СОДЕРЖИМОЕ (корень дерева), сам виджет остаётся живым и тикающим.
	void ApplyMainMenuGate(bool bMainMenuOnScreen);

	// Видно ли сейчас содержимое панели (для диагностики HUD и автотестов).
	bool IsStatsContentVisible() const { return IsContentVisible(); }

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

	// Патроны — ДВА числа, как в STALKER (решение владельца 2026-07-20): сколько в магазине
	// и сколько всего есть у игрока. {InClip} — в магазине оружия, {Total} — всё остальное
	// вместе (запас при оружии + пачки в рюкзаке). Именно {Total} игрок реально может
	// расстрелять: перезарядка сама досыпает патроны из рюкзака в запас
	// (APlayerCharacter::ReloadCurrentWeapon), отдельного действия для этого нет.
	// Раздельные {Reserve} и {Bag} тоже доступны — если захочешь вернуть три числа.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "4"))
	FText AmmoFormat = NSLOCTEXT("PlayerStatsWidget", "AmmoFormat", "{InClip} / {Total}");

	// Патроны в отдельном кубике AmmoBagText (если он есть): те же подстановки, что и выше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "5"))
	FText AmmoBagFormat = NSLOCTEXT("PlayerStatsWidget", "AmmoBagFormat", "{Total}");

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

private:
	// Флаг «Warning рассинхрона оружия уже написан» (WeaponUiSyncLog::ShouldLogDesyncOnce):
	// одна строка на эпизод рассинхрона вместо спама каждый кадр.
	bool bWeaponDesyncLogged = false;

	// Диагностика задачи Г (08-08, «ряд голода исчез после Продолжить»): последний снимок
	// живого состояния кубиков ряда — новая строка в журнале пишется только при его смене.
	FString HungerRowLastSnapshot;
};
