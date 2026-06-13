// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/WolfAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

// Базовая скорость бандита (дефолт CharacterMovementComponent::MaxWalkSpeed UE = 600).
// Используется как опорная для множителя скорости волка.
static constexpr float BanditBaseWalkSpeed = 600.0f;

AWolfCharacter::AWolfCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));

	// AI: волк управляется AWolfAIController (chase/attack), авто-поссесс при спавне.
	AIControllerClass = AWolfAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Капсула должна блокировать Visibility, чтобы LineTrace дальнобоя игрока (ECC_Visibility)
	// попадал по волку (та же причина, что в фиксе боя бандита, Фаза 2).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// ПЛЕЙСХОЛДЕР-меш: простой цилиндр движка, чтобы волк был виден/функционален без редактора.
	// Реальный скелет-меш волка назначит unreal-operator позже (modeler-3d делает меш).
	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(GetCapsuleComponent());
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // коллизию держит капсула
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		PlaceholderMesh->SetStaticMesh(CylinderAsset.Object);
		// Грубо «лежачий» силуэт зверя: ниже и длиннее. Чисто визуальный плейсхолдер.
		PlaceholderMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.9f));
		PlaceholderMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	}

	// Скорость ~1.3× бандита (draft).
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = BanditBaseWalkSpeed * SpeedMultiplierVsBandit;
	}
}

void AWolfCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Stats)
	{
		Stats->InitHealth(WolfMaxHealth, /*bSetToMax=*/true);
		Stats->OnDeath.AddDynamic(this, &AWolfCharacter::HandleDeath);
	}
}

float AWolfCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Источник истины по HP волка — UStatsComponent (как у бандита). Брони у волка нет.
	if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float Applied = Stats->ApplyDamage(DamageAmount);

	UE_LOG(LogTemp, Log, TEXT("%s (wolf) took %.1f damage. Health: %.1f/%.1f"),
		*GetName(), Applied, Stats->GetHealth(), Stats->GetMaxHealth());

	return Applied;
}

void AWolfCharacter::HandleDeath()
{
	UE_LOG(LogTemp, Log, TEXT("%s (wolf) died."), *GetName());

	if (AController* AICtrl = GetController())
	{
		AICtrl->UnPossess();
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Плейсхолдер-меш без физ.ассета: рэгдолла нет, просто снимаем тело с задержкой.
	SetLifeSpan(CorpseLifeSpan);
}
