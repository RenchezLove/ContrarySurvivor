// Fill out your copyright notice in the Description page of Project Settings.

#include "QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h" // F3: события взятия/сдачи квеста
#include "UInventoryComponent.h"   // изъятие предметов при сдаче / подсчёт прогресса (Фаза 5)
#include "AMasterInventoryItem.h"  // ItemName
#include "GameFramework/Actor.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"  // выдача предметов задания герою (GiveItemFromRow)
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"    // проверка строк предметов до выдачи

namespace
{
	// Все ли предметы списка можно выдать: получатель — герой и строка есть в таблице предметов.
	// Проверяется ДО смены состояния задания, чтобы не оставить героя без обещанного.
	bool CanGrantAll(const AActor* OwnerActor, const TArray<FQuestItemGrant>& Grants, FName& OutMissingRow)
	{
		if (Grants.Num() == 0)
		{
			return true;
		}
		if (!Cast<APlayerCharacter>(OwnerActor))
		{
			return false;
		}
		for (const FQuestItemGrant& Grant : Grants)
		{
			if (!ContraryItems::FindRow(Grant.ItemRow))
			{
				OutMissingRow = Grant.ItemRow;
				return false;
			}
		}
		return true;
	}

	void GrantAll(AActor* OwnerActor, const TArray<FQuestItemGrant>& Grants, const TCHAR* Why, FName QuestId)
	{
		APlayerCharacter* Player = Cast<APlayerCharacter>(OwnerActor);
		if (!Player)
		{
			return;
		}
		for (const FQuestItemGrant& Grant : Grants)
		{
			const int32 Given = Player->GiveItemFromRow(Grant.ItemRow, Grant.Count);
			UE_LOG(LogQA, Display, TEXT("QA: quest %s %s: '%s' x%d (выдано %d)"),
				*QuestId.ToString(), Why, *Grant.ItemRow.ToString(), Grant.Count, Given);
		}
	}
}

UQuestComponent::UQuestComponent()
{
	// Журнал тикать не нужно — реактивен (события offer/accept/kill/turn-in + Sync из контроллера).
	PrimaryComponentTick.bCanEverTick = false;
}

FQuest* UQuestComponent::FindQuestMutable(FName QuestId)
{
	for (FQuest& Q : Quests)
	{
		if (Q.QuestId == QuestId)
		{
			return &Q;
		}
	}
	return nullptr;
}

const FQuest* UQuestComponent::FindQuest(FName QuestId) const
{
	for (const FQuest& Q : Quests)
	{
		if (Q.QuestId == QuestId)
		{
			return &Q;
		}
	}
	return nullptr;
}

const FQuest* UQuestComponent::GetTrackedQuest() const
{
	if (!TrackedQuestId.IsNone())
	{
		const FQuest* Chosen = FindQuest(TrackedQuestId);
		if (Chosen && IsInActivePhase(*Chosen))
		{
			return Chosen;
		}
	}

	// Выбора нет (старое сохранение, первые шаги) — первое идущее задание журнала, как раньше.
	for (const FQuest& Q : Quests)
	{
		if (IsInActivePhase(Q))
		{
			return &Q;
		}
	}
	return nullptr;
}

bool UQuestComponent::SetTrackedQuest(FName QuestId)
{
	const FQuest* Q = QuestId.IsNone() ? nullptr : FindQuest(QuestId);
	if (!Q || !IsInActivePhase(*Q))
	{
		return false;
	}
	if (TrackedQuestId != QuestId)
	{
		TrackedQuestId = QuestId;
		UE_LOG(LogQA, Display, TEXT("QA: quest TRACKED (%s)"), *QuestId.ToString());
		// Подписчикам (строка на экране, метки) — повод перечитать журнал.
		OnQuestChanged.Broadcast(*Q);
	}
	return true;
}

void UQuestComponent::PickNextTrackedQuest()
{
	const FQuest* Next = nullptr;
	for (const FQuest& Q : Quests)
	{
		if (!IsInActivePhase(Q))
		{
			continue;
		}
		if (Q.Kind == EQuestKind::Main)
		{
			Next = &Q;
			break;
		}
		if (!Next)
		{
			Next = &Q;
		}
	}
	TrackedQuestId = Next ? Next->QuestId : NAME_None;
	UE_LOG(LogQA, Display, TEXT("QA: quest TRACKED -> %s"), *TrackedQuestId.ToString());
}

