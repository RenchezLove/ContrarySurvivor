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

	// Грубый overlap по объектам-пешкам: радиус с запасом (носитель + дальность + типовая
	// капсула цели ~60). Точную проверку поверхность-к-поверхности делаем ниже по каждой цели.
	const float QueryRadius = WielderRadius + MeleeRange + 60.0f;

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Wielder);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	const bool bAny = World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjParams,
		FCollisionShape::MakeSphere(QueryRadius),
		QueryParams);

	if (!bAny)
	{
		UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: swing — no target in range"));
		return;
	}

	int32 HitCount = 0;
	TSet<AActor*> Damaged; // не бить одну цель дважды за замах
	for (const FOverlapResult& Ov : Overlaps)
	{
		AActor* HitActor = Ov.GetActor();
		if (!HitActor || HitActor == Wielder || Damaged.Contains(HitActor))
		{
			continue;
		}

		// Дистанция поверхность-к-поверхности (как в фиксе боя бандита).
		float TargetRadius = 0.0f;
		if (const ACharacter* HitChar = Cast<ACharacter>(HitActor))
		{
			if (const UCapsuleComponent* Capsule = HitChar->GetCapsuleComponent())
			{
				TargetRadius = Capsule->GetScaledCapsuleRadius();
			}
		}

		const float CenterDist = FVector::Dist(Origin, HitActor->GetActorLocation());
		const float SurfaceDist = CenterDist - WielderRadius - TargetRadius;
		if (SurfaceDist > MeleeRange)
		{
			continue; // вне короткого радиуса ножа
		}

		FDamageEvent DamageEvent;
		HitActor->TakeDamage(Damage, DamageEvent, GetInstigatorController(), this);
		Damaged.Add(HitActor);
		++HitCount;

		UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: knife hit %s for %.1f"),
			*HitActor->GetName(), Damage);
	}

	UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: swing dealt damage to %d target(s)"), HitCount);
}
