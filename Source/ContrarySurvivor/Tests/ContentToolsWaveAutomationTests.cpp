// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны инструментов контента (ADR-075, спека
// spec-datatables-phase2.md). Запуск (гоняет game-lead/qa, НЕ cpp-dev — правило Рината):
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.ContentTools; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывают: контракт квест-тегов убийств (поле QuestKillTag против квестов старосты),
// действующий ключ строки таблицы предметов, наложение строки на предмет (ApplyRowToItem),
// каталог торговца из таблицы (RebuildCatalog путём таблицы + выкуп по ключу),
// петли/галочки конструктора брошенных авто.
// Все проверяемые значения взяты из РЕАЛЬНОГО кода (EnemyCharacter/WolfCharacter/ElderNPC/
// MasterTrader/ContraryItemLibrary/AbandonedCar), не выдуманы.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/AbandonedCar.h"
#include "ContrarySurvivor/Data/ContraryItemRow.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryQuestRow.h"
#include "ContrarySurvivor/Data/ContraryQuestLibrary.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "AConsumableItem.h"
#include "AArmorTiers.h"
#include "AAmmoItem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/WorldSettings.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h" // FindFProperty (тест ставит настройки машины через рефлексию)

static constexpr EAutomationTestFlags ContentToolsTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки CombatAutomationTests.cpp — статики свои на файл,
// правило «свои тесты в НОВЫЙ файл» против гонок с чужими правками).
namespace ContentToolsTestWorld
{
	static UWorld* Create()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			return nullptr;
		}
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(World);

		const FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			WorldSettings->NotifyBeginPlay();
		}
		return World;
	}

	static void Destroy(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}

	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc = FVector(0.f, 0.f, 100.f))
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params);
	}
}

// ===========================================================================
// 1. Квест-теги убийств: поле QuestKillTag врага сходится с квестами старосты
//    (группа 2 волны: тег больше не зашит в HandleDeath, но контракт обязан жить)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentToolsQuestKillTagsTest,
	"ContrarySurvivor.ContentTools.QuestKillTagsMatchElderQuests", ContentToolsTestFlags)

bool FContentToolsQuestKillTagsTest::RunTest(const FString& Parameters)
{
	const AEnemyCharacter* BanditCDO = GetDefault<AEnemyCharacter>();
	const AWolfCharacter* WolfCDO = GetDefault<AWolfCharacter>();
	const AElderNPC* ElderCDO = GetDefault<AElderNPC>();
	if (!BanditCDO || !WolfCDO || !ElderCDO)
	{
		AddError(TEXT("Нет CDO классов врагов/старосты"));
		return false;
	}

	// Квест 2 старосты считает убийства бандитов — тег пешки бандита ОБЯЗАН совпадать с
	// целью квеста посимвольно (иначе зачистка базы не засчитывается). Раньше связь держал
	// хардкод в EnemyCharacter.cpp; теперь — это поле, и совпадение сторожит тест.
	TestEqual(TEXT("QuestKillTag бандита == KillTargetTag второго квеста старосты"),
		BanditCDO->GetQuestKillTagForQA(), ElderCDO->GetSecondQuest().KillTargetTag);
	TestFalse(TEXT("QuestKillTag бандита не пуст"), BanditCDO->GetQuestKillTagForQA().IsNone());

	// Тег волка — стабильный ключ "Wolf" (контракт: латиница, уходит и в аналитику киллов).
	TestEqual(TEXT("QuestKillTag волка == 'Wolf'"),
		WolfCDO->GetQuestKillTagForQA(), FName(TEXT("Wolf")));

	return true;
}

// ===========================================================================
// 2. Таблица предметов: действующий ключ строки (LegacyKey либо имя строки)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentToolsItemRowKeyTest,
	"ContrarySurvivor.ContentTools.ItemRowEffectiveKey", ContentToolsTestFlags)

bool FContentToolsItemRowKeyTest::RunTest(const FString& Parameters)
{
	FContraryItemRow Row;

	// LegacyKey задан — ключ строки он и есть (старые предметы: «Шкура волка», «Pistol»).
	Row.LegacyKey = TEXT("Шкура волка");
	TestEqual(TEXT("Ключ = LegacyKey, когда он задан"),
		Row.GetEffectiveKey(FName(TEXT("wolf_pelt"))), FString(TEXT("Шкура волка")));

	// LegacyKey пуст — ключом служит имя строки (путь НОВЫХ предметов без старого ключа).
	Row.LegacyKey.Empty();
	TestEqual(TEXT("Ключ = имя строки, когда LegacyKey пуст"),
		Row.GetEffectiveKey(FName(TEXT("new_item"))), FString(TEXT("new_item")));

	return true;
}

