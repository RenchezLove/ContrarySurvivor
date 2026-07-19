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
#include "TimerManager.h" // D5: таймер восстановления времени после hitstop
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
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

	// Звук замаха ножом (Демо). Дефолт из импортированного ассета; переопределяется в BP.
	static ConstructorHelpers::FObjectFinder<USoundBase> SwingSoundAsset(
		TEXT("/Game/Audio/Demo/knife_melee_swing.knife_melee_swing"));
	if (SwingSoundAsset.Succeeded())
	{
		SwingSound = SwingSoundAsset.Object;
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

	// ItemName — служебный ключ, НЕ трогаем; игроку показывается ItemDisplayText (ADR-050).
	ItemName        = FString("Knife");
	ItemDisplayText = NSLOCTEXT("Items", "Knife", "Нож");
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

	// Звук замаха — на каждый реальный взмах (звучит и при промахе).
	if (SwingSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SwingSound, Wielder->GetActorLocation(), SwingSoundVolume);
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

	// --- ЭТАП D (ADR-037): передний взмах/сектор вместо радиального удара ---

	// Доворот к залоченной цели (решение game-lead): если лок в радиусе удара — носитель-игрок
	// поворачивается к нему лицом ПЕРЕД проверкой сектора. Сектор остаётся передним (ADR-037),
	// но удар по локу не промахивается из-за ориентации бега (orient-to-movement).
	if (bTurnToLockedTarget)
	{
		if (const AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetInstigatorController()))
		{
			AActor* Locked = PC->GetCurrentTarget();
			if (IsValid(Locked) && Locked != Wielder && SurfaceDistTo(Locked) <= MeleeRange)
			{
				FVector ToLocked = Locked->GetActorLocation() - Origin;
				ToLocked.Z = 0.0f;
				if (!ToLocked.IsNearlyZero())
				{
					Wielder->SetActorRotation(FRotator(0.0f, ToLocked.Rotation().Yaw, 0.0f));
				}
			}
		}
	}

	// Кандидаты: пешки в сфере вокруг носителя (грубая выборка), затем фильтр
	// «в дистанции MeleeRange И в переднем секторе», сортировка по близости,
	// урон первым MaxTargetsPerSwing (ADR-037: «1-2 цели впереди, НЕ круг»).
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

	const FVector Forward2D = Wielder->GetActorForwardVector().GetSafeNormal2D();
	const float CosSectorLimit = FMath::Cos(FMath::DegreesToRadians(MeleeSectorHalfAngleDeg));

	struct FMeleeCandidate
	{
		AActor* Actor = nullptr;
		float SurfaceDist = 0.0f;
	};
	TArray<FMeleeCandidate> Candidates;

	for (const FOverlapResult& Ov : Overlaps)
	{
		AActor* HitActor = Ov.GetActor();
		if (!HitActor || HitActor == Wielder)
		{
			continue;
		}

		const float SurfaceDist = SurfaceDistTo(HitActor);
		if (SurfaceDist > MeleeRange)
		{
			continue;
		}

		// Передний сектор: угол между взглядом носителя и направлением на цель (в плоскости).
		// Цель ВПЛОТНУЮ (направление вырождено — капсулы слиплись) считается «впереди».
		const FVector ToTarget2D = (HitActor->GetActorLocation() - Origin).GetSafeNormal2D();
		if (!ToTarget2D.IsNearlyZero()
			&& FVector::DotProduct(Forward2D, ToTarget2D) < CosSectorLimit)
		{
			continue; // вне переднего сектора — НЕ круговой удар (ADR-037)
		}

		Candidates.Add({ HitActor, SurfaceDist });
	}

	if (Candidates.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: swing — no target in front sector (half-angle %.0f)"),
			MeleeSectorHalfAngleDeg);
		return;
	}

	// Ближайшие первыми; урон максимум MaxTargetsPerSwing целям.
	Candidates.Sort([](const FMeleeCandidate& A, const FMeleeCandidate& B)
	{
		return A.SurfaceDist < B.SurfaceDist;
	});

	const int32 HitCount = FMath::Min(FMath::Max(1, MaxTargetsPerSwing), Candidates.Num());
	for (int32 i = 0; i < HitCount; ++i)
	{
		FDamageEvent DamageEvent;
		Candidates[i].Actor->TakeDamage(Damage, DamageEvent, GetInstigatorController(), this);

		UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: knife hit %s for %.1f (front sector, %d/%d)"),
			*Candidates[i].Actor->GetName(), Damage, i + 1, HitCount);
	}

	// D5: микро-заморозка при реальном попадании — только для носителя-игрока
	// (у ИИ hitstop дёргал бы время всей игры без действия игрока).
	if (bEnableHitStop && Wielder->IsPlayerControlled())
	{
		ApplyHitStop();
	}
}

void AMeleeWeapon::ApplyHitStop()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameplayStatics::SetGlobalTimeDilation(World, HitStopTimeDilation);
	bHitStopPending = true;

	// Таймеры считают ИГРОВОЕ (замедленное) время: чтобы пауза длилась HitStopDuration
	// РЕАЛЬНЫХ секунд, период таймера = HitStopDuration * dilation. Повторный удар до
	// восстановления просто перезапускает таймер (та же ручка) — время не «застревает».
	World->GetTimerManager().SetTimer(HitStopTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bHitStopPending = false;
			if (UWorld* W = GetWorld())
			{
				UGameplayStatics::SetGlobalTimeDilation(W, 1.0f);
			}
		}),
		FMath::Max(0.001f, HitStopDuration * HitStopTimeDilation), /*bLoop=*/false);
}

void AMeleeWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Страховка (qa): уничтожение/снятие оружия в окно hitstop — таймер с WeakLambda уже
	// не выполнится, восстанавливаем нормальный ход времени сами. Вне окна — ничего не трогаем
	// (не сбивать дилатацию, если её меняет кто-то другой).
	if (bHitStopPending)
	{
		bHitStopPending = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HitStopTimerHandle);
			UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
		}
	}

	Super::EndPlay(EndPlayReason);
}
