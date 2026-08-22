// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "AMasterInventoryItem.h"
#include "AArmor.generated.h"

// Слот брони на модульном персонаже (GDD §7.4: Head/Torso/Legs отдельными скелетными
// мешами на общем скелете). Классы AHeadArmor/ATorsoArmor/APantsArmor задают свой слот
// в конструкторе; экипировка подменяет меш соответствующего слота персонажа.
UENUM(BlueprintType)
enum class EArmorSlot : uint8
{
	Head  UMETA(DisplayName = "Head"),
	Torso UMETA(DisplayName = "Torso"),
	Legs  UMETA(DisplayName = "Legs")   // APantsArmor (штаны) -> слот Legs
};

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AArmor : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AArmor();

protected:
	virtual void BeginPlay() override;

public:
    
    // Меш, который надевается на слот персонажа при экипировке (SK_Armor_*). ADR-075 (волна
    // инструментов): EditAnywhere — BP-наследник/экземпляр задаёт свой меш БЕЗ правки кода
    // (раньше VisibleAnywhere: меш можно было задать только FObjectFinder'ом в конструкторе
    // C++ — главная дыра линейки брони по инвентаризации bp-audit-phase1). Дефолты тиров
    // из AArmorTiers.cpp продолжают работать: конструктор заполняет поле, BP переопределяет.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor", meta = (AllowPrivateAccess = "true", DisplayPriority = "2",
        DisplayName = "Меш экипировки (на слот персонажа)",
        ToolTip = "Скелетный меш, который заменит слот персонажа (голова/торс/ноги), когда броню наденут. Пусто = слот останется с базовым мешем тела."))
    USkeletalMesh* ArmorMesh_Equipped;

    // Geting mesh which equipted
    UFUNCTION(BlueprintPure, Category = "Armor")
    USkeletalMesh* GetMesh() const { return ArmorMesh_Equipped; }

    // Слот, в который надевается эта броня. Задаётся в конструкторе наследника
    // (Head/Torso/Pants). Используется AMasterHumanoidCharacter::EquipArmor для выбора
    // модульного меша-слота и UnequipArmor(slot).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor")
    EArmorSlot ArmorSlot;

    UFUNCTION(BlueprintPure, Category = "Armor")
    FORCEINLINE EArmorSlot GetArmorSlot() const { return ArmorSlot; }

    // Доля снижения урона этим предметом брони [0..1] (решение Рината: ПРОЦЕНТНАЯ броня
    // вместо flat). Напр. 0.25 = -25% урона от слота. Тюнингуется в редакторе без пересборки
    // (ADR-042/ADR-043: все числа защиты настраиваемые). Значения по тирам задаются в
    // конструкторах конкретных классов (Т1=0.05 / Т2=0.10 / Т3=0.16 на слот).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "1"))
    float ArmorProtection;

    // Иконка предмета для слотов UI-инвентаря (ADR-043) — унаследованный ItemIcon базы
    // AMasterInventoryItem (мягкая ссылка TSoftObjectPtr<UTexture2D>, текстур может ещё не
    // быть — HUD живёт на текстовом фолбэке). Дефолт-пути задаются в конструкторах тиров
    // (AArmorTiers.cpp); у старой брони _01 иконки нет (пустая ссылка).

    // Доля снижения урона этим слотом [0..1]. Суммируется по экипированным слотам и
    // используется при расчёте получаемого урона (GDD §7.2: «урон рассчитывается от
    // характеристик оружия и брони»). См. AMasterHumanoidCharacter::ComputeArmoredDamage.
    UFUNCTION(BlueprintPure, Category = "Armor")
    FORCEINLINE float GetArmorProtection() const { return ArmorProtection; }
};
