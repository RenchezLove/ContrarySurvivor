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
#include "AAmmoItem.h" // распознавание позиции патронов по классу (путь таблицы, ADR-075)
#include "ContrarySurvivor/Data/ContraryItemLibrary.h" // строки таблицы предметов DT_Items (ADR-075)

AMasterTrader::AMasterTrader()
{
	// Тик торговцу не нужен (стоит на месте). База включает тик — гасим для экономии.
	PrimaryActorTick.bCanEverTick = false;

	// Торговец неуязвим — та же дыра, что у старосты (потомок гуманоида получал урон от
	// ножа игрока и «умирал» заглушкой, ломая магазин и проход; дефект 08-08, задача Д).
	bImmuneToDamage = true;

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

	// Особые цены выкупа (Build 1.2.2, решение лида 05-08 по РИ-28, подтверждено Ринатом):
	// шкура волка стоит 15 монет вместо прежних 2. Обоснование числа: после конца сюжета
	// игрок живёт охотой и торговлей, а при цене 2 на первую броню за 50 монет уходило
	// 25 шкур — это не занятие, а наказание. При 15 монетах десять шкур дают 150, ровно
	// цену пистолета, и у охоты появляется понятная цель. Категорию предмета не трогаем:
	// шкура остаётся квестовой (её нельзя съесть и она не теряется при смерти).
	SpecialSellValues.Add(TEXT("Шкура волка"), 15.0f);

	// ADR-075: дефолтный состав прайс-листа ССЫЛКАМИ на строки таблицы предметов DT_Items —
	// те же 15 позиций, что в зашитом прайсе (BuildLegacyCatalog). Имена строк = прежние
	// латинские AnalyticsId (решение лида 22.08). Цена -1 = из таблицы. Пока таблица не
	// назначена в настройках проекта, RebuildCatalog сам откатится на зашитый прайс.
	const TCHAR* DefaultRows[] = {
		TEXT("water_bottle"), TEXT("canned_food"), TEXT("bandage"), TEXT("ammo_9mm"),
		TEXT("knife"), TEXT("pistol"),
		TEXT("armor_t1_head"), TEXT("armor_t1_torso"), TEXT("armor_t1_pants"),
		TEXT("armor_t2_head"), TEXT("armor_t2_torso"), TEXT("armor_t2_pants"),
		TEXT("armor_t3_head"), TEXT("armor_t3_torso"), TEXT("armor_t3_pants") };
	for (const TCHAR* RowName : DefaultRows)
	{
		FShopCatalogRef Ref;
		Ref.ItemRow = FName(RowName);
		CatalogRows.Add(Ref);
	}

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
	// Задача Д (08-08): мирный NPC урона не получает ВОВСЕ — гейт тот же, что в базе
	// (собственный override его обходил, поэтому дублируем первой строкой). Прежняя схема
	// «царапается до нижнего порога» ниже остаётся страховкой на случай, если оператор
	// снимет флаг неуязвимости в BP.
	if (bImmuneToDamage)
	{
		return 0.0f;
	}

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

const FShopEntry* AMasterTrader::FindCatalogEntryForItem(const AMasterInventoryItem* Item) const
{
	if (!Item)
	{
		return nullptr;
	}

	const UClass* ItemClass = Item->GetClass();
	const AConsumableItem* Consumable = Cast<AConsumableItem>(Item);
	const FShopEntry* BestChildMatch = nullptr;

	// ADR-075: первый проход — по СЛУЖЕБНОМУ КЛЮЧУ (DisplayName позиции == ItemName предмета;
	// ключ уходит в ItemName при покупке и задаётся конструкторами/таблицей). Ключ точнее
	// класса: с таблицей несколько позиций могут делить один базовый BP-класс (вся броня
	// одним классом), и классовый проход не смог бы их различить.
	if (!Item->ItemName.IsEmpty())
	{
		for (const FShopEntry& Entry : Catalog)
		{
			if (Entry.Kind == EShopEntryKind::Item && Entry.DisplayName == Item->ItemName)
			{
				return &Entry;
			}
		}
	}

	for (const FShopEntry& Entry : Catalog)
	{
		// Патроны в прайс-листе — не предмет, а пополнение резерва: у них своя цена выкупа
		// за штуку (SellValueAmmoPerRound), поэтому позиции такого вида здесь пропускаем.
		if (Entry.Kind != EShopEntryKind::Item || !Entry.ItemClass)
		{
			continue;
		}
		if (!ItemClass->IsChildOf(Entry.ItemClass))
		{
			continue;
		}
		// Вода, консервы и бинт стоят в прайс-листе ОДНИМ классом AConsumableItem и
		// различаются только типом — без этой проверки бинт выкупался бы по цене воды.
		if (Entry.bApplyConsumableType
			&& (!Consumable || Consumable->ConsumableType != Entry.ConsumableType))
		{
			continue;
		}

		if (ItemClass == Entry.ItemClass)
		{
			return &Entry; // точное совпадение класса — лучший возможный ответ
		}
		if (!BestChildMatch || Entry.Price < BestChildMatch->Price)
		{
			BestChildMatch = &Entry; // наследник (BP-класс): берём самую дешёвую подходящую позицию
		}
	}
	return BestChildMatch;
}

float AMasterTrader::GetSellValue(const AMasterInventoryItem* Item) const
{
	if (!Item)
	{
		return 0.0f;
	}

	// Особая цена по служебному ключу предмета (шкура волка) — главнее всего остального.
	if (const float* Special = SpecialSellValues.Find(Item->ItemName))
	{
		return FMath::Max(0.0f, *Special);
	}

	// Товар из прайс-листа выкупаем ОТ ЕГО ЖЕ цены покупки: так цена выкупа гарантированно
	// ниже цены покупки того же предмета и «купил дешевле — продал дороже» невозможно
	// (Build 1.2.2, 05-08: вода 5/6 и нож 40/70 позволяли печатать деньги).
	if (const FShopEntry* Entry = FindCatalogEntryForItem(Item))
	{
		const float Fraction = FMath::Clamp(BuybackPriceFraction, 0.0f, 0.9f);
		const float Buyback = FMath::FloorToFloat(Entry->Price * Fraction);
		// Округление вниз может дать ноль на копеечном товаре — тогда платим одну монету,
		// но только если цена покупки строго больше монеты (иначе вернули бы дыру).
		if (Buyback < 1.0f && Entry->Price > 1.0f)
		{
			return 1.0f;
		}
		return FMath::Max(0.0f, Buyback);
	}

	// Вещи вне прайс-листа (лут бандитов, квестовые предметы) — прежние цены по категории.
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
	// ADR-075: сначала путь таблицы (каталог из ссылок CatalogRows на строки DT_Items) —
	// правки состава и цен на размещённом BP_Trader переживают рантайм. Таблица не
	// назначена / список пуст / все ссылки битые — прежний зашитый прайс, магазин не пустеет.
	if (BuildCatalogFromRows())
	{
		return;
	}
	BuildLegacyCatalog();
}

bool AMasterTrader::BuildCatalogFromRows()
{
	if (CatalogRows.Num() == 0 || !ContraryItems::GetItemTable())
	{
		return false;
	}

	Catalog.Reset();
	for (const FShopCatalogRef& Ref : CatalogRows)
	{
		const FContraryItemRow* Row = ContraryItems::FindRow(Ref.ItemRow);
		if (!Row)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s: в таблице предметов нет строки '%s' — позиция прайс-листа пропущена."),
				*GetName(), *Ref.ItemRow.ToString());
			continue;
		}

		UClass* ItemClass = Row->ItemClass.LoadSynchronous();
		if (!ItemClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s: у строки '%s' таблицы предметов не задан класс — позиция прайс-листа пропущена."),
				*GetName(), *Ref.ItemRow.ToString());
			continue;
		}

		FShopEntry E;
		E.ItemRow = Ref.ItemRow;
		E.DisplayName = Row->GetEffectiveKey(Ref.ItemRow); // служебный ключ (уходит в ItemName)
		E.DisplayText = Row->DisplayText;
		E.AnalyticsId = Ref.ItemRow.ToString(); // имя строки И ЕСТЬ латинский ид аналитики
		E.Price = (Ref.PriceOverride >= 0.0f) ? Ref.PriceOverride : Row->Price;

		if (ItemClass->IsChildOf(AAmmoItem::StaticClass()))
		{
			// Патроны — позиция вида «пополнение», как в зашитом прайсе: единица покупки =
			// один патрон в стак рюкзака (слайдер количества делает остальное).
			E.Kind = EShopEntryKind::Ammo;
			E.AmmoAmount = 1;
		}
		else
		{
			E.Kind = EShopEntryKind::Item;
			E.ItemClass = ItemClass;
			if (ItemClass->IsChildOf(AConsumableItem::StaticClass()))
			{
				// Вода/консервы/бинт стоят одним классом и различаются типом из строки.
				E.bApplyConsumableType = true;
				E.ConsumableType = Row->ConsumableType;
			}
		}
		Catalog.Add(E);
	}

	// Хотя бы одна позиция собралась — путь таблицы состоялся. Ноль позиций при непустом
	// списке ссылок = данные битые целиком, честнее откатиться на зашитый прайс.
	return Catalog.Num() > 0;
}

