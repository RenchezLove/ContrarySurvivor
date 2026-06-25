// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campfire.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Костёр (GDD §7.8): точка СЕЙВА и РЕСПАУНА.
 * Триггер-объём (USphereComponent): при входе игрока -> автосейв (APlayerCharacter::SaveGame),
 * позиция игрока в безопасной зоне становится точкой респауна.
 * Меш — плейсхолдер (визуал назначает unreal-operator в BP).
 *
 * НАСТРОЙКИ В BLUEPRINT (фидбек Рината): все тюнинг-параметры (радиус зоны, параметры огня,
 * антиспам) — EditAnywhere + BlueprintReadWrite с meta DisplayPriority, чтобы менять их прямо
 * на РАЗМЕЩЁННОМ на уровне костре, и чтобы они были наверху Details, а не внизу. OnConstruction
 * применяет их к компонентам (сфера-триггер + свет) сразу при правке в редакторе — фикс бага
 * «SafeZoneRadius у размещённого костра не влияет на отображаемый радиус» (раньше радиус сферы
 * задавался только в конструкторе и не пересчитывался при правке поля).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ACampfire : public AActor
{
	GENERATED_BODY()

public:
	ACampfire();

protected:
	// Подгоняет сферу-триггер под SafeZoneRadius и применяет параметры огня к свету — чтобы
	// правки этих полей на РАЗМЕЩЁННОМ костре (и в превью BP) сразу отражались во вьюпорте.
	virtual void OnConstruction(const FTransform& Transform) override;

	// === НАСТРОЙКИ КОСТРА (подняты наверх Details, фидбек Рината) ===

	// Радиус безопасной зоны (см): вход игрока в эту сферу = автосейв + точка респауна.
	// Меняй прямо на размещённом костре — сфера-триггер обновляется сразу (OnConstruction).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campfire", meta = (ClampMin = "50.0", UIMin = "50.0", DisplayPriority = "1"))
	float SafeZoneRadius = 300.0f;

	// Яркость огня (интенсивность точечного света FireLight, безразмерная как у Point Light).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campfire", meta = (ClampMin = "0.0", UIMin = "0.0", DisplayPriority = "2"))
	float FireLightIntensity = 5000.0f;

	// Цвет огня (тёплый оранжевый по умолчанию).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campfire", meta = (DisplayPriority = "3"))
	FLinearColor FireLightColor = FLinearColor(1.0f, 0.6f, 0.2f, 1.0f);

	// Радиус затухания света огня (см): как далеко добивает свет костра.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campfire", meta = (ClampMin = "0.0", UIMin = "0.0", DisplayPriority = "4"))
	float FireLightAttenuationRadius = 600.0f;

	// Антиспам автосейва: минимальный интервал между автосейвами при входе (сек).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Campfire", meta = (ClampMin = "0.0", UIMin = "0.0", DisplayPriority = "5"))
	float AutoSaveCooldown = 2.0f;

	// === Компоненты (визуал/триггер; имена совпадают с компонентами BP_Campfire для репарента) ===

	// Корень.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campfire")
	USceneComponent* SceneRoot;

	// Меш-плейсхолдер (StaticMesh назначается в BP). Имя сабобъекта "Mesh".
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campfire")
	UStaticMeshComponent* MeshComponent;

	// Триггер безопасной зоны (вход -> автосейв). Имя сабобъекта "SafeZoneTrigger".
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campfire")
	USphereComponent* SafeZoneTrigger;

	// Точечный свет огня. Параметры задаются полями FireLight* выше и применяются в OnConstruction.
	// Имя сабобъекта "FireLight" — совпадает с компонентом в BP_Campfire.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Campfire")
	UPointLightComponent* FireLight;

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
