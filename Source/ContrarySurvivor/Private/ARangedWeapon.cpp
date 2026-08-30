// Fill out your copyright notice in the Description page of Project Settings.

#include "ARangedWeapon.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h" // D5: лёгкая тряска камеры при выстреле игрока
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h" // громкость эффектов (экран настроек)
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // вариант A прицеливания: доворот корпуса носителя

ARangedWeapon::ARangedWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	LockedTarget    = nullptr;
	MuzzleSocketName = FName("MuzzleSocket");
	Spread          = 0.05f;

	// Звук выстрела (Демо). Дефолт из импортированного ассета; переопределяется в BP.
	// 1.0: прежние 0.5 звучали слишком тихо (фидбек Рината 07-17 «очень тихий, как с
	// глушителем»; вторая половина проблемы — сам сэмпл .22, решение по замене отложено).
	FireSoundVolume = 1.0f;
	static ConstructorHelpers::FObjectFinder<USoundBase> FireSoundAsset(
		TEXT("/Game/Audio/Demo/pistol_22_gunshot.pistol_22_gunshot"));
	if (FireSoundAsset.Succeeded())
	{
		FireSound = FireSoundAsset.Object;
	}

	// Вариации выстрела (Ринат 30.08): два куска нарезки A_34P (1911) чередуются, чтобы
	// подряд не звучал один и тот же сэмпл. Жёсткие ссылки конструктора — как у прочих
	// звуков /Game/Audio/Demo (папка не в DirectoriesToAlwaysCook, в пак они попадают
	// именно по таким ссылкам).
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot1Asset(
		TEXT("/Game/Audio/Demo/pistol_1911_shot_1.pistol_1911_shot_1"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot2Asset(
		TEXT("/Game/Audio/Demo/pistol_1911_shot_2.pistol_1911_shot_2"));
	if (Shot1Asset.Succeeded()) { FireSoundVariations.Add(Shot1Asset.Object); }
	if (Shot2Asset.Succeeded()) { FireSoundVariations.Add(Shot2Asset.Object); }

	// --- Вспышка + след пули (D2): переиспользуемые компоненты, скрыты до выстрела ---
	// Меши — базовые фигуры движка; материал — BasicShapeMaterial (параметр Color),
	// оператор может заменить FXMaterial на светящийся без правок кода.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FlashSphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TracerCylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FXMaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (FXMaterialAsset.Succeeded())
	{
		FXMaterial = FXMaterialAsset.Object;
	}

	MuzzleFlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlash"));
	MuzzleFlashMesh->SetupAttachment(RootComponent); // ItemMesh — корень AMasterInventoryItem
	MuzzleFlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MuzzleFlashMesh->SetGenerateOverlapEvents(false);
	MuzzleFlashMesh->SetCanEverAffectNavigation(false);
	MuzzleFlashMesh->SetCastShadow(false);
	MuzzleFlashMesh->SetVisibility(false);
	if (FlashSphereAsset.Succeeded())
	{
		MuzzleFlashMesh->SetStaticMesh(FlashSphereAsset.Object);
	}

	TracerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer"));
	TracerMesh->SetupAttachment(RootComponent);
	// След позиционируется в МИРОВЫХ координатах (линия дуло->цель), не следует за оружием.
	TracerMesh->SetAbsolute(/*bNewAbsoluteLocation=*/true, /*bNewAbsoluteRotation=*/true, /*bNewAbsoluteScale=*/true);
	TracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TracerMesh->SetGenerateOverlapEvents(false);
	TracerMesh->SetCanEverAffectNavigation(false);
	TracerMesh->SetCastShadow(false);
	TracerMesh->SetVisibility(false);
	if (TracerCylinderAsset.Succeeded())
	{
		TracerMesh->SetStaticMesh(TracerCylinderAsset.Object);
	}
}

FVector ARangedWeapon::GetMuzzleLocation() const
{
	if (ItemMesh)
	{
		// Сокет дула, если моделлер/оператор добавил его на меш оружия.
		if (ItemMesh->DoesSocketExist(MuzzleSocketName))
		{
			return ItemMesh->GetSocketLocation(MuzzleSocketName);
		}
		// Иначе — параметрический офсет от меша (подбирается в Details).
		return ItemMesh->GetComponentTransform().TransformPosition(MuzzleOffset);
	}
	return GetActorLocation();
}

void ARangedWeapon::EnsureFXMaterial()
{
	if (FXMID || !FXMaterial)
	{
		return;
	}

	// Один динамический материал на оба меша; цвет — параметр "Color" (BasicShapeMaterial).
	// Если у заменённого материала параметра нет — SetVectorParameterValue просто ничего
	// не изменит (не ошибка).
	FXMID = UMaterialInstanceDynamic::Create(FXMaterial, this);
	if (FXMID)
	{
		FXMID->SetVectorParameterValue(FName("Color"), FireFXColor);
		if (MuzzleFlashMesh)
		{
			MuzzleFlashMesh->SetMaterial(0, FXMID);
		}
		if (TracerMesh)
		{
			TracerMesh->SetMaterial(0, FXMID);
		}
	}
}

void ARangedWeapon::PlayFireSound()
{
	// Вариации выстрела (Ринат 30.08: «каждый выстрел не похож на предыдущий»): из списка
	// берём случайный, ИЗБЕГАЯ игравшего в прошлый раз (при двух звуках — чередование).
	// Пустой список — запасной путь на одиночный FireSound (старое поведение).
	USoundBase* Sound = FireSound;
	if (FireSoundVariations.Num() > 0)
	{
		int32 Index = FMath::RandRange(0, FireSoundVariations.Num() - 1);
		if (FireSoundVariations.Num() > 1 && Index == LastFireSoundIndex)
		{
			Index = (Index + 1) % FireSoundVariations.Num();
		}
		LastFireSoundIndex = Index;
		Sound = FireSoundVariations[Index];
	}
	if (!Sound)
	{
		return;
	}

	// Разброс высоты тона: дешёвая вариативность поверх смены сэмпла (0 — выключен).
	const float Pitch = (FireSoundPitchVariation > 0.0f)
		? FMath::RandRange(1.0f - FireSoundPitchVariation, 1.0f + FireSoundPitchVariation)
		: 1.0f;

	// Громкость эффектов с экрана настроек (ADR-062).
	UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(),
		FireSoundVolume * UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe(), Pitch);
}

