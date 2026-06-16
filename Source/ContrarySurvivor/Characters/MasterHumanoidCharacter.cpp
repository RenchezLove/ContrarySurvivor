// Fill out your copyright notice in the Description page of Project Settings.

#include "MasterHumanoidCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h" // USkeletalMesh (полный тип для GetName в QA-логах)
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

    // Привязка оружия к КОСТИ правой кисти модульного гуманоида (а не к несуществующему
    // сокету). Офсет грипа подбирается по скрину; дефолт — нулевой (на кости).
    WeaponAttachBoneName = FName("R_Hand");
    WeaponGripLocation = FVector::ZeroVector;
    WeaponGripRotation = FRotator::ZeroRotator;

    EquippedHeadArmor  = nullptr;
    EquippedTorsoArmor = nullptr;
    EquippedPantsArmor = nullptr;

    // DRAFT (решение Рината): потолок процентного снижения урона бронёй = 75%.
    ArmorReductionCap = 0.75f;

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

    // КОРЕНЬ БАГА «поворачивается, но не едет» (BugReport 12). Раньше здесь было безусловно
    // `BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;`. Если CMC/BP стартует с MaxWalkSpeed=0
    // (наследие спринт-рефактора / оверрайд в BP), BaseWalkSpeed становился 0, а SetSprint(false)
    // затем держал MaxWalkSpeed=0 → AddMovementInput копит Acceleration (поэтому персонаж
    // поворачивается в сторону ввода), но скорость капается в 0 → трансляции нет. Прошлый фикс
    // (дефолт члена BaseWalkSpeed=600) не помогал, т.к. эта строка перетирала его нулём.
    //
    // ФИКС: если CMC несёт валидную (>0) скорость — уважаем её как BaseWalkSpeed (тюнинг из BP);
    // иначе оставляем дефолт BaseWalkSpeed (600). В ЛЮБОМ случае жёстко применяем ненулевую
    // BaseWalkSpeed обратно в MaxWalkSpeed, гарантируя, что персонаж реально едет на старте.
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        const float ConfiguredSpeed = Move->MaxWalkSpeed;
        if (ConfiguredSpeed > 0.0f)
        {
            BaseWalkSpeed = ConfiguredSpeed;
        }
        Move->MaxWalkSpeed = BaseWalkSpeed;
        UE_LOG(LogTemp, Log, TEXT("%s: movement init MaxWalkSpeed=%.0f (BaseWalkSpeed=%.0f)"),
            *GetName(), Move->MaxWalkSpeed, BaseWalkSpeed);
    }

    // Снимок базовых мешей слотов (тело без брони). Делаем ДО любой авто-экипировки
    // (дефолтная броня экипируется позже в APlayerCharacter::BeginPlay), чтобы UnequipArmor
    // мог вернуть исходный меш слота.
    CacheBaseSlotMeshes();
}

void AMasterHumanoidCharacter::CacheBaseSlotMeshes()
{
    if (bBaseSlotMeshesCached)
    {
        return;
    }
    if (HeadMesh)  { BaseHeadMesh  = HeadMesh->GetSkeletalMeshAsset(); }
    if (TorsoMesh) { BaseTorsoMesh = TorsoMesh->GetSkeletalMeshAsset(); }
    if (LegsMesh)  { BaseLegsMesh  = LegsMesh->GetSkeletalMeshAsset(); }
    bBaseSlotMeshesCached = true;
}

USkeletalMeshComponent* AMasterHumanoidCharacter::GetMeshComponentForSlot(EArmorSlot Slot) const
{
    switch (Slot)
    {
        case EArmorSlot::Head:  return HeadMesh;
        case EArmorSlot::Torso: return TorsoMesh;
        case EArmorSlot::Legs:  return LegsMesh;
        default:                return nullptr;
    }
}