// ===========================================================================
// 3. ApplyRowToItem: строка накладывает ключ/название/тип/стак/броню на живой предмет
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentToolsApplyRowTest,
	"ContrarySurvivor.ContentTools.ItemRowApplyToItem", ContentToolsTestFlags)

bool FContentToolsApplyRowTest::RunTest(const FString& Parameters)
{
	UWorld* World = ContentToolsTestWorld::Create();
	if (!World)
	{
		AddError(TEXT("Не создан тестовый мир"));
		return false;
	}

	// Расходник: ключ, название, тип, лимит стака, обрезка счётчика по лимиту.
	{
		AConsumableItem* Item = ContentToolsTestWorld::Spawn<AConsumableItem>(World);
		if (!Item)
		{
			AddError(TEXT("Не заспавнен AConsumableItem"));
		}
		else
		{
			FContraryItemRow Row;
			Row.LegacyKey = AConsumableItem::GetDefaultDisplayName(EConsumableType::Medkit);
			Row.DisplayText = FText::FromString(TEXT("Тестовый бинт"));
			Row.Category = EItemCategory::Consumable;
			Row.ConsumableType = EConsumableType::Medkit;
			Row.MaxStackCount = 10;

			ContraryItems::ApplyRowToItem(*Item, Row, FName(TEXT("bandage")), /*StackCount=*/25);

			TestEqual(TEXT("ItemName = LegacyKey строки"), Item->ItemName, Row.LegacyKey);
			TestEqual(TEXT("Название для игрока — из строки"),
				Item->GetItemDisplayText().ToString(), FString(TEXT("Тестовый бинт")));
			TestEqual(TEXT("Тип расходника — из строки"), Item->ConsumableType, EConsumableType::Medkit);
			TestEqual(TEXT("Категория — из строки"), Item->GetItemCategory(), EItemCategory::Consumable);
			TestEqual(TEXT("Лимит стака — из строки"), Item->MaxStackCount, 10);
			TestEqual(TEXT("Счётчик стака обрезан лимитом (25 -> 10)"), Item->GetStackCount(), 10);
		}
	}

	// Броня: слот и защита из строки перекрывают конструктор класса (тир Т1 = 0.05).
	{
		AHeadArmorT1* Armor = ContentToolsTestWorld::Spawn<AHeadArmorT1>(World);
		if (!Armor)
		{
			AddError(TEXT("Не заспавнена броня AHeadArmorT1"));
		}
		else
		{
			FContraryItemRow Row;
			Row.LegacyKey = TEXT("Броня Т1 — голова");
			Row.Category = EItemCategory::Armor;
			Row.ArmorSlot = EArmorSlot::Head;
			Row.ArmorProtection = 0.07f;

			ContraryItems::ApplyRowToItem(*Armor, Row, FName(TEXT("armor_t1_head")));

			TestEqual(TEXT("Слот брони — из строки"), Armor->GetArmorSlot(), EArmorSlot::Head);
			TestEqual(TEXT("Защита — из строки (перекрыла конструктор)"),
				Armor->GetArmorProtection(), 0.07f);
		}
	}

	ContentToolsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 4. Каталог торговца из таблицы (группа 5): RebuildCatalog путём таблицы,
//    цена/перекрытие цены, позиция патронов, выкуп по служебному ключу
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentToolsTraderCatalogTest,
	"ContrarySurvivor.ContentTools.TraderCatalogFromRows", ContentToolsTestFlags)

