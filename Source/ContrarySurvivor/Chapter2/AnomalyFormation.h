// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnomalyFormation.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class APlayerCharacter;
class UMaterialInterface;

/**
 * Аномальное образование в зоне отравленного воздуха (Ринат 23.10.2026: «В зоне три светящихся
 * образца, подбираются касанием, сдаются Вере»; 27.10.2026: «Образцы тоже переименуй в
 * аномальные образования. Сами аномальные образования сделай предметом. Этот предмет попадает в
 * инвентарь, когда игрок берёт его»). Подбирается, когда герой касается его (входит в сферу),
 * пока задание «Образцы из аномалий» взято; подобранное лежит в сумке предметом
 * AAnomalyFormationItem. Видно только пока задание идёт и образование не подобрано.
 *
 * До 27.10.2026 класс назывался AAnomalySample (Config/DefaultEngine.ini, [CoreRedirects]).
 * Имя для сохранения (Sample1, Sample2…) прежнее: старые сохранения читаются как раньше.
 *
 * Свечение дешёвое для телефона: один точечный свет без теней плюс меш со светящимся материалом.
 */
UCLASS(Blueprintable, PrioritizeCategories = ("Аномальное_образование"), meta = (DisplayName = "Аномальное образование"))
class CONTRARYSURVIVOR_API AAnomalyFormation : public AActor
{
	GENERATED_BODY()

public:
	AAnomalyFormation();

	// Под каким именем подбор пишется в сохранение. Пусто — имя актора на уровне.
	FName GetFormationId() const;

	// Показать/спрятать образование (вместе со светом и касанием).
	void SetFormationVisible(bool bVisible);
	bool IsFormationVisible() const { return bFormationVisible; }

	// Имя и материал свечения — до появления в мире (зона ставит образования на свои места).
	void InitFormation(FName InFormationId, UMaterialInterface* GlowMaterial);

	// Попытка подбора героем (её зовёт касание; публичная — её же зовёт автопроверка).
	bool TryCollect(APlayerCharacter* Player);

	float GetTouchRadius() const { return TouchRadius; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTouchBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Аномальное_образование", meta = (DisplayName = "Сфера касания"))
	TObjectPtr<USphereComponent> TouchSphere;

	// Внешний вид: меш и светящийся материал задаются на чертеже.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Аномальное_образование", meta = (DisplayName = "Вид"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Аномальное_образование", meta = (DisplayName = "Свечение"))
	TObjectPtr<UPointLightComponent> Glow;

	// Уникальное имя латиницей (Sample1, Sample2, Sample3 — так они записаны в сохранениях).
	// Пусто — имя актора.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Аномальное_образование", meta = (DisplayPriority = "1",
		DisplayName = "Имя для сохранения (латиницей)"))
	FName FormationId = NAME_None;

	// Радиус касания (см).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Аномальное_образование", meta = (DisplayPriority = "2", ClampMin = "20.0",
		DisplayName = "Радиус касания (см)"))
	float TouchRadius = 120.0f;

private:
	bool bFormationVisible = true;
};
