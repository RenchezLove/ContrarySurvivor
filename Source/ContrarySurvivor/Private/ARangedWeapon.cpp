// Fill out your copyright notice in the Description page of Project Settings.

#include "ARangedWeapon.h"
#include "DrawDebugHelpers.h" // Для отладочной визуализации LineTrace
#include "Engine/DamageEvents.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ARangedWeapon::ARangedWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	LockedTarget    = nullptr;
	MuzzleSocketName = FName("MuzzleSocket");
	Spread          = 0.05f;

	// Звук выстрела (Демо). Дефолт из импортированного ассета; переопределяется в BP.
	FireSoundVolume = 0.5f;
	static ConstructorHelpers::FObjectFinder<USoundBase> FireSoundAsset(
		TEXT("/Game/Audio/Demo/pistol_22_gunshot.pistol_22_gunshot"));
	if (FireSoundAsset.Succeeded())
	{
		FireSound = FireSoundAsset.Object;
	}
}

void ARangedWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void ARangedWeapon::AddReserveAmmo(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	// CurrentAmmoReserve/MaxAmmoReserve — protected в AMasterWeapon, доступны из наследника.
	CurrentAmmoReserve = FMath::Min(CurrentAmmoReserve + Amount, MaxAmmoReserve);
	UE_LOG(LogTemp, Log, TEXT("%s: +%d reserve ammo -> %d/%d"),
		*GetName(), Amount, CurrentAmmoReserve, MaxAmmoReserve);
}

void ARangedWeapon::Fire(AActor* Target)
{
	if (!CanFire()) 
	{
		// Если патронов нет — автоматически начинаем перезарядку
		if (CurrentAmmoInClip <= 0)
		{
			Reload();
		}
		return;
	}

	AActor* FiringTarget = Target ? Target : LockedTarget;
	if (!FiringTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARangedWeapon::Fire() — no target"));
		return;
	}

	FHitResult HitResult;
	if (PerformLineTrace(FiringTarget, HitResult))
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			// Наносим урон через стандартную систему урона UE
			FDamageEvent DamageEvent;
			HitActor->TakeDamage(Damage, DamageEvent, GetInstigatorController(), this);

			UE_LOG(LogTemp, Warning, TEXT("ARangedWeapon: Hit %s for %.1f damage"), 
				*HitActor->GetName(), Damage);
		}
	}

	// Звук выстрела — только при реальном выстреле (CanFire() уже прошёл, обойма не пуста).
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation(), FireSoundVolume);
	}

	// Тратим патрон и обновляем время последнего выстрела
	CurrentAmmoInClip--;
	LastFireTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Warning, TEXT("ARangedWeapon: Fired. Ammo: %d/%d"), 
		CurrentAmmoInClip, MaxAmmoInClip);
}

void ARangedWeapon::SetTarget(AActor* NewTarget)
{
	LockedTarget = NewTarget;

	if (LockedTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARangedWeapon: Target locked — %s"), *LockedTarget->GetName());
	}
}

void ARangedWeapon::ClearTarget()
{
	LockedTarget = nullptr;
	UE_LOG(LogTemp, Warning, TEXT("ARangedWeapon: Target cleared"));
}

bool ARangedWeapon::PerformLineTrace(AActor* Target, FHitResult& OutHit)
{
	if (!Target || !GetWorld()) return false;

	// Стартовая точка — позиция самого оружия (в будущем заменим на MuzzleSocket)
	FVector StartLocation = GetActorLocation();

	// Конечная точка — центр цели + небольшой разброс
	FVector TargetLocation = Target->GetActorLocation();

	if (Spread > 0.0f)
	{
		FVector RandomOffset = FVector(
			FMath::RandRange(-Spread * 100.0f, Spread * 100.0f),
			FMath::RandRange(-Spread * 100.0f, Spread * 100.0f),
			0.0f
		);
		TargetLocation += RandomOffset;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);              // Игнорируем само оружие
	QueryParams.AddIgnoredActor(GetInstigator());   // Игнорируем владельца оружия

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		OutHit,
		StartLocation,
		TargetLocation,
		ECC_Visibility,
		QueryParams
	);

	// Отладочная линия (убери когда не нужна)
	DrawDebugLine(GetWorld(), StartLocation, TargetLocation,
		bHit ? FColor::Red : FColor::Green, false, 1.0f);

	return bHit;
}