void ARangedWeapon::PlayFireVisuals(const FVector& TraceEnd, bool bPlaySound)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Звук выстрела (для ИИ бандита; игрок проигрывает его сам в Fire()).
	if (bPlaySound)
	{
		PlayFireSound();
	}

	if (!bEnableFireVisuals)
	{
		return;
	}

	EnsureFXMaterial();

	const FVector Muzzle = GetMuzzleLocation();

	// Вспышка: сфера у дула на MuzzleFlashDuration (таймер переиспользуется — повторный
	// выстрел просто продлевает показ).
	if (MuzzleFlashMesh)
	{
		MuzzleFlashMesh->SetWorldLocation(Muzzle);
		MuzzleFlashMesh->SetWorldScale3D(FVector(MuzzleFlashSize));
		MuzzleFlashMesh->SetVisibility(true);
		World->GetTimerManager().SetTimer(MuzzleFlashTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (MuzzleFlashMesh)
				{
					MuzzleFlashMesh->SetVisibility(false);
				}
			}),
			MuzzleFlashDuration, /*bLoop=*/false);
	}

	// След пули: цилиндр (100 см по Z у базового меша) растягивается от дула до TraceEnd.
	if (TracerMesh)
	{
		const FVector Segment = TraceEnd - Muzzle;
		const float Length = Segment.Size();
		if (Length > 1.0f)
		{
			const FVector Dir = Segment / Length;
			TracerMesh->SetWorldLocation(Muzzle + Segment * 0.5f);
			TracerMesh->SetWorldRotation(FRotationMatrix::MakeFromZ(Dir).Rotator());
			TracerMesh->SetWorldScale3D(FVector(TracerThickness / 100.0f, TracerThickness / 100.0f, Length / 100.0f));
			TracerMesh->SetVisibility(true);
			World->GetTimerManager().SetTimer(TracerTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (TracerMesh)
					{
						TracerMesh->SetVisibility(false);
					}
				}),
				TracerDuration, /*bLoop=*/false);
		}
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

int32 ARangedWeapon::DrainReserveAmmo()
{
	// Фикс п.6 отчёта 23.08 (единая бухгалтерия патронов) — см. комментарий в заголовке.
	const int32 Drained = CurrentAmmoReserve;
	CurrentAmmoReserve = 0;
	if (Drained > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("%s: reserve drained (%d rounds -> backpack)"), *GetName(), Drained);
	}
	return Drained;
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

	// Вариант A прицеливания (фидбек Рината 07-05): носитель плавно доворачивается корпусом
	// на цель реального выстрела (CanFire/цель уже проверены). Игрок и любой гуманоид.
	if (AMasterHumanoidCharacter* Wielder = Cast<AMasterHumanoidCharacter>(GetInstigator()))
	{
		Wielder->StartAimTurnTo(FiringTarget);
		// Анимация отдачи (Build 1.1). Ассета нет — просто не проиграется, выстрел не страдает.
		Wielder->PlayFireMontage();
	}

	FHitResult HitResult;
	FVector TracerEnd = FiringTarget->GetActorLocation(); // дефолт следа — к цели
	if (PerformLineTrace(FiringTarget, HitResult))
	{
		TracerEnd = HitResult.ImpactPoint; // след до реальной точки попадания
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

	// Вспышка у дула + след пули (D2). Звук здесь свой (ниже) — не дублируем.
	PlayFireVisuals(TracerEnd, /*bPlaySound=*/false);

	// Лёгкая тряска камеры при выстреле ИГРОКА (D5). У бандита instigator — не игрок.
	if (FireShakeTrauma > 0.0f)
	{
		if (APlayerCharacter* PlayerWielder = Cast<APlayerCharacter>(GetInstigator()))
		{
			PlayerWielder->AddCameraShake(FireShakeTrauma);
		}
	}

	// Звук выстрела — только при реальном выстреле (CanFire() уже прошёл, обойма не пуста).
	PlayFireSound();

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

	// Отладочная DrawDebugLine убрана (Этап D): её заменил реальный след пули (PlayFireVisuals).

	return bHit;
}
