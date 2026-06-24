// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // база: модульный гуманоид (как AMasterTrader)
#include "InteractableNPCInterface.h"                            // HUD-маркер находимости
#include "ContrarySurvivor/Components/QuestComponent.h"          // FQuest (предлагаемый квест)
#include "ElderNPC.generated.h"

class USphereComponent;
class UQuestComponent;

/**
 * Староста деревни (Фаза 5, GDD §7.7 — квестодатель MVP).
 *
 * A3: переведён с болванки AActor на AMasterHumanoidCharacter (модульный гуманоид, как
 * AMasterTrader). Визуал (Head/Torso/Legs + AnimBP) назначается в BP_Elder оператором —
 * C++ больше НЕ хардкодит меш/ABP. Не несёт UStatsComponent и НЕ блокирует ECC_Visibility,
 * как и торговец, поэтому не попадает в авто-лок/хелсбары игрока и простреливается насквозь.
 *
 * Взаимодействие (E) открывает ДИАЛОГ (а не магазин). Диалог предлагает Collect-квест
 * «3 шкуры волков» (без kill-цели — гейт по собранным шкурам), после сдачи — «зачистить базу
 * бандитов». При входе игрока в радиус (overlap) староста регистрируется у
 * AContrarySurvivorPlayerController (NearbyElder).
 *
 * Риски модульного гуманоида (проверить в PIE): капсула Character'а ТВЁРДАЯ (может загородить
 * проход) — приемлемо для статичного NPC; авто-AIController подавлен AutoPossessAI=Disabled.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AElderNPC : public AMasterHumanoidCharacter, public IInteractableNPCInterface
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
	virtual void PostInitializeComponents() override;

	// Триггер диалоговой зоны: overlap по Pawn (игроку). По образцу AMasterTrader::InteractTrigger.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elder")
	USphereComponent* InteractTrigger;

	// Радиус, в котором доступно взаимодействие (см). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elder")
	float InteractRadius = 220.0f;

	// Квест 1 (DRAFT: «Шкуры волков» — собрать 3 шкуры, награда 150). Тюнингуется в редакторе.
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