void AMasterTrader::BuildLegacyCatalog()
{
	// Перенос дефолтного каталога ATraderNPC (GDD §7.6 — DRAFT-цены на тюнинг).
	Catalog.Reset();

	// Имя товара (русское, видит игрок) и AnalyticsId (латинский, для событий аналитики)
	// разведены — см. комментарий у FShopEntry::AnalyticsId.
	auto MakeConsumable = [](EConsumableType Type, const FString& AnalyticsId, float Price)
	{
		FShopEntry E;
		E.DisplayName = AConsumableItem::GetDefaultDisplayName(Type);
		E.DisplayText = AConsumableItem::GetDefaultDisplayText(Type);
		E.AnalyticsId = AnalyticsId;
		E.Price = Price;
		E.Kind = EShopEntryKind::Item;
		E.ItemClass = AConsumableItem::StaticClass();
		E.bApplyConsumableType = true;
		E.ConsumableType = Type;
		return E;
	};

	// Name — служебный КЛЮЧ позиции (уходит в ItemName купленного предмета), Text —
	// переводимое название для показа игроку. Ключи NSLOCTEXT совпадают с конструкторами
	// предметов (AArmorTiers, APistol, AMeleeWeapon) — это одна и та же строка перевода.
	auto MakeItem = [](const FString& Name, const FText& Text, const FString& AnalyticsId,
		float Price, TSubclassOf<AMasterInventoryItem> Cls)
	{
		FShopEntry E;
		E.DisplayName = Name;
		E.DisplayText = Text;
		E.AnalyticsId = AnalyticsId;
		E.Price = Price;
		E.Kind = EShopEntryKind::Item;
		E.ItemClass = Cls;
		return E;
	};

	// Расходники: вода 5, еда/бинт 12 (цены DRAFT прежние; имена русские — «Вода»/«Консервы»/
	// «Бинт», единый источник AConsumableItem::GetDefaultDisplayName, решение Рината 07-17).
	Catalog.Add(MakeConsumable(EConsumableType::Water,  TEXT("water_bottle"), 5.0f));
	Catalog.Add(MakeConsumable(EConsumableType::Food,   TEXT("canned_food"), 12.0f));
	Catalog.Add(MakeConsumable(EConsumableType::Medkit, TEXT("bandage"),     12.0f));

	// Патроны: 2/шт (GDD §7.6), покупаются стаком в рюкзак (AAmmoItem).
	// Имя = ItemName AAmmoItem («Патроны 9мм»): в каталоге и в инвентаре предмет зовётся одинаково.
	{
		FShopEntry Ammo;
		Ammo.DisplayName = TEXT("Патроны 9мм");
		Ammo.DisplayText = NSLOCTEXT("Items", "Ammo9mm", "Патроны 9мм");
		Ammo.AnalyticsId = TEXT("ammo_9mm");
		Ammo.Price = 2.0f;
		Ammo.Kind = EShopEntryKind::Ammo;
		Ammo.AmmoAmount = 1;
		Catalog.Add(Ammo);
	}

	// Оружие: нож 40, пистолет 150 (GDD §7.6).
	Catalog.Add(MakeItem(TEXT("Knife"), NSLOCTEXT("Items", "Knife", "Нож"), TEXT("knife"), 40.0f, AMeleeWeapon::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Pistol"), NSLOCTEXT("Items", "Pistol", "Пистолет"), TEXT("pistol"), 150.0f, APistol::StaticClass()));

	// Старая броня _01 из каталога УБРАНА (ADR-042): ассеты не удаляются, но торговец ей
	// не торгует (бэклог Рината — возможно, отдать жителям деревни).

	// Броня Т1-Т3 (ADR-042): цена за слот из настроек PriceArmorT1/T2/T3 (Т1≈50 / Т2≈120 /
	// Т3≈250). Имя позиции = ItemName класса (как у «Патроны 9мм»).
	// Т0 — стартовая одежда, в магазин не кладём.
	Catalog.Add(MakeItem(TEXT("Броня Т1 — голова"), NSLOCTEXT("Items", "ArmorT1Head", "Броня Т1 — голова"), TEXT("armor_t1_head"), PriceArmorT1, AHeadArmorT1::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т1 — торс"), NSLOCTEXT("Items", "ArmorT1Torso", "Броня Т1 — торс"), TEXT("armor_t1_torso"), PriceArmorT1, ATorsoArmorT1::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т1 — штаны"), NSLOCTEXT("Items", "ArmorT1Legs", "Броня Т1 — штаны"), TEXT("armor_t1_pants"), PriceArmorT1, APantsArmorT1::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т2 — голова"), NSLOCTEXT("Items", "ArmorT2Head", "Броня Т2 — голова"), TEXT("armor_t2_head"), PriceArmorT2, AHeadArmorT2::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т2 — торс"), NSLOCTEXT("Items", "ArmorT2Torso", "Броня Т2 — торс"), TEXT("armor_t2_torso"), PriceArmorT2, ATorsoArmorT2::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т2 — штаны"), NSLOCTEXT("Items", "ArmorT2Legs", "Броня Т2 — штаны"), TEXT("armor_t2_pants"), PriceArmorT2, APantsArmorT2::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т3 — голова"), NSLOCTEXT("Items", "ArmorT3Head", "Броня Т3 — голова"), TEXT("armor_t3_head"), PriceArmorT3, AHeadArmorT3::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т3 — торс"), NSLOCTEXT("Items", "ArmorT3Torso", "Броня Т3 — торс"), TEXT("armor_t3_torso"), PriceArmorT3, ATorsoArmorT3::StaticClass()));
	Catalog.Add(MakeItem(TEXT("Броня Т3 — штаны"), NSLOCTEXT("Items", "ArmorT3Legs", "Броня Т3 — штаны"), TEXT("armor_t3_pants"), PriceArmorT3, APantsArmorT3::StaticClass()));
}
