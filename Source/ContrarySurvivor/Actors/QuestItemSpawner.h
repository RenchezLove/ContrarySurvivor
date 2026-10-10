// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // EQuestState
#include "QuestItemSpawner.generated.h"

class APickup;
class APlayerCharacter;

/**
 * Место, где лежит предмет задания (10.10.2026: рация охранника базы учёных у южного логова
 * волков). Ставится на уровень актором; сам кладёт на своё место подбираемый мешок (APickup)
 * с предметом и следит, чтобы предмет лежал ровно тогда, когда должен:
 *  - задание сдано — предмета нет (и не появится после «Продолжить»);
 *  - предмет уже в сумке героя — второго не кладём;
 *  - задание ещё не взято — предмета нет (переключатель «Только при взятом задании»);
 *  - «Новая игра» в живом мире — журнал героя пуст, сумка пуста: всё возвращается к началу
 *    само, отдельного сброса не нужно.
 * Состояние не хранит и в сохранение ничего не пишет — раз в секунду сверяется с журналом
 * и сумкой героя.
 *
 * Известная граница: если герой выбросит предмет на землю или уберёт его в личный сундук,
 * раскладчик положит ещё один (в сумке предмета нет). Заданию это не мешает.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AQuestItemSpawner : public AActor
{
	GENERATED_BODY()

public:
	AQuestItemSpawner();

	// Сверить состояние прямо сейчас (её же зовёт таймер). Публичная — для автотеста.
	void RefreshNow();

	// Мешок, который сейчас лежит на месте (null — не лежит).
	APickup* GetSpawnedPickup() const { return SpawnedPickup.Get(); }

	// Должен ли предмет лежать — чистое правило, проверяется автотестом без мира.
	static bool ShouldItemLie(bool bQuestInJournal, EQuestState QuestState, bool bPlayerHasItem,
		bool bOnlyWhileQuestTaken);

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuestItem")
	USceneComponent* SceneRoot;

	// Задание, которому принадлежит предмет (идентификатор латиницей).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestItem", meta = (DisplayPriority = "1",
		DisplayName = "Задание (идентификатор)"))
	FName QuestId = FName(TEXT("FetchGuardRadio"));

	// Что лежит: строка таблицы предметов.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestItem", meta = (DisplayPriority = "2",
		DisplayName = "Предмет (строка таблицы предметов)"))
	FName ItemRow = FName(TEXT("guard_radio"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestItem", meta = (DisplayPriority = "3", ClampMin = "1",
		DisplayName = "Сколько штук"))
	int32 ItemCount = 1;

	// Каким мешком лежит предмет (внешний вид, свечение, окно обыска — всё от этого класса).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestItem", meta = (DisplayPriority = "4",
		DisplayName = "Класс подбираемого мешка"))
	TSubclassOf<APickup> PickupClass;

	// Включено: предмет появляется только когда задание взято. Выключено: лежит с самого начала,
	// подобранный заранее засчитается, когда задание возьмут.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestItem", meta = (DisplayPriority = "5",
		DisplayName = "Только при взятом задании"))
	bool bOnlyWhileQuestTaken = true;

	// Метка цели на экране: по ней задание находит это место (FQuest::MapMarkerTag).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestItem", meta = (DisplayPriority = "6",
		DisplayName = "Метка цели на экране"))
	FName QuestMarkerTag = FName(TEXT("GuardRadioSpot"));

	// Как часто сверяться с журналом и сумкой героя (сек).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestItem", meta = (DisplayPriority = "7", ClampMin = "0.1"))
	float CheckInterval = 1.0f;

private:
	bool PlayerHasItem(const APlayerCharacter* Player) const;
	void SpawnPickup();

	TWeakObjectPtr<APickup> SpawnedPickup;
	FTimerHandle CheckTimer;
	bool bWarnedMissingRow = false;
};
