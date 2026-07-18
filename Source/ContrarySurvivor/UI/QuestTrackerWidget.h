// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuestTrackerWidget.generated.h"

class UTextBlock;

/**
 * Трекер активного квеста на UMG (ADR-048, этап 3): строка «Квест: Шкуры волков —
 * Шкура волка 1/3» в углу экрана. Раскладку WBP_QuestTracker строит Ринат. Живёт на
 * экране всю игру (создаёт HUD в BeginPlay); сам прячется, когда квеста нет или
 * открыт модальный экран (как Canvas-путь: на модалках квест виден в самом диалоге).
 */
UCLASS()
class CONTRARYSURVIVOR_API UQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Настройки (Class Defaults WBP_QuestTracker; владение переехало из HUD — ADR-048) ---

	// Активный квест: «Квест: <название> — <прогресс>».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "1"))
	FString TrackerPrefix = TEXT("Квест: ");

	// Выполненный квест собирается кодом: Prefix + название + (прогресс) + Suffix.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "2"))
	FString DonePrefix = TEXT("Квест выполнен: ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "3"))
	FString DoneSuffix = TEXT(" - вернись к старосте");

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