int32 UQuestComponent::GetTurnedInQuestCount() const
{
	int32 Count = 0;
	for (const FQuest& Q : Quests)
	{
		if (Q.State == EQuestState::TurnedIn)
		{
			++Count;
		}
	}
	return Count;
}

bool UQuestComponent::IsQuestTurnedIn(FName QuestId) const
{
	const FQuest* Quest = QuestId.IsNone() ? nullptr : FindQuest(QuestId);
	return Quest && Quest->State == EQuestState::TurnedIn;
}

bool UQuestComponent::AreObjectivesMet(const FQuest& Quest)
{
	const bool bKillMet = (Quest.TargetCount <= 0) || (Quest.Progress >= Quest.TargetCount);
	const bool bItemMet = (Quest.RequiredItemCount <= 0) || (Quest.ItemProgress >= Quest.RequiredItemCount);
	return bKillMet && bItemMet;
}

bool UQuestComponent::RecomputeState(FQuest& Quest)
{
	// Пересчёт только в активной фазе (Active<->Completed). NotStarted/TurnedIn не трогаем.
	if (Quest.State != EQuestState::Active && Quest.State != EQuestState::Completed)
	{
		return false;
	}

	const EQuestState Desired = AreObjectivesMet(Quest) ? EQuestState::Completed : EQuestState::Active;
	if (Desired == Quest.State)
	{
		return false;
	}

	Quest.State = Desired;
	if (Desired == EQuestState::Completed)
	{
		UE_LOG(LogQA, Display, TEXT("QA: quest COMPLETED (%s)"), *Quest.QuestId.ToString());
	}
	else
	{
		UE_LOG(LogQA, Display, TEXT("QA: quest back to ACTIVE (%s) - objectives no longer met"), *Quest.QuestId.ToString());
	}
	return true;
}

void UQuestComponent::OfferQuest(const FQuest& Quest)
{
	if (Quest.QuestId.IsNone())
	{
		return;
	}

	// Идемпотентно: если квест уже в журнале (в любом состоянии) — не дублируем и не сбрасываем.
	if (FQuest* Existing = FindQuestMutable(Quest.QuestId))
	{
		// Вид задания и выдавший — описание, а не ход выполнения: у задания из сохранения,
		// записанного до 10.10.2026, этих полей нет, и они читаются «основное, от старосты».
		// Выдающий знает правду — доносим её при первом же разговоре.
		if (Existing->Kind != Quest.Kind || Existing->GiverNpcTag != Quest.GiverNpcTag)
		{
			Existing->Kind = Quest.Kind;
			Existing->GiverNpcTag = Quest.GiverNpcTag;
			OnQuestChanged.Broadcast(*Existing);
		}
		MergePrepDefinition(Quest); // то же для подготовительного шага (11.10.2026)
		RefreshDefinitionTexts(Quest, *Existing); // и для текстов: журнал хранит их копию
		return;
	}

	FQuest Added = Quest;
	Added.Progress = 0;
	Added.ItemProgress = 0;
	Added.State = EQuestState::NotStarted;
	Quests.Add(Added);

	UE_LOG(LogQA, Display, TEXT("QA: quest OFFERED (%s, kill %d %s, item %d '%s')"),
		*Added.QuestId.ToString(), Added.TargetCount, *Added.KillTargetTag.ToString(),
		Added.RequiredItemCount, *Added.RequiredItemName);

	// Датчик «квест предложен» — начало воронки квеста (сводное ТЗ издателя 13.08, задача 3).
	// Строго один раз на квест: сторож стоит выше журнала, потому что OfferQuest зовётся
	// каждый кадр отрисовки диалога, а журнал может быть очищен загрузкой/новой игрой.
	if (MarkQuestOfferedForAnalytics(Added.QuestId))
	{
		if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
		{
			Analytics->RecordQuestOffered(Added.QuestId);
		}
	}

	OnQuestChanged.Broadcast(Added);
}

bool UQuestComponent::MarkQuestOfferedForAnalytics(FName QuestId)
{
	if (QuestId.IsNone() || AnalyticsOfferedQuests.Contains(QuestId))
	{
		return false;
	}
	AnalyticsOfferedQuests.Add(QuestId);
	return true;
}

