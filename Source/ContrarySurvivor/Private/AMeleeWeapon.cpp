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
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // Build 1.1: плавный доворот StartAimTurnTo

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

	// Иконка для тайлового UI (Build 1.2.2) — мягкая ссылка, как у брони (ADR-043).
	ItemIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/UI/Icons/Items/T_Item_Knife.T_Item_Knife")));

	// --- Поправка хвата ножа (Build 1.2.2) ---
	// Сокет WeaponGripSocket Ринат развернул под ПИСТОЛЕТ (yaw≈180° + сдвиг Y −0.0613 в
	// местных единицах кости R_Hand) — нож при этом «лёг неправильно». Эта поправка = точная
	// ИНВЕРСИЯ сокет-трансформа (напечатана прогоном AddGripSocket -normalize): поправка ∘
	// сокет = тождество, то есть нож снова лежит ровно по кости R_Hand — как до правки сокета
	// (офсеты игрока/бандита нулевые, проверено срезом CDO 08-02). Тонкая доводка — этими же
	// полями в Details.
	GripOffsetLocation = FVector(0.000000, -0.061311, 0.000000);
	GripOffsetRotation = FRotator(0.000341, 179.999728, -0.000062);
}

// Радиус капсулы актёра (0, если это не персонаж) — общая часть расчёта дистанции
// поверхность-к-поверхности.
static float CapsuleRadiusOf(const AActor* Actor)
{
	if (const ACharacter* Char = Cast<ACharacter>(Actor))
	{
		if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			return Capsule->GetScaledCapsuleRadius();
		}
	}
	return 0.0f;
}

float AMeleeWeapon::GetSurfaceDistanceTo(const AActor* Target) const
{
	const APawn* Wielder = GetInstigator();
	if (!Wielder || !IsValid(Target) || Target == Wielder)
	{
		return TNumericLimits<float>::Max();
	}
	const float CenterDist = FVector::Dist(Wielder->GetActorLocation(), Target->GetActorLocation());
	return CenterDist - CapsuleRadiusOf(Wielder) - CapsuleRadiusOf(Target);
}

bool AMeleeWeapon::ConsumePendingSwing()
{
	const bool bWasPending = bSwingAwaitingNotify;
	bSwingAwaitingNotify = false;
	return bWasPending;
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

	const FVector Origin = Wielder->GetActorLocation();

	// Доворот к залоченной цели, если лок в радиусе удара.
	// Build 1.1 (решение game-lead по формулировке Рината «в ЭТОМ ЖЕ секторе наносится урон»):
	// по умолчанию доворот ПЛАВНЫЙ — тот же StartAimTurnTo, что у стрельбы. Он лишь запускает
	// разворот корпуса, который отрабатывает Tick персонажа за следующие кадры, поэтому ЭТОТ
	// взмах считается по текущему направлению взгляда — ровно по тому сектору, который игрок
	// видит подсвеченным на земле. Мгновенный рывок (прежнее поведение) остался под флагом
	// bMeleeSnapToTarget: он не промахивается, но бьёт мимо нарисованного сектора.
	if (bTurnToLockedTarget)
	{
		if (const AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetInstigatorController()))
		{
			AActor* Locked = PC->GetCurrentTarget();
			if (IsValid(Locked) && Locked != Wielder && GetSurfaceDistanceTo(Locked) <= MeleeRange)
			{
				if (bMeleeSnapToTarget)
				{
					FVector ToLocked = Locked->GetActorLocation() - Origin;
					ToLocked.Z = 0.0f;
					if (!ToLocked.IsNearlyZero())
					{
						Wielder->SetActorRotation(FRotator(0.0f, ToLocked.Rotation().Yaw, 0.0f));
					}
				}
				else if (AMasterHumanoidCharacter* WielderHumanoid = Cast<AMasterHumanoidCharacter>(Wielder))
				{
					// Единый механизм прицеливания корпусом с ARangedWeapon::Fire: скорость и
					// длительность ведения цели настраиваются на персонаже (AimTurnInterpSpeed,
					// AimTurnHoldTime), выключается там же (bAimTurnToTarget).
					WielderHumanoid->StartAimTurnTo(Locked);
				}
			}
		}
	}

	// Момент урона (Build 1.1). Анимация удара живёт на ПЕРСОНАЖЕ (одна на игрока и бандита).
	// Пошла — урон нанесёт метка на её дорожке (UAnimNotify_MeleeHit) на нужном кадре, то есть
	// попадание совпадёт с движением. Не пошла (нет ассета, нет слота в анимационном блюпринте) —
	// бьём сразу, как работало до Build 1.1, чтобы удар не пропал.
	bSwingAwaitingNotify = false; // новый замах отменяет ожидание прошлого, если тот не долетел
	if (AMasterHumanoidCharacter* WielderHumanoid = Cast<AMasterHumanoidCharacter>(Wielder))
	{
		if (WielderHumanoid->PlayMeleeMontage())
		{
			bSwingAwaitingNotify = true;
			return;
		}
	}
	ApplyMeleeDamage();
}

void AMeleeWeapon::ApplyMeleeDamage()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* Wielder = GetInstigator();
	if (!Wielder)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMeleeWeapon::ApplyMeleeDamage — no instigator"));
		return;
	}

	const FVector Origin = Wielder->GetActorLocation();
	const float WielderRadius = CapsuleRadiusOf(Wielder);

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

		// Мирные неуязвимые NPC (староста/торговец) — не цели: не бьём и НЕ занимаем ими
		// слот удара (дефект 08-08: староста забирал «front sector 1/1» у реальных врагов).
		if (const AMasterHumanoidCharacter* Humanoid = Cast<AMasterHumanoidCharacter>(HitActor))
		{
			if (Humanoid->IsImmuneToDamage())
			{
				continue;
			}
		}

		const float SurfaceDist = GetSurfaceDistanceTo(HitActor);
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

		// Фикс 08-07: сквозь стену/дерево нож не бьёт — линия удара должна быть свободна.
		if (!HasLineOfSightToTarget(Wielder, HitActor))
		{
			UE_LOG(LogTemp, Log, TEXT("AMeleeWeapon: %s в секторе, но за преградой — удар не засчитан"),
				*HitActor->GetName());
			continue;
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

bool AMeleeWeapon::HasLineOfSightToTarget(const APawn* Wielder, const AActor* Target) const
{
	if (!bRequireLineOfSight)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World || !Wielder || !Target)
	{
		return false;
	}

	// Игнорируем носителя, нож и цель (меш цели сам блокирует Visibility) + прикреплённое
	// к обоим (оружие в руках лежит прямо на линии удара).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeLOS), /*bTraceComplex=*/false, Wielder);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Target);
	TArray<AActor*> Attached;
	Wielder->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
	Params.AddIgnoredActors(Attached);
	Target->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
	Params.AddIgnoredActors(Attached);

	// Центр капсулы носителя -> центр цели: высота удара ножом, той же парой точек меряется
	// дистанция сектора (GetSurfaceDistanceTo).
	return !World->LineTraceTestByChannel(Wielder->GetActorLocation(), Target->GetActorLocation(),
		LineOfSightChannel, Params);
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
