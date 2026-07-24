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
 *
 * Build 1 (решение Рината 07-24): если игроку ещё предстоит интро первой встречи со старостой
 * (признак bElderHookShown в сейве не стоит), окно при входе НЕ показывается — оно портило
 * атмосферу интро. Проверка и показ откладываются до закрытия диалога старосты (контроллер
 * зовёт NotifyElderDialogClosed) + задержка PostIntroShowDelay. Если в момент отложенного
 * показа открыт другой модальный экран (инвентарь/магазин/смерть/пауза) — показ не отменяется,
 * а повторяется по таймеру DeferredRetryDelay, пока экраны не освободятся (баг живого PIE
 * 07-24: ожидание СЛЕДУЮЩЕГО диалога старосты теряло баннер). Интро уже было — прежнее
 * поведение.
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

	// Задержка окна после закрытия интро-диалога старосты (сек), когда показ был отложен
	// из-за предстоящего интро (Build 1, решение Рината 07-24).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "0.0", DisplayPriority = "5"))
	float PostIntroShowDelay = 1.0f;

	// Интервал повторных попыток отложенного показа (сек), когда в момент показа открыт
	// другой модальный экран. Попытки идут, пока экраны не освободятся.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (ClampMin = "0.1", DisplayPriority = "6"))
	float DeferredRetryDelay = 1.0f;

	// Стиль окна (цвета/тексты/шрифты) — применяется при создании виджета
	// (директива Рината 07-18: настройка в BP_PlayerCharacter без пересборки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DailyReward", meta = (DisplayPriority = "7"))
	FDailyRewardStyle WindowStyle;

	// Зовёт контроллер при КАЖДОМ закрытии диалога старосты (Build 1): если окно награды было
	// отложено из-за интро — показать его теперь (через PostIntroShowDelay). Без отложенного
	// окна — тихий no-op.
	void NotifyElderDialogClosed();

protected:
	virtual void BeginPlay() override;

private:
	// Первый шаг после старта уровня (таймер BeginPlay): интро первой встречи ещё впереди —
	// отложить окно до закрытия диалога старосты, иначе прежний путь (EvaluateDailyReward).
	void EvaluateOrDefer();

	// Отложенный показ после закрытия диалога. Если открыт другой модальный экран (например,
	// инвентарь или экран смерти) — не лезем поверх, но и не бросаем: сами повторяем попытку
	// по таймеру DeferredRetryDelay, пока экраны не освободятся (следующего диалога старосты
	// может не быть — прежнее ожидание его теряло баннер, живой PIE 07-24).
	void HandleDeferredShow();

	// Проверка даты + начисление + запись в сейв + показ окна. Зовётся таймером из BeginPlay.
	// Контроллер и окно готовятся ДО начисления и записи даты: показ не состоялся — день в
	// сейве не помечен выданным, награда не сгорает молча.
	void EvaluateDailyReward();

	// Кнопка «Забрать»: вернуть игровой режим ввода (если не открыт другой модальный экран).
	void HandleWindowClosed();

	UPROPERTY()
	TObjectPtr<UDailyRewardWidget> ActiveWindow;

	FTimerHandle EvaluateTimer;

	// Окно отложено до закрытия диалога старосты (интро первой встречи ещё впереди).
	bool bAwaitingElderDialog = false;
};
