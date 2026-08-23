// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты «надетая броня переживает круг сохранения» (критбаг Рината
// 24.08.2026: «я запустил сохранение и часть брони пропала — торса и ног»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.SaveLoad.ArmorRows; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// ЧТО ЛОВИМ. Со времён ADR-075 идентичность предмета живёт в СТРОКЕ таблицы DT_Items, а не
// в классе: все девять броней собраны из одной BP_ArmorBase, а слот, защита и меш экипировки
// приходят строкой (ContraryItems::ApplyRowToItem). Сейв же хранил только путь класса, и
// восстановление поднимало голую заготовку — слот «торс», защита 0. Обе надетые брони
// садились в один слот, вторая молча вытесняла первую, защита обнулялась.
//
// Прежний тест ContrarySurvivor.SaveLoad.Continue.RestoresBackpackAndArmor эту дыру НЕ видел:
// он берёт AHeadArmorT1 — наследника C++, который задаёт слот и защиту в конструкторе, и
// потому чинит себя сам. Здесь броня — ASaveTestArmorItem, двойник BP_ArmorBase: один класс
// на три разных предмета, своих значений нет.
//
// Покрыто:
//   - все ТРИ слота (голова/торс/ноги) переживают SaveGame -> LoadGameForContinue: каждый
//     на месте, со своей защитой, ни один не вытеснил другой;
//   - СТАРЫЙ формат сейва (записан до появления ключа строки): восстановление находит строку
//     по служебному ключу ADR-050 из колонки LegacyKey — так лечится телефонное сохранение
//     Рината и магазинная версия 0.1.0.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Tests/SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Tests/SaveTestArmorItem.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "AArmor.h"
#include "UInventoryComponent.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags ArmorRowTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир + таблица брони (обвязка «свой мир на файл теста», как в
// SaveLoadAutomationTests/ContentToolsWaveAutomationTests — она static в своих .cpp).
namespace ArmorRowTestWorld
{
	// Служебные ключи ADR-050 — ДОСЛОВНО как в боевой DT_Items (колонка LegacyKey): именно
	// они лежат в уже существующих сейвах игроков и по ним идёт миграция.
	static const TCHAR* KeyHead  = TEXT("Броня Т1 — голова");
	static const TCHAR* KeyTorso = TEXT("Броня Т3 — торс");
	static const TCHAR* KeyPants = TEXT("Броня Т2 — штаны");

	static constexpr float ProtHead  = 0.05f;
	static constexpr float ProtTorso = 0.16f;
	static constexpr float ProtPants = 0.10f;

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

	static ASaveTestPlayerCharacter* SpawnPlayer(UWorld* World, const FString& Slot)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASaveTestPlayerCharacter* Player = World->SpawnActor<ASaveTestPlayerCharacter>(
			ASaveTestPlayerCharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		if (Player)
		{
			Player->UseTestSaveSlot(Slot);
		}
		return Player;
	}

	// Три строки брони на ОДНОМ классе-пустышке — точный слепок боевой картины (девять
	// броней из одной BP_ArmorBase, все различия в строках).
	static UDataTable* MakeArmorTable()
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_ItemsForArmorSaveQA"));
		Table->RowStruct = FContraryItemRow::StaticStruct();

		auto AddArmorRow = [Table](const TCHAR* RowName, const TCHAR* LegacyKey, const TCHAR* Display,
			EArmorSlot Slot, float Protection)
		{
			FContraryItemRow Row;
			Row.LegacyKey = LegacyKey;
			Row.DisplayText = FText::FromString(Display);
			Row.ItemClass = ASaveTestArmorItem::StaticClass();
			Row.Category = EItemCategory::Armor;
			Row.ArmorSlot = Slot;
			Row.ArmorProtection = Protection;
			Table->AddRow(FName(RowName), Row);
		};

		AddArmorRow(TEXT("armor_t1_head"),  KeyHead,  TEXT("Кепка"),          EArmorSlot::Head,  ProtHead);
		AddArmorRow(TEXT("armor_t3_torso"), KeyTorso, TEXT("Военная куртка"), EArmorSlot::Torso, ProtTorso);
		AddArmorRow(TEXT("armor_t2_pants"), KeyPants, TEXT("Штаны бандита"),  EArmorSlot::Legs,  ProtPants);
		return Table;
	}

	// Надевает на игрока броню из строки таблицы — тем же путём, каким её даёт покупка/лут.
	static AArmor* GiveAndEquipFromRow(UWorld* World, ASaveTestPlayerCharacter* Player, const TCHAR* RowName)
	{
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (!Inv)
		{
			return nullptr;
		}
		AMasterInventoryItem* Item = ContraryItems::SpawnItemFromRow(World, FName(RowName));
		AArmor* Armor = Cast<AArmor>(Item);
		if (!Armor)
		{
			return nullptr;
		}
		Armor->SetActorHiddenInGame(true);
		Armor->SetActorEnableCollision(false);
		Inv->AddItem(Armor);
		Player->EquipArmor(Armor);
		Inv->SetItemEquipped(Armor, true);
		return Armor;
	}

	static FString TestSlot(const TCHAR* Tag)
	{
		return FString::Printf(TEXT("Test_ArmorRows_%s"), Tag);
	}
}

