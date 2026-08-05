// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ContrarySurvivorStatics.generated.h"

/**
 * Небольшой набор статических Blueprint-функций проекта (Build 1). Пока — только сглаженное
 * число кадров/сек для отладочного показа (например, в меню паузы). Функции статические, поэтому
 * узел «Get Current FPS» можно поставить в любом виджете/блюпринте без ссылки на объект.
 */
UCLASS()
class CONTRARYSURVIVOR_API UContrarySurvivorStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Сглаженное число кадров в секунду (усреднение движка GAverageFPS — не «дёргается» покадрово).
	// Для показа в UI (меню паузы). Размещение и размер числа настраивает unreal-operator в ассете.
	UFUNCTION(BlueprintPure, Category = "Perf")
	static float GetCurrentFPS();

	/**
	 * Четыре времени кадра в миллисекундах — те же величины, что показывает движковая команда
	 * «stat unit» (консоль на телефоне нам недоступна, поэтому берём их из движка напрямую).
	 * Нужны, чтобы отличить упор в процессор от упора в видеочип: у кого число ближе всего к
	 * времени кадра — тот и узкое место.
	 *
	 *  OutFrameMs — кадр целиком (сколько прошло от кадра до кадра);
	 *  OutGameMs  — игровая логика (главный поток);
	 *  OutDrawMs  — отправка отрисовки (поток рендера, БЕЗ времени самого видеочипа);
	 *  OutGpuMs   — работа видеочипа. На Android число приходит от системы кадрового ритма
	 *               (Swappy) — если она выключена, здесь останется 0, и это тоже ответ:
	 *               значит времени видеочипа устройство не отдаёт.
	 *
	 * Считается ровно как в движке (UnrealClient.cpp, FStatUnitData::DrawStat): сырое значение
	 * сглаживается формулой 0.9 старого плюс 0.1 нового, иначе числа скачут покадрово и читать
	 * их на телефоне невозможно. Вызывать не чаще одного раза за кадр: сглаживание накопительное.
	 * Стоимость вызова — чтение трёх счётчиков движка и несколько умножений, память не трогает.
	 */
	static void GetFrameTimingsMs(float& OutFrameMs, float& OutGameMs, float& OutDrawMs, float& OutGpuMs);
};
