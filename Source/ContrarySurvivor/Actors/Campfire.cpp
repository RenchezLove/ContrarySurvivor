// Fill out your copyright notice in the Description page of Project Settings.

#include "Campfire.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"

ACampfire::ACampfire()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Меш-плейсхолдер: без коллизии (это декор), конкретный StaticMesh задаёт BP.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(SceneRoot);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Триггер безопасной зоны: только overlap по Pawn.
	SafeZoneTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("SafeZoneTrigger"));
	SafeZoneTrigger->SetupAttachment(SceneRoot);
	SafeZoneTrigger->InitSphereRadius(SafeZoneRadius);
	SafeZoneTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SafeZoneTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	SafeZoneTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SafeZoneTrigger->SetGenerateOverlapEvents(true);

	SafeZoneTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACampfire::OnSafeZoneBeginOverlap);
}

void ACampfire::OnSafeZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}

	// Антиспам автосейва.
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastAutoSaveTime < AutoSaveCooldown)
	{
		return;
	}
	LastAutoSaveTime = Now;

	// Автосейв: костёр = точка сейва и респауна (сохраняется текущая позиция игрока
	// в безопасной зоне -> туда же респаун при смерти, GDD §7.8).
	const bool bOk = Player->SaveGame();
	UE_LOG(LogTemp, Log, TEXT("Campfire '%s' autosave for player: %s"),
		*GetName(), bOk ? TEXT("OK") : TEXT("FAIL"));
}

bool ACampfire::SaveAtCampfire(APlayerCharacter* Player)
{
	if (!Player)
	{
		return false;
	}
	return Player->SaveGame();
}
