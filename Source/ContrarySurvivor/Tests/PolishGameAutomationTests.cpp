// Fill out your copyright notice in the Description page of Project Settings.

// Правки после игры Рината на телефоне 11.10.2026 (игровая часть):
//  - «Какая-то проблема со стаканием 9мм патронов… одна плитка с 36 патронами 9мм и другая
//    плитка с патронами 9мм в количестве 10 шт. Проверь, возможно такая же проблема есть и с
//    12 мм патронами» — одинаковые предметы доливаются в одну стопку при любом способе
//    получения, раздвоенные стопки из сохранения сливаются при загрузке;
//  - «необходимо сообщать игроку о малом количестве патронов в магазине - звуком. Когда
//    патронов в магазине нет совсем - нажатие на кнопку выстрела должно воспроизводить звук
//    щелчка»;
//  - «после покупки игроком пистолета - необходимо закрыть окно торговли, открыть инвентарь и
//    экипировать пистолет» и проверка здоровья при выходе из деревни;
//  - «Вернул рацию охраннику, но из диалога с ним не следует, что он вручил нам двустволку…
//    после диалога - мигает кнопка инвентаря, а в нем мигает двустволка… когда игрок закрыл
//    инвентарь - мигает кнопка смены оружия».
// Само мигание и звук — живой вид и слух, их прогон не показывает: проверяются правила и
// счётчики, по которым мигание и звук включаются.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryItemRow.h"
#include "ContrarySurvivor/Data/ContraryQuestLibrary.h"
#include "ContrarySurvivor/Data/ContraryWeaponAudioSettings.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/InventoryScreenWidget.h"
#include "ContrarySurvivor/UI/ItemPanelMode.h"
#include "UInventoryComponent.h"
#include "AAmmoItem.h"
#include "AConsumableItem.h"
#include "APistol.h"
#include "AShotgun.h"
#include "ARangedWeapon.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

static constexpr EAutomationTestFlags PolishGameTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace PolishGameTest
{
	static const FName Ammo9Row(TEXT("ammo_9mm"));
	static const FName BandageRow(TEXT("bandage"));
	static const FName ShotgunRow(TEXT("shotgun_toz34"));

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
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			WorldSettings->NotifyBeginPlay();
		}
		return World;
	}

	static void Destroy(UWorld* World)
	{
		if (World && GEngine)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		}
	}

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}

	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World ? World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params) : nullptr;
	}

	// Один кадр тестового мира даёт не больше 0,4 с игрового времени — мотаем шагами.
	static void AdvanceTime(UWorld* World, float Seconds)
	{
		for (float Left = Seconds; Left > 0.0f; Left -= 0.25f)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 0.25f);
		}
	}

	static AActor* SpawnTarget(UWorld* World, const FVector& Loc)
	{
		AActor* Target = Spawn<AActor>(World, Loc);
		if (!Target)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Target);
		Box->InitBoxExtent(FVector(34.0f, 34.0f, 88.0f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Target->SetRootComponent(Box);
		Box->RegisterComponent();
		Target->SetActorLocation(Loc);
		return Target;
	}

	// Сколько плиток патронов такого вида лежит в сумке и сколько в них патронов всего.
	static void CountAmmo(const APlayerCharacter* Player, FName AmmoType, int32& OutTiles, int32& OutRounds)
	{
		OutTiles = 0;
		OutRounds = 0;
		for (const AMasterInventoryItem* Item : Player->GetInventory()->GetInventoryItems())
		{
			const AAmmoItem* Ammo = Cast<AAmmoItem>(Item);
			if (Ammo && Ammo->GetAmmoType() == AmmoType)
			{
				++OutTiles;
				OutRounds += Ammo->GetStackCount();
			}
		}
	}

	// То же для любого предмета по строке таблицы предметов.
	static void CountRow(const APlayerCharacter* Player, FName Row, int32& OutTiles, int32& OutPieces)
	{
		OutTiles = 0;
		OutPieces = 0;
		for (const AMasterInventoryItem* Item : Player->GetInventory()->GetInventoryItems())
		{
			if (Item && Item->GetItemRowName() == Row)
			{
				++OutTiles;
				OutPieces += FMath::Max(1, Item->GetStackCount());
			}
		}
	}

	// Патроны «с земли и с трупа»: предмет создан сам по себе и кладётся в сумку общим путём
	// (так кладут обыск трупа, подбор и сундук).
	static bool LootFromRow(UWorld* World, APlayerCharacter* Player, FName Row, int32 Count)
	{
		AMasterInventoryItem* Item = ContraryItems::SpawnItemFromRow(World, Row, Count, FTransform(Player->GetActorLocation()), nullptr);
		return Item && Player->GetInventory()->AddItem(Item);
	}

	// Пачка «голого» класса кода — так кладёт патроны размещённый на уровне подбираемый мешок.
	static bool LootNativePack(UWorld* World, APlayerCharacter* Player, UClass* PackClass, int32 Count)
	{
		AMasterInventoryItem* Item = ContraryItems::SpawnItem(World, PackClass, FTransform(Player->GetActorLocation()), nullptr,
			[Count](AMasterInventoryItem& It) { It.StackCount = Count; });
		return Item && Player->GetInventory()->AddItem(Item);
	}

	static FString ItemsToString(const APlayerCharacter* Player)
	{
		FString Out;
		for (const AMasterInventoryItem* Item : Player->GetInventory()->GetInventoryItems())
		{
			if (Item)
			{
				Out += FString::Printf(TEXT("[%s '%s' строка %s x%d] "), *Item->GetClass()->GetName(), *Item->ItemName,
					*Item->GetItemRowName().ToString(), Item->GetStackCount());
			}
		}
		return Out;
	}
}