void AMasterHumanoidCharacter::RelinkSlotToLeaderPose(EArmorSlot Slot)
{
    // Head — корневой скелет (лидер позы), сам себе лидером не является.
    if (Slot == EArmorSlot::Head)
    {
        return;
    }

    USkeletalMeshComponent* Leader = GetMesh(); // == HeadMesh (см. конструктор базы)
    USkeletalMeshComponent* Follower = GetMeshComponentForSlot(Slot);
    if (Leader && Follower)
    {
        // Тот же механизм, что и для модульных частей базы (как в AEnemyCharacter):
        // после подмены меша заново привязываем часть к позе Head — синхронная анимация
        // модульных частей через Leader/Master Pose Component (GDD §7.4).
        Follower->SetLeaderPoseComponent(Leader);
    }
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

    // Крепим оружие к КОСТИ правой кисти (R_Hand), а не к сокету (сокета WeaponSocket на
    // TorsoMesh нет — оружие падало в origin). Носитель кости определяем ПО ФАКТУ:
    // у скелетного меша DoesSocketExist(BoneName) истинно и для костей.
    //
    // ПРИОРИТЕТ — ЛИДЕР-меш GetMesh() (== HeadMesh, см. конструктор/RelinkSlotToLeaderPose):
    // это ГЛАВНЫЙ анимируемый скелет, follower-меши (Torso/Legs) идут за ним через
    // SetLeaderPoseComponent. Привязка к лидеру корректнее и устойчивее к смене модулей,
    // чем к follower-под-мешу. Если у лидера в текущем ассете нет R_Hand — пробуем followers
    // как запас (иная раскладка скелета), и только затем root-фолбэк.
    USkeletalMeshComponent* BoneCarrier = nullptr;
    bool bCarrierIsLeader = false;

    USkeletalMeshComponent* Leader = GetMesh();
    if (Leader && Leader->DoesSocketExist(WeaponAttachBoneName))
    {
        BoneCarrier = Leader;
        bCarrierIsLeader = true;
    }
    else
    {
        // Лидер не несёт R_Hand в текущем ассете — оставляем привязку как есть (follower),
        // но громко сообщаем: это нештатно (см. задачу — целевой носитель = GetMesh()).
        UE_LOG(LogTemp, Warning,
            TEXT("EquipWeapon: leader mesh GetMesh() has NO bone '%s' — falling back to follower meshes."),
            *WeaponAttachBoneName.ToString());
        USkeletalMeshComponent* Followers[] = { TorsoMesh, LegsMesh };
        for (USkeletalMeshComponent* MeshComp : Followers)
        {
            if (MeshComp && MeshComp->DoesSocketExist(WeaponAttachBoneName))
            {
                BoneCarrier = MeshComp;
                break;
            }
        }
    }

    if (BoneCarrier)
    {
        // Снап к кости БЕЗ наследования масштаба кости/меша: с IncludingScale оружие
        // наследовало крупный масштаб кости и раздувалось до scale~100 (гигантский
        // пистолет в воздухе). NotIncludingScale сохраняет собственный масштаб оружия (1).
        // Затем относительный офсет грипа (подбор по скрину).
        CurrentWeapon->AttachToComponent(BoneCarrier,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            WeaponAttachBoneName);
        CurrentWeapon->SetActorRelativeLocation(WeaponGripLocation);
        CurrentWeapon->SetActorRelativeRotation(WeaponGripRotation);

        const USkeletalMesh* CarrierAsset = BoneCarrier->GetSkeletalMeshAsset();
        UE_LOG(LogTemp, Log,
            TEXT("EquipWeapon: attached %s to bone '%s' on %s mesh '%s' (component '%s')"),
            *CurrentWeapon->GetName(), *WeaponAttachBoneName.ToString(),
            bCarrierIsLeader ? TEXT("LEADER") : TEXT("follower"),
            CarrierAsset ? *CarrierAsset->GetName() : TEXT("none"),
            *BoneCarrier->GetName());
    }
    else
    {
        // Кость не найдена ни на одном меше — крепим к капсуле (корню), чтобы оружие не
        // улетало в origin мира, и громко логируем (нужно поправить имя кости/скелет).
        CurrentWeapon->AttachToComponent(GetRootComponent(),
            FAttachmentTransformRules::KeepRelativeTransform);
        UE_LOG(LogTemp, Warning,
            TEXT("EquipWeapon: bone '%s' NOT found on Head/Torso/Legs meshes — attached to root as fallback. Check bone name / skeleton."),
            *WeaponAttachBoneName.ToString());
    }

    // Устанавливаем владельца оружия
    CurrentWeapon->SetInstigator(this);

    // Надетое оружие — визуальное: гасим мировую коллизию. Иначе актёр оружия (root=ItemMesh
    // с дефолтной БЛОКИРУЮЩЕЙ коллизией), привязанный к кости R_Hand, едет вместе с
    // персонажем и упирается в капсулу/пол → персонаж крутится, но не движется
    // (vel=0, floorDist отрицательный). Парно коллизия возвращается в UnequipWeapon.
    CurrentWeapon->SetActorEnableCollision(false);

    UE_LOG(LogTemp, Warning, TEXT("EquipWeapon: Equipped %s"), *CurrentWeapon->GetName());
}

