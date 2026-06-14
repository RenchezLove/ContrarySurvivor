// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuestComponent.generated.h"

class UStatsComponent;
class UInventoryComponent;

/**
 * Тип квеста (GDD §7.7: «типы Kill/Collect/Deliver»). Фаза 5 (демка): реализованы все три
 * через ОБОБЩЁННУЮ модель из двух целей у квеста — KILL-цель (KillTargetTag/TargetCount/
 * Progress) и ITEM-цель (RequiredItemName/RequiredItemCount/ItemProgress). Type теперь
 * описательный (для текста/иконки); завершённость считается по обеим целям независимо от Type
 * (см. UQuestComponent::AreObjectivesMet). Это позволяет:
 *   - Kill    — только KILL-цель (волки/бандиты);
 *   - Collect — только ITEM-цель (собрать N предметов в рюкзак, напр. шкуры);
 *   - Deliver — ITEM-цель (+ опц. KILL-цель), напр. «убить N бандитов и принести ноутбук».
 */
UENUM(BlueprintType)
enum class EQuestType : uint8
{
	Kill    UMETA(DisplayName = "Kill"),     // убить N целей с тегом KillTargetTag
	Collect UMETA(DisplayName = "Collect"),  // задел: собрать N предметов
	Deliver UMETA(DisplayName = "Deliver")   // задел: доставить предмет
};

/**
 * Состояние квеста в журнале игрока.
 * NotStarted — предложен (offered), но не принят; Active — принят, идёт; Completed —
 * условие выполнено (Progress >= TargetCount), но награда не выдана; TurnedIn — сдан, награда выдана.
 */
UENUM(BlueprintType)
enum class EQuestState : uint8
{
	NotStarted UMETA(DisplayName = "NotStarted"),
	Active     UMETA(DisplayName = "Active"),
	Completed  UMETA(DisplayName = "Completed"),
	TurnedIn   UMETA(DisplayName = "TurnedIn")
};

/**
 * Описание квеста (расширяемая структура, GDD §7.7). MVP: один Kill-квест от старосты
 * («убить 5 волков», награда 150 денег). Поля рассчитаны на разные типы (TargetCount/
 * Progress универсальны; KillTargetTag используется типом Kill).
 */
USTRUCT(BlueprintType)
struct FQuest
{
	GENERATED_BODY()

	// Стабильный идентификатор квеста (для поиска в журнале). DRAFT: "KillWolves".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName QuestId = NAME_None;

	// Короткое название (для диалога/журнала).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FString Title;

	// Текст задания (что сказать игроку).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQuestType Type = EQuestType::Kill;

	// --- KILL-цель ---
	// Тег цели, чьи убийства засчитываются (например, "Wolf"/"Bandit"). Пусто = любая цель.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName KillTargetTag = NAME_None;

	// Сколько целей нужно убить. 0 = у квеста НЕТ kill-цели (только сбор/доставка).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 TargetCount = 5;

	// Текущий прогресс убийств (инкрементит NotifyKill).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 Progress = 0;

	// --- ITEM-цель (Collect/Deliver, Фаза 5 демка) ---
	// Имя предмета (AMasterInventoryItem::ItemName), который нужно собрать/принести
	// (например, "Шкура волка"/"Ноутбук"). Пусто = у квеста нет item-цели.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FString RequiredItemName;

	// Сколько таких предметов нужно сдать. 0 = item-цели нет. При сдаче они ИЗЫМАЮТСЯ из рюкзака.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 RequiredItemCount = 0;

	// Текущий прогресс по предметам в рюкзаке (пересчитывает SyncInventoryQuests; min с RequiredItemCount).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 ItemProgress = 0;

	// Награда деньгами при сдаче (TurnedIn). DRAFT (решение Рината): 150. Тюнингуется на старосте/квесте.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	float RewardMoney = 150.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestState State = EQuestState::NotStarted;
};

// Делегат изменения квеста (для HUD/звука/журнала). Передаёт изменённый квест.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestChanged, const FQuest&, Quest);

/**
 * Журнал квестов игрока (Фаза 5, GDD §7.7 «UQuestComponent / структура квеста»).
 *
 * Каркас расширяемый: хранит МАССИВ FQuest, поддерживает типы Kill/Collect/Deliver
 * (в MVP инкрементируется только Kill через NotifyKill). Живёт на APlayerCharacter
 * (C++-сабобъект, editor-независимо) — игрок владеет своим журналом.
 *
 * Поток: староста предлагает квест (OfferQuest, состояние NotStarted) -> игрок принимает
 * (AcceptQuest, Active) -> убийства целей с нужным тегом инкрементят Progress (NotifyKill);
 * при Progress >= TargetCount -> Completed -> сдача у старосты (TurnInQuest) выдаёт деньги -> TurnedIn.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestComponent();

	// Делегат изменения состояния любого квеста (offer/accept/progress/complete/turn-in).
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestChanged OnQuestChanged;

	// Предложить квест: добавляет его в журнал в состоянии NotStarted, если такого ещё нет.
	// Идемпотентно (повторный offer того же QuestId ничего не меняет). Логирует OFFERED один раз.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void OfferQuest(const FQuest& Quest);

	// Принять предложенный квест (NotStarted -> Active). true при успехе.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool AcceptQuest(FName QuestId);

	// Уведомить журнал об убийстве цели с тегом TargetTag: для всех Active квестов с kill-целью
	// (TargetCount>0) и совпадающим KillTargetTag инкрементит Progress; пересчитывает завершённость.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void NotifyKill(FName TargetTag);

	// Пересчитать ITEM-прогресс Active/Completed квестов по содержимому рюкзака игрока
	// (считает предметы с ItemName == RequiredItemName). Вызывается реактивно (контроллер в Tick).
	// Меняет ItemProgress/State и шлёт OnQuestChanged ТОЛЬКО при реальном изменении (без спама).
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void SyncInventoryQuests(UInventoryComponent* Inventory);

	// Сдать выполненный квест (Completed -> TurnedIn): если у квеста есть item-цель — проверяет
	// наличие RequiredItemCount предметов «RequiredItemName» в рюкзаке владельца и ИЗЫМАЕТ их
	// (RemoveItem + Destroy), затем начисляет RewardMoney в UStatsComponent владельца.
	// true при успехе (квест найден, был Completed и предметы изъяты).
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TurnInQuest(FName QuestId);

	// --- Геттеры для UI/диалога ---

	// Указатель на квест по id (const). null, если нет.
	const FQuest* FindQuest(FName QuestId) const;

	// Первый квест в активной фазе (Active или Completed) — для трекера на HUD. null, если нет.
	const FQuest* GetTrackedQuest() const;

	const TArray<FQuest>& GetQuests() const { return Quests; }

	// Сколько квестов сдано (State == TurnedIn) — для экрана смерти (#26 «сколько квестов выполнено»).
	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetTurnedInQuestCount() const;

	// Выполнены ли ОБЕ цели квеста (kill + item). Цель с нулевым требованием считается выполненной.
	static bool AreObjectivesMet(const FQuest& Quest);

private:
	// Журнал квестов игрока (расширяемо — массив).
	UPROPERTY()
	TArray<FQuest> Quests;

	// Изменяемый поиск (внутренний).
	FQuest* FindQuestMutable(FName QuestId);

	// Пересчитать состояние одной активной фазы квеста (Active<->Completed) по целям.
	// Возвращает true, если состояние изменилось (тогда вызывающий шлёт OnQuestChanged).
	bool RecomputeState(FQuest& Quest);
};
