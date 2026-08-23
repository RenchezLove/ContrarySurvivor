// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ContrarySurvivor/Actors/Pickup.h" // FPlacedLootEntry — формат «какие конкретно предметы»
#include "AbandonedCar.generated.h"

class UCorpseLootComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

// Дороговизна лута машины (Report1 п.9: «3 варианта: дешевый, средний, дорогой») —
// выбирает денежный диапазон (поля «Деньги: …» на машине).
UENUM(BlueprintType)
enum class ECarLootRichness : uint8
{
	Cheap UMETA(DisplayName = "Дешёвый"),
	Medium UMETA(DisplayName = "Средний"),
	Expensive UMETA(DisplayName = "Дорогой")
};

/**
 * Конструктор брошенных авто (ADR-075 п.4, спека spec-datatables-phase2.md группа 7).
 *
 * Разборная модель уже в проекте — Content/Environment/Props/AbandonedCar (проверено по
 * дереву Content 22.08): кузов SM_AbandonedCar_Body, четыре двери _Door_FL/_FR/_RL/_RR,
 * капот _Hood, багажник _Trunk, колесо _Wheel (один меш, ставится четырежды), стёкла
 * _Glass, царапины _Scratches, блок-подпорка _Block; материалы M_CarPaint/MI_CarPaint
 * (кузов) и M_CarGlass (стёкла).
 *
 * ИНСТРУМЕНТ-актор (образец оформления — ACampfire/AWorldBorder): один C++ класс, один
 * BP_AbandonedCar (ADR-028), расстановка и настройка на РАЗМЕЩЁННОМ экземпляре — Ринат.
 * Каждая правка галочки/угла/цвета видна во вьюпорте сразу (OnConstruction). Визуал-
 * конструктор: галочки наличия и битости деталей, углы открытия створок, цвет кузова,
 * целые/битые стёкла; из игровой логики — только контейнер обыска (см. блок п.9 ниже).
 *
 * БИТЫЕ СТЁКЛА: отдельного меша битого стекла в папке НЕТ (проверено), поэтому битость
 * решается материалом — скалярный параметр (имя настраивается полем GlassBrokenParamName)
 * выставляется в 1. Если у материала стекла такого параметра нет, деградация честная:
 * «битые» = стекло скрыто (оговорка ADR-075 «решение за исполнителем по факту»).
 *
 * МОНТАЖ И ПЕТЛИ СТВОРОК (паспорт моделлера assets/abandoned_car/abandoned_car.asset.md):
 * пивот каждой навесной детали лежит НА ОСИ ПЕТЛИ (задумано моделлером под простой
 * поворот), а сами меши экспортированы с нулевым трансформом — деталь нужно СНАЧАЛА
 * поставить на место. Поэтому у шести створок два поля: «монтажная точка» (куда деталь
 * крепится на кузове; дефолты — из схемы сборки паспорта, метры переведены в см) и
 * «петля» (точка оси вращения в локальных координатах детали; при пивоте-на-петле
 * остаётся нулём — вращение вокруг пивота). Позиция створки считается формулой
 * «монтажная точка + петля − поворот·петля»: точка на оси петли неподвижна при любом
 * угле, и это верно и для переопределённых в BP мешей с пивотом не на петле.
 * Оси (паспорт): двери — петля вертикальная (рыскание); капот/багажник — ось X (крен,
 * НЕ тангаж: нос машины = -Y). Знак угла открывания подбирается по месту (FBX-зеркало Y).
 * ⚠ Код ПЕРЕЗАПИСЫВАЕТ RelativeLocation/Rotation створок и стёкол из полей монтажа/угла/
 * петли — двигать их мышкой в BP бесполезно, положение задаётся ТОЛЬКО полями (иначе
 * OnConstruction копил бы повороты). Колёса и блок код не двигает — их расставляет
 * оператор в BP (координаты — в той же схеме сборки паспорта).
 *
 * КОЛЛИЗИЯ (дёшево для телефона): только кузов, блокирует всё, КРОМЕ камеры (ECC_Camera
 * Ignore — пружина камеры не дёргается об машину сверху). Pawn — не пройти насквозь,
 * Visibility — машина честное укрытие от выстрелов и ИИ (гейт прямой видимости атак 08-07).
 * Детали коллизии не несут.
 *
 * ОТЧЁТ РИНАТА 23.08.2026, п.9 («BP_AbandonedCar пока очень сырой»):
 *  а) простыня Mobility на каждую деталь и б) слоты мешей деталей убраны из панели
 *     Details: компоненты больше не VisibleAnywhere (разворачиваемые секции компонентов
 *     в деталях актора давали и Mobility, и слот меша на каждую из 14 деталей).
 *     Подменить меш детали по-прежнему можно в BP_AbandonedCar через дерево компонентов
 *     (наследованные компоненты в нём остаются);
 *  в) «как бы таблица» деталей (название | есть | битая) — галочки лежат здесь, а ТАБЛИЦЕЙ
 *     их рисует кастомизация панели FAbandonedCarDetails (редакторный модуль). Галочка
 *     «битая» затемняет деталь параметром цвета её материала (доля — «Затемнение битой
 *     детали»); у материала без параметра цвета (резина колеса) битость пока только
 *     данные без визуала — вид битой детали уточняется у Рината;
 *  +) ОБЫСК МАШИНЫ: контейнер обыска (UCorpseLootComponent, «отдельное хранилище» — в
 *     групповой обыск не входит, окно своё). Поля: «Лут в машине есть», дороговизна
 *     (дешёвый/средний/дорогой — задаёт денежный диапазон), список конкретных предметов
 *     (строкой таблицы предметов или классом + количество, формат мешка ADR-076 п.10).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AAbandonedCar : public AActor
{
	GENERATED_BODY()

	// Кастомизация панели Details (редакторный модуль): рисует галочки таблицей
	// «Деталь | Есть | Битая» и берёт имена полей через GET_MEMBER_NAME_CHECKED.
	friend class FAbandonedCarDetails;

public:
	AAbandonedCar();

	// Денежный диапазон по дороговизне — чистое правило, проверяется автотестом.
	FInt32Interval MoneyRangeForRichness(ECarLootRichness Richness) const;

	// Контейнер обыска машины (для автотестов и подсказки контроллера).
	UCorpseLootComponent* GetLootContainer() const { return CarLoot; }

protected:
	// Применяет галочки/углы/цвет/стёкла к компонентам — при каждой правке в редакторе.
	virtual void OnConstruction(const FTransform& Transform) override;

	// Наполняет контейнер обыска (только в игровом мире; в редакторе машина — визуал).
	virtual void BeginPlay() override;

	// === ДЕТАЛИ КУЗОВА: есть / битая (Report1 п.9 — «как бы таблица»; таблицей рисует
	// FAbandonedCarDetails, здесь — сами галочки; нечётный приоритет = есть, чётный = битая) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "1", DisplayName = "Дверь передняя левая"))
	bool bDoorFL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "2", DisplayName = "Дверь передняя левая: битая", EditCondition = "bDoorFL"))
	bool bDoorFLBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "3", DisplayName = "Дверь передняя правая"))
	bool bDoorFR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "4", DisplayName = "Дверь передняя правая: битая", EditCondition = "bDoorFR"))
	bool bDoorFRBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "5", DisplayName = "Дверь задняя левая"))
	bool bDoorRL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "6", DisplayName = "Дверь задняя левая: битая", EditCondition = "bDoorRL"))
	bool bDoorRLBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "7", DisplayName = "Дверь задняя правая"))
	bool bDoorRR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "8", DisplayName = "Дверь задняя правая: битая", EditCondition = "bDoorRR"))
	bool bDoorRRBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "9", DisplayName = "Капот"))
	bool bHood = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "10", DisplayName = "Капот: битый", EditCondition = "bHood"))
	bool bHoodBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "11", DisplayName = "Багажник"))
	bool bTrunk = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "12", DisplayName = "Багажник: битый", EditCondition = "bTrunk"))
	bool bTrunkBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "13", DisplayName = "Колесо переднее левое"))
	bool bWheelFL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "14", DisplayName = "Колесо переднее левое: битое", EditCondition = "bWheelFL"))
	bool bWheelFLBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "15", DisplayName = "Колесо переднее правое"))
	bool bWheelFR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "16", DisplayName = "Колесо переднее правое: битое", EditCondition = "bWheelFR"))
	bool bWheelFRBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "17", DisplayName = "Колесо заднее левое"))
	bool bWheelRL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "18", DisplayName = "Колесо заднее левое: битое", EditCondition = "bWheelRL"))
	bool bWheelRLBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "19", DisplayName = "Колесо заднее правое"))
	bool bWheelRR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "20", DisplayName = "Колесо заднее правое: битое", EditCondition = "bWheelRR"))
	bool bWheelRRBroken = false;

	// У царапин и подпорки «битости» нет — это накладки, а не детали кузова.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "21", DisplayName = "Царапины на кузове"))
	bool bScratches = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "22", DisplayName = "Блок-подпорка",
		ToolTip = "Подпорка под кузов (например, когда снято колесо). Ставится/двигается оператором в BP; здесь только показать/скрыть."))
	bool bBlock = false;

	// === ЛУТ МАШИНЫ (Report1 п.9: обыск автомобиля) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Лут", meta = (DisplayPriority = "1",
		DisplayName = "Лут в машине есть",
		ToolTip = "Включено — машину можно обыскать: внутри деньги по дороговизне и предметы из списка ниже."))
	bool bHasLoot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Лут", meta = (DisplayPriority = "2",
		DisplayName = "Дороговизна лута", EditCondition = "bHasLoot",
		ToolTip = "Задаёт, из какого денежного диапазона машина возьмёт сумму (поля «Деньги: …» ниже)."))
	ECarLootRichness LootRichness = ECarLootRichness::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Лут", meta = (DisplayPriority = "3",
		TitleProperty = "ItemRow",
		DisplayName = "Какие конкретно предметы", EditCondition = "bHasLoot",
		ToolTip = "Список предметов в машине: каждая запись — предмет (строкой таблицы предметов DT_Items или классом) и количество. Формат тот же, что у мешка-пикапа."))
	TArray<FPlacedLootEntry> PlacedLootList;

	// Денежные диапазоны дороговизны (от..до, монеты целиком). Допущение cpp-dev 23.08 —
	// числа стартовые, крутятся здесь без пересборки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Лут", meta = (DisplayPriority = "4", DisplayName = "Деньги: дешёвый (от..до)"))
	FInt32Interval CheapMoney = FInt32Interval(5, 15);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Лут", meta = (DisplayPriority = "5", DisplayName = "Деньги: средний (от..до)"))
	FInt32Interval MediumMoney = FInt32Interval(20, 45);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Лут", meta = (DisplayPriority = "6", DisplayName = "Деньги: дорогой (от..до)"))
	FInt32Interval ExpensiveMoney = FInt32Interval(50, 90);

	// === СТЁКЛА ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Стёкла", meta = (DisplayPriority = "1", DisplayName = "Стёкла есть"))
	bool bGlass = true;

	// Битые стёкла: параметр материала (имя ниже) = 1. Параметра у материала нет —
	// стекло честно скрывается (см. шапку класса).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Стёкла", meta = (DisplayPriority = "2", DisplayName = "Стёкла битые",
		EditCondition = "bGlass"))
	bool bGlassBroken = false;

	// Имя скалярного параметра «битости» в материале стекла (контракт с материалом НЕ
	// зашивается в код — оператор сверяет с M_CarGlass и правит здесь без пересборки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Стёкла", meta = (DisplayPriority = "3", DisplayName = "Имя параметра битости в материале",
		EditCondition = "bGlass && bGlassBroken"))
	FName GlassBrokenParamName = FName(TEXT("Broken"));

	// === ЦВЕТ КУЗОВА ===

	// Выключено = кузов остаётся с материалом ассета (MI_CarPaint) как есть.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Цвет", meta = (DisplayPriority = "1", DisplayName = "Перекрасить кузов"))
	bool bOverrideBodyColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Цвет", meta = (DisplayPriority = "2", DisplayName = "Цвет кузова",
		EditCondition = "bOverrideBodyColor"))
	FLinearColor BodyColor = FLinearColor(0.35f, 0.05f, 0.03f, 1.0f); // ржаво-красный по умолчанию

	// Имя параметра цвета в материале кузова (сверяется с M_CarPaint/MI_CarPaint оператором,
	// правится без пересборки — контракт с материалом не зашит в код).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Цвет", meta = (DisplayPriority = "3", DisplayName = "Имя параметра цвета в материале",
		EditCondition = "bOverrideBodyColor"))
	FName PaintColorParamName = FName(TEXT("Color"));

	// Красить и створки (двери/капот/багажник — они из того же материала кузова).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Цвет", meta = (DisplayPriority = "4", DisplayName = "Красить и двери/капот/багажник",
		EditCondition = "bOverrideBodyColor"))
	bool bPaintMovableParts = true;

	// Report1 п.9: галочка «битая» затемняет деталь — цвет умножается на эту долю (через
	// тот же параметр цвета материала, что и перекраска). У материала без параметра цвета
	// (резина колеса) визуала битости нет — только данные.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Цвет", meta = (DisplayPriority = "5", DisplayName = "Затемнение битой детали (доля цвета)",
		ClampMin = "0.0", ClampMax = "1.0"))
	float BrokenTintMultiplier = 0.55f;

	// === УГЛЫ ОТКРЫТИЯ СТВОРОК (решение лида 22.08: визуал «брошенности»). 0 = закрыто. ===
	// Двери открываются вокруг вертикали (рыскание), капот и багажник — вокруг поперечной
	// оси (тангаж). Знак угла подбирается на глаз (зависит от того, как смотрит меш).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки", meta = (DisplayPriority = "1", DisplayName = "Дверь ПЛ: угол (°)",
		ClampMin = "-179.0", ClampMax = "179.0", EditCondition = "bDoorFL"))
	float DoorFLOpenAngleDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки", meta = (DisplayPriority = "2", DisplayName = "Дверь ПП: угол (°)",
		ClampMin = "-179.0", ClampMax = "179.0", EditCondition = "bDoorFR"))
	float DoorFROpenAngleDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки", meta = (DisplayPriority = "3", DisplayName = "Дверь ЗЛ: угол (°)",
		ClampMin = "-179.0", ClampMax = "179.0", EditCondition = "bDoorRL"))
	float DoorRLOpenAngleDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки", meta = (DisplayPriority = "4", DisplayName = "Дверь ЗП: угол (°)",
		ClampMin = "-179.0", ClampMax = "179.0", EditCondition = "bDoorRR"))
	float DoorRROpenAngleDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки", meta = (DisplayPriority = "5", DisplayName = "Капот: угол (°)",
		ClampMin = "-179.0", ClampMax = "179.0", EditCondition = "bHood"))
	float HoodOpenAngleDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки", meta = (DisplayPriority = "6", DisplayName = "Багажник: угол (°)",
		ClampMin = "-179.0", ClampMax = "179.0", EditCondition = "bTrunk"))
	float TrunkOpenAngleDeg = 0.0f;

	// === МОНТАЖНЫЕ ТОЧКИ СТВОРОК (см, относительно кузова; origin кузова = центр на земле,
	// нос = -Y). Дефолты — схема сборки паспорта моделлера (метры x100). Меши створок
	// экспортированы с нулевым трансформом — без монтажной точки все собрались бы кучей
	// в центре машины. ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Монтаж двери ПЛ", DisplayPriority = "1"))
	FVector DoorFLMount = FVector(-80.0f, -47.0f, 31.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Монтаж двери ПП", DisplayPriority = "2"))
	FVector DoorFRMount = FVector(80.0f, -47.0f, 31.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Монтаж двери ЗЛ", DisplayPriority = "3"))
	FVector DoorRLMount = FVector(-80.0f, 37.5f, 31.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Монтаж двери ЗП", DisplayPriority = "4"))
	FVector DoorRRMount = FVector(80.0f, 37.5f, 31.5f);

	// Z капота/багажника — живая UE-подгонка из паспорта (0.880/0.893, не расчётные 0.830/0.843).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Монтаж капота", DisplayPriority = "5"))
	FVector HoodMount = FVector(0.0f, -59.0f, 88.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Монтаж багажника", DisplayPriority = "6"))
	FVector TrunkMount = FVector(0.0f, 137.5f, 89.3f);

	// === ПЕТЛИ СТВОРОК (локальные координаты детали, см). Ноль = вращение вокруг пивота
	// меша — штатный случай: у мешей комплекта пивот лежит НА ОСИ ПЕТЛИ (паспорт).
	// Поле нужно переопределённым мешам, у которых пивот не на петле. ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Петля двери ПЛ"))
	FVector DoorFLHinge = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Петля двери ПП"))
	FVector DoorFRHinge = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Петля двери ЗЛ"))
	FVector DoorRLHinge = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Петля двери ЗП"))
	FVector DoorRRHinge = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Петля капота"))
	FVector HoodHinge = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Створки|Петли", meta = (DisplayName = "Петля багажника"))
	FVector TrunkHinge = FVector::ZeroVector;

	// === КОМПОНЕНТЫ (меши-дефолты из Content/Environment/Props/AbandonedCar). Report1 п.9
	// а/б: БЕЗ VisibleAnywhere намеренно — разворачиваемые секции компонентов в Details
	// давали Ринату простыню Mobility и слоты мешей на каждую деталь. Подмена меша —
	// в BP_AbandonedCar через дерево компонентов (наследованные компоненты там остаются). ===

	UPROPERTY()
	USceneComponent* SceneRoot;

	UPROPERTY()
	UStaticMeshComponent* Body;

	UPROPERTY()
	UStaticMeshComponent* DoorFL;

	UPROPERTY()
	UStaticMeshComponent* DoorFR;

	UPROPERTY()
	UStaticMeshComponent* DoorRL;

	UPROPERTY()
	UStaticMeshComponent* DoorRR;

	UPROPERTY()
	UStaticMeshComponent* Hood;

	UPROPERTY()
	UStaticMeshComponent* Trunk;

	UPROPERTY()
	UStaticMeshComponent* Glass;

	UPROPERTY()
	UStaticMeshComponent* Scratches;

	UPROPERTY()
	UStaticMeshComponent* WheelFL;

	UPROPERTY()
	UStaticMeshComponent* WheelFR;

	UPROPERTY()
	UStaticMeshComponent* WheelRL;

	UPROPERTY()
	UStaticMeshComponent* WheelRR;

	UPROPERTY()
	UStaticMeshComponent* Block;

	// Контейнер обыска машины (Report1 п.9): «отдельное хранилище» — своё окно, в групповой
	// обыск не входит; наполняется в BeginPlay. Настройки самого контейнера скрыты — правда
	// лута живёт в полях «Car|Лут» выше.
	UPROPERTY()
	UCorpseLootComponent* CarLoot;

private:
	// Фабрика конструктора: деталь без коллизии, приаттачена к кузову, меш из папки машины
	// (путь может не разрешиться — деталь остаётся пустой, BP назначит).
	UStaticMeshComponent* CreatePart(const TCHAR* SubobjectName, const TCHAR* MeshAssetPath);

	// Видимость детали по галочке (без пересоздания компонентов).
	static void SetPartVisible(UStaticMeshComponent* Part, bool bVisible);

	// Ставит створку на монтажную точку и поворачивает на AngleDeg вокруг точки-петли
	// HingeLocal (локальные координаты детали): позиция = монтаж + петля − поворот·петля.
	// bYaw: двери — вокруг вертикали (рыскание); капот/багажник — вокруг оси X (крен,
	// ось петли по паспорту; нос машины = -Y). ПЕРЕЗАПИСЫВАЕТ RelativeLocation/Rotation.
	static void ApplyHinge(UStaticMeshComponent* Part, const FVector& MountLocal,
		const FVector& HingeLocal, float AngleDeg, bool bYaw);

	// Итоговый цвет детали: перекраска (BodyColor) и/или затемнение битости поверх родного
	// цвета материала. Ни того ни другого — возвращает родное значение, если MID уже висел.
	void ApplyPartColor(UStaticMeshComponent* Part, bool bPaintOverride, bool bBroken);

	// Применяет состояние стёкол (видимость + параметр битости / скрытие без параметра).
	void ApplyGlassState();

	// Динамические инстансы (создаются при первом использовании; UPROPERTY — защита от GC).
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PaintMIDs;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GlassMID;
};
