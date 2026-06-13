// Fill out your copyright notice in the Description page of Project Settings.

#include "AMeleeWeapon.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
#include "UObject/ConstructorHelpers.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"

AMeleeWeapon::AMeleeWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	// Реальный меш ножа (импортирован, Фаза 3). ItemMesh — корневой StaticMeshComponent
	// из базы AMasterInventoryItem. BP-наследник может переопределить.
	if (ItemMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> KnifeMeshAsset(TEXT("/Game/Weapons/SM_Knife.SM_Knife"));
		if (KnifeMeshAsset.Succeeded())
		{
			ItemMesh->SetStaticMesh(KnifeMeshAsset.Object);
		}
		// Нож носится прикреплённым к сокету персонажа; собственная коллизия не нужна.
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// --- ЧЕРНОВЫЕ статы ножа (draft, GDD §7.2) ---
	Damage      = 35.0f;          // урон удара (draft)
	WeaponType  = EWeaponType::OneHanded;
	Range       = MeleeRange;     // справочно (база использует Range для дальнобоя)

	// У ближнего оружия нет патронов — нейтрализуем ammo-логику базы,
	// чтобы CanFire() базы не блокировал по «пустому магазину».
	MaxAmmoInClip      = 0;
	CurrentAmmoInClip  = 0;
	MaxAmmoReserve     = 0;
	CurrentAmmoReserve = 0;

	ItemName        = FString("Knife");
	ItemDescription = FString("A short blade for close combat.");
}

void AMeleeWeapon::Fire(AActor* /*Target*/)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Кулдаун замаха (без комбо).
	const float Now = World->GetTimeSeconds();
	if (Now - LastMeleeTime < AttackCooldown)
	{
		return;
	}
	LastMeleeTime = Now;

	// Носитель ножа (тот, кто экипировал) — игнорируем как цель.
	APawn* Wielder = GetInstigator();
	if (!Wielder)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMeleeWeapon::Fire — no instigator"));
		return;
	}

	// Радиус капсулы носителя — для перевода MeleeRange (surface) в дистанцию центров.
	float WielderRadius = 0.0f;
	if (const ACharacter* WielderChar = Cast<ACharacter>(Wielder))
	{
		if (const UCapsuleComponent* Capsule = WielderChar->GetCapsuleComponent())
		{
			WielderRadius = Capsule->GetScaledCapsuleRadius();
		}
	}

	const FVector Origin = Wielder->GetActorLocation();

	// Дистанция поверхность-к-поверхности до актёра (центр-к-центру минус радиусы капсул),
	// как в фиксе боя бандита (Фаза 2). Возвращает BIG_NUMBER, если цель невалидна.
	auto SurfaceDistTo = [&](AActor* Target) -> float
	{
		if (!IsValid(Target) || Target == Wielder)
		{
			return TNumericLimits<float>::Max();
		}
		float TargetRadius = 0.0f;
		if (const ACharacter* TargetChar = Cast<ACharacter>(Target))
		{
			if (const UCapsuleComponent* Capsule = TargetChar->GetCapsuleComponent())
			{
				TargetRadius = Capsule->GetScaledCapsuleRadius();
			}
		}
		const float CenterDist = FVector::Dist(Origin, Target->GetActorLocation());
		return CenterDist - WielderRadius - TargetRadius;
	};

	// ФИКС2: бьём РОВНО ОДНУ цель за удар (не AoE).
	AActor* Victim = nullptr;

	// 1) Приоритет — залоченная цель игрока (консистентно с тач-управлением: лок = кого бьём),
	//    если она в дистанции удара. Авто-таргет контроллера держит CurrentTarget живым.
	if (const AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetInstigatorController()))
	{
		AActor* Locked = PC->GetCurrentTarget();
		if (IsValid(Locked) && Locked != Wielder && SurfaceDistTo(Locked) <= MeleeRange)
		{
			Victim = Locked;
		}
	}

	// 2) Фолбэк — ближайшая ОДНА цель-пешка в коротком радиусе перед игроком.
	if (!Victim)
	{
		const float QueryRadius = WielderRadius + MeleeRange + 60.0f;

		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery(ECC_Pawn);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Wielder);
		QueryParams.AddIgnoredActor(this);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			Origin,
			FQuat::Identity,
			ObjParams,
			FCollisionShape::MakeSphere(QueryRadius),
			QueryParams);

		float BestDist = MeleeRange;
		for (const FOverlapResult& Ov : Overlaps)
		{
			AActor* HitActor = Ov.GetActor();
			if (!HitActor || HitActor == Wielder)
			{
				continue;
			}
			const float SurfaceDist = SurfaceDistTo(HitActor);
			if (SurfaceDist <= BestDist) // строго ближайший в пределах MeleeRange
			{
				BestDist = SurfaceDist;
				Victim = HitActor;
			}
		}
	}

	if (!Victim)
	{
		UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: swing — no target in range"));
		return;
	}

	// Урон РОВНО одной цели.
	FDamageEvent DamageEvent;
	Victim->TakeDamage(Damage, DamageEvent, GetInstigatorController(), this);

	UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: knife hit %s for %.1f (single target)"),
		*Victim->GetName(), Damage);
}
