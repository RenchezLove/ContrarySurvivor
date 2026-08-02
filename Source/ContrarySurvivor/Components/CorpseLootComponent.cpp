// Fill out your copyright notice in the Description page of Project Settings.

#include "CorpseLootComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AMasterInventoryItem.h"

TArray<TWeakObjectPtr<UCorpseLootComponent>> UCorpseLootComponent::SearchableCorpses;

UCorpseLootComponent::UCorpseLootComponent()
{
	// Контейнер реактивен (Init/Take), тикать нечему.
	PrimaryComponentTick.bCanEverTick = false;
}

void UCorpseLootComponent::InitLoot(float InMoney, const TArray<AMasterInventoryItem*>& InItems,
	bool bRegisterSearchable)
{
	// Замещающая укладка (труп): начинаем с чистого контейнера.
	Money = 0.0f;
	Items.Reset();
	AddLoot(InMoney, InItems, bRegisterSearchable);
}

void UCorpseLootComponent::AddLoot(float InMoney, const TArray<AMasterInventoryItem*>& InItems,
	bool bRegisterSearchable)
{
	Money = FMath::Max(0.0f, Money + InMoney);

	for (AMasterInventoryItem* Item : InItems)
	{
		if (IsValid(Item))
		{
			Items.AddUnique(Item);
		}
	}

	// Регистрация в реестре обыскиваемых + попутная чистка отмерших слабых ссылок
	// (трупы исчезают по таймеру — не копим пустые записи между волнами врагов).
	// Пикап регистрации не просит: его контроллер находит перебором акторов APickup.
	if (bRegisterSearchable)
	{
		SearchableCorpses.RemoveAll([](const TWeakObjectPtr<UCorpseLootComponent>& Ptr)
		{
			return !Ptr.IsValid();
		});
		SearchableCorpses.AddUnique(this);
	}

	if (InMoney > 0.0f || InItems.Num() > 0)
	{
		UE_LOG(LogQA, Display, TEXT("QA: CORPSE loot init on '%s' - money=%.0f items=%d"),
			*GetNameSafe(GetOwner()), Money, Items.Num());
	}
}

bool UCorpseLootComponent::HasLoot() const
{
	if (Money > 0.0f)
	{
		return true;
	}
	for (const TObjectPtr<AMasterInventoryItem>& Item : Items)
	{
		if (IsValid(Item))
		{
			return true;
		}
	}
	return false;
}

TArray<AMasterInventoryItem*> UCorpseLootComponent::GetLootItems() const
{
	TArray<AMasterInventoryItem*> Result;
	for (const TObjectPtr<AMasterInventoryItem>& Item : Items)
	{
		if (IsValid(Item))
		{
			Result.Add(Item);
		}
	}
	return Result;
}

float UCorpseLootComponent::TakeMoney()
{
	const float Taken = Money;
	Money = 0.0f;
	if (Taken > 0.0f)
	{
		UE_LOG(LogQA, Display, TEXT("QA: CORPSE take money %.0f from '%s' (left: items=%d)"),
			Taken, *GetNameSafe(GetOwner()), GetLootItems().Num());

		// Оповещение — ПОСЛЕДНИМ действием: слушатель (мешок-пикап) вправе уничтожить
		// владельца прямо здесь, после броадкаста мы к своим полям уже не обращаемся.
		OnLootChanged.Broadcast();
	}
	return Taken;
}

bool UCorpseLootComponent::TakeItem(AMasterInventoryItem* Item)
{
	if (!Item || Items.Remove(Item) == 0)
	{
		return false;
	}
	UE_LOG(LogQA, Display, TEXT("QA: CORPSE take item '%s' from '%s' (left: money=%.0f items=%d)"),
		*Item->ItemName, *GetNameSafe(GetOwner()), Money, GetLootItems().Num());

	// Оповещение — ПОСЛЕДНИМ действием (см. TakeMoney): опустевший мешок-пикап
	// уничтожает себя прямо в обработчике, поэтому после броадкаста ничего не трогаем.
	OnLootChanged.Broadcast();
	return true;
}

void UCorpseLootComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SearchableCorpses.Remove(this);

	// Не забранные предметы уничтожаем вместе с трупом — скрытые акторы-данные не должны
	// висеть в мире без носителя (тот же принцип, что фолбэк в AEnemyCharacter::DropLoot).
	for (const TObjectPtr<AMasterInventoryItem>& Item : Items)
	{
		if (IsValid(Item))
		{
			Item->Destroy();
		}
	}
	Items.Reset();

	Super::EndPlay(EndPlayReason);
}
