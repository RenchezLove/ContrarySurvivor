// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campfire.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * Костёр (GDD §7.8): точка СЕЙВА и РЕСПАУНА.
 * Триггер-объём (USphereComponent): при входе игрока -> автосейв (APlayerCharacter::SaveGame),
 * позиция игрока в безопасной зоне становится точкой респауна.
 * Меш — плейсхолдер (визуал назначает unreal-operator в BP).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ACampfire : public AActor
{
	GENERATED_BODY()

public:
	ACampfire();

protected:
	// Корень.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campfire")
	USceneComponent* SceneRoot;

	// Меш-плейсхолдер (StaticMesh назначается в BP).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campfire")
	UStaticMeshComponent* MeshComponent;

	// Триггер безопасной зоны (вход -> автосейв).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campfire")
	USphereComponent* SafeZoneTrigger;

	// Радиус безопасной зоны (см).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campfire")
	float SafeZoneRadius = 300.0f;

	// Антиспам автосейва: минимальный интервал между автосейвами при входе (сек).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campfire")
	float AutoSaveCooldown = 2.0f;

	UFUNCTION()
	void OnSafeZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	// Ручной сейв через костёр (задел под вызов из UI/кнопки). Сохраняет переданного игрока.
	UFUNCTION(BlueprintCallable, Category = "Campfire")
	bool SaveAtCampfire(class APlayerCharacter* Player);

private:
	float LastAutoSaveTime = -1000.0f;
};