// ===========================================================================
// 1. Патроны и расходники доливаются в одну стопку при любом способе получения.
//    Идёт на НАСТОЯЩИХ таблицах проекта: раздвоение давала разница между классом пачки из
//    таблицы предметов и классом пачки из кода.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishStackingEveryPathTest,
	"ContrarySurvivor.Stacking.SameItemsMergeOnEveryPath", PolishGameTestFlags)

bool FPolishStackingEveryPathTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryPolishStackingQA");
	PolishGameTest::DeleteSlot(Slot);
	UWorld* World = PolishGameTest::Create();
	ASaveTestPlayerCharacter* Player = PolishGameTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	if (!World || !Player || !Player->GetInventory() || !Player->GetStats())
	{
		AddError(TEXT("Не заспавнен игрок"));
		PolishGameTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	const FContraryItemRow* Ammo9 = ContraryItems::FindRow(PolishGameTest::Ammo9Row);
	const FContraryItemRow* Ammo12 = ContraryItems::FindRow(ContraryAmmo::Shotgun12gType());
	if (!TestNotNull(TEXT("В таблице предметов есть строка патронов 9 мм"), Ammo9)
		|| !TestNotNull(TEXT("В таблице предметов есть строка патронов 12-го калибра"), Ammo12))
	{
		PolishGameTest::Destroy(World);
		return false;
	}
	AddInfo(FString::Printf(TEXT("Предел стопки по таблице: 9 мм — %d, 12-й калибр — %d"), Ammo9->MaxStackCount, Ammo12->MaxStackCount));
	TestTrue(TEXT("Предел стопки 9 мм больше 46 — 36 и 10 обязаны лежать одной стопкой"), Ammo9->MaxStackCount >= 46);

	int32 Tiles = 0, Rounds = 0;
	const FName Type9 = ContraryAmmo::DefaultType();

	// --- 9 мм. Случай Рината: запас купленного пистолета (36 после стрельбы) + 10 с бандита ---
	Player->AddAmmoToInventory(36, Type9); // так в сумку приходят патроны купленного ствола и разрядка
	TestTrue(TEXT("10 патронов с трупа легли в сумку"), PolishGameTest::LootFromRow(World, Player, PolishGameTest::Ammo9Row, 10));
	PolishGameTest::CountAmmo(Player, Type9, Tiles, Rounds);
	TestEqual(*FString::Printf(TEXT("36 купленных и 10 найденных патронов 9 мм — одна плитка (сумка: %s)"), *PolishGameTest::ItemsToString(Player)), Tiles, 1);
	TestEqual(TEXT("В ней 46 патронов"), Rounds, 46);

	// Подбираемый мешок с патронами, размещённый на уровне.
	TestTrue(TEXT("Патроны из размещённого мешка легли в сумку"),
		PolishGameTest::LootNativePack(World, Player, AAmmoItem::StaticClass(), 5));
	// Награда задания и покупка у торговца.
	TestEqual(TEXT("Награда задания выдала 4 патрона"), Player->GiveItemFromRow(PolishGameTest::Ammo9Row, 4), 4);
	FShopEntry AmmoEntry;
	AmmoEntry.DisplayName = TEXT("ammo_9mm");
	AmmoEntry.Kind = EShopEntryKind::Ammo;
	AmmoEntry.AmmoAmount = 1;
	AmmoEntry.Price = 2.0f;
	AmmoEntry.ItemRow = PolishGameTest::Ammo9Row;
	Player->GetStats()->AddMoney(1000.0f);
	TestTrue(TEXT("Куплено 7 патронов"), Player->Shop_BuyEntryQty(AmmoEntry, 7));
	PolishGameTest::CountAmmo(Player, Type9, Tiles, Rounds);
	TestEqual(*FString::Printf(TEXT("После мешка, награды и покупки патроны 9 мм — одна плитка (сумка: %s)"), *PolishGameTest::ItemsToString(Player)), Tiles, 1);
	TestEqual(TEXT("В ней 62 патрона"), Rounds, 62);

	// Обратный порядок: сначала найденные, потом купленные.
	Player->TakeAmmoFromInventory(62, Type9);
	PolishGameTest::CountAmmo(Player, Type9, Tiles, Rounds);
	TestEqual(TEXT("Сумка от патронов 9 мм очищена"), Tiles, 0);
	PolishGameTest::LootNativePack(World, Player, AAmmoItem::StaticClass(), 3);
	PolishGameTest::LootFromRow(World, Player, PolishGameTest::Ammo9Row, 10);
	Player->AddAmmoToInventory(20, Type9);
	PolishGameTest::CountAmmo(Player, Type9, Tiles, Rounds);
	TestEqual(*FString::Printf(TEXT("Мешок, труп, покупка — снова одна плитка (сумка: %s)"), *PolishGameTest::ItemsToString(Player)), Tiles, 1);
	TestEqual(TEXT("В ней 33 патрона"), Rounds, 33);

	// --- 12-й калибр: те же пути ---
	const FName Type12 = ContraryAmmo::Shotgun12gType();
	Player->AddAmmoToInventory(4, Type12);
	PolishGameTest::LootFromRow(World, Player, Type12, 6);
	PolishGameTest::LootNativePack(World, Player, AShotgunAmmoItem::StaticClass(), 2);
	Player->GiveItemFromRow(Type12, 4);
	PolishGameTest::CountAmmo(Player, Type12, Tiles, Rounds);
	TestEqual(*FString::Printf(TEXT("Патроны 12-го калибра — одна плитка (сумка: %s)"), *PolishGameTest::ItemsToString(Player)), Tiles, 1);
	TestEqual(TEXT("В ней 16 патронов"), Rounds, 16);
	PolishGameTest::CountAmmo(Player, Type9, Tiles, Rounds);
	TestEqual(TEXT("Дробь с пистолетными патронами не смешалась"), Rounds, 33);

	// --- Расходники: подарок старосты (класс кода) и аптечка из таблицы — одна плитка ---
	TestEqual(TEXT("Подарена одна аптечка"), Player->GiveConsumableToBackpack(EConsumableType::Medkit, 1), 1);
	PolishGameTest::LootFromRow(World, Player, PolishGameTest::BandageRow, 1);
	int32 Pieces = 0;
	PolishGameTest::CountRow(Player, PolishGameTest::BandageRow, Tiles, Pieces);
	TestEqual(*FString::Printf(TEXT("Подаренная и найденная аптечки — одна плитка (сумка: %s)"), *PolishGameTest::ItemsToString(Player)), Tiles, 1);
	TestEqual(TEXT("В ней две аптечки"), Pieces, 2);
	PolishGameTest::LootFromRow(World, Player, FName(TEXT("water_bottle")), 1);
	PolishGameTest::CountRow(Player, PolishGameTest::BandageRow, Tiles, Pieces);
	TestEqual(TEXT("Вода в стопку аптечек не влилась"), Pieces, 2);

	PolishGameTest::Destroy(World);
	PolishGameTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 2. Раздвоенные стопки из сохранения сливаются при загрузке.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishStackingLoadMergeTest,
	"ContrarySurvivor.Stacking.SplitStacksMergeOnLoad", PolishGameTestFlags)

bool FPolishStackingLoadMergeTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryPolishStackingLoadQA");
	PolishGameTest::DeleteSlot(Slot);
	UWorld* World = PolishGameTest::Create();
	ASaveTestPlayerCharacter* Donor = PolishGameTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	if (!World || !Donor)
	{
		AddError(TEXT("Не заспавнен игрок"));
		PolishGameTest::Destroy(World);
		return false;
	}
	Donor->UseTestSaveSlot(Slot);
	TestTrue(TEXT("Исходное сохранение записано"), Donor->SaveGame());

	// Сохранение «как на телефоне Рината»: две пачки 9 мм разных классов (36 и 10) и две пачки
	// дроби (4 и 6) — так они лежали в сумке до починки.
	const FContraryItemRow* Ammo9 = ContraryItems::FindRow(PolishGameTest::Ammo9Row);
	const FContraryItemRow* Ammo12 = ContraryItems::FindRow(ContraryAmmo::Shotgun12gType());
	UContrarySaveGame* Save = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!TestNotNull(TEXT("Сохранение прочитано"), Save) || !Ammo9 || !Ammo12)
	{
		PolishGameTest::Destroy(World);
		PolishGameTest::DeleteSlot(Slot);
		return false;
	}
	auto AddEntry = [Save](const FString& ClassPath, const FString& ItemName, FName Row, int32 Count)
	{
		FSavedInventoryEntry Entry;
		Entry.ClassPath = ClassPath;
		Entry.ItemName = ItemName;
		Entry.ItemRow = Row;
		Entry.StackCount = Count;
		Save->InventoryEntries.Add(Entry);
	};
	Save->InventoryEntries.Reset();
	AddEntry(AAmmoItem::StaticClass()->GetPathName(), GetDefault<AAmmoItem>()->ItemName, NAME_None, 36);
	AddEntry(Ammo9->ItemClass.ToString(), Ammo9->GetEffectiveKey(PolishGameTest::Ammo9Row), PolishGameTest::Ammo9Row, 10);
	AddEntry(AShotgunAmmoItem::StaticClass()->GetPathName(), GetDefault<AShotgunAmmoItem>()->ItemName, NAME_None, 4);
	AddEntry(Ammo12->ItemClass.ToString(), Ammo12->GetEffectiveKey(ContraryAmmo::Shotgun12gType()), ContraryAmmo::Shotgun12gType(), 6);
	TestTrue(TEXT("Сохранение с раздвоенными пачками записано"), UGameplayStatics::SaveGameToSlot(Save, Slot, 0));

	ASaveTestPlayerCharacter* Player = PolishGameTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(500.0, 0.0, 100.0));
	if (TestNotNull(TEXT("Игрок-приёмник заспавнен"), Player))
	{
		Player->UseTestSaveSlot(Slot);
		TestTrue(TEXT("«Продолжить» отработало"), Player->LoadGameForContinue());
		int32 Tiles = 0, Rounds = 0;
		PolishGameTest::CountAmmo(Player, ContraryAmmo::DefaultType(), Tiles, Rounds);
		TestEqual(*FString::Printf(TEXT("После загрузки патроны 9 мм — одна плитка (сумка: %s)"), *PolishGameTest::ItemsToString(Player)), Tiles, 1);
		TestEqual(TEXT("В ней 46 патронов — ни один не потерян"), Rounds, 46);
		PolishGameTest::CountAmmo(Player, ContraryAmmo::Shotgun12gType(), Tiles, Rounds);
		TestEqual(TEXT("После загрузки патроны 12-го калибра — одна плитка"), Tiles, 1);
		TestEqual(TEXT("В ней 10 патронов"), Rounds, 10);
	}

	PolishGameTest::Destroy(World);
	PolishGameTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 3. Звуки оружия: правило «осталась четверть» и «щелчок не чаще заданного».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishWeaponNoticeRulesTest,
	"ContrarySurvivor.WeaponAudio.LowAmmoAndDryFireRules", PolishGameTestFlags)

bool FPolishWeaponNoticeRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Магазин на 12 патронов: порог — три"), ARangedWeapon::ComputeLowAmmoThreshold(12, 0.25f), 3);
	TestEqual(TEXT("Магазин на 7 патронов: порог — один (четверть вниз)"), ARangedWeapon::ComputeLowAmmoThreshold(7, 0.25f), 1);
	TestEqual(TEXT("Двустволка на 2 патрона: порог не меньше одного"), ARangedWeapon::ComputeLowAmmoThreshold(2, 0.25f), 1);
	TestEqual(TEXT("Нулевая доля выключает предупреждение"), ARangedWeapon::ComputeLowAmmoThreshold(12, 0.0f), 0);
	TestEqual(TEXT("Пустой по замыслу магазин — предупреждения нет"), ARangedWeapon::ComputeLowAmmoThreshold(0, 0.25f), 0);

	// Расстрел магазина на 12: предупреждение ровно одно — когда осталось три.
	int32 Warnings = 0, WarnedAt = -1;
	for (int32 Left = 11; Left >= 0; --Left)
	{
		if (ARangedWeapon::ShouldWarnLowAmmo(Left, 12, 0.25f))
		{
			++Warnings;
			WarnedAt = Left;
		}
	}
	TestEqual(TEXT("За магазин предупреждение одно"), Warnings, 1);
	TestEqual(TEXT("Оно звучит, когда осталось три патрона"), WarnedAt, 3);
	TestFalse(TEXT("Выключенное предупреждение молчит"), ARangedWeapon::ShouldWarnLowAmmo(3, 12, 0.0f));

	TestTrue(TEXT("Первый щелчок разрешён сразу"), ARangedWeapon::IsDryFireClickAllowed(10.0, -1.0, 0.25f));
	TestFalse(TEXT("Через 0,1 с после щелчка следующий ещё рано"), ARangedWeapon::IsDryFireClickAllowed(10.1, 10.0, 0.25f));
	TestTrue(TEXT("Через 0,25 с щелчок снова разрешён"), ARangedWeapon::IsDryFireClickAllowed(10.25, 10.0, 0.25f));

	const UContraryWeaponAudioSettings* Audio = UContraryWeaponAudioSettings::Get();
	if (TestNotNull(TEXT("Настройки звуков оружия есть"), Audio))
	{
		TestEqual(TEXT("По умолчанию «мало» — четверть магазина"), Audio->LowAmmoClipFraction, 0.25f);
		TestTrue(TEXT("Щелчок — не чаще четырёх раз в секунду"), Audio->DryFireMinInterval >= 0.25f);
	}
	return true;
}

// ===========================================================================
// 4. Звуки оружия вживую: пистолет в руках героя, расстрел магазина и нажатия на пустом.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishWeaponNoticeLiveTest,
	"ContrarySurvivor.WeaponAudio.OneWarningPerClipAndThrottledClicks", PolishGameTestFlags)

bool FPolishWeaponNoticeLiveTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryPolishWeaponAudioQA");
	PolishGameTest::DeleteSlot(Slot);
	UWorld* World = PolishGameTest::Create();
	ASaveTestPlayerCharacter* Player = PolishGameTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	APistol* Pistol = PolishGameTest::Spawn<APistol>(World, FVector(0.0, 0.0, 100.0));
	APistol* BanditPistol = PolishGameTest::Spawn<APistol>(World, FVector(0.0, 300.0, 100.0));
	AActor* Target = PolishGameTest::SpawnTarget(World, FVector(150.0, 0.0, 100.0));
	if (!World || !Player || !Pistol || !BanditPistol || !Target || !Player->GetInventory())
	{
		AddError(TEXT("Не заспавнены участники проверки"));
		PolishGameTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	Pistol->DrainReserveAmmo();
	BanditPistol->DrainReserveAmmo();
	Player->GetInventory()->AddItem(Pistol);
	TestTrue(TEXT("Пистолет встал в слот героя"), Player->TryAdoptRangedWeapon(Pistol));
	// Патроны, перелитые из запаса ствола в сумку, убираем: перезаряжать должно быть нечем.
	Player->TakeAmmoFromInventory(9999, Pistol->GetAmmoType());

	const UContraryWeaponAudioSettings* Audio = UContraryWeaponAudioSettings::Get();
	const int32 Clip = Pistol->GetMaxAmmoInClip();
	const int32 Threshold = ARangedWeapon::ComputeLowAmmoThreshold(Clip, Audio->LowAmmoClipFraction);
	TestTrue(TEXT("У пистолета есть порог предупреждения"), Threshold >= 1);

	for (int32 Shot = 0; Shot < Clip; ++Shot)
	{
		PolishGameTest::AdvanceTime(World, 1.0f);
		Pistol->Fire(Target);
		const int32 Left = Pistol->GetCurrentAmmoInClip();
		TestEqual(*FString::Printf(TEXT("В магазине осталось %d: предупреждений столько, сколько положено"), Left),
			Pistol->GetLowAmmoWarningCount(), Left <= Threshold ? 1 : 0);
	}
	TestEqual(TEXT("Магазин расстрелян"), Pistol->GetCurrentAmmoInClip(), 0);
	TestEqual(TEXT("Пока были патроны, щелчков не было"), Pistol->GetDryFireClickCount(), 0);

	// Пустой магазин: кнопка выстрела зажата — нажатие приходит каждый кадр.
	PolishGameTest::AdvanceTime(World, 1.0f);
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		Pistol->Fire(Target);
	}
	TestEqual(TEXT("Пять нажатий в одно мгновение — один щелчок"), Pistol->GetDryFireClickCount(), 1);
	PolishGameTest::AdvanceTime(World, 1.0f);
	Pistol->Fire(Target);
	TestEqual(TEXT("Через секунду — второй щелчок"), Pistol->GetDryFireClickCount(), 2);
	PolishGameTest::AdvanceTime(World, 1.0f);
	Pistol->Fire(nullptr);
	TestEqual(TEXT("Щелчок звучит и без цели под прицелом"), Pistol->GetDryFireClickCount(), 3);
	TestEqual(TEXT("Щелчки предупреждений не добавили"), Pistol->GetLowAmmoWarningCount(), 1);

	// Оружие не в руках героя (ствол бандита) молчит.
	for (int32 Shot = 0; Shot <= BanditPistol->GetMaxAmmoInClip(); ++Shot)
	{
		PolishGameTest::AdvanceTime(World, 1.0f);
		BanditPistol->Fire(Target);
	}
	TestEqual(TEXT("Чужой ствол о малом магазине не предупреждает"), BanditPistol->GetLowAmmoWarningCount(), 0);
	TestEqual(TEXT("Чужой ствол пустым не щёлкает"), BanditPistol->GetDryFireClickCount(), 0);

	PolishGameTest::Destroy(World);
	PolishGameTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 5. Здоровье при выходе из деревни: правило предупреждения и данные задания на бандитов.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishLowHealthWarningTest,
	"ContrarySurvivor.QuestHints.LowHealthWarningOnLeavingVillage", PolishGameTestFlags)

