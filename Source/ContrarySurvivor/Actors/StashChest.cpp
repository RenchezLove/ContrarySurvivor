// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Actors/StashChest.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "AMasterInventoryItem.h"
#include "AConsumableItem.h"
#include "ARangedWeapon.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------------------
// Общее хранилище
// ---------------------------------------------------------------------------

APersonalStashStorage::APersonalStashStorage()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Container = CreateDefaultSubobject<UCorpseLootComponent>(TEXT("StashContainer"));
	Container->bStandaloneStash = true;   // своё окно, в групповой обыск не входит
	Container->bSinkWhenSearched = false; // опустевшее хранилище никуда не исчезает
	Container->bMergeStacksOnAdd = true;  // положенные стопки сливаются, как в рюкзаке
	Container->SearchTitle = NSLOCTEXT("StashChest", "SearchTitle", "Личный сундук");
	Container->SearchObjectName = NSLOCTEXT("StashChest", "SearchObjectName", "Сундук");
}

APersonalStashStorage* APersonalStashStorage::Get(UWorld* World, bool bCreateIfMissing)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<APersonalStashStorage> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	if (!bCreateIfMissing)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APersonalStashStorage>(APersonalStashStorage::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

FSavedInventoryEntry APersonalStashStorage::MakeSaveEntry(const AMasterInventoryItem& Item)
{
	// Те же поля, что пишет рюкзак (APlayerCharacter::SaveGame).
	FSavedInventoryEntry Entry;
	Entry.ClassPath = Item.GetClass()->GetPathName();
	Entry.ItemName = Item.ItemName;
	Entry.ItemRow = Item.SourceItemRow;
	Entry.ItemDisplayText = Item.ItemDisplayText;
	Entry.StackCount = Item.GetStackCount();
	Entry.bEquipped = false;
	if (const AConsumableItem* Cons = Cast<const AConsumableItem>(&Item))
	{
		Entry.ConsumableType = Cons->ConsumableType;
	}
	// Запас при стволе пишется как есть (то же поле, что у рюкзака).
	if (const ARangedWeapon* Ranged = Cast<const ARangedWeapon>(&Item))
	{
		Entry.WeaponReserveAmmo = FMath::Max(0, Ranged->GetCurrentAmmoReserve());
	}
	return Entry;
}

AMasterInventoryItem* APersonalStashStorage::SpawnFromSaveEntry(UWorld* World, const FSavedInventoryEntry& Entry,
	const FTransform& Transform, AActor* ItemOwner)
{
	UClass* ItemClass = StaticLoadClass(AMasterInventoryItem::StaticClass(), nullptr, *Entry.ClassPath);
	if (!ItemClass)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: STASH - класс предмета '%s' не найден, запись сундука пропущена"),
			*Entry.ClassPath);
		return nullptr;
	}

	// ADR-088: всё записанное передаётся ДО появления предмета, строку таблицы он накладывает сам.
	AMasterInventoryItem* Item = ContraryItems::SpawnItem(World, ItemClass, Transform, ItemOwner,
		[&Entry](AMasterInventoryItem& It)
		{
			It.SourceItemRow = Entry.ItemRow;
			if (!Entry.ItemName.IsEmpty())
			{
				It.ItemName = Entry.ItemName;
			}
			if (!Entry.ItemDisplayText.IsEmpty())
			{
				It.ItemDisplayText = Entry.ItemDisplayText;
			}
			It.StackCount = FMath::Max(0, Entry.StackCount);
			if (AConsumableItem* Cons = Cast<AConsumableItem>(&It))
			{
				Cons->ConsumableType = Entry.ConsumableType;
			}
		});
	if (!Item)
	{
		return nullptr;
	}

	// Предмет хранилища — данные, не объект сцены.
	Item->SetActorHiddenInGame(true);
	Item->SetActorEnableCollision(false);

	// Заново созданный ствол несёт запас своего класса — его выбрасываем, иначе сундук стал бы
	// источником бесплатных патронов при каждой загрузке. Возвращаем ровно то, что было при
	// стволе в момент записи (обычно ноль: запас игрок получил пачкой, когда брал ствол).
	if (ARangedWeapon* Ranged = Cast<ARangedWeapon>(Item))
	{
		Ranged->DrainReserveAmmo();
		if (Entry.WeaponReserveAmmo > 0)
		{
			Ranged->AddReserveAmmo(Entry.WeaponReserveAmmo);
		}
	}
	return Item;
}

void APersonalStashStorage::WriteToSave(UWorld* World, UContrarySaveGame* Save)
{
	if (!Save)
	{
		return;
	}
	Save->StashEntries.Reset();
	const APersonalStashStorage* Storage = Get(World, /*bCreateIfMissing=*/false);
	if (!Storage || !Storage->Container)
	{
		return;
	}
	for (const AMasterInventoryItem* Item : Storage->Container->GetLootItems())
	{
		if (Item)
		{
			Save->StashEntries.Add(MakeSaveEntry(*Item));
		}
	}
}

void APersonalStashStorage::RestoreFromSave(UWorld* World, const UContrarySaveGame* Save)
{
	if (!World || !Save)
	{
		return;
	}
	// Пустое сохранение (или записанное до сундука) и хранилища ещё нет — создавать нечего.
	APersonalStashStorage* Storage = Get(World, /*bCreateIfMissing=*/Save->StashEntries.Num() > 0);
	if (!Storage || !Storage->Container)
	{
		return;
	}

	Storage->Container->ClearLoot();

	TArray<AMasterInventoryItem*> Items;
	for (const FSavedInventoryEntry& Entry : Save->StashEntries)
	{
		if (AMasterInventoryItem* Item = SpawnFromSaveEntry(World, Entry, FTransform::Identity, Storage))
		{
			Items.Add(Item);
		}
	}
	Storage->Container->AddLoot(0.0f, Items, /*bRegisterSearchable=*/false);

	UE_LOG(LogQA, Display, TEXT("QA: STASH - из сохранения восстановлено %d из %d записей сундука"),
		Items.Num(), Save->StashEntries.Num());
}

