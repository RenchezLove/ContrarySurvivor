// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "QuestTrackerWidget.generated.h"

class UTextBlock;

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
	FText TrackerFormat = NSLOCTEXT("QuestTracker", "TrackerFormat", "Квест: {Title} — {Objectives}");

	// У выполненного квеста прогресс не показываем: он уже 3 из 3 и только удлиняет строку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "2"))
	FText DoneFormat = NSLOCTEXT("QuestTracker", "DoneFormat",
		"Квест выполнен: {Title} — вернись к старосте");

	// Одна цель квеста: {Objective} — что сделать, {Done} — сделано, {Total} — сколько надо.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "3"))
	FText ObjectiveFormat = NSLOCTEXT("Quest", "ObjectiveFormat", "{Objective} {Done} из {Total}");

	// Между двумя целями, когда их у квеста две.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "4"))
	FText ObjectiveSeparator = NSLOCTEXT("Quest", "ObjectiveSeparator", ", ");

	// Цвета строки: активный квест — золотой, выполненный — зелёный (как Canvas).
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
