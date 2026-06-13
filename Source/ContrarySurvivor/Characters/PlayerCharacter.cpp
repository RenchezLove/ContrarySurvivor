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
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "UInventoryComponent.h"
#include "AMasterInventoryItem.h"
#include "AMeleeWeapon.h"
#include "AHeadArmor.h"
#include "ATorsoArmor.h"
#include "APantsArmor.h"
#include "Kismet/GameplayStatics.h"

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

    // Нож доступен «из коробки» без нового .uasset: дефолт = конкретный AMeleeWeapon.
    // BP игрока может переопределить (например, на BP_Knife) в дефолтах.
    DefaultMeleeWeaponClass = AMeleeWeapon::StaticClass();

    // Дефолтная броня (Фаза 3, для наблюдаемости снижения урона). Конкретные классы с
    // черновыми значениями защиты (Head 5 / Torso 12 / Pants 8).
    DefaultHeadArmorClass  = AHeadArmor::StaticClass();
    DefaultTorsoArmorClass = ATorsoArmor::StaticClass();
    DefaultPantsArmorClass = APantsArmor::StaticClass();

    SetUpMovement();
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("Compiler is working correctly"));

    // Запоминаем стартовый трансформ — фолбэк-точка респауна, если сейва ещё нет.
    InitialSpawnTransform = GetActorTransform();

    // Инициализируем HP игрока через UStatsComponent (источник истины).
    if (Stats)
    {
        Stats->InitHealth(PlayerMaxHealth, /*bSetToMax=*/true);
        // Смерть игрока -> респаун (GDD §7.8).
        Stats->OnDeath.AddDynamic(this, &APlayerCharacter::HandleDeath);
    }

    // Стартовое оружие (Фаза 1: автоэкипировка пистолета вместо подбора с земли).
    EquipDefaultWeapon();

    // Нож держим «в кобуре» (скрыт), переключение по SwitchWeapon (Фаза 3).
    SpawnMeleeWeapon();

    // Дефолтная броня (Фаза 3): снижение урона наблюдаемо без экип-UI.
    EquipDefaultArmor();
}

float APlayerCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // Намеренно НЕ зовём Super (инлайн-Health базы): единственный источник истины по HP
    // игрока — UStatsComponent. Death/респаун повесим на Stats->OnDeath (Пункт 3).
    if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
    {
        return 0.0f;
    }

    // GDD §7.2: броня снижает урон. ПРОЦЕНТНАЯ формула (решение Рината):
    // Final = Incoming * (1 - clamp(SumArmorFraction, 0, Cap)). Без min-1 неуязвимости.
    const float Reduced = ComputeArmoredDamage(DamageAmount);

    const float Applied = Stats->ApplyDamage(Reduced);

    UE_LOG(LogTemp, Log, TEXT("Player took %.1f dmg (incoming %.1f, armor frac %.2f cap %.2f). Health: %.1f/%.1f"),
        Applied, DamageAmount, GetTotalArmorProtection(), ArmorReductionCap, Stats->GetHealth(), Stats->GetMaxHealth());

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
        RangedWeaponInstance = SpawnedWeapon;
        // EquipWeapon крепит оружие к WeaponSocketName на TorsoMesh и выставляет CurrentWeapon.
        EquipWeapon(SpawnedWeapon);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipDefaultWeapon: failed to spawn DefaultWeaponClass"));
    }
}

void APlayerCharacter::SpawnMeleeWeapon()
{
    if (!DefaultMeleeWeaponClass)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AMasterWeapon* Knife = World->SpawnActor<AMasterWeapon>(
        DefaultMeleeWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

    if (Knife)
    {
        MeleeWeaponInstance = Knife;
        // Нож нужен для урона (Instigator), но не активен: гасим видимость/коллизию.
        Knife->SetInstigator(this);
        Knife->SetActorHiddenInGame(true);
        Knife->SetActorEnableCollision(false);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnMeleeWeapon: failed to spawn DefaultMeleeWeaponClass"));
    }
}

void APlayerCharacter::EquipDefaultArmor()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Универсальный лямбда-помощник: спавн предмета брони и экип в слот.
    auto SpawnAndEquip = [&](TSubclassOf<AArmor> ArmorClass)
    {
        if (!ArmorClass)
        {
            return;
        }
        AArmor* Armor = World->SpawnActor<AArmor>(ArmorClass, GetActorLocation(), GetActorRotation(), SpawnParams);
        if (Armor)
        {
            // Броня — не игровой объект на сцене (Фаза 3): прячем визуал/коллизию,
            // используем только параметры защиты. Полноценная экип/визуал — Фаза 4.
            Armor->SetActorHiddenInGame(true);
            Armor->SetActorEnableCollision(false);
            EquipArmor(Armor);
        }
    };

    SpawnAndEquip(DefaultHeadArmorClass);
    SpawnAndEquip(DefaultTorsoArmorClass);
    SpawnAndEquip(DefaultPantsArmorClass);
}

void APlayerCharacter::SwitchWeapon()
{
    // Тоггл между дальним (пистолет) и ближним (нож) оружием.
    AMasterWeapon* Active = GetCurrentWeapon();
    AMasterWeapon* Target = (Active == MeleeWeaponInstance) ? RangedWeaponInstance : MeleeWeaponInstance;

    if (!Target || Target == Active)
    {
        UE_LOG(LogTemp, Warning, TEXT("SwitchWeapon: no alternate weapon to switch to"));
        return;
    }

    // Прячем снимаемое, показываем экипируемое (EquipWeapon снимет текущее и прикрепит новое).
    if (Active)
    {
        Active->SetActorHiddenInGame(true);
        Active->SetActorEnableCollision(false);
    }

    EquipWeapon(Target);
    Target->SetActorHiddenInGame(false);

    UE_LOG(LogTemp, Log, TEXT("SwitchWeapon: now wielding %s"), *Target->GetName());
}


