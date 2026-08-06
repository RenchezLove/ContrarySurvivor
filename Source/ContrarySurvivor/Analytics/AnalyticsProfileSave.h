// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AnalyticsProfileSave.generated.h"

/**
 * Служебная память аналитики на УСТАНОВКУ игры (Б4, задание издателя ADR-059).
 *
 * Лежит в ОТДЕЛЬНОМ слоте сохранения ('ContraryAnalytics'), а не в игровом 'ContrarySave',
 * специально: кнопка «Новая игра» и отладочная очистка (F12) стирают именно игровой слот
 * (APlayerCharacter::ResetToNewGame, AContrarySurvivorPlayerController), а признак «игра уже
 * запускалась на этом устройстве» переживать новую игру ОБЯЗАН — новая игра это не новая
 * установка. Слот пропадает только вместе с данными приложения (переустановка/очистка данных),
 * а это ровно и есть «новая установка».
 *
 * Здесь же ведётся защита от повторной отправки событий обучения: издателю нужна воронка
 * «установка → шаги обучения → завершение», в которой каждое событие приходит не более
 * одного раза за установку, даже если игрок начал игру заново.
 */
UCLASS()
class CONTRARYSURVIVOR_API UAnalyticsProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	// Событие первого запуска уже отправлено.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	bool bFirstLaunchReported = false;

	// Событие завершения обучения уже отправлено.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	bool bTutorialCompletedReported = false;

	// Имена шагов обучения, о которых уже отправлено событие (movement/pickup/elder/...).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	TArray<FString> ReportedTutorialSteps;

	// --- Пометки «отправлено». Возвращают true ТОЛЬКО в первый раз: вызывающий по этому
	// признаку решает, слать событие или промолчать. Диска не касаются — запись слота
	// делает UAnalyticsSubsystem, чтобы место записи было одно. ---

	bool MarkFirstLaunchReported();
	bool MarkTutorialStepReported(const FString& StepId);
	bool MarkTutorialCompletedReported();

	// Сколько разных шагов обучения уже отмечено (для проверки «пройдены все»).
	int32 GetReportedTutorialStepCount() const { return ReportedTutorialSteps.Num(); }
};
