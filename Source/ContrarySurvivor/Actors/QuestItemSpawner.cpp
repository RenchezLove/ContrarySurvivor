// Fill out your copyright notice in the Description page of Project Settings.

#include "QuestItemSpawner.h"
#include "Pickup.h"
#include "Components/SceneComponent.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h" // служебный ключ предмета по строке таблицы
#include "ContrarySurvivor/ContrarySurvivor.h"         // LogQA
#include "UInventoryComponent.h"
#include "AMasterInventoryItem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator: герой ищется по классу

AQuestItemSpawner::AQuestItemSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PickupClass = APickup::StaticClass();
}

void AQuestItemSpawner::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Метка цели — обычной меткой актора: метка задания на экране ищет цель именно по ней.
	if (!QuestMarkerTag.IsNone())
	{
		Tags.AddUnique(QuestMarkerTag);
	}
}

void AQuestItemSpawner::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		World->GetTimerManager().SetTimer(CheckTimer, this, &AQuestItemSpawner::RefreshNow,
			FMath::Max(0.1f, CheckInterval), /*bLoop=*/true);
	}
}

bool AQuestItemSpawner::ShouldItemLie(bool bQuestInJournal, EQuestState QuestState, bool bPlayerHasItem,
	bool bOnlyWhileQuestTaken)
{
	if (bPlayerHasItem)
	{
		return false; // предмет уже у героя
	}
	if (bQuestInJournal && QuestState == EQuestState::TurnedIn)
	{
		return false; // задание сдано — предмет больше не нужен
	}
	if (bOnlyWhileQuestTaken)
	{
		return bQuestInJournal
			&& (QuestState == EQuestState::Active || QuestState == EQuestState::Completed);
	}
	return true;
}

bool AQuestItemSpawner::PlayerHasItem(const APlayerCharacter* Player) const
{
	const UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
	if (!Inv)
	{
		return false;
	}
	// Предмет узнаём и по строке таблицы, и по служебному ключу (у восстановленного из
	// старого сохранения предмета строки может не быть).
	const FContraryItemRow* Row = ContraryItems::FindRow(ItemRow);
	const FString Key = Row ? Row->GetEffectiveKey(ItemRow) : ItemRow.ToString();
	for (const AMasterInventoryItem* Item : Inv->GetInventoryItems())
	{
		if (Item && (Item->SourceItemRow == ItemRow || Item->ItemName.Equals(Key, ESearchCase::CaseSensitive)))
		{
			return true;
		}
	}
	return false;
}

void AQuestItemSpawner::RefreshNow()
{
	UWorld* World = GetWorld();
	const APlayerCharacter* Player = nullptr;
	if (World)
	{
		for (TActorIterator<APlayerCharacter> It(World); It; ++It)
		{
			Player = *It;
			break;
		}
	}
	const UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Quests || ItemRow.IsNone())
	{
		return; // героя ещё нет — решать рано
	}

	const FQuest* Quest = Quests->FindQuest(QuestId);
	const bool bShould = ShouldItemLie(Quest != nullptr, Quest ? Quest->State : EQuestState::NotStarted,
		PlayerHasItem(Player), bOnlyWhileQuestTaken);

	APickup* Live = SpawnedPickup.Get();
	if (IsValid(Live) && !Live->HasLoot())
	{
		Live = nullptr; // пустой мешок исчезает сам — лежащим его не считаем
	}

	if (bShould && !Live)
	{
		// Строки предмета нет в таблице — класть нечего; говорим об этом один раз.
		if (!ContraryItems::FindRow(ItemRow))
		{
			if (!bWarnedMissingRow)
			{
				bWarnedMissingRow = true;
				UE_LOG(LogQA, Warning,
					TEXT("QA: quest item '%s' положить нельзя (%s): такой строки нет в таблице предметов"),
					*ItemRow.ToString(), *GetName());
			}
			return;
		}
		SpawnPickup();
	}
	else if (!bShould && Live)
	{
		Live->Destroy();
		SpawnedPickup.Reset();
		UE_LOG(LogQA, Display, TEXT("QA: quest item '%s' убран с места (%s)"), *ItemRow.ToString(), *GetName());
	}
}

void AQuestItemSpawner::SpawnPickup()
{
	UWorld* World = GetWorld();
	UClass* Class = PickupClass ? PickupClass.Get() : APickup::StaticClass();
	if (!World)
	{
		return;
	}

	const FTransform SpawnTM(GetActorRotation(), GetActorLocation());
	APickup* Pickup = World->SpawnActorDeferred<APickup>(Class, SpawnTM, /*Owner=*/nullptr,
		/*Instigator=*/nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return;
	}

	// Содержимое — списком до появления мешка (штатный путь мешков с набором предметов);
	// денег в мешке нет, даже если они заданы в классе мешка.
	FPlacedLootEntry Entry;
	Entry.ItemRow = ItemRow;
	Entry.Count = FMath::Max(1, ItemCount);
	Pickup->SetPlacedLootList({ Entry }, /*MoneyOverride=*/0.0f);
	UGameplayStatics::FinishSpawningActor(Pickup, SpawnTM);

	if (!IsValid(Pickup) || !Pickup->HasLoot())
	{
		// Строки предмета нет в таблице — пустой мешок не оставляем и говорим об этом.
		UE_LOG(LogQA, Warning,
			TEXT("QA: quest item '%s' положить не удалось (%s): проверьте строку в таблице предметов"),
			*ItemRow.ToString(), *GetName());
		if (IsValid(Pickup))
		{
			Pickup->Destroy();
		}
		return;
	}

	SpawnedPickup = Pickup;
	UE_LOG(LogQA, Display, TEXT("QA: quest item '%s' положен на место (%s)"), *ItemRow.ToString(), *GetName());
}
