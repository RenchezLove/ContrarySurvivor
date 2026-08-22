// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbandonedCar.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

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
 * Каждая правка галочки/угла/цвета видна во вьюпорте сразу (OnConstruction). Логики нет —
 * чистый визуал-конструктор: галочки наличия деталей, углы открытия створок, цвет кузова,
 * целые/битые стёкла.
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
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AAbandonedCar : public AActor
{
	GENERATED_BODY()

public:
	AAbandonedCar();

protected:
	// Применяет галочки/углы/цвет/стёкла к компонентам — при каждой правке в редакторе.
	virtual void OnConstruction(const FTransform& Transform) override;

	// === НАЛИЧИЕ ДЕТАЛЕЙ (наверху Details; выключено = деталь скрыта) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "1", DisplayName = "Дверь передняя левая"))
	bool bDoorFL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "2", DisplayName = "Дверь передняя правая"))
	bool bDoorFR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "3", DisplayName = "Дверь задняя левая"))
	bool bDoorRL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "4", DisplayName = "Дверь задняя правая"))
	bool bDoorRR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "5", DisplayName = "Капот"))
	bool bHood = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "6", DisplayName = "Багажник"))
	bool bTrunk = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "7", DisplayName = "Колесо переднее левое"))
	bool bWheelFL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "8", DisplayName = "Колесо переднее правое"))
	bool bWheelFR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "9", DisplayName = "Колесо заднее левое"))
	bool bWheelRL = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "10", DisplayName = "Колесо заднее правое"))
	bool bWheelRR = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "11", DisplayName = "Царапины на кузове"))
	bool bScratches = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car", meta = (DisplayPriority = "12", DisplayName = "Блок-подпорка",
		ToolTip = "Подпорка под кузов (например, когда снято колесо). Ставится/двигается оператором в BP; здесь только показать/скрыть."))
	bool bBlock = false;

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

	// === КОМПОНЕНТЫ (меши-дефолты из Content/Environment/Props/AbandonedCar; в BP заменяемы) ===

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* DoorFL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* DoorFR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* DoorRL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* DoorRR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* Hood;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* Trunk;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* Glass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* Scratches;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* WheelFL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* WheelFR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* WheelRL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* WheelRR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car")
	UStaticMeshComponent* Block;

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

	// Красит одну деталь динамическим инстансом её материала слота 0 (лениво).
	void PaintPart(UStaticMeshComponent* Part);

	// Применяет состояние стёкол (видимость + параметр битости / скрытие без параметра).
	void ApplyGlassState();

	// Динамические инстансы (создаются при первом использовании; UPROPERTY — защита от GC).
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PaintMIDs;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GlassMID;
};
