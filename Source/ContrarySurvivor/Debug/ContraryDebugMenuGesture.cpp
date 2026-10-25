// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Debug/ContraryDebugMenuGesture.h"

#if CONTRARY_WITH_QA_CHEATS

#include "HAL/PlatformTime.h"
#include "Input/Events.h"

bool FContraryThreeFingerTapDetector::OnTouchStarted(int32 PointerIndex, double NowSeconds)
{
	// Тот же палец мог «потерять» отпускание (приложение уходило в фон) — просто перезаписываем.
	ActiveTouchStarts.Add(PointerIndex, NowSeconds);
	SwallowedTouches.Remove(PointerIndex);

	// Считаем касания, начавшиеся в окне и после прошлого срабатывания.
	const double WindowStart = NowSeconds - ThreeFingerTapWindowSeconds;
	int32 RecentTouches = 0;
	for (const TPair<int32, double>& Touch : ActiveTouchStarts)
	{
		if (Touch.Value >= WindowStart && Touch.Value > LastGestureTime)
		{
			++RecentTouches;
		}
	}
	if (RecentTouches < RequiredTouches)
	{
		return false;
	}

	LastGestureTime = NowSeconds;
	SwallowedTouches.Add(PointerIndex);
	return true;
}

bool FContraryThreeFingerTapDetector::OnTouchEnded(int32 PointerIndex)
{
	ActiveTouchStarts.Remove(PointerIndex);
	return SwallowedTouches.Remove(PointerIndex) > 0;
}

void FContraryThreeFingerTapDetector::Reset()
{
	ActiveTouchStarts.Reset();
	SwallowedTouches.Reset();
	LastGestureTime = -1.0e9;
}

bool FContraryDebugMenuTouchProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!MouseEvent.IsTouchEvent())
	{
		return false;
	}
	const double Now = NowProvider ? NowProvider() : FPlatformTime::Seconds();
	if (!Detector.OnTouchStarted(MouseEvent.GetPointerIndex(), Now))
	{
		return false;
	}
	if (OnGesture)
	{
		OnGesture();
	}
	return true; // третье касание игре не отдаём
}

bool FContraryDebugMenuTouchProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return MouseEvent.IsTouchEvent() && Detector.OnTouchEnded(MouseEvent.GetPointerIndex());
}

bool FContraryDebugMenuTouchProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return MouseEvent.IsTouchEvent() && Detector.IsSwallowed(MouseEvent.GetPointerIndex());
}

#endif // CONTRARY_WITH_QA_CHEATS
