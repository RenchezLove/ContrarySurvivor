// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Components/MeleeSectorIndicatorComponent.h"
#include "AMeleeWeapon.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

UMeleeSectorIndicatorComponent::UMeleeSectorIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Локальная X вниз: проекция на землю, ось U материала = взгляд игрока, V = «вправо»
	// (вывод — в класс-комментарии, сверено с шейдером DeferredDecal.usf).
	SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	// Габарит по X — глубина проекции, по Y и Z — радиус удара (пересчитывается каждый кадр
	// из дальности ножа). Стартовое значение неважно: до первого показа декаль невидима.
	DecalSize = FVector(ProjectionDepth, 100.0f, 100.0f);

	// Подсветка появляется только в бою — стартуем погашенными.
	SetVisibility(false);

	// Материал НЕ назначаем в конструкторе: грузим лениво при первом показе
	// (GetOrCreateSectorMID), чтобы отсутствие ассета не мешало запуску игры.
}

void UMeleeSectorIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	SetRelativeLocation(FVector::ZeroVector); // центр декали = позиция игрока (центр капсулы)
	SetVisibility(false);
}

void UMeleeSectorIndicatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Имена скалярных параметров материала — контракт с unreal-operator (см. класс-коммент).
	// Локальные static: FName-таблица движка к первому тику уже поднята (глобальные FName
	// строятся до неё и это известная ловушка).
	static const FName ParamSectorHalfAngleRad(TEXT("SectorHalfAngleRad"));
	static const FName ParamInnerRadiusFrac(TEXT("InnerRadiusFrac"));
	static const FName ParamOpacity(TEXT("Opacity"));

	if (!bShowMeleeSector)
	{
		HideSector();
		return;
	}

	// Носитель — гуманоид (у него живёт экипированное оружие).
	const AMasterHumanoidCharacter* Wielder = Cast<AMasterHumanoidCharacter>(GetOwner());
	if (!Wielder)
	{
		HideSector();
		return;
	}

	// Условие 1: в руках холодное оружие.
	const AMeleeWeapon* Melee = Cast<AMeleeWeapon>(Wielder->GetCurrentWeapon());
	if (!Melee)
	{
		HideSector();
		return;
	}

	// Условие 2: есть захваченная цель (тот же лок, что показывает HUD и к которому
	// доворачивается удар).
	const AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Wielder->GetController());
	AActor* LockedTarget = PC ? PC->GetCurrentTarget() : nullptr;
	if (!IsValid(LockedTarget))
	{
		HideSector();
		return;
	}

	// Условие 3: есть материал подсветки.
	UMaterialInstanceDynamic* MID = GetOrCreateSectorMID();
	if (!MID)
	{
		HideSector();
		return;
	}

	// --- Геометрия ровно та же, что в AMeleeWeapon::Fire ---
	// Урон проходит при ЦентрДистанция <= РадиусКапсулыНосителя + MeleeRange + РадиусКапсулыЦели,
	// поэтому радиус подсветки считаем этой же суммой. Радиус цели берём у захваченной цели —
	// подсветка показывает область именно для неё.
	float WielderRadius = 0.0f;
	if (const UCapsuleComponent* WielderCapsule = Wielder->GetCapsuleComponent())
	{
		WielderRadius = WielderCapsule->GetScaledCapsuleRadius();
	}

	float TargetRadius = 0.0f;
	if (const ACharacter* TargetChar = Cast<ACharacter>(LockedTarget))
	{
		if (const UCapsuleComponent* TargetCapsule = TargetChar->GetCapsuleComponent())
		{
			TargetRadius = TargetCapsule->GetScaledCapsuleRadius();
		}
	}

	const float OuterRadius = FMath::Max(1.0f, WielderRadius + Melee->GetMeleeRange() + TargetRadius);
	const float HalfAngleRad = FMath::DegreesToRadians(Melee->GetMeleeSectorHalfAngleDeg());
	const float InnerFrac = bAutoInnerRadius
		? FMath::Clamp((WielderRadius + TargetRadius) / OuterRadius, 0.0f, 0.95f)
		: FMath::Clamp(InnerRadiusFrac, 0.0f, 0.95f);

	// Размер декали: X — глубина проекции вниз/вверх, Y и Z — радиус удара (одинаковые,
	// иначе круг превратился бы в эллипс и угол на экране разошёлся бы с уроном).
	if (!FMath::IsNearlyEqual(static_cast<float>(DecalSize.Y), OuterRadius)
		|| !FMath::IsNearlyEqual(static_cast<float>(DecalSize.X), ProjectionDepth))
	{
		DecalSize = FVector(ProjectionDepth, OuterRadius, OuterRadius);
		MarkRenderTransformDirty(); // прокси декали читает DecalSize через трансформ (FScene::UpdateDecalTransform)
	}

	if (!FMath::IsNearlyEqual(AppliedYawOffsetDeg, SectorYawOffsetDeg))
	{
		// Roll крутит декаль вокруг её локальной X, а та смотрит вниз — то есть вокруг вертикали.
		SetRelativeRotation(FRotator(-90.0f, 0.0f, SectorYawOffsetDeg));
		AppliedYawOffsetDeg = SectorYawOffsetDeg;
	}

	if (!FMath::IsNearlyEqual(AppliedHalfAngleRad, HalfAngleRad))
	{
		MID->SetScalarParameterValue(ParamSectorHalfAngleRad, HalfAngleRad);
		AppliedHalfAngleRad = HalfAngleRad;
	}
	if (!FMath::IsNearlyEqual(AppliedInnerFrac, InnerFrac))
	{
		MID->SetScalarParameterValue(ParamInnerRadiusFrac, InnerFrac);
		AppliedInnerFrac = InnerFrac;
	}
	if (!FMath::IsNearlyEqual(AppliedOpacity, SectorOpacity))
	{
		MID->SetScalarParameterValue(ParamOpacity, SectorOpacity);
		AppliedOpacity = SectorOpacity;
	}

	if (!IsVisible())
	{
		SetVisibility(true);
	}
}

void UMeleeSectorIndicatorComponent::HideSector()
{
	if (IsVisible())
	{
		SetVisibility(false);
	}
}

UMaterialInstanceDynamic* UMeleeSectorIndicatorComponent::GetOrCreateSectorMID()
{
	if (SectorMID)
	{
		return SectorMID;
	}

	// Материал мог быть назначен прямо в Details декали (поле Decal Material) — тогда он
	// главнее мягкой ссылки: берём его и не лезем в загрузку.
	UMaterialInterface* BaseMaterial = GetDecalMaterial();
	if (!BaseMaterial)
	{
		if (bSectorMaterialMissing || SectorMaterial.IsNull())
		{
			return nullptr; // поле пустое или ассет уже искали и не нашли — тихо, без подсветки
		}

		BaseMaterial = SectorMaterial.LoadSynchronous();
		if (!BaseMaterial)
		{
			bSectorMaterialMissing = true;
			UE_LOG(LogTemp, Warning,
				TEXT("MeleeSectorIndicator: материал подсветки '%s' не найден — сектор ближнего боя не рисуется"),
				*SectorMaterial.ToString());
			return nullptr;
		}
		SetDecalMaterial(BaseMaterial);
	}

	SectorMID = CreateDynamicMaterialInstance();
	if (!SectorMID)
	{
		bSectorMaterialMissing = true;
		UE_LOG(LogTemp, Warning,
			TEXT("MeleeSectorIndicator: не удалось создать динамический инстанс материала '%s'"),
			*BaseMaterial->GetName());
	}
	return SectorMID;
}
