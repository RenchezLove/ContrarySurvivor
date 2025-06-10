// Fill out your copyright notice in the Description page of Project Settings.


#include "MasterHumanoidCharacter.h"

// Sets default values
AMasterHumanoidCharacter::AMasterHumanoidCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMasterHumanoidCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMasterHumanoidCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMasterHumanoidCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

