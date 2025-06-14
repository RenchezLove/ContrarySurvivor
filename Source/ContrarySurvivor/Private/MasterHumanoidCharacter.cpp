// Fill out your copyright notice in the Description page of Project Settings.

#include "MasterHumanoidCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // Исправленный include

AMasterHumanoidCharacter::AMasterHumanoidCharacter()
{
 	PrimaryActorTick.bCanEverTick = true;

    MaxHealth = 100.0f;
    Health = MaxHealth;

    bIsAttacking = false;

    HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
    HeadMesh->SetupAttachment(GetMesh());

    TorsoMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TorsoMesh"));
    TorsoMesh->SetupAttachment(GetMesh());

    LegsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegsMesh"));
    LegsMesh->SetupAttachment(GetMesh());
}

void AMasterHumanoidCharacter::BeginPlay()
{
	Super::BeginPlay();
    GetCharacterMovement()->MaxWalkSpeed = 300.f;
}

void AMasterHumanoidCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMasterHumanoidCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AMasterHumanoidCharacter::Attack()
{
    UE_LOG(LogTemp, Warning, TEXT("MasterHumanoidCharacter: Attack!"));
}

float AMasterHumanoidCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (ActualDamage > 0.0f)
    {
        Health -= ActualDamage;
        Health = FMath::Max(Health, 0.0f);
        UE_LOG(LogTemp, Warning, TEXT("Health now %s"), *FString::SanitizeFloat(Health));

        if (Health <= 0.f)
        {
             UE_LOG(LogTemp, Warning, TEXT("I am dead"));
        }
    }
    return ActualDamage;
}

void AMasterHumanoidCharacter::RestoreHealth(float HealAmount)
{
    if (HealAmount <= 0.0f) return;

    Health += HealAmount;
    Health = FMath::Min(Health, MaxHealth);

     UE_LOG(LogTemp, Warning, TEXT("MasterHumanoidCharacter: RestoreHealth! Health = %f"), Health);
}

void AMasterHumanoidCharacter::UpdateCharacterAppearance()
{
    UE_LOG(LogTemp, Warning, TEXT("MasterHumanoidCharacter: UpdateCharacterAppearance!"));
}