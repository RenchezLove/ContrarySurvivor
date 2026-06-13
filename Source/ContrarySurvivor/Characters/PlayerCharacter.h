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
class UContrarySaveGame;

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

    // Доля MaxHealth, до которой восстанавливается HP при респауне (решение game-lead:
    // респаун = полный HP). 1.0 -> Health = MaxHealth. Остальные статы — из сейва.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RespawnHealthFraction = 1.0f;

    // Доля SurvivalMax, до которой восстанавливаются Голод/Жажда при death-респауне
    // (решение game-lead: респаун = полные статы). 1.0 -> полные. Применяется ТОЛЬКО в
    // ветке HandleDeath, не ломает обычный save/load (quit->reload восстанавливает точные значения).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RespawnSurvivalFraction = 1.0f;

    // --- Сейв/респаун (GDD §7.8) ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save")
    FString SaveSlotName = TEXT("ContrarySave");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save")
    int32 SaveUserIndex = 0;

    // ЧЕРНОВИК (на тюнинг): доля НЕэкипированных предметов рюкзака, теряемых при смерти.
    // ВНИМАНИЕ: UInventoryComponent сейчас НЕ различает экип/неэкип и категории
    // (расходник/ресурс/броня) — теряется доля ВСЕХ предметов массива. См. эскалацию.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathItemLossPercent = 0.25f;

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

    // Класс стартового ближнего оружия (нож). По умолчанию AMeleeWeapon (конкретный класс),
    // чтобы нож был доступен без создания нового .uasset в редакторе (Фаза 3).
    // Спавнится в BeginPlay и держится «в кобуре»; переключение — SwitchWeapon().
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TSubclassOf<AMasterWeapon> DefaultMeleeWeaponClass;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Спавнит DefaultWeaponClass и экипирует через EquipWeapon (если класс задан).
    void EquipDefaultWeapon();

    // Спавнит DefaultMeleeWeaponClass (нож) и держит «в кобуре» (скрыт, не экипирован).
    void SpawnMeleeWeapon();

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

    // Переключение между дальним (пистолет) и ближним (нож) оружием.
    // Вызывается из контроллера по legacy-инпуту (DefaultInput.ini), без нового .uasset.
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void SwitchWeapon();

    // --- Сейв/респаун API (GDD §7.8) ---

    // Сохраняет текущее состояние (статы + позиция) в слот. Вызывается костром (автосейв)
    // и доступно из UI/кнопки (ручной сейв, задел). Возвращает true при успехе.
    UFUNCTION(BlueprintCallable, Category = "Save")
    bool SaveGame();

    // Загружает сейв в текущего игрока (статы + позиция). Возвращает true, если сейв был.
    UFUNCTION(BlueprintCallable, Category = "Save")
    bool LoadGame();

    // Есть ли сохранение в слоте.
    UFUNCTION(BlueprintPure, Category = "Save")
    bool HasSaveGame() const;

protected:
    // Смерть игрока (привязана к Stats->OnDeath): респаун на последней точке сейва
    // (костёр) + потеря доли расходников рюкзака. Экипированное оружие сохраняется.
    virtual void HandleDeath() override;

    // Применяет потерю предметов рюкзака при смерти (DeathItemLossPercent).
    void ApplyDeathInventoryPenalty();

    // Применяет загруженный сейв к игроку (статы + телепорт в точку респауна).
    void ApplySaveData(const UContrarySaveGame* Save);

private:
    // Стартовый трансформ (фолбэк-точка респауна, если сейва ещё нет).
    FTransform InitialSpawnTransform;

    // Инстансы оружия (оба заспавнены в BeginPlay). CurrentWeapon базы указывает на активный.
    UPROPERTY()
    AMasterWeapon* RangedWeaponInstance = nullptr;

    UPROPERTY()
    AMasterWeapon* MeleeWeaponInstance = nullptr;

    // Кэш инвентаря (UInventoryComponent на базе AMasterHumanoidCharacter, защищён).
    // Доступ к нему — через каст в .cpp (Inventory protected в базе).
};
