// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты ADR-088 «таблица предметов DT_Items — единственный источник правды
// о предмете» (вопрос Рината 23.09: «если в DT_Items поменять иконку у предмета, в игре она
// не меняется. Почему?»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Items.SingleSource; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// ЧТО ЛОВИМ. Раньше строка таблицы накладывалась только на путях «создан по строке» (покупка,
// «Продолжить», спавн по строке). Предмет, созданный ПО КЛАССУ (лут бандита, запись списка
// классом, размещённый на карте), строку не получал и показывал картинку класса/чертежа;
// пустая ячейка иконки оставляла картинку класса; расходники брали прошитые в коде пути.
// Теперь строку предмет накладывает на себя сам при появлении — один раз, на любом пути.
//
// Таблица — транзиентная, на время теста (боевая DT_Items не трогается). Картинки — мягкие
// ссылки на несуществующие пути: проверяется, ЧТО записано в предмет, загрузка не нужна.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Tests/SaveTestArmorItem.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Data/ContraryItemRow.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "AMasterInventoryItem.h"
#include "AConsumableItem.h"
#include "APistol.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags ItemsSingleSourceTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace ItemsSingleSourceTest
{
	static const TCHAR* IconWater = TEXT("/Game/QA/T_QA_Water.T_QA_Water");
	static const TCHAR* IconFood  = TEXT("/Game/QA/T_QA_Food.T_QA_Food");
	static const TCHAR* IconPants = TEXT("/Game/QA/T_QA_Pants.T_QA_Pants");
	static const TCHAR* PantsKey  = TEXT("QA штаны");

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

	static TSoftObjectPtr<UTexture2D> Icon(const TCHAR* Path)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path));
	}

	// Слепок боевой картины: расходники на ОДНОМ классе различаются типом и ключом строки,
	// броня — общий класс-пустышка (двойник BP_ArmorBase), у пистолета иконки в строке НЕТ.
	static UDataTable* MakeTable()
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_ItemsForSingleSourceQA"));
		Table->RowStruct = FContraryItemRow::StaticStruct();

		FContraryItemRow Water;
		Water.LegacyKey = AConsumableItem::GetDefaultDisplayName(EConsumableType::Water);
		Water.DisplayText = FText::FromString(TEXT("QA вода"));
		Water.Icon = Icon(IconWater);
		Water.ItemClass = AConsumableItem::StaticClass();
		Water.Category = EItemCategory::Consumable;
		Water.MaxStackCount = 999;
		Water.ConsumableType = EConsumableType::Water;
		Table->AddRow(FName(TEXT("qa_water")), Water);

		FContraryItemRow Food = Water;
		Food.LegacyKey = AConsumableItem::GetDefaultDisplayName(EConsumableType::Food);
		Food.DisplayText = FText::FromString(TEXT("QA консервы"));
		Food.Icon = Icon(IconFood);
		Food.ConsumableType = EConsumableType::Food;
		Table->AddRow(FName(TEXT("qa_food")), Food);

		FContraryItemRow Pants;
		Pants.LegacyKey = PantsKey;
		Pants.Icon = Icon(IconPants);
		Pants.ItemClass = ASaveTestArmorItem::StaticClass();
		Pants.Category = EItemCategory::Armor;
		Pants.MaxStackCount = 1;
		Pants.ArmorSlot = EArmorSlot::Legs;
		Pants.ArmorProtection = 0.10f;
		Table->AddRow(FName(TEXT("qa_pants")), Pants);

		// Ключ «Pistol» — тот, что задаёт конструктор APistol. Иконка пустая НАМЕРЕННО.
		FContraryItemRow Pistol;
		Pistol.LegacyKey = TEXT("Pistol");
		Pistol.ItemClass = APistol::StaticClass();
		Pistol.Category = EItemCategory::Weapon;
		Pistol.MaxStackCount = 1;
		Table->AddRow(FName(TEXT("qa_pistol")), Pistol);
		return Table;
	}

	// Подмена таблицы в настройках проекта на время теста (CDO переживает прогон — вернуть).
	struct FScopedItemTable
	{
		UContraryDataSettings* Settings;
		TSoftObjectPtr<UDataTable> Saved;

		FScopedItemTable()
			: Settings(GetMutableDefault<UContraryDataSettings>())
		{
			Saved = Settings->ItemTable;
			Settings->ItemTable = MakeTable();
		}
		~FScopedItemTable()
		{
			Settings->ItemTable = Saved;
		}
	};
}

