// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h" // мягкая ссылка на материал подсветки
#include "Components/DecalComponent.h"
#include "MeleeSectorIndicatorComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Подсветка сектора ближнего боя на земле перед игроком (Build 1.1, п.5 плана Рината:
 * «Во время боя (когда есть цель и если у игрока выбрано холодное оружие) — необходимо
 * подсвечивать сектор в 100 градусов перед игроком. В этом же секторе должен наноситься
 * урон по противнику в случае если игрок бъет холодным оружием»).
 *
 * ГЛАВНОЕ ПРАВИЛО — «что видишь, то и бьёшь». Угол и дальность подсветки берутся ИЗ САМОГО
 * ножа (AMeleeWeapon::GetMeleeSectorHalfAngleDeg / GetMeleeRange) и переводятся в радиус
 * на земле ТОЙ ЖЕ формулой, по которой считается урон в AMeleeWeapon::Fire: удар проходит,
 * если расстояние между центрами <= радиус капсулы игрока + MeleeRange + радиус капсулы цели.
 * Отдельных «визуальных» чисел угла и дальности у подсветки НЕТ — разойтись с уроном нечему.
 *
 * Показывается ровно при трёх условиях сразу: есть захваченная цель
 * (AContrarySurvivorPlayerController::GetCurrentTarget), в руках холодное оружие
 * (GetCurrentWeapon приводится к AMeleeWeapon) и назначен материал. Иначе — гаснет.
 *
 * ПРОЕКЦИЯ И КОНТРАКТ МАТЕРИАЛА. Декаль привязана к капсуле игрока и повёрнута так, что её
 * локальная ось X смотрит вниз (проекция на землю). Тогда, по шейдеру движка
 * (UE 5.5, Engine/Shaders/Private/DeferredDecal.usf:161-167 — DecalUVs = (DecalVector.z,
 * DecalVector.y), рамка нормируется на DecalSize через
 * UDecalComponent::GetTransformIncludingDecalSize, DecalComponent.h:206):
 *   ось U материала = локальная Z компонента = НАПРАВЛЕНИЕ ВЗГЛЯДА игрока;
 *   ось V материала = локальная Y компонента = «вправо» от игрока;
 *   центр декали (U=0.5, V=0.5) = позиция игрока, край (0 и 1) = радиус удара.
 * Материал делает unreal-operator (/Game/Materials/MI_MeleeSectorSoft), скалярные параметры:
 *   SectorHalfAngleRad — ПОЛУугол сектора в радианах; сектор рисуется там, где
 *                        |atan2(V - 0.5, U - 0.5)| <= SectorHalfAngleRad;
 *   InnerRadiusFrac    — доля радиуса, внутри которой не рисуем (тело игрока), 0..1;
 *   Opacity            — общая непрозрачность подсветки.
 * Поле материала пустое или ассет не найден — подсветки просто нет, без крашей и без спама
 * в лог (одно предупреждение).
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UMeleeSectorIndicatorComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	UMeleeSectorIndicatorComponent();

	// --- Настройки (директива Рината 06-25: тюнинг-параметры EditAnywhere + наверх Details) ---

	// Общий выключатель подсветки сектора.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Sector", meta = (DisplayPriority = "1"))
	bool bShowMeleeSector = true;

	// Материал подсветки (домен Deferred Decal). Пусто — подсветки нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Sector", meta = (DisplayPriority = "2"))
	TSoftObjectPtr<UMaterialInterface> SectorMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/MI_MeleeSectorSoft.MI_MeleeSectorSoft")));

	// Непрозрачность подсветки (уходит в параметр материала Opacity).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Sector", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "3"))
	float SectorOpacity = 0.35f;

	// Дырка под ногами считается сама: враг физически не может подойти центром ближе, чем
	// сумма радиусов капсул (капсулы не проходят друг сквозь друга), значит эта область
	// заведомо пустая и её честно не подсвечивать. Выключить — задать долю руками ниже.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Sector", meta = (DisplayPriority = "4"))
	bool bAutoInnerRadius = true;

	// Доля радиуса, внутри которой подсветка не рисуется. Действует при bAutoInnerRadius = false.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Sector", meta = (ClampMin = "0.0", ClampMax = "0.95", EditCondition = "!bAutoInnerRadius", DisplayPriority = "5"))
	float InnerRadiusFrac = 0.35f;

	// Глубина проекции декали вверх/вниз от центра капсулы, см. Должна с запасом доставать
	// до земли под игроком (центр капсулы примерно в 88 см над полом).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Sector", meta = (ClampMin = "10.0", DisplayPriority = "6"))
	float ProjectionDepth = 200.0f;

	// Поворот сектора вокруг вертикали, градусы. Штатное значение — 0 (сектор смотрит туда же,
	// куда игрок). Оставлено на случай, если у готового материала нулевой угол окажется
	// развёрнут: правится в редакторе, без пересборки кода.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Sector", meta = (DisplayPriority = "7"))
	float SectorYawOffsetDeg = 0.0f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Гасит подсветку (без лишних вызовов, если она уже погашена).
	void HideSector();

	// Лениво грузит материал и делает из него динамический инстанс (в нём меняем параметры).
	// null — материала нет; повторных попыток загрузки не делает.
	UMaterialInstanceDynamic* GetOrCreateSectorMID();

	// Динамический инстанс материала подсветки (создаётся при первом показе).
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SectorMID;

	// Материал уже пробовали загрузить и не нашли — больше не пытаемся и не спамим в лог.
	bool bSectorMaterialMissing = false;

	// Последние отданные в материал значения — чтобы не дёргать параметры каждый кадр.
	// Стартовые значения заведомо недостижимые: первый кадр показа применяет всё.
	float AppliedHalfAngleRad = -1.0f;
	float AppliedInnerFrac = -1.0f;
	float AppliedOpacity = -1.0f;
	float AppliedYawOffsetDeg = -1000.0f;
};