bool UQuestComponent::AcceptQuest(FName QuestId)
{
	FQuest* Q = FindQuestMutable(QuestId);
	if (!Q || Q->State != EQuestState::NotStarted)
	{
		return false;
	}

	// Предметы, которые задание выдаёт при взятии (ноутбук для учёных): нельзя выдать — задание
	// не берётся вовсе, иначе герой получил бы невыполнимое поручение.
	FName MissingRow = NAME_None;
	if (!CanGrantAll(GetOwner(), Q->GiveOnAccept, MissingRow))
	{
		UE_LOG(LogQA, Warning,
			TEXT("QA: quest ACCEPT REFUSED (%s) - предмет задания '%s' выдать нельзя (нет строки в таблице предметов или владелец не герой)"),
			*Q->QuestId.ToString(), *MissingRow.ToString());
		return false;
	}

	// Отслеживание: взятое задание сразу становится отслеживаемым — и основное, и побочное
	// (Ринат 11.10.2026 про задание охранника: «Предлагаю сразу ставить в отслеживаемое этот
	// квест»). Метка основного задания при этом с экрана не уходит: меток две, по одной на
	// вид задания (QuestTargets::PickMarkerQuest).
	Q->State = EQuestState::Active;
	TrackedQuestId = QuestId;

	// Копия списка: выдача меняет рюкзак, а указатель на задание после неё перечитываем.
	const TArray<FQuestItemGrant> GiveCopy = Q->GiveOnAccept;
	GrantAll(GetOwner(), GiveCopy, TEXT("выдано при взятии"), QuestId);
	Q = FindQuestMutable(QuestId);
	if (!Q)
	{
		return false;
	}

	UE_LOG(LogQA, Display, TEXT("QA: quest ACCEPTED (%s, kill %d %s, item %d '%s')"),
		*Q->QuestId.ToString(), Q->TargetCount, *Q->KillTargetTag.ToString(),
		Q->RequiredItemCount, *Q->RequiredItemName);

	// На случай, если предметы уже в рюкзаке к моменту принятия (или kill-цели нет) — сразу пересчёт.
	const bool bChanged = RecomputeState(*Q);
	OnQuestChanged.Broadcast(*Q);
	(void)bChanged;

	// F3 (ADR-038): событие «взятие квеста». Хук именно здесь (не на OnQuestChanged): делегат
	// стреляет и на прогрессе, а нам нужен один факт принятия. Без ключей — no-op.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordQuestAccepted(QuestId);
	}
	return true;
}

void UQuestComponent::NotifyKill(FName TargetTag)
{
	for (FQuest& Q : Quests)
	{
		if (Q.State != EQuestState::Active)
		{
			continue;
		}
		// Учитываем только квесты с kill-целью (TargetCount>0).
		if (Q.TargetCount <= 0)
		{
			continue;
		}
		// Пустой KillTargetTag = любая цель; иначе должен совпадать тег.
		if (!Q.KillTargetTag.IsNone() && Q.KillTargetTag != TargetTag)
		{
			continue;
		}

		Q.Progress = FMath::Min(Q.Progress + 1, Q.TargetCount);
		UE_LOG(LogQA, Display, TEXT("QA: quest %s kill progress %d/%d"),
			*Q.QuestId.ToString(), Q.Progress, Q.TargetCount);

		RecomputeState(Q); // может перевести в Completed (лог внутри)
		OnQuestChanged.Broadcast(Q);
	}
}

void UQuestComponent::NotifyObjectiveEvent(FName ObjectiveTag)
{
	if (ObjectiveTag.IsNone())
	{
		return;
	}
	for (FQuest& Q : Quests)
	{
		if (Q.State != EQuestState::Active || Q.TargetCount <= 0 || Q.KillTargetTag != ObjectiveTag)
		{
			continue;
		}
		Q.Progress = FMath::Min(Q.Progress + 1, Q.TargetCount);
		UE_LOG(LogQA, Display, TEXT("QA: quest %s objective %s progress %d/%d"),
			*Q.QuestId.ToString(), *ObjectiveTag.ToString(), Q.Progress, Q.TargetCount);
		RecomputeState(Q);
		OnQuestChanged.Broadcast(Q);
	}
}

