// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StashChest.generated.h"

class UCorpseLootComponent;
class UContrarySaveGame;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class AMasterInventoryItem;
struct FSavedInventoryEntry;

/**
 * ОБЩЕЕ ХРАНИЛИЩЕ личного сундука (Ринат 09.10.2026: «личный ящик с общим содержимым между
 * деревней и базой учёных»). Содержимое живёт НЕ в сундуках, а в одном невидимом акторе на
 * мир: все сундуки на карте открывают его же, поэтому положенное в деревне видно на базе.
 *
 * Содержимое — обычный контейнер обыска (UCorpseLootComponent), поэтому сундук открывает то же
 * окно, что труп и машина, в паре с окном сумки (нажатие на плитку переносит вещь туда или
 * обратно). Сохраняется и читается вместе с рюкзаком: пишет APlayerCharacter::SaveGame, читает
 * «Продолжить», «Новая игра» очищает. Вместимость не ограничена (ограничение — отдельная
 * будущая задача).
 *
 * На карту не ставится: появляется сам при первом обращении (Get).
 */
UCLASS(NotPlaceable, NotBlueprintable)
class CONTRARYSURVIVOR_API APersonalStashStorage : public AActor
{
	GENERATED_BODY()

public:
	APersonalStashStorage();

	// Хранилище этого мира. bCreateIfMissing — создать, если его ещё нет (сундук открывают
	// впервые); false — только найти (сохранение пустого мира не должно его плодить).
	static APersonalStashStorage* Get(UWorld* World, bool bCreateIfMissing);

	UCorpseLootComponent* GetContainer() const { return Container; }

	// Записать содержимое в сохранение (поле StashEntries). Хранилища в мире нет — пишется
	// пустой список: сундук, как и рюкзак, сохраняется таким, каков он сейчас в игре.
	static void WriteToSave(UWorld* World, UContrarySaveGame* Save);

	// Заменить содержимое тем, что записано в сохранении («Продолжить»). Старое сохранение без
	// сундука даёт пустое хранилище.
	static void RestoreFromSave(UWorld* World, const UContrarySaveGame* Save);

	// Выбросить содержимое («Новая игра»).
	static void ClearInWorld(UWorld* World);

	// Запись сохранения из предмета и предмет из записи — тот же формат, что у рюкзака.
	static FSavedInventoryEntry MakeSaveEntry(const AMasterInventoryItem& Item);
	static AMasterInventoryItem* SpawnFromSaveEntry(UWorld* World, const FSavedInventoryEntry& Entry,
		const FTransform& Transform, AActor* ItemOwner);

private:
	UPROPERTY()
	TObjectPtr<UCorpseLootComponent> Container;
};

