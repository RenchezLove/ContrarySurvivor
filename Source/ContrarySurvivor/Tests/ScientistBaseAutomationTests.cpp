// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты базы учёных (поручение Рината 10.10.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Scientists; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат:
//   - третье задание старосты: появляется только после сценки про ноутбук (то есть в следующем
//     разговоре), выдаёт ноутбук при взятии, старосте не сдаётся, сдаётся квестовому учёному
//     (ноутбук изымается, монеты начисляются); старое сохранение получает его так же;
//   - задание охранника: выдача, появление рации на месте, подбор, награда двустволкой и
//     четырьмя патронами в сумку при занятом слоте оружия, возврат всего к началу в новой игре;
//   - учёный со шлемом: плата только за досмотренный показ, три подряд и перерыв, отказ без
//     платы, когда показ не готов;
//   - прайс-лист учёного-торговца и покупка двустволки;
//   - запас патронов при стволе не дарится заново при каждом «Продолжить».
//
// Разговор проверяется теми же вызовами, что делает окно (ATalkerNPC::BuildTalkView и
// HandleTalkPrimary) — живой Slate в прогонах проекта не поднимается.
// Таблицы предметов и заданий на время теста ПОДМЕНЯЮТСЯ своими (боевые таблицы не читаются
// и не меняются). Слот сохранения ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Actors/QuestItemSpawner.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Actors/TalkerNPC.h"
#include "ContrarySurvivor/Ads/AdSeriesLogic.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryItemRow.h"
#include "ContrarySurvivor/Data/ContraryQuestRow.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "UInventoryComponent.h"
#include "AAmmoItem.h"
#include "AConsumableItem.h"
#include "APistol.h"
#include "AQuestItem.h"
#include "ARangedWeapon.h"
#include "AShotgun.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h" // GetTransientPackage
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags ScientistTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace ScientistTest
{
	static const FName LaptopRow(TEXT("laptop"));
	static const FName RadioRow(TEXT("guard_radio"));
	static const FName ShotgunItemRow(TEXT("shotgun_toz34"));
	static const TCHAR* LaptopKey = TEXT("Ноутбук");

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
	static T* Spawn(UWorld* World, const FVector& Loc)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params);
	}

	// Слепок таблицы предметов: только то, на что ссылаются задания и прайс-лист базы учёных.
	static UDataTable* MakeItemTable()
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_ItemsForScientistQA"));
		Table->RowStruct = FContraryItemRow::StaticStruct();

		FContraryItemRow Laptop;
		Laptop.LegacyKey = LaptopKey; // прежний служебный ключ ноутбука — по нему считает задание
		Laptop.DisplayText = FText::FromString(TEXT("QA ноутбук"));
		Laptop.ItemClass = AQuestItem::StaticClass();
		Laptop.Category = EItemCategory::Quest;
		Laptop.MaxStackCount = 999;
		Table->AddRow(LaptopRow, Laptop);

		FContraryItemRow Radio; // новый предмет: колонка старого ключа пустая, ключ = имя строки
		Radio.DisplayText = FText::FromString(TEXT("QA рация"));
		Radio.ItemClass = AQuestItem::StaticClass();
		Radio.Category = EItemCategory::Quest;
		Radio.MaxStackCount = 999;
		Table->AddRow(RadioRow, Radio);

		FContraryItemRow Shotgun;
		Shotgun.DisplayText = FText::FromString(TEXT("QA двустволка"));
		Shotgun.ItemClass = AShotgun::StaticClass();
		Shotgun.Category = EItemCategory::Weapon;
		Shotgun.MaxStackCount = 1;
		Shotgun.Price = 300.0f;
		Table->AddRow(ShotgunItemRow, Shotgun);

		FContraryItemRow Shells;
		Shells.DisplayText = FText::FromString(TEXT("QA дробь"));
		Shells.ItemClass = AShotgunAmmoItem::StaticClass();
		Shells.Category = EItemCategory::Resource;
		Shells.MaxStackCount = 999;
		Shells.Price = 5.0f;
		Table->AddRow(ContraryAmmo::Shotgun12gType(), Shells);

		FContraryItemRow Bandage;
		Bandage.LegacyKey = AConsumableItem::GetDefaultDisplayName(EConsumableType::Medkit);
		Bandage.ItemClass = AConsumableItem::StaticClass();
		Bandage.Category = EItemCategory::Consumable;
		Bandage.MaxStackCount = 999;
		Bandage.ConsumableType = EConsumableType::Medkit;
		Bandage.Price = 12.0f;
		Table->AddRow(FName(TEXT("bandage")), Bandage);

		FContraryItemRow Water = Bandage;
		Water.LegacyKey = AConsumableItem::GetDefaultDisplayName(EConsumableType::Water);
		Water.ConsumableType = EConsumableType::Water;
		Water.Price = 5.0f;
		Table->AddRow(FName(TEXT("water_bottle")), Water);

		FContraryItemRow Food = Bandage;
		Food.LegacyKey = AConsumableItem::GetDefaultDisplayName(EConsumableType::Food);
		Food.ConsumableType = EConsumableType::Food;
		Food.Price = 12.0f;
		Table->AddRow(FName(TEXT("canned_food")), Food);
		return Table;
	}

	// Подмена обеих таблиц в настройках проекта на время теста (CDO переживает прогон — вернуть).
	// Таблица заданий подменяется ПУСТОЙ: задания берутся из кода, боевые строки не мешают.
	struct FScopedTables
	{
		UContraryDataSettings* Settings;
		TSoftObjectPtr<UDataTable> SavedItems;
		TSoftObjectPtr<UDataTable> SavedQuests;
		UDataTable* Items;
		UDataTable* Quests;

		FScopedTables()
			: Settings(GetMutableDefault<UContraryDataSettings>())
		{
			SavedItems = Settings->ItemTable;
			SavedQuests = Settings->QuestTable;
			Items = MakeItemTable();
			Quests = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_QuestsForScientistQA"));
			Quests->RowStruct = FContraryQuestRow::StaticStruct();
			Items->AddToRoot();
			Quests->AddToRoot();
			Settings->ItemTable = Items;
			Settings->QuestTable = Quests;
		}
		~FScopedTables()
		{
			Settings->ItemTable = SavedItems;
			Settings->QuestTable = SavedQuests;
			Items->RemoveFromRoot();
			Quests->RemoveFromRoot();
		}
	};

	// Сколько штук предмета с таким служебным ключом лежит в сумке.
	static int32 CountInBag(const APlayerCharacter* Player, const FString& Key)
	{
		int32 Total = 0;
		const UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (Inv)
		{
			for (const AMasterInventoryItem* Item : Inv->GetInventoryItems())
			{
				if (Item && Item->ItemName.Equals(Key, ESearchCase::CaseSensitive))
				{
					Total += FMath::Max(1, Item->GetStackCount());
				}
			}
		}
		return Total;
	}

	static AShotgun* FindShotgunInBag(const APlayerCharacter* Player)
	{
		const UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (Inv)
		{
			for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
			{
				if (AShotgun* Gun = Cast<AShotgun>(Item))
				{
					return Gun;
				}
			}
		}
		return nullptr;
	}

	static EQuestState StateOf(const APlayerCharacter* Player, FName QuestId)
	{
		const FQuest* Q = (Player && Player->GetQuests()) ? Player->GetQuests()->FindQuest(QuestId) : nullptr;
		return Q ? Q->State : EQuestState::NotStarted;
	}

	static bool InJournal(const APlayerCharacter* Player, FName QuestId)
	{
		return Player && Player->GetQuests() && Player->GetQuests()->FindQuest(QuestId) != nullptr;
	}

	static void SetNotebookHintShown(const APlayerCharacter* Player, bool bShown)
	{
		if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			Save->bElderNotebookHintShown = bShown;
			Player->WriteSaveObject(Save);
		}
	}

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}
}

