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
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "UInventoryComponent.h"
#include "AMasterInventoryItem.h"
#include "AMeleeWeapon.h"
#include "AHeadArmor.h"
#include "ATorsoArmor.h"
#include "APantsArmor.h"
#include "AConsumableItem.h"
#include "ARangedWeapon.h"
#include "ContrarySurvivor/Actors/TraderNPC.h" // FShopEntry, EShopEntryKind
#include "ContrarySurvivor/Actors/Pickup.h"    // выброс = мировой пикап (BUG3)
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h" // CloseAllUI при смерти
#include "ContrarySurvivor/ContrarySurvivor.h"  // LogQA
#include "Kismet/GameplayStatics.h"

APlayerCharacter::APlayerCharacter()
{
    

    // Камера в стиле Last Day on Earth (#20): пологий угол сверху, узкий FOV, плавный lag.
    // Конкретные значения берутся из тюнингуемых UPROPERTY (дефолты заданы в заголовке) и
    // применяются здесь + повторно в BeginPlay (ApplyCameraSettings) на случай BP-оверрайда.

    // Create Spring Arm Component
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArmComponent->SetupAttachment(RootComponent); // Attaching to RootComponent (CapsuleComponent)
    SpringArmComponent->TargetArmLength = CameraArmLength;          // Дистанция камеры (LDoE ~1000)
    SpringArmComponent->SetRelativeRotation(CameraBoomRotation);    // Угол LDoE (Pitch ~-55, Yaw 90)

    SpringArmComponent->bDoCollisionTest = false;
    //Disable collision chek for springarm. If true -> When spring arm is overlaped by something -> Camera movese closer to player

    // Камера фиксирована относительно мира (top-down/LDoE): не наследует вращение пешки/контроллера.
    SpringArmComponent->bUsePawnControlRotation = false;
    SpringArmComponent->bInheritPitch = false;
    SpringArmComponent->bInheritYaw   = false;
    SpringArmComponent->bInheritRoll  = false;

    // Плавное отставание (lag) — лёгкое «оживление» движения камеры.
    SpringArmComponent->bEnableCameraLag       = bEnableCameraLag;
    SpringArmComponent->CameraLagSpeed         = CameraLagSpeed;
    SpringArmComponent->CameraLagMaxDistance   = CameraLagMaxDistance;

    // Create Camera Component
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName); // Attach to   SpringArm
    CameraComponent->bUsePawnControlRotation = false;                                      // Camera not rotates whith character
    CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
    CameraComponent->SetFieldOfView(CameraFieldOfView);                                    // Узкий FOV (LDoE-сжатие)

    
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

    // Журнал квестов (Фаза 5). C++-сабобъект — детерминированно, без BP.
    Quests = CreateDefaultSubobject<UQuestComponent>(TEXT("QuestComponent"));

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

    // Применяем тюнингуемые параметры камеры (#20) — учитывает BP-оверрайды, не только дефолты ctor.
    ApplyCameraSettings();

    // Запоминаем стартовый трансформ — фолбэк-точка респауна, если сейва ещё нет.
    InitialSpawnTransform = GetActorTransform();

    // Инициализируем HP игрока через UStatsComponent (источник истины).
    if (Stats)
    {
        Stats->InitHealth(PlayerMaxHealth, /*bSetToMax=*/true);
        // НОВЫЙ игрок стартует с GDD §7.6 = 50 денег. Делаем это явно в коде (не полагаясь на
        // дефолт компонента/возможный оверрайд в BP), но ТОЛЬКО как стартовое значение нового
        // персонажа: BeginPlay сейв не загружает, загрузка (RestoreState) идёт позже отдельно.
        Stats->InitMoney(StartingMoney);
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

void APlayerCharacter::ApplyCameraSettings()
{
    if (SpringArmComponent)
    {
        SpringArmComponent->TargetArmLength = CameraArmLength;
        SpringArmComponent->SetRelativeRotation(CameraBoomRotation);
        SpringArmComponent->bDoCollisionTest = false;
        SpringArmComponent->bUsePawnControlRotation = false;
        SpringArmComponent->bInheritPitch = false;
        SpringArmComponent->bInheritYaw   = false;
        SpringArmComponent->bInheritRoll  = false;
        SpringArmComponent->bEnableCameraLag     = bEnableCameraLag;
        SpringArmComponent->CameraLagSpeed       = CameraLagSpeed;
        SpringArmComponent->CameraLagMaxDistance = CameraLagMaxDistance;
    }
    if (CameraComponent)
    {
        CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
        CameraComponent->SetFieldOfView(CameraFieldOfView);
    }
}

void APlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // --- Процедурные эффекты камеры (#28): дыхание + look-ahead ---
    // Оба ЕДВА ЗАМЕТНЫ (запрос «чуть-чуть»). Подмешиваем в SpringArm->TargetOffset (world space),
    // не трогая базовую позицию/поворот руки — управление/прицел не затрагиваются.
    if (!SpringArmComponent)
    {
        return;
    }

    FVector DesiredOffset = FVector::ZeroVector;

    // Look-ahead: целевое смещение по горизонтали в сторону движения пешки.
    if (bEnableCameraLookAhead)
    {
        FVector Velocity = GetVelocity();
        Velocity.Z = 0.0f;
        const float Speed = Velocity.Size();
        FVector TargetLookAhead = FVector::ZeroVector;
        if (Speed > LookAheadSpeedThreshold)
        {
            TargetLookAhead = Velocity.GetSafeNormal() * LookAheadAmount;
        }
        // Плавно подмешиваем (в т.ч. возврат к нулю при остановке).
        CameraLookAheadOffset = FMath::VInterpTo(CameraLookAheadOffset, TargetLookAhead, DeltaTime, LookAheadInterpSpeed);
        DesiredOffset += CameraLookAheadOffset;
    }
    else
    {
        CameraLookAheadOffset = FVector::ZeroVector;
    }

    // «Дыхание»: крошечный вертикальный синусный боб — камера кажется живой.
    if (bEnableCameraBreathing)
    {
        CameraBreathingTime += DeltaTime * BreathingSpeed;
        DesiredOffset.Z += FMath::Sin(CameraBreathingTime) * BreathingAmplitude;
    }

    SpringArmComponent->TargetOffset = DesiredOffset;
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

void APlayerCharacter::EquipTestArmor()
{
    // Консольная команда: (пере)надеть дефолтную броню всех слотов. Использует тот же путь,
    // что и автоэкип в BeginPlay (спавн DefaultHead/Torso/PantsArmorClass + EquipArmor),
    // т.е. подменяет модульные меши слотов на ArmorMesh_Equipped брони.
    EquipDefaultArmor();
    UE_LOG(LogTemp, Log, TEXT("EquipTestArmor: equipped default armor. Total armor %.2f"),
        GetTotalArmorProtection());
}

void APlayerCharacter::UnequipTestArmor()
{
    // Консольная команда: снять броню всех слотов (возврат базовых мешей тела).
    UnequipArmor(EArmorSlot::Head);
    UnequipArmor(EArmorSlot::Torso);
    UnequipArmor(EArmorSlot::Legs);
    UE_LOG(LogTemp, Log, TEXT("UnequipTestArmor: all slots cleared. Total armor %.2f"),
        GetTotalArmorProtection());
}

// ---------------------------------------------------------------------------
// Действия UI-инвентаря (Фаза 4) — вызываются из AContrarySurvivorHUD по клику
// ---------------------------------------------------------------------------

void APlayerCharacter::Inv_UseBackpackItem(AMasterInventoryItem* Item)
{
    if (!Item || !Inventory)
    {
        return;
    }

    switch (Item->GetItemCategory())
    {
        case EItemCategory::Armor:
        {
            // Броня -> надеть в её слот (подмена меша) и пометить экипированной
            // (чтобы не теряться при смерти и не дублироваться в списке рюкзака).
            if (AArmor* Armor = Cast<AArmor>(Item))
            {
                EquipArmor(Armor);
                Inventory->SetItemEquipped(Armor, true);
                UE_LOG(LogTemp, Log, TEXT("Inv: equipped %s"), *Armor->GetName());
            }
            break;
        }
        case EItemCategory::Consumable:
        {
            // Расходник -> применить эффект (еда +Hunger / вода +Thirst) и израсходовать.
            if (AConsumableItem* Cons = Cast<AConsumableItem>(Item))
            {
                if (Cons->ApplyConsumeEffect(Stats))
                {
                    Inventory->RemoveItem(Item);
                    Item->Destroy();
                    UE_LOG(LogTemp, Log, TEXT("Inv: consumed %s"), *Cons->GetName());
                }
            }
            break;
        }
        default:
            UE_LOG(LogTemp, Log, TEXT("Inv: item %s has no use action (category %d)"),
                *Item->GetName(), (int32)Item->GetItemCategory());
            break;
    }
}

void APlayerCharacter::Inv_DropItem(AMasterInventoryItem* Item)
{
    if (!Item || !Inventory)
    {
        return;
    }

    // Если выбрасываем экипированную броню — сперва снять (вернуть меш слота к базовому).
    if (AArmor* Armor = Cast<AArmor>(Item))
    {
        if (Inventory->IsItemEquipped(Armor))
        {
            UnequipArmor(Armor->GetArmorSlot());
        }
    }

    Inventory->RemoveItem(Item);

    // BUG3-фикс (решение Рината/game-lead): выброс = заспавнить предмет МИРОВЫМ пикапом
    // у ног игрока (НЕ Destroy). Подобрать обратно можно клавишей E. Предмет остаётся
    // скрытым/без коллизии и переносится пикапом как данные (его визуал — меш пикапа).
    UWorld* World = GetWorld();
    if (!World)
    {
        Item->Destroy(); // нет мира — фолбэк, не оставляем висящий предмет
        return;
    }

    Item->SetActorHiddenInGame(true);
    Item->SetActorEnableCollision(false);

    // Чуть впереди игрока и ниже (примерно к ногам), чтобы мешок был виден.
    const FVector DropLoc = GetActorLocation()
        + GetActorForwardVector() * DropForwardOffset
        + FVector(0.0f, 0.0f, -DropDownOffset);

    FActorSpawnParameters Sp;
    Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APickup* Dropped = World->SpawnActor<APickup>(APickup::StaticClass(), DropLoc, FRotator::ZeroRotator, Sp);
    if (Dropped)
    {
        Dropped->InitLoot(0.0f, Item);
        UE_LOG(LogTemp, Log, TEXT("Inv: dropped %s as world pickup at %s"), *Item->GetName(), *DropLoc.ToString());
        UE_LOG(LogQA, Display, TEXT("QA: DROP '%s' at feet (world pickup spawned) at %s"), *Item->GetName(), *DropLoc.ToString());
    }
    else
    {
        // Пикап не заспавнился — не оставляем висящий предмет.
        Item->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("Inv: drop failed to spawn pickup for %s (destroyed item)"), *Item->GetName());
    }
}

