// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/NoExportTypes.h"
#include "Components/StaticMeshComponent.h"
#include "AMasterInventoryItem.generated.h"

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AMasterInventoryItem : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMasterInventoryItem();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Variables:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FString ItemName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FString ItemDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	UTexture2D* ItemIcon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* ItemMesh;


	// Functions:
	UFUNCTION(BlueprintCallable, Category = "Item")
	virtual void Use();

};