// ===========================================================================
// 1. Третье задание старосты: выдача только после сценки, ноутбук при взятии, сдача учёному.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScientistThirdQuestTest,
	"ContrarySurvivor.Scientists.ThirdQuestOfferedLaterAndTurnedInToScientist", ScientistTestFlags)

bool FScientistThirdQuestTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryScientistQ3QA");
	ScientistTest::DeleteSlot(Slot);
	ScientistTest::FScopedTables Tables;

	// Чистое правило открытия.
	TestFalse(TEXT("Второе задание не сдано — третьего нет"),
		AElderNPC::ShouldUnlockThirdQuest(false, true, true, true));
	TestFalse(TEXT("Сценка про ноутбук ещё не показана — третьего нет"),
		AElderNPC::ShouldUnlockThirdQuest(true, false, true, true));
	TestTrue(TEXT("Второе сдано и сценка показана — третье открывается"),
		AElderNPC::ShouldUnlockThirdQuest(true, true, true, true));
	TestTrue(TEXT("Сценки у старосты нет вовсе — ждать нечего"),
		AElderNPC::ShouldUnlockThirdQuest(true, false, false, true));

	UWorld* World = ScientistTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	ASaveTestPlayerCharacter* Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AElderNPC* Elder = ScientistTest::Spawn<AElderNPC>(World, FVector(3000.0f, 0.0f, 100.0f));
	AScientistQuestNPC* Scientist = ScientistTest::Spawn<AScientistQuestNPC>(World, FVector(9000.0f, 0.0f, 100.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
	if (!Player || !Elder || !Scientist || !Quests || !Inv || !Player->GetStats())
	{
		AddError(TEXT("Не заспавнены игрок, староста или учёный"));
		ScientistTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	const FName Q3 = Elder->GetThirdQuest().QuestId;
	TestFalse(TEXT("У третьего задания есть идентификатор"), Q3.IsNone());
	TestEqual(TEXT("Третье задание сдаётся квестовому учёному"),
		Elder->GetThirdQuest().TurnInNpcTag, Scientist->GetNpcTag());

	// Учёный до ноутбука: нейтральная реплика, основной кнопки нет.
	Scientist->OnTalkOpened(Player);
	FNpcTalkView View;
	Scientist->BuildTalkView(Player, View);
	TestTrue(TEXT("Без ноутбука у учёного нет основной кнопки"), View.PrimaryLabel.IsEmpty());
	TestFalse(TEXT("Без ноутбука учёный что-то говорит"), View.Replica.IsEmpty());
	const FString IdleReplica = View.Replica.ToString();

	// Старое сохранение: первые два задания давно сданы, третьего в журнале нет.
	FQuest Q1Done = Elder->GetOfferedQuest();
	Q1Done.State = EQuestState::TurnedIn;
	FQuest Q2Done = Elder->GetSecondQuest();
	Q2Done.State = EQuestState::TurnedIn;
	Quests->RestoreQuests({ Q1Done, Q2Done });

	// Сценка ещё не показана (тот разговор, в котором сдан ноутбук) — третьего задания нет.
	ScientistTest::SetNotebookHintShown(Player, false);
	TestFalse(TEXT("До сценки третье задание не открывается"), Elder->TryUnlockThirdQuest(Player));
	TestFalse(TEXT("До сценки третьего задания нет в журнале"), ScientistTest::InJournal(Player, Q3));
	TestEqual(TEXT("До сценки староста говорит о втором задании"),
		Elder->GetQuestForPlayer(Quests).QuestId, Elder->GetSecondQuest().QuestId);

	// Сценка показана — в СЛЕДУЮЩЕМ разговоре задание открывается.
	ScientistTest::SetNotebookHintShown(Player, true);
	TestTrue(TEXT("После сценки третье задание открывается"), Elder->TryUnlockThirdQuest(Player));
	TestFalse(TEXT("Повторное открытие ничего не делает"), Elder->TryUnlockThirdQuest(Player));
	TestEqual(TEXT("Третье задание предложено, но не взято"), ScientistTest::StateOf(Player, Q3), EQuestState::NotStarted);
	TestEqual(TEXT("Староста говорит о третьем задании"), Elder->GetQuestForPlayer(Quests).QuestId, Q3);
	TestEqual(TEXT("До взятия ноутбука в сумке нет"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 0);

	// Взятие: староста отдаёт ноутбук.
	TestTrue(TEXT("Третье задание берётся"), Quests->AcceptQuest(Q3));
	TestEqual(TEXT("При взятии герой получил ноутбук"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 1);
	Quests->SyncInventoryQuests(Inv);
	TestEqual(TEXT("С ноутбуком в сумке задание готово к сдаче"), ScientistTest::StateOf(Player, Q3), EQuestState::Completed);
	if (const FQuest* InLog = Quests->FindQuest(Q3))
	{
		TestEqual(TEXT("Для старосты готовое задание остаётся «взято»"),
			AElderNPC::GetElderDialogState(*InLog), EQuestState::Active);
	}

	// Старосте оно не сдаётся (его окно зовёт сдачу без метки собеседника).
	const float MoneyBefore = Player->GetStats()->GetMoney();
	TestFalse(TEXT("Староста третье задание принять не может"), Quests->TurnInQuest(Q3));
	TestEqual(TEXT("Отказ старосты ноутбук не изъял"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 1);
	TestEqual(TEXT("Отказ старосты монет не дал"), Player->GetStats()->GetMoney(), MoneyBefore);

	// Учёный: принимает ноутбук.
	Scientist->OnTalkOpened(Player);
	Scientist->BuildTalkView(Player, View);
	TestFalse(TEXT("С ноутбуком у учёного есть кнопка сдачи"), View.PrimaryLabel.IsEmpty());
	TestNotEqual(TEXT("С ноутбуком учёный говорит другое"), View.Replica.ToString(), IdleReplica);
	TestFalse(TEXT("После сдачи разговор не закрывается сам"), Scientist->HandleTalkPrimary(Player));
	TestEqual(TEXT("Задание сдано учёному"), ScientistTest::StateOf(Player, Q3), EQuestState::TurnedIn);
	TestEqual(TEXT("Ноутбук изъят"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 0);
	const float Reward = Elder->GetThirdQuest().RewardMoney;
	TestTrue(TEXT("У третьего задания есть награда монетами"), Reward > 0.0f);
	TestEqual(TEXT("Награда начислена"), Player->GetStats()->GetMoney(), MoneyBefore + Reward);

	Scientist->BuildTalkView(Player, View);
	TestTrue(TEXT("Сразу после сдачи кнопки сдачи уже нет"), View.PrimaryLabel.IsEmpty());
	TestTrue(TEXT("Учёный называет сумму награды"),
		View.Replica.ToString().Contains(FString::FromInt(FMath::RoundToInt32(Reward))));

	// Конец сюжета объявляется при закрытии ЭТОГО разговора, один раз.
	if (const FQuest* Done = Quests->FindQuest(Q3))
	{
		TestTrue(TEXT("Сдача ноутбука учёному — задание, которым кончается сюжет"), AQuestGiverNPC::IsStoryEndingQuest(*Done));
	}
	TestFalse(TEXT("Второе задание старосты сюжет не заканчивает"), AQuestGiverNPC::IsStoryEndingQuest(Q2Done));
	TestTrue(TEXT("После приёма ноутбука закрытие разговора объявляет конец сюжета"), Scientist->ShouldAnnounceStoryEndOnClose());
	Scientist->OnTalkClosed(Player);
	TestFalse(TEXT("Конец сюжета объявляется один раз"), Scientist->ShouldAnnounceStoryEndOnClose());

	// Следующий разговор — заглушка «нужно время», без кнопок.
	Scientist->OnTalkOpened(Player);
	Scientist->BuildTalkView(Player, View);
	TestTrue(TEXT("После сдачи у учёного нет основной кнопки"), View.PrimaryLabel.IsEmpty());
	TestNotEqual(TEXT("После сдачи учёный говорит не нейтральную реплику"), View.Replica.ToString(), IdleReplica);

	// Староста после сдачи: задание остаётся третьим и считается сданным.
	TestEqual(TEXT("После сдачи староста говорит о третьем задании"), Elder->GetQuestForPlayer(Quests).QuestId, Q3);
	TestFalse(TEXT("Сданное задание заново не открывается"), Elder->TryUnlockThirdQuest(Player));

	ScientistTest::Destroy(World);
	ScientistTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 2. Задание охранника: выдача, рация на месте, подбор, награда, новая игра.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScientistGuardQuestTest,
	"ContrarySurvivor.Scientists.GuardRadioQuestRewardAndNewGame", ScientistTestFlags)

bool FScientistGuardQuestTest::RunTest(const FString& Parameters)
{
	// Чистое правило «лежит ли предмет».
	TestFalse(TEXT("Задание не взято — рации нет"),
		AQuestItemSpawner::ShouldItemLie(false, EQuestState::NotStarted, false, true));
	TestFalse(TEXT("Задание только предложено — рации нет"),
		AQuestItemSpawner::ShouldItemLie(true, EQuestState::NotStarted, false, true));
	TestTrue(TEXT("Задание взято — рация лежит"),
		AQuestItemSpawner::ShouldItemLie(true, EQuestState::Active, false, true));
	TestFalse(TEXT("Рация уже в сумке — вторую не кладём"),
		AQuestItemSpawner::ShouldItemLie(true, EQuestState::Active, true, true));
	TestFalse(TEXT("Задание сдано — рации нет"),
		AQuestItemSpawner::ShouldItemLie(true, EQuestState::TurnedIn, false, true));
	TestTrue(TEXT("Переключатель выключен — рация лежит с самого начала"),
		AQuestItemSpawner::ShouldItemLie(false, EQuestState::NotStarted, false, false));
	TestFalse(TEXT("Переключатель выключен, задание сдано — рации нет"),
		AQuestItemSpawner::ShouldItemLie(true, EQuestState::TurnedIn, false, false));

	const FString Slot = TEXT("ContraryScientistGuardQA");
	ScientistTest::DeleteSlot(Slot);
	ScientistTest::FScopedTables Tables;
	UWorld* World = ScientistTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// ⛔ Игрок с изолированным слотом: подсказки и подбор могут писать в сохранение.
	ASaveTestPlayerCharacter* Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AScientistGuardNPC* Guard = ScientistTest::Spawn<AScientistGuardNPC>(World, FVector(3000.0f, 0.0f, 100.0f));
	AQuestItemSpawner* Spot = ScientistTest::Spawn<AQuestItemSpawner>(World, FVector(-6000.0f, 0.0f, 100.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
	if (!Player || !Guard || !Spot || !Quests || !Inv)
	{
		AddError(TEXT("Не заспавнены игрок, охранник или место рации"));
		ScientistTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	const FQuest& GuardQuest = Guard->GetOfferedQuest();
	const FName QuestId = GuardQuest.QuestId;
	const FString RadioKey = ScientistTest::RadioRow.ToString();
	TestEqual(TEXT("Задание охранника требует рацию"), GuardQuest.RequiredItemName, RadioKey);
	TestEqual(TEXT("Задание охранника сдаётся ему самому"), GuardQuest.TurnInNpcTag, Guard->GetNpcTag());
	TestTrue(TEXT("Место рации несёт метку цели задания"), Spot->ActorHasTag(GuardQuest.MapMarkerTag));
	TestTrue(TEXT("Охранник несёт свою метку собеседника"), Guard->ActorHasTag(Guard->GetNpcTag()));

	// В слоте оружия уже пистолет: награда-ружьё обязана лечь в сумку, а не вытеснить его.
	APistol* Pistol = ScientistTest::Spawn<APistol>(World, FVector(0.0f, 0.0f, 100.0f));
	if (!Pistol)
	{
		AddError(TEXT("Не заспавнен пистолет"));
		ScientistTest::Destroy(World);
		return false;
	}
	Pistol->DrainReserveAmmo();
	Inv->AddItem(Pistol);
	TestTrue(TEXT("Пистолет встал в слот оружия"), Player->TryAdoptRangedWeapon(Pistol));

	// До разговора: рации на месте нет.
	Spot->RefreshNow();
	TestNull(TEXT("До взятия задания рации на месте нет"), Spot->GetSpawnedPickup());

	// Разговор: предложение -> взятие.
	FNpcTalkView View;
	Guard->OnTalkOpened(Player);
	TestEqual(TEXT("После первого разговора задание в журнале"), ScientistTest::InJournal(Player, QuestId), true);
	Guard->BuildTalkView(Player, View);
	TestFalse(TEXT("Охранник предлагает задание кнопкой"), View.PrimaryLabel.IsEmpty());
	TestEqual(TEXT("Реплика предложения — описание задания"), View.Replica.ToString(), GuardQuest.Description.ToString());
	TestFalse(TEXT("После взятия разговор не закрывается сам"), Guard->HandleTalkPrimary(Player));
	TestEqual(TEXT("Задание взято"), ScientistTest::StateOf(Player, QuestId), EQuestState::Active);
	Guard->BuildTalkView(Player, View);
	TestTrue(TEXT("Пока рации нет, основной кнопки нет"), View.PrimaryLabel.IsEmpty());

	// Рация появляется на месте.
	Spot->RefreshNow();
	APickup* RadioBag = Spot->GetSpawnedPickup();
	TestNotNull(TEXT("После взятия задания рация лежит на месте"), RadioBag);
	if (!RadioBag)
	{
		ScientistTest::Destroy(World);
		return false;
	}
	TestTrue(TEXT("В мешке на месте что-то есть"), RadioBag->HasLoot());
	Spot->RefreshNow();
	TestTrue(TEXT("Повторная сверка второй мешок не кладёт"), Spot->GetSpawnedPickup() == RadioBag);

	// Подбор.
	TestTrue(TEXT("Рация подобрана"), RadioBag->Collect(Player));
	TestEqual(TEXT("Рация в сумке"), ScientistTest::CountInBag(Player, RadioKey), 1);
	Quests->SyncInventoryQuests(Inv);
	TestEqual(TEXT("С рацией задание готово к сдаче"), ScientistTest::StateOf(Player, QuestId), EQuestState::Completed);
	Spot->RefreshNow();
	TestNull(TEXT("Пока рация в сумке, новая на месте не появляется"), Spot->GetSpawnedPickup());

	// Сдача: рация изымается, в сумке двустволка и четыре патрона, пистолет остался в слоте.
	const FName Type12 = ContraryAmmo::Shotgun12gType();
	TestEqual(TEXT("До награды дроби в сумке нет"), Player->GetReserveAmmoInInventory(Type12), 0);
	Guard->BuildTalkView(Player, View);
	TestFalse(TEXT("С рацией у охранника есть кнопка сдачи"), View.PrimaryLabel.IsEmpty());
	Guard->HandleTalkPrimary(Player);
	TestEqual(TEXT("Задание охранника сдано"), ScientistTest::StateOf(Player, QuestId), EQuestState::TurnedIn);
	TestEqual(TEXT("Рация изъята"), ScientistTest::CountInBag(Player, RadioKey), 0);
	const AShotgun* RewardGun = ScientistTest::FindShotgunInBag(Player);
	TestNotNull(TEXT("Двустволка лежит в сумке"), RewardGun);
	if (RewardGun)
	{
		TestEqual(TEXT("Ружьё-награда без скрытого запаса патронов"), RewardGun->GetCurrentAmmoReserve(), 0);
	}
	TestEqual(TEXT("В награду ровно четыре патрона"), Player->GetReserveAmmoInInventory(Type12), 4);
	TestTrue(TEXT("Пистолет остался в слоте оружия"), Player->GetRangedWeaponInstance() == Pistol);
	Guard->BuildTalkView(Player, View);
	TestTrue(TEXT("После сдачи кнопок задания нет"), View.PrimaryLabel.IsEmpty());
	Spot->RefreshNow();
	TestNull(TEXT("После сдачи рация на месте не появляется"), Spot->GetSpawnedPickup());

	// Новая игра в живом мире: журнал пуст — всё возвращается к началу.
	Quests->RestoreQuests(TArray<FQuest>());
	Spot->RefreshNow();
	TestNull(TEXT("Новая игра: до взятия задания рации нет"), Spot->GetSpawnedPickup());
	Guard->OnTalkOpened(Player);
	TestEqual(TEXT("Новая игра: охранник снова предлагает задание"),
		ScientistTest::StateOf(Player, QuestId), EQuestState::NotStarted);
	Guard->HandleTalkPrimary(Player);
	Spot->RefreshNow();
	TestNotNull(TEXT("Новая игра: рация снова на месте"), Spot->GetSpawnedPickup());

	ScientistTest::Destroy(World);
	ScientistTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 3. Учёный со шлемом: плата за досмотренный показ, три подряд и перерыв.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScientistHelmetAdTest,
	"ContrarySurvivor.Scientists.HelmetAdThreeInRowThenBreak", ScientistTestFlags)

bool FScientistHelmetAdTest::RunTest(const FString& Parameters)
{
	// --- Чистое правило серии ---
	const FDateTime T0(2026, 10, 10, 12, 0, 0);
	const double Break = 240.0;
	FAdSeriesState Series;
	TestTrue(TEXT("Первый просмотр доступен"), AdSeries::CanWatch(Series, T0, 3, Break));
	AdSeries::RegisterView(Series, T0, Break);
	AdSeries::RegisterView(Series, T0 + FTimespan::FromSeconds(40.0), Break);
	TestTrue(TEXT("После двух просмотров третий доступен"),
		AdSeries::CanWatch(Series, T0 + FTimespan::FromSeconds(60.0), 3, Break));
	const FDateTime Third = T0 + FTimespan::FromSeconds(80.0);
	AdSeries::RegisterView(Series, Third, Break);
	TestFalse(TEXT("После трёх подряд — перерыв"),
		AdSeries::CanWatch(Series, Third + FTimespan::FromSeconds(1.0), 3, Break));
	TestEqual(TEXT("Осталось ждать ровно остаток перерыва"),
		AdSeries::BreakSecondsLeft(Series, Third + FTimespan::FromSeconds(100.0), 3, Break), 140.0);
	TestFalse(TEXT("За секунду до конца перерыва ещё нельзя"),
		AdSeries::CanWatch(Series, Third + FTimespan::FromSeconds(239.0), 3, Break));
	TestTrue(TEXT("Перерыв прошёл — снова можно"),
		AdSeries::CanWatch(Series, Third + FTimespan::FromSeconds(240.0), 3, Break));
	TestFalse(TEXT("Часы перевели назад — перерыв не обойти"),
		AdSeries::CanWatch(Series, Third - FTimespan::FromSeconds(3600.0), 3, Break));
	AdSeries::RegisterView(Series, Third + FTimespan::FromSeconds(300.0), Break);
	TestEqual(TEXT("После перерыва серия начинается заново"), Series.ViewsInSeries, 1);

	// Два просмотра, долгая пауза, ещё один: серия считается заново, перерыва нет.
	FAdSeriesState Slow;
	AdSeries::RegisterView(Slow, T0, Break);
	AdSeries::RegisterView(Slow, T0 + FTimespan::FromSeconds(30.0), Break);
	AdSeries::RegisterView(Slow, T0 + FTimespan::FromSeconds(3600.0), Break);
	TestEqual(TEXT("Просмотр после долгой паузы — первый в новой серии"), Slow.ViewsInSeries, 1);

	// --- Персонаж ---
	const FString Slot = TEXT("ContraryScientistHelmetQA");
	ScientistTest::DeleteSlot(Slot);
	UWorld* World = ScientistTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	// ⛔ Только игрок с изолированным слотом: плата зеркалится в сохранение.
	ASaveTestPlayerCharacter* Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AScientistHelmetNPC* Helmet = ScientistTest::Spawn<AScientistHelmetNPC>(World, FVector(3000.0f, 0.0f, 100.0f));
	if (!Player || !Helmet || !Player->GetStats())
	{
		AddError(TEXT("Не заспавнены игрок или учёный со шлемом"));
		ScientistTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	const float Pay = Helmet->GetRewardMoney();
	TestTrue(TEXT("Плата за просмотр положительная"), Pay > 0.0f);
	const float Money0 = Player->GetStats()->GetMoney();

	// Предложение: две кнопки.
	FNpcTalkView View;
	Helmet->OnTalkOpened(Player);
	Helmet->BuildTalkView(Player, View);
	TestFalse(TEXT("Учёный предлагает испытание кнопкой"), View.PrimaryLabel.IsEmpty());
	TestFalse(TEXT("У предложения есть ответ-отказ"), View.CloseLabel.IsEmpty());
	TestTrue(TEXT("В предложении названа плата"),
		View.Replica.ToString().Contains(FString::FromInt(FMath::RoundToInt32(Pay))));
	const FString OfferReplica = View.Replica.ToString();

	// Показ не готов (в прогоне без игры слоя рекламы нет): вежливый отказ, платы нет.
	TestFalse(TEXT("Согласие разговор не закрывает"), Helmet->HandleTalkPrimary(Player));
	TestTrue(TEXT("Показ не готов — «прибор не готов»"), Helmet->GetStage() == EHelmetTalkStage::NotReady);
	TestEqual(TEXT("Показ не готов — платы нет"), Player->GetStats()->GetMoney(), Money0);
	Helmet->BuildTalkView(Player, View);
	TestTrue(TEXT("При неготовом показе основной кнопки нет"), View.PrimaryLabel.IsEmpty());
	TestNotEqual(TEXT("При неготовом показе реплика другая"), View.Replica.ToString(), OfferReplica);

	// Недосмотренный показ: платы нет, в серию не идёт.
	const FDateTime Now = FDateTime::UtcNow();
	Helmet->CompleteExperiment(Player, /*bWatched=*/false, Now);
	TestTrue(TEXT("Недосмотрел — «запись не засчитана»"), Helmet->GetStage() == EHelmetTalkStage::NotFinished);
	TestEqual(TEXT("Недосмотрел — платы нет"), Player->GetStats()->GetMoney(), Money0);

	// Три досмотренных подряд: плата за каждый, затем перерыв.
	Helmet->CompleteExperiment(Player, true, Now);
	TestTrue(TEXT("Досмотрел — заплатили"), Helmet->GetStage() == EHelmetTalkStage::Paid);
	TestEqual(TEXT("Плата за первый просмотр"), Player->GetStats()->GetMoney(), Money0 + Pay);
	Helmet->BuildTalkView(Player, View);
	TestFalse(TEXT("После первого просмотра предлагают ещё раз"), View.PrimaryLabel.IsEmpty());
	Helmet->CompleteExperiment(Player, true, Now);
	Helmet->CompleteExperiment(Player, true, Now);
	TestEqual(TEXT("Плата за три просмотра"), Player->GetStats()->GetMoney(), Money0 + Pay * 3.0f);
	TestFalse(TEXT("После трёх подряд начать нельзя"), Helmet->CanStartExperiment(Now + FTimespan::FromSeconds(30.0)));
	TestTrue(TEXT("Через четыре минуты снова можно"), Helmet->CanStartExperiment(Now + FTimespan::FromMinutes(4.0)));
	Helmet->BuildTalkView(Player, View);
	TestTrue(TEXT("После третьего просмотра кнопки «ещё раз» нет"), View.PrimaryLabel.IsEmpty());

	// Новый разговор во время перерыва: «прибор остывает», кнопки нет, платы нет.
	Helmet->OnTalkOpened(Player);
	Helmet->BuildTalkView(Player, View);
	TestTrue(TEXT("Во время перерыва основной кнопки нет"), View.PrimaryLabel.IsEmpty());
	TestNotEqual(TEXT("Во время перерыва учёный не предлагает испытание"), View.Replica.ToString(), OfferReplica);
	Helmet->HandleTalkPrimary(Player);
	TestEqual(TEXT("Во время перерыва денег не прибавилось"), Player->GetStats()->GetMoney(), Money0 + Pay * 3.0f);

	ScientistTest::Destroy(World);
	ScientistTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 4. Учёный-торговец: свой прайс-лист, покупка двустволки.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScientistTraderCatalogTest,
	"ContrarySurvivor.Scientists.TraderSellsShotgunAndShells", ScientistTestFlags)

bool FScientistTraderCatalogTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryScientistTraderQA");
	ScientistTest::DeleteSlot(Slot);
	ScientistTest::FScopedTables Tables;
	UWorld* World = ScientistTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	ASaveTestPlayerCharacter* Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AScientistTrader* Trader = ScientistTest::Spawn<AScientistTrader>(World, FVector(5000.0f, 0.0f, 100.0f));
	if (!Player || !Trader || !Player->GetStats())
	{
		AddError(TEXT("Не заспавнены игрок или учёный-торговец"));
		ScientistTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	const FName Type12 = ContraryAmmo::Shotgun12gType();
	const FShopEntry* GunEntry = nullptr;
	const FShopEntry* ShellEntry = nullptr;
	bool bHasBandage = false;
	bool bHasPistol = false;
	for (const FShopEntry& Entry : Trader->GetCatalog())
	{
		if (Entry.ItemRow == ScientistTest::ShotgunItemRow) { GunEntry = &Entry; }
		if (Entry.ItemRow == Type12) { ShellEntry = &Entry; }
		if (Entry.ItemRow == FName(TEXT("bandage"))) { bHasBandage = true; }
		if (Entry.ItemRow == FName(TEXT("pistol"))) { bHasPistol = true; }
	}
	TestNotNull(TEXT("В прайс-листе есть двустволка"), GunEntry);
	TestNotNull(TEXT("В прайс-листе есть патроны к ней"), ShellEntry);
	TestTrue(TEXT("В прайс-листе есть аптечка"), bHasBandage);
	TestFalse(TEXT("Пистолета деревенского торговца в прайс-листе нет"), bHasPistol);
	if (!GunEntry || !ShellEntry)
	{
		ScientistTest::Destroy(World);
		return false;
	}
	TestTrue(TEXT("Двустволка продаётся предметом"), GunEntry->Kind == EShopEntryKind::Item);
	TestTrue(TEXT("Патроны продаются патронами"), ShellEntry->Kind == EShopEntryKind::Ammo);
	TestEqual(TEXT("Цена двустволки — из таблицы предметов"), GunEntry->Price, 300.0f);

	// Покупка: ружьё и патроны в сумке, монеты списаны.
	Player->GetStats()->InitMoney(1000.0f);
	TestTrue(TEXT("Двустволка куплена"), Player->Shop_BuyEntry(*GunEntry));
	TestNotNull(TEXT("Купленная двустволка лежит в сумке"), ScientistTest::FindShotgunInBag(Player));
	const int32 ShellsWithGun = Player->GetReserveAmmoInInventory(Type12);
	TestTrue(TEXT("Патроны куплены"), Player->Shop_BuyEntryQty(*ShellEntry, 6));
	TestEqual(TEXT("Купленные патроны легли в сумку"),
		Player->GetReserveAmmoInInventory(Type12), ShellsWithGun + 6 * FMath::Max(1, ShellEntry->AmmoAmount));
	TestEqual(TEXT("Монеты списаны за ружьё и патроны"),
		Player->GetStats()->GetMoney(), 1000.0f - GunEntry->Price - ShellEntry->Price * 6.0f);

	ScientistTest::Destroy(World);
	ScientistTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 5. Запас патронов при стволе не дарится заново при каждом «Продолжить».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScientistWeaponReserveSaveTest,
	"ContrarySurvivor.Scientists.ContinueDoesNotRegiftWeaponReserve", ScientistTestFlags)

bool FScientistWeaponReserveSaveTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryScientistReserveQA");
	ScientistTest::DeleteSlot(Slot);

	const FName Type12 = ContraryAmmo::Shotgun12gType();
	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	// ⛔ Игрок появляется ВДАЛИ от костра и только потом получает свой слот (иначе
	// автосохранение костра при появлении ушло бы в боевой слот).
	const FVector FarLoc(4000.0f, 0.0f, 100.0f);
	const FVector NearCampfireLoc = CampfireLoc + FVector(100.0f, 0.0f, 0.0f);

	auto PrepareCampfire = [&](UWorld* World)
	{
		if (ACampfire* Campfire = ScientistTest::Spawn<ACampfire>(World, CampfireLoc))
		{
			if (FFloatProperty* CooldownProp = FindFProperty<FFloatProperty>(ACampfire::StaticClass(), TEXT("AutoSaveCooldown")))
			{
				CooldownProp->SetPropertyValue_InContainer(Campfire, 0.0f);
			}
		}
	};

	// Один запуск игры: загрузить слот, вернуть число патронов в сумке и снова сохранить.
	auto ContinueAndResave = [&](const TCHAR* Label, int32& OutShells, int32& OutGuns) -> bool
	{
		UWorld* World = ScientistTest::Create();
		if (!World)
		{
			AddError(FString::Printf(TEXT("Не создан мир (%s)"), Label));
			return false;
		}
		PrepareCampfire(World);
		ASaveTestPlayerCharacter* P = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		if (!P)
		{
			AddError(FString::Printf(TEXT("Не заспавнен игрок (%s)"), Label));
			ScientistTest::Destroy(World);
			return false;
		}
		P->UseTestSaveSlot(Slot);
		const bool bLoaded = P->LoadGameForContinue();
		OutShells = P->GetReserveAmmoInInventory(Type12);
		OutGuns = (Cast<AShotgun>(P->GetRangedWeaponInstance()) ? 1 : 0)
			+ (ScientistTest::FindShotgunInBag(P) ? 1 : 0);
		const bool bSaved = bLoaded && P->SaveGame();
		ScientistTest::Destroy(World);
		if (!bLoaded || !bSaved)
		{
			AddError(FString::Printf(TEXT("«Продолжить» или сохранение не прошли (%s)"), Label));
			return false;
		}
		return true;
	};

	// --- Запуск 1: ружьё в слоте оружия, его запас перелит пачкой в сумку; сохраняемся ---
	int32 ShellsAtSave = 0;
	{
		UWorld* World = ScientistTest::Create();
		if (!World)
		{
			AddError(TEXT("Не создан мир запуска 1"));
			return false;
		}
		PrepareCampfire(World);
		ASaveTestPlayerCharacter* P1 = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		AShotgun* Gun = ScientistTest::Spawn<AShotgun>(World, FarLoc);
		if (!P1 || !Gun || !P1->GetInventory())
		{
			AddError(TEXT("Не заспавнены игрок или ружьё запуска 1"));
			ScientistTest::Destroy(World);
			return false;
		}
		P1->UseTestSaveSlot(Slot);
		Gun->SetActorHiddenInGame(true);
		Gun->SetActorEnableCollision(false);
		P1->GetInventory()->AddItem(Gun);
		TestTrue(TEXT("Ружьё встало в слот оружия"), P1->TryAdoptRangedWeapon(Gun));
		ShellsAtSave = P1->GetReserveAmmoInInventory(Type12);
		TestTrue(TEXT("Запас ружья перелит пачкой в сумку"), ShellsAtSave > 0);
		TestEqual(TEXT("При стволе запаса не осталось"), Gun->GetCurrentAmmoReserve(), 0);

		P1->SetActorLocation(NearCampfireLoc, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Сохранение запуска 1 прошло"), P1->SaveGame());
		ScientistTest::Destroy(World);
	}

	// --- Запуски 2 и 3: патронов ровно столько, сколько было при сохранении ---
	int32 Shells = 0;
	int32 Guns = 0;
	if (!ContinueAndResave(TEXT("запуск 2"), Shells, Guns))
	{
		ScientistTest::DeleteSlot(Slot);
		return false;
	}
	TestEqual(TEXT("Запуск 2: ружьё одно"), Guns, 1);
	TestEqual(TEXT("Запуск 2: патронов не прибавилось"), Shells, ShellsAtSave);
	if (!ContinueAndResave(TEXT("запуск 3"), Shells, Guns))
	{
		ScientistTest::DeleteSlot(Slot);
		return false;
	}
	TestEqual(TEXT("Запуск 3: патронов не прибавилось"), Shells, ShellsAtSave);

	// --- Старая запись (сохранение прежних сборок запаса не писало): перенос один раз ---
	if (UContrarySaveGame* Disk = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		for (FSavedInventoryEntry& Entry : Disk->InventoryEntries)
		{
			Entry.WeaponReserveAmmo = -1;
		}
		UGameplayStatics::SaveGameToSlot(Disk, Slot, 0);
	}
	else
	{
		AddError(TEXT("Слот не читается перед проверкой старой записи"));
	}
	if (!ContinueAndResave(TEXT("старая запись"), Shells, Guns))
	{
		ScientistTest::DeleteSlot(Slot);
		return false;
	}
	const int32 ShellsAfterOldRecord = Shells;
	TestTrue(TEXT("Старая запись: запас ствола один раз перенесён в сумку"), ShellsAfterOldRecord > ShellsAtSave);
	if (!ContinueAndResave(TEXT("после старой записи"), Shells, Guns))
	{
		ScientistTest::DeleteSlot(Slot);
		return false;
	}
	TestEqual(TEXT("После старой записи патроны больше не прибавляются"), Shells, ShellsAfterOldRecord);

	ScientistTest::DeleteSlot(Slot);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
