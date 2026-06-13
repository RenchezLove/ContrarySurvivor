// Fill out your copyright notice in the Description page of Project Settings.

#include "MasterHumanoidCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h"
#include "UInventoryComponent.h"
#include "AArmor.h"
#include "AHeadArmor.h"
#include "ATorsoArmor.h"
#include "APantsArmor.h"


AMasterHumanoidCharacter::AMasterHumanoidCharacter()
{
 	PrimaryActorTick.bCanEverTick = true;

    MaxHealth = 100.0f;
    Health = MaxHealth;

    bIsAttacking = false;

    CurrentWeapon = nullptr;
    WeaponSocketName = FName("WeaponSocket");

    EquippedHeadArmor  = nullptr;
    EquippedTorsoArmor = nullptr;
    EquippedPantsArmor = nullptr;

    HeadMesh = GetMesh();

    TorsoMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TorsoMesh"));
    TorsoMesh->SetupAttachment(HeadMesh);

    LegsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegsMesh"));
    LegsMesh->SetupAttachment(HeadMesh);

    Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
}

void AMasterHumanoidCharacter::BeginPlay()
{
	Super::BeginPlay();
    BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
}

void AMasterHumanoidCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMasterHumanoidCharacter::EquipWeapon(AMasterWeapon* NewWeapon)
{
    if (!NewWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipWeapon: NewWeapon is null"));
        return;
    }

    // Снимаем текущее оружие если есть
    if (CurrentWeapon)
    {
        UnequipWeapon();
    }

    CurrentWeapon = NewWeapon;

    // Крепим оружие к сокету на торсе
    if (TorsoMesh)
    {
        CurrentWeapon->AttachToComponent(TorsoMesh,
            FAttachmentTransformRules::SnapToTargetIncludingScale,
            WeaponSocketName);
    }

    // Устанавливаем владельца оружия
    CurrentWeapon->SetInstigator(this);

    UE_LOG(LogTemp, Warning, TEXT("EquipWeapon: Equipped %s"), *CurrentWeapon->GetName());
}

void AMasterHumanoidCharacter::UnequipWeapon()
{
    if (!CurrentWeapon) return;

    CurrentWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    CurrentWeapon = nullptr;

    UE_LOG(LogTemp, Warning, TEXT("UnequipWeapon: Weapon removed"));
}

void AMasterHumanoidCharacter::EquipArmor(AArmor* Armor)
{
    if (!Armor)
    {
        return;
    }

    // Раскладываем по слотам по типу. Визуал брони (меш) — Фаза 4; здесь только параметры.
    if (Armor->IsA(AHeadArmor::StaticClass()))
    {
        EquippedHeadArmor = Armor;
    }
    else if (Armor->IsA(ATorsoArmor::StaticClass()))
    {
        EquippedTorsoArmor = Armor;
    }
    else if (Armor->IsA(APantsArmor::StaticClass()))
    {
        EquippedPantsArmor = Armor;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipArmor: unknown armor slot for %s"), *Armor->GetName());
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("EquipArmor: %s (protection %.1f). Total armor now %.1f"),
        *Armor->GetName(), Armor->GetArmorProtection(), GetTotalArmorProtection());
}

float AMasterHumanoidCharacter::GetTotalArmorProtection() const
{
    float Total = 0.0f;
    if (EquippedHeadArmor)  { Total += EquippedHeadArmor->GetArmorProtection(); }
    if (EquippedTorsoArmor) { Total += EquippedTorsoArmor->GetArmorProtection(); }
    if (EquippedPantsArmor) { Total += EquippedPantsArmor->GetArmorProtection(); }
    return Total;
}

void AMasterHumanoidCharacter::FireCurrentWeapon(AActor* Target)
{
    if (!CurrentWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("FireCurrentWeapon: No weapon equipped"));
        return;
    }

    CurrentWeapon->Fire(Target);
}

void AMasterHumanoidCharacter::ReloadCurrentWeapon()
{
    if (!CurrentWeapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("ReloadCurrentWeapon: No weapon equipped"));
        return;
    }

    CurrentWeapon->Reload();
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
            HandleDeath();
        }
    }
    return ActualDamage;
}

void AMasterHumanoidCharacter::HandleDeath()
{
    // Минимальная заглушка смерти (Фаза 1): без краша, без респауна (респаун — Фаза 2).
    // Глушим движение, отключаем коллизию капсулы. Тело остаётся на сцене.
    UE_LOG(LogTemp, Warning, TEXT("%s: HandleDeath (stub)"), *GetName());

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
    }

    if (AController* Ctrl = GetController())
    {
        DisableInput(Cast<APlayerController>(Ctrl));
    }

    SetActorEnableCollision(false);
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

void AMasterHumanoidCharacter::SetSprint(bool bIsSprinting)
{
    IsSprinting = bIsSprinting;
    
    GetCharacterMovement()->MaxWalkSpeed = 
        IsSprinting ? BaseWalkSpeed * SprintMultiplier : BaseWalkSpeed;
        
    UE_LOG(LogTemp, Warning, TEXT("Sprint state changed to: %s"), IsSprinting ? TEXT("true") : TEXT("false"));
}