void APlayerCharacter::Inv_UnequipSlot(EArmorSlot Slot)
{
    AArmor* Armor = GetEquippedArmor(Slot);
    UnequipArmor(Slot);

    if (Armor && Inventory)
    {
        Inventory->SetItemEquipped(Armor, false);
        // Вернуть в рюкзак как неэкипированный (без дублирования).
        if (!Inventory->GetInventoryItems().Contains(Armor))
        {
            Inventory->AddItem(Armor);
        }
        UE_LOG(LogTemp, Log, TEXT("Inv: unequipped slot %d (%s -> backpack)"),
            (int32)Slot, *Armor->GetName());
    }
}

// ---------------------------------------------------------------------------
// Магазин торговца (Фаза 4, экономика) — вызываются из HUD по клику
// ---------------------------------------------------------------------------

bool APlayerCharacter::Shop_BuyEntry(const FShopEntry& Entry)
{
    if (!Stats)
    {
        return false;
    }

    // Проверяем платёжеспособность ДО выдачи товара (clamp >=0: нельзя купить без денег).
    if (Stats->GetMoney() < Entry.Price)
    {
        UE_LOG(LogTemp, Log, TEXT("Shop: not enough money for '%s' (%.0f < %.0f)"),
            *Entry.DisplayName, Stats->GetMoney(), Entry.Price);
        return false;
    }

    if (Entry.Kind == EShopEntryKind::Ammo)
    {
        // Патроны -> резерв дальнобойного оружия игрока.
        ARangedWeapon* Ranged = Cast<ARangedWeapon>(RangedWeaponInstance);
        if (!Ranged)
        {
            Ranged = Cast<ARangedWeapon>(GetCurrentWeapon());
        }
        if (!Ranged)
        {
            UE_LOG(LogTemp, Log, TEXT("Shop: no ranged weapon to receive ammo"));
            return false; // не списываем деньги, если некуда класть патроны
        }
        Ranged->AddReserveAmmo(Entry.AmmoAmount);
    }
    else
    {
        // Предмет -> в рюкзак (скрытый, как тестовые/лут-предметы).
        if (!Entry.ItemClass)
        {
            return false;
        }
        UWorld* World = GetWorld();
        if (!World || !Inventory)
        {
            return false;
        }

        FActorSpawnParameters Sp;
        Sp.Owner = this;
        Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AMasterInventoryItem* Bought = World->SpawnActor<AMasterInventoryItem>(
            Entry.ItemClass, GetActorLocation(), GetActorRotation(), Sp);
        if (!Bought)
        {
            return false;
        }

        // Если это расходник и задан тип — выставляем (еда/вода/аптечка).
        if (Entry.bApplyConsumableType)
        {
            if (AConsumableItem* Cons = Cast<AConsumableItem>(Bought))
            {
                Cons->ConsumableType = Entry.ConsumableType;
            }
        }
        if (Bought->ItemName.IsEmpty())
        {
            Bought->ItemName = Entry.DisplayName;
        }

        Bought->SetActorHiddenInGame(true);
        Bought->SetActorEnableCollision(false);
        Inventory->AddItem(Bought);
    }

    // Списываем цену (SpendMoney clamp >=0 + бродкаст HUD).
    Stats->SpendMoney(Entry.Price);
    UE_LOG(LogTemp, Log, TEXT("Shop: bought '%s' for %.0f. Money left %.0f"),
        *Entry.DisplayName, Entry.Price, Stats->GetMoney());
    UE_LOG(LogQA, Display, TEXT("QA: BUY '%s' for %.0f, balance %.0f"),
        *Entry.DisplayName, Entry.Price, Stats->GetMoney());
    return true;
}