// Общая проверка «все три слота на месте со своей защитой» — одна и та же для нового и для
// старого формата сейва, поэтому вынесена в помощник теста.
namespace
{
	void CheckAllThreeSlotsRestored(FAutomationTestBase& Test, ASaveTestPlayerCharacter* Player)
	{
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (!Inv)
		{
			Test.AddError(TEXT("У восстановленного игрока нет рюкзака"));
			return;
		}

		AArmor* Head  = Player->GetEquippedArmor(EArmorSlot::Head);
		AArmor* Torso = Player->GetEquippedArmor(EArmorSlot::Torso);
		AArmor* Pants = Player->GetEquippedArmor(EArmorSlot::Legs);

		Test.TestNotNull(TEXT("Броня головы на месте после «Продолжить»"), Head);
		Test.TestNotNull(TEXT("Броня ТОРСА на месте после «Продолжить» (баг Рината 24.08)"), Torso);
		Test.TestNotNull(TEXT("Броня НОГ на месте после «Продолжить» (баг Рината 24.08)"), Pants);

		if (Head)
		{
			Test.TestEqual(TEXT("Защита головы — из строки таблицы, а не дефолт класса"),
				Head->GetArmorProtection(), ArmorRowTestWorld::ProtHead);
			Test.TestTrue(TEXT("Броня головы помечена надетой в рюкзаке"), Inv->IsItemEquipped(Head));
		}
		if (Torso)
		{
			Test.TestEqual(TEXT("Защита торса — из строки таблицы, а не дефолт класса (0)"),
				Torso->GetArmorProtection(), ArmorRowTestWorld::ProtTorso);
			Test.TestTrue(TEXT("Броня торса помечена надетой в рюкзаке"), Inv->IsItemEquipped(Torso));
		}
		if (Pants)
		{
			Test.TestEqual(TEXT("Защита ног — из строки таблицы, а не дефолт класса (0)"),
				Pants->GetArmorProtection(), ArmorRowTestWorld::ProtPants);
			Test.TestTrue(TEXT("Броня ног помечена надетой в рюкзаке"), Inv->IsItemEquipped(Pants));
		}

		// Три РАЗНЫХ предмета: при баге штаны и торс схлопывались в один слот, и один из
		// указателей просто повторял другой.
		Test.TestTrue(TEXT("Слоты заняты тремя разными предметами"),
			Head && Torso && Pants && Head != Torso && Torso != Pants && Head != Pants);

		Test.TestEqual(TEXT("Суммарная защита сложилась из трёх слотов"),
			Player->GetTotalArmorProtection(),
			ArmorRowTestWorld::ProtHead + ArmorRowTestWorld::ProtTorso + ArmorRowTestWorld::ProtPants);

		Test.TestEqual(TEXT("В рюкзаке ровно три записи брони"), Inv->GetInventoryItems().Num(), 3);
	}
}

// ===========================================================================
// 1. Надел броню всех трёх слотов -> сохранил -> загрузил: всё на месте.
//    Броня — один класс на три предмета (двойник BP_ArmorBase), различия только в строках.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveArmorRowsRestoresAllThreeSlotsTest,
	"ContrarySurvivor.SaveLoad.ArmorRows.RestoresAllThreeSlots", ArmorRowTestFlags)

