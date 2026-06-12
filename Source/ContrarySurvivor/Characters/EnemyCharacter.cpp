// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

AEnemyCharacter::AEnemyCharacter()
{
	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));

	// Враг управляется AI-контроллером. Конкретный класс назначается в BP/дефолтах
	// (AEnemyAIController), здесь только включаем авто-поссесс при спавне/размещении.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Stats)
	{
		// UStatsComponent — источник истины по HP врага (инлайн-Health базы не используем).
		Stats->InitHealth(BanditMaxHealth, /*bSetToMax=*/true);
		Stats->OnDeath.AddDynamic(this, &AEnemyCharacter::HandleDeath);
	}
}

float AEnemyCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Намеренно НЕ зовём Super (инлайн-Health базы), чтобы единственным
	// источником истины по HP врага был UStatsComponent.
	if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float Applied = Stats->ApplyDamage(DamageAmount);

	UE_LOG(LogTemp, Log, TEXT("%s took %.1f damage. Health: %.1f/%.1f"),
		*GetName(), Applied, Stats->GetHealth(), Stats->GetMaxHealth());

	return Applied;
}

void AEnemyCharacter::HandleDeath()
{
	UE_LOG(LogTemp, Log, TEXT("%s died."), *GetName());

	// 1) Отключаем ИИ: освобождаем контроллер, чтобы он перестал двигать/атаковать.
	if (AController* AICtrl = GetController())
	{
		AICtrl->UnPossess();
	}

	// 2) Останавливаем движение.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 3) Отключаем коллизию капсулы (труп не блокирует игрока/трейсы по Pawn).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 4) Рэгдолл на основном меше (Head — корневой скелет из мастер-базы).
	//    Если у меша нет физ.ассета — ветка молча не даст эффекта, краша не будет.
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SkelMesh->SetCollisionObjectType(ECC_PhysicsBody);
		SkelMesh->SetAllBodiesSimulatePhysics(true);
		SkelMesh->SetSimulatePhysics(true);
		SkelMesh->WakeAllRigidBodies();
	}

	// 5) Снимаем тело с задержкой (даём отыграть рэгдолл).
	SetLifeSpan(CorpseLifeSpan);
}
