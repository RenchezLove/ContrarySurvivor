// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterInventoryItem.h"
#include "AMasterWeapon.generated.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	OneHanded    UMETA(DisplayName = "One Handed"),   // Пистолет, нож
	TwoHanded    UMETA(DisplayName = "Two Handed")    // Дробовик
};

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AMasterWeapon : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AMasterWeapon();

protected:
	virtual void BeginPlay() override;

	// --- Параметры оружия ---

	// meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината), сразу после Transform.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats", meta = (DisplayPriority = "1"))
	float Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats")
	float FireRate;           // Выстрелов в секунду

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats")
	float Range;              // Дальность (в Unreal units)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats")
	EWeaponType WeaponType;

	// --- Патроны ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (DisplayPriority = "2"))
	int32 MaxAmmoInClip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmoInClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MaxAmmoReserve;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmoReserve;

	// --- Хват (Build 1.2.2, Ринат: «пистолет нормально, но нож теперь лежит неправильно») ---
	// Скелетный сокет WeaponGripSocket ОДИН на всех, а лежать в ладони каждое оружие должно
	// по-своему. Поэтому поза сокета = эталон ПИСТОЛЕТА (Ринат подбирал её с превью пистолета),
	// а каждый класс оружия несёт СВОЮ цифровую поправку, которую EquipWeapon применяет ПОВЕРХ
	// сокета всегда (раньше при сокете поправки игнорировались — это и сломало нож).
	// Пистолет = нулевая поправка. Нож = инверсия сокет-трансформа (поза кости R_Hand, как
	// до правки сокета) — задаётся в конструкторе AMeleeWeapon, расчёт в ADR волны.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Grip", meta = (DisplayPriority = "1",
		DisplayName = "Поправка хвата: сдвиг",
		ToolTip = "Сдвиг оружия в ладони ОТНОСИТЕЛЬНО сокета хвата (в местных единицах сокета). Применяется всегда, поверх позиции сокета. Ноль = оружие лежит ровно по сокету (эталон пистолета)."))
	FVector GripOffsetLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Grip", meta = (DisplayPriority = "2",
		DisplayName = "Поправка хвата: поворот",
		ToolTip = "Поворот оружия в ладони ОТНОСИТЕЛЬНО сокета хвата. Применяется всегда, поверх позиции сокета. Ноль = оружие повёрнуто ровно по сокету (эталон пистолета)."))
	FRotator GripOffsetRotation;

	// Задел на двуручное оружие (Build 1.2.2, Ринат: «какое-то оружие будет удерживаться
	// двумя руками»): имя сокета ЛЕВОЙ руки на МЕШЕ САМОГО ОРУЖИЯ. Пусто = одноручное.
	// Пока только данные (IK левой руки подключится отдельной волной; см. ADR волны 1.2.2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Grip", meta = (DisplayPriority = "3",
		DisplayName = "Сокет левой руки на меше оружия (пусто = одноручное)",
		ToolTip = "Имя сокета на меше самого оружия, за который возьмётся ЛЕВАЯ рука двуручного хвата. Пусто = оружие одноручное. Пока задел данных: IK левой руки будет подключён отдельно."))
	FName LeftHandGripSocketName;

	// --- Состояние ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State", meta = (DisplayPriority = "3"))
	bool bIsReloading;

	// Таймер между выстрелами
	float LastFireTime;

public:

	// --- Основные функции ---

	// Выстрел — переопределяется в дочерних классах
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire(AActor* Target);

	// Перезарядка
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Reload();

	// Можно ли стрелять прямо сейчас
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanFire() const;

	// Есть ли патроны для перезарядки
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanReload() const;

	// --- Геттеры ---

	UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
	FORCEINLINE float GetDamage() const { return Damage; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
	FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FORCEINLINE int32 GetCurrentAmmoInClip() const { return CurrentAmmoInClip; }

	// Ёмкость обоймы (фикс п.6 отчёта 23.08: перезарядка тянет из рюкзака ровно недостающее
	// В ОБОЙМУ, а не полный запас резерва — патроны живут в рюкзаке, видимые игроку).
	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FORCEINLINE int32 GetMaxAmmoInClip() const { return MaxAmmoInClip; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FORCEINLINE int32 GetCurrentAmmoReserve() const { return CurrentAmmoReserve; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FORCEINLINE int32 GetMaxAmmoReserve() const { return MaxAmmoReserve; }

	// Сколько ещё патронов влезает в резерв оружия (для пополнения из рюкзака при перезарядке).
	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FORCEINLINE int32 GetReserveSpace() const { return FMath::Max(0, MaxAmmoReserve - CurrentAmmoReserve); }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	FORCEINLINE bool GetIsReloading() const { return bIsReloading; }

	// --- Хват (Build 1.2.2) ---

	UFUNCTION(BlueprintPure, Category = "Weapon|Grip")
	FORCEINLINE FVector GetGripOffsetLocation() const { return GripOffsetLocation; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Grip")
	FORCEINLINE FRotator GetGripOffsetRotation() const { return GripOffsetRotation; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Grip")
	FORCEINLINE FName GetLeftHandGripSocketName() const { return LeftHandGripSocketName; }
};
