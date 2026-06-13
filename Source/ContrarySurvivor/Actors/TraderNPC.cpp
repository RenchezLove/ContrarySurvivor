// Fill out your copyright notice in the Description page of Project Settings.

#include "TraderNPC.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "AMasterInventoryItem.h"
#include "AConsumableItem.h"
#include "AHeadArmor.h"
#include "ATorsoArmor.h"
#include "APantsArmor.h"
#include "APistol.h"
#include "AMeleeWeapon.h"

ATraderNPC::ATraderNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	// Триггер взаимодействия — корень. Overlap только по Pawn, не блокирует движение.
	InteractTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("InteractTrigger"));
	SetRootComponent(InteractTrigger);
	InteractTrigger->InitSphereRadius(InteractRadius);
	InteractTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractTrigger->SetGenerateOverlapEvents(true);
	InteractTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATraderNPC::OnInteractBeginOverlap);
	InteractTrigger->OnComponentEndOverlap.AddDynamic(this, &ATraderNPC::OnInteractEndOverlap);

	// Плейсхолдер-тело торговца (цилиндр движка), без коллизии (тело не препятствие в MVP).
	// КРУПНЕЕ и ВЫШЕ прежнего (был стаб ~ниже игрока, тонул в геометрии деревни и терялся):
	// высокий столб, стоящий примерно от земли (root спавнится на +90 над навмешем).
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(InteractTrigger);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(1.1f, 1.1f, 2.6f)); // цилиндр 100ед -> ~260ед в высоту
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f)); // низ ~у земли, верх заметно выше игрока
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CylMesh.Object);
	}

	// «Маяк»-шар над телом — яркая макушка для находимости издалека.
	BeaconComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	BeaconComponent->SetupAttachment(InteractTrigger);
	BeaconComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconComponent->SetRelativeScale3D(FVector(1.4f, 1.4f, 1.4f)); // сфера 100ед -> 140ед диаметр
	BeaconComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 240.0f)); // над верхушкой тела
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BeaconComponent->SetStaticMesh(SphereMesh.Object);
	}

	// Базовый материал плейсхолдера: BasicShapeMaterial — у него есть вектор-параметр "Color"
	// (подтверждено по ассету Engine/Content/BasicShapes/BasicShapeMaterial). В BeginPlay
	// из него создаётся dynamic instance и заливается ярким цветом.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMat.Succeeded())
	{
		PlaceholderBaseMaterial = BaseMat.Object;
	}

	BuildDefaultCatalog();
}

void ATraderNPC::BeginPlay()
{
	Super::BeginPlay();

	// Яркий плейсхолдер: dynamic material instance с вектор-параметром "Color" поверх тела и маяка.
	if (PlaceholderBaseMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PlaceholderBaseMaterial, this);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), PlaceholderColor);
			if (MeshComponent)
			{
				MeshComponent->SetMaterial(0, MID);
			}
			if (BeaconComponent)
			{
				BeaconComponent->SetMaterial(0, MID);
			}
		}
	}
}

void ATraderNPC::BuildDefaultCatalog()
{
	Catalog.Reset();

	// Хелпер для расходника.
	auto MakeConsumable = [](const FString& Name, float Price, EConsumableType Type)
	{
		FShopEntry E;
		E.DisplayName = Name;
		E.Price = Price;
		E.Kind = EShopEntryKind::Item;
		E.ItemClass = AConsumableItem::StaticClass();
		E.bApplyConsumableType = true;
		E.ConsumableType = Type;
		return E;
	};

	auto MakeItem = [](const FString& Name, float Price, TSubclassOf<AMasterInventoryItem> Cls)
	{
		FShopEntry E;
		E.DisplayName = Name;
		E.Price = Price;
		E.Kind = EShopEntryKind::Item;
		E.ItemClass = Cls;
		return E;
	};

	// --- Цены DRAFT по GDD §7.6 (на тюнинг) ---
	// Расходники: вода 5, еда/бинт 10-15.
	Catalog.Add(MakeConsumable(TEXT("Water Bottle"), 5.0f, EConsumableType::Water));
	Catalog.Add(MakeConsumable(TEXT("Canned Food"), 12.0f, EConsumableType::Food));
	Catalog.Add(MakeConsumable(TEXT("Bandage"), 12.0f, EConsumableType::Medkit));

	// Патроны: 2/шт (GDD §7.6). Пачка 10 шт = 20.
	{
		FShopEntry Ammo;
		Ammo.DisplayName = TEXT("Pistol Ammo x10");
		Ammo.Price = 20.0f;
		Ammo.Kind = EShopEntryKind::Ammo;
		Ammo.AmmoAmount = 10;
		Catalog.Add(Ammo);
	}

	// Оружие: нож 40, пистолет 150 (GDD §7.6). Покупается в рюкзак как предмет.
	Catalog.Add(MakeItem(TEXT("Knife"), 40.0f, AMeleeWeapon::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Pistol"), 150.0f, APistol::StaticClass()));

	// Броня: 60-120 (GDD §7.6). Голова 60 / штаны 80 / торс 120.
	Catalog.Add(MakeItem(TEXT("Head Armor"), 60.0f, AHeadArmor::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Pants Armor"), 80.0f, APantsArmor::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Torso Armor"), 120.0f, ATorsoArmor::StaticClass()));
}

float ATraderNPC::GetSellValue(const AMasterInventoryItem* Item) const
{
	if (!Item)
	{
		return 0.0f;
	}

	switch (Item->GetItemCategory())
	{
		case EItemCategory::Consumable: return SellValueConsumable;
		case EItemCategory::Armor:      return SellValueArmor;
		case EItemCategory::Weapon:     return SellValueWeapon;
		case EItemCategory::Resource:   return SellValueResource;
		default:                        return SellValueResource;
	}
}

void ATraderNPC::OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->SetNearbyTrader(this);
		UE_LOG(LogTemp, Log, TEXT("Trader '%s': player in range (press Interact to trade)"), *GetName());
	}
}

void ATraderNPC::OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->ClearNearbyTrader(this);
	}
}
