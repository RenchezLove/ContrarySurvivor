// Fill out your copyright notice in the Description page of Project Settings.

#include "TraderNPC.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
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

	// Реальный визуал торговца: скелет-меш на общем гуманоидном скелете (Head_Skeleton),
	// корень — InteractTrigger. Трансформ по образцу бандита AEnemyCharacter: Z=-90 ставит
	// ноги на навмеш (root спавнится на +90 над навмешем), Yaw=-90 разворачивает лицом по +X.
	// DRAFT: используем SK_Elder (отдельный меш торговца — финал-арт-пасс, решение Рината).
	CharMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharMesh"));
	CharMesh->SetupAttachment(InteractTrigger);
	CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TraderMeshAsset(TEXT("/Game/Characters/Elder/SK_Elder.SK_Elder"));
	if (TraderMeshAsset.Succeeded())
	{
		CharMesh->SetSkeletalMeshAsset(TraderMeshAsset.Object);
	}
	// AnimBP с idle/walk/run на общем скелете. FClassFinder без дота сам добавит ".<name>_C".
	static ConstructorHelpers::FClassFinder<UAnimInstance> HumanoidABP(TEXT("/Game/TestContentAndCode/PreProduction/ABP_HumanoidCharacter"));
	if (HumanoidABP.Succeeded())
	{
		CharMesh->SetAnimInstanceClass(HumanoidABP.Class);
	}

	// Плейсхолдер-тело торговца (цилиндр движка) — СКРЫТ (заменён на CharMesh, визуал-пасс).
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(InteractTrigger);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(1.1f, 1.1f, 2.6f));
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	MeshComponent->SetVisibility(false);
	MeshComponent->SetHiddenInGame(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CylMesh.Object);
	}

	// «Маяк»-шар над телом — СКРЫТ (кислотный плейсхолдер убран, визуал-пасс).
	BeaconComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	BeaconComponent->SetupAttachment(InteractTrigger);
	BeaconComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconComponent->SetRelativeScale3D(FVector(1.4f, 1.4f, 1.4f));
	BeaconComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 240.0f));
	BeaconComponent->SetVisibility(false);
	BeaconComponent->SetHiddenInGame(true);
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

	// Визуал-пасс: кислотная MID-окраска плейсхолдеров убрана. CharMesh (SK_Elder) несёт
	// собственный материал (M_VColor) с ассета; цилиндр/маяк скрыты в конструкторе.
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
		// Поштучно (STALKER 2-стиль): 1 патрон = 2 валюты. Слайдер магазина даёт купить N патронов
		// с живым пересчётом цены. Покупка идёт в рюкзак СТАКОМ (AAmmoItem), не в резерв напрямую.
		FShopEntry Ammo;
		Ammo.DisplayName = TEXT("Pistol Ammo");
		Ammo.Price = 2.0f;
		Ammo.Kind = EShopEntryKind::Ammo;
		Ammo.AmmoAmount = 1;
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