// ---------------------------------------------------------------------------
// Подготовительный шаг задания (11.10.2026)
// ---------------------------------------------------------------------------

EQuestPrepStep UQuestComponent::ComputePrepStep(const FQuest& Quest, EQuestPrepPossession Possession)
{
	if (Quest.PrepItemRow.IsNone() || Quest.PrepStep == EQuestPrepStep::Done)
	{
		return EQuestPrepStep::Done; // шагов нет либо они пройдены: снятый потом предмет заново не просим
	}
	if (Quest.State != EQuestState::Active)
	{
		return Quest.PrepStep; // не взято, готово к сдаче или сдано — шаг не двигается
	}
	// Первая сверка после взятия задания: предмет уже есть (в сумке или в слоте) — шаги
	// засчитаны сразу, игрок их не видит.
	if (Quest.PrepStep == EQuestPrepStep::NotChecked && Possession != EQuestPrepPossession::None)
	{
		return EQuestPrepStep::Done;
	}
	switch (Possession)
	{
		case EQuestPrepPossession::InSlot: return EQuestPrepStep::Done;
		case EQuestPrepPossession::InBag:  return EQuestPrepStep::Equip;
		default:                           return EQuestPrepStep::Buy;
	}
}

FText UQuestComponent::GetPrepObjectiveText(const FQuest& Quest)
{
	if (!IsPrepStepShown(Quest))
	{
		return FText::GetEmpty();
	}
	return Quest.PrepStep == EQuestPrepStep::Buy ? Quest.PrepBuyObjectiveText : Quest.PrepEquipObjectiveText;
}

bool UQuestComponent::HasPendingPrep() const
{
	for (const FQuest& Q : Quests)
	{
		if (Q.State == EQuestState::Active && Q.PrepStep != EQuestPrepStep::Done)
		{
			return true;
		}
	}
	return false;
}

bool UQuestComponent::HasUncheckedPrep() const
{
	for (const FQuest& Q : Quests)
	{
		if (Q.State == EQuestState::Active && Q.PrepStep == EQuestPrepStep::NotChecked)
		{
			return true;
		}
	}
	return false;
}

bool UQuestComponent::IsAnyPrepStepShown(EQuestPrepStep Step) const
{
	for (const FQuest& Q : Quests)
	{
		if (IsPrepStepShown(Q) && Q.PrepStep == Step)
		{
			return true;
		}
	}
	return false;
}

void UQuestComponent::UpdatePrepSteps(TFunctionRef<EQuestPrepPossession(FName)> GetPossession)
{
	for (FQuest& Q : Quests)
	{
		if (Q.State != EQuestState::Active || Q.PrepStep == EQuestPrepStep::Done)
		{
			continue;
		}
		const EQuestPrepStep Old = Q.PrepStep;
		const EQuestPrepStep New = ComputePrepStep(Q,
			Q.PrepItemRow.IsNone() ? EQuestPrepPossession::None : GetPossession(Q.PrepItemRow));
		if (New == Old)
		{
			continue;
		}
		Q.PrepStep = New;
		if (Q.PrepItemRow.IsNone())
		{
			continue; // у задания шагов нет — отметили и молчим
		}
		UE_LOG(LogQA, Display, TEXT("QA: quest prep step (%s) %d -> %d"), *Q.QuestId.ToString(),
			static_cast<int32>(Old), static_cast<int32>(New));
		if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(GetOwner()))
		{
			// Предмет уже был у героя при взятии задания — шагов он не видел, событий нет.
			if (Old == EQuestPrepStep::NotChecked && New == EQuestPrepStep::Buy)
			{
				Analytics->RecordQuestStep(Q.QuestId, TEXT("prep_buy_shown"));
			}
			else if (New == EQuestPrepStep::Done && Old != EQuestPrepStep::NotChecked)
			{
				Analytics->RecordQuestStep(Q.QuestId, TEXT("prep_equipped"));
			}
		}
		OnQuestChanged.Broadcast(Q);
	}
}

const FQuest* UQuestComponent::FindPrepDeal(FName ItemRow) const
{
	if (ItemRow.IsNone())
	{
		return nullptr;
	}
	for (const FQuest& Q : Quests)
	{
		if (Q.PrepItemRow == ItemRow && IsPrepStepShown(Q) && Q.PrepStep == EQuestPrepStep::Buy
			&& !Q.bPrepDealUsed && Q.PrepDealPrice > 0.0f)
		{
			return &Q;
		}
	}
	return nullptr;
}

