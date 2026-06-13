// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableNPCInterface.h" // HUD-маркер находимости
#include "ContrarySurvivor/Components/QuestComponent.h" // FQuest (предлагаемый квест)
#include "ElderNPC.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 * Староста деревни (Фаза 5, GDD §7.7 — квестодатель MVP).
 *
 * По образцу ATraderNPC: обычный AActor (НЕ Pawn), без UStatsComponent — поэтому не
 * попадает в авто-лок/хелсбары игрока (двойная защита: не Pawn + нет Stats). Меш без
 * коллизии. Реализует IInteractableNPCInterface -> над ним HUD рисует маркер находимости
 * (метка "Elder"). Спавнится кодом (UElderSpawnSubsystem) на навмеше в деревне.
 *
 * Отличие от торговца: ЯРКО-ГОЛУБОЙ плейсхолдер (торговец — маджента), и взаимодействие
 * (E) открывает ДИАЛОГ (а не магазин). Диалог предлагает Kill-квест «убить 5 волков».
 *
 * Регистрация: при входе игрока в радиус (overlap) староста регистрируется у
 * AContrarySurvivorPlayerController (NearbyElder). Клавиша Interact открывает диалог.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AElderNPC : public AActor, public IInteractableNPCInterface
{
	GENERATED_BODY()

public:
	AElderNPC();

	// Квест, который предлагает староста (для диалога/журнала). Заполняется в конструкторе.
	const FQuest& GetOfferedQuest() const { return OfferedQuest; }

	// --- IInteractableNPCInterface (HUD-маркер находимости) ---
	virtual FString GetNPCMarkerLabel() const override { return TEXT("Elder"); }
	virtual float GetNPCMarkerZOffset() const override { return 320.0f; }

protected:
	virtual void BeginPlay() override;

	// Триггер диалоговой зоны: overlap по Pawn (игроку).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elder")
	USphereComponent* InteractTrigger;

	// Плейсхолдер-меш тела (без коллизии). Реальный меш старосты — позже (modeler/operator).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elder")
	UStaticMeshComponent* MeshComponent;

	// Яркий «маяк»-шар над телом — заметная макушка, чтобы старосту было видно издалека.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elder")
	UStaticMeshComponent* BeaconComponent;

	// Базовый материал плейсхолдера (BasicShapeMaterial: вектор-параметр "Color").
	UPROPERTY()
	UMaterialInterface* PlaceholderBaseMaterial = nullptr;

	// Яркий цвет плейсхолдера: ярко-голубой (ОТЛИЧЕН от торговца — мадженты), решение Рината.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elder")
	FLinearColor PlaceholderColor = FLinearColor(0.1f, 0.7f, 1.0f, 1.0f); // ярко-голубой

	// Радиус, в котором доступно взаимодействие (см). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elder")
	float InteractRadius = 220.0f;

	// Предлагаемый квест (DRAFT: убить 5 волков, награда 150). Тюнингуется в редакторе.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FQuest OfferedQuest;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
