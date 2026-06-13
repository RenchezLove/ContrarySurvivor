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

	// Триггер подбора — корень. Overlap только по Pawn (игрок), без блокировки движения.
	PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	SetRootComponent(PickupTrigger);
	PickupTrigger->InitSphereRadius(PickupRadius);
	PickupTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupTrigger->SetGenerateOverlapEvents(true);
	PickupTrigger->OnComponentBeginOverlap.AddDynamic(this, &APickup::OnPickupBeginOverlap);

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

void APickup::OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return; // лут подбирает только игрок
	}

	// Деньги -> в статы.
	if (MoneyAmount > 0.0f)
	{
		if (UStatsComponent* Stats = Player->GetStats())
		{
			Stats->AddMoney(MoneyAmount);
			UE_LOG(LogTemp, Log, TEXT("Pickup: +%.0f money"), MoneyAmount);
		}
	}

	// Предмет -> в рюкзак (предмет уже скрыт/без коллизии, как тестовые предметы).
	if (IsValid(CarriedItem))
	{
		if (UInventoryComponent* Inv = Player->GetInventory())
		{
			Inv->AddItem(CarriedItem);
			UE_LOG(LogTemp, Log, TEXT("Pickup: looted item %s"), *CarriedItem->GetName());
			CarriedItem = nullptr; // передан игроку, EndPlay его не уничтожит
		}
	}

	bCollected = true;
	Destroy();
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
