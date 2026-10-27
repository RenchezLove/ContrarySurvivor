// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "ToxicAnomalyZone.generated.h"

class UBillboardComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class AAnomalyFormation;

/*
 * Порядок разделов в панели настроек (правка Рината 27.10.2026: «настройки отравленной низины
 * (плотность тумана, цвет, количество образцов и т д) … в панели details они должны идти первыми
 * или одними из первых (после transform)»): у классов ниже метаданные PrioritizeCategories.
 * Как это работает в UE 5.5 (сверено с исходниками движка):
 *  - UHT складывает список в метаданные класса через пробел (EpicGames.UHT, UhtClass.cs:718,
 *    UhtMetaData.cs:463), движок читает его разбиением по пробелу (Class.cpp:6949-6955) —
 *    поэтому в именах разделов нет пробелов, вместо них «_»;
 *  - панель ставит эти разделы первыми среди обычных (ObjectPropertyNode.cpp:582-589), а
 *    «Transform» стоит выше всех обычных всегда (ECategoryPriority::Transform,
 *    ActorDetails.cpp:483, SceneComponentDetails.cpp:170) — значит, наши разделы сразу под ним;
 *  - чертёж наследует список от класса при сборке (BlueprintEditorUtils.cpp:1400-1402);
 *  - «_» в имени раздела панель показывает пробелом (FName::NameToDisplayString через
 *    FObjectEditorUtils::GetCategoryText, ObjectEditorUtils.cpp:33).
 */

/**
 * Место аномального образования внутри зоны отравленного воздуха (правка Рината 25.10.2026
 * вечер: «отравленную зону … отдельным BP, что бы я мог подвигать её по карте»). Точка-компонент
 * чертежа зоны: двигаешь зону — едет и она. При старте игры на ней появляется аномальное
 * образование с этим именем. До 27.10.2026 — UAnomalySampleSpotComponent ([CoreRedirects]).
 */
UCLASS(ClassGroup = (Custom), PrioritizeCategories = ("Аномальное_образование"),
	meta = (BlueprintSpawnableComponent, DisplayName = "Место аномального образования"))
class CONTRARYSURVIVOR_API UAnomalyFormationSpotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Под этим именем подбор пишется в сохранение (Sample1, Sample2, Sample3 — как у образцов,
	// стоявших на уровне раньше: старые сохранения читаются как прежде). Пусто — «Sample» и номер
	// места по порядку.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Аномальное_образование",
		meta = (DisplayName = "Имя для сохранения (латиницей)"))
	FName FormationId = NAME_None;
};

/**
 * Область тумана зоны (правка Рината 26.10.2026: «Сделай проще: одна область тумана, которую
 * можно двигать и настраивать… кнопка, которая добавляет дополнительную область тумана… в т.ч.
 * настраивать её объём, плотность и цвет (но по умолчанию — цвет такой, какой сейчас)»).
 *
 * Коробка (в редакторе видна зелёной рамкой; выдели её в списке компонентов зоны — и двигай
 * мышью). Внутри коробки код сам кладёт несколько горизонтальных полупрозрачных слоёв тумана
 * с материалом M_PoisonFog: их размер — по коробке, прозрачность — по плотности, цвет — свой.
 *
 * Почему слои, а не движковый Local Fog Volume: тот в UE 5.5 рисуется и на телефоне
 * (LocalFogVolumeRendering.cpp, RenderLocalFogVolumeMobile), но отдельным проходом по экрану и не
 * ближе r.LocalFogVolume.GlobalStartDistance (20 м по умолчанию, там же) — при камере сверху
 * туман у героя пропадал бы. Слои — это несколько полупрозрачных плоскостей без света: самый
 * дешёвый способ для слабых Android (по слою на пиксель, где туман виден).
 */
UCLASS(ClassGroup = (Custom), PrioritizeCategories = ("Туман"),
	hidecategories = (Shape, Collision, Physics, Navigation, HLOD, Rendering, Mobile, RayTracing, TextureStreaming, AssetUserData, Cooking, Tags, LOD, Lighting, Activation, Sockets),
	meta = (BlueprintSpawnableComponent, DisplayName = "Область тумана"))