void APersonalStashStorage::ClearInWorld(UWorld* World)
{
	if (APersonalStashStorage* Storage = Get(World, /*bCreateIfMissing=*/false))
	{
		if (Storage->Container)
		{
			Storage->Container->ClearLoot();
		}
	}
}

// ---------------------------------------------------------------------------
// Сундук
// ---------------------------------------------------------------------------

TArray<TWeakObjectPtr<AStashChest>> AStashChest::ActiveChests;

AStashChest::AStashChest()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	RootComponent = SceneRoot;

	// Сетка сундука. Коллизия — как у кузова машины: блокирует всё, кроме камеры.
	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
	ChestMesh->SetupAttachment(SceneRoot);
	ChestMesh->SetMobility(EComponentMobility::Static);
	ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChestMesh->SetCollisionObjectType(ECC_WorldStatic);
	ChestMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ChestMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ChestMeshAsset(
		TEXT("/Game/Environment/Props/ScientistBase/SM_StashChest.SM_StashChest"));
	if (ChestMeshAsset.Succeeded())
	{
		ChestMesh->SetStaticMesh(ChestMeshAsset.Object);
	}

	// Зона вокруг сундука: только замечает игрока (усиление свечения), ничего не блокирует.
	InteractZone = CreateDefaultSubobject<USphereComponent>(TEXT("InteractZone"));
	InteractZone->SetupAttachment(SceneRoot);
	InteractZone->SetMobility(EComponentMobility::Static);
	InteractZone->InitSphereRadius(InteractRadius);
	InteractZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractZone->SetGenerateOverlapEvents(true);
	InteractZone->SetCanEverAffectNavigation(false);

	// Свет: неподвижный (запекается), без теней, тёплый и слабый.
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(SceneRoot);
	GlowLight->SetMobility(EComponentMobility::Static);
	GlowLight->SetCastShadows(false);
	GlowLight->SetRelativeLocation(LightOffset);
	GlowLight->SetIntensity(LightIntensity);
	GlowLight->SetLightColor(LightColor);
	GlowLight->SetAttenuationRadius(LightRadius);
}

void AStashChest::ApplySettings()
{
	if (ChestMesh)
	{
		ChestMesh->SetRelativeScale3D(FVector(FMath::Max(ChestScale, 0.1f)));
	}
	if (InteractZone)
	{
		InteractZone->SetSphereRadius(FMath::Max(InteractRadius, 50.0f));
	}
	if (GlowLight)
	{
		GlowLight->SetRelativeLocation(LightOffset);
		GlowLight->SetIntensity(LightIntensity);
		GlowLight->SetLightColor(LightColor);
		GlowLight->SetAttenuationRadius(LightRadius);
		GlowLight->SetVisibility(bLightEnabled);
	}
}

void AStashChest::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySettings();
}

void AStashChest::BeginPlay()
{
	Super::BeginPlay();

	ActiveChests.RemoveAll([](const TWeakObjectPtr<AStashChest>& Ptr) { return !Ptr.IsValid(); });
	ActiveChests.AddUnique(this);

	// Живые материалы — по одному на слот сетки; параметра свечения у материала может не
	// быть (материал ещё не готов) — тогда выставление просто ничего не меняет.
	if (ChestMesh && bGlowEnabled)
	{
		const int32 NumMaterials = ChestMesh->GetNumMaterials();
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			if (UMaterialInstanceDynamic* MID = ChestMesh->CreateAndSetMaterialInstanceDynamic(Index))
			{
				GlowMIDs.Add(MID);
			}
		}
	}
	ApplyGlow(bGlowEnabled ? GlowStrengthIdle : 0.0f);

	if (InteractZone)
	{
		InteractZone->OnComponentBeginOverlap.AddDynamic(this, &AStashChest::HandleZoneBeginOverlap);
		InteractZone->OnComponentEndOverlap.AddDynamic(this, &AStashChest::HandleZoneEndOverlap);
	}
}

void AStashChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveChests.Remove(this);
	Super::EndPlay(EndPlayReason);
}

UCorpseLootComponent* AStashChest::GetStashContainer() const
{
	const APersonalStashStorage* Storage = APersonalStashStorage::Get(GetWorld(), /*bCreateIfMissing=*/true);
	return Storage ? Storage->GetContainer() : nullptr;
}

bool AStashChest::IsWithinInteractRange(const FVector& Location) const
{
	const float Radius = FMath::Max(InteractRadius, 50.0f);
	return FVector::DistSquared2D(Location, GetActorLocation()) <= Radius * Radius;
}

void AStashChest::ApplyGlow(float Strength)
{
	CurrentGlowStrength = Strength;
	for (UMaterialInstanceDynamic* MID : GlowMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(GlowParamName, Strength);
		}
	}
}

void AStashChest::SetPlayerNearby(bool bNearby)
{
	if (!bGlowEnabled)
	{
		return;
	}
	ApplyGlow(bNearby ? GlowStrengthNear : GlowStrengthIdle);
}

void AStashChest::HandleZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<APlayerCharacter>(OtherActor))
	{
		SetPlayerNearby(true);
	}
}

void AStashChest::HandleZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Cast<APlayerCharacter>(OtherActor))
	{
		SetPlayerNearby(false);
	}
}