bool FSaveArmorRowsRestoresAllThreeSlotsTest::RunTest(const FString& Parameters)
{
	UWorld* World = ArmorRowTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Таблица предметов — на время теста (CDO настроек переживает прогон, вернуть обязательно).
	UContraryDataSettings* Settings = GetMutableDefault<UContraryDataSettings>();
	const TSoftObjectPtr<UDataTable> SavedTable = Settings->ItemTable;
	Settings->ItemTable = ArmorRowTestWorld::MakeArmorTable();

	const FString Slot = ArmorRowTestWorld::TestSlot(TEXT("AllThree"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* PlayerA = ArmorRowTestWorld::SpawnPlayer(World, Slot);
		if (TestNotNull(TEXT("Игрок-донор заспавнен"), PlayerA))
		{
			AArmor* Head  = ArmorRowTestWorld::GiveAndEquipFromRow(World, PlayerA, TEXT("armor_t1_head"));
			AArmor* Torso = ArmorRowTestWorld::GiveAndEquipFromRow(World, PlayerA, TEXT("armor_t3_torso"));
			AArmor* Pants = ArmorRowTestWorld::GiveAndEquipFromRow(World, PlayerA, TEXT("armor_t2_pants"));

			TestTrue(TEXT("Все три брони созданы из строк таблицы"), Head && Torso && Pants);
			TestTrue(TEXT("Все три — ОДИН класс (как боевая BP_ArmorBase)"),
				Head && Torso && Pants
				&& Head->GetClass() == Torso->GetClass() && Torso->GetClass() == Pants->GetClass());
			TestEqual(TEXT("До сохранения защита сложилась из трёх слотов"),
				PlayerA->GetTotalArmorProtection(),
				ArmorRowTestWorld::ProtHead + ArmorRowTestWorld::ProtTorso + ArmorRowTestWorld::ProtPants);

			TestTrue(TEXT("SaveGame() записал надетую броню"), PlayerA->SaveGame());
		}

		// Ключ строки лёг в сейв — по нему восстановление и поднимет слот с защитой.
		if (const UContrarySaveGame* Written = Cast<UContrarySaveGame>(
			UGameplayStatics::LoadGameFromSlot(Slot, 0)))
		{
			int32 WithRow = 0;
			for (const FSavedInventoryEntry& Entry : Written->InventoryEntries)
			{
				if (!Entry.ItemRow.IsNone())
				{
					++WithRow;
				}
			}
			TestEqual(TEXT("У всех трёх записей сейва проставлена строка таблицы"), WithRow, 3);
		}

		ASaveTestPlayerCharacter* PlayerB = ArmorRowTestWorld::SpawnPlayer(World, Slot);
		if (TestNotNull(TEXT("Игрок-приёмник заспавнен"), PlayerB))
		{
			TestTrue(TEXT("LoadGameForContinue() прошёл"), PlayerB->LoadGameForContinue());
			CheckAllThreeSlotsRestored(*this, PlayerB);
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	Settings->ItemTable = SavedTable;
	ArmorRowTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 2. СТАРЫЙ формат сейва (без ключа строки) — сейв Рината на телефоне и магазинная 0.1.0.
//    Строка ищется по служебному ключу ADR-050 из колонки LegacyKey; броня обязана вернуться.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveArmorRowsMigratesOldSaveTest,
	"ContrarySurvivor.SaveLoad.ArmorRows.MigratesSaveWithoutRowKey", ArmorRowTestFlags)

bool FSaveArmorRowsMigratesOldSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = ArmorRowTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	UContraryDataSettings* Settings = GetMutableDefault<UContraryDataSettings>();
	const TSoftObjectPtr<UDataTable> SavedTable = Settings->ItemTable;
	Settings->ItemTable = ArmorRowTestWorld::MakeArmorTable();

	const FString Slot = ArmorRowTestWorld::TestSlot(TEXT("OldFormat"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* PlayerA = ArmorRowTestWorld::SpawnPlayer(World, Slot);
		if (TestNotNull(TEXT("Игрок-донор заспавнен"), PlayerA))
		{
			ArmorRowTestWorld::GiveAndEquipFromRow(World, PlayerA, TEXT("armor_t1_head"));
			ArmorRowTestWorld::GiveAndEquipFromRow(World, PlayerA, TEXT("armor_t3_torso"));
			ArmorRowTestWorld::GiveAndEquipFromRow(World, PlayerA, TEXT("armor_t2_pants"));
			TestTrue(TEXT("SaveGame() записал надетую броню"), PlayerA->SaveGame());
		}

		// Откатываем записанный слот к СТАРОМУ формату: ключа строки в нём нет вовсе, есть
		// только служебный ключ ADR-050 — ровно так выглядит сейв с телефона Рината
		// (в нём у обеих броней класс один: /Game/Items/BP_ArmorBase.BP_ArmorBase_C).
		if (UContrarySaveGame* Old = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
		{
			int32 Keys = 0;
			for (FSavedInventoryEntry& Entry : Old->InventoryEntries)
			{
				Entry.ItemRow = NAME_None;
				if (Entry.ItemName == ArmorRowTestWorld::KeyHead
					|| Entry.ItemName == ArmorRowTestWorld::KeyTorso
					|| Entry.ItemName == ArmorRowTestWorld::KeyPants)
				{
					++Keys;
				}
			}
			TestEqual(TEXT("В старом сейве у брони остались служебные ключи ADR-050"), Keys, 3);
			TestTrue(TEXT("Старый сейв записан обратно в слот"),
				UGameplayStatics::SaveGameToSlot(Old, Slot, 0));
		}
		else
		{
			AddError(TEXT("Не прочитан только что записанный сейв"));
		}

		ASaveTestPlayerCharacter* PlayerB = ArmorRowTestWorld::SpawnPlayer(World, Slot);
		if (TestNotNull(TEXT("Игрок-приёмник заспавнен"), PlayerB))
		{
			TestTrue(TEXT("LoadGameForContinue() прошёл на старом сейве"), PlayerB->LoadGameForContinue());
			CheckAllThreeSlotsRestored(*this, PlayerB);
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);

	Settings->ItemTable = SavedTable;
	ArmorRowTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