bool FPolishLowHealthWarningTest::RunTest(const FString& Parameters)
{
	FQuest Quest;
	Quest.QuestId = FName(TEXT("QA"));
	Quest.TargetCount = 3;
	Quest.LowHealthWarnThreshold = 45.0f;
	Quest.State = EQuestState::Active;
	TestTrue(TEXT("Здоровье 44 при пороге 45 — предупреждаем"), UQuestComponent::ShouldWarnLowHealth(Quest, 44.0f));
	TestFalse(TEXT("Здоровье ровно 45 — не предупреждаем"), UQuestComponent::ShouldWarnLowHealth(Quest, 45.0f));
	TestFalse(TEXT("Здоровье 80 — не предупреждаем"), UQuestComponent::ShouldWarnLowHealth(Quest, 80.0f));
	Quest.Progress = 3;
	TestFalse(TEXT("Бандиты уже перебиты — не предупреждаем"), UQuestComponent::ShouldWarnLowHealth(Quest, 10.0f));
	Quest.Progress = 1;
	Quest.State = EQuestState::NotStarted;
	TestFalse(TEXT("Задание не взято — не предупреждаем"), UQuestComponent::ShouldWarnLowHealth(Quest, 10.0f));
	Quest.State = EQuestState::TurnedIn;
	TestFalse(TEXT("Задание сдано — не предупреждаем"), UQuestComponent::ShouldWarnLowHealth(Quest, 10.0f));
	Quest.State = EQuestState::Active;
	Quest.LowHealthWarnThreshold = 0.0f;
	TestFalse(TEXT("Порог не задан — не предупреждаем"), UQuestComponent::ShouldWarnLowHealth(Quest, 10.0f));

	// Настоящая таблица заданий: порог и текст заданы у задания на бандитов и только у него.
	const AElderNPC* ElderDefaults = GetDefault<AElderNPC>();
	FQuest Wolves, Bandits;
	if (TestTrue(TEXT("Задание на волков читается из таблицы"), ContraryQuests::BuildQuestFromRow(ElderDefaults->GetOfferedQuest().QuestId, Wolves))
		&& TestTrue(TEXT("Задание на бандитов читается из таблицы"), ContraryQuests::BuildQuestFromRow(ElderDefaults->GetSecondQuest().QuestId, Bandits)))
	{
		TestEqual(TEXT("Порог здоровья у задания на бандитов — 45"), Bandits.LowHealthWarnThreshold, 45.0f);
		TestFalse(TEXT("Текст предупреждения задан"), Bandits.LowHealthWarnText.IsEmpty());
		TestFalse(TEXT("Подсказка после покупки пистолета задана"), Bandits.PrepBoughtHintText.IsEmpty());
		TestEqual(TEXT("Задание на волков о здоровье не предупреждает"), Wolves.LowHealthWarnThreshold, 0.0f);
		AddInfo(FString::Printf(TEXT("Предупреждение: %s"), *Bandits.LowHealthWarnText.ToString()));
		AddInfo(FString::Printf(TEXT("После покупки: %s"), *Bandits.PrepBoughtHintText.ToString()));
	}

	// Вживую: герой со взятым заданием и малым здоровьем; контроллер предупреждает.
	const FString Slot = TEXT("ContraryPolishLowHealthQA");
	PolishGameTest::DeleteSlot(Slot);
	UWorld* World = PolishGameTest::Create();
	ASaveTestPlayerCharacter* Player = PolishGameTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	AContrarySurvivorPlayerController* PC = PolishGameTest::Spawn<AContrarySurvivorPlayerController>(World, FVector::ZeroVector);
	if (!World || !Player || !PC || !Player->GetQuests() || !Player->GetStats())
	{
		AddError(TEXT("Не заспавнены игрок и контроллер"));
		PolishGameTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	PC->Possess(Player);
	UQuestComponent* Quests = Player->GetQuests();
	UStatsComponent* Stats = Player->GetStats();

	FQuest BanditsQa;
	BanditsQa.QuestId = FName(TEXT("QaBandits"));
	BanditsQa.KillTargetTag = FName(TEXT("Bandit"));
	BanditsQa.TargetCount = 3;
	BanditsQa.LowHealthWarnThreshold = 45.0f;
	BanditsQa.LowHealthWarnText = FText::FromString(TEXT("QA: подлечись"));
	Quests->OfferQuest(BanditsQa);

	Stats->SetHealth(30.0f);
	TestFalse(TEXT("Задание только предложено — предупреждения нет"), PC->WarnLowHealthOnLeavingVillage());
	TestTrue(TEXT("Задание берётся"), Quests->AcceptQuest(BanditsQa.QuestId));
	// У проверочного контроллера нет окна игры, и всплывающее сообщение создать не в чем: движок
	// пишет об этом ошибку ровно один раз — на единственное показанное предупреждение.
	AddExpectedErrorPlain(TEXT("CreateWidget cannot be used on Player Controller with no attached player"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("Здоровье 30, задание взято — предупреждение показано"), PC->WarnLowHealthOnLeavingVillage());
	Stats->SetHealth(Stats->GetMaxHealth());
	TestFalse(TEXT("Здоровье полное — предупреждения нет"), PC->WarnLowHealthOnLeavingVillage());
	Stats->SetHealth(30.0f);
	for (int32 Kill = 0; Kill < 3; ++Kill)
	{
		Quests->NotifyKill(BanditsQa.KillTargetTag);
	}
	TestFalse(TEXT("Бандиты перебиты — предупреждения нет"), PC->WarnLowHealthOnLeavingVillage());

	// Контроллер переживает проверочный мир до сборки мусора и при выходе из редактора записал
	// бы сохранение своего героя — отвязываем героя.
	PC->UnPossess();
	PolishGameTest::Destroy(World);
	PolishGameTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 6. Купленный пистолет при открытой торговле не мигает; подсказка после покупки — из задания.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishPistolAfterPurchaseTest,
	"ContrarySurvivor.QuestHints.BoughtPistolBlinksOnlyInPlainBag", PolishGameTestFlags)

bool FPolishPistolAfterPurchaseTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("В обычной сумке плитка предмета задания мигает"), UInventoryScreenWidget::IsEquipHintAllowed(EItemPanelMode::Normal));
	TestFalse(TEXT("При открытой торговле — не мигает"), UInventoryScreenWidget::IsEquipHintAllowed(EItemPanelMode::Trade));
	TestFalse(TEXT("При обыске — не мигает"), UInventoryScreenWidget::IsEquipHintAllowed(EItemPanelMode::Search));
	TestFalse(TEXT("У сундука — не мигает"), UInventoryScreenWidget::IsEquipHintAllowed(EItemPanelMode::Stash));

	const FString Slot = TEXT("ContraryPolishPistolHintQA");
	PolishGameTest::DeleteSlot(Slot);
	UWorld* World = PolishGameTest::Create();
	ASaveTestPlayerCharacter* Player = PolishGameTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	if (!World || !Player || !Player->GetQuests() || !Player->GetStats())
	{
		AddError(TEXT("Не заспавнен игрок"));
		PolishGameTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	UQuestComponent* Quests = Player->GetQuests();

	FQuest Bandits;
	const FName BanditsId = GetDefault<AElderNPC>()->GetSecondQuest().QuestId;
	if (!TestTrue(TEXT("Задание на бандитов читается из таблицы"), ContraryQuests::BuildQuestFromRow(BanditsId, Bandits)))
	{
		PolishGameTest::Destroy(World);
		return false;
	}
	const FContraryItemRow* PistolRow = ContraryItems::FindRow(Bandits.PrepItemRow);
	if (!TestNotNull(TEXT("Пистолет есть в таблице предметов"), PistolRow))
	{
		PolishGameTest::Destroy(World);
		return false;
	}
	Quests->OfferQuest(Bandits);
	TestTrue(TEXT("Задание берётся"), Quests->AcceptQuest(BanditsId));
	Player->UpdateQuestPrepSteps();
	TestTrue(TEXT("До покупки подсказки «что делать с купленным» нет"), Player->GetPrepBoughtHint(Bandits.PrepItemRow).IsEmpty());

	FShopEntry Pistol;
	Pistol.DisplayName = TEXT("pistol");
	Pistol.Kind = EShopEntryKind::Item;
	Pistol.ItemClass = PistolRow->ItemClass.LoadSynchronous();
	Pistol.ItemRow = Bandits.PrepItemRow;
	Pistol.Price = PistolRow->Price;
	Player->GetStats()->AddMoney(1000.0f);
	TestTrue(TEXT("Пистолет куплен"), Player->Shop_BuyEntryQty(Pistol, 1));
	const FQuest* InLog = Quests->FindQuest(BanditsId);
	TestTrue(TEXT("После покупки идёт шаг «поставь в слот»"), InLog && InLog->PrepStep == EQuestPrepStep::Equip);
	TestEqual(TEXT("Подсказка после покупки — текст из таблицы заданий"),
		Player->GetPrepBoughtHint(Bandits.PrepItemRow).ToString(), Bandits.PrepBoughtHintText.ToString());
	TestTrue(TEXT("На другой товар подсказки нет"), Player->GetPrepBoughtHint(FName(TEXT("knife"))).IsEmpty());

	// Журнал хранит копию задания: новый текст из таблицы доходит и до уже взятого задания.
	FQuest Edited = Bandits;
	Edited.Description = FText::FromString(TEXT("QA: новое описание"));
	Edited.LowHealthWarnThreshold = 50.0f;
	Quests->OfferQuest(Edited);
	InLog = Quests->FindQuest(BanditsId);
	TestEqual(TEXT("Описание взятого задания обновилось"), InLog->Description.ToString(), Edited.Description.ToString());
	TestEqual(TEXT("Порог здоровья взятого задания обновился"), InLog->LowHealthWarnThreshold, 50.0f);
	TestEqual(TEXT("Ход задания при этом не тронут"), InLog->PrepStep, EQuestPrepStep::Equip);
	TestEqual(TEXT("Состояние задания не тронуто"), InLog->State, EQuestState::Active);

	PolishGameTest::Destroy(World);
	PolishGameTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 7. Двустволка охранника: реплика называет её прямо, дальше игрока ведут в сумку, в слот и
//    к кнопке смены оружия.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishShotgunRewardGuideTest,
	"ContrarySurvivor.QuestHints.GuardNamesShotgunAndGuideLeadsToSlotAndSwitch", PolishGameTestFlags)

bool FPolishShotgunRewardGuideTest::RunTest(const FString& Parameters)
{
	// Чистое правило шагов проводника.
	FQuest Rule;
	Rule.RewardEquipItemRow = PolishGameTest::ShotgunRow;
	Rule.RewardGuideStep = EQuestRewardGuideStep::Equip;
	TestEqual(TEXT("Награда в сумке — ждём слота"),
		UQuestComponent::ComputeRewardGuideStep(Rule, EQuestPrepPossession::InBag, true), EQuestRewardGuideStep::Equip);
	TestEqual(TEXT("Награда в слоте, есть и пистолет — показываем кнопку смены оружия"),
		UQuestComponent::ComputeRewardGuideStep(Rule, EQuestPrepPossession::InSlot, true), EQuestRewardGuideStep::Switch);
	TestEqual(TEXT("Награда в слоте, другого ствола нет — менять нечего, проводник закончен"),
		UQuestComponent::ComputeRewardGuideStep(Rule, EQuestPrepPossession::InSlot, false), EQuestRewardGuideStep::Done);
	TestEqual(TEXT("Награду продали или выбросили — проводник закончен"),
		UQuestComponent::ComputeRewardGuideStep(Rule, EQuestPrepPossession::None, true), EQuestRewardGuideStep::Done);
	Rule.RewardGuideStep = EQuestRewardGuideStep::Switch;
	TestEqual(TEXT("Шаг кнопки сам не закрывается"),
		UQuestComponent::ComputeRewardGuideStep(Rule, EQuestPrepPossession::InSlot, true), EQuestRewardGuideStep::Switch);

	const FString Slot = TEXT("ContraryPolishShotgunGuideQA");
	PolishGameTest::DeleteSlot(Slot);
	UWorld* World = PolishGameTest::Create();
	ASaveTestPlayerCharacter* Player = PolishGameTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	AScientistGuardNPC* Guard = PolishGameTest::Spawn<AScientistGuardNPC>(World, FVector(3000.0, 0.0, 100.0));
	AContrarySurvivorPlayerController* PC = PolishGameTest::Spawn<AContrarySurvivorPlayerController>(World, FVector::ZeroVector);
	APistol* Pistol = PolishGameTest::Spawn<APistol>(World, FVector(0.0, 0.0, 100.0));
	if (!World || !Player || !Guard || !PC || !Pistol || !Player->GetQuests() || !Player->GetInventory())
	{
		AddError(TEXT("Не заспавнены участники проверки"));
		PolishGameTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	PC->Possess(Player);
	UQuestComponent* Quests = Player->GetQuests();
	UInventoryComponent* Inv = Player->GetInventory();

	// Настоящая таблица заданий: задание охранника лежит в ней и ведёт двустволку в слот.
	const FQuest& GuardQuest = Guard->GetOfferedQuest();
	const FName QuestId = GuardQuest.QuestId;
	FQuest FromTable;
	TestTrue(TEXT("Задание охранника читается из таблицы заданий"), ContraryQuests::BuildQuestFromRow(QuestId, FromTable));
	TestEqual(TEXT("Задание охранника — побочное"), GuardQuest.Kind, EQuestKind::Side);
	TestEqual(TEXT("Награда, которую ведут в слот, — двустволка"), GuardQuest.RewardEquipItemRow, PolishGameTest::ShotgunRow);
	TestFalse(TEXT("У задания охранника есть точка цели на карте"), GuardQuest.MapMarkerTag.IsNone());

	// В слоте уже пистолет — как у игрока, дошедшего до базы учёных.
	Pistol->DrainReserveAmmo();
	Inv->AddItem(Pistol);
	TestTrue(TEXT("Пистолет встал в слот"), Player->TryAdoptRangedWeapon(Pistol));

	// Разговор: взять задание, принести рацию, сдать.
	FNpcTalkView View;
	Guard->OnTalkOpened(Player);
	Guard->HandleTalkPrimary(Player);
	TestTrue(TEXT("Рация выдана герою"), Player->GiveItemFromRow(FName(TEXT("guard_radio")), 1) == 1);
	Quests->SyncInventoryQuests(Inv);
	Guard->OnTalkClosed(Player);
	Guard->OnTalkOpened(Player);
	Guard->BuildTalkView(Player, View);
	TestFalse(TEXT("С рацией у охранника есть кнопка сдачи"), View.PrimaryLabel.IsEmpty());
	TestFalse(TEXT("После сдачи разговор не закрывается сам — охранник говорит про награду"), Guard->HandleTalkPrimary(Player));
	TestTrue(TEXT("Задание сдано"), Quests->IsQuestTurnedIn(QuestId));
	Guard->BuildTalkView(Player, View);
	TestTrue(*FString::Printf(TEXT("Сразу после сдачи охранник прямо называет двустволку: «%s»"), *View.Replica.ToString()),
		View.Replica.ToString().Contains(TEXT("двустволк")));
	const FString JustTurnedIn = View.Replica.ToString();

	// Двустволка в сумке: проводник ведёт к слоту.
	AMasterInventoryItem* Shotgun = nullptr;
	for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
	{
		if (APlayerCharacter::DoesItemMatchRow(Item, PolishGameTest::ShotgunRow))
		{
			Shotgun = Item;
		}
	}
	if (!TestNotNull(TEXT("Двустволка лежит в сумке"), Shotgun))
	{
		PolishGameTest::Destroy(World);
		PolishGameTest::DeleteSlot(Slot);
		return false;
	}
	Player->UpdateQuestPrepSteps();
	TestTrue(TEXT("После разговора мигает кнопка сумки (идёт шаг «поставь награду в слот»)"),
		Quests->IsAnyRewardGuideStep(EQuestRewardGuideStep::Equip));
	TestTrue(TEXT("Плитка двустволки в сумке мигает"), Player->IsQuestPrepItemToEquip(Shotgun));
	TestFalse(TEXT("Кнопка смены оружия ещё не мигает"), Quests->IsAnyRewardGuideStep(EQuestRewardGuideStep::Switch));

	// Следующий разговор: охранник двустволку заново не «вручает».
	Guard->OnTalkClosed(Player);
	Guard->OnTalkOpened(Player);
	Guard->BuildTalkView(Player, View);
	TestNotEqual(TEXT("В следующем разговоре реплика уже другая"), View.Replica.ToString(), JustTurnedIn);
	TestFalse(TEXT("И «держи двустволку» в ней нет"), View.Replica.ToString().Contains(TEXT("Держи двустволку")));
	Guard->OnTalkClosed(Player);

	// Игрок ставит двустволку в слот: плитка гаснет, мигает кнопка смены оружия.
	TestTrue(TEXT("Двустволка встала в слот длинного оружия"), Player->TryAdoptRangedWeapon(Shotgun));
	Player->UpdateQuestPrepSteps();
	TestFalse(TEXT("Кнопке сумки мигать больше незачем"), Quests->IsAnyRewardGuideStep(EQuestRewardGuideStep::Equip));
	TestTrue(TEXT("Мигает кнопка смены оружия"), Quests->IsAnyRewardGuideStep(EQuestRewardGuideStep::Switch));

	// Шаг переживает сохранение: журнал пишется целиком.
	const TArray<FQuest> Snapshot = Quests->GetQuests();
	Quests->RestoreQuests(Snapshot);
	TestTrue(TEXT("После загрузки журнала кнопка смены оружия всё ещё мигает"),
		Quests->IsAnyRewardGuideStep(EQuestRewardGuideStep::Switch));

	// Первое нажатие кнопки смены оружия гасит мигание.
	AMasterWeapon* InHandsBefore = Player->GetCurrentWeapon();
	PC->TouchSwitchWeapon();
	TestTrue(TEXT("Кнопка сменила оружие в руках"), Player->GetCurrentWeapon() != InHandsBefore);
	TestFalse(TEXT("После первого нажатия кнопка смены оружия не мигает"),
		Quests->IsAnyRewardGuideStep(EQuestRewardGuideStep::Switch));
	TestFalse(TEXT("Проводник закончен"), Quests->HasPendingRewardGuide());
	PC->TouchSwitchWeapon();
	TestFalse(TEXT("Повторные нажатия мигание не возвращают"), Quests->HasPendingRewardGuide());

	PC->UnPossess(); // см. проверку здоровья: иначе контроллер записал бы сохранение при выходе
	PolishGameTest::Destroy(World);
	PolishGameTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 8. Тексты: слова, на которые пожаловался Ринат, из реплик ушли; староста говорит про
//    отметку на карте; задания старосты и охранника лежат в таблице заданий.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishTextsTest,
	"ContrarySurvivor.Texts.PlainRussianAndQuestsLiveInTheTable", PolishGameTestFlags)

bool FPolishTextsTest::RunTest(const FString& Parameters)
{
	const AElderNPC* Elder = GetDefault<AElderNPC>();
	const AScientistGuardNPC* Guard = GetDefault<AScientistGuardNPC>();

	// Все четыре задания читаются из таблицы заданий — тексты правятся там, без кода.
	TArray<FString> Texts;
	const FName QuestIds[] = { Elder->GetOfferedQuest().QuestId, Elder->GetSecondQuest().QuestId,
		ContraryQuestIds::DeliverLaptopToScientists, ContraryQuestIds::FetchGuardRadio };
	for (const FName& QuestId : QuestIds)
	{
		FQuest Quest;
		if (TestTrue(*FString::Printf(TEXT("Задание %s лежит в таблице заданий"), *QuestId.ToString()),
			ContraryQuests::BuildQuestFromRow(QuestId, Quest)))
		{
			TestFalse(*FString::Printf(TEXT("У задания %s есть название"), *QuestId.ToString()), Quest.Title.IsEmpty());
			TestFalse(*FString::Printf(TEXT("У задания %s есть описание"), *QuestId.ToString()), Quest.Description.IsEmpty());
			Texts.Add(Quest.Description.ToString());
		}
	}

	bool bMapMarkSaid = false;
	for (const FElderIntroLine& Line : Elder->GetIntroLines())
	{
		Texts.Add(Line.NPCText.ToString());
	}
	for (const FElderIntroLine& Line : Elder->GetNotebookHintLines())
	{
		Texts.Add(Line.NPCText.ToString());
		bMapMarkSaid |= Line.NPCText.ToString().Contains(TEXT("на твоей карте"));
	}
	TestTrue(TEXT("После сдачи ноутбука староста говорит, что отметил логово на карте героя"), bMapMarkSaid);
	Texts.Add(Elder->GetDialogueCompletedText().ToString());
	Texts.Add(Elder->GetNotebookHintRepeatText().ToString());
	Texts.Add(Elder->GetThirdQuestCarryText().ToString());
	Texts.Add(Elder->GetThirdQuestDoneText().ToString());
	Texts.Add(Guard->GetNotVouchedText().ToString());

	// Слова, которые Ринат назвал «хренью»: устаревшие и просторечные обороты.
	const TCHAR* Banned[] = { TEXT("ретьего дня"), TEXT("околиц"), TEXT("Кабы "), TEXT("привечают"), TEXT("не сладил"),
		TEXT("Не сладил"), TEXT("по-людски"), TEXT("обчёлся"), TEXT("справишь"), TEXT("Добро.") };
	for (const FString& Text : Texts)
	{
		for (const TCHAR* Word : Banned)
		{
			TestFalse(*FString::Printf(TEXT("В реплике нет оборота «%s»: %s"), Word, *Text), Text.Contains(Word, ESearchCase::CaseSensitive));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
