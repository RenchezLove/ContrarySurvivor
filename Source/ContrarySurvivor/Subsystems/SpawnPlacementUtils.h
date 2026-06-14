// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "ContrarySurvivor/Debug/QADebug.h" // FQADebug::QA — подробный лог трассы спавна

/**
 * Утилиты надёжной постановки спавна по высоте (BUG: спавн под картой на Z=-4055).
 *
 * Раньше при провале навмеш-проекции спавн-сабсистемы брали "сырой" fallback Z,
 * унаследованный от игрока. Если игрок проваливался под карту (Z=-4055), все NPC
 * наследовали этот битый Z -> петля "спавн под полом -> KillZ -> респавн".
 *
 * Здесь — НЕ зависящая от навмеша трассировка вниз до пола (по ECC_WorldStatic:
 * пол/дома/дороги — статика; пешки/оружие — динамика и в трассу НЕ ловятся),
 * используется как fallback (навмеш остаётся primary) и как кламп битого Z игрока.
 *
 * BUG (демка неиграбельна): трасса из +5000 по ECC_Visibility попадала в рантайм-мусор
 * в небе (гигантский пистолет scale~100 на Z~5090) — игрок/NPC «приземлялись» на него.
 * Лечим: канал ECC_WorldStatic (динамику игнорируем) + старт трассы НИЖЕ мусора (2000).
 */
namespace SpawnPlacement
{
	// Высота, с которой трассируем вниз в поисках пола: выше геометрии деревни (дома ~до 300),
	// но НИЖЕ небесного рантайм-мусора (~5090), чтобы трасса физически до него не доставала.
	static constexpr float TraceStartZ = 2000.0f;
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
		float& OutZ, const AActor* IgnoreActor = nullptr, const TCHAR* Context = TEXT("?"))
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

		// Трасса ПО ТИПУ ОБЪЕКТА только по статической/динамической геометрии мира — Pawn'ы
		// (object type Pawn) в выборку НЕ входят. WorldStatic — пол/дома/дороги; WorldDynamic —
		// на случай движимого пола.
		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		// ДИАГНОСТИКА (2026-06-14): прошлый object-type фикс НЕ изменил результат — игрок/NPC
		// всё равно релокались на Z≈2098 (= старт 2000 + offset), т.е. трасса «попадает» в
		// НАЧАЛЕ (Z≈2000), а не в полу деревни (~0..162). Чтобы увидеть, ВО ЧТО реально упирается
		// трасса, берём MULTI-трассу и логируем КАЖДЫЙ хит: bStartPenetrating (initial overlap!),
		// bBlockingHit, ImpactPoint.Z, имя+класс актора, имя компонента. Затем выбираем как «пол»
		// ПЕРВЫЙ хит, который НЕ является initial-overlap (bStartPenetrating=false) — это отсекает
		// случай «старт трассы внутри коллайдера → возвращался старт Z=2000».
		TArray<FHitResult> Hits;
		const bool bAny = World->LineTraceMultiByObjectType(Hits, Start, End, ObjParams, Params);

		FQADebug::QA(World, FString::Printf(
			TEXT("QA: floortrace[%s] start=%s end=%s hits=%d"),
			Context, *Start.ToCompactString(), *End.ToCompactString(), Hits.Num()), /*bScreen=*/true);

		const FHitResult* Floor = nullptr;
		for (int32 i = 0; i < Hits.Num(); ++i)
		{
			const FHitResult& H = Hits[i];
			const AActor* HitActor = H.GetActor();
			const UPrimitiveComponent* HitComp = H.GetComponent();
			// Подробности каждого хита — в лог-файл (bScreen=false), чтобы не залить оверлей.
			FQADebug::QA(World, FString::Printf(
				TEXT("QA:  hit[%d] startPen=%d block=%d impZ=%.1f locZ=%.1f actor='%s'(%s) comp='%s'"),
				i,
				H.bStartPenetrating ? 1 : 0,
				H.bBlockingHit ? 1 : 0,
				H.ImpactPoint.Z,
				H.Location.Z,
				HitActor ? *HitActor->GetName() : TEXT("none"),
				HitActor ? *HitActor->GetClass()->GetName() : TEXT("?"),
				HitComp ? *HitComp->GetName() : TEXT("none")), /*bScreen=*/false);

			// Пол = первый НЕ-проникающий-в-старте хит (реальная поверхность ниже старта).
			if (!Floor && !H.bStartPenetrating)
			{
				Floor = &H;
			}
		}

		if (Floor)
		{
			OutZ = Floor->ImpactPoint.Z + ZOffset;
			const AActor* FloorActor = Floor->GetActor();
			FQADebug::QA(World, FString::Printf(
				TEXT("QA: floortrace[%s] FLOOR '%s'(%s) impZ=%.1f -> spawnZ=%.1f"),
				Context,
				FloorActor ? *FloorActor->GetName() : TEXT("none"),
				FloorActor ? *FloorActor->GetClass()->GetName() : TEXT("?"),
				Floor->ImpactPoint.Z, OutZ), /*bScreen=*/true);
			return true;
		}

		// Пол не найден: либо вообще нет хитов, либо ВСЕ хиты — initial-overlap (старт внутри
		// геометрии). В обоих случаях возвращаем false → ResolveSpawnZ возьмёт SafeDefaultZ (>0),
		// НЕ старт трассы (2000) — это и есть прицельная защита от Z≈2098.
		FQADebug::QA(World, FString::Printf(
			TEXT("QA: floortrace[%s] NO floor (any=%d, all-startPenetrating or empty) -> caller uses safe-default"),
			Context, bAny ? 1 : 0), /*bScreen=*/true);
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
		if (TraceFloorZ(World, X, Y, ZOffset, FloorZ, IgnoreActor, Context))
		{
			UE_LOG(LogTemp, Warning, TEXT("QA: %s spawn fallback via floor-trace Z=%.1f"), Context, FloorZ);
			return FloorZ;
		}

		UE_LOG(LogTemp, Warning, TEXT("QA: %s spawn safe-default Z=%.1f (floor-trace found nothing)"), Context, SafeDefaultZ);
		return SafeDefaultZ;
	}
}
