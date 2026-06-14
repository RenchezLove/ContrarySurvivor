// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

/**
 * Утилиты надёжной постановки спавна по высоте (BUG: спавн под картой на Z=-4055).
 *
 * Раньше при провале навмеш-проекции спавн-сабсистемы брали "сырой" fallback Z,
 * унаследованный от игрока. Если игрок проваливался под карту (Z=-4055), все NPC
 * наследовали этот битый Z -> петля "спавн под полом -> KillZ -> респавн".
 *
 * Здесь — НЕ зависящая от навмеша трассировка вниз до пола (ECC_Visibility),
 * используется как fallback (навмеш остаётся primary) и как кламп битого Z игрока.
 */
namespace SpawnPlacement
{
	// Высота, с которой трассируем вниз в поисках пола (над любой геометрией деревни).
	static constexpr float TraceStartZ = 5000.0f;
	// Длина трассы вниз (покрывает любой разумный рельеф).
	static constexpr float TraceLength = 20000.0f;
	// Безопасный Z по умолчанию, если пол не найден. НИКОГДА не отрицательный.
	static constexpr float SafeDefaultZ = 200.0f;
	// Порог "битого" Z: всё ниже считаем проваленным под карту.
	static constexpr float BadZThreshold = -1000.0f;

	/**
	 * Трассирует вниз из (X, Y, TraceStartZ) и возвращает Z пола + ZOffset.
	 * @return true если пол найден; OutZ заполнен. false — пол не найден.
	 */
	inline bool TraceFloorZ(const UWorld* World, float X, float Y, float ZOffset,
		float& OutZ, const AActor* IgnoreActor = nullptr)
	{
		if (!World)
		{
			return false;
		}

		const FVector Start(X, Y, TraceStartZ);
		const FVector End(X, Y, TraceStartZ - TraceLength);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnFloorTrace), /*bTraceComplex=*/false);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			OutZ = Hit.Location.Z + ZOffset;
			return true;
		}
		return false;
	}

	/**
	 * Возвращает надёжный Z для постановки спавна в точке (X, Y):
	 * сперва трасса до пола (+ZOffset), иначе безопасный фиксированный Z (>0).
	 * Пишет QA-лог о выбранной ветке. Никогда не возвращает отрицательный Z.
	 */
	inline float ResolveSpawnZ(const UWorld* World, float X, float Y, float ZOffset,
		const TCHAR* Context, const AActor* IgnoreActor = nullptr)
	{
		float FloorZ = 0.0f;
		if (TraceFloorZ(World, X, Y, ZOffset, FloorZ, IgnoreActor))
		{
			UE_LOG(LogTemp, Warning, TEXT("QA: %s spawn fallback via floor-trace Z=%.1f"), Context, FloorZ);
			return FloorZ;
		}

		UE_LOG(LogTemp, Warning, TEXT("QA: %s spawn safe-default Z=%.1f (floor-trace found nothing)"), Context, SafeDefaultZ);
		return SafeDefaultZ;
	}
}