void APlayerCharacter::Shop_SellItem(AMasterInventoryItem* Item, float SellPrice)
{
    if (!Item || !Inventory || !Stats)
    {
        return;
    }

    // Если продаём экипированную броню — сперва снять (вернуть меш слота к базовому).
    if (AArmor* Armor = Cast<AArmor>(Item))
    {
        if (Inventory->IsItemEquipped(Armor))
        {
            UnequipArmor(Armor->GetArmorSlot());
            Inventory->SetItemEquipped(Armor, false);
        }
    }

    const FString SoldName = Item->GetName();
    Inventory->RemoveItem(Item);
    Stats->AddMoney(SellPrice);
    UE_LOG(LogTemp, Log, TEXT("Shop: sold %s for %.0f. Money now %.0f"),
        *SoldName, SellPrice, Stats->GetMoney());
    UE_LOG(LogQA, Display, TEXT("QA: SELL '%s' for %.0f, balance %.0f"),
        *SoldName, SellPrice, Stats->GetMoney());

    Item->Destroy();
}

void APlayerCharacter::GiveTestItems()
{
    UWorld* World = GetWorld();
    if (!World || !Inventory)
    {
        return;
    }

    FActorSpawnParameters Sp;
    Sp.Owner = this;
    Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Предметы рюкзака — не объекты на сцене: прячем визуал/коллизию, держим как данные.
    auto AddHidden = [&](AMasterInventoryItem* It)
    {
        if (!It)
        {
            return;
        }
        It->SetActorHiddenInGame(true);
        It->SetActorEnableCollision(false);
        Inventory->AddItem(It);
    };

    // Расходники: еда (+Hunger) и вода (+Thirst).
    if (AConsumableItem* Food = World->SpawnActor<AConsumableItem>(
            AConsumableItem::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Food->ConsumableType = EConsumableType::Food;
        Food->ItemName = TEXT("Canned Food");
        AddHidden(Food);
    }
    if (AConsumableItem* Water = World->SpawnActor<AConsumableItem>(
            AConsumableItem::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Water->ConsumableType = EConsumableType::Water;
        Water->ItemName = TEXT("Water Bottle");
        AddHidden(Water);
    }

    // Запасная броня (Head_02 / Torso_02) — лежит в рюкзаке неэкипированной,
    // чтобы было что надеть через paper-doll.
    if (AHeadArmor* Head = World->SpawnActor<AHeadArmor>(
            AHeadArmor::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Head->ItemName = TEXT("Spare Head Armor (Head_02)");
        AddHidden(Head);
    }
    if (ATorsoArmor* Torso = World->SpawnActor<ATorsoArmor>(
            ATorsoArmor::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Torso->ItemName = TEXT("Spare Torso Armor (Torso_02)");
        AddHidden(Torso);
    }

    UE_LOG(LogTemp, Log, TEXT("GiveTestItems: added 2 consumables + 2 spare armor pieces. Backpack size now %d"),
        Inventory->GetInventoryItems().Num());
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

    // ЗАДЕЛ (Фаза 4): сериализуем экипированную броню по слотам как пути классов.
    auto ArmorPath = [](AArmor* Armor) -> FString
    {
        return Armor ? Armor->GetClass()->GetPathName() : FString();
    };
    Save->EquippedHeadArmorClassPath  = ArmorPath(GetEquippedArmor(EArmorSlot::Head));
    Save->EquippedTorsoArmorClassPath = ArmorPath(GetEquippedArmor(EArmorSlot::Torso));
    Save->EquippedLegsArmorClassPath  = ArmorPath(GetEquippedArmor(EArmorSlot::Legs));

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
    // GDD §7.8 (Фаза 4, тех-долг Фазы 2 закрыт): при смерти теряется только доля
    // НЕэкипированных предметов категорий Consumable/Resource. Надетая броня (экип-слоты)
    // и оружие в руках (CurrentWeapon, хранится отдельно от InventoryItems) сохраняются.
    if (!Inventory || DeathItemLossPercent <= 0.0f)
    {
        return;
    }

    // Кандидаты на потерю: неэкипированные расходники + ресурсы.
    TArray<AMasterInventoryItem*> Candidates = Inventory->GetUnequippedItemsOfCategory(EItemCategory::Consumable);
    Candidates.Append(Inventory->GetUnequippedItemsOfCategory(EItemCategory::Resource));

    const int32 Total = Candidates.Num();
    if (Total <= 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Death penalty: no unequipped consumables/resources to lose."));
        return;
    }

    const int32 LoseCount = FMath::FloorToInt(Total * DeathItemLossPercent);
    for (int32 i = 0; i < LoseCount; ++i)
    {
        // Снимаем с конца списка кандидатов (без выпадения лут-мешка — MVP).
        AMasterInventoryItem* Item = Candidates[Total - 1 - i];
        Inventory->RemoveItem(Item);
    }

    UE_LOG(LogTemp, Log, TEXT("Death penalty: lost %d of %d unequipped consumable/resource items (%.0f%%). Equipped armor/weapons kept."),
        LoseCount, Total, DeathItemLossPercent * 100.0f);
}

void APlayerCharacter::HandleDeath()
{
    UE_LOG(LogTemp, Warning, TEXT("APlayerCharacter: death -> respawn"));

    // 0) Закрываем все открытые модальные окна (инвентарь/магазин/диалог), чтобы UI не
    //    «зависал» поверх экрана после респауна (BUG: окна оставались открытыми при смерти).
    if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetController()))
    {
        PC->CloseAllUI();
    }

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