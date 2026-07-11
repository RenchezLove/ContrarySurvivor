// Fill out your copyright notice in the Description page of Project Settings.

#include "MasterTrader.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PlayerCharacter.h" // APlayerCharacter (тот же каталог Characters/) — для каста overlap'а
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h" // регистрация ближайшего вендора (A2)
#include "AMasterInventoryItem.h"
#include "AConsumableItem.h"
#include "AArmorTiers.h" // броня Т1-Т3 (9 классов; старая _01 из каталога убрана — ADR-042)
#include "APistol.h"
#include "AMeleeWeapon.h"

AMasterTrader::AMasterTrader()
{
	// Тик торговцу не нужен (стоит на месте). База включает тик — гасим для экономии.
	PrimaryActorTick.bCanEverTick = false;

	// Триггер взаимодействия: overlap ТОЛЬКО по Pawn (игроку), не блокирует движение/выстрелы.
	// QueryOnly + Ignore по всем каналам → не участвует в физике и не ловит ECC_Visibility (выстрел
	// проходит мимо). Крепим к капсуле-корню Character'а.
	InteractTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(GetRootComponent());
	InteractTrigger->InitSphereRadius(InteractRadius);
	InteractTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractTrigger->SetGenerateOverlapEvents(true);
	InteractTrigger->OnComponentBeginOverlap.AddDynamic(this, &AMasterTrader::OnInteractBeginOverlap);
	InteractTrigger->OnComponentEndOverlap.AddDynamic(this, &AMasterTrader::OnInteractEndOverlap);

	// Стандартное для ACharacter выравнивание модульного меша под капсулу (как AEnemyCharacter
	// «ПРАВКА C» и старый ATraderNPC): Z=-90 ставит ноги на дно капсулы, Yaw=-90 разворачивает
	// меш лицом по +X. BP_Trader, созданный заново, иначе унаследовал бы перевёрнутый/утопленный меш.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	}

	// СОЗНАТЕЛЬНО НЕ блокируем ECC_Visibility на капсуле (в отличие от AEnemyCharacter) и НЕ
	// добавляем UStatsComponent — чтобы торговец оставался непростреливаемым и не попадал в
	// авто-лок игрока (стрельба «как раньше»). См. шапку класса.

	RebuildCatalog();
}

void AMasterTrader::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Torso/Legs следуют за позой Head (корневой скелет мастер-базы) через Leader Pose —
	// тот же механизм, что у AEnemyCharacter «ПРАВКА B»: к этому моменту все компоненты
	// (включая дефолты BP_Trader) сконструированы. AnimBP на Head назначает оператор в BP.
	if (USkeletalMeshComponent* Head = GetMesh())
	{
		if (TorsoMesh)
		{
			TorsoMesh->SetLeaderPoseComponent(Head);
		}
		if (LegsMesh)
		{
			LegsMesh->SetLeaderPoseComponent(Head);
		}
	}
}

void AMasterTrader::BeginPlay()
{
	Super::BeginPlay();

	// Применяем огромный запас HP ПОСЛЕ дефолтов BP (в BeginPlay уже видно итоговое
	// TraderMaxHealth из Class Defaults BP_Trader).
	ApplyTraderHealth();

	// Пересобираем каталог ЗДЕСЬ же: только сейчас видны цены Price*, выставленные на
	// размещённом экземпляре BP_Trader (директива Рината: параметры крутятся на экземпляре).
	// Конструкторная сборка давала бы лишь дефолты CDO, а сериализованный каталог старого
	// BP мог бы протащить убранные позиции (например, старую броню _01).
	RebuildCatalog();
}

void AMasterTrader::ApplyTraderHealth()
{
	// MaxHealth/Health — protected-члены базы, доступны из наследника. Огромный запас.
	MaxHealth = TraderMaxHealth;
	Health = TraderMaxHealth;

	UE_LOG(LogTemp, Log, TEXT("%s (Trader): health set to %.0f/%.0f (immortal, min %.0f)"),
		*GetName(), Health, MaxHealth, TraderMinHealth);
}

float AMasterTrader::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// НАМЕРЕННО НЕ зовём Super (AMasterHumanoidCharacter::TakeDamage) — его путь Health<=0 →
	// HandleDeath обошёл бы неубиваемость. Торговец лишь «царапается»: HP упирается в нижний
	// порог TraderMinHealth и никогда не достигает 0 → смерть невозможна. База/игрок/оружие
	// при этом НЕ модифицируются.
	if (DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float Before = Health;
	Health = FMath::Max(Health - DamageAmount, TraderMinHealth);
	const float Applied = Before - Health;

	UE_LOG(LogTemp, Verbose, TEXT("%s (Trader): took %.1f dmg, HP %.0f/%.0f (immortal)"),
		*GetName(), Applied, Health, MaxHealth);

	return Applied;
}

void AMasterTrader::OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Реагируем только на игрока.
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}

	bPlayerInRange = true;

	// A2: контроллер развязан от конкретного класса — NearbyTrader теперь TScriptInterface<IShopVendor>.
	// 'this' (AMasterTrader реализует IShopVendor) регистрируется как ближайший вендор; клавиша
	// Interact открывает магазин через интерфейс. По образцу overlap'а бывшего ATraderNPC.
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->SetNearbyTrader(this);
		UE_LOG(LogTemp, Log, TEXT("MasterTrader '%s': player in range (press Interact to trade)"), *GetName());
	}
}

void AMasterTrader::OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}

	bPlayerInRange = false;

	// A2: снимаем регистрацию ближайшего вендора (контроллер сам закроет магазин, если был открыт).
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->ClearNearbyTrader(this);
	}
}

float AMasterTrader::GetSellValue(const AMasterInventoryItem* Item) const
{
	if (!Item)
	{
		return 0.0f;
	}

	switch (Item->GetItemCategory())
	{
		case EItemCategory::Consumable: return SellValueConsumable;
		case EItemCategory::Armor:      return SellValueArmor;
		case EItemCategory::Weapon:     return SellValueWeapon;
		case EItemCategory::Resource:   return SellValueResource;
		default:                        return SellValueResource;
	}
}

void AMasterTrader::RebuildCatalog()
{
	// Перенос дефолтного каталога ATraderNPC (GDD §7.6 — DRAFT-цены на тюнинг).
	Catalog.Reset();

	auto MakeConsumable = [](const FString& Name, float Price, EConsumableType Type)
	{
		FShopEntry E;
		E.DisplayName = Name;
		E.Price = Price;
		E.Kind = EShopEntryKind::Item;
		E.ItemClass = AConsumableItem::StaticClass();
		E.bApplyConsumableType = true;
		E.ConsumableType = Type;
		return E;
	};

	auto MakeItem = [](const FString& Name, float Price, TSubclassOf<AMasterInventoryItem> Cls)
	{
		FShopEntry E;
		E.DisplayName = Name;
		E.Price = Price;
		E.Kind = EShopEntryKind::Item;
		E.ItemClass = Cls;
		return E;
	};

	// Расходники: вода 5, еда/бинт 12.
	Catalog.Add(MakeConsumable(TEXT("Water Bottle"), 5.0f, EConsumableType::Water));
	Catalog.Add(MakeConsumable(TEXT("Canned Food"), 12.0f, EConsumableType::Food));
	Catalog.Add(MakeConsumable(TEXT("Bandage"), 12.0f, EConsumableType::Medkit));

	// Патроны: 2/шт (GDD §7.6), покупаются стаком в рюкзак (AAmmoItem).
	// Имя = ItemName AAmmoItem («Патроны 9мм»): в каталоге и в инвентаре предмет зовётся одинаково.
	{
		FShopEntry Ammo;
		Ammo.DisplayName = TEXT("Патроны 9мм");
		Ammo.Price = 2.0f;
		Ammo.Kind = EShopEntryKind::Ammo;
		Ammo.AmmoAmount = 1;
		Catalog.Add(Ammo);
	}

	// Оружие: нож 40, пистолет 150 (GDD §7.6).
	Catalog.Add(MakeItem(TEXT("Knife"), 40.0f, AMeleeWeapon::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Pistol"), 150.0f, APistol::StaticClass()));

	// Старая броня _01 из каталога УБРАНА (ADR-042): ассеты не удаляются, но торговец ей
	// не торгует (бэклог Рината — возможно, отдать жителям деревни).

	// Броня Т1-Т3 (ADR-042): цена за слот из настроек PriceArmorT1/T2/T3 (Т1≈50 / Т2≈120 /
	// Т3≈250). Имя позиции = ItemName класса (как у «Патроны 9мм»).
	// Т0 — стартовая одежда, в магазин не кладём.
	Catalog.Add(MakeItem(TEXT("Броня Т1 — голова"), PriceArmorT1, AHeadArmorT1::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т1 — торс"), PriceArmorT1, ATorsoArmorT1::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т1 — штаны"), PriceArmorT1, APantsArmorT1::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т2 — голова"), PriceArmorT2, AHeadArmorT2::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т2 — торс"), PriceArmorT2, ATorsoArmorT2::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т2 — штаны"), PriceArmorT2, APantsArmorT2::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т3 — голова"), PriceArmorT3, AHeadArmorT3::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т3 — торс"), PriceArmorT3, ATorsoArmorT3::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т3 — штаны"), PriceArmorT3, APantsArmorT3::StaticClass()));
}
