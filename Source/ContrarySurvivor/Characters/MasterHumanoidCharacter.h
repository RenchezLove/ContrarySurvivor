// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UObject/SoftObjectPtr.h" // мягкие ссылки на боевые монтажи
#include "AMasterWeapon.h"
#include "AArmor.h" // AArmor + EArmorSlot (тип параметра UFUNCTION UnequipArmor)

class UInventoryComponent;
class UAnimMontage;

#include "MasterHumanoidCharacter.generated.h"

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AMasterHumanoidCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMasterHumanoidCharacter();

	UFUNCTION(BlueprintCallable)
    void SetSprint(bool bIsSprinting);

	// Ставит множитель скорости ходьбы (Build 1: хромота игрока при низком HP) и сразу применяет
	// к MaxWalkSpeed с учётом текущего спринта. 1 = обычная скорость. У врагов/NPC не трогается.
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetWalkSpeedMultiplier(float NewMultiplier);

	// Спринтит ли персонаж сейчас (буст MaxWalkSpeed активен). Читается UStatsComponent
	// для повышенного расхода голода/жажды при спринте (#2). Источник истины — флаг IsSprinting,
	// выставляемый SetSprint из контроллера по Enhanced Input (Shift).
	UFUNCTION(BlueprintPure, Category = "Movement")
	FORCEINLINE bool GetIsSprinting() const { return IsSprinting; }

protected:
	virtual void BeginPlay() override;

	// --- Статы ---

	// meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината); базовый класс
	// держит бОльшие номера (50+), чтобы категории конкретного класса-наследника шли выше унаследованных.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats", meta = (DisplayPriority = "50"))
    float Health;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float MaxHealth;

	// --- Состояние боя ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (DisplayPriority = "52"))
    bool bIsAttacking;

	// --- Прицеливание корпусом (вариант A, фидбек Рината 07-05 «не целятся») ---
	// При реальном выстреле/ударе носитель ПЛАВНО доворачивается (yaw) на цель и AimTurnHoldTime
	// секунд «ведёт» её; окно продлевается каждым выстрелом (StartAimTurnTo). Работает у игрока
	// и бандита (хуки: ARangedWeapon::Fire, AEnemyAIController::PerformRangedAttack/PerformAttack).
	// С Build 1.1 сюда же переведён нож игрока (AMeleeWeapon::Fire) — чтобы удар приходился ровно
	// по подсвеченному сектору; прежний мгновенный рывок остался под AMeleeWeapon::bMeleeSnapToTarget.
	// На окно доворота bOrientRotationToMovement выключается и потом восстанавливается —
	// иначе ориентация бега (BP игрока держит её включённой) борется с прицелом каждый кадр.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aim", meta = (DisplayPriority = "55"))
	bool bAimTurnToTarget = true;

	// Скорость доворота (FMath::RInterpTo, 1/сек): больше = быстрее лицом к цели. Плавно, не снап.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aim", meta = (ClampMin = "0.5", DisplayPriority = "56"))
	float AimTurnInterpSpeed = 10.0f;

	// Сколько секунд после выстрела корпус продолжает вести цель (окно продлевается выстрелами).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aim", meta = (ClampMin = "0.0", DisplayPriority = "57"))
	float AimTurnHoldTime = 1.0f;

	// --- Боевые анимации (Build 1.1) ---
	// Мягкие ссылки: ассет грузится при первом проигрывании. Ассета нет или поле очищено —
	// анимации не будет, бой работает как раньше. Общие для игрока и бандита.

	// Анимация отдачи при выстреле.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation", meta = (DisplayName = "Анимация выстрела", DisplayPriority = "58"))
	TSoftObjectPtr<UAnimMontage> FireMontage =
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Characters/Shared/Humanoid/AM_FirePistol.AM_FirePistol")));

	// Анимация размашистого удара холодным оружием. У игрока на её дорожке стоит метка
	// UAnimNotify_MeleeHit — именно по ней наносится урон ножа.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animation", meta = (DisplayName = "Анимация удара", DisplayPriority = "59"))
	TSoftObjectPtr<UAnimMontage> MeleeMontage =
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Characters/Shared/Humanoid/AM_MeleeSlash.AM_MeleeSlash")));

	// --- Меши ---
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true", DisplayPriority = "54"))
	USkeletalMeshComponent* HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* TorsoMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* LegsMesh; 

	// --- Компоненты ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* Inventory;

	// --- Оружие ---

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (DisplayPriority = "53"))
	AMasterWeapon* CurrentWeapon;

	// УСТАРЕЛО: имя сокета (на TorsoMesh такого сокета нет — оружие падало в origin).
	// Оставлено для совместимости с BP-дефолтами; привязка теперь к КОСТИ (см. ниже).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName WeaponSocketName;

	// Имя КОСТИ правой кисти, к которой крепится оружие (привязка к кости, не к сокету).
	// На модульном гуманоиде это R_Hand. AActor::AttachToComponent принимает имя кости
	// напрямую как SocketName. Меш-носитель кости выбирается ПО ФАКТУ в EquipWeapon
	// (DoesSocketExist на скелетном меше возвращает true и для костей) с приоритетом
	// анимируемого Leader-меша (GetMesh()).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName WeaponAttachBoneName;

	// Тонкая подстройка положения/поворота оружия в кисти ОТНОСИТЕЛЬНО кости R_Hand
	// (подбирается по скрину; дефолт 0). Применяется через SetRelativeLocationAndRotation
	// после attach к кости.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FVector WeaponGripLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FRotator WeaponGripRotation;

	// --- Экипированная броня по слотам (GDD §7.2: броня влияет на урон) ---
	// Хранятся ссылки на экипированные предметы брони; суммарная защита снижает
	// входящий урон в TakeDamage. Полноценная экип-UI — Фаза 4.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Armor")
	AArmor* EquippedHeadArmor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Armor")
	AArmor* EquippedTorsoArmor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Armor")
	AArmor* EquippedPantsArmor;

	// Потолок суммарного процентного снижения урона бронёй [0..1] (решение Рината:
	// процентная броня). Final = Incoming * (1 - clamp(SumArmorFraction, 0, Cap)).
	// DRAFT = 0.75 (макс −75% урона, всегда остаётся минимум 25% — нет min-1 неуязвимости).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Armor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArmorReductionCap;