class CONTRARYSURVIVOR_API UPoisonFogAreaComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UPoisonFogAreaComponent();

	// Размер области: половина длины, ширины и высоты (см). Туман занимает всю коробку.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "1",
		DisplayName = "Размер области (половина длины, ширины, высоты; см)"))
	FVector AreaHalfSize = FVector(1800.0f, 1800.0f, 120.0f);

	// 0 — тумана нет, 1 — сквозь середину ничего не видно.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "2", ClampMin = "0.0", ClampMax = "1.0",
		DisplayName = "Плотность тумана"))
	float Density = 0.45f;

	// По умолчанию — нынешний зеленоватый цвет тумана (FogColor материала M_PoisonFog).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "3",
		DisplayName = "Цвет тумана"))
	FLinearColor FogColor = FLinearColor(0.5f, 0.7f, 0.25f, 1.0f);

	// Больше слоёв — туман объёмнее по высоте, но дороже для телефона.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "4", ClampMin = "1", ClampMax = "6",
		DisplayName = "Слоёв тумана (1–6)"))
	int32 LayerCount = 3;

	// Как быстро туман редеет к краю области: больше — плотная середина и мягкий край.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "5", ClampMin = "0.2", ClampMax = "6.0",
		DisplayName = "Мягкость края"))
	float EdgeSoftness = 1.6f;

	// Насколько мягко туман сходит на нет у земли и предметов (см).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "6", ClampMin = "0.0",
		DisplayName = "Мягкость у земли (см)"))
	float GroundSoftness = 120.0f;

	// Верхние слои чуть меньше нижних (туман «холмом», а не стопкой одинаковых блинов).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "7", ClampMin = "0.3", ClampMax = "1.0",
		DisplayName = "Верхний слой от нижнего по размеру (доля)"))
	float TopLayerScale = 0.75f;

	// Пересобрать слои тумана по нынешним настройкам (их зовёт и сама область при появлении).
	void RebuildLayers();

	const TArray<TObjectPtr<UStaticMeshComponent>>& GetLayers() const { return Layers; }

	// Прозрачность одного слоя, чтобы сквозь все слои в середине вышла заданная плотность.
	static float LayerOpacity(float InDensity, int32 InLayerCount);

protected:
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Layers;
};

/**
 * Зона отравленного воздуха главы 2 (Ринат 23.10.2026: «На карте отравленная (химически) зона
 * рядом с базой учёных, зеленоватый туман»; 27.10.2026: «Это по сути область с отравленным
 * воздухом, в которой спавнятся аномальные образования»). До 27.10.2026 — APoisonZone, чертёж
 * BP_PoisonZone ([CoreRedirects] в Config/DefaultEngine.ini переносят на этот класс и чертёж, и
 * экземпляр на уровне со всеми настройками).
 *
 * Объём-коробка: внутри без дыхательного аппарата (или с пустым баллоном) здоровье тает каждую
 * секунду; у края появляется кнопка «Надеть дыхательный аппарат».
 *
 * Зона — ОДИН объект вместе с туманом и местами аномальных образований. Настройки экземпляра на
 * уровне (правка 26.10):
 *  - туман — «области тумана» (по умолчанию одна, FogArea1); кнопки «Добавить область тумана» и
 *    «Удалить последнюю область тумана»;
 *  - сколько аномальных образований в зоне (по умолчанию 3) — цель задания [216] берёт число
 *    отсюда. Места — компоненты «Место аномального образования» (SampleSpot1–3, можно добавить
 *    свои); мест меньше, чем образований, — недостающие встают в случайные точки зоны (одни и те
 *    же при каждом запуске, пока не сменено число-зерно);
 *  - свой урон в секунду и своя дальность детектора (иначе — из настроек проекта).
 * Размер самой зоны (где идёт урон) — коробка «Граница зоны» (Volume, поле Box Extent).
 *
 * Метка AnomalyLowland добавляется сама: на неё ведёт метка задания «Образцы из аномалий».
 */
UCLASS(Blueprintable, PrioritizeCategories = ("Туман", "Аномальные_образования", "Урон", "Детектор", "Зона"),
	meta = (DisplayName = "Зона отравленного воздуха"))
class CONTRARYSURVIVOR_API AToxicAnomalyZone : public AActor
{
	GENERATED_BODY()

public:
	AToxicAnomalyZone();

