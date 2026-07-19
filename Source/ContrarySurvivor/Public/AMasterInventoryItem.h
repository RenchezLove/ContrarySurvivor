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

	// СЛУЖЕБНЫЙ КЛЮЧ предмета, НЕ переводится (ADR-050, порция 0). По нему сходится
	// логика квестов: UQuestComponent сравнивает ItemName с FQuest::RequiredItemName
	// посимвольно (QuestComponent.cpp:204,252). Менять значения нельзя — сломается зачёт
	// квеста. Игроку показывается НЕ это поле, а ItemDisplayText (см. GetItemDisplayText).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FString ItemName;

	// ПЕРЕВОДИМОЕ название, которое видит игрок («Пистолет», «Шкура волка»). Живёт на
	// ЭКЗЕМПЛЯРЕ, а не только на классе: один класс обслуживает разные предметы
	// (AQuestItem — и шкура, и ноутбук; AConsumableItem — вода/консервы/бинт), поэтому
	// имя класса их различить не может. Кто создаёт предмет — тот и заполняет это поле
	// рядом с ключом ItemName. Пусто — откат на ключ (см. GetItemDisplayText).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "1"))
	FText ItemDisplayText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FString ItemDescription;

	// Иконка предмета для UI (ADR-043, этап E). МЯГКАЯ ссылка (была мёртвым жёстким
	// UTexture2D*, в коде нигде не читалась): текстур может ещё не быть в проекте — UI
	// обязан работать без них (текстовый фолбэк, см. AContrarySurvivorHUD::ResolveIcon).
	// Дефолт-пути для брони Т1-Т3 задаются в конструкторах AArmorTiers
	// (/Game/UI/Icons/T_Icon_Armor_T{1..3}_{Head,Torso,Legs}); у прочих предметов пусто.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "2"))
	TSoftObjectPtr<UTexture2D> ItemIcon;

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

	// ЕДИНСТВЕННЫЙ способ получить название предмета для показа игроку. Интерфейс читает
	// только его, поля напрямую не трогает. Откат в три ступени: переводимое название ->
	// ключ ItemName как есть (сегодняшнее поведение, ничего не пропадает) -> нейтральная
	// заглушка с предупреждением в лог. Третья ступень закрывает протечку служебных имён
	// вида «BP_Pistol_C_1» в корне: GetName() наружу больше не уходит (ADR-049).
	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetItemDisplayText() const;

};