public:
	virtual void Tick(float DeltaTime) override;

	// --- Функции оружия ---

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipWeapon(AMasterWeapon* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipWeapon();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void FireCurrentWeapon(AActor* Target);

	// --- Боевые анимации (Build 1.1, требование Рината: «человеческие персонажи, в том числе
	// игрок и бандиты»). Ссылки живут на ПЕРСОНАЖЕ, поэтому одинаково работают у игрока и у
	// бандита — оба наследуют этот класс. Поле пустое или ассета нет — анимации просто не будет,
	// бой продолжает работать как раньше. ---

	// Проигрывает анимацию выстрела (отдача). true — анимация реально пошла.
	// Зовётся из ARangedWeapon::Fire (игрок) и AEnemyAIController::PerformRangedAttack (бандит).
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	bool PlayFireMontage();

	// Проигрывает анимацию удара холодным оружием. true — анимация реально пошла.
	// У игрока по метке на её дорожке наносится урон (UAnimNotify_MeleeHit); у бандита урон
	// считает его ИИ отдельно, поэтому там анимация чисто зрелищная.
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	bool PlayMeleeMontage();

	// Целится ли персонаж прямо сейчас: в руках ДАЛЬНОБОЙНОЕ оружие и есть цель.
	// По этому признаку анимация накладывает позу прицеливания (UHumanoidAnimInstance).
	// Два источника цели, потому что игрок и бандит стреляют разными путями: у игрока цель
	// проставлена самому оружию контроллером (ARangedWeapon::HasTarget), у бандита оружию
	// цель не ставят вовсе — там признаком служит окно доворота корпуса, которое открывается
	// на каждом реальном выстреле (StartAimTurnTo) и держится AimTurnHoldTime секунд.
	UFUNCTION(BlueprintPure, Category = "Combat|Animation")
	bool IsAimingAtTarget() const;

	// Запустить/продлить плавный доворот корпуса на цель (вариант A прицеливания). Зовётся из
	// точек РЕАЛЬНОГО выстрела/удара (после кулдаунов/патронов): ARangedWeapon::Fire (игрок),
	// AMeleeWeapon::Fire (нож игрока, с Build 1.1), AEnemyAIController::PerformRangedAttack/
	// PerformAttack (бандит). No-op при bAimTurnToTarget=false.
	void StartAimTurnTo(AActor* Target);

	// virtual: APlayerCharacter переопределяет, чтобы перед штатной перезарядкой пополнить
	// резерв оружия из пачки патронов (AAmmoItem) в рюкзаке (Фаза 5, STALKER 2-стиль).
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void ReloadCurrentWeapon();

	// --- Броня ---

	// Экипирует предмет брони в слот по его GetArmorSlot() (Head/Torso/Legs). Хранит ссылку
	// для расчёта защиты (GetTotalArmorProtection) И подменяет модульный скелетный меш
	// соответствующего слота на Armor->GetMesh() (ArmorMesh_Equipped) (GDD §7.4). Если в
	// слоте уже была броня — она снимается (меш слота возвращается к базовому перед сменой).
	UFUNCTION(BlueprintCallable, Category = "Equipment|Armor")
	void EquipArmor(AArmor* Armor);

	// Снимает броню из слота: очищает ссылку (защита пересчитывается) и возвращает
	// модульный меш слота к базовому (запомненному в BeginPlay). Меш слота снова
	// анимируется синхронно с телом через Leader Pose.
	UFUNCTION(BlueprintCallable, Category = "Equipment|Armor")
	void UnequipArmor(EArmorSlot Slot);

	// Суммарная ДОЛЯ снижения урона по всем экипированным слотам брони [0..N] (без капа;
	// кап применяется в ComputeArmoredDamage). Читается при расчёте урона.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	float GetTotalArmorProtection() const;

	// Применяет процентную броню к входящему урону (решение Рината):
	// Final = Incoming * (1 - clamp(GetTotalArmorProtection(), 0, ArmorReductionCap)).
	// Используется в TakeDamage игрока и врага. Процент всегда оставляет часть урона —
	// убирает min-1 неуязвимость старой flat-формулы.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	float ComputeArmoredDamage(float Incoming) const;

	// ФАКТИЧЕСКАЯ доля снижения урона с учётом потолка ArmorReductionCap:
	// clamp(GetTotalArmorProtection(), 0, Cap). Для UI «Защита: N%» (ADR-043) — показываем
	// реальное снижение, а не сырую сумму слотов, которая может превышать кап.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	float GetEffectiveArmorFraction() const;

	// Возвращает экипированную броню в слоте (или nullptr). Для сохранения/UI.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	AArmor* GetEquippedArmor(EArmorSlot Slot) const;

	// --- Геттеры ---

	UFUNCTION(BlueprintCallable, Category = "Components")
	FORCEINLINE USkeletalMeshComponent* GetTorsoMesh() const { return TorsoMesh; }

	// Доступ к рюкзаку (Inventory protected в базе). Нужен UI-инвентарю (HUD/контроллер)
	// и логике использования/выброса предметов.
	UFUNCTION(BlueprintPure, Category = "Components")
	FORCEINLINE UInventoryComponent* GetInventory() const { return Inventory; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FORCEINLINE AMasterWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	FORCEINLINE float GetHealth() const { return Health; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	// --- Внешний вид ---

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void UpdateCharacterAppearance();

	// --- Урон и лечение ---

	UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual void RestoreHealth(float HealAmount);

	// Вызывается при достижении 0 HP (инлайн-Health база). Минимальная заглушка;
	// дочерние классы могут переопределить (враг использует UStatsComponent вместо этого).
	UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual void HandleDeath();

protected:
	// Модульный меш-компонент слота (Head=GetMesh()/Torso/Legs). nullptr для неизвестного.
	USkeletalMeshComponent* GetMeshComponentForSlot(EArmorSlot Slot) const;

	// Перепривязывает меш-компонент слота к Leader Pose (Head) после подмены меша, чтобы
	// часть продолжала анимироваться синхронно с телом (GDD §7.4). Для самого Head — no-op
	// (он и есть лидер). Использует тот же механизм, что и риг модульных частей базы.
	void RelinkSlotToLeaderPose(EArmorSlot Slot);

	// Запоминает базовые (надетые в BP) скелетные меши слотов — нужно для возврата при
	// снятии брони. Вызывается в BeginPlay (после применения дефолтов BP).
	void CacheBaseSlotMeshes();

protected:
    // Базовая скорость ходьбы (см/с). Если CharacterMovement в BP несёт валидную (>0) MaxWalkSpeed —
    // она уважается как BaseWalkSpeed (см. конструктор/init движения). Тюнинг из BP.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", DisplayPriority = "51"))
    float BaseWalkSpeed = 600.0f;   // дефолт, чтобы SetSprint никогда не выставил MaxWalkSpeed=0 (фикс «поворачивается, но не идёт»)

    // Множитель скорости при спринте (Shift): MaxWalkSpeed = BaseWalkSpeed * SprintMultiplier. Тюнинг из BP.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1.0"))
    float SprintMultiplier = 2.0f;

    // Общий множитель скорости ходьбы поверх базовой/спринтовой (Build 1: хромота игрока при низком
    // HP). 1 = обычная скорость. Меняется через SetWalkSpeedMultiplier; у врагов/NPC остаётся 1.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    float WalkSpeedMultiplier = 1.0f;

private:
    bool IsSprinting = false;

    // --- Рантайм доворота корпуса на цель (вариант A прицеливания) ---

    // Текущая цель доворота (слабый указатель: гибель цели просто завершает доворот).
    TWeakObjectPtr<AActor> AimTurnTarget;

    // Время конца окна доворота (мировые секунды); продлевается каждым StartAimTurnTo.
    float AimTurnEndTime = 0.0f;

    bool bAimTurnActive = false;

    // Сохранённый bOrientRotationToMovement на окно доворота (восстанавливается в EndAimTurn).
    bool bAimTurnSavedOrientToMovement = false;

    // Кадровый шаг доворота (из Tick): плавный yaw на цель, завершение по таймеру/гибели/трупу.
    void UpdateAimTurn(float DeltaTime);

    // Завершить доворот и восстановить ориентацию бега.
    void EndAimTurn();

    // Базовые меши слотов (тело без брони) — снимок BeginPlay для UnequipArmor.
    UPROPERTY()
    USkeletalMesh* BaseHeadMesh = nullptr;

    UPROPERTY()
    USkeletalMesh* BaseTorsoMesh = nullptr;

    UPROPERTY()
    USkeletalMesh* BaseLegsMesh = nullptr;

    bool bBaseSlotMeshesCached = false;
};