	// Внутри ли точка (с учётом поворота и масштаба коробки).
	bool IsInside(const FVector& WorldPoint) const;

	// Расстояние от точки до коробки (0 — внутри).
	float DistanceTo(const FVector& WorldPoint) const;

	UBoxComponent* GetVolume() const { return Volume; }

	// Поставить аномальные образования на места зоны (зовёт BeginPlay; публичная — её зовёт
	// автопроверка). Образование с тем же именем уже есть на уровне — место пропускается.
	// Возвращает, сколько образований поставлено.
	int32 SpawnFormationsAtSpots();

	// Имя образования для места (поле места, иначе «Sample» и номер по порядку с единицы:
	// под этими именами подбор записан в сохранениях).
	static FName ResolveSpotFormationId(const UAnomalyFormationSpotComponent* Spot, int32 Index);

	// Где встанут образования зоны (до опускания на землю): сначала места-компоненты по порядку
	// имён, недостающие — случайные точки в коробке зоны по числу-зерну. Ровно GetFormationCount()
	// штук. Публичная — её зовёт автопроверка.
	void ComputeFormationPlaces(TArray<FVector>& OutLocations, TArray<FName>& OutIds) const;

	const TArray<TWeakObjectPtr<AAnomalyFormation>>& GetSpawnedFormations() const { return SpawnedFormations; }

	int32 GetFormationCount() const { return FMath::Max(1, FormationCount); }

	// Свои числа зоны; меньше нуля — не заданы (берутся из настроек проекта).
	float GetDamageNoMaskOverride() const { return bOverrideDamage ? DamageNoMaskPerSecond : -1.0f; }
	float GetDamageNoAirOverride() const { return bOverrideDamage ? DamageNoAirPerSecond : -1.0f; }
	float GetDetectorRangeOverride() const { return bOverrideDetectorRange ? DetectorRange : -1.0f; }

	// Области тумана зоны (все компоненты «Область тумана»).
	TArray<UPoisonFogAreaComponent*> GetFogAreas() const;

	// Кнопки в панели настроек зоны на уровне. Добавленные кнопкой области можно двигать и
	// настраивать как первую; удаляется последняя добавленная (первая, FogArea1, — нет).
	UFUNCTION(CallInEditor, Category = "Туман", meta = (DisplayName = "Добавить область тумана"))
	void AddFogArea();

	UFUNCTION(CallInEditor, Category = "Туман", meta = (DisplayName = "Удалить последнюю область тумана"))
	void RemoveLastFogArea();