void AMasterHumanoidCharacter::UnequipWeapon()
{
    if (!CurrentWeapon) return;

    // Возвращаем мировую коллизию (гасилась в EquipWeapon): отстёгнутое/выброшенное
    // оружие снова должно сталкиваться с миром и подбираться.
    CurrentWeapon->SetActorEnableCollision(true);

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

    const EArmorSlot Slot = Armor->GetArmorSlot();

    // 1) Сохраняем ссылку в слот (для расчёта суммарной защиты).
    switch (Slot)
    {
        case EArmorSlot::Head:  EquippedHeadArmor  = Armor; break;
        case EArmorSlot::Torso: EquippedTorsoArmor = Armor; break;
        case EArmorSlot::Legs:  EquippedPantsArmor = Armor; break;
        default:
            UE_LOG(LogTemp, Warning, TEXT("EquipArmor: unknown slot for %s"), *Armor->GetName());
            return;
    }

    // 2) Подменяем модульный меш слота на меш брони (GDD §7.4). Если у брони меш не задан
    //    (ArmorMesh_Equipped == nullptr) — слот не трогаем (видим базовое тело), но защита
    //    учитывается. Так механика работает и до прихода реальных ассетов брони.
    if (USkeletalMeshComponent* SlotComp = GetMeshComponentForSlot(Slot))
    {
        if (USkeletalMesh* ArmorMesh = Armor->GetMesh())
        {
            SlotComp->SetSkeletalMeshAsset(ArmorMesh);
            // 3) Переустанавливаем Leader Pose, чтобы новый меш анимировался синхронно.
            RelinkSlotToLeaderPose(Slot);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("EquipArmor: %s has no ArmorMesh_Equipped; slot mesh unchanged (protection still applied)."), *Armor->GetName());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("EquipArmor: %s in slot %d (protection %.2f). Total armor now %.2f"),
        *Armor->GetName(), (int32)Slot, Armor->GetArmorProtection(), GetTotalArmorProtection());

    // QA-харнесс: слот + назначенный меш (или none, если ассета брони ещё нет).
    const USkeletalMeshComponent* SlotCompLog = GetMeshComponentForSlot(Slot);
    const USkeletalMesh* SlotMeshLog = SlotCompLog ? SlotCompLog->GetSkeletalMeshAsset() : nullptr;
    UE_LOG(LogQA, Display, TEXT("QA: EQUIP armor '%s' slot %d mesh '%s' (prot %.2f, total %.2f)"),
        *Armor->GetName(), (int32)Slot,
        SlotMeshLog ? *SlotMeshLog->GetName() : TEXT("none"),
        Armor->GetArmorProtection(), GetTotalArmorProtection());
}

