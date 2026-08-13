// Fill out your copyright notice in the Description page of Project Settings.

#include "CorpseLootComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AMasterInventoryItem.h"
#include "Components/SkeletalMeshComponent.h" // рэгдолл уходящего в землю тела
#include "GameFramework/Character.h"
#include "PhysicsEngine/BodyInstance.h"

TArray<TWeakObjectPtr<UCorpseLootComponent>> UCorpseLootComponent::SearchableCorpses;

UCorpseLootComponent::UCorpseLootComponent()
{
	// Контейнер реактивен (Init/Take) — тик нужен ТОЛЬКО пока обысканное тело уходит в
	// землю (п.3.2 задания издателя), поэтому со старта он выключен и включается в
	// StartSearchedSink.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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

		// Забрали всё — тело обыскано, и теперь уходит в землю (п.3.2). Проверка идёт ДО
		// броадкаста: после него владельца может уже не быть.
		if (!HasLoot())
		{
			StartSearchedSink();
		}

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

	// Забрали последнее — тело обыскано и уходит в землю (п.3.2), см. TakeMoney.
	if (!HasLoot())
	{
		StartSearchedSink();
	}

	// Оповещение — ПОСЛЕДНИМ действием (см. TakeMoney): опустевший мешок-пикап
	// уничтожает себя прямо в обработчике, поэтому после броадкаста ничего не трогаем.
	OnLootChanged.Broadcast();
	return true;
}

TArray<UCorpseLootComponent*> UCorpseLootComponent::CollectSearchableGroup(
	const AActor* AnchorActor, float GroupRadius)
{
	// Задание издателя п.3.1: одно нажатие «Обыскать» забирает содержимое ВСЕХ необысканных
	// тел рядом. «Рядом» считается от подсвеченного тела (якоря) и ровно один раз — цепочка
	// «от тела к телу» запрещена решением game-lead.
	TArray<UCorpseLootComponent*> Group;
	if (!IsValid(AnchorActor))
	{
		return Group;
	}

	const UWorld* World = AnchorActor->GetWorld();
	const FVector AnchorLoc = AnchorActor->GetActorLocation();
	const float Radius = FMath::Max(0.0f, GroupRadius);
	const float RadiusSq = Radius * Radius;

	for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : SearchableCorpses)
	{
		UCorpseLootComponent* Corpse = Ptr.Get();
		AActor* CorpseOwner = Corpse ? Corpse->GetOwner() : nullptr;
		if (!Corpse || !IsValid(CorpseOwner) || Corpse->GetWorld() != World || !Corpse->HasLoot())
		{
			continue;
		}
		if (FVector::DistSquared(AnchorLoc, CorpseOwner->GetActorLocation()) > RadiusSq)
		{
			continue;
		}

		// Якорь — первым: окно открывается тем телом, на которое смотрел игрок.
		if (CorpseOwner == AnchorActor)
		{
			Group.Insert(Corpse, 0);
		}
		else
		{
			Group.Add(Corpse);
		}
	}
	return Group;
}

float UCorpseLootComponent::GetSinkDepthAtTime(float Elapsed, float Delay, float Duration, float Depth)
{
	// Пауза после обыска: тело ещё лежит на месте.
	if (Elapsed <= Delay)
	{
		return 0.0f;
	}
	// Нулевая длительность — тело проваливается сразу (защита от настройки «0 секунд»).
	if (Duration <= 0.0f)
	{
		return Depth;
	}
	return Depth * FMath::Clamp((Elapsed - Delay) / Duration, 0.0f, 1.0f);
}

void UCorpseLootComponent::StartSearchedSink()
{
	AActor* CorpseOwner = GetOwner();
	if (bSinking || !bSinkWhenSearched || !IsValid(CorpseOwner))
	{
		return;
	}

	bSinking = true;
	SinkElapsed = 0.0f;
	SinkAppliedDepth = 0.0f;

	// Тело, упавшее рэгдоллом, за актором НЕ едет: его физические тела стоят в мировых
	// координатах (USkeletalMeshComponent::OnUpdateTransform двигает только кинематические).
	// Такому мешу двигаем сами тела и снимаем гравитацию, иначе между переносами оно
	// продолжит падать своим ходом.
	if (ACharacter* CorpseCharacter = Cast<ACharacter>(CorpseOwner))
	{
		USkeletalMeshComponent* SkelMesh = CorpseCharacter->GetMesh();
		if (SkelMesh && SkelMesh->IsSimulatingPhysics())
		{
			SkelMesh->SetEnableGravity(false);
			SinkRagdollMesh = SkelMesh;
		}
	}

	SetComponentTickEnabled(true);
	UE_LOG(LogQA, Display, TEXT("QA: CORPSE searched -> sink started on '%s' (пауза %.1f с, спуск %.1f с, глубина %.0f см)"),
		*GetNameSafe(CorpseOwner), SearchedSinkDelay, SearchedSinkDuration, SearchedSinkDepth);
}

void UCorpseLootComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* CorpseOwner = GetOwner();
	if (!bSinking || !IsValid(CorpseOwner))
	{
		return;
	}

	SinkElapsed += DeltaTime;

	// Шаг = разница пройденного, а не «скорость × кадр»: просадка кадров не сдвигает итог.
	const float Depth = GetSinkDepthAtTime(SinkElapsed, SearchedSinkDelay, SearchedSinkDuration, SearchedSinkDepth);
	const float Step = Depth - SinkAppliedDepth;
	if (Step > 0.0f)
	{
		const FVector Down(0.0f, 0.0f, -Step);
		CorpseOwner->SetActorLocation(CorpseOwner->GetActorLocation() + Down,
			/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

		if (USkeletalMeshComponent* Ragdoll = SinkRagdollMesh.Get())
		{
			if (const FBodyInstance* RootBody = Ragdoll->GetBodyInstance())
			{
				Ragdoll->SetAllPhysicsPosition(RootBody->GetUnrealWorldTransform().GetLocation() + Down);
			}
		}
		SinkAppliedDepth = Depth;
	}

	if (SinkElapsed >= SearchedSinkDelay + SearchedSinkDuration)
	{
		FinishSearchedSink();
	}
}

void UCorpseLootComponent::FinishSearchedSink()
{
	AActor* CorpseOwner = GetOwner();
	if (!IsValid(CorpseOwner))
	{
		return;
	}

	// Привязанное к телу движок вместе с телом НЕ убирает (потому у оружия и был свой
	// таймер, AEnemyCharacter::HandleDeath): без этого после ухода тела в землю на его
	// месте остался бы висеть в воздухе пистолет.
	TArray<AActor*> AttachedActors;
	CorpseOwner->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		if (IsValid(Attached))
		{
			Attached->Destroy();
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: CORPSE sink finished -> '%s' removed (attached: %d)"),
		*GetNameSafe(CorpseOwner), AttachedActors.Num());
	CorpseOwner->Destroy();
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