bool UQuestComponent::NotePrepPurchase(FName QuestId, float Debt)
{
	FQuest* Q = FindQuestMutable(QuestId);
	if (!Q || Q->PrepItemRow.IsNone())
	{
		return false;
	}
	Q->bPrepDealUsed = true;
	Q->PrepDebt += FMath::Max(0.0f, Debt);
	if (Q->PrepStep == EQuestPrepStep::Buy)
	{
		Q->PrepStep = EQuestPrepStep::Equip; // предмет в сумке; в слот его ставит игрок
	}
	UE_LOG(LogQA, Display, TEXT("QA: quest prep purchase (%s) - долг %.0f, всего долга %.0f"),
		*Q->QuestId.ToString(), Debt, Q->PrepDebt);
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(GetOwner()))
	{
		Analytics->RecordQuestStep(Q->QuestId, Debt > 0.0f ? TEXT("prep_bought_debt") : TEXT("prep_bought_discount"));
	}
	OnQuestChanged.Broadcast(*Q);
	return true;
}

bool UQuestComponent::MergePrepDefinition(const FQuest& Definition)
{
	FQuest* Q = FindQuestMutable(Definition.QuestId);
	if (!Q || Definition.PrepItemRow.IsNone() || !Q->PrepItemRow.IsNone())
	{
		return false;
	}
	Q->PrepItemRow = Definition.PrepItemRow;
	Q->PrepDealPrice = Definition.PrepDealPrice;
	Q->PrepBuyObjectiveText = Definition.PrepBuyObjectiveText;
	Q->PrepEquipObjectiveText = Definition.PrepEquipObjectiveText;
	Q->PrepDealText = Definition.PrepDealText;
	Q->PrepDebtText = Definition.PrepDebtText;
	Q->PrepBoughtHintText = Definition.PrepBoughtHintText;
	// Задание могли успеть отметить «шагов нет», пока описания у него не было.
	if (Q->PrepStep == EQuestPrepStep::Done && Q->State == EQuestState::Active && !Q->bPrepDealUsed)
	{
		Q->PrepStep = EQuestPrepStep::NotChecked;
	}
	return true;
}

void UQuestComponent::RefreshDefinitionTexts(const FQuest& Definition, FQuest& InJournal)
{
	InJournal.Title = Definition.Title;
	InJournal.Description = Definition.Description;
	InJournal.KillObjectiveLabel = Definition.KillObjectiveLabel;
	InJournal.ItemObjectiveLabel = Definition.ItemObjectiveLabel;
	InJournal.AcceptReplyText = Definition.AcceptReplyText;
	InJournal.TurnInReplyText = Definition.TurnInReplyText;
	InJournal.CloseReplyText = Definition.CloseReplyText;
	InJournal.TurnInHintText = Definition.TurnInHintText;
	InJournal.ObjectiveFormatOverride = Definition.ObjectiveFormatOverride;
	InJournal.LowHealthWarnThreshold = Definition.LowHealthWarnThreshold;
	InJournal.LowHealthWarnText = Definition.LowHealthWarnText;
	InJournal.RewardEquipItemRow = Definition.RewardEquipItemRow;
	// Подписи подготовительного шага — только тому же шагу (другой предмет несёт свои подписи).
	if (InJournal.PrepItemRow == Definition.PrepItemRow)
	{
		InJournal.PrepBuyObjectiveText = Definition.PrepBuyObjectiveText;
		InJournal.PrepEquipObjectiveText = Definition.PrepEquipObjectiveText;
		InJournal.PrepDealText = Definition.PrepDealText;
		InJournal.PrepDebtText = Definition.PrepDebtText;
		InJournal.PrepBoughtHintText = Definition.PrepBoughtHintText;
	}
}

// ---------------------------------------------------------------------------
// Предупреждение о малом здоровье при выходе из деревни (11.10.2026)
// ---------------------------------------------------------------------------