/**
 * Личный сундук (Ринат 09.10.2026). ИНСТРУМЕНТ-актор по образцу машины и костра: один класс
 * кода, один чертёж BP_StashChest, расстановка и настройка — на размещённом экземпляре.
 *
 * Что делает: стоит на карте, подсвечен (свой слабый тёплый свет и свечение материала), рядом
 * с ним игроку показывается подсказка действия, по действию открывается окно «сумка и сундук».
 * Своего содержимого у сундука нет — он открывает общее хранилище APersonalStashStorage.
 *
 * Свет ЗАПЕКАЕМЫЙ (неподвижный источник без теней — на телефоне бесплатен): яркость, цвет,
 * радиус и галочка «свет включён» применяются в редакторе, после их правки освещение уровня
 * нужно пересчитать. Свечение материала, наоборот, живое: код выставляет скалярный параметр
 * материала (имя — в поле) и усиливает его, пока игрок стоит рядом.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AStashChest : public AActor
{
	GENERATED_BODY()

public:
	AStashChest();

	// Общее хранилище, которое открывает этот сундук (создаётся при первом обращении).
	UCorpseLootComponent* GetStashContainer() const;

	// Стоит ли точка (игрок) достаточно близко, чтобы открыть сундук.
	bool IsWithinInteractRange(const FVector& Location) const;

	// Сундуки, живущие сейчас в мире (для подсказки контроллера — без перебора всех акторов).
	static const TArray<TWeakObjectPtr<AStashChest>>& GetActiveChests() { return ActiveChests; }

	// Сила свечения материала, выставленная сейчас (для автотестов и отладки).
	float GetCurrentGlowStrength() const { return CurrentGlowStrength; }

	// Игрок вошёл в зону сундука или вышел из неё: свечение усиливается или возвращается.
	// Зовётся событиями зоны; публичная — для автотестов.
	void SetPlayerNearby(bool bNearby);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// === РАЗМЕР (как «Масштаб машины»): меняется в чертеже — все сундуки на карте, у которых
	// число не переопределено, подхватывают его сразу. ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Размер", meta = (DisplayPriority = "1",
		DisplayName = "Масштаб сундука", ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0"))
	float ChestScale = 1.5f;

	// === ВЗАИМОДЕЙСТВИЕ ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Взаимодействие", meta = (DisplayPriority = "1",
		DisplayName = "Радиус зоны взаимодействия (см)", ClampMin = "50.0",
		ToolTip = "На каком расстоянии от сундука игроку показывается подсказка и работает кнопка действия. В этой же зоне усиливается свечение сундука."))
	float InteractRadius = 250.0f;

	// === СВЕТ (запекаемый: после правки пересчитать освещение уровня) ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свет", meta = (DisplayPriority = "1",
		DisplayName = "Свет включён"))
	bool bLightEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свет", meta = (DisplayPriority = "2",
		DisplayName = "Яркость света", ClampMin = "0.0", EditCondition = "bLightEnabled"))
	float LightIntensity = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свет", meta = (DisplayPriority = "3",
		DisplayName = "Цвет света", EditCondition = "bLightEnabled"))
	FLinearColor LightColor = FLinearColor(1.0f, 0.72f, 0.40f, 1.0f); // тёплый

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свет", meta = (DisplayPriority = "4",
		DisplayName = "Радиус света (см)", ClampMin = "10.0", EditCondition = "bLightEnabled"))
	float LightRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свет", meta = (DisplayPriority = "5",
		DisplayName = "Положение света над сундуком (см)", EditCondition = "bLightEnabled"))
	FVector LightOffset = FVector(0.0f, 0.0f, 90.0f);

	// === СВЕЧЕНИЕ МАТЕРИАЛА (живое: код выставляет скалярный параметр материала) ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свечение", meta = (DisplayPriority = "1",
		DisplayName = "Свечение включено"))
	bool bGlowEnabled = true;

	// Контракт с материалом не зашит в код: имя параметра правится здесь без пересборки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свечение", meta = (DisplayPriority = "2",
		DisplayName = "Имя параметра свечения в материале", EditCondition = "bGlowEnabled"))
	FName GlowParamName = FName(TEXT("GlowStrength"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свечение", meta = (DisplayPriority = "3",
		DisplayName = "Сила свечения обычная", ClampMin = "0.0", EditCondition = "bGlowEnabled"))
	float GlowStrengthIdle = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chest|Свечение", meta = (DisplayPriority = "4",
		DisplayName = "Сила свечения, когда игрок рядом", ClampMin = "0.0", EditCondition = "bGlowEnabled"))
	float GlowStrengthNear = 2.0f;

	// === КОМПОНЕНТЫ ===
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest")
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	UPROPERTY()
	TObjectPtr<USphereComponent> InteractZone;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> GlowLight;

private:
	// Применяет масштаб, радиус зоны и настройки света к компонентам.
	void ApplySettings();

	// Выставляет параметр свечения на материалах сундука.
	void ApplyGlow(float Strength);

	UFUNCTION()
	void HandleZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Живые материалы сундука (создаются в BeginPlay; UPROPERTY — защита от сборки мусора).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;

	float CurrentGlowStrength = 0.0f;

	static TArray<TWeakObjectPtr<AStashChest>> ActiveChests;
};