bool FContentToolsTraderCatalogTest::RunTest(const FString& Parameters)
{
	UWorld* World = ContentToolsTestWorld::Create();
	if (!World)
	{
		AddError(TEXT("Не создан тестовый мир"));
		return false;
	}

	// Таблица предметов в памяти: вода (цена 5), патроны (цена 2).
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_ItemsForQA"));
	Table->RowStruct = FContraryItemRow::StaticStruct();

	FContraryItemRow WaterRow;
	WaterRow.LegacyKey = AConsumableItem::GetDefaultDisplayName(EConsumableType::Water);
	WaterRow.DisplayText = FText::FromString(TEXT("Вода (тест)"));
	WaterRow.ItemClass = AConsumableItem::StaticClass();
	WaterRow.Category = EItemCategory::Consumable;
	WaterRow.ConsumableType = EConsumableType::Water;
	WaterRow.Price = 5.0f;
	Table->AddRow(FName(TEXT("water_bottle")), WaterRow);

	FContraryItemRow AmmoRow;
	AmmoRow.LegacyKey = TEXT("Патроны 9мм");
	AmmoRow.ItemClass = AAmmoItem::StaticClass();
	AmmoRow.Category = EItemCategory::Resource;
	AmmoRow.Price = 2.0f;
	Table->AddRow(FName(TEXT("ammo_9mm")), AmmoRow);

	// Подставляем таблицу в настройки проекта НА ВРЕМЯ теста (вернуть обязательно: это CDO).
	UContraryDataSettings* Settings = GetMutableDefault<UContraryDataSettings>();
	const TSoftObjectPtr<UDataTable> SavedTable = Settings->ItemTable;
	Settings->ItemTable = Table;

	{
		AMasterTrader* Trader = ContentToolsTestWorld::Spawn<AMasterTrader>(World);
		if (!Trader)
		{
			AddError(TEXT("Не заспавнен торговец"));
		}
		else
		{
			// Реальный RebuildCatalog путём таблицы: две ссылки, у воды цена перекрыта (7).
			TArray<FShopCatalogRef> Rows;
			FShopCatalogRef WaterRef;
			WaterRef.ItemRow = FName(TEXT("water_bottle"));
			WaterRef.PriceOverride = 7.0f;
			Rows.Add(WaterRef);
			FShopCatalogRef AmmoRef;
			AmmoRef.ItemRow = FName(TEXT("ammo_9mm"));
			Rows.Add(AmmoRef);

			Trader->RebuildCatalogWithRowsForQA(Rows);

			const TArray<FShopEntry>& Catalog = static_cast<IShopVendor*>(Trader)->GetCatalog();
			TestEqual(TEXT("Каталог собран из двух строк таблицы"), Catalog.Num(), 2);
			if (Catalog.Num() == 2)
			{
				TestEqual(TEXT("Ключ позиции = LegacyKey строки"),
					Catalog[0].DisplayName, WaterRow.LegacyKey);
				TestEqual(TEXT("Цена воды перекрыта торговцем (7, не 5)"), Catalog[0].Price, 7.0f);
				TestEqual(TEXT("Ид аналитики = имя строки"),
					Catalog[0].AnalyticsId, FString(TEXT("water_bottle")));
				TestEqual(TEXT("Позиция помнит свою строку таблицы"),
					Catalog[0].ItemRow, FName(TEXT("water_bottle")));
				TestTrue(TEXT("Вода — предмет с типом расходника"),
					Catalog[0].Kind == EShopEntryKind::Item && Catalog[0].bApplyConsumableType
					&& Catalog[0].ConsumableType == EConsumableType::Water);
				TestTrue(TEXT("Патроны распознаны по классу как позиция-пополнение"),
					Catalog[1].Kind == EShopEntryKind::Ammo && Catalog[1].AmmoAmount == 1);
				TestEqual(TEXT("Цена патрона из таблицы (2)"), Catalog[1].Price, 2.0f);
			}

			// Выкуп по СЛУЖЕБНОМУ КЛЮЧУ (новый первый проход FindCatalogEntryForItem):
			// предмет с ключом воды выкупается от ЕЁ цены (floor(7 * 0.5) = 3), а не по
			// категории расходника (6).
			AConsumableItem* WaterItem = ContentToolsTestWorld::Spawn<AConsumableItem>(World);
			if (WaterItem)
			{
				WaterItem->ItemName = WaterRow.LegacyKey;
				WaterItem->ConsumableType = EConsumableType::Water;
				TestEqual(TEXT("Выкуп воды = половина цены её позиции, найденной ПО КЛЮЧУ"),
					static_cast<IShopVendor*>(Trader)->GetSellValue(WaterItem), 3.0f);
			}
		}
	}

	// Вернуть настройки (CDO переживает тест) и мир.
	Settings->ItemTable = SavedTable;
	ContentToolsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 5. Конструктор брошенных авто (группа 7): галочка прячет деталь, угол створки
//    вертит её вокруг петли (точка петли остаётся на месте)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentToolsAbandonedCarTest,
	"ContrarySurvivor.ContentTools.AbandonedCarHingeAndParts", ContentToolsTestFlags)

