// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

class AWeapon;
class UInventoryComponent;

#include "MasterHumanoidCharacter.generated.h"

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AMasterHumanoidCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AMasterHumanoidCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float Health;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float MaxHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bIsAttacking;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* HeadMesh; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* TorsoMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* LegsMesh; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* Inventory; 
  

	/*
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Equipment") 
    AWeapon* CurrentWeapon;

	Comented til i create AWeapon (AWeapon is not created yet)
	*/

	UFUNCTION(BlueprintCallable, Category = "Components")
	FORCEINLINE USkeletalMeshComponent* GetTorsoMesh() const { return TorsoMesh; }

	/* 
	
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void Attack(); // virtual UseWeapoon function To atack enemy. Whill be derived in child class

	i don't like this way of Attack realization. Comented for now. I dicided to make this functionality diferently in AI and player classes

	*/
    
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void UpdateCharacterAppearance();

	/*

	UFUNCTION(BlueprintCallable, Category = "Equipment")
    AWeapon* GetCurrentWeapon() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
    void EquipWeapon(AWeapon* Weapon);

	Comented til i create AWeapon (AWeapon is not created yet)

	*/

	UFUNCTION(BlueprintCallable, Category = "Stats")
    FORCEINLINE float GetHealth() const { return Health; }

    UFUNCTION(BlueprintCallable, Category = "Stats")
    FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual void RestoreHealth(float HealAmount);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
 