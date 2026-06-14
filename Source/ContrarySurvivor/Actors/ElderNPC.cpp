// Fill out your copyright notice in the Description page of Project Settings.

#include "ElderNPC.h"
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

	// Реальный визуал старосты: скелет-меш на общем гуманоидном скелете (Head_Skeleton),
	// корень — InteractTrigger. Трансформ по образцу бандита AEnemyCharacter: Z=-90 ставит
	// ноги на навмеш (root спавнится на +90 над навмешем), Yaw=-90 разворачивает лицом по +X.
	CharMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharMesh"));
	CharMesh->SetupAttachment(InteractTrigger);
	CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ElderMeshAsset(TEXT("/Game/Characters/Elder/SK_Elder.SK_Elder"));
	if (ElderMeshAsset.Succeeded())
	{
		CharMesh->SetSkeletalMeshAsset(ElderMeshAsset.Object);
	}
	// AnimBP с idle/walk/run на общем скелете. FClassFinder без дота сам добавит ".<name>_C".
	static ConstructorHelpers::FClassFinder<UAnimInstance> HumanoidABP(TEXT("/Game/TestContentAndCode/PreProduction/ABP_HumanoidCharacter"));
	if (HumanoidABP.Succeeded())
	{
		CharMesh->SetAnimInstanceClass(HumanoidABP.Class);
	}

	// Плейсхолдер-тело старосты (цилиндр движка) — СКРЫТ (заменён на CharMesh, визуал-пасс).
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

	// Визуал-пасс: кислотная MID-окраска плейсхолдеров убрана. CharMesh (SK_Elder) несёт
	// собственный материал (M_VColor) с ассета; цилиндр/маяк скрыты в конструкторе.
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
