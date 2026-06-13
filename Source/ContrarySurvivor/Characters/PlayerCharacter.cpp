// Fill out your copyright notice in the Description page of Project Settings.


// PlayerCharacter.cpp
// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h" // Enhanced Input
#include "ContrarySurvivor/Components/StatsComponent.h"

APlayerCharacter::APlayerCharacter()
{
    

    // Create Spring Arm Component
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArmComponent->SetupAttachment(RootComponent); // Attaching to RootComponent (CapsuleComponent)
    SpringArmComponent->TargetArmLength = 1500.0f;      // Distance to Character
    SpringArmComponent->SetRelativeRotation(FRotator(-70.f, 90.f, 0.f)); //Sets isometric view

    SpringArmComponent->bDoCollisionTest = false;
    //Disable collision chek for springarm. If true -> When spring arm is overlaped by something -> Camera movese closer to player

    // Create Camera Component
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName); // Attach to   SpringArm
    CameraComponent->bUsePawnControlRotation = false;                                      // Camera not rotates whith character

    
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    //Sets that camera is not rotates whith controller

    //Parameters initialasation (устаревшие инлайн-поля, см. заголовок)
    Hunger = 100.0f;
    Thirst = 100.0f;

    // Компонент статов игрока (ADR-015). Источник истины по HP/выживанию.
    Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));
    // У игрока (в отличие от врага) деградация голода/жажды включена (GDD §7.3).
    Stats->SetSurvivalDegradationEnabled(true);

    SetUpMovement();
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("Compiler is working correctly"));

    // Инициализируем HP игрока через UStatsComponent (источник истины).
    if (Stats)
    {
        Stats->InitHealth(PlayerMaxHealth, /*bSetToMax=*/true);
    }

    // Стартовое оружие (Фаза 1: автоэкипировка пистолета вместо подбора с земли).
    EquipDefaultWeapon();
}

float APlayerCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // Намеренно НЕ зовём Super (инлайн-Health базы): единственный источник истины по HP
    // игрока — UStatsComponent. Death/респаун повесим на Stats->OnDeath (Пункт 3).
    if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
    {
        return 0.0f;
    }

    const float Applied = Stats->ApplyDamage(DamageAmount);

    UE_LOG(LogTemp, Log, TEXT("Player took %.1f damage. Health: %.1f/%.1f"),
        Applied, Stats->GetHealth(), Stats->GetMaxHealth());

    return Applied;
}

void APlayerCharacter::EquipDefaultWeapon()
{
    if (!DefaultWeaponClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipDefaultWeapon: DefaultWeaponClass not set, skipping"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Спавним оружие. Owner/Instigator — игрок; EquipWeapon довыставит Instigator и прикрепит к сокету.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AMasterWeapon* SpawnedWeapon = World->SpawnActor<AMasterWeapon>(
        DefaultWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

    if (SpawnedWeapon)
    {
        // EquipWeapon крепит оружие к WeaponSocketName на TorsoMesh и выставляет CurrentWeapon.
        EquipWeapon(SpawnedWeapon);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipDefaultWeapon: failed to spawn DefaultWeaponClass"));
    }
}


void APlayerCharacter::SetUpMovement()
{
    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 380.0f, 0.0f); // ...at this rotation rate
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}