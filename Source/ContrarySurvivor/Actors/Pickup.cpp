// Fill out your copyright notice in the Description page of Project Settings.

#include "Pickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h" // Build 1.2.1 (ТЗ А4): MID свечения
#include "UObject/ConstructorHelpers.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "AMasterInventoryItem.h"
#include "AAmmoItem.h" // D8: пачка патронов размещаемого пикапа (PlacedAmmoAmount)
#include "UInventoryComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Debug/QADebug.h"    // QA-хелпер (оверлей/флаги/flush)

APickup::APickup()
{
	// Build 1.2.1 (ТЗ А4): тик разрешён, но по умолчанию ВЫКЛЮЧЕН — включает его только
	// BeginPlay при включённом свечении с пульсацией (bCanEverTick=false нельзя включить
	// в рантайме, поэтому разрешение здесь, а гейт — bStartWithTickEnabled).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Корень-сфера (маркер позиции лута). Подбор теперь по клавише E (контроллер ищет
	// ближайший пикап и зовёт Collect), а не по overlap — поэтому коллизию/оверлапы гасим.
	PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	SetRootComponent(PickupTrigger);
	PickupTrigger->InitSphereRadius(40.0f);
	PickupTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupTrigger->SetGenerateOverlapEvents(false);

	// Визуал (без коллизии). Build 1.2.1 (ТЗ А3): уменьшающий масштаб 0.3 старого
	// шара-заглушки УБРАН — его наследовали BP_Pickup/BP_PickupWolf, и реальные меши
	// модельера выходили «микроскопическими» (сами меши уже в игровом масштабе:
	// свёрток 45 см, мешок 39 см — паспорта ассетов). Дефолтный меш теперь мешок
	// SM_LootSack, а не движковый шар: фолбэк-пути спавна (базовый APickup без BP)
	// тоже перестают показывать «серый шар». Меша нет на диске — прежний шар 0.3.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(PickupTrigger);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SackMesh(TEXT("/Game/Environment/Props/SM_LootSack.SM_LootSack"));
	if (SackMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SackMesh.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereMesh.Succeeded())
		{
			MeshComponent->SetStaticMesh(SphereMesh.Object);
		}
		MeshComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
	}
}

void APickup::BeginPlay()
{
	Super::BeginPlay();

	// D8: размещённый на карте пикап сам наполняет себя предметами из Edit-полей.
	// Только в игровом мире; у рантайм-дропов (DropLoot/мешок смерти) поля пусты — no-op.
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		SpawnPlacedLoot();
		SetupGlow(); // Build 1.2.1 (ТЗ А4): свечение, если включено на экземпляре/BP

		// Build 1.2.2 (Ринат: «кнопка не срабатывает»): РАЗМЕЩЁННЫЙ на карте пикап без
		// лута невидим для клавиши подбора (контроллер берёт только HasLoot()==true) —
		// громко предупреждаем, какие поля заполнить. Только для акторов С КАРТЫ
		// (IsNetStartupActor, Actor.cpp:652): рантайм-дропы (DropLoot/мешок смерти) в
		// BeginPlay всегда пусты — лут им кладут InitLoot/InitLootBag ПОСЛЕ спавна.
		if (IsNetStartupActor() && !HasLoot())
		{
			UE_LOG(LogQA, Warning,
				TEXT("QA: PICKUP '%s' размещён на карте ПУСТЫМ — кнопка подбора его не увидит. ")
				TEXT("Заполните в Details (категория Pickup) хотя бы одно поле: «Денег внутри», ")
				TEXT("«Что лежит внутри (класс предмета)» или «Патронов внутри (дополнительно)»."),
				*GetName());
		}
	}
}

void APickup::SetupGlow()
{
	if (!bGlowEnabled || !MeshComponent)
	{
		return;
	}

	// MID от материала слота 0 (после починки А2 это M_VColor с параметрами свечения).
	GlowMID = MeshComponent->CreateDynamicMaterialInstance(0);
	if (!GlowMID)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Pickup '%s': свечение включено, но MID слота 0 не создался — свечения не будет."),
			*GetName());
		return;
	}

	// Материал без параметров свечения — громко в лог: значит на меше не M_VColor
	// (или М_VColor без -pickupfix), и Set*ParameterValue уйдёт в пустоту.
	float ProbeValue = 0.0f;
	if (!GlowMID->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("GlowIntensity")), ProbeValue))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Pickup '%s': у материала '%s' нет параметра GlowIntensity — свечение работать не будет."),
			*GetName(), *GetNameSafe(GlowMID->Parent));
	}

	GlowMID->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
	GlowMID->SetScalarParameterValue(TEXT("GlowIntensity"), FMath::Max(0.0f, GlowStrength));

	// Пульсация — единственная причина тикать; ровное свечение выставлено и забыто.
	if (GlowPulsePeriod > KINDA_SMALL_NUMBER)
	{
		SetActorTickEnabled(true);
	}
}

void APickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GlowMID || GlowPulsePeriod <= KINDA_SMALL_NUMBER)
	{
		SetActorTickEnabled(false);
		return;
	}

	// Синус полного цикла за GlowPulsePeriod; сила ходит 25%..100% от заданной, чтобы
	// пикап не «гас» целиком в нижней точке пульса.
	GlowTime += DeltaTime;
	const float Phase = FMath::Sin(2.0f * PI * GlowTime / GlowPulsePeriod);
	const float Pulse = 0.625f + 0.375f * Phase;
	GlowMID->SetScalarParameterValue(TEXT("GlowIntensity"), FMath::Max(0.0f, GlowStrength) * Pulse);
}

void APickup::SpawnPlacedLoot()
{
	UWorld* World = GetWorld();
	if (!World || (!PlacedItemClass && PlacedAmmoAmount <= 0))
	{
		return;
	}

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Предмет лута — данные рюкзака, не объект на сцене (тот же приём, что в DropLoot).
	auto SpawnHiddenItem = [&](TSubclassOf<AMasterInventoryItem> ItemClass) -> AMasterInventoryItem*
	{
		AMasterInventoryItem* Item = World->SpawnActor<AMasterInventoryItem>(
			ItemClass, GetActorLocation(), FRotator::ZeroRotator, Sp);
		if (Item)
		{
			Item->SetActorHiddenInGame(true);
			Item->SetActorEnableCollision(false);
			CarriedItems.Add(Item);
		}
		return Item;
	};

	int32 SpawnedCount = 0;

	if (PlacedItemClass)
	{
		if (PlacedItemClass->IsChildOf(AAmmoItem::StaticClass()))
		{
			// Патроны — стак-предмет: ОДНА пачка со StackCount=PlacedItemCount
			// (N пустых пачек были бы ошибкой конфигурации).
			if (AAmmoItem* Pack = Cast<AAmmoItem>(SpawnHiddenItem(PlacedItemClass)))
			{
				Pack->StackCount = FMath::Max(1, PlacedItemCount);
				if (!PlacedItemDisplayName.IsEmpty())
				{
					Pack->ItemName = PlacedItemDisplayName;
				}
				if (!PlacedItemDisplayText.IsEmpty())
				{
					Pack->ItemDisplayText = PlacedItemDisplayText;
				}
				++SpawnedCount;
			}
		}
		else
		{
			for (int32 i = 0; i < FMath::Max(1, PlacedItemCount); ++i)
			{
				if (AMasterInventoryItem* Item = SpawnHiddenItem(PlacedItemClass))
				{
					if (!PlacedItemDisplayName.IsEmpty())
					{
						Item->ItemName = PlacedItemDisplayName;
					}
					if (!PlacedItemDisplayText.IsEmpty())
					{
						Item->ItemDisplayText = PlacedItemDisplayText;
					}
					++SpawnedCount;
				}
			}
		}
	}

	// Патроны В ДОПОЛНЕНИЕ к предмету (D8: стоянка = расходник + патроны одним пикапом).
	if (PlacedAmmoAmount > 0)
	{
		if (AAmmoItem* Pack = Cast<AAmmoItem>(SpawnHiddenItem(AAmmoItem::StaticClass())))
		{
			Pack->StackCount = PlacedAmmoAmount;
			++SpawnedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Pickup '%s': placed loot ready (money=%.0f, items=%d, ammo=%d)"),
		*GetName(), MoneyAmount, SpawnedCount, PlacedAmmoAmount);
}

void APickup::InitLoot(float Money, AMasterInventoryItem* InCarriedItem)
{
	MoneyAmount = Money;
	CarriedItem = InCarriedItem;
}

void APickup::InitLootBag(const TArray<AMasterInventoryItem*>& Items, float Money)
{
	// A4/ADR-027: «мешок» из нескольких предметов (предметы уже сняты из рюкзака и скрыты).
	// Build 1.2: + доля потерянных при смерти денег (подбор — штатный путь MoneyAmount).
	CarriedItems = Items;
	MoneyAmount = FMath::Max(0.0f, Money);
}

bool APickup::HasLoot() const
{
	return MoneyAmount > 0.0f || IsValid(CarriedItem) || CarriedItems.Num() > 0;
}

bool APickup::Collect(APlayerCharacter* Player)
{
	if (!IsValid(Player))
	{
		return false;
	}

	UStatsComponent* Stats = Player->GetStats();
	UInventoryComponent* Inv = Player->GetInventory();

	// QA-инструментирование: КТО подбирает и ЧТО (класс предмета), + найдены ли приёмники.
	const FString ItemClassName = IsValid(CarriedItem) ? CarriedItem->GetClass()->GetName() : TEXT("none");
	FQADebug::QA(this, FString::Printf(
		TEXT("QA: COLLECT %s by %s (money=%.0f, Stats=%s Inv=%s)"),
		*ItemClassName, *Player->GetName(), MoneyAmount,
		Stats ? TEXT("ok") : TEXT("NULL"), Inv ? TEXT("ok") : TEXT("NULL")), /*bScreen=*/true);

	// Деньги -> в статы. Считаем «начислено», только если реально добавили (или денег нет).
	bool bMoneyDone = (MoneyAmount <= 0.0f);
	if (MoneyAmount > 0.0f && Stats)
	{
		Stats->AddMoney(MoneyAmount);
		UE_LOG(LogTemp, Log, TEXT("Pickup '%s': +%.0f money"), *GetName(), MoneyAmount);
		UE_LOG(LogQA, Display, TEXT("QA: PICKUP +%.0f money. Balance now %.0f"), MoneyAmount, Stats->GetMoney());
		bMoneyDone = true;
	}

	// Предмет -> в рюкзак (предмет уже скрыт/без коллизии, как тестовые предметы).
	bool bItemDone = !IsValid(CarriedItem);
	if (IsValid(CarriedItem) && Inv)
	{
		Inv->AddItem(CarriedItem);
		UE_LOG(LogTemp, Log, TEXT("Pickup '%s': looted item %s"), *GetName(), *CarriedItem->GetName());
		UE_LOG(LogQA, Display, TEXT("QA: PICKUP item '%s' (name '%s') into backpack"),
			*CarriedItem->GetName(), *CarriedItem->ItemName);
		CarriedItem = nullptr; // передан игроку, EndPlay его не уничтожит
		bItemDone = true;
	}

	// A4/ADR-027: «мешок» из нескольких предметов (дроп расходников при смерти) — отдаём ВСЕ.
	bool bBagDone = (CarriedItems.Num() == 0);
	if (CarriedItems.Num() > 0 && Inv)
	{
		int32 Given = 0;
		for (AMasterInventoryItem* It : CarriedItems)
		{
			if (IsValid(It)) { Inv->AddItem(It); ++Given; }
		}
		UE_LOG(LogTemp, Log, TEXT("Pickup '%s': looted bag of %d items"), *GetName(), Given);
		UE_LOG(LogQA, Display, TEXT("QA: PICKUP bag of %d items into backpack"), Given);
		CarriedItems.Reset(); // переданы игроку, EndPlay их не уничтожит
		bBagDone = true;
	}

	// BUG2-фикс: НЕ уничтожаем пикап, если что-то из лута не удалось начислить — иначе деньги/
	// предмет «терялись». Пикап остаётся на земле; игрок может нажать E ещё раз.
	if (!bMoneyDone || !bItemDone || !bBagDone)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pickup '%s': collect partial (money %s, item %s, bag %s) — kept on ground"),
			*GetName(), bMoneyDone ? TEXT("ok") : TEXT("FAIL"), bItemDone ? TEXT("ok") : TEXT("FAIL"),
			bBagDone ? TEXT("ok") : TEXT("FAIL"));
		return false;
	}

	bCollected = true;
	Destroy();
	return true;
}

void APickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Если пикап исчез (например, по таймеру/смене уровня) НЕ будучи подобранным —
	// уничтожаем висящий «при себе» предмет, чтобы он не утёк в мире.
	if (!bCollected && IsValid(CarriedItem))
	{
		CarriedItem->Destroy();
		CarriedItem = nullptr;
	}

	// A4/ADR-027: то же для предметов «мешка» (дроп расходников), если не подобрали.
	if (!bCollected)
	{
		for (AMasterInventoryItem* It : CarriedItems)
		{
			if (IsValid(It)) { It->Destroy(); }
		}
		CarriedItems.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

APickup* APickup::DropLoot(UWorld* World, const FVector& Location, float MoneyAmount,
	TSubclassOf<AMasterInventoryItem> ItemClass, float ItemDropChance,
	TSubclassOf<APickup> PickupClass, const FString& ItemDisplayName,
	const FText& ItemDisplayText)
{
	if (!World)
	{
		return nullptr;
	}

	// QA force-drop (клавиша U): все враги роняют предмет со 100% шансом — для проверки
	// цепочки предмет->пикап->рюкзак. Влияет только на шанс предмета, не на деньги.
	if (FQADebug::bForceDrop)
	{
		ItemDropChance = 1.0f;
	}

	TSubclassOf<APickup> SpawnClass = PickupClass;
	if (!SpawnClass)
	{
		SpawnClass = APickup::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Шанс выпадения предмета (изношенное оружие/расходник — GDD §7.8).
	AMasterInventoryItem* DroppedItem = nullptr;
	const float ItemRoll = FMath::FRand();
	const bool bItemChanceHit = (ItemClass != nullptr) && (ItemRoll <= ItemDropChance);
	if (bItemChanceHit)
	{
		DroppedItem = World->SpawnActor<AMasterInventoryItem>(
			ItemClass, Location, FRotator::ZeroRotator, SpawnParams);
		if (DroppedItem)
		{
			// Предмет лута — данные рюкзака, не объект на сцене: прячем визуал/коллизию.
			DroppedItem->SetActorHiddenInGame(true);
			DroppedItem->SetActorEnableCollision(false);
			// Служебный ключ (напр. «Шкура волка») — по нему сходится зачёт квеста.
			if (!ItemDisplayName.IsEmpty())
			{
				DroppedItem->ItemName = ItemDisplayName;
			}
			// Переводимое название рядом с ключом: один класс AQuestItem обслуживает и
			// шкуру, и ноутбук, поэтому название задаёт тот, кто создаёт предмет.
			if (!ItemDisplayText.IsEmpty())
			{
				DroppedItem->ItemDisplayText = ItemDisplayText;
			}
		}
	}

	// QA-инструментирование (BUG «лут не попадает в рюкзак»): что именно дропнулось при смерти врага.
	FQADebug::QA(World, FString::Printf(
		TEXT("QA: DROPLOOT money=%.0f item=%s chance=%.2f roll=%.2f hit=%s spawned=%s"),
		MoneyAmount,
		ItemClass ? *ItemClass->GetName() : TEXT("none"),
		ItemDropChance, ItemRoll,
		bItemChanceHit ? TEXT("YES") : TEXT("no"),
		DroppedItem ? *DroppedItem->GetName() : TEXT("none")), /*bScreen=*/true);

	// Деньги/предмет несёт один пикап (общий overlap отдаёт оба).
	APickup* Pickup = World->SpawnActor<APickup>(
		SpawnClass, Location, FRotator::ZeroRotator, SpawnParams);
	if (Pickup)
	{
		Pickup->InitLoot(MoneyAmount, DroppedItem);
	}
	else if (DroppedItem)
	{
		// Пикап не заспавнился — не оставляем висящий предмет.
		DroppedItem->Destroy();
	}

	return Pickup;
}
