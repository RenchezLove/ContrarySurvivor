// Fill out your copyright notice in the Description page of Project Settings.

#include "ElderNPC.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"

AElderNPC::AElderNPC()
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
	InteractTrigger->OnComponentBeginOverlap.AddDynamic(this, &AElderNPC::OnInteractBeginOverlap);
	InteractTrigger->OnComponentEndOverlap.AddDynamic(this, &AElderNPC::OnInteractEndOverlap);

	// Плейсхолдер-тело старосты (цилиндр движка), без коллизии (как у торговца).
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(InteractTrigger);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(1.1f, 1.1f, 2.6f));
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CylMesh.Object);
	}

	// «Маяк»-шар над телом — яркая макушка для находимости издалека.
	BeaconComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	BeaconComponent->SetupAttachment(InteractTrigger);
	BeaconComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconComponent->SetRelativeScale3D(FVector(1.4f, 1.4f, 1.4f));
	BeaconComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 240.0f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BeaconComponent->SetStaticMesh(SphereMesh.Object);
	}

	// Базовый материал плейсхолдера (BasicShapeMaterial — есть вектор-параметр "Color").
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMat.Succeeded())
	{
		PlaceholderBaseMaterial = BaseMat.Object;
	}

	// Дефолтный квест старосты (DRAFT, решение Рината): убить 5 волков, награда 150 денег.
	OfferedQuest.QuestId = FName(TEXT("KillWolves"));
	OfferedQuest.Title = TEXT("Стая волков");
	OfferedQuest.Description = TEXT("Волки одолели деревню. Перебей стаю - пять волков. Награда: 150 монет.");
	OfferedQuest.Type = EQuestType::Kill;
	OfferedQuest.KillTargetTag = FName(TEXT("Wolf"));
	OfferedQuest.TargetCount = 5;
	OfferedQuest.RewardMoney = 150.0f;
	OfferedQuest.State = EQuestState::NotStarted;
}

void AElderNPC::BeginPlay()
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

void AElderNPC::OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->SetNearbyElder(this);
		UE_LOG(LogTemp, Log, TEXT("Elder '%s': player in range (press Interact to talk)"), *GetName());
	}
}

void AElderNPC::OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->ClearNearbyElder(this);
	}
}