bool UQuestComponent::ShouldWarnLowHealth(const FQuest& Quest, float Health)
{
	// Только пока задание взято и врагов ещё надо бить: с перебитыми бандитами (осталось
	// забрать ноутбук или вернуться к старосте) предупреждать уже не о чем.
	return Quest.State == EQuestState::Active
		&& Quest.LowHealthWarnThreshold > 0.0f
		&& Quest.TargetCount > 0 && Quest.Progress < Quest.TargetCount
		&& Health < Quest.LowHealthWarnThreshold;
}

const FQuest* UQuestComponent::FindLowHealthWarning(float Health) const
{
	for (const FQuest& Q : Quests)
	{
		if (ShouldWarnLowHealth(Q, Health))
		{
			return &Q;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Проводник к предмету-награде (11.10.2026)
// ---------------------------------------------------------------------------

EQuestRewardGuideStep UQuestComponent::ComputeRewardGuideStep(const FQuest& Quest,
	EQuestPrepPossession Possession, bool bCanSwitchFirearms)
{
	if (Quest.RewardGuideStep != EQuestRewardGuideStep::Equip)
	{
		return Quest.RewardGuideStep; // шаг «показать кнопку» закрывает само нажатие кнопки
	}
	switch (Possession)
	{
		// В слоте: дальше показываем кнопку смены оружия, если ей есть что менять.
		case EQuestPrepPossession::InSlot:
			return bCanSwitchFirearms ? EQuestRewardGuideStep::Switch : EQuestRewardGuideStep::Done;
		case EQuestPrepPossession::InBag:
			return EQuestRewardGuideStep::Equip;
		// Предмета больше нет (продан, выброшен) — вести не к чему.
		default:
			return EQuestRewardGuideStep::Done;
	}
}

bool UQuestComponent::HasPendingRewardGuide() const
{
	return IsAnyRewardGuideStep(EQuestRewardGuideStep::Equip) || IsAnyRewardGuideStep(EQuestRewardGuideStep::Switch);
}

bool UQuestComponent::IsAnyRewardGuideStep(EQuestRewardGuideStep Step) const
{
	for (const FQuest& Q : Quests)
	{
		if (Q.RewardGuideStep == Step && !Q.RewardEquipItemRow.IsNone())
		{
			return true;
		}
	}
	return false;
}

void UQuestComponent::UpdateRewardGuides(TFunctionRef<EQuestPrepPossession(FName)> GetPossession, bool bCanSwitchFirearms)
{
	for (FQuest& Q : Quests)
	{
		if (Q.RewardGuideStep != EQuestRewardGuideStep::Equip || Q.RewardEquipItemRow.IsNone())
		{
			continue;
		}
		const EQuestRewardGuideStep New = ComputeRewardGuideStep(Q, GetPossession(Q.RewardEquipItemRow), bCanSwitchFirearms);
		if (New == Q.RewardGuideStep)
		{
			continue;
		}
		UE_LOG(LogQA, Display, TEXT("QA: quest reward guide (%s) %d -> %d"), *Q.QuestId.ToString(),
			static_cast<int32>(Q.RewardGuideStep), static_cast<int32>(New));
		Q.RewardGuideStep = New;
		OnQuestChanged.Broadcast(Q);
	}
}

bool UQuestComponent::NoteWeaponSwitched()
{
	bool bChanged = false;
	for (FQuest& Q : Quests)
	{
		if (Q.RewardGuideStep == EQuestRewardGuideStep::Switch)
		{
			Q.RewardGuideStep = EQuestRewardGuideStep::Done;
			UE_LOG(LogQA, Display, TEXT("QA: quest reward guide (%s) - кнопка смены оружия нажата"), *Q.QuestId.ToString());
			OnQuestChanged.Broadcast(Q);
			bChanged = true;
		}
	}
	return bChanged;
}

void UQuestComponent::SyncInventoryQuests(UInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return;
	}

	for (FQuest& Q : Quests)
	{
		// Только активная фаза и только квесты с item-целью.
		if (Q.RequiredItemCount <= 0)
		{
			continue;
		}
		if (Q.State != EQuestState::Active && Q.State != EQuestState::Completed)
		{
			continue;
		}

		// Считаем предметы рюкзака с нужным именем (регистр учитывается; имена задаём
		// детерминированно). Build 1.2.1 (ТЗ Г): стак считается ПО ШТУКАМ — «Шкура волка» x3
		// в одном акторе засчитывает 3 (Max(1,...): нестакаемый актор = 1, как раньше).
		int32 Count = 0;
		for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
		{
			if (Item && Item->ItemName.Equals(Q.RequiredItemName, ESearchCase::CaseSensitive))
			{
				Count += FMath::Max(1, Item->GetStackCount());
			}
		}

		const int32 NewProgress = FMath::Min(Count, Q.RequiredItemCount);
		bool bChanged = false;
		if (NewProgress != Q.ItemProgress)
		{
			Q.ItemProgress = NewProgress;
			bChanged = true;
			UE_LOG(LogQA, Display, TEXT("QA: quest %s item progress %d/%d ('%s')"),
				*Q.QuestId.ToString(), Q.ItemProgress, Q.RequiredItemCount, *Q.RequiredItemName);
		}

		const bool bStateChanged = RecomputeState(Q);
		if (bChanged || bStateChanged)
		{
			OnQuestChanged.Broadcast(Q);
		}
	}
}

bool UQuestComponent::TurnInQuest(FName QuestId, FName ReceiverNpcTag)
{
	FQuest* Q = FindQuestMutable(QuestId);
	if (!Q || Q->State != EQuestState::Completed)
	{
		return false;
	}

	// Задание сдаётся только тому, кому положено (10.10.2026): у прежних заданий старосты обе
	// метки пусты; задание «отнести учёным» староста принять не может.
	if (Q->TurnInNpcTag != ReceiverNpcTag)
	{
		UE_LOG(LogQA, Display, TEXT("QA: turn-in REFUSED (%s) - сдаётся '%s', а принимает '%s'"),
			*Q->QuestId.ToString(), *Q->TurnInNpcTag.ToString(), *ReceiverNpcTag.ToString());
		return false;
	}

	// Награду предметами проверяем ДО изъятия вещей: выдать нельзя — задание не сдаётся.
	FName MissingRewardRow = NAME_None;
	if (!CanGrantAll(GetOwner(), Q->RewardItems, MissingRewardRow))
	{
		UE_LOG(LogQA, Warning,
			TEXT("QA: turn-in FAILED (%s) - награду '%s' выдать нельзя (нет строки в таблице предметов или владелец не герой)"),
			*Q->QuestId.ToString(), *MissingRewardRow.ToString());
		return false;
	}

	AActor* OwnerActor = GetOwner();

	// Если у квеста есть item-цель — изымаем предметы из рюкзака владельца.
	if (Q->RequiredItemCount > 0)
	{
		UInventoryComponent* Inv = OwnerActor ? OwnerActor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inv)
		{
			UE_LOG(LogQA, Display, TEXT("QA: turn-in FAILED (%s) - no inventory on owner"), *Q->QuestId.ToString());
			return false;
		}

		// Собираем предметы с совпадающим именем НА НУЖНОЕ ЧИСЛО ШТУК. Build 1.2.1 (ТЗ Г):
		// предмет может быть стаком — с актора берём до GetStackCount() штук; если стак
		// больше остатка требования, изымаем ЧАСТИЧНО (уменьшаем счётчик, актор живёт).
		TArray<AMasterInventoryItem*> ToRemove;   // акторы, уходящие целиком
		AMasterInventoryItem* PartialFrom = nullptr; // стак, из которого берём часть
		int32 PartialTake = 0;
		int32 Collected = 0;
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (Collected >= Q->RequiredItemCount)
			{
				break;
			}
			if (!Item || !Item->ItemName.Equals(Q->RequiredItemName, ESearchCase::CaseSensitive))
			{
				continue;
			}
			const int32 Available = FMath::Max(1, Item->GetStackCount());
			const int32 Need = Q->RequiredItemCount - Collected;
			if (Available <= Need)
			{
				ToRemove.Add(Item);
				Collected += Available;
			}
			else
			{
				PartialFrom = Item;
				PartialTake = Need;
				Collected += Need;
			}
		}

		if (Collected < Q->RequiredItemCount)
		{
			UE_LOG(LogQA, Display, TEXT("QA: turn-in FAILED (%s) - need %d '%s', have %d"),
				*Q->QuestId.ToString(), Q->RequiredItemCount, *Q->RequiredItemName, Collected);
			return false;
		}

		for (AMasterInventoryItem* Item : ToRemove)
		{
			Inv->RemoveItem(Item);
			if (IsValid(Item))
			{
				Item->Destroy();
			}
		}
		if (PartialFrom && PartialTake > 0)
		{
			PartialFrom->StackCount = FMath::Max(0, PartialFrom->StackCount - PartialTake);
		}
		UE_LOG(LogQA, Display, TEXT("QA: turn-in took %d x '%s' from backpack (%s)"),
			Q->RequiredItemCount, *Q->RequiredItemName, *Q->QuestId.ToString());
	}

	// Сдаётся то самое задание, которое отслеживалось, — отслеживание переходит дальше.
	const bool bWasTracked = (GetTrackedQuest() == Q);

	Q->State = EQuestState::TurnedIn;
	if (bWasTracked)
	{
		PickNextTrackedQuest();
	}

	// Награда деньгами начисляется в UStatsComponent владельца (игрока).
	float Balance = -1.0f;
	if (OwnerActor)
	{
		if (UStatsComponent* Stats = OwnerActor->FindComponentByClass<UStatsComponent>())
		{
			Stats->AddMoney(Q->GetPayoutMoney()); // награда минус долг за предмет подготовительного шага
			Balance = Stats->GetMoney();
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: quest TURNED IN (%s), +%.0f money, balance %.0f"),
		*Q->QuestId.ToString(), Q->RewardMoney, Balance);

	// Награда предметами (двустволка и патроны охранника) — после монет. Копия списка: выдача
	// меняет рюкзак, указатель на задание после неё перечитываем.
	const TArray<FQuestItemGrant> RewardCopy = Q->RewardItems;
	GrantAll(OwnerActor, RewardCopy, TEXT("награда"), QuestId);
	Q = FindQuestMutable(QuestId);
	if (!Q)
	{
		return true;
	}

	// Награду-оружие игроку помогают поставить в слот (двустволка охранника, 11.10.2026).
	if (!Q->RewardEquipItemRow.IsNone())
	{
		Q->RewardGuideStep = EQuestRewardGuideStep::Equip;
	}

	OnQuestChanged.Broadcast(*Q);

	// F3 (ADR-038): событие «сдача квеста» (с id квеста). Без ключей — no-op.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordQuestTurnedIn(QuestId);
	}
	return true;
}

