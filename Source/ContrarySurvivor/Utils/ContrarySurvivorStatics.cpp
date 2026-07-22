// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Utils/ContrarySurvivorStatics.h"
#include "Misc/App.h" // FApp::GetDeltaTime — фолбэк, пока GAverageFPS не посчитан

// Сглаженное движком число кадров/сек. Публичного заголовка у символа нет — объявляем extern
// локально (штатный приём движка: так же он объявляется в самом UnrealEngine.cpp).
extern ENGINE_API float GAverageFPS;

float UContrarySurvivorStatics::GetCurrentFPS()
{
	// GAverageFPS — усреднённое значение, обновляется движком каждый кадр. Пока не посчитано
	// (0 на первых кадрах) — мгновенное из дельты кадра приложения как безопасный фолбэк.
	if (GAverageFPS > 0.0f)
	{
		return GAverageFPS;
	}
	const float Delta = FApp::GetDeltaTime();
	return Delta > 0.0f ? 1.0f / Delta : 0.0f;
}
