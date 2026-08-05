// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Utils/ContrarySurvivorStatics.h"
#include "Misc/App.h"      // FApp::GetDeltaTime — фолбэк, пока GAverageFPS не посчитан
#include "HAL/PlatformTime.h" // перевод счётчиков движка в миллисекунды
#include "RenderTimer.h"   // GGameThreadTime / GRenderThreadTime (модуль RenderCore)
#include "RHIGlobals.h"    // GGPUFrameTime (модуль RHI)

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

void UContrarySurvivorStatics::GetFrameTimingsMs(float& OutFrameMs, float& OutGameMs,
	float& OutDrawMs, float& OutGpuMs)
{
	// Сглаженные значения живут между вызовами. Экземпляр игры в процессе один, поэтому
	// файловых статиков достаточно — ровно так же хранит их и сам движок в FStatUnitData.
	static float SmoothedFrameMs = 0.0f;
	static float SmoothedGameMs  = 0.0f;
	static float SmoothedDrawMs  = 0.0f;
	static float SmoothedGpuMs   = 0.0f;

	// Время кадра целиком берём тем же способом, что движок: разница отметок начала кадров
	// приложения. Она правильно учитывает простой в конце кадра и сходится с остальными
	// числами (UnrealClient.cpp, FStatUnitData::DrawStat).
	const float RawFrameMs = static_cast<float>((FApp::GetCurrentTime() - FApp::GetLastTime()) * 1000.0);
	const float RawGameMs  = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float RawDrawMs  = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const float RawGpuMs   = FPlatformTime::ToMilliseconds(GGPUFrameTime);

	// Сглаживание один в один как в движке: девять десятых прежнего плюс одна десятая нового.
	// Без него числа скачут покадрово и на телефоне их не прочитать.
	SmoothedFrameMs = 0.9f * SmoothedFrameMs + 0.1f * RawFrameMs;
	SmoothedGameMs  = 0.9f * SmoothedGameMs  + 0.1f * RawGameMs;
	SmoothedDrawMs  = 0.9f * SmoothedDrawMs  + 0.1f * RawDrawMs;
	SmoothedGpuMs   = 0.9f * SmoothedGpuMs   + 0.1f * RawGpuMs;

	OutFrameMs = SmoothedFrameMs;
	OutGameMs  = SmoothedGameMs;
	OutDrawMs  = SmoothedDrawMs;
	OutGpuMs   = SmoothedGpuMs;
}