bool FContentToolsAbandonedCarTest::RunTest(const FString& Parameters)
{
	UWorld* World = ContentToolsTestWorld::Create();
	if (!World)
	{
		AddError(TEXT("Не создан тестовый мир"));
		return false;
	}

	AAbandonedCar* Car = ContentToolsTestWorld::Spawn<AAbandonedCar>(World);
	if (!Car)
	{
		AddError(TEXT("Не заспавнена машина"));
		ContentToolsTestWorld::Destroy(World);
		return false;
	}

	// Поля настроек protected (наружу их читает только редактор) — тест ставит их через
	// рефлексию, как их ставил бы редактор, и перегоняет конструкцию заново.
	auto SetBoolProp = [&](const TCHAR* Name, bool bValue)
	{
		if (FBoolProperty* P = FindFProperty<FBoolProperty>(AAbandonedCar::StaticClass(), Name))
		{
			P->SetPropertyValue_InContainer(Car, bValue);
		}
		else
		{
			AddError(FString::Printf(TEXT("Нет свойства %s"), Name));
		}
	};
	auto SetFloatProp = [&](const TCHAR* Name, float Value)
	{
		if (FFloatProperty* P = FindFProperty<FFloatProperty>(AAbandonedCar::StaticClass(), Name))
		{
			P->SetPropertyValue_InContainer(Car, Value);
		}
		else
		{
			AddError(FString::Printf(TEXT("Нет свойства %s"), Name));
		}
	};
	auto SetVectorProp = [&](const TCHAR* Name, const FVector& Value)
	{
		if (FStructProperty* P = FindFProperty<FStructProperty>(AAbandonedCar::StaticClass(), Name))
		{
			*P->ContainerPtrToValuePtr<FVector>(Car) = Value;
		}
		else
		{
			AddError(FString::Printf(TEXT("Нет свойства %s"), Name));
		}
	};
	auto FindPart = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		TArray<UStaticMeshComponent*> Parts;
		Car->GetComponents<UStaticMeshComponent>(Parts);
		for (UStaticMeshComponent* Part : Parts)
		{
			if (Part && Part->GetName() == Name)
			{
				return Part;
			}
		}
		return nullptr;
	};

	const FVector Mount(-80.0f, -47.0f, 31.5f);  // монтаж двери ПЛ (паспорт моделлера, см)
	const FVector Hinge(50.0f, -10.0f, 0.0f);
	const float Angle = 35.0f;
	SetBoolProp(TEXT("bDoorFR"), false);         // галочка прячет деталь
	SetFloatProp(TEXT("DoorFLOpenAngleDeg"), Angle);
	SetVectorProp(TEXT("DoorFLMount"), Mount);
	SetVectorProp(TEXT("DoorFLHinge"), Hinge);
	Car->RerunConstructionScripts();             // применяет OnConstruction, как правка в редакторе

	UStaticMeshComponent* DoorFR = FindPart(TEXT("DoorFR"));
	UStaticMeshComponent* DoorFL = FindPart(TEXT("DoorFL"));
	UStaticMeshComponent* DoorRL = FindPart(TEXT("DoorRL"));
	if (!DoorFR || !DoorFL || !DoorRL)
	{
		AddError(TEXT("Не найдены компоненты дверей DoorFR/DoorFL/DoorRL"));
		ContentToolsTestWorld::Destroy(World);
		return false;
	}

	TestTrue(TEXT("Снятая галочкой дверь скрыта и в игре"), DoorFR->bHiddenInGame);
	TestTrue(TEXT("Оставленная дверь видима"), !DoorFL->bHiddenInGame);

	// Угол применён вокруг вертикали...
	TestTrue(TEXT("Угол двери применён (рыскание ~35°)"),
		FMath::IsNearlyEqual(DoorFL->GetRelativeRotation().Yaw, Angle, 0.1f));

	// ...и точка петли осталась НА МЕСТЕ относительно кузова: локальный трансформ двери
	// переводит точку петли в «монтаж + петля» при ЛЮБОМ угле (инвариант формулы патча
	// 22.08 «позиция = монтаж + петля - поворот*петля»).
	const FVector HingeAfter = DoorFL->GetRelativeTransform().TransformPosition(Hinge);
	TestTrue(TEXT("Точка петли неподвижна при открытой двери (монтаж + петля)"),
		HingeAfter.Equals(Mount + Hinge, 0.05f));

	// Патч 22.08 (пивоты мешей НА оси петли, меши с нулевым трансформом): при нулевом угле
	// створка стоит РОВНО в своей монтажной точке — сверяем дверь ЗЛ (угол не трогали) с
	// значением её поля монтажа (формула, не пришпиленные числа).
	if (FStructProperty* MountProp = FindFProperty<FStructProperty>(AAbandonedCar::StaticClass(), TEXT("DoorRLMount")))
	{
		const FVector RLMount = *MountProp->ContainerPtrToValuePtr<FVector>(Car);
		TestTrue(TEXT("Закрытая дверь стоит ровно в монтажной точке"),
			DoorRL->GetRelativeLocation().Equals(RLMount, 0.05f));
		TestTrue(TEXT("Монтажная точка двери ЗЛ ненулевая (дефолт из паспорта)"),
			!RLMount.IsNearlyZero());
	}
	else
	{
		AddError(TEXT("Нет свойства DoorRLMount"));
	}

	ContentToolsTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 6. Таблица квестов (группа 6): FQuest собирается из строки, предмет цели