// ===========================================================================
// 1. Предмет, созданный ОБЫЧНЫМ SpawnActor по классу (так же появляется предмет, размещённый
//    на карте: тот же PostInitializeComponents), сам находит свою строку по ключу класса.
//    Пустая ячейка иконки ОЧИЩАЕТ картинку класса — она больше не перебивает таблицу.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemsSingleSourceClassSpawnTest,
	"ContrarySurvivor.Items.SingleSource.ClassSpawnTakesRowByKey", ItemsSingleSourceTestFlags)

bool FItemsSingleSourceClassSpawnTest::RunTest(const FString& Parameters)
{
	UWorld* World = ItemsSingleSourceTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ItemsSingleSourceTest::FScopedItemTable Table;

	TestFalse(TEXT("У класса пистолета своя картинка есть (иначе проверка ниже пустая)"),
		GetDefault<APistol>()->ItemIcon.IsNull());

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APistol* Pistol = World->SpawnActor<APistol>(APistol::StaticClass(), FTransform::Identity, Sp);
	if (TestNotNull(TEXT("Пистолет заспавнен"), Pistol))
	{
		TestTrue(TEXT("Строка наложена сразу при появлении"), Pistol->IsItemRowResolved());
		TestEqual(TEXT("Строка найдена по ключу класса «Pistol»"), Pistol->SourceItemRow, FName(TEXT("qa_pistol")));
		TestTrue(TEXT("Пустая картинка строки убрала картинку класса (таблица — единственный источник)"),
			Pistol->GetItemIcon().IsNull());
	}

	ItemsSingleSourceTest::Destroy(World);
	return true;
}

// ===========================================================================
// 2. Голый класс расходника: картинка — из строки по ТИПУ (прошитых путей в коде больше нет),
//    а ключ, заданный по-старому и не совпадающий с таблицей («Тушёнка» на карте), не
//    оставляет предмет без строки.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemsSingleSourceConsumableTest,
	"ContrarySurvivor.Items.SingleSource.ConsumableByType", ItemsSingleSourceTestFlags)

