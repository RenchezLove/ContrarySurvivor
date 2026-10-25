// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Debug/QADebug.h" // CONTRARY_WITH_QA_CHEATS

/**
 * ВЫЗОВ МЕНЮ ОТЛАДКИ НА ТЕЛЕФОНЕ (Ринат 25.10.2026: «Добавь пока способ вызова меню отладки в
 * мобильной версии»). Касание экрана тремя пальцами разом открывает меню, повторное — закрывает
 * (то же, что клавиша «]» / «ъ» на ПК).
 *
 * Как отличаем жест от игры:
 *  - считаются только касания, начавшиеся почти одновременно (в пределах окна
 *    ThreeFingerTapWindowSeconds). Стик и кнопка огня, зажатые давно, плюс короткое нажатие
 *    третьим пальцем (перезарядка) жестом НЕ считаются;
 *  - двух касаний мало при любом раскладе;
 *  - третье касание, которое замкнуло жест, игре не отдаётся (не нажмёт кнопку под пальцем),
 *    его движение и отпускание тоже; первые два касания игра уже получила — их зажатый стик и
 *    огонь контроллер сбрасывает сразу при срабатывании жеста;
 *  - после срабатывания касания, начавшиеся раньше, повторно не считаются (четвёртый палец не
 *    закроет меню тут же).
 *
 * Касания ловит обработчик ввода Slate, который видит их РАНЬШЕ виджетов (IInputProcessor,
 * SlateApplication.cpp: ProcessTouchStartedEvent -> ProcessMouseButtonDownEvent ->
 * InputPreProcessors, UE 5.5). Поэтому жест работает поверх любых экранных кнопок и окон.
 * Мышь не участвует (только события с признаком касания).
 *
 * Всё содержимое — под CONTRARY_WITH_QA_CHEATS: в сборке для магазина (Shipping) жеста нет.
 */
#if CONTRARY_WITH_QA_CHEATS

#include "Framework/Application/IInputProcessor.h"

// Счётчик жеста без Slate и без времени движка — его проверяют автотесты.
class FContraryThreeFingerTapDetector
{
public:
	static constexpr int32 RequiredTouches = 3;

	// Окно, в которое должны уложиться начала всех трёх касаний, секунды.
	static constexpr double ThreeFingerTapWindowSeconds = 0.3;

	// Палец коснулся экрана. true — это касание замкнуло жест (его надо поглотить).
	bool OnTouchStarted(int32 PointerIndex, double NowSeconds);

	// Палец отпущен. true — касание было поглощено жестом, отпускание тоже поглотить.
	bool OnTouchEnded(int32 PointerIndex);

	// Касание поглощено жестом (его движения игре не отдаём).
	bool IsSwallowed(int32 PointerIndex) const { return SwallowedTouches.Contains(PointerIndex); }

	int32 GetActiveTouchCount() const { return ActiveTouchStarts.Num(); }

	void Reset();

private:
	// Палец -> время начала касания.
	TMap<int32, double> ActiveTouchStarts;
	TSet<int32> SwallowedTouches;
	double LastGestureTime = -1.0e9;
};

// Обработчик ввода Slate: передаёт касания счётчику, при жесте зовёт OnGesture.
class FContraryDebugMenuTouchProcessor : public IInputProcessor
{
public:
	explicit FContraryDebugMenuTouchProcessor(TFunction<void()> InOnGesture)
		: OnGesture(MoveTemp(InOnGesture))
	{
	}

	// Источник времени подменяется в автотестах (по умолчанию FPlatformTime::Seconds).
	TFunction<double()> NowProvider;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("ContraryDebugMenuThreeFingerTap"); }

	const FContraryThreeFingerTapDetector& GetDetector() const { return Detector; }

private:
	TFunction<void()> OnGesture;
	FContraryThreeFingerTapDetector Detector;
};

#endif // CONTRARY_WITH_QA_CHEATS
