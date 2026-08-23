// Fill out your copyright notice in the Description page of Project Settings.

#include "CorpseLootComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Actors/Pickup.h"    // ADR-076 п.2: мешки в групповом обыске (реестр пикапов)
#include "AMasterInventoryItem.h"
#include "Components/MeshComponent.h"         // растворение: подмена материалов всех мешей тела
#include "Components/SkeletalMeshComponent.h" // рэгдолл уходящего в землю тела
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h" // растворение: живой материал с параметрами
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodyInstance.h"

TArray<TWeakObjectPtr<UCorpseLootComponent>> UCorpseLootComponent::SearchableCorpses;

UCorpseLootComponent::UCorpseLootComponent()
{
	// Контейнер реактивен (Init/Take) — тик нужен ТОЛЬКО пока обысканное тело убирается
	// из мира (растворение п.5 Рината / уход в землю п.3.2 издателя), поэтому со старта
	// он выключен и включается в StartSearchedSink.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Мягкая ссылка (не FObjectFinder): ассет создаёт оператор, и до его появления
	// конструктор не должен ни падать, ни сыпать ошибками — LoadSynchronous в
	// StartSearchedDissolve честно вернёт null, и тело уйдёт в землю по-старому.
	DissolveMaterial = FSoftObjectPath(
		TEXT("/Game/Characters/Shared/M_CorpseDissolve.M_CorpseDissolve"));
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

void UCorpseLootComponent::ClearLoot()
{
	// Содержимое выбрасывается, а не переходит игроку: не забранные скрытые акторы-данные
	// уничтожаем, как при исчезновении трупа (EndPlay) — без носителя им в мире не место.
	for (const TObjectPtr<AMasterInventoryItem>& Item : Items)
	{
		if (IsValid(Item))
		{
			Item->Destroy();
		}
	}
	Items.Reset();
	Money = 0.0f;
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
	// тел рядом. «Рядом» считается от подсвеченного якоря (тело ИЛИ мешок — ADR-076 п.2) и
	// ровно один раз — цепочка «от тела к телу» запрещена решением game-lead.
	TArray<UCorpseLootComponent*> Group;
	if (!IsValid(AnchorActor))
	{
		return Group;
	}

	// Якорь — ОТДЕЛЬНОЕ ХРАНИЛИЩЕ (задел под базы): группу не собирает, своё окно один на один.
	if (const UCorpseLootComponent* AnchorContainer = AnchorActor->FindComponentByClass<UCorpseLootComponent>())
	{
		if (AnchorContainer->bStandaloneStash)
		{
			Group.Add(const_cast<UCorpseLootComponent*>(AnchorContainer));
			return Group;
		}
	}

	const UWorld* World = AnchorActor->GetWorld();
	const FVector AnchorLoc = AnchorActor->GetActorLocation();
	const float Radius = FMath::Max(0.0f, GroupRadius);
	const float RadiusSq = Radius * Radius;

	// Общая проверка члена группы: живой, тот же мир, с лутом, в радиусе. «Отдельные
	// хранилища» в группу не входят, КРОМЕ хранилищ БАЗ (StashLevel > 0): Report1 баг 1 +
	// решение лида 24.08 (вариант А по ADR-077 п.14 «в перечне обыска название базы
	// выделяется отдельно от трупов») — награда зачищенной базы уходит в ТО ЖЕ окно, что
	// трупы вокруг, и база обыскивается сразу по смерти последнего врага, а не после того,
	// как труп у пивота исчезнет или будет опустошён. Машина (StashLevel = 0) остаётся со
	// своим окном. Якорь встаёт первым — окно открывается тем, на что смотрел игрок.
	auto TryAddMember = [&](UCorpseLootComponent* Container)
	{
		AActor* ContainerOwner = Container ? Container->GetOwner() : nullptr;
		if (!Container || !IsValid(ContainerOwner) || Container->GetWorld() != World
			|| !Container->HasLoot()
			|| (Container->bStandaloneStash && Container->StashLevel <= 0))
		{
			return;
		}
		if (FVector::DistSquared(AnchorLoc, ContainerOwner->GetActorLocation()) > RadiusSq)
		{
			return;
		}
		if (ContainerOwner == AnchorActor)
		{
			Group.Insert(Container, 0);
		}
		else
		{
			Group.Add(Container);
		}
	};

	// Проход 1: тела из реестра обыскиваемых.
	for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : SearchableCorpses)
	{
		TryAddMember(Ptr.Get());
	}

	// Проход 2 (ADR-076 п.2): мешки-пикапы из реестра живых пикапов. Отдельным реестром,
	// НЕ через реестр тел — там мешок давал бы один объект двумя интерактивами (комментарий
	// класса). Кейс Рината: «в лагере два мешка, обыскиваются по очереди» — теперь одной группой.
	for (const TWeakObjectPtr<APickup>& Ptr : APickup::GetActivePickups())
	{
		if (const APickup* Pickup = Ptr.Get())
		{
			TryAddMember(Pickup->GetLootContainer());
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

float UCorpseLootComponent::GetDissolveOpacityAtTime(float Elapsed, float InstantTransparency,
	float Delay, float Duration)
{
	// Мгновенный сдвиг Рината («сразу на 35% прозрачнее») — стартовая непрозрачность.
	const float Base = 1.0f - FMath::Clamp(InstantTransparency, 0.0f, 1.0f);
	// Пауза: тело лежит полупрозрачно-серым, дальше не тает.
	if (Elapsed <= Delay)
	{
		return Base;
	}
	// Нулевая длительность — исчезает сразу после паузы (защита от настройки «0 секунд»).
	if (Duration <= 0.0f)
	{
		return 0.0f;
	}
	return Base * (1.0f - FMath::Clamp((Elapsed - Delay) / Duration, 0.0f, 1.0f));
}

bool UCorpseLootComponent::StartSearchedDissolve()
{
	AActor* CorpseOwner = GetOwner();

	UMaterialInterface* DissolveBase = DissolveMaterial.LoadSynchronous();
	if (!DissolveBase)
	{
		UE_LOG(LogQA, Warning,
			TEXT("QA: CORPSE dissolve: материал растворения '%s' не загрузился — тело '%s' уходит в землю по-старому"),
			*DissolveMaterial.ToString(), *GetNameSafe(CorpseOwner));
		return false;
	}

	// Один живой материал на все слоты: параметры общие, вершинный цвет каждый меш даёт свой.
	DissolveMID = UMaterialInstanceDynamic::Create(DissolveBase, this);
	DissolveMID->SetScalarParameterValue(TEXT("Opacity"),
		GetDissolveOpacityAtTime(0.0f, DissolveInstantTransparency, DissolveDelay, DissolveDuration));
	DissolveMID->SetScalarParameterValue(TEXT("Desaturation"),
		FMath::Clamp(DissolveInstantDesaturation, 0.0f, 1.0f));

	// Меши самого тела и всего привязанного к нему (оружие в руке бандита): привязанное
	// движок с телом не убирает, растворяться они должны вместе — как и удаляются вместе
	// в FinishSearchedSink.
	TArray<UMeshComponent*> Meshes;
	CorpseOwner->GetComponents<UMeshComponent>(Meshes);
	TArray<AActor*> AttachedActors;
	CorpseOwner->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		if (IsValid(Attached))
		{
			TArray<UMeshComponent*> AttachedMeshes;
			Attached->GetComponents<UMeshComponent>(AttachedMeshes);
			Meshes.Append(AttachedMeshes);
		}
	}

	int32 SlotCount = 0;
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
		{
			continue;
		}
		const int32 NumMaterials = Mesh->GetNumMaterials();
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			Mesh->SetMaterial(Index, DissolveMID);
			++SlotCount;
		}
	}

	bDissolving = true;
	DissolveElapsed = 0.0f;
	SetComponentTickEnabled(true);
	UE_LOG(LogQA, Display,
		TEXT("QA: CORPSE searched -> dissolve started on '%s' (мгновенно: прозрачность %.0f%%, серость %.0f%%; пауза %.1f с, растворение %.1f с; слотов материалов %d)"),
		*GetNameSafe(CorpseOwner), DissolveInstantTransparency * 100.0f,
		DissolveInstantDesaturation * 100.0f, DissolveDelay, DissolveDuration, SlotCount);
	return true;
}

