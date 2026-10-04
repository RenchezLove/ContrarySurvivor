// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ContrarySurvivor/Actors/Pickup.h" // FPlacedLootEntry — формат «какие конкретно предметы»
#include "BusStop.generated.h"

class ACampfire;
class UChildActorComponent;
class UCorpseLootComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Автобусная остановка (Ринат 04.10.2026: «Сделай мастер остановку (BP). По аналогии с
 * мастер брошенным автомобилем. Чтобы можно было настраивать цвет крыши, Цвет урны. Наклон
 * покосившегося знака автобусной остановки. Лут автобусной остановки. Цвет металлопрофиля
 * боков остановки (стеночек). Наличие / отсутствие костра напротив остановки. А также
 * является ли этот костёр точкой сохранения (true\false)»).
 *
 * ИНСТРУМЕНТ-актор по образцу AAbandonedCar: один C++ класс, один BP_BusStop, настройка на
 * РАЗМЕЩЁННОМ экземпляре; каждая правка видна во вьюпорте сразу (OnConstruction).
 *
 * МЕШИ: навес SM_BusStop, знак SM_BusStopSign, урна SM_BusStopUrn из
 * Content/Environment/Props/BusStop. На момент написания (04.10.2026) ассетов ещё нет — их
 * делает моделлер; пока пакета нет на диске, компонент остаётся пустым без ошибок в журнале,
 * после появления ассета меш подхватится сам при следующем запуске редактора.
 * Допущения по мешам (сверить с паспортом моделлера): начало координат навеса — центр на
 * земле, открытая сторона смотрит в +Y; пивот знака — основание столба (наклон идёт вокруг
 * него); все три меша на одном материале с тремя векторными параметрами цвета.
 *
 * ЦВЕТ: три поля цвета пишутся в векторные параметры материала (имена — поля ниже, контракт
 * с материалом в код не зашит). Значение поля ставится всегда; белый по умолчанию рассчитан
 * на параметр-множитель («белый = как нарисовано»).
 *
 * ⚠ Код ПЕРЕЗАПИСЫВАЕТ положение и поворот знака, урны и точки костра из полей — двигать их
 * мышкой в BP бесполезно (иначе OnConstruction копил бы наклоны).
 *
 * ПРЕГРАДА: навес и урна блокируют всё, КРОМЕ камеры (как кузов машины); знак преграды не несёт.
 * Корень и меши неподвижные (Static) — тени запекаются.
 *
 * КОСТЁР: дочерний актор класса «Класс костра». Галочка «Костёр напротив остановки» создаёт
 * и убирает его; галочка «Костёр — точка сохранения» передаётся костру (ACampfire::
 * SetIsSavePoint): выключено — костёр горит, но не сохраняет и точкой возрождения не служит.
 *
 * ОБЫСК: контейнер UCorpseLootComponent, «отдельное хранилище» — как у машины: своё окно,
 * в групповой обыск не входит; наполняется в BeginPlay.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ABusStop : public AActor
{
	GENERATED_BODY()

public:
	ABusStop();

	// Контейнер обыска остановки (для автотестов и подсказки контроллера).
	UCorpseLootComponent* GetLootContainer() const { return StopLoot; }

	// Костёр остановки (nullptr — галочка снята или класс костра не задан).
	UFUNCTION(BlueprintPure, Category = "BusStop")
	ACampfire* GetCampfire() const;

	// Поворот покосившегося знака — чистое правило, проверяется автотестом: верх знака
	// заваливается на TiltDeg градусов в сторону DirectionDeg (0 = вдоль +X остановки,
	// 90 = в сторону +Y, то есть к открытой стороне навеса).
	static FQuat ComputeSignTilt(float TiltDeg, float DirectionDeg);

	// Дочерний костёр создаётся при регистрации компонентов (в том числе при загрузке уровня
	// в игре, где OnConstruction не зовётся) — признак точки сохранения доносим и здесь.
	virtual void PostRegisterAllComponents() override;

protected:
	// Применяет цвета, наклон знака, положение урны и костёр — при каждой правке в редакторе.
	virtual void OnConstruction(const FTransform& Transform) override;

	// Наполняет контейнер обыска (только в игровом мире; в редакторе остановка — визуал).
	virtual void BeginPlay() override;

	// === ЦВЕТ ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Цвет", meta = (DisplayPriority = "1", DisplayName = "Цвет крыши"))
	FLinearColor RoofColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Цвет", meta = (DisplayPriority = "2", DisplayName = "Цвет профиля стенок"))
	FLinearColor WallColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Цвет", meta = (DisplayPriority = "3", DisplayName = "Цвет урны"))
	FLinearColor UrnColor = FLinearColor::White;

	// Имена векторных параметров материала (сверяются с материалом остановки оператором,
	// правятся без пересборки — контракт с материалом не зашит в код).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Цвет", meta = (DisplayPriority = "4", DisplayName = "Имя параметра цвета крыши в материале"))
	FName RoofColorParamName = FName(TEXT("RoofTint"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Цвет", meta = (DisplayPriority = "5", DisplayName = "Имя параметра цвета стенок в материале"))
	FName WallColorParamName = FName(TEXT("WallTint"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Цвет", meta = (DisplayPriority = "6", DisplayName = "Имя параметра цвета урны в материале"))
	FName UrnColorParamName = FName(TEXT("UrnTint"));

	// === ЗНАК (положение — см, относительно навеса) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Знак", meta = (DisplayPriority = "1", DisplayName = "Положение знака"))
	FVector SignLocation = FVector(220.0f, 60.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Знак", meta = (DisplayPriority = "2", DisplayName = "Наклон знака (°)",
		ClampMin = "0.0", ClampMax = "89.0",
		ToolTip = "На сколько градусов знак завален от вертикали. 0 — стоит ровно."))
	float SignTiltDeg = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Знак", meta = (DisplayPriority = "3", DisplayName = "Куда завален знак (°)",
		ClampMin = "-180.0", ClampMax = "180.0",
		ToolTip = "В какую сторону завален верх знака: 0 — вдоль остановки (+X), 90 — к открытой стороне навеса (+Y)."))
	float SignTiltDirectionDeg = 0.0f;

	// === УРНА (положение — см, относительно навеса) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Урна", meta = (DisplayPriority = "1", DisplayName = "Положение урны"))
	FVector UrnLocation = FVector(-160.0f, 60.0f, 0.0f);

	// === ЛУТ (по образцу машины, но диапазон денег один) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Лут", meta = (DisplayPriority = "1",
		DisplayName = "Лут есть",
		ToolTip = "Включено — остановку можно обыскать: внутри деньги из диапазона и предметы из списка ниже."))
	bool bHasLoot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Лут", meta = (DisplayPriority = "2",
		TitleProperty = "ItemRow",
		DisplayName = "Какие конкретно предметы", EditCondition = "bHasLoot",
		ToolTip = "Список предметов на остановке: каждая запись — предмет (строкой таблицы предметов DT_Items или классом) и количество. Формат тот же, что у мешка-пикапа."))
	TArray<FPlacedLootEntry> PlacedLootList;

	// Допущение cpp-dev 04.10 — числа стартовые (уровень «дешёвой» машины), крутятся здесь
	// без пересборки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Лут", meta = (DisplayPriority = "3",
		DisplayName = "Деньги (от..до)", EditCondition = "bHasLoot"))
	FInt32Interval MoneyRange = FInt32Interval(5, 20);

	// === КОСТЁР ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Костёр", meta = (DisplayPriority = "1",
		DisplayName = "Костёр напротив остановки"))
	bool bHasCampfire = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Костёр", meta = (DisplayPriority = "2",
		DisplayName = "Костёр — точка сохранения", EditCondition = "bHasCampfire",
		ToolTip = "Включено — костёр сохраняет игру и служит точкой возрождения. Выключено — просто горит."))
	bool bCampfireIsSavePoint = false;

	// Смещение костра относительно остановки (см). По умолчанию 3 м перед открытой стороной.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Костёр", meta = (DisplayPriority = "3",
		DisplayName = "Смещение костра", EditCondition = "bHasCampfire"))
	FVector CampfireOffset = FVector(0.0f, 300.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BusStop|Костёр", meta = (DisplayPriority = "4",
		DisplayName = "Класс костра", EditCondition = "bHasCampfire"))
	TSubclassOf<ACampfire> CampfireClass;

	// === КОМПОНЕНТЫ. БЕЗ VisibleAnywhere намеренно — как у машины (Report1 п.9 а/б): иначе
	// в Details вылезает простыня настроек каждого компонента. ===

	UPROPERTY()
	USceneComponent* SceneRoot;

	UPROPERTY()
	UStaticMeshComponent* Shelter;

	UPROPERTY()
	UStaticMeshComponent* Sign;

	UPROPERTY()
	UStaticMeshComponent* Urn;

	// Точка костра: держит дочерний актор-костёр (класс ставится и снимается галочкой).
	UPROPERTY()
	UChildActorComponent* CampfireChild;

	// Контейнер обыска остановки: «отдельное хранилище», наполняется в BeginPlay. Правда лута
	// живёт в полях «BusStop|Лут» выше.
	UPROPERTY()
	UCorpseLootComponent* StopLoot;

private:
	// Фабрика деталей: неподвижный меш из папки остановки. Пакета ассета ещё нет на диске —
	// деталь остаётся пустой без ошибки в журнале.
	UStaticMeshComponent* CreatePart(const TCHAR* SubobjectName, const TCHAR* MeshPackagePath,
		USceneComponent* Parent, bool bBlocking);

	// Ставит компонент в положение/поворот ТОЛЬКО если они изменились: неподвижный компонент
	// в уже идущей игре двигать нельзя (движок ругается), а повторный прогон конструкции с
	// теми же числами ничего двигать и не должен.
	static void SetRelativeIfChanged(USceneComponent* Component, const FVector& Location, const FQuat& Rotation);

	// Пишет три цвета в материалы детали (в те слоты, у материала которых есть хоть один из
	// трёх параметров; динамический инстанс создаётся при первой надобности).
	void ApplyColors(UStaticMeshComponent* Part);

	// Создаёт/убирает костёр по галочке и ставит точку костра на смещение.
	void ApplyCampfire();

	// Передаёт костру (если он есть) признак точки сохранения.
	void ApplyCampfireSavePointFlag();

	// Динамические инстансы материалов (UPROPERTY — защита от GC).
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ColorMIDs;
};
