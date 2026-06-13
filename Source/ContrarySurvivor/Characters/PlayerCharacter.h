// Fill out your copyright notice in the Description page of Project Settings.

// PlayerCharacter.h
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h" // InheritingFrom от AMasterHumanoidCharacter
#include "GameFramework/SpringArmComponent.h" 
#include "Camera/CameraComponent.h" 
#include "GameFramework/Controller.h" // Enhanced Input
//#include "PlayerController.h"
#include "AMasterWeapon.h"
#include "PlayerCharacter.generated.h"

class UStatsComponent;

/**
 * 
 */
UCLASS()
class CONTRARYSURVIVOR_API APlayerCharacter : public AMasterHumanoidCharacter
{
    GENERATED_BODY()

public:
    APlayerCharacter();


protected:

    // Spring Arm 
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArmComponent;

    // Camera Component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* CameraComponent;

    // Компонент статов игрока (ADR-015) — ИСТОЧНИК ИСТИНЫ по HP/голоду/жажде/деньгам
    // (Фаза 2). Инлайн-Health базы AMasterHumanoidCharacter для игрока не используется,
    // как и у врага: TakeDamage роутится в Stats.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
    UStatsComponent* Stats;

    // Стартовое здоровье игрока. Тюнингуемое черновое значение.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float PlayerMaxHealth = 100.0f;

    // УСТАРЕЛО (Фаза 1): инлайн-поля голода/жажды. Источник истины теперь Stats.
    // Оставлены, чтобы не ломать возможные ссылки BP; не используются логикой.
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats|Deprecated")
    float Hunger;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats|Deprecated")
    float Thirst;

    // Класс стартового оружия. Если задан — спавнится и экипируется в BeginPlay.
    // Значение (например, BP_Pistol) выставляется в дефолтах BP игрока.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TSubclassOf<AMasterWeapon> DefaultWeaponClass;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Спавнит DefaultWeaponClass и экипирует через EquipWeapon (если класс задан).
    void EquipDefaultWeapon();

    /**
     * Sets movement parameters
     */
    void SetUpMovement();

public:
    // Перехват стандартного пайплайна урона UE и роутинг в UStatsComponent
    // (как у AEnemyCharacter), чтобы система оружия работала без правок.
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintPure, Category = "Stats")
    UStatsComponent* GetStats() const { return Stats; }

};
