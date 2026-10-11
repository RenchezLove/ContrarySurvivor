// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/QuestTargets.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/InteractableNPCInterface.h" // подъём метки над собеседником
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Characters/MasterTrader.h" // цель шага покупки
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Actors/TalkerNPC.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator

namespace QuestTargets
{
	bool LeadsToReceiver(const FQuest& Quest)
	{
		return Quest.State == EQuestState::Completed;
	}

	AActor* FindTargetActor(UWorld* World, const FQuest& Quest)
	{
		if (!World || Quest.MapMarkerTag.IsNone() || !UQuestComponent::IsInActivePhase(Quest))
		{
			return nullptr;
		}

		// Подготовительный шаг (11.10.2026): на шаге покупки задание ведёт к торговцу, у
		// которого этот предмет есть в продаже; на шаге «поставь в слот» вести некуда.
		if (UQuestComponent::IsPrepStepShown(Quest))
		{
			if (Quest.PrepStep == EQuestPrepStep::Buy)
			{
				for (TActorIterator<AMasterTrader> It(World); It; ++It)
				{
					for (const FShopEntry& Entry : It->GetCatalog())
					{
						if (Entry.ItemRow == Quest.PrepItemRow)
						{
							return *It;
						}
					}
				}
			}
			return nullptr;
		}

		if (LeadsToReceiver(Quest))
		{
			if (!Quest.TurnInNpcTag.IsNone())
			{
				// Задание сдаётся конкретному собеседнику (ноутбук — учёному, рация — охраннику).
				for (TActorIterator<ATalkerNPC> It(World); It; ++It)
				{
					if (It->GetNpcTag() == Quest.TurnInNpcTag)
					{
						return *It;
					}
				}
				return nullptr;
			}
			// Сдать тому, кто выдал: это староста (единственный AElderNPC на карте).
			TActorIterator<AElderNPC> ElderIt(World);
			return ElderIt ? *ElderIt : nullptr;
		}

		// Цель: база врагов с совпадающим QuestMarkerTag (BP_WolfDen/BP_BanditBase)…
		for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
		{
			if (It->GetQuestMarkerTag() == Quest.MapMarkerTag)
			{
				return *It;
			}
		}
		// …или ЛЮБОЙ актор со стандартным Actor Tag (гибкость для будущих квестов без правок кода).
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->ActorHasTag(Quest.MapMarkerTag))
			{
				return *It;
			}
		}
		return nullptr;
	}

	const FQuest* PickMarkerQuest(const TArray<FQuest>& Journal, const FQuest* Tracked, EQuestKind Kind)
	{
		if (Tracked && Tracked->Kind == Kind && UQuestComponent::IsInActivePhase(*Tracked))
		{
			return Tracked;
		}
		for (const FQuest& Quest : Journal)
		{
			if (Quest.Kind == Kind && UQuestComponent::IsInActivePhase(Quest))
			{
				return &Quest;
			}
		}
		return nullptr;
	}

	float GetMarkerZOffset(const FQuest& Quest, const AActor* Target, float DefaultZOffset)
	{
		// К сдаче метка висит над собеседником ВЫШЕ его зелёного ромба (чтобы не сливались).
		if (LeadsToReceiver(Quest))
		{
			if (const IInteractableNPCInterface* NPC = Cast<IInteractableNPCInterface>(Target))
			{
				return NPC->GetNPCMarkerZOffset() + 120.0f;
			}
		}
		return DefaultZOffset;
	}

	FString BuildMarkerLabel(const FQuest& Quest, const FString& TurnInPrefix)
	{
		if (LeadsToReceiver(Quest))
		{
			return FString::Printf(TEXT("%s%s"), *TurnInPrefix, *Quest.Title.ToString());
		}
		if (UQuestComponent::IsPrepStepShown(Quest))
		{
			return UQuestComponent::GetPrepObjectiveText(Quest).ToString(); // «Купи пистолет у торговца»
		}
		// Текст метки = ПЕРВАЯ невыполненная цель квеста (фидбек Рината 07-06), меняется по
		// ходу: «Перебить бандитов (1/3)» -> «Забрать ноутбук». Порядок целей kill -> item —
		// тот же, что в строке задания. Подписи берутся из данных квеста (FQuest::*ObjectiveLabel).
		if (Quest.TargetCount > 0 && Quest.Progress < Quest.TargetCount)
		{
			const FString Obj = !Quest.KillObjectiveLabel.IsEmpty()
				? Quest.KillObjectiveLabel.ToString() : Quest.KillTargetTag.ToString();
			return FString::Printf(TEXT("%s (%d/%d)"), *Obj, Quest.Progress, Quest.TargetCount);
		}
		if (Quest.RequiredItemCount > 0 && Quest.ItemProgress < Quest.RequiredItemCount)
		{
			const FString Obj = !Quest.ItemObjectiveLabel.IsEmpty()
				? Quest.ItemObjectiveLabel.ToString() : Quest.RequiredItemName;
			// Счётчик (x/N) у единичной цели («Забрать ноутбук») — шум, не показываем.
			return (Quest.RequiredItemCount > 1)
				? FString::Printf(TEXT("%s (%d/%d)"), *Obj, Quest.ItemProgress, Quest.RequiredItemCount)
				: Obj;
		}
		return Quest.Title.ToString(); // целей с подписью нет — название квеста
	}

	FText ResolveNpcName(UWorld* World, FName NpcTag)
	{
		if (!World)
		{
			return FText::GetEmpty();
		}
		if (NpcTag.IsNone())
		{
			TActorIterator<AElderNPC> ElderIt(World);
			return ElderIt ? ElderIt->GetNPCMarkerLabel() : FText::GetEmpty();
		}
		for (TActorIterator<ATalkerNPC> It(World); It; ++It)
		{
			if (It->GetNpcTag() == NpcTag)
			{
				return It->GetNPCMarkerLabel();
			}
		}
		return FText::GetEmpty();
	}

	FText ResolveGiverName(UWorld* World, const FQuest& Quest)
	{
		return ResolveNpcName(World, Quest.GiverNpcTag);
	}

	FText ResolveReceiverName(UWorld* World, const FQuest& Quest)
	{
		// Пустая метка получателя — «сдать тому, кто выдал».
		return ResolveNpcName(World, Quest.TurnInNpcTag.IsNone() ? Quest.GiverNpcTag : Quest.TurnInNpcTag);
	}
}