bool FItemsSingleSourceConsumableTest::RunTest(const FString& Parameters)
{
	UWorld* World = ItemsSingleSourceTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ItemsSingleSourceTest::FScopedItemTable Table;

	// Штатный путь спавнеров: тип и ключ типа — до появления (как лут бандита, выдача, мешок).
	AMasterInventoryItem* Water = ContraryItems::SpawnItem(World, AConsumableItem::StaticClass(),
		FTransform::Identity, nullptr, [](AMasterInventoryItem& It)
		{
			CastChecked<AConsumableItem>(&It)->ConsumableType = EConsumableType::Water;
			It.ItemName = AConsumableItem::GetDefaultDisplayName(EConsumableType::Water);
		});
	if (TestNotNull(TEXT("Вода заспавнена"), Water))
	{
		TestEqual(TEXT("Строка воды найдена по ключу"), Water->SourceItemRow, FName(TEXT("qa_water")));
		TestEqual(TEXT("Картинка воды — из строки (прошитого пути в коде больше нет)"),
			Water->GetItemIcon().ToSoftObjectPath(), FSoftObjectPath(ItemsSingleSourceTest::IconWater));
		TestEqual(TEXT("Название воды — из строки"), Water->GetItemDisplayText().ToString(), FString(TEXT("QA вода")));
	}

	// Голый класс совсем без ключа строку НЕ угадывает: иначе спавнер, выставляющий тип
	// после появления, унёс бы в сейв чужую строку.
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AConsumableItem* Bare = World->SpawnActor<AConsumableItem>(AConsumableItem::StaticClass(), FTransform::Identity, Sp);
	if (TestNotNull(TEXT("Голый расходник заспавнен"), Bare))
	{
		TestTrue(TEXT("Без ключа строка не угадана"), Bare->SourceItemRow.IsNone());
		TestTrue(TEXT("Без ключа имя не подставлено"), Bare->ItemName.IsEmpty());
	}

	AMasterInventoryItem* Stew = ContraryItems::SpawnItem(World, AConsumableItem::StaticClass(),
		FTransform::Identity, nullptr, [](AMasterInventoryItem& It)
		{
			It.ItemName = TEXT("Тушёнка"); // ключ с карты L_World_C (Pickup_1), в таблице его нет
			It.StackCount = 5000;          // больше лимита строки — обрежется
		});
	if (TestNotNull(TEXT("Тушёнка заспавнена"), Stew))
	{
		TestEqual(TEXT("Незнакомый ключ — строка по типу расходника (консервы)"),
			Stew->SourceItemRow, FName(TEXT("qa_food")));
		TestEqual(TEXT("Картинка консервов — из строки"),
			Stew->GetItemIcon().ToSoftObjectPath(), FSoftObjectPath(ItemsSingleSourceTest::IconFood));
		TestEqual(TEXT("Стак обрезан лимитом строки"), Stew->GetStackCount(), 999);
	}

	ItemsSingleSourceTest::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Общий класс (двойник BP_ArmorBase) со строкой, заданной ДО появления (так делают
//    спавн по строке, сейв и дизайнер на экземпляре карты). Строка накладывается ОДИН раз:
//    повторный вызов единой точки ничего не перетирает.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemsSingleSourceSharedClassTest,
	"ContrarySurvivor.Items.SingleSource.SharedClassRowAppliedOnce", ItemsSingleSourceTestFlags)

bool FItemsSingleSourceSharedClassTest::RunTest(const FString& Parameters)
{
	UWorld* World = ItemsSingleSourceTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ItemsSingleSourceTest::FScopedItemTable Table;

	AMasterInventoryItem* Item = ContraryItems::SpawnItemFromRow(World, FName(TEXT("qa_pants")));
	AArmor* Pants = Cast<AArmor>(Item);
	if (TestNotNull(TEXT("Броня создана по строке на общем классе"), Pants))
	{
		TestEqual(TEXT("Слот — из строки"), Pants->GetArmorSlot(), EArmorSlot::Legs);
		TestEqual(TEXT("Защита — из строки"), Pants->GetArmorProtection(), 0.10f);
		TestEqual(TEXT("Ключ — из строки"), Pants->ItemName, FString(ItemsSingleSourceTest::PantsKey));
		TestEqual(TEXT("Картинка — из строки"),
			Pants->GetItemIcon().ToSoftObjectPath(), FSoftObjectPath(ItemsSingleSourceTest::IconPants));

		// Второго наложения нет: правка после появления переживает повторный вызов.
		Pants->ItemIcon = ItemsSingleSourceTest::Icon(ItemsSingleSourceTest::IconWater);
		Pants->ApplyOwnItemRow();
		TestEqual(TEXT("Повторный вызов единой точки ничего не перетёр"),
			Pants->GetItemIcon().ToSoftObjectPath(), FSoftObjectPath(ItemsSingleSourceTest::IconWater));
	}

	ItemsSingleSourceTest::Destroy(World);
	return true;
}

// ===========================================================================
// 4. Запись списка добычи, заданная КЛАССОМ (временный запасной путь, машина BP_AbandonedCar_C_3
//    на карте): предмет всё равно получает строку, стак ложится одним предметом.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemsSingleSourceLootClassEntryTest,
	"ContrarySurvivor.Items.SingleSource.LootClassEntryGetsRow", ItemsSingleSourceTestFlags)

bool FItemsSingleSourceLootClassEntryTest::RunTest(const FString& Parameters)
{
	UWorld* World = ItemsSingleSourceTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ItemsSingleSourceTest::FScopedItemTable Table;

	TArray<FPlacedLootEntry> List;
	FPlacedLootEntry ByClass;
	ByClass.ItemClass = AConsumableItem::StaticClass();
	ByClass.Count = 3;
	List.Add(ByClass);
	FPlacedLootEntry ByRow;
	ByRow.ItemRow = FName(TEXT("qa_water"));
	ByRow.Count = 2;
	List.Add(ByRow);

	TArray<AMasterInventoryItem*> Items = APickup::SpawnLootEntries(World, List, FVector::ZeroVector, TEXT("QA"));
	if (TestEqual(TEXT("Две записи — два стака"), Items.Num(), 2))
	{
		TestEqual(TEXT("Запись классом получила строку (голый расходник = консервы)"),
			Items[0]->SourceItemRow, FName(TEXT("qa_food")));
		TestEqual(TEXT("Картинка записи классом — из строки"),
			Items[0]->GetItemIcon().ToSoftObjectPath(), FSoftObjectPath(ItemsSingleSourceTest::IconFood));
		TestEqual(TEXT("Стак записи классом = 3"), Items[0]->GetStackCount(), 3);
		TestEqual(TEXT("Запись строкой — своя строка"), Items[1]->SourceItemRow, FName(TEXT("qa_water")));
		TestEqual(TEXT("Стак записи строкой = 2"), Items[1]->GetStackCount(), 2);
	}

	ItemsSingleSourceTest::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
