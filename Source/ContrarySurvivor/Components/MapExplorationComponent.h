// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ContrarySurvivor/UI/PdaMapFog.h"
#include "MapExplorationComponent.generated.h"

class UContrarySaveGame;

/**
 * Память героя о том, какую часть карты он уже видел (слова Рината 10.10.2026: «изначально на
 * карте ничего не должно быть видно, кроме деревни… карта появляется в каком-то радиусе от него»).
 *
 * Герой раз в короткий промежуток открывает круг вокруг себя; открытое обратно не закрывается.
 * С самого начала открыта деревня. Сама сетка и её правила — FPdaMapFog; размеры, радиусы и
 * период — UContraryPdaSettings (Project Settings → «Contrary Survivor: КПК и карта»).
 *
 * Окно КПК состояние только читает: по счётчику изменений (GetRevision) оно понимает, что
 * черноту и подписи пора перерисовать, и не делает этого каждый кадр.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UMapExplorationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMapExplorationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Состояние начала игры: открыта только деревня.
	void ResetToVillage();

	// Открыть круг вокруг точки мира. true — открылось что-то новое.
	bool RevealAroundWorld(const FVector2D& WorldLocation, float RadiusWorld);

	// Открыть круг вокруг героя радиусом из настроек (то же делает сам компонент по таймеру).
	bool RevealAroundOwner();

	// Меню отладки: открыть всю карту.
	void RevealAll();

	// Открыта ли клетка карты под точкой мира.
	bool IsWorldLocationRevealed(const FVector2D& WorldLocation) const;

	// Центр деревни: зона деревни на уровне, а без неё — точка из настроек.
	FVector2D FindVillageCenter() const;

	const FPdaMapFog& GetFog() const { return Fog; }

	// Растёт при каждом изменении открытого (и при сбросе, и при загрузке).
	int32 GetRevision() const { return Revision; }

	// Сохранение. Запись без этих данных (сделана раньше) читается как «деревня и место, где
	// герой стоит сейчас».
	void WriteToSave(UContrarySaveGame* Save) const;
	void RestoreFromSave(const UContrarySaveGame* Save);

private:
	// Сетка нужного размера есть? Нет — заводим и открываем деревню.
	void EnsureFog();

	FPdaMapFog Fog;
	int32 Revision = 0;
};
