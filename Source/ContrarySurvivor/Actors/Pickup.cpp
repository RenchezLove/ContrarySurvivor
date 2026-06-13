// Fill out your copyright notice in the Description page of Project Settings.

#include "Pickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "AMasterInventoryItem.h"
#include "UInventoryComponent.h"

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// Корень-сфера (маркер позиции лута). Подбор теперь по клавише E (контроллер ищет
	// ближайший пикап и зовёт Collect), а не по overlap — поэтому коллизию/оверлапы гасим.
	PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	SetRootComponent(PickupTrigger);
	PickupTrigger->InitSphereRadius(40.0f);
	PickupTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupTrigger->SetGenerateOverlapEvents(false);

	// Плейсхолдер-визуал (без коллизии). Меш мелкий, чтобы читался как «лут на земле».
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(PickupTrigger);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
}

void APickup::InitLoot(float Money, AMasterInventoryItem* InCarriedItem)
{
	MoneyAmount = Money;
	CarriedItem = InCarriedItem;
}

bool APickup::HasLoot() const
{
	return MoneyAmount > 0.0f || IsValid(CarriedItem);
}

bool APickup::Collect(APlayerCharacter* Player)
{
	if (!IsValid(Player))
	{
		return false;
	}

	UStatsComponent* Stats = Player->GetStats();
	UInventoryComponent* Inv = Player->GetInventory();

	// Деньги -> в статы. Считаем «начислено», только если реально добавили (или денег нет).
	bool bMoneyDone = (MoneyAmount <= 0.0f);
	if (MoneyAmount > 0.0f && Stats)
	{
		Stats->AddMoney(MoneyAmount);
		UE_LOG(LogTemp, Log, TEXT("Pickup '%s': +%.0f money"), *GetName(), MoneyAmount);
		bMoneyDone = true;
	}

	// Предмет -> в рюкзак (предмет уже скрыт/без коллизии, как тестовые предметы).
	bool bItemDone = !IsValid(CarriedItem);
	if (IsValid(CarriedItem) && Inv)
	{
		Inv->AddItem(CarriedItem);
		UE_LOG(LogTemp, Log, TEXT("Pickup '%s': looted item %s"), *GetName(), *CarriedItem->GetName());
		CarriedItem = nullptr; // передан игроку, EndPlay его не уничтожит
		bItemDone = true;
	}

	// BUG2-фикс: НЕ уничтожаем пикап, если что-то из лута не удалось начислить — иначе деньги/
	// предмет «терялись». Пикап остаётся на земле; игрок может нажать E ещё раз.
	if (!bMoneyDone || !bItemDone)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pickup '%s': collect partial (money %s, item %s) — kept on ground"),
			*GetName(), bMoneyDone ? TEXT("ok") : TEXT("FAIL"), bItemDone ? TEXT("ok") : TEXT("FAIL"));
		return false;
	}

	bCollected = true;
	Destroy();
	return true;
}

void APickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Если пикап исчез (например, по таймеру/смене уровня) НЕ будучи подобранным —
	// уничтожаем висящий «при себе» предмет, чтобы он не утёк в мире.
	if (!bCollected && IsValid(CarriedItem))
	{
		CarriedItem->Destroy();
		CarriedItem = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

APickup* APickup::DropLoot(UWorld* World, const FVector& Location, float MoneyAmount,
	TSubclassOf<AMasterInventoryItem> ItemClass, float ItemDropChance,
	TSubclassOf<APickup> PickupClass)
{
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<APickup> SpawnClass = PickupClass;
	if (!SpawnClass)
	{
		SpawnClass = APickup::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Шанс выпадения предмета (изношенное оружие/расходник — GDD §7.8).
	AMasterInventoryItem* DroppedItem = nullptr;
	if (ItemClass && FMath::FRand() <= ItemDropChance)
	{
		DroppedItem = World->SpawnActor<AMasterInventoryItem>(
			ItemClass, Location, FRotator::ZeroRotator, SpawnParams);
		if (DroppedItem)
		{
			// Предмет лута — данные рюкзака, не объект на сцене: прячем визуал/коллизию.
			DroppedItem->SetActorHiddenInGame(true);
			DroppedItem->SetActorEnableCollision(false);
		}
	}

	// Деньги/предмет несёт один пикап (общий overlap отдаёт оба).
	APickup* Pickup = World->SpawnActor<APickup>(
		SpawnClass, Location, FRotator::ZeroRotator, SpawnParams);
	if (Pickup)
	{
		Pickup->InitLoot(MoneyAmount, DroppedItem);
	}
	else if (DroppedItem)
	{
		// Пикап не заспавнился — не оставляем висящий предмет.
		DroppedItem->Destroy();
	}

	return Pickup;
}
