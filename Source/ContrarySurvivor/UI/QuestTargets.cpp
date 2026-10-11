// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/QuestTargets.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
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