void AMasterHumanoidCharacter::UnequipArmor(EArmorSlot Slot)
{
    // 1) Очищаем ссылку слота (защита пересчитается в GetTotalArmorProtection).
    switch (Slot)
    {
        case EArmorSlot::Head:  EquippedHeadArmor  = nullptr; break;
        case EArmorSlot::Torso: EquippedTorsoArmor = nullptr; break;
        case EArmorSlot::Legs:  EquippedPantsArmor = nullptr; break;
        default: return;
    }

    // 2) Возвращаем базовый меш слота (снимок BeginPlay).
    if (USkeletalMeshComponent* SlotComp = GetMeshComponentForSlot(Slot))
    {
        USkeletalMesh* BaseMesh = nullptr;
        switch (Slot)
        {
            case EArmorSlot::Head:  BaseMesh = BaseHeadMesh;  break;
            case EArmorSlot::Torso: BaseMesh = BaseTorsoMesh; break;
            case EArmorSlot::Legs:  BaseMesh = BaseLegsMesh;  break;
            default: break;
        }
        SlotComp->SetSkeletalMeshAsset(BaseMesh);
        RelinkSlotToLeaderPose(Slot);
    }

    UE_LOG(LogTemp, Log, TEXT("UnequipArmor: slot %d cleared. Total armor now %.2f"),
        (int32)Slot, GetTotalArmorProtection());

    // QA-харнесс: слот + меш, к которому вернулись (базовый меш тела или none).
    const USkeletalMeshComponent* SlotCompLog = GetMeshComponentForSlot(Slot);
    const USkeletalMesh* SlotMeshLog = SlotCompLog ? SlotCompLog->GetSkeletalMeshAsset() : nullptr;
    UE_LOG(LogQA, Display, TEXT("QA: UNEQUIP armor slot %d -> base mesh '%s' (total %.2f)"),
        (int32)Slot, SlotMeshLog ? *SlotMeshLog->GetName() : TEXT("none"), GetTotalArmorProtection());
}

AArmor* AMasterHumanoidCharacter::GetEquippedArmor(EArmorSlot Slot) const
{
    switch (Slot)
    {
        case EArmorSlot::Head:  return EquippedHeadArmor;
        case EArmorSlot::Torso: return EquippedTorsoArmor;
        case EArmorSlot::Legs:  return EquippedPantsArmor;
        default:                return nullptr;
    }
}

float AMasterHumanoidCharacter::GetTotalArmorProtection() const
{
    float Total = 0.0f;
    if (EquippedHeadArmor)  { Total += EquippedHeadArmor->GetArmorProtection(); }
    if (EquippedTorsoArmor) { Total += EquippedTorsoArmor->GetArmorProtection(); }
    if (EquippedPantsArmor) { Total += EquippedPantsArmor->GetArmorProtection(); }
    return Total;
}

float AMasterHumanoidCharacter::ComputeArmoredDamage(float Incoming) const
{
    // Процентная броня (решение Рината): Final = Incoming * (1 - clamp(Sum, 0, Cap)).
    const float Fraction = FMath::Clamp(GetTotalArmorProtection(), 0.0f, ArmorReductionCap);
    return Incoming * (1.0f - Fraction);
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
    // SetSprint зовётся каждый кадр на бегу. Логируем ТОЛЬКО при реальной смене состояния,
    // иначе "Sprint state changed to: true" спамит каждый кадр.
    const bool bChanged = (bIsSprinting != IsSprinting);

    IsSprinting = bIsSprinting;

    GetCharacterMovement()->MaxWalkSpeed =
        IsSprinting ? BaseWalkSpeed * SprintMultiplier : BaseWalkSpeed;

    if (bChanged)
    {
        UE_LOG(LogTemp, Warning, TEXT("Sprint state changed to: %s"), IsSprinting ? TEXT("true") : TEXT("false"));
    }
}