	// Материал слоёв тумана (M_PoisonFog: FogColor, MaxOpacity, EdgeFalloff, DepthFadeDistance).
	UMaterialInterface* GetFogMaterial() const { return FogMaterial; }
	UStaticMesh* GetFogLayerMesh() const { return FogLayerMesh; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	// --- Туман ---
	// Плотность, цвет и размер тумана — у самой области тумана: выдели «Область тумана 1» в
	// списке компонентов зоны.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "10", DisplayName = "Материал тумана"))
	TObjectPtr<UMaterialInterface> FogMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Туман", meta = (DisplayPriority = "11", DisplayName = "Плоскость слоя тумана"))
	TObjectPtr<UStaticMesh> FogLayerMesh;

	// --- Аномальные образования ---

	// Сколько аномальных образований в зоне; цель задания [216] «({Done} из {Total})» берёт
	// число отсюда.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Аномальные_образования", meta = (DisplayPriority = "1", ClampMin = "1", ClampMax = "20",
		DisplayName = "Сколько аномальных образований в зоне"))
	int32 FormationCount = 3;

	// Мест-компонентов меньше, чем образований, — недостающие встают в случайные точки зоны.
	// Точки одни и те же при каждом запуске; другое число — другие точки.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Аномальные_образования", meta = (DisplayPriority = "2",
		DisplayName = "Случайные места: число-зерно (смени — образования переставятся)"))
	int32 RandomPlacesSeed = 2610;

	// Ближе этого (см) случайные места друг к другу и к другим образованиям не встают (если влезают).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Аномальные_образования", meta = (DisplayPriority = "3", ClampMin = "0.0",
		DisplayName = "Случайные места: не ближе друг к другу (см)"))
	float RandomPlacesMinDistance = 500.0f;

	// Какое аномальное образование ставить на места (его чертёж или сам класс).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Аномальные_образования", meta = (DisplayPriority = "4",
		DisplayName = "Чертёж аномального образования"))
	TSubclassOf<AAnomalyFormation> FormationClass;

	// Светящийся материал образования (M_AnomalyGlow). Пусто — материал чертежа образования.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Аномальные_образования", meta = (DisplayPriority = "5",
		DisplayName = "Материал свечения образования"))
	TObjectPtr<UMaterialInterface> FormationGlowMaterial;

	// Ставить ли образование на землю под местом (луч вниз), а не ровно в точку места.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Аномальные_образования", meta = (DisplayPriority = "6",
		DisplayName = "Ставить образования на землю"))
	bool bSnapFormationsToGround = true;

	// --- Урон ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Урон", meta = (InlineEditConditionToggle))
	bool bOverrideDamage = false;

	// Включить галочкой — у этой зоны свой урон; иначе — из настроек проекта «глава 2».
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Урон", meta = (DisplayPriority = "1", ClampMin = "0.0", EditCondition = "bOverrideDamage",
		DisplayName = "Свой урон без аппарата (здоровья в секунду)"))
	float DamageNoMaskPerSecond = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Урон", meta = (DisplayPriority = "2", ClampMin = "0.0", EditCondition = "bOverrideDamage",
		DisplayName = "Свой урон, когда воздух кончился (здоровья в секунду)"))
	float DamageNoAirPerSecond = 8.0f;

	// --- Детектор ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Детектор", meta = (InlineEditConditionToggle))
	bool bOverrideDetectorRange = false;

	// Включить галочкой — у этой зоны своя дальность детектора; иначе — из настроек проекта.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Детектор", meta = (DisplayPriority = "1", ClampMin = "100.0", EditCondition = "bOverrideDetectorRange",
		DisplayName = "Своя дальность детектора (см)"))
	float DetectorRange = 3000.0f;

	// --- Части зоны (компоненты; двигаются мышью в списке компонентов) ---

	// Границы зоны: размер задаётся полем «Box Extent» коробки (половина стороны, см).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Зона", meta = (DisplayName = "Граница зоны"))
	TObjectPtr<UBoxComponent> Volume;

	// Места трёх образований (свои места можно добавить компонентом «Место аномального
	// образования»). Имена самих компонентов прежние (SampleSpot1–3): на них держатся
	// перемещения мест на уровне.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Зона", meta = (DisplayName = "Место образования 1"))
	TObjectPtr<UAnomalyFormationSpotComponent> FormationSpot1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Зона", meta = (DisplayName = "Место образования 2"))
	TObjectPtr<UAnomalyFormationSpotComponent> FormationSpot2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Зона", meta = (DisplayName = "Место образования 3"))
	TObjectPtr<UAnomalyFormationSpotComponent> FormationSpot3;

	// Первая область тумана (есть всегда; остальные добавляет кнопка).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Зона", meta = (DisplayName = "Область тумана 1"))
	TObjectPtr<UPoisonFogAreaComponent> FogArea1;

private:
	TArray<TWeakObjectPtr<AAnomalyFormation>> SpawnedFormations;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<TObjectPtr<UBillboardComponent>> SpotSprites;
#endif
};

/**
 * Где баллон заправляется (решение лида 23.10: «баллон заправляется при входе на базу учёных»).
 * Коробка накрывает базу учёных. Нет ни одной на уровне — баллон заправляет Вера при разговоре.
 * Она же — территория базы для наёмника: туда он за героем не идёт.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AAirRefillZone : public AActor
{
	GENERATED_BODY()

public:
	AAirRefillZone();

	bool IsInside(const FVector& WorldPoint) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Заправка")
	TObjectPtr<UBoxComponent> Volume;
};

namespace Chapter2Volumes
{
	// Внутри ли точка коробки (общая проверка обеих зон).
	bool IsInsideBox(const UBoxComponent* Box, const FVector& WorldPoint);

	// Расстояние до коробки в мировых сантиметрах (0 — внутри).
	float DistanceToBox(const UBoxComponent* Box, const FVector& WorldPoint);
}
