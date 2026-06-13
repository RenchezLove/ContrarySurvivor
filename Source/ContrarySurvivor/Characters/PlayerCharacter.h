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
class UQuestComponent;
class UContrarySaveGame;
class AMasterInventoryItem;
struct FShopEntry;

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

    // Журнал квестов игрока (Фаза 5, GDD §7.7). C++-сабобъект — добавляется детерминированно,
    // без BP. Староста предлагает квест, убийства целей инкрементят прогресс, сдача даёт деньги.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest", meta = (AllowPrivateAccess = "true"))
    UQuestComponent* Quests;

    // Стартовое здоровье игрока. Тюнингуемое черновое значение.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float PlayerMaxHealth = 100.0f;

    // Стартовые деньги НОВОГО персонажа (GDD §7.6 = 50). Применяются в BeginPlay (новый игрок)
    // через Stats->InitMoney — детерминированно, независимо от дефолта компонента/оверрайда в BP.
    // НЕ перетирают загруженный сейв: загрузка идёт только при смерти/у костра (LoadGame ->
    // RestoreState), а BeginPlay сейв не загружает.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float StartingMoney = 50.0f;

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

    // ЧЕРНОВИК (на тюнинг): доля теряемых при смерти НЕэкипированных предметов
    // категорий Consumable/Resource (GDD §7.8). Надетая броня и оружие в руках —
    // сохраняются (Фаза 4: UInventoryComponent различает экип/неэкип и категории).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathItemLossPercent = 0.25f;

    // Куда падает выброшенный из рюкзака предмет (мировой пикап): вперёд от игрока и вниз
    // к ногам (см). DRAFT-тюнинг (BUG3: выброс = пикап, а не Destroy).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    float DropForwardOffset = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    float DropDownOffset = 80.0f;

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

    // Дефолтная броня по слотам (Фаза 3): экипируется в BeginPlay, чтобы снижение урона
    // (GDD §7.2) было наблюдаемо без экип-UI (UI — Фаза 4). По умолчанию конкретные классы
    // брони с черновыми значениями защиты. BP игрока может переопределить/обнулить.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Armor")
    TSubclassOf<class AHeadArmor> DefaultHeadArmorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Armor")
    TSubclassOf<class ATorsoArmor> DefaultTorsoArmorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Armor")
    TSubclassOf<class APantsArmor> DefaultPantsArmorClass;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Спавнит DefaultWeaponClass и экипирует через EquipWeapon (если класс задан).
    void EquipDefaultWeapon();

    // Спавнит DefaultMeleeWeaponClass (нож) и держит «в кобуре» (скрыт, не экипирован).
    void SpawnMeleeWeapon();

    // Спавнит и экипирует дефолтную броню по слотам (для наблюдаемости снижения урона).
    void EquipDefaultArmor();

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

    UFUNCTION(BlueprintPure, Category = "Quest")
    UQuestComponent* GetQuests() const { return Quests; }

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

    // --- Тестирование брони без UI (Фаза 4, UI — отдельная волна) ---
    // Консольные команды (открыть консоль `~`, ввести имя). Pawn должен быть под управлением.
    //   EquipTestArmor   — (пере)спавнит и надевает дефолтную броню всех слотов (подмена меша).
    //   UnequipTestArmor — снимает броню всех слотов (возврат базовых мешей тела).
    // Позволяют наблюдать смену модульного меша слота и пересчёт суммарной защиты.

    UFUNCTION(Exec, Category = "Equipment|Armor|Debug")
    void EquipTestArmor();

    UFUNCTION(Exec, Category = "Equipment|Armor|Debug")
    void UnequipTestArmor();

    // --- Действия UI-инвентаря (Фаза 4, GDD §7.4) ---
    // Высокоуровневые операции инвентаря (вызываются из AContrarySurvivorHUD по клику).
    // Знание о Stats/EquipArmor/Inventory сосредоточено здесь, у владельца предметов.

    // Использовать предмет рюкзака по клику: броня -> надеть (EquipArmor + пометить экип);
    // расходник -> применить эффект (еда/вода через Stats) и удалить из рюкзака.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Inv_UseBackpackItem(AMasterInventoryItem* Item);

    // Выбросить предмет рюкзака (удалить из инвентаря). Экипированную броню сперва снимает.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Inv_DropItem(AMasterInventoryItem* Item);

    // Снять броню из слота (клик по слоту paper-doll): UnequipArmor + вернуть предмет в рюкзак
    // (как неэкипированный), чтобы его было видно/можно надеть заново.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Inv_UnequipSlot(EArmorSlot Slot);

    // --- Магазин торговца (Фаза 4, экономика — GDD §7.6) ---
    // Вызываются из AContrarySurvivorHUD по клику в экране магазина.

    // Купить позицию каталога: проверяет деньги, выдаёт товар (предмет в рюкзак или
    // патроны в резерв оружия) и списывает цену. Возвращает true при успешной покупке.
    bool Shop_BuyEntry(const FShopEntry& Entry);

    // Продать предмет рюкзака за SellPrice: экип. броню сначала снимает, удаляет предмет,
    // начисляет деньги.
    void Shop_SellItem(AMasterInventoryItem* Item, float SellPrice);

    // DEBUG-команда наполнения рюкзака тестовыми предметами (консоль `~`, ввести GiveTestItems):
    // пара расходников (еда+вода) + запасная броня головы/торса — чтобы было что
    // надевать/использовать/выбрасывать в Play без редактора и без BP-ассетов.
    UFUNCTION(Exec, Category = "Inventory|Debug")
    void GiveTestItems();

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
