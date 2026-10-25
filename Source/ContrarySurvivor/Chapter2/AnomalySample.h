// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnomalySample.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class APlayerCharacter;
class UMaterialInterface;

/**
 * Аномальный образец (Ринат 23.10.2026: «В зоне три светящихся образца, подбираются касанием,
 * сдаются Вере»). Подбирается, когда герой касается его (входит в сферу), пока задание
 * «Образцы из аномалий» взято. Виден только пока задание идёт и образец не подобран.
 *
 * Свечение дешёвое для телефона: один точечный свет без теней плюс меш с материалом, который
 * задаётся на чертеже (светящийся материал ставит оператор).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AAnomalySample : public AActor
{
	GENERATED_BODY()

public:
	AAnomalySample();

	// Под каким именем подбор образца пишется в сохранение. Пусто — имя актора на уровне.
	FName GetSampleId() const;

	// Показать/спрятать образец (вместе со светом и касанием).
	void SetSampleVisible(bool bVisible);
	bool IsSampleVisible() const { return bSampleVisible; }

	// Имя и материал свечения — до появления в мире (низина ставит образцы на свои места).
	void InitSample(FName InSampleId, UMaterialInterface* GlowMaterial);

	// Попытка подбора героем (её зовёт касание; публичная — её же зовёт автопроверка).
	bool TryCollect(APlayerCharacter* Player);

	const FText& GetDisplayName() const { return DisplayName; }
	float GetTouchRadius() const { return TouchRadius; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTouchBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Образец")
	TObjectPtr<USphereComponent> TouchSphere;

	// Внешний вид образца: меш и светящийся материал задаются на чертеже.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Образец")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Образец")
	TObjectPtr<UPointLightComponent> Glow;

	// Уникальное имя образца латиницей (Sample1, Sample2, Sample3). Пусто — имя актора.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Образец", meta = (DisplayPriority = "1",
		DisplayName = "Имя образца для сохранения (латиницей)"))
	FName SampleId = NAME_None;

	// [226] Название предмета: образец. Всплывает при подборе.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Образец", meta = (DisplayPriority = "2",
		DisplayName = "Название образца"))
	FText DisplayName = NSLOCTEXT("Chapter2", "SampleName", "Аномальный образец");

	// Радиус касания (см).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Образец", meta = (DisplayPriority = "3", ClampMin = "20.0",
		DisplayName = "Радиус касания (см)"))
	float TouchRadius = 120.0f;

private:
	bool bSampleVisible = true;
};