//    разрешается ЧЕРЕЗ таблицу предметов в старый служебный ключ (ADR-050/069);
//    неразрешимая ссылка на предмет — квест из таблицы НЕ собирается (атомарность)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentToolsQuestRowTest,
	"ContrarySurvivor.ContentTools.QuestRowBuildsQuest", ContentToolsTestFlags)

bool FContentToolsQuestRowTest::RunTest(const FString& Parameters)
{
	// Таблица предметов: шкура волка (LegacyKey — тот же ключ, что дропает волк).
	UDataTable* ItemTable = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_ItemsForQuestQA"));
	ItemTable->RowStruct = FContraryItemRow::StaticStruct();
	FContraryItemRow PeltRow;
	PeltRow.LegacyKey = TEXT("Шкура волка");
	PeltRow.Category = EItemCategory::Quest;
	ItemTable->AddRow(FName(TEXT("wolf_pelt")), PeltRow);

	// Таблица квестов: аналог первого квеста старосты (значения из сверки лида 22.08).
	UDataTable* QuestTable = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_QuestsForQA"));
	QuestTable->RowStruct = FContraryQuestRow::StaticStruct();
	FContraryQuestRow Q1;
	Q1.Title = FText::FromString(TEXT("Шкуры волков (тест)"));
	Q1.Type = EQuestType::Collect;
	Q1.RequiredItemRow = FName(TEXT("wolf_pelt"));
	Q1.RequiredItemCount = 3;
	Q1.MapMarkerTag = FName(TEXT("WolfDen"));
	Q1.RewardMoney = 200.0f;
	QuestTable->AddRow(FName(TEXT("KillWolves")), Q1);
	// Строка с БИТОЙ ссылкой на предмет (нет в таблице предметов).
	FContraryQuestRow QBroken;
	QBroken.RequiredItemRow = FName(TEXT("no_such_item"));
	QBroken.RequiredItemCount = 1;
	QuestTable->AddRow(FName(TEXT("BrokenQuest")), QBroken);

	// Подставляем обе таблицы в настройки проекта НА ВРЕМЯ теста (CDO — вернуть обязательно).
	UContraryDataSettings* Settings = GetMutableDefault<UContraryDataSettings>();
	const TSoftObjectPtr<UDataTable> SavedItems = Settings->ItemTable;
	const TSoftObjectPtr<UDataTable> SavedQuests = Settings->QuestTable;
	Settings->ItemTable = ItemTable;
	Settings->QuestTable = QuestTable;

	FQuest Built;
	const bool bBuilt = ContraryQuests::BuildQuestFromRow(FName(TEXT("KillWolves")), Built);
	TestTrue(TEXT("Квест собрался из строки таблицы"), bBuilt);
	if (bBuilt)
	{
		TestEqual(TEXT("QuestId = имя строки"), Built.QuestId, FName(TEXT("KillWolves")));
		// ГЛАВНЫЙ контракт (ADR-050/069): ссылка на строку предметов разрешилась в СТАРЫЙ
		// служебный ключ — рантайм-сравнение с ItemName предметов остаётся посимвольным.
		TestEqual(TEXT("Предмет цели разрешён в служебный ключ через таблицу предметов"),
			Built.RequiredItemName, FString(TEXT("Шкура волка")));
		TestEqual(TEXT("Количество предметов — из строки"), Built.RequiredItemCount, 3);
		TestEqual(TEXT("Награда — из строки"), Built.RewardMoney, 200.0f);
		TestEqual(TEXT("Метка карты — из строки"), Built.MapMarkerTag, FName(TEXT("WolfDen")));
		TestTrue(TEXT("Свежий квест не начат"), Built.State == EQuestState::NotStarted
			&& Built.Progress == 0 && Built.ItemProgress == 0);
	}

	// Битая ссылка на предмет — сборка честно отказывает (вызывающий останется на коде).
	FQuest BuiltBroken;
	TestFalse(TEXT("Квест с неразрешимым предметом цели из таблицы НЕ собирается"),
		ContraryQuests::BuildQuestFromRow(FName(TEXT("BrokenQuest")), BuiltBroken));

	// Вернуть настройки (CDO переживает тест).
	Settings->ItemTable = SavedItems;
	Settings->QuestTable = SavedQuests;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
