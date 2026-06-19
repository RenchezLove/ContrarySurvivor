// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableNPCInterface.h" // HUD-маркер находимости
#include "ContrarySurvivor/Components/QuestComponent.h" // FQuest (предлагаемый квест)
#include "ElderNPC.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInterface;
class UQuestComponent;

/**
 * Староста деревни (Фаза 5, GDD §7.7 — квестодатель MVP).
 *
 * По образцу ATraderNPC: обычный AActor (НЕ Pawn), без UStatsComponent — поэтому не
 * попадает в авто-лок/хелсбары игрока (двойная защита: не Pawn + нет Stats). Меш без
 * коллизии. Реализует IInteractableNPCInterface -> над ним HUD рисует маркер находимости
 * (метка "Elder"). Ставится актёром на уровень (в деревне) в редакторе.
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

	// Первый квест старосты (DRAFT: «Шкуры волков»). Заполняется в конструкторе.
	const FQuest& GetOfferedQuest() const { return OfferedQuest; }

	// Квест, актуальный для игрока СЕЙЧАС (выдача по порядку, Фаза 5):
	//  - пока кв.1 не сдан (TurnedIn) — возвращает кв.1;
	//  - после сдачи кв.1 — возвращает кв.2 («Зачистить базу бандитов»).
	// Возвращается ссылка на член (валидна, пока жив актор). PlayerQuests может быть null
	// (тогда возвращается кв.1).
	const FQuest& GetQuestForPlayer(const UQuestComponent* PlayerQuests) const;

	// --- IInteractableNPCInterface (HUD-маркер находимости) ---
	virtual FString GetNPCMarkerLabel() const override { return TEXT("Elder"); }
	virtual float GetNPCMarkerZOffset() const override { return 320.0f; }

protected:
	virtual void BeginPlay() override;

	// Триггер диалоговой зоны: overlap по Pawn (игроку).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elder")
	USphereComponent* InteractTrigger;

	// Визуал старосты: скелет-меш SK_Elder на общем гуманоидном скелете (Head_Skeleton)
	// + AnimBP ABP_HumanoidCharacter (idle/walk/run). Заменил кислотный плейсхолдер.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elder")
	USkeletalMeshComponent* CharMesh;

	// Плейсхолдер-меш тела (без коллизии). СКРЫТ (визуал-пасс): заменён на CharMesh.
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

	// [ТЕСТ] временное поле — будет удалено
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elder")
	float TestPingValue = 1.0f;

	// Квест 1 (DRAFT: «Шкуры волков» — собрать 5 шкур, награда 150). Тюнингуется в редакторе.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FQuest OfferedQuest;

	// Квест 2 (DRAFT: «Зачистить базу бандитов» — убить 3 бандитов + принести Ноутбук, награда 250).
	// Выдаётся ПОСЛЕ сдачи кв.1 (GetQuestForPlayer). Тюнингуется в редакторе.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FQuest SecondQuest;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
