// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/AnomalyFormation.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Retention/OnboardingComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Scene.h" // ELightUnits
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAnomalyFormation::AAnomalyFormation()
{
	PrimaryActorTick.bCanEverTick = false;

	TouchSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TouchSphere"));
	TouchSphere->InitSphereRadius(TouchRadius);
	TouchSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TouchSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TouchSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TouchSphere->SetGenerateOverlapEvents(true);
	TouchSphere->OnComponentBeginOverlap.AddDynamic(this, &AAnomalyFormation::OnTouchBegin);
	RootComponent = TouchSphere;

	// Меш и светящийся материал задаются на чертеже. Столкновений и тени нет.
	// Пока своего меша нет — небольшой шар из движка, чтобы образование было видно на уровне.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(TouchSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DefaultMesh.Succeeded())
	{
		Mesh->SetStaticMesh(DefaultMesh.Object);
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
		Mesh->SetRelativeScale3D(FVector(0.35f));
	}

	// Свечение: один мягкий точечный свет без теней — дёшево для слабых телефонов.
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(TouchSphere);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	Glow->CastShadows = false;
	Glow->IntensityUnits = ELightUnits::Unitless;
	Glow->Intensity = 2500.0f;
	Glow->AttenuationRadius = 450.0f;
	Glow->LightColor = FColor(120, 255, 90);
}

void AAnomalyFormation::BeginPlay()
{
	Super::BeginPlay();
	if (TouchSphere)
	{
		TouchSphere->SetSphereRadius(TouchRadius);
	}
	// Пока задание не взято, образований не видно; дальше видимостью правит состояние героя.
	SetFormationVisible(false);
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UChapter2Component* Chapter2 = Player->GetChapter2())
		{
			Chapter2->RefreshSampleActors();
		}
	}
}

void AAnomalyFormation::InitFormation(FName InFormationId, UMaterialInterface* GlowMaterial)
{
	FormationId = InFormationId;
	if (GlowMaterial && Mesh)
	{
		Mesh->SetMaterial(0, GlowMaterial);
	}
}

FName AAnomalyFormation::GetFormationId() const
{
	return FormationId.IsNone() ? GetFName() : FormationId;
}

void AAnomalyFormation::SetFormationVisible(bool bVisible)
{
	if (bFormationVisible == bVisible)
	{
		return;
	}
	bFormationVisible = bVisible;
	SetActorHiddenInGame(!bVisible);
	if (TouchSphere)
	{
		TouchSphere->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

bool AAnomalyFormation::TryCollect(APlayerCharacter* Player)
{
	UChapter2Component* Chapter2 = Player ? Player->GetChapter2() : nullptr;
	// CollectSample сам кладёт предмет «Аномальное образование» в сумку.
	if (!Chapter2 || !bFormationVisible || !Chapter2->CollectSample(GetFormationId()))
	{
		return false;
	}
	SetFormationVisible(false);
	if (UOnboardingComponent* Hints = Player->GetOnboarding())
	{
		Hints->ShowTransientHint(UChapter2Settings::Get()->FormationNameText); // [226]
	}
	return true;
}

void AAnomalyFormation::OnTouchBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryCollect(Cast<APlayerCharacter>(OtherActor));
}
