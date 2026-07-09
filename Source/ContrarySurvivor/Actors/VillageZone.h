// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VillageZone.generated.h"

class UBoxComponent;
class UNavModifierComponent;
class USceneComponent;

/**
 * Зона деревни (Этап D, ADR-036 «деревня = безопасная зона»).
 *
 * ЕДИНЫЙ источник границы деревни (на карте NavModifierVolume отсутствовал — проверено
 * game-lead по списку акторов уровня, поэтому граница задаётся этим размещаемым актором):
 *   1) НАВИГАЦИЯ: UNavModifierComponent помечает объём зоны областью UNavArea_Village →
 *      существующий фильтр врагов UNavQueryFilter_ExcludeVillage начинает реально исключать
 *      деревню из поиска пути (до появления этой зоны фильтру нечего было исключать).
 *   2) РАНТАЙМ-БАРЬЕР ИИ: статические запросы IsPointInVillage/IsNearVillage — их зовёт
 *      AEnemyAIController в ОБОИХ режимах погони (навмеш И прямой fallback), чтобы враг
 *      замедлился у границы, постоял и ушёл (ADR-036), даже когда навмеш не задействован.
 *
 * ВАЖНО (сверено с исходником UE 5.5, NavModifierComponent.cpp:138-189): у актора НЕТ
 * коллизионных компонентов, влияющих на навигацию, поэтому границы нав-модификатора берутся
 * из FailsafeExtent вокруг позиции актора (поворот актора учитывается). OnConstruction
 * синхронизирует ZoneBox (видимая рамка в редакторе) И FailsafeExtent от одного параметра
 * ZoneExtent — граница в редакторе и граница навигации всегда совпадают.
 *
 * Размещение и подбор размеров — game-lead (BP_VillageZone). Дефолтные полуразмеры — по
 * фактическим габаритам деревни L_World_C (~3800×2800×600 см с запасом).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AVillageZone : public AActor
{
	GENERATED_BODY()

public:
	AVillageZone();

	// Точка внутри ЛЮБОЙ размещённой зоны деревни (+Margin см наружу от границы)?
	// Margin > 0 расширяет проверку — «рядом с деревней» для замедления у границы.
	// Работает без коллизии: чистая математика по трансформу бокса. Реестр зон ведут
	// BeginPlay/EndPlay; World фильтрует зоны чужих миров (PIE-безопасность).
	static bool IsPointInVillage(const UWorld* World, const FVector& Point, float Margin = 0.0f);

	// Проверка одной зоны (локальное пространство бокса; Margin — в мировых см,
	// при неравномерном масштабе актора приближение по осям).
	bool ContainsPoint(const FVector& Point, float Margin = 0.0f) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Полуразмеры зоны (см). ЕДИНСТВЕННЫЙ источник размера: OnConstruction применяет его
	// и к видимому боксу, и к FailsafeExtent нав-модификатора.
	// meta DisplayPriority — наверх Details (директива Рината 06-25), редактируется на экземпляре.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VillageZone", meta = (DisplayPriority = "1"))
	FVector ZoneExtent = FVector(1900.0f, 1400.0f, 300.0f);

	// Корень-трансформ (placeable).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VillageZone")
	USceneComponent* SceneRoot;

	// Видимая рамка границы деревни во вьюпорте редактора (линии бокса; скрыта в игре —
	// bHiddenInGame у UShapeComponent = true по умолчанию). Коллизии/навигации НЕ несёт.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VillageZone")
	UBoxComponent* ZoneBox;

	// Нав-модификатор: помечает объём (FailsafeExtent вокруг актора) областью UNavArea_Village.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VillageZone")
	UNavModifierComponent* NavModifier;

private:
	// Реестр живых зон (weak — сам чистится при уничтожении актора).
	static TArray<TWeakObjectPtr<AVillageZone>> ActiveZones;
};
