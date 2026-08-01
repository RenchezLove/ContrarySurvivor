// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathScreenWidget.generated.h"

class UTextBlock;
class UButton;
class UPanelWidget;
class APlayerCharacter;

/**
 * Экран смерти на UMG (ADR-048, этап 3). Раскладку WBP_Death строит Ринат по схеме
 * docs/contrary-survivor/umg-layout-guide.md (кубики по ТОЧНЫМ именам; BindWidgetOptional —
 * предупреждение, не краш). Статичные строки (заголовок «ВЫ ПОГИБЛИ», строки штрафа по
 * ADR-027/ADR-044, подсказка клавиш) Ринат пишет прямо в WBP; код ставит только живые
 * строки статистики. Возрождение — существующий APlayerCharacter::Respawn (кнопка) и
 * прежние клавиши Enter/Пробел (путь контроллера не тронут).
 *
 * Build 1.2 (ТЗ издателя №1 + переопределение Рината): блок «Будет потеряно» (до 8 позиций
 * иконками с количеством + «и ещё N предметов» + строка денег) и золотая кнопка «Спасти
 * рюкзак» с иконкой видео. Пока экран открыт — НИЧЕГО не списано; выбор кнопки применяет
 * план потерь (Respawn(bBackpackRescued)). Кнопка рекламы показывается только при
 * выполнении ВСЕХ условий ТЗ (15 минут игрового времени, есть что спасать, ролик готов,
 * лимит 3/сутки); иначе — прячется целиком, без серых заглушек. Таймера обратного
 * отсчёта нет (ТЗ №1 раздел 2, прямой запрет).
 */
UCLASS()
class CONTRARYSURVIVOR_API UDeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязка игрока после CreateWidget. Зовётся HUD при КАЖДОМ показе экрана — здесь же
	// пересобирается превью потерь и решается видимость кнопки «Спасти рюкзак».
	void InitDeath(APlayerCharacter* InPlayer);

	// --- Настройки (Class Defaults WBP_Death; владение переехало из HUD — ADR-048) ---

	// Форматы ЗНАЧЕНИЙ. Подписи («Прожито», «Убийца», «Монеты», «Квестов выполнено»,
	// «Врагов убито») — статичные кубики в дизайнере, код их НЕ пишет (ADR-050).

	// Время последней жизни: {Minutes} — минуты, {Seconds} — секунды (обе с ведущим нулём).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "1"))
	FText LifetimeFormat = NSLOCTEXT("Death", "LifetimeFormat", "{Minutes}:{Seconds}");

	// Кто убил: {Name} — имя убийцы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "2"))
	FText KillerFormat = NSLOCTEXT("Death", "KillerFormat", "{Name}");

	// Монеты: {Amount} — сколько осталось.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "3"))
	FText MoneyFormat = NSLOCTEXT("Death", "MoneyFormat", "{Amount}");

	// Сдано квестов: {Count}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "4"))
	FText QuestsFormat = NSLOCTEXT("Death", "QuestsFormat", "{Count}");

	// Убито врагов: {Count}.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "5"))
	FText KillsFormat = NSLOCTEXT("Death", "KillsFormat", "{Count}");

	// Строка штрафа — цельная фраза, в подпись и значение не делится. {Percent} — живой
	// процент из настроек игрока, чтобы текст не расходился с фактической потерей.
	// Build 1.2: потеря применяется только при возрождении БЕЗ просмотра ролика.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Texts", meta = (DisplayPriority = "6", MultiLine = "true"))
	FText MoneyLossFormat = NSLOCTEXT("Death", "MoneyLossFormat",
		"−{Percent}% монет — если возродиться без просмотра ролика.");

	// --- Build 1.2: блок «Будет потеряно» + кнопка «Спасти рюкзак» ---

	// Количество у позиции превью: {Count} — сколько штук этого предмета теряется.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (DisplayPriority = "1"))
	FText LossCountFormat = NSLOCTEXT("Death", "LossCountFormat", "×{Count}");

	// Свёрнутый хвост списка: {Count} — сколько позиций не поместилось в сетку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (DisplayPriority = "2"))
	FText LossMoreFormat = NSLOCTEXT("Death", "LossMoreFormat", "и ещё {Count} предметов");

	// Строка потери денег в блоке: {Amount} — сколько монет теряется без ролика.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (DisplayPriority = "3"))
	FText LossMoneyFormat = NSLOCTEXT("Death", "LossMoneyFormat", "−{Amount} монет");

	// Живая подстрока золотой кнопки: конкретная выгода числами (ТЗ раздел 0 п.3).
	// {Items} — сколько предметов сохранит просмотр, {Money} — сколько монет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (DisplayPriority = "4"))
	FText SaveBackpackSubFormat = NSLOCTEXT("Death", "SaveBackpackSubFormat",
		"Сохранить {Items} предм. и {Money} монет — за просмотр ролика");

	// Живая подстрока кнопки «Возродиться»: {Items} — предметов теряется, {Money} — монет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (DisplayPriority = "5"))
	FText RespawnSubFormat = NSLOCTEXT("Death", "RespawnSubFormat",
		"Потеряешь {Items} предм. и {Money} монет");

	// Строка после досрочного закрытия ролика (ТЗ №1 п.4; у заглушки не случается).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (DisplayPriority = "6"))
	FText AdNotFinishedText = NSLOCTEXT("Death", "AdNotFinishedText",
		"Награда даётся за полный просмотр");

	// Сколько позиций превью помещается в сетку (ТЗ №1 раздел 2: до 8, остальное — «и ещё N»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (ClampMin = "1", ClampMax = "16", DisplayPriority = "7"))
	int32 MaxLossPreviewEntries = 8;

	// Размер иконки позиции превью (px).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death|Loss", meta = (ClampMin = "16.0", DisplayPriority = "8"))
	float LossIconSize = 44.0f;