// ---------------------------------------------------------------------------
// Сейв / смерть / респаун (GDD §7.8)
// ---------------------------------------------------------------------------

bool APlayerCharacter::SaveGame()
{
    if (!Stats)
    {
        return false;
    }

    UContrarySaveGame* Save = Cast<UContrarySaveGame>(
        UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
    if (!Save)
    {
        return false;
    }

    Save->bHasData = true;
    Save->Health = Stats->GetHealth();
    Save->MaxHealth = Stats->GetMaxHealth();
    Save->Hunger = Stats->GetHunger();
    Save->Thirst = Stats->GetThirst();
    Save->Money = Stats->GetMoney();
    Save->PlayerLocation = GetActorLocation();
    Save->PlayerRotation = GetActorRotation();

    // ЗАДЕЛ: инвентарь сериализуем как пути классов предметов рюкзака.
    Save->InventoryItemClassPaths.Reset();
    if (Inventory)
    {
        for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
        {
            if (Item)
            {
                Save->InventoryItemClassPaths.Add(Item->GetClass()->GetPathName());
            }
        }
    }

    const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex);
    UE_LOG(LogTemp, Log, TEXT("APlayerCharacter::SaveGame -> slot '%s' : %s"),
        *SaveSlotName, bOk ? TEXT("OK") : TEXT("FAIL"));
    return bOk;
}

bool APlayerCharacter::HasSaveGame() const
{
    return UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex);
}

bool APlayerCharacter::LoadGame()
{
    if (!HasSaveGame())
    {
        return false;
    }

    UContrarySaveGame* Save = Cast<UContrarySaveGame>(
        UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
    if (!Save || !Save->bHasData)
    {
        return false;
    }

    ApplySaveData(Save);
    return true;
}

void APlayerCharacter::ApplySaveData(const UContrarySaveGame* Save)
{
    if (!Save)
    {
        return;
    }

    if (Stats)
    {
        Stats->RestoreState(Save->Health, Save->Hunger, Save->Thirst, Save->Money);
    }

    // Телепорт в точку респауна (последний костёр/сейв). Гасим скорость.
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }
    SetActorLocationAndRotation(Save->PlayerLocation, Save->PlayerRotation,
        /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
}

void APlayerCharacter::ApplyDeathInventoryPenalty()
{
    // ВНИМАНИЕ (эскалация): UInventoryComponent не различает экип/неэкип и категории
    // (расходник/ресурс/броня). Экипированное оружие хранится отдельно (CurrentWeapon на
    // базе) и в InventoryItems НЕ лежит — поэтому оно сохраняется автоматически.
    // Здесь теряется доля ВСЕХ предметов рюкзака (MVP-приближение GDD §7.8).
    if (!Inventory || DeathItemLossPercent <= 0.0f)
    {
        return;
    }

    TArray<AMasterInventoryItem*> Items = Inventory->GetInventoryItems();
    const int32 Total = Items.Num();
    if (Total <= 0)
    {
        return;
    }

    const int32 LoseCount = FMath::FloorToInt(Total * DeathItemLossPercent);
    for (int32 i = 0; i < LoseCount; ++i)
    {
        // Снимаем с конца массива (без выпадения лут-мешка — MVP).
        AMasterInventoryItem* Item = Items[Total - 1 - i];
        Inventory->RemoveItem(Item);
    }

    UE_LOG(LogTemp, Log, TEXT("Death penalty: lost %d of %d backpack items (%.0f%%)"),
        LoseCount, Total, DeathItemLossPercent * 100.0f);
}

void APlayerCharacter::HandleDeath()
{
    UE_LOG(LogTemp, Warning, TEXT("APlayerCharacter: death -> respawn"));

    // 1) Потеря доли расходников рюкзака (экипировка сохраняется).
    ApplyDeathInventoryPenalty();

    // 2) Респаун: восстановление из последнего сейва (костёр). Если сейва нет —
    //    фолбэк на стартовый трансформ + полные статы.
    if (!LoadGame())
    {
        if (Stats)
        {
            Stats->RestoreState(Stats->GetMaxHealth(), Stats->GetSurvivalMax(),
                Stats->GetSurvivalMax(), Stats->GetMoney());
        }
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            Move->StopMovementImmediately();
        }
        SetActorTransform(InitialSpawnTransform, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Warning, TEXT("Respawn: no save found, used initial spawn transform."));
    }

    // 3) Death-респаун = полные HP/Голод/Жажда (решение game-lead). Деньги — из сейва (шаг 2).
    //    Автосейв костра пишет ЖИВЫЕ значения голода/жажды (жажда деградирует быстрее),
    //    поэтому при загрузке они «нестабильны» по таймингу — форсим в максимум здесь.
    //    ВАЖНО: только в death-ветке; обычный quit->reload (ApplySaveData) значения НЕ трогает.
    if (Stats)
    {
        Stats->SetHealth(Stats->GetMaxHealth() * RespawnHealthFraction);
        Stats->SetHunger(Stats->GetSurvivalMax() * RespawnSurvivalFraction);
        Stats->SetThirst(Stats->GetSurvivalMax() * RespawnSurvivalFraction);
        UE_LOG(LogTemp, Warning, TEXT("Respawn stats: HP %.1f/%.1f, Hunger %.1f, Thirst %.1f (frac HP %.2f / Surv %.2f)"),
            Stats->GetHealth(), Stats->GetMaxHealth(), Stats->GetHunger(), Stats->GetThirst(),
            RespawnHealthFraction, RespawnSurvivalFraction);
    }
}

void APlayerCharacter::SetUpMovement()
{
    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 380.0f, 0.0f); // ...at this rotation rate
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}