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

// Слот огнестрела у игрока (Ринат 10.10.2026: «быстро переключаться между пистолетом и
// двуручным оружием»). Слотов два, и какой из них занимает ствол — признак самого оружия:
// короткое (пистолет) носится в одном, длинное двуручное (двустволка, будущие автоматы и
// винтовки) — в другом. Оба могут быть заняты одновременно.
UENUM(BlueprintType)
enum class EFirearmSlot : uint8
{
	Sidearm UMETA(DisplayName = "Короткое (пистолет)"),
	Long    UMETA(DisplayName = "Длинное двуручное"),
	Count   UMETA(Hidden)
};

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

	// --- Дробь (двустволка, 09.10.2026) ---
	// Оружие с числом дробин больше одной (или с ненулевым углом) стреляет ДРОБЬЮ: за один
	// выстрел из ствола вылетает несколько дробин, каждая со своим случайным отклонением
	// внутри конуса, и попадание считается по каждой отдельно — вблизи в цель приходит почти
	// всё, на дистанции часть уходит мимо. Значения по умолчанию (1 дробина, угол 0) — это
	// прежняя одиночная пуля: пистолет идёт старым путём без единого отличия.

	// Сколько дробин вылетает за один выстрел. Урон оружия (Damage) — это урон ОДНОЙ дробины.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat", meta = (ClampMin = "1", ClampMax = "32", DisplayPriority = "1",
		DisplayName = "Дробин за выстрел (1 = обычная пуля)",
		ToolTip = "Сколько дробин вылетает за один выстрел. 1 = обычная пуля (как у пистолета). Урон оружия считается ЗА ОДНУ дробину: залп в упор = урон x число дробин."))
	int32 PelletsPerShot = 1;

	// Полный угол конуса разброса дроби в градусах (16 = по 8 в каждую сторону от линии
	// прицеливания). Дальность полёта дробины — поле Range оружия.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Combat", meta = (ClampMin = "0.0", ClampMax = "90.0", DisplayPriority = "2",
		DisplayName = "Угол разброса дроби, полный конус (градусы)",
		ToolTip = "Полный угол конуса, внутри которого случайно разлетаются дробины. Чем больше угол, тем больше дробин уходит мимо цели на дистанции. 0 при одной дробине = обычная пуля."))
	float PelletSpreadAngle = 0.0f;

	// --- Вид патрона (двустволка, 09.10.2026) ---
	// Каким патроном заряжается оружие: имя строки таблицы предметов (ammo_9mm, ammo_12g).
	// Перезарядка берёт из рюкзака только пачки этого вида, счётчик на экране считает только
	// их. По умолчанию — прежний пистолетный патрон.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (DisplayPriority = "1",
		DisplayName = "Вид патрона (строка таблицы предметов)",
		ToolTip = "Каким патроном заряжается оружие: имя строки таблицы предметов (ammo_9mm — пистолетный, ammo_12g — дробовой). Должно совпадать с видом патрона у пачки."))
	FName AmmoType;

	// Какой слот оружия игрока занимает этот ствол (см. EFirearmSlot). По умолчанию короткое —
	// прежний пистолет; длинное оружие ставит значение в своём классе или чертеже.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Stats", meta = (DisplayPriority = "1",
		DisplayName = "Слот оружия у игрока",
		ToolTip = "В какой слот оружия игрока встаёт этот ствол: короткое (пистолет) или длинное двуручное (ружьё, автомат, винтовка). Стволы разных слотов носятся одновременно, кнопка смены оружия переключает между ними."))
	EFirearmSlot FirearmSlot = EFirearmSlot::Sidearm;

	// --- Звук выстрела (Демо; вариации — Ринат 30.08) ---
	// Проигрывается в момент реального выстрела (не на пустой обойме). Выстрел бандита
	// переиспользует ту же логику (PlayFireVisuals с bPlaySound=true), сам выбор звука —
	// PlayFireSound().

	// Набор звуков выстрела: каждый раз берётся НЕ тот, что играл в прошлый выстрел
	// (задание Рината 30.08 «каждый выстрел не похож на предыдущий»). Дефолт — два куска
	// нарезки A_34P (1911): /Game/Audio/Demo/pistol_1911_shot_1 и _2, грузятся в
	// конструкторе. Пустой список — запасной путь на одиночный «Звук выстрела» ниже.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio", meta = (DisplayPriority = "1",
		DisplayName = "Звуки выстрела (вариации)"))
	TArray<TObjectPtr<USoundBase>> FireSoundVariations;

	// Разброс высоты тона на выстрел: 0.05 = каждый выстрел случайно на ±5% выше/ниже.
	// Дешёвая добавка к вариациям, чтобы даже один и тот же сэмпл не звучал штампом.
	// 0 — выключено.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio", meta = (ClampMin = "0.0", ClampMax = "0.5", DisplayPriority = "2",
		DisplayName = "Разброс высоты тона (доля)"))
	float FireSoundPitchVariation = 0.05f;

	// Одиночный звук выстрела — запасной путь, если список вариаций пуст (и совместимость
	// со старыми BP-переопределениями). Дефолт — прежний pistol_22_gunshot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio", meta = (DisplayPriority = "3"))
	USoundBase* FireSound;

	// Громкость выстрела. Тюнингуется (дефолт 1.0 — фидбек Рината 07-17 «слишком тихо»).
	// Итог в игре = эта громкость x ползунок «Эффекты» на экране настроек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio", meta = (ClampMin = "0.0", DisplayPriority = "4"))
	float FireSoundVolume;

	// Выбрать и проиграть звук выстрела в точке оружия (вариации + разброс тона + громкость
	// с настройками). Общая точка для выстрела игрока (Fire) и бандита (PlayFireVisuals).
	void PlayFireSound();

	// Индекс звука, игравшего в ПРОШЛЫЙ выстрел (-1 — ещё не стреляли): следующий выбор
	// его избегает; при двух звуках это чистое чередование.
	int32 LastFireSoundIndex = -1;

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

	// --- Дробь и вид патрона (двустволка, 09.10.2026) ---

	// Стреляет ли оружие дробью (иначе — прежняя одиночная пуля).
	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	bool IsPelletWeapon() const { return PelletsPerShot > 1 || PelletSpreadAngle > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	int32 GetPelletsPerShot() const { return FMath::Max(1, PelletsPerShot); }

	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	float GetPelletSpreadAngle() const { return PelletSpreadAngle; }

	// Вид патрона оружия (пустое поле = прежний пистолетный).
	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FName GetAmmoType() const;

	// Слот оружия игрока, который занимает этот ствол.
	UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
	EFirearmSlot GetFirearmSlot() const { return FirearmSlot; }

	// Случайные направления дробин внутри конуса вокруг AimDir (полный угол в градусах).
	// Чистый расчёт без мира — его же проверяет автотест «дробины не выходят за конус».
	static void BuildPelletDirections(const FVector& AimDir, int32 Count, float FullConeAngleDeg,
		TArray<FVector>& OutDirections);

	// Один залп дроби без траты патрона и без эффектов: трассы из Start вдоль AimDir на
	// дальность Range. OutHitsByActor — сколько дробин пришло в каждого задетого актора,
	// OutPelletEnds — где закончила полёт каждая дробина (для следов). Возвращает число
	// попавших дробин. Урон не наносит — его раздаёт Fire(). Публичная — для автотестов.
	int32 TracePellets(const FVector& Start, const FVector& AimDir,
		TMap<AActor*, int32>& OutHitsByActor, TArray<FVector>& OutPelletEnds) const;

	// Итог последнего выстрела дробью: сколько дробин вылетело и сколько из них попало
	// хоть во что-то (для журнала и автотестов).
	int32 GetLastShotPelletsFired() const { return LastShotPelletsFired; }
	int32 GetLastShotPelletsHit() const { return LastShotPelletsHit; }

private:

	// LineTrace от мушки до цели, возвращает true если попал
	bool PerformLineTrace(AActor* Target, FHitResult& OutHit);

	// Выстрел дробью по цели: залп, урон по каждому задетому актору (одним вызовом на
	// актора — сумма его дробин), следы всех дробин. Патрон и звук — в Fire().
	void FirePellets(AActor* Target);

	// Растягивает меш следа от From до To и показывает его. false — отрезок слишком короткий.
	bool PlaceTracer(UStaticMeshComponent* Mesh, const FVector& From, const FVector& To) const;

	// Следы дробин: первый рисует штатный след пули (вместе со вспышкой), остальные —
	// дополнительные меши, которые создаются один раз и переиспользуются между выстрелами.
	void PlayPelletVisuals(const TArray<FVector>& PelletEnds);

	// Дополнительные меши следов для дроби (сверх штатного TracerMesh).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ExtraTracerMeshes;

	FTimerHandle ExtraTracersTimerHandle;

	int32 LastShotPelletsFired = 0;
	int32 LastShotPelletsHit = 0;

	// Ленивая инициализация динамического материала эффектов (цвет FireFXColor) на обоих мешах.
	void EnsureFXMaterial();

	// Динамический материал эффектов (создаётся один раз из FXMaterial).
	UPROPERTY()
	UMaterialInstanceDynamic* FXMID = nullptr;

	// Таймеры скрытия вспышки/следа (переиспользуются между выстрелами).
	FTimerHandle MuzzleFlashTimerHandle;
	FTimerHandle TracerTimerHandle;
};
