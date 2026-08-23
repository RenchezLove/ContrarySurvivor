// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterWeapon.h"
#include "Engine/TimerHandle.h"
#include "ARangedWeapon.generated.h"

class USoundBase;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API ARangedWeapon : public AMasterWeapon
{
	GENERATED_BODY()

public:
	ARangedWeapon();

protected:
	virtual void BeginPlay() override;

	// Текущая заблокированная цель (устанавливается тапом/кликом по врагу)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	AActor* LockedTarget;

	// Имя сокета на меше торса, из которого вылетает пуля
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
	FName MuzzleSocketName;

	// Разброс (0 = идеальная точность, 1 = максимальный разброс)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Spread;

	// --- Звук выстрела (Демо) ---
	// Проигрывается в момент реального выстрела (не на пустой обойме). Дефолт грузится
	// из /Game/Audio/Demo/pistol_22_gunshot через FObjectFinder в конструкторе; можно
	// переопределить в редакторе/BP. Выстрел бандита переиспользует ЭТОТ же звук
	// (PlayFireVisuals с bPlaySound=true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	USoundBase* FireSound;

	// Громкость выстрела. Тюнингуется (дефолт 1.0 — фидбек Рината 07-17 «слишком тихо»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio", meta = (ClampMin = "0.0"))
	float FireSoundVolume;

	// --- Вспышка выстрела + след пули (D2, Этап D) ---
	// Дёшево для слабого Android: два ПЕРЕИСПОЛЬЗУЕМЫХ StaticMesh-компонента на оружии
	// (базовые меши движка /Engine/BasicShapes), показываются на доли секунды по таймеру.
	// Без партиклов и без аллокаций на каждый выстрел. Директива Рината 06-25: параметры
	// EditAnywhere+BlueprintReadWrite, наверх Details.

	// Включатель эффектов выстрела (вспышка + след).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (DisplayPriority = "1"))
	bool bEnableFireVisuals = true;

	// Точка дула ОТНОСИТЕЛЬНО меша оружия (см) — используется, если на SM_Pistol НЕТ сокета
	// MuzzleSocketName (наличие проверяется в рантайме). DRAFT — подобрать по виду в PIE.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (DisplayPriority = "2"))
	FVector MuzzleOffset = FVector(30.0f, 0.0f, 10.0f);

	// Длительность показа вспышки (сек реального времени показа кадра).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (ClampMin = "0.01", DisplayPriority = "3"))
	float MuzzleFlashDuration = 0.06f;

	// Размер вспышки (масштаб сферы 100 см: 0.15 ≈ 15 см).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (ClampMin = "0.01", DisplayPriority = "4"))
	float MuzzleFlashSize = 0.15f;

	// Длительность показа следа пули (сек).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (ClampMin = "0.01", DisplayPriority = "5"))
	float TracerDuration = 0.08f;

	// Толщина следа пули (см, диаметр).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (ClampMin = "0.1", DisplayPriority = "6"))
	float TracerThickness = 3.0f;

	// Цвет вспышки/следа (подаётся в параметр "Color" материала FXMaterial).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (DisplayPriority = "7"))
	FLinearColor FireFXColor = FLinearColor(1.0f, 0.85f, 0.35f, 1.0f);

	// Материал эффектов. Дефолт — BasicShapeMaterial движка (простой, с параметром Color);
	// оператор может заменить на светящийся (эмиссив) материал проекта без правок кода.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (DisplayPriority = "8"))
	UMaterialInterface* FXMaterial = nullptr;

	// Сила тряски камеры при выстреле ИГРОКА (trauma 0..1; «лёгкая» по D5). 0 = выкл.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|FX", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "9"))
	float FireShakeTrauma = 0.12f;

	// Меш вспышки у дула (сфера; скрыт, показывается на MuzzleFlashDuration при выстреле).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|FX")
	UStaticMeshComponent* MuzzleFlashMesh;

	// Меш следа пули (тонкий цилиндр от дула до точки попадания; мировые координаты).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|FX")
	UStaticMeshComponent* TracerMesh;

public:

	// Выстрел по заблокированной цели
	virtual void Fire(AActor* Target) override;

	// Установить цель (вызывается из PlayerController при тапе по врагу)
	UFUNCTION(BlueprintCallable, Category = "Weapon|Combat")
	void SetTarget(AActor* NewTarget);

	// Сбросить цель
	UFUNCTION(BlueprintCallable, Category = "Weapon|Combat")
	void ClearTarget();

	// Пополнить запас патронов (резерв) на Amount, не превышая MaxAmmoReserve.
	// Используется покупкой патронов у торговца (Фаза 4, экономика, GDD §7.6).
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	void AddReserveAmmo(int32 Amount);

	// Слить ВЕСЬ резерв оружия (вернуть сколько было и обнулить). Фикс п.6 отчёта Рината
	// 23.08 («в инвентаре 3, а HUD пишет 12/51»): корень — ДВОЙНАЯ бухгалтерия, невидимый
	// резерв ствола (48 у пистолета из конструктора) плюс видимая пачка рюкзака. Единая
	// бухгалтерия: при покупке/взятии ствола резерв ПЕРЕЛИВАЕТСЯ пачкой в рюкзак (баланс
	// «пистолет приходит с патронами» цел — патроны видимы), перезарядка берёт из рюкзака
	// ровно недостающее в обойму, резерв между перезарядками пуст — плитка и HUD сходятся.
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 DrainReserveAmmo();

	// Есть ли активная цель
	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	FORCEINLINE bool HasTarget() const { return LockedTarget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	FORCEINLINE AActor* GetLockedTarget() const { return LockedTarget; }

	// --- Эффекты выстрела (D2) ---

	// Мировая точка дула: сокет MuzzleSocketName на меше, если он есть; иначе MuzzleOffset
	// относительно меша оружия; без меша — позиция актора.
	UFUNCTION(BlueprintPure, Category = "Weapon|FX")
	FVector GetMuzzleLocation() const;

	// Показывает вспышку у дула и след пули до TraceEnd (+опц. звук выстрела FireSound).
	// Зовут: Fire() игрока (bPlaySound=false — звук уже проигран) и ИИ бандита
	// (AEnemyAIController::PerformRangedAttack, bPlaySound=true).
	UFUNCTION(BlueprintCallable, Category = "Weapon|FX")
	void PlayFireVisuals(const FVector& TraceEnd, bool bPlaySound);

private:

	// LineTrace от мушки до цели, возвращает true если попал
	bool PerformLineTrace(AActor* Target, FHitResult& OutHit);

	// Ленивая инициализация динамического материала эффектов (цвет FireFXColor) на обоих мешах.
	void EnsureFXMaterial();

	// Динамический материал эффектов (создаётся один раз из FXMaterial).
	UPROPERTY()
	UMaterialInstanceDynamic* FXMID = nullptr;

	// Таймеры скрытия вспышки/следа (переиспользуются между выстрелами).
	FTimerHandle MuzzleFlashTimerHandle;
	FTimerHandle TracerTimerHandle;
};