void UCorpseLootComponent::StartSearchedSink()
{
	AActor* CorpseOwner = GetOwner();
	if (bSinking || bDissolving || !bSinkWhenSearched || !IsValid(CorpseOwner))
	{
		return;
	}

	// Основной путь — растворение (Ринат 23.08 п.5); не запустилось (нет материала) —
	// запасной уход в землю ниже.
	if (bDissolveWhenSearched && StartSearchedDissolve())
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
	if (!IsValid(CorpseOwner))
	{
		return;
	}

	// Растворение (Ринат 23.08 п.5): мгновенный сдвиг уже поставлен в StartSearchedDissolve,
	// здесь только плавное таяние после паузы и удаление в конце.
	if (bDissolving)
	{
		DissolveElapsed += DeltaTime;
		if (DissolveMID)
		{
			DissolveMID->SetScalarParameterValue(TEXT("Opacity"),
				GetDissolveOpacityAtTime(DissolveElapsed, DissolveInstantTransparency,
					DissolveDelay, DissolveDuration));
		}
		if (DissolveElapsed >= DissolveDelay + DissolveDuration)
		{
			FinishSearchedSink();
		}
		return;
	}

	if (!bSinking)
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

	UE_LOG(LogQA, Display, TEXT("QA: CORPSE %s finished -> '%s' removed (attached: %d)"),
		bDissolving ? TEXT("dissolve") : TEXT("sink"),
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
