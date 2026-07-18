// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ContrarySurvivor/UI/DailyRewardWidget.h" // FDailyRewardStyle (стиль окна)
#include "DailyRewardComponent.generated.h"

/**
 * Ежедневная награда за вход (Этап F2, ADR-044 п.4). Живёт на APlayerCharacter.
 *
 * При старте сессии (BeginPlay + ShowWindowDelay) сравнивает локальную календарную дату
 * с датой последнего входа из сейва (UContrarySaveGame): дата сменилась — начисляет монеты
 * в UStatsComponent, зеркалит их в поле Money сейва (чтобы награда пережила смерть/загрузку),
 * пишет новую дату/серию в слот и показывает окно UDailyRewardWidget. Тот же календарный
 * день — тихо ничего не делает. Все числа настраиваются в редакторе.
 */
UCLASS(ClassGroup = (Retention), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UDailyRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDailyRewardComponent();

	// День 1 серии (ADR-044: 25 монет).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "0.0", DisplayPriority = "1"))
	float BaseReward = 25.0f;

	// Прибавка за каждый следующий день серии (ADR-044: +10).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "0.0", DisplayPriority = "2"))
	float RewardStepPerDay = 10.0f;

	// Потолок суммы (ADR-044: 75).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "0.0", DisplayPriority = "3"))
	float MaxReward = 75.0f;

	// Задержка проверки/окна после старта уровня (сек) — даём миру дорисоваться.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "0.0", DisplayPriority = "4"))
	float ShowWindowDelay = 0.8f;

	// Стиль окна (цвета/тексты/шрифты) — применяется при создании виджета
	// (директива Рината 07-18: настройка в BP_PlayerCharacter без пересборки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (DisplayPriority = "5"))
	FDailyRewardStyle WindowStyle;

protected:
	virtual void BeginPlay() override;

private:
	// Проверка даты + начисление + запись в сейв + показ окна. Зовётся таймером из BeginPlay.
	void EvaluateDailyReward();

	// Кнопка «Забрать»: вернуть игровой режим ввода (если не открыт другой модальный экран).
	void HandleWindowClosed();

	UPROPERTY()
	TObjectPtr<UDailyRewardWidget> ActiveWindow;

	FTimerHandle EvaluateTimer;
};