const FQuest* UQuestComponent::FindCompletedQuestForReceiver(FName ReceiverNpcTag) const
{
	if (ReceiverNpcTag.IsNone())
	{
		return nullptr;
	}
	for (const FQuest& Q : Quests)
	{
		if (Q.State == EQuestState::Completed && Q.TurnInNpcTag == ReceiverNpcTag)
		{
			return &Q;
		}
	}
	return nullptr;
}

bool UQuestComponent::HasTurnedInQuestForReceiver(FName ReceiverNpcTag) const
{
	if (ReceiverNpcTag.IsNone())
	{
		return false;
	}
	for (const FQuest& Q : Quests)
	{
		if (Q.State == EQuestState::TurnedIn && Q.TurnInNpcTag == ReceiverNpcTag)
		{
			return true;
		}
	}
	return false;
}

void UQuestComponent::RestoreQuests(const TArray<FQuest>& SavedQuests, FName SavedTrackedQuestId)
{
	// Б3 («Продолжить»): журнал заменяется целиком сохранённым снимком — квесты, ещё не
	// предложенные в ЭТОЙ сессии (игрок не успел снова подойти к старосте), уже показывают
	// верный заголовок/прогресс/состояние, а не пустую заглушку до следующего OfferQuest.
	Quests = SavedQuests;

	// Отслеживаемое задание — из сохранения. Пусто (сохранение записано до 10.10.2026) —
	// остаётся прежнее «первое идущее». Записанное задание уже не идёт — выбираем следующее.
	TrackedQuestId = SavedTrackedQuestId;
	if (!TrackedQuestId.IsNone())
	{
		const FQuest* Saved = FindQuest(TrackedQuestId);
		if (!Saved || !IsInActivePhase(*Saved))
		{
			PickNextTrackedQuest();
		}
	}

	for (const FQuest& Q : Quests)
	{
		OnQuestChanged.Broadcast(Q);
	}
	OnJournalRestored.Broadcast();

	UE_LOG(LogQA, Display, TEXT("QA: quest journal restored from save (%d quest(s))"), Quests.Num());
}