protected:
	virtual void NativeOnInitialized() override;

	// Живые строки — каждый кадр (как Canvas DrawDeathScreen; дёшево).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleRespawnClicked();
	UFUNCTION() void HandleSaveBackpackClicked();

	// Результат «ролика» заглушки/сети (IAdService::ShowRewarded).
	void HandleAdSuccess();
	void HandleAdFail();

	// Пересобирает блок «Будет потеряно» и подстроки кнопок; решает видимость золотой
	// кнопки по условиям ТЗ и шлёт события показа/непоказа в аналитику. Зовёт InitDeath.
	void RefreshLossPreview();

	// --- Кубики WBP_Death (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LifetimeText;   // «Прожито:  02:31»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KillerText;     // «Убийца:  Волк»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyText;      // «Монеты:  120»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuestsText;     // «Квестов выполнено:  2»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KillsText;      // «Врагов убито:  7»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyLossText;  // «−50% монет — если возродиться без просмотра ролика.»

	// Кнопка возрождения; подпись («ВОЗРОДИТЬСЯ») Ринат пишет внутри сам.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RespawnButton;

	// Подстрока кнопки возрождения — живые числа потерь (код пишет по RespawnSubFormat).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RespawnSubText;

	// --- Build 1.2: блок «Будет потеряно» ---

	// Контейнер блока целиком (заголовок-кубик Рината + сетка + хвост): прячется, когда
	// терять нечего. Любой панельный/обычный виджет.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LossPanel;

	// Сетка позиций (HorizontalBox/WrapBox — код только добавляет детей).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> LossGrid;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LossMoreText;   // «и ещё 3 предметов»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LossMoneyText;  // «−120 монет»

	// Золотая кнопка «Спасти рюкзак» (иконка видео и заголовок — в ассете).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveBackpackButton;

	// Живая подстрока золотой кнопки (выгода числами / строка «за полный просмотр»).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SaveBackpackSubText;

private:
	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// «Ролик» уже показывается — защита от двойного клика по золотой кнопке.
	bool bAdInProgress = false;
};
