// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "QuestTrackerWidget.generated.h"

class UTextBlock;
enum class EElderNotebookWaitPhase : uint8;

/**
 * Трекер активного квеста на UMG (ADR-048, этап 3): строка «Квест: Шкуры волков —
 * Шкура волка 1/3» в углу экрана. Раскладку WBP_QuestTracker строит Ринат. Живёт на
 * экране всю игру (создаёт HUD в BeginPlay); сам прячется, когда квеста нет или
 * открыт модальный экран (как Canvas-путь: на модалках квест виден в самом диалоге) —
 * через SetContentVisible базы (Collapsed на себе остановил бы тик навсегда, баг 07-18).
 */
UCLASS()
class CONTRARYSURVIVOR_API UQuestTrackerWidget : public USelfHidingWidget
{
	GENERATED_BODY()

public:
	// --- Настройки (Class Defaults WBP_QuestTracker; владение переехало из HUD — ADR-048) ---

	// Строка трекера собирается кодом целиком: слово «Квест» меняется на «Квест выполнен»
	// по состоянию, поэтому в отдельную статичную подпись оно уйти не может (ADR-050).
	// Подстановки: {Title} — название квеста, {Objectives} — строка целей.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "1"))
	FText TrackerFormat = NSLOCTEXT("QuestTracker", "TrackerFormat", "Квест: {Objectives}");

	// У выполненного квеста прогресс не показываем: он уже 3 из 3 и только удлиняет строку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "2"))
	FText DoneFormat = NSLOCTEXT("QuestTracker", "DoneFormat",
		"Квест выполнен: {Title} — вернись к старосте");

	// Готовое задание, которое сдаётся НЕ старосте (10.10.2026: ноутбук — учёным, рация —
	// охраннику): {Hint} — подсказка из самого задания («отнеси ноутбук учёным»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "2"))
	FText DoneWithHintFormat = NSLOCTEXT("QuestTracker", "DoneWithHintFormat", "Квест: {Title} — {Hint}");

	// Одна цель квеста: {Objective} — что сделать, {Done} — сделано, {Total} — сколько надо.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "3"))
	FText ObjectiveFormat = NSLOCTEXT("Quest", "ObjectiveFormat", "{Objective} {Done} из {Total}");

	// Между двумя целями, когда их у квеста две.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "4"))
	FText ObjectiveSeparator = NSLOCTEXT("Quest", "ObjectiveSeparator", ", ");

	// --- Ожидание после сдачи ноутбука старосте (Ринат 10.10.2026) ---
	// Пока староста «разбирается с ноутбуком», в этом же блоке стоит строка с оставшимся
	// временем: {Minutes} — сколько минут осталось (вверх до целой, не меньше одной).
	// Черновик, правится здесь. Перенос строки внутри текста — намеренный: строка длинная.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "5", MultiLine = "true"))
	FText ElderWaitFormat = NSLOCTEXT("QuestTracker", "ElderWaitFormat",
		"Староста разбирается с ноутбуком — осталось около {Minutes} мин.\nПока можно заработать на волчьих шкурах.");

	// Время вышло — строка держится, пока следующее задание у старосты не взято. Черновик.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "6", MultiLine = "true"))
	FText ElderWaitReadyText = NSLOCTEXT("QuestTracker", "ElderWaitReady",
		"Староста зовёт: есть новости про ноутбук.");

	// Строка ожидания по её состоянию (пусто — показывать нечего). Чистая функция: её же
	// зовёт запасная отрисовка экрана игры и проверяют автотесты.
	static FText BuildElderWaitLine(EElderNotebookWaitPhase Phase, float RemainingSeconds,
		const FText& WaitFormat, const FText& ReadyText);

	// Цвета строки. С 10.10.2026 идущее задание красится цветом своего вида (основное или
	// побочное — UContraryPdaSettings), поэтому ActiveColor больше не применяется; выполненное
	// по-прежнему зелёное.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker", meta = (DisplayPriority = "4"))
	FLinearColor ActiveColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker", meta = (DisplayPriority = "5"))
	FLinearColor DoneColor = FLinearColor(0.4f, 1.0f, 0.4f, 1.0f);

protected:
	// Каждый кадр: текст/цвет/видимость (дёшево; модалки и прогресс меняются на лету).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Кубики WBP_QuestTracker (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Строка трекера. Видимость и цвет ставит код.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TrackerText;
};
