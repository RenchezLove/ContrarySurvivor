// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/NoExportTypes.h"
#include "Components/StaticMeshComponent.h"
#include "AMasterInventoryItem.generated.h"

// Категория предмета инвентаря (Фаза 4). Используется логикой потери рюкзака при
// смерти (GDD §7.8: теряется только часть НЕэкипированных расходников/ресурсов) и
// будущим UI-инвентарём (фильтр/сортировка по вкладкам).
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	Consumable UMETA(DisplayName = "Consumable"), // Еда/вода/аптечки — расходуются при Use()
	Resource   UMETA(DisplayName = "Resource"),   // Крафт-ресурсы/материалы
	Armor      UMETA(DisplayName = "Armor"),       // Броня (AArmor и наследники)
	Weapon     UMETA(DisplayName = "Weapon"),      // Оружие (AMasterWeapon и наследники)
	Quest      UMETA(DisplayName = "Quest")        // Квест-предметы (шкуры/ноутбук): НЕ теряются при смерти, не используются/не едятся (Фаза 5)
};

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

	// Категория предмета (Фаза 4). База = Resource; наследники задают свою в конструкторе
	// (AArmor -> Armor, AMasterWeapon -> Weapon). Расходники (еда/вода/аптечки) ставят
	// Consumable в своих BP/классах. Влияет на потерю рюкзака при смерти (GDD §7.8).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemCategory ItemCategory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* ItemMesh;


	// Functions:
	UFUNCTION(BlueprintCallable, Category = "Item")
	virtual void Use();

	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE EItemCategory GetItemCategory() const { return ItemCategory; }

};
