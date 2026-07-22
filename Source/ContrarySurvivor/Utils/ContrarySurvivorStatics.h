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
};
