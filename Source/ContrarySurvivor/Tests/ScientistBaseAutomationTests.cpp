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
//   - ожидание после сценки про ноутбук: таймер идёт игровым временем, пишется в сохранение,
//     третье задание до его конца закрыто; сохранение без таймера не застревает;
//   - сюжетные кнопки меню отладки: чистое состояние, запись в сохранение, пропуск ожидания.
//
// Разговор проверяется теми же вызовами, что делает окно (ATalkerNPC::BuildTalkView и
// HandleTalkPrimary) — живой Slate в прогонах проекта не поднимается.
// Таблицы предметов и заданий на время теста ПОДМЕНЯЮТСЯ своими (боевые таблицы не читаются
// и не меняются). Слот сохранения ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Debug/QADebug.h"            // CONTRARY_WITH_QA_CHEATS
#include "ContrarySurvivor/Debug/ContraryDebugMenu.h"  // сюжетные кнопки меню отладки
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"  // шаг «купи пистолет» ведёт к торговцу
#include "ContrarySurvivor/UI/QuestTrackerWidget.h"    // строка ожидания старосты
#include "ContrarySurvivor/UI/QuestTargets.h"           // цель задания для карты
#include "Components/BoxComponent.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Actors/QuestBarrier.h"
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
		FContraryItemRow Pistol; // его просит купить задание на бандитов (подготовительный шаг)
		Pistol.DisplayText = FText::FromString(TEXT("QA пистолет"));
		Pistol.ItemClass = APistol::StaticClass();
		Pistol.Category = EItemCategory::Weapon;
		Pistol.MaxStackCount = 1;
		Pistol.Price = 150.0f;
		Table->AddRow(FName(TEXT("pistol")), Pistol);

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

	// Запись сохранения, как её оставила сборка ДО таймера ожидания: признак сценки есть,
	// отметки конца ожидания нет. Герой перечитывает признаки — как при запуске игры.
	static void SetNotebookHintShown(APlayerCharacter* Player, bool bShown)
	{
		if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			Save->bElderNotebookHintShown = bShown;
			Save->ElderNotebookWaitEndPlayTime = -1.0f;
			Player->WriteSaveObject(Save);
		}
		Player->ReloadStoryStateFromSave();
	}

	// Кадр тестового мира (счётчик кадров двигаем сами — см. CombatAutomationTests.cpp).
	static void TickWorld(UWorld* World, float DeltaSeconds)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, DeltaSeconds);
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
		AElderNPC::ShouldUnlockThirdQuest(false, true, true, true, true));
	TestFalse(TEXT("Сценка про ноутбук ещё не показана — третьего нет"),
		AElderNPC::ShouldUnlockThirdQuest(true, false, true, true, true));
	TestTrue(TEXT("Второе сдано и сценка показана — третье открывается"),
		AElderNPC::ShouldUnlockThirdQuest(true, true, true, true, true));
	TestFalse(TEXT("Сценка показана, но староста ещё разбирается с ноутбуком — третьего нет"),
		AElderNPC::ShouldUnlockThirdQuest(true, true, true, true, false));
	TestTrue(TEXT("Сценки у старосты нет вовсе — ждать нечего"),
		AElderNPC::ShouldUnlockThirdQuest(true, false, false, true, false));

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

	// Сценка показана, а таймера ожидания в записи нет (сохранение до правки 10.10.2026):
	// ожидание считается оконченным, задание открывается сразу — такой игрок не застревает.
	ScientistTest::SetNotebookHintShown(Player, true);
	TestEqual(TEXT("У сохранения без таймера ждать нечего"), Player->GetElderNotebookWaitRemaining(), 0.0f);
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

// ===========================================================================
// 6. Ожидание после сценки про ноутбук: таймер идёт, пишется в сохранение, третье задание
//    до конца ожидания закрыто и открывается после; строка для списка заданий на экране.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScientistElderWaitTest,
	"ContrarySurvivor.Scientists.ElderNotebookWaitGatesThirdQuest", ScientistTestFlags)

bool FScientistElderWaitTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryElderWaitQA");
	ScientistTest::DeleteSlot(Slot);
	ScientistTest::FScopedTables Tables;

	// --- Чистые правила ---
	TestEqual(TEXT("Отметки в записи нет — ждать нечего"), AElderNPC::GetNotebookWaitRemaining(-1.0f, 100.0f), 0.0f);
	TestEqual(TEXT("Остаток — разница отметки и накопителя"), AElderNPC::GetNotebookWaitRemaining(400.0f, 100.0f), 300.0f);
	TestEqual(TEXT("Отметка в прошлом — остатка нет"), AElderNPC::GetNotebookWaitRemaining(50.0f, 100.0f), 0.0f);
	TestEqual(TEXT("Пять минут — пять"), AElderNPC::GetNotebookWaitMinutesLeft(300.0f), 5);
	TestEqual(TEXT("Чуть больше четырёх минут — пять"), AElderNPC::GetNotebookWaitMinutesLeft(241.0f), 5);
	TestEqual(TEXT("Ровно четыре минуты — четыре"), AElderNPC::GetNotebookWaitMinutesLeft(240.0f), 4);
	TestEqual(TEXT("Последние секунды — одна минута, а не ноль"), AElderNPC::GetNotebookWaitMinutesLeft(10.0f), 1);

	TestTrue(TEXT("Сценки не было — строки ожидания нет"),
		AElderNPC::GetNotebookWaitPhase(false, false, 120.0f) == EElderNotebookWaitPhase::None);
	TestTrue(TEXT("Время идёт — староста разбирается"),
		AElderNPC::GetNotebookWaitPhase(true, false, 120.0f) == EElderNotebookWaitPhase::Waiting);
	TestTrue(TEXT("Время вышло — пора поговорить"),
		AElderNPC::GetNotebookWaitPhase(true, false, 0.0f) == EElderNotebookWaitPhase::Ready);
	TestTrue(TEXT("Следующее задание взято — строки нет"),
		AElderNPC::GetNotebookWaitPhase(true, true, 0.0f) == EElderNotebookWaitPhase::None);

	const FText WaitFormat = FText::FromString(TEXT("ждать {Minutes} мин"));
	const FText ReadyText = FText::FromString(TEXT("готово"));
	TestEqual(TEXT("Строка ожидания несёт число минут"),
		UQuestTrackerWidget::BuildElderWaitLine(EElderNotebookWaitPhase::Waiting, 300.0f, WaitFormat, ReadyText).ToString(),
		FString(TEXT("ждать 5 мин")));
	TestEqual(TEXT("Строка «время вышло» — как задана"),
		UQuestTrackerWidget::BuildElderWaitLine(EElderNotebookWaitPhase::Ready, 0.0f, WaitFormat, ReadyText).ToString(),
		ReadyText.ToString());
	TestTrue(TEXT("Без ожидания строка пустая"),
		UQuestTrackerWidget::BuildElderWaitLine(EElderNotebookWaitPhase::None, 0.0f, WaitFormat, ReadyText).IsEmpty());

	// --- В мире ---
	UWorld* World = ScientistTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(4000.0f, 0.0f, 100.0f));
	AElderNPC* Elder = ScientistTest::Spawn<AElderNPC>(World, FVector(7000.0f, 0.0f, 100.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Elder || !Quests)
	{
		AddError(TEXT("Не заспавнены игрок или староста"));
		ScientistTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	Player->ReloadStoryStateFromSave(); // признаки — из своего пустого слота, а не из боевого

	const FName Q3 = Elder->GetThirdQuest().QuestId;
	FQuest Q1Done = Elder->GetOfferedQuest();
	Q1Done.State = EQuestState::TurnedIn;
	FQuest Q2Done = Elder->GetSecondQuest();
	Q2Done.State = EQuestState::TurnedIn;
	Quests->RestoreQuests({ Q1Done, Q2Done });

	TestFalse(TEXT("В свежем слоте сценка не показана"), Player->IsElderNotebookHintShown());
	TestFalse(TEXT("До сценки третье задание закрыто"), Elder->TryUnlockThirdQuest(Player));

	// Сценка доиграна — ожидание пошло.
	const float Wait = Elder->GetNotebookWaitSeconds();
	TestTrue(TEXT("У старосты задано время ожидания"), Wait > 1.0f);
	Player->MarkElderNotebookHintShown(Wait);
	TestTrue(TEXT("Сценка отмечена показанной"), Player->IsElderNotebookHintShown());
	const float RemainingAtStart = Player->GetElderNotebookWaitRemaining();
	TestTrue(TEXT("Ожидание началось с полного времени"),
		RemainingAtStart > Wait - 1.0f && RemainingAtStart <= Wait + 0.01f);
	TestFalse(TEXT("Пока староста разбирается, третье задание закрыто"), Elder->TryUnlockThirdQuest(Player));
	TestFalse(TEXT("Третьего задания нет в журнале"), ScientistTest::InJournal(Player, Q3));
	TestEqual(TEXT("Староста по-прежнему говорит о втором задании"),
		Elder->GetQuestForPlayer(Quests).QuestId, Elder->GetSecondQuest().QuestId);

	// Таймер считает игровое время.
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		ScientistTest::TickWorld(World, 0.2f);
	}
	const float RemainingAfterTicks = Player->GetElderNotebookWaitRemaining();
	TestTrue(TEXT("Игровое время идёт — остаток уменьшился"), RemainingAfterTicks < RemainingAtStart);
	TestTrue(TEXT("Секунда игры не съела всё ожидание"), RemainingAfterTicks > 0.0f);

	// Повторная отметка таймер заново не заводит.
	Player->MarkElderNotebookHintShown(Wait);
	TestTrue(TEXT("Повторная отметка сценки ожидание не продлила"),
		Player->GetElderNotebookWaitRemaining() <= RemainingAfterTicks + 0.01f);

	// Запись в сохранении и её перенос при обычном сохранении игры.
	float SavedEnd = -1.0f;
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		TestTrue(TEXT("Признак сценки записан в сохранение"), Save->bElderNotebookHintShown);
		SavedEnd = Save->ElderNotebookWaitEndPlayTime;
		TestTrue(TEXT("Отметка конца ожидания записана в сохранение"), SavedEnd >= 0.0f);
	}
	TestTrue(TEXT("Обычное сохранение прошло"), Player->SaveGame());
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		TestEqual(TEXT("Обычное сохранение отметку ожидания не потеряло"), Save->ElderNotebookWaitEndPlayTime, SavedEnd);
		TestTrue(TEXT("Обычное сохранение признак сценки не потеряло"), Save->bElderNotebookHintShown);
	}
	Player->ReloadStoryStateFromSave();
	TestTrue(TEXT("После перечитывания сохранения ожидание продолжается"), Player->GetElderNotebookWaitRemaining() > 0.0f);
	TestFalse(TEXT("После перечитывания третье задание всё ещё закрыто"), Elder->TryUnlockThirdQuest(Player));

	// Время выходит само: оставляем полсекунды и проживаем их.
	if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		Save->ElderNotebookWaitEndPlayTime = Player->GetTotalPlayTimeSeconds() + 0.5f;
		Player->WriteSaveObject(Save);
	}
	Player->ReloadStoryStateFromSave();
	TestTrue(TEXT("Осталось полсекунды — ещё ждём"), Player->GetElderNotebookWaitRemaining() > 0.0f);
	for (int32 Frame = 0; Frame < 6; ++Frame)
	{
		ScientistTest::TickWorld(World, 0.2f);
	}
	TestEqual(TEXT("Время вышло — остатка нет"), Player->GetElderNotebookWaitRemaining(), 0.0f);
	TestTrue(TEXT("Время вышло — на экране «поговорите со старостой»"),
		AElderNPC::GetNotebookWaitPhase(Player->IsElderNotebookHintShown(),
			AQuestGiverNPC::IsStoryEndingQuestTaken(Quests->GetQuests()), Player->GetElderNotebookWaitRemaining())
		== EElderNotebookWaitPhase::Ready);

	TestTrue(TEXT("После ожидания третье задание открывается"), Elder->TryUnlockThirdQuest(Player));
	TestEqual(TEXT("Третье задание предложено, но не взято"), ScientistTest::StateOf(Player, Q3), EQuestState::NotStarted);
	TestTrue(TEXT("Пока задание не взято, строка «поговорите» держится"),
		AElderNPC::GetNotebookWaitPhase(true, AQuestGiverNPC::IsStoryEndingQuestTaken(Quests->GetQuests()), 0.0f)
		== EElderNotebookWaitPhase::Ready);
	TestTrue(TEXT("Третье задание берётся"), Quests->AcceptQuest(Q3));
	TestTrue(TEXT("Задание взято — строка ожидания убрана"),
		AElderNPC::GetNotebookWaitPhase(true, AQuestGiverNPC::IsStoryEndingQuestTaken(Quests->GetQuests()), 0.0f)
		== EElderNotebookWaitPhase::None);

	ScientistTest::Destroy(World);
	ScientistTest::DeleteSlot(Slot);
	return true;
}

#if CONTRARY_WITH_QA_CHEATS

// ===========================================================================
// 7. Сюжетные кнопки меню отладки: чистое состояние из любого исходного, запись в
//    сохранение, «Продолжить» даёт то же самое, пропуск ожидания.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuStoryButtonsTest,
	"ContrarySurvivor.DebugMenu.StoryButtonsGiveCleanState", ScientistTestFlags)

bool FDebugMenuStoryButtonsTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryDebugStoryQA");
	ScientistTest::DeleteSlot(Slot);
	ScientistTest::FScopedTables Tables;

	UWorld* World = ScientistTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Костёр рядом со старостой: сохранение у старосты остаётся «у костра», и «Продолжить»
	// героя не переставляет. Игрок появляется вдали и только потом получает свой слот.
	if (ACampfire* Campfire = ScientistTest::Spawn<ACampfire>(World, FVector(0.0f, 0.0f, 100.0f)))
	{
		if (FFloatProperty* CooldownProp = FindFProperty<FFloatProperty>(ACampfire::StaticClass(), TEXT("AutoSaveCooldown")))
		{
			CooldownProp->SetPropertyValue_InContainer(Campfire, 0.0f);
		}
	}
	ASaveTestPlayerCharacter* Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(4000.0f, 0.0f, 100.0f));
	AElderNPC* Elder = ScientistTest::Spawn<AElderNPC>(World, FVector(300.0f, 0.0f, 100.0f));
	AMasterEnemyBase* BanditBase = ScientistTest::Spawn<AMasterEnemyBase>(World, FVector(30000.0f, 0.0f, 0.0f));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Elder || !BanditBase || !Quests || !Player->GetInventory())
	{
		AddError(TEXT("Не заспавнены игрок, староста или база"));
		ScientistTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	Player->ReloadStoryStateFromSave();

	const FQuest& First = Elder->GetOfferedQuest();
	const FQuest& Second = Elder->GetSecondQuest();
	const FName Q1 = First.QuestId;
	const FName Q2 = Second.QuestId;
	const FName Q3 = Elder->GetThirdQuest().QuestId;
	const FString RadioKey = ScientistTest::RadioRow.ToString();

	// База бандитов — та, на которую ведёт метка второго задания.
	if (FNameProperty* MarkerProp = FindFProperty<FNameProperty>(AMasterEnemyBase::StaticClass(), TEXT("QuestMarkerTag")))
	{
		MarkerProp->SetPropertyValue_InContainer(BanditBase, Second.MapMarkerTag);
	}
	else
	{
		AddError(TEXT("У базы нет поля метки задания"));
	}
	const FName BaseId = BanditBase->GetBaseSaveId();
	auto SaveHasBaseRecord = [Player, BaseId]()
	{
		const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject();
		return Save && Save->EnemyBaseStates.ContainsByPredicate(
			[BaseId](const FSavedEnemyBaseState& State) { return State.BaseId == BaseId; });
	};

	TestFalse(TEXT("Ожидание не идёт — пропускать нечего"), ContraryDebugTools::SkipElderNotebookWait(Player));

	// --- Исходное состояние: сюжет пройден дальше и намусорено ---
	FQuest Q1Done = First;
	Q1Done.State = EQuestState::TurnedIn;
	FQuest Q2Done = Second;
	Q2Done.State = EQuestState::TurnedIn;
	FQuest Q3Active = Elder->GetThirdQuest();
	Q3Active.State = EQuestState::Active;
	FQuest RadioQuest;
	RadioQuest.QuestId = FName(TEXT("QA_RadioQuest"));
	RadioQuest.TargetCount = 0;
	RadioQuest.RequiredItemName = RadioKey;
	RadioQuest.RequiredItemCount = 1;
	RadioQuest.State = EQuestState::Active;
	Quests->RestoreQuests({ Q1Done, Q2Done, Q3Active, RadioQuest });
	Player->GiveItemFromRow(ScientistTest::LaptopRow, 1);
	Player->GiveItemFromRow(ScientistTest::RadioRow, 1);
	TestEqual(TEXT("В исходном состоянии в сумке ноутбук"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 1);
	TestEqual(TEXT("В исходном состоянии в сумке рация"), ScientistTest::CountInBag(Player, RadioKey), 1);
	if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		Save->bElderNotebookHintShown = true;
		Save->ElderNotebookWaitEndPlayTime = 99999.0f;
		Save->bEndOfStoryShown = true;
		Save->bElderHookShown = false;
		FSavedEnemyBaseState BaseState;
		BaseState.BaseId = BaseId;
		BaseState.Tier = 3;
		BaseState.LastClearedTier = 2;
		BaseState.LastClearUtc = FDateTime::UtcNow();
		Save->EnemyBaseStates.Add(BaseState);
		Player->WriteSaveObject(Save);
	}
	Player->ReloadStoryStateFromSave();

	// --- Кнопка «сдать старосте ноутбук бандитов» ---
	TestTrue(TEXT("Кнопка «сдать ноутбук» отработала"), ContraryDebugTools::SetStoryReadyToTurnInBanditLaptop(Player));
	TestEqual(TEXT("В журнале только два задания старосты"), Quests->GetQuests().Num(), 2);
	TestEqual(TEXT("Первое задание сдано"), ScientistTest::StateOf(Player, Q1), EQuestState::TurnedIn);
	TestEqual(TEXT("Задание на бандитов готово к сдаче"), ScientistTest::StateOf(Player, Q2), EQuestState::Completed);
	TestFalse(TEXT("Третьего задания в журнале нет"), ScientistTest::InJournal(Player, Q3));
	TestEqual(TEXT("В сумке ровно один ноутбук"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 1);
	TestEqual(TEXT("Рация чужого задания убрана"), ScientistTest::CountInBag(Player, RadioKey), 0);
	TestFalse(TEXT("У героя сценка про ноутбук не показана"), Player->IsElderNotebookHintShown());
	TestEqual(TEXT("У героя ожидания нет"), Player->GetElderNotebookWaitRemaining(), 0.0f);
	TestTrue(TEXT("Герой стоит рядом со старостой"),
		FVector::Dist2D(Player->GetActorLocation(), Elder->GetActorLocation()) <= 700.0f);
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		TestTrue(TEXT("Состояние записано в сохранение"), Save->bHasData);
		TestEqual(TEXT("В сохранении тот же журнал"), Save->Quests.Num(), 2);
		TestFalse(TEXT("В сохранении сценка про ноутбук сброшена"), Save->bElderNotebookHintShown);
		TestTrue(TEXT("В сохранении таймера ожидания нет"), Save->ElderNotebookWaitEndPlayTime < 0.0f);
		TestFalse(TEXT("В сохранении сообщение о конце сюжета сброшено"), Save->bEndOfStoryShown);
		TestTrue(TEXT("В сохранении вступление старосты пройдено"), Save->bElderHookShown);
	}
	TestTrue(TEXT("Кнопка «сдать ноутбук» память базы не трогает"), SaveHasBaseRecord());

	// «Продолжить» читает именно эту запись: в ней то же задание в том же состоянии и один
	// ноутбук. (Само восстановление сумки из записи держат проверки сохранений; звать его
	// здесь на живом герое нельзя — оно рассчитано на пустую сумку после запуска игры.)
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		const FQuest* SavedSecond = Save->Quests.FindByPredicate([Q2](const FQuest& Q) { return Q.QuestId == Q2; });
		TestTrue(TEXT("В сохранении задание на бандитов готово к сдаче"),
			SavedSecond && SavedSecond->State == EQuestState::Completed);
		int32 SavedLaptops = 0;
		for (const FSavedInventoryEntry& Entry : Save->InventoryEntries)
		{
			if (Entry.ItemName.Equals(ScientistTest::LaptopKey, ESearchCase::CaseSensitive))
			{
				SavedLaptops += FMath::Max(1, Entry.StackCount);
			}
		}
		TestEqual(TEXT("В сохранении ровно один ноутбук"), SavedLaptops, 1);
	}

	// Дальше сюжет идёт сам: сдача, сценка, ожидание, пропуск, третье задание.
	TestTrue(TEXT("Задание на бандитов сдаётся старосте"), Quests->TurnInQuest(Q2));
	TestEqual(TEXT("Ноутбук изъят при сдаче"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 0);
	TestFalse(TEXT("До сценки третье задание закрыто"), Elder->TryUnlockThirdQuest(Player));
	Player->MarkElderNotebookHintShown(Elder->GetNotebookWaitSeconds());
	TestFalse(TEXT("Во время ожидания третье задание закрыто"), Elder->TryUnlockThirdQuest(Player));

	TestTrue(TEXT("Сохранение во время ожидания прошло"), Player->SaveGame());
	TestTrue(TEXT("«Продолжить» во время ожидания прошло"), Player->LoadGameForContinue());
	TestTrue(TEXT("После «Продолжить» сценка остаётся показанной"), Player->IsElderNotebookHintShown());
	TestTrue(TEXT("После «Продолжить» ожидание продолжается"), Player->GetElderNotebookWaitRemaining() > 0.0f);
	TestFalse(TEXT("После «Продолжить» третье задание всё ещё закрыто"), Elder->TryUnlockThirdQuest(Player));

	TestTrue(TEXT("Кнопка «пропустить ожидание» отработала"), ContraryDebugTools::SkipElderNotebookWait(Player));
	TestEqual(TEXT("После пропуска ждать нечего"), Player->GetElderNotebookWaitRemaining(), 0.0f);
	TestFalse(TEXT("Второй пропуск уже нечего снимать"), ContraryDebugTools::SkipElderNotebookWait(Player));
	TestTrue(TEXT("После пропуска третье задание открывается"), Elder->TryUnlockThirdQuest(Player));

	// --- Кнопка «староста выдаёт задание на бандитов» из ушедшего дальше сюжета ---
	TestTrue(TEXT("Третье задание берётся"), Quests->AcceptQuest(Q3));
	TestEqual(TEXT("Староста отдал ноутбук"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 1);
	if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		Save->bEndOfStoryShown = true;
		Player->WriteSaveObject(Save);
	}

	TestTrue(TEXT("Кнопка «задание на бандитов» отработала"), ContraryDebugTools::SetStoryElderOffersBanditQuest(Player));
	TestEqual(TEXT("В журнале одно задание"), Quests->GetQuests().Num(), 1);
	TestEqual(TEXT("Первое задание сдано"), ScientistTest::StateOf(Player, Q1), EQuestState::TurnedIn);
	TestEqual(TEXT("Староста предлагает задание на бандитов"), Elder->GetQuestForPlayer(Quests).QuestId, Q2);
	TestEqual(TEXT("Ноутбука в сумке нет"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 0);
	TestFalse(TEXT("Сценка про ноутбук сброшена"), Player->IsElderNotebookHintShown());
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		TestEqual(TEXT("В сохранении одно задание"), Save->Quests.Num(), 1);
		TestFalse(TEXT("В сохранении сценка сброшена"), Save->bElderNotebookHintShown);
		TestTrue(TEXT("В сохранении таймера нет"), Save->ElderNotebookWaitEndPlayTime < 0.0f);
		TestFalse(TEXT("В сохранении сообщение о конце сюжета сброшено"), Save->bEndOfStoryShown);
	}
	TestFalse(TEXT("Память базы бандитов стёрта из сохранения"), SaveHasBaseRecord());

	// Игрок сам берёт задание у старосты.
	Quests->OfferQuest(Elder->GetQuestForPlayer(Quests));
	TestTrue(TEXT("Задание на бандитов берётся"), Quests->AcceptQuest(Q2));
	TestEqual(TEXT("Задание на бандитов взято и не выполнено"), ScientistTest::StateOf(Player, Q2), EQuestState::Active);

	ScientistTest::Destroy(World);
	ScientistTest::DeleteSlot(Slot);
	return true;
}


// ===========================================================================
// Сквозная проходимость сюжета без боёв (слова Рината 11.10.2026: «Проверь проходимость всех
// квестов (имей ввиду, что телепорт на базу ученых телепортирует сразу внутрь базы, т.е. за
// шлагбаум…)»). Персонажи, шлагбаум и место рации стоят там же, где на уровне (снято с уровня
// 11.10.2026), задания идут в том порядке, в каком их встречает игрок: волки, бандиты,
// ожидание старосты, охранник у закрытого шлагбаума, ноутбук учёным, рация. Убийства и подбор
// засчитываются прямыми вызовами. На каждом шаге у задания есть цель для карты, шлагбаум
// закрыт до взятия задания про ноутбук и открыт после.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScientistWholeStoryWalkthroughTest,
	"ContrarySurvivor.Story.WholeChainIsPassableWithoutFights", ScientistTestFlags)

bool FScientistWholeStoryWalkthroughTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryWholeStoryQA");
	ScientistTest::DeleteSlot(Slot);
	ScientistTest::FScopedTables Tables;

	UWorld* World = ScientistTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Места — как на уровне.
	const FVector BarrierLoc(7378.0, -16040.0, 0.0);
	const FVector GuardLoc(7250.0, -15740.0, 91.0);
	const double TalkRadius = 220.0;

	ASaveTestPlayerCharacter* Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(-7088.0, -6672.0, 100.0));
	AElderNPC* Elder = ScientistTest::Spawn<AElderNPC>(World, FVector(-4530.0, -4100.0, 91.0));
	AMasterEnemyBase* WolfDen = ScientistTest::Spawn<AMasterEnemyBase>(World, FVector(13943.0, -5998.0, 0.0));
	AMasterEnemyBase* BanditBase = ScientistTest::Spawn<AMasterEnemyBase>(World, FVector(6940.0, 9070.0, 0.0));
	AScientistGuardNPC* Guard = ScientistTest::Spawn<AScientistGuardNPC>(World, GuardLoc);
	AScientistQuestNPC* Scientist = ScientistTest::Spawn<AScientistQuestNPC>(World, FVector(9231.0, -16562.0, 91.0));
	AQuestItemSpawner* RadioSpot = ScientistTest::Spawn<AQuestItemSpawner>(World, FVector(-5280.0, -13440.0, 0.0));
	AQuestBarrier* Barrier = ScientistTest::Spawn<AQuestBarrier>(World, BarrierLoc);
	AMasterTrader* Trader = ScientistTest::Spawn<AMasterTrader>(World, FVector(-3000.0, -3960.0, 91.0));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
	if (!Player || !Elder || !WolfDen || !BanditBase || !Guard || !Scientist || !RadioSpot || !Barrier
		|| !Barrier->GetBlocker() || !Quests || !Inv || !Player->GetStats())
	{
		AddError(TEXT("Не заспавнены участники сюжета"));
		ScientistTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);

	// Метки целей у баз — те же, что стоят у баз на уровне.
	if (const FNameProperty* MarkerProp = FindFProperty<FNameProperty>(AMasterEnemyBase::StaticClass(), TEXT("QuestMarkerTag")))
	{
		MarkerProp->SetPropertyValue_InContainer(WolfDen, FName(TEXT("WolfDen")));
		MarkerProp->SetPropertyValue_InContainer(BanditBase, FName(TEXT("BanditBase")));
	}
	else
	{
		AddError(TEXT("У базы врагов нет поля метки цели"));
	}

	auto GiveQuestItems = [&](const FString& ItemKey, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			AQuestItem* Item = ScientistTest::Spawn<AQuestItem>(World, Player->GetActorLocation());
			if (Item)
			{
				Item->ItemName = ItemKey;
				Inv->AddItem(Item);
			}
		}
		Quests->SyncInventoryQuests(Inv);
	};
	auto TargetOf = [&](FName QuestId) -> AActor*
	{
		const FQuest* Quest = Quests->FindQuest(QuestId);
		return Quest ? QuestTargets::FindTargetActor(World, *Quest) : nullptr;
	};
	auto ExpectBarrier = [&](bool bOpen, const TCHAR* When)
	{
		Barrier->RefreshNow();
		TestEqual(*FString::Printf(TEXT("%s: шлагбаум %s"), When, bOpen ? TEXT("открыт") : TEXT("закрыт")), Barrier->IsOpen(), bOpen);
	};

	ExpectBarrier(false, TEXT("Начало игры"));

	// --- Задание 1: шкуры волков ---
	const FQuest First = Elder->GetQuestForPlayer(Quests);
	const FName Q1 = First.QuestId;
	TestEqual(TEXT("Первым староста даёт своё первое задание"), Q1, Elder->GetOfferedQuest().QuestId);
	Quests->OfferQuest(First);
	TestTrue(TEXT("Первое задание берётся"), Quests->AcceptQuest(Q1));
	TestTrue(TEXT("Первое задание ведёт к логову волков"), TargetOf(Q1) == WolfDen);
	TestTrue(TEXT("Первому заданию нужны предметы"), First.RequiredItemCount > 0 && !First.RequiredItemName.IsEmpty());
	for (int32 Kill = 0; Kill < First.TargetCount; ++Kill)
	{
		Quests->NotifyKill(First.KillTargetTag);
	}
	GiveQuestItems(First.RequiredItemName, First.RequiredItemCount);
	TestEqual(TEXT("Первое задание готово к сдаче"), ScientistTest::StateOf(Player, Q1), EQuestState::Completed);
	TestTrue(TEXT("Готовое первое задание ведёт к старосте"), TargetOf(Q1) == Elder);
	TestTrue(TEXT("Первое задание сдаётся старосте"), Quests->TurnInQuest(Q1));
	ExpectBarrier(false, TEXT("Первое задание сдано"));

	// --- Задание 2: лагерь бандитов и ноутбук ---
	const FQuest Second = Elder->GetQuestForPlayer(Quests);
	const FName Q2 = Second.QuestId;
	TestEqual(TEXT("После первого староста даёт второе задание"), Q2, Elder->GetSecondQuest().QuestId);
	Quests->OfferQuest(Second);
	TestTrue(TEXT("Второе задание берётся"), Quests->AcceptQuest(Q2));
	// Подготовительный шаг: купить пистолет у торговца со скидкой и поставить его в слот.
	Player->UpdateQuestPrepSteps();
	TestEqual(TEXT("После взятия второго задания идёт шаг покупки пистолета"),
		Quests->FindQuest(Q2)->PrepStep, EQuestPrepStep::Buy);
	TestTrue(TEXT("На шаге покупки второе задание ведёт к торговцу"), Trader && TargetOf(Q2) == Trader);
	const FShopEntry* PistolEntry = Trader ? Trader->GetCatalog().FindByPredicate(
		[&Second](const FShopEntry& Entry) { return Entry.ItemRow == Second.PrepItemRow; }) : nullptr;
	if (TestNotNull(TEXT("У торговца в продаже есть пистолет"), PistolEntry))
	{
		const float MoneyBeforePistol = Player->GetStats()->GetMoney();
		TestTrue(TEXT("Награды за волков хватает на пистолет со скидкой"), MoneyBeforePistol >= Second.PrepDealPrice);
		TestTrue(TEXT("Пистолет покупается"), Player->Shop_BuyEntryQty(*PistolEntry, 1));
		TestEqual(TEXT("За пистолет взята цена со скидкой"), Player->GetStats()->GetMoney(), MoneyBeforePistol - Second.PrepDealPrice);
		TestEqual(TEXT("После покупки идёт шаг «поставь в слот»"), Quests->FindQuest(Q2)->PrepStep, EQuestPrepStep::Equip);
		AMasterInventoryItem* BoughtPistol = nullptr;
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (APlayerCharacter::DoesItemMatchRow(Item, Second.PrepItemRow))
			{
				BoughtPistol = Item;
			}
		}
		TestTrue(TEXT("Купленный пистолет встаёт в слот"), BoughtPistol && Player->TryAdoptRangedWeapon(BoughtPistol));
		Player->UpdateQuestPrepSteps();
		TestEqual(TEXT("Пистолет в слоте — шаги пройдены"), Quests->FindQuest(Q2)->PrepStep, EQuestPrepStep::Done);
	}
	TestTrue(TEXT("Второе задание ведёт к лагерю бандитов"), TargetOf(Q2) == BanditBase);
	for (int32 Kill = 0; Kill < Second.TargetCount; ++Kill)
	{
		Quests->NotifyKill(Second.KillTargetTag);
	}
	GiveQuestItems(Second.RequiredItemName, Second.RequiredItemCount);
	TestEqual(TEXT("Второе задание готово к сдаче"), ScientistTest::StateOf(Player, Q2), EQuestState::Completed);
	TestTrue(TEXT("Готовое второе задание ведёт к старосте"), TargetOf(Q2) == Elder);
	TestTrue(TEXT("Второе задание сдаётся старосте"), Quests->TurnInQuest(Q2));
	TestEqual(TEXT("Ноутбук бандитов у старосты, в сумке его нет"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 0);
	ExpectBarrier(false, TEXT("Второе задание сдано"));

	// --- Ожидание старосты: третье задание не открывается, пока он разбирается с ноутбуком ---
	const FName Q3 = Elder->GetThirdQuest().QuestId;
	Player->MarkElderNotebookHintShown(Elder->GetNotebookWaitSeconds());
	TestTrue(TEXT("После сценки про ноутбук идёт ожидание"), Player->GetElderNotebookWaitRemaining() > 0.0f);
	TestFalse(TEXT("Пока староста занят, третье задание не открывается"), Elder->TryUnlockThirdQuest(Player));

	// --- Охранник у закрытого шлагбаума: точка телепорта снаружи, разговор через шлагбаум ---
	TArray<FContraryDebugTeleportPoint> Points;
	ContraryDebugTools::CollectTeleportPoints(World, Points);
	const FContraryDebugTeleportPoint* Outside = Points.FindByPredicate([](const FContraryDebugTeleportPoint& Point)
	{
		return Point.Label == TEXT("База учёных — перед шлагбаумом");
	});
	const double BlockerHalfDepth = Barrier->GetBlocker()->GetScaledBoxExtent().X;
	const double GuardSide = FMath::Sign(GuardLoc.X - BarrierLoc.X);
	TestTrue(TEXT("Охранник стоит снаружи, не в створе шлагбаума"), FMath::Abs(GuardLoc.X - BarrierLoc.X) > BlockerHalfDepth);
	if (TestNotNull(TEXT("В меню отладки есть точка «перед шлагбаумом»"), Outside))
	{
		TestTrue(TEXT("Точка «перед шлагбаумом» ставит героя ровно в неё"), Outside->bExact);
		TestTrue(TEXT("Телепорт «перед шлагбаумом» прошёл"),
			ContraryDebugTools::TeleportPawnNear(Player, Outside->Location, Outside->bExact));
		const FVector HeroLoc = Player->GetActorLocation();
		TestTrue(*FString::Printf(TEXT("Герой после телепорта стоит с той же стороны шлагбаума, что охранник (герой X=%.0f, шлагбаум X=%.0f)"),
			HeroLoc.X, BarrierLoc.X), (HeroLoc.X - BarrierLoc.X) * GuardSide > BlockerHalfDepth);
		TestTrue(*FString::Printf(TEXT("Герой стоит недалеко от шлагбаума (%.0f)"), FVector::Dist2D(HeroLoc, BarrierLoc)),
			FVector::Dist2D(HeroLoc, BarrierLoc) <= ContraryDebugTools::BarrierTeleportOutsideDistance + 50.0);

		// Место разговора: на прямой от охранника к герою, в радиусе разговора; шлагбаум
		// при этом пересекать не нужно.
		const FVector ToHero = (HeroLoc - GuardLoc).GetSafeNormal2D();
		const FVector TalkSpot = GuardLoc + ToHero * (TalkRadius - 40.0);
		TestTrue(TEXT("К охраннику можно подойти на расстояние разговора, не заходя за шлагбаум"),
			(TalkSpot.X - BarrierLoc.X) * GuardSide > BlockerHalfDepth
			&& FVector::Dist2D(TalkSpot, GuardLoc) <= TalkRadius);
		Player->SetActorLocation(FVector(TalkSpot.X, TalkSpot.Y, HeroLoc.Z));
	}
	ExpectBarrier(false, TEXT("Герой у охранника до задания про ноутбук"));

	const FName QGuard = Guard->GetOfferedQuest().QuestId;
	FNpcTalkView View;
	Guard->OnTalkOpened(Player);
	Guard->BuildTalkView(Player, View);
	TestFalse(TEXT("Охранник предлагает своё задание"), View.PrimaryLabel.IsEmpty());
	Guard->HandleTalkPrimary(Player);
	TestEqual(TEXT("Задание охранника взято"), ScientistTest::StateOf(Player, QGuard), EQuestState::Active);
	RadioSpot->RefreshNow();
	TestTrue(TEXT("Задание охранника ведёт к месту рации"), TargetOf(QGuard) == RadioSpot);
	TestNotNull(TEXT("Рация лежит на месте"), RadioSpot->GetSpawnedPickup());
	ExpectBarrier(false, TEXT("Задание охранника взято, но не сдано"));

	// --- Задание 3: ноутбук учёным. Берётся у старосты — шлагбаум открывается ---
	ScientistTest::SetNotebookHintShown(Player, true); // ожидание кончилось
	TestTrue(TEXT("После ожидания третье задание открывается"), Elder->TryUnlockThirdQuest(Player));
	TestEqual(TEXT("Староста говорит о третьем задании"), Elder->GetQuestForPlayer(Quests).QuestId, Q3);
	TestTrue(TEXT("Третье задание берётся"), Quests->AcceptQuest(Q3));
	TestEqual(TEXT("При взятии герой получил ноутбук"), ScientistTest::CountInBag(Player, ScientistTest::LaptopKey), 1);
	Quests->SyncInventoryQuests(Inv);
	TestTrue(TEXT("Третье задание ведёт к квестовому учёному"), TargetOf(Q3) == Scientist);
	ExpectBarrier(true, TEXT("Задание про ноутбук взято"));
	TestTrue(TEXT("Квестовый учёный стоит за шлагбаумом, внутри базы"),
		(Scientist->GetActorLocation().X - BarrierLoc.X) * GuardSide < -BlockerHalfDepth);

	Scientist->OnTalkOpened(Player);
	Scientist->BuildTalkView(Player, View);
	TestFalse(TEXT("С ноутбуком у учёного есть кнопка сдачи"), View.PrimaryLabel.IsEmpty());
	Scientist->HandleTalkPrimary(Player);
	TestEqual(TEXT("Третье задание сдано учёному"), ScientistTest::StateOf(Player, Q3), EQuestState::TurnedIn);
	TestTrue(TEXT("После приёма ноутбука объявляется конец сюжета"), Scientist->ShouldAnnounceStoryEndOnClose());
	Scientist->OnTalkClosed(Player);
	ExpectBarrier(true, TEXT("Ноутбук сдан"));

	// --- Рация охраннику ---
	TestTrue(TEXT("Рация подбирается"), Player->GiveItemFromRow(ScientistTest::RadioRow, 1) > 0);
	Quests->SyncInventoryQuests(Inv);
	TestEqual(TEXT("Задание охранника готово к сдаче"), ScientistTest::StateOf(Player, QGuard), EQuestState::Completed);
	TestTrue(TEXT("Готовое задание охранника ведёт к охраннику"), TargetOf(QGuard) == Guard);
	Guard->OnTalkOpened(Player);
	Guard->BuildTalkView(Player, View);
	TestFalse(TEXT("С рацией у охранника есть кнопка сдачи"), View.PrimaryLabel.IsEmpty());
	Guard->HandleTalkPrimary(Player);
	TestEqual(TEXT("Задание охранника сдано"), ScientistTest::StateOf(Player, QGuard), EQuestState::TurnedIn);
	TestTrue(TEXT("Награда охранника — двустволка в сумке или в слоте длинного оружия"),
		ScientistTest::FindShotgunInBag(Player) != nullptr || Player->GetFirearmInSlot(EFirearmSlot::Long) != nullptr);
	ExpectBarrier(true, TEXT("Все задания сданы"));

	TestEqual(TEXT("Сданы все четыре задания"), Quests->GetTurnedInQuestCount(), 4);
	for (const FQuest& Quest : Quests->GetQuests())
	{
		TestEqual(*FString::Printf(TEXT("Задание %s сдано"), *Quest.QuestId.ToString()), Quest.State, EQuestState::TurnedIn);
	}

	ScientistTest::Destroy(World);
	ScientistTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// Выход из игры в любой точке цепочки заданий не даёт тупика (ошибка Рината 11.10.2026:
// «перебил бандитов и шел в деревню и по пути вышел из игры… Я пошел в лагерь бандитов, а их
// там уже не было. И ноутбука тоже не было. Таким образом квест стал непроходимым»).
// Причина была в том, что зачистка базы писалась в сохранение сразу, а журнал заданий и сумка —
// только у костра. Здесь после каждого шага цепочки приложение «уходит в фон» (та же запись,
// что на телефоне), мир создаётся заново, герой загружается из сохранения — журнал и сумка
// обязаны совпасть с тем, что было, и цепочка обязана дойти до конца.
// ===========================================================================
namespace SaveChainTest
{
	struct FSession
	{
		FString Slot;
		UWorld* World = nullptr;
		ASaveTestPlayerCharacter* Player = nullptr;
		AElderNPC* Elder = nullptr;
		AMasterEnemyBase* WolfDen = nullptr;
		AMasterEnemyBase* BanditBase = nullptr;
		AScientistGuardNPC* Guard = nullptr;
		AScientistQuestNPC* Scientist = nullptr;
		AQuestItemSpawner* RadioSpot = nullptr;
		AQuestBarrier* Barrier = nullptr;
		AMasterTrader* Trader = nullptr;
		UQuestComponent* Quests = nullptr;
		UInventoryComponent* Inv = nullptr;

		bool Build()
		{
			World = ScientistTest::Create();
			if (!World)
			{
				return false;
			}
			Player = ScientistTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(-3880.0, -4320.0, 100.0));
			Elder = ScientistTest::Spawn<AElderNPC>(World, FVector(-4530.0, -4100.0, 91.0));
			WolfDen = ScientistTest::Spawn<AMasterEnemyBase>(World, FVector(13943.0, -5998.0, 0.0));
			BanditBase = ScientistTest::Spawn<AMasterEnemyBase>(World, FVector(6940.0, 9070.0, 0.0));
			Guard = ScientistTest::Spawn<AScientistGuardNPC>(World, FVector(7250.0, -15740.0, 91.0));
			Scientist = ScientistTest::Spawn<AScientistQuestNPC>(World, FVector(9231.0, -16562.0, 91.0));
			RadioSpot = ScientistTest::Spawn<AQuestItemSpawner>(World, FVector(-5280.0, -13440.0, 0.0));
			Barrier = ScientistTest::Spawn<AQuestBarrier>(World, FVector(7378.0, -16040.0, 0.0));
			Trader = ScientistTest::Spawn<AMasterTrader>(World, FVector(-3000.0, -3960.0, 91.0));
			if (!Player || !Elder || !WolfDen || !BanditBase || !Guard || !Scientist || !RadioSpot || !Barrier || !Trader)
			{
				return false;
			}
			Player->UseTestSaveSlot(Slot);
			Quests = Player->GetQuests();
			Inv = Player->GetInventory();
			// Базы — как на уровне: метки целей и ноутбук у лагеря бандитов.
			if (const FNameProperty* MarkerProp = FindFProperty<FNameProperty>(AMasterEnemyBase::StaticClass(), TEXT("QuestMarkerTag")))
			{
				MarkerProp->SetPropertyValue_InContainer(WolfDen, FName(TEXT("WolfDen")));
				MarkerProp->SetPropertyValue_InContainer(BanditBase, FName(TEXT("BanditBase")));
			}
			if (const FBoolProperty* LaptopProp = FindFProperty<FBoolProperty>(AMasterEnemyBase::StaticClass(), TEXT("bSpawnQuestItem")))
			{
				LaptopProp->SetPropertyValue_InContainer(BanditBase, true);
			}
			return Quests && Inv && Player->GetStats();
		}

		void Teardown()
		{
			ScientistTest::Destroy(World);
			World = nullptr;
		}

		FString Describe() const
		{
			FString Out;
			for (const FQuest& Quest : Quests->GetQuests())
			{
				Out += FString::Printf(TEXT("[%s state=%d kills=%d items=%d prep=%d debt=%.0f]"), *Quest.QuestId.ToString(),
					static_cast<int32>(Quest.State), Quest.Progress, Quest.ItemProgress, static_cast<int32>(Quest.PrepStep), Quest.PrepDebt);
			}
			TMap<FString, int32> Bag;
			for (const AMasterInventoryItem* Item : Inv->GetInventoryItems())
			{
				if (Item)
				{
					Bag.FindOrAdd(Item->ItemName) += FMath::Max(1, Item->GetStackCount());
				}
			}
			Bag.KeySort([](const FString& A, const FString& B) { return A < B; });
			for (const TPair<FString, int32>& Pair : Bag)
			{
				Out += FString::Printf(TEXT("{%s x%d}"), *Pair.Key, Pair.Value);
			}
			Out += FString::Printf(TEXT(" money=%.0f"), Player->GetStats()->GetMoney());
			return Out;
		}

		// Приложение ушло в фон, игру закрыли, запустили заново и нажали «Продолжить».
		bool ExitAndContinue(FAutomationTestBase& Test, const TCHAR* Where)
		{
			Quests->SyncInventoryQuests(Inv);
			const FString Before = Describe();
			if (!Test.TestTrue(*FString::Printf(TEXT("%s: уход в фон записал сохранение"), Where), Player->AutoSaveNow(TEXT("QA exit"))))
			{
				return false;
			}
			Teardown();
			if (!Build() || !Test.TestTrue(*FString::Printf(TEXT("%s: сохранение загрузилось"), Where), Player->LoadGameForContinue()))
			{
				Test.AddError(FString::Printf(TEXT("%s: мир после выхода не собран"), Where));
				return false;
			}
			Quests->SyncInventoryQuests(Inv);
			Test.TestEqual(*FString::Printf(TEXT("%s: журнал, сумка и монеты после выхода те же"), Where), Describe(), Before);
			return true;
		}

		void Give(const FString& ItemKey, int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (AQuestItem* Item = ScientistTest::Spawn<AQuestItem>(World, Player->GetActorLocation()))
				{
					Item->ItemName = ItemKey;
					Inv->AddItem(Item);
				}
			}
			Quests->SyncInventoryQuests(Inv);
		}

		EQuestState StateOf(FName QuestId) const { return ScientistTest::StateOf(Player, QuestId); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveChainExitAnywhereTest,
	"ContrarySurvivor.Story.ExitAtAnyPointKeepsChainPassable", ScientistTestFlags)

bool FSaveChainExitAnywhereTest::RunTest(const FString& Parameters)
{
	ScientistTest::FScopedTables Tables;
	SaveChainTest::FSession S;
	S.Slot = TEXT("ContrarySaveChainQA");
	ScientistTest::DeleteSlot(S.Slot);
	if (!TestTrue(TEXT("Мир собран"), S.Build()))
	{
		S.Teardown();
		return false;
	}

	TestFalse(TEXT("Свежая игра без заданий и без сохранения в фон не пишется"), S.Player->AutoSaveNow(TEXT("QA fresh")));

	bool bOk = true;
	FNpcTalkView View;

	// --- Задание 1: шкуры ---
	const FQuest First = S.Elder->GetOfferedQuest();
	S.Quests->OfferQuest(First);
	TestTrue(TEXT("Первое задание берётся"), S.Quests->AcceptQuest(First.QuestId));
	TestTrue(TEXT("Взятие задания просит запись сохранения"), S.Player->HasPendingAutoSave());
	bOk = bOk && S.ExitAndContinue(*this, TEXT("Первое задание взято"));
	if (bOk)
	{
		S.Give(First.RequiredItemName, First.RequiredItemCount - 1);
		bOk = S.ExitAndContinue(*this, TEXT("Собрана часть шкур"));
	}
	if (bOk)
	{
		S.Give(First.RequiredItemName, 1);
		TestEqual(TEXT("Шкуры собраны — задание готово"), S.StateOf(First.QuestId), EQuestState::Completed);
		bOk = S.ExitAndContinue(*this, TEXT("Шкуры собраны"));
	}
	if (bOk)
	{
		TestEqual(TEXT("После выхода первое задание по-прежнему готово к сдаче"), S.StateOf(First.QuestId), EQuestState::Completed);
		TestTrue(TEXT("Первое задание сдаётся"), S.Quests->TurnInQuest(First.QuestId));
		bOk = S.ExitAndContinue(*this, TEXT("Первое задание сдано"));
	}

	// --- Задание 2: бандиты и ноутбук, с покупкой пистолета ---
	const FQuest Second = bOk ? S.Elder->GetSecondQuest() : FQuest();
	if (bOk)
	{
		TestEqual(TEXT("После выхода староста даёт второе задание"), S.Elder->GetQuestForPlayer(S.Quests).QuestId, Second.QuestId);
		S.Quests->OfferQuest(Second);
		TestTrue(TEXT("Второе задание берётся"), S.Quests->AcceptQuest(Second.QuestId));
		S.Player->UpdateQuestPrepSteps();
		bOk = S.ExitAndContinue(*this, TEXT("Второе задание взято, шаг покупки пистолета"));
	}
	if (bOk)
	{
		const FShopEntry* PistolEntry = S.Trader->GetCatalog().FindByPredicate(
			[&Second](const FShopEntry& Entry) { return Entry.ItemRow == Second.PrepItemRow; });
		if (TestNotNull(TEXT("У торговца есть пистолет"), PistolEntry))
		{
			TestEqual(TEXT("После выхода скидка на пистолет действует"), S.Player->GetShopBuyPrice(*PistolEntry), Second.PrepDealPrice);
			TestTrue(TEXT("Пистолет куплен"), S.Player->Shop_BuyEntryQty(*PistolEntry, 1));
		}
		bOk = S.ExitAndContinue(*this, TEXT("Пистолет куплен, шаг слота"));
	}
	if (bOk)
	{
		AMasterInventoryItem* Pistol = nullptr;
		for (AMasterInventoryItem* Item : S.Inv->GetInventoryItems())
		{
			if (APlayerCharacter::DoesItemMatchRow(Item, Second.PrepItemRow))
			{
				Pistol = Item;
			}
		}
		TestTrue(TEXT("После выхода купленный пистолет в сумке и встаёт в слот"), Pistol && S.Player->TryAdoptRangedWeapon(Pistol));
		S.Player->UpdateQuestPrepSteps();
		TestEqual(TEXT("Шаги пистолета пройдены"), S.Quests->FindQuest(Second.QuestId)->PrepStep, EQuestPrepStep::Done);
		bOk = S.ExitAndContinue(*this, TEXT("Пистолет в слоте"));
	}
	if (bOk)
	{
		S.Quests->NotifyKill(Second.KillTargetTag);
		S.Quests->NotifyKill(Second.KillTargetTag);
		TestTrue(TEXT("Засчитанное убийство просит запись сохранения"), S.Player->HasPendingAutoSave());
		bOk = S.ExitAndContinue(*this, TEXT("Убиты двое бандитов из трёх"));
	}
	if (bOk)
	{
		TestEqual(TEXT("После выхода счёт убийств на месте"), S.Quests->FindQuest(Second.QuestId)->Progress, 2);
		S.Quests->NotifyKill(Second.KillTargetTag);
		bOk = S.ExitAndContinue(*this, TEXT("Бандиты перебиты, ноутбук не взят"));
	}
	if (bOk)
	{
		// Мир помнит зачищенную базу (её память пишется в сохранение сразу).
		S.BanditBase->MarkClearedForQA();
		TestEqual(TEXT("Бандитов возвращать незачем — убийства засчитаны"), S.Player->RepairQuestTargetsAfterLoad(), 0);
		S.BanditBase->RefreshQuestItem();
		TestNotNull(TEXT("Ноутбук лежит и на зачищенной базе, пока он нужен заданию"), S.BanditBase->GetSpawnedQuestPickupForQA());
		S.Give(Second.RequiredItemName, Second.RequiredItemCount);
		TestTrue(TEXT("Подобранный предмет задания просит запись сохранения"), S.Player->HasPendingAutoSave());
		S.BanditBase->RefreshQuestItem();
		TestNull(TEXT("Ноутбук в сумке — второго на базе нет"), S.BanditBase->GetSpawnedQuestPickupForQA());
		TestEqual(TEXT("Второе задание готово к сдаче"), S.StateOf(Second.QuestId), EQuestState::Completed);
		// Тот самый случай Рината: перебил бандитов, взял ноутбук, по пути в деревню вышел.
		bOk = S.ExitAndContinue(*this, TEXT("Бандиты перебиты, ноутбук в сумке, по пути в деревню"));
	}
	if (bOk)
	{
		S.BanditBase->MarkClearedForQA();
		TestEqual(TEXT("После выхода задание в состоянии «вернись к старосте»"), S.StateOf(Second.QuestId), EQuestState::Completed);
		TestEqual(TEXT("После выхода ноутбук в сумке"), ScientistTest::CountInBag(S.Player, ScientistTest::LaptopKey), 1);
		const FQuest* InLog = S.Quests->FindQuest(Second.QuestId);
		TestTrue(TEXT("После выхода задание ведёт к старосте"), InLog && QuestTargets::FindTargetActor(S.World, *InLog) == S.Elder);
		TestTrue(TEXT("Второе задание сдаётся"), S.Quests->TurnInQuest(Second.QuestId));
		bOk = S.ExitAndContinue(*this, TEXT("Второе задание сдано"));
	}

	// --- Ожидание старосты, охранник и рация ---
	const FName QGuard = bOk ? S.Guard->GetOfferedQuest().QuestId : NAME_None;
	if (bOk)
	{
		S.Player->MarkElderNotebookHintShown(S.Elder->GetNotebookWaitSeconds());
		bOk = S.ExitAndContinue(*this, TEXT("Идёт ожидание старосты"));
	}
	if (bOk)
	{
		TestTrue(TEXT("После выхода ожидание старосты продолжается"), S.Player->GetElderNotebookWaitRemaining() > 0.0f);
		TestFalse(TEXT("После выхода третье задание раньше времени не открывается"), S.Elder->TryUnlockThirdQuest(S.Player));
		S.Guard->OnTalkOpened(S.Player);
		S.Guard->BuildTalkView(S.Player, View);
		S.Guard->HandleTalkPrimary(S.Player);
		TestEqual(TEXT("Задание охранника взято"), S.StateOf(QGuard), EQuestState::Active);
		bOk = S.ExitAndContinue(*this, TEXT("Задание охранника взято"));
	}
	if (bOk)
	{
		S.RadioSpot->RefreshNow();
		TestNotNull(TEXT("После выхода рация лежит на месте"), S.RadioSpot->GetSpawnedPickup());
		TestTrue(TEXT("Рация подобрана"), S.Player->GiveItemFromRow(ScientistTest::RadioRow, 1) > 0);
		S.Quests->SyncInventoryQuests(S.Inv);
		bOk = S.ExitAndContinue(*this, TEXT("Рация подобрана"));
	}
	if (bOk)
	{
		TestEqual(TEXT("После выхода задание охранника готово к сдаче"), S.StateOf(QGuard), EQuestState::Completed);
		S.RadioSpot->RefreshNow();
		TestNull(TEXT("Рация в сумке — второй на месте нет"), S.RadioSpot->GetSpawnedPickup());
	}

	// --- Задание 3: ноутбук учёным ---
	const FName Q3 = bOk ? S.Elder->GetThirdQuest().QuestId : NAME_None;
	if (bOk)
	{
		ScientistTest::SetNotebookHintShown(S.Player, true); // ожидание кончилось
		TestTrue(TEXT("Третье задание открывается"), S.Elder->TryUnlockThirdQuest(S.Player));
		TestTrue(TEXT("Третье задание берётся"), S.Quests->AcceptQuest(Q3));
		bOk = S.ExitAndContinue(*this, TEXT("Третье задание взято, ноутбук выдан"));
	}
	if (bOk)
	{
		TestEqual(TEXT("После выхода выданный ноутбук в сумке"), ScientistTest::CountInBag(S.Player, ScientistTest::LaptopKey), 1);
		S.Barrier->RefreshNow();
		TestTrue(TEXT("После выхода шлагбаум открыт"), S.Barrier->IsOpen());
		S.Scientist->OnTalkOpened(S.Player);
		S.Scientist->BuildTalkView(S.Player, View);
		S.Scientist->HandleTalkPrimary(S.Player);
		TestEqual(TEXT("Третье задание сдано учёному"), S.StateOf(Q3), EQuestState::TurnedIn);
		bOk = S.ExitAndContinue(*this, TEXT("Третье задание сдано"));
	}
	if (bOk)
	{
		S.Guard->OnTalkOpened(S.Player);
		S.Guard->BuildTalkView(S.Player, View);
		S.Guard->HandleTalkPrimary(S.Player);
		TestEqual(TEXT("Задание охранника сдано"), S.StateOf(QGuard), EQuestState::TurnedIn);
		bOk = S.ExitAndContinue(*this, TEXT("Все задания сданы"));
	}
	if (bOk)
	{
		TestEqual(TEXT("После всех выходов сданы все четыре задания"), S.Quests->GetTurnedInQuestCount(), 4);
	}

	S.Teardown();
	ScientistTest::DeleteSlot(S.Slot);
	return true;
}

// ===========================================================================
// Причина ошибки и страховка для уже испорченных сохранений. «Так было»: сохранение записано
// у костра до боя, бандиты перебиты и ноутбук взят, запись хода не сделана — после выхода
// журнал откатывается, а мир помнит зачищенную базу. Страховка при загрузке взводит такую
// базу заново, а ноутбук кладётся по журналу; награду дважды получить нельзя — задание в
// журнале не выполнено, его проходят заново.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveBrokenQuestRepairTest,
	"ContrarySurvivor.Story.BrokenSaveWithClearedBaseIsRepaired", ScientistTestFlags)

bool FSaveBrokenQuestRepairTest::RunTest(const FString& Parameters)
{
	ScientistTest::FScopedTables Tables;
	SaveChainTest::FSession S;
	S.Slot = TEXT("ContrarySaveRepairQA");
	ScientistTest::DeleteSlot(S.Slot);
	if (!TestTrue(TEXT("Мир собран"), S.Build()))
	{
		S.Teardown();
		return false;
	}
	const FQuest First = S.Elder->GetOfferedQuest();
	const FQuest Second = S.Elder->GetSecondQuest();

	// До взятия задания ноутбука на базе нет.
	S.BanditBase->RefreshQuestItem();
	TestNull(TEXT("Задание на бандитов не взято — ноутбука на базе нет"), S.BanditBase->GetSpawnedQuestPickupForQA());

	FQuest FirstDone = First;
	FirstDone.State = EQuestState::TurnedIn;
	S.Quests->RestoreQuests({ FirstDone });
	S.Quests->OfferQuest(Second);
	S.BanditBase->RefreshQuestItem();
	TestNull(TEXT("Задание только предложено — ноутбука на базе нет"), S.BanditBase->GetSpawnedQuestPickupForQA());
	TestTrue(TEXT("Второе задание берётся"), S.Quests->AcceptQuest(Second.QuestId));
	S.BanditBase->RefreshQuestItem();
	TestNotNull(TEXT("Задание взято — ноутбук лежит на базе"), S.BanditBase->GetSpawnedQuestPickupForQA());

	// «Так было»: запись только у костра, до боя.
	TestTrue(TEXT("Сохранение у костра до боя"), S.Player->SaveGame());
	for (int32 Kill = 0; Kill < Second.TargetCount; ++Kill)
	{
		S.Quests->NotifyKill(Second.KillTargetTag);
	}
	S.Give(Second.RequiredItemName, Second.RequiredItemCount);
	TestEqual(TEXT("В игре задание готово к сдаче"), S.StateOf(Second.QuestId), EQuestState::Completed);
	TestTrue(TEXT("Теперь убийства и ноутбук просят запись сохранения"), S.Player->HasPendingAutoSave());

	// Выход БЕЗ записи хода — как в сборке 0.4.0 (8).
	S.Teardown();
	if (!TestTrue(TEXT("Мир после выхода собран"), S.Build()) || !TestTrue(TEXT("Сохранение загрузилось"), S.Player->LoadGameForContinue()))
	{
		S.Teardown();
		return false;
	}
	S.BanditBase->MarkClearedForQA(); // память базы писалась в сохранение сразу — база пуста
	const FQuest* Rolled = S.Quests->FindQuest(Second.QuestId);
	if (TestNotNull(TEXT("Задание на бандитов в журнале"), Rolled))
	{
		TestEqual(TEXT("Так было: журнал откатился к «идти в лагерь бандитов»"), Rolled->State, EQuestState::Active);
		TestEqual(TEXT("Так было: счёт убийств потерян"), Rolled->Progress, 0);
	}
	TestEqual(TEXT("Так было: ноутбука в сумке нет"), ScientistTest::CountInBag(S.Player, ScientistTest::LaptopKey), 0);
	TestEqual(TEXT("Так было: база стоит пустой"), S.BanditBase->GetOccupancyForQA(), EEnemyBaseOccupancy::Cleared);

	// Страховка при загрузке.
	TestEqual(TEXT("Страховка взводит одну базу"), S.Player->RepairQuestTargetsAfterLoad(), 1);
	TestEqual(TEXT("База бандитов снова ждёт героя"), S.BanditBase->GetOccupancyForQA(), EEnemyBaseOccupancy::Dormant);
	TestEqual(TEXT("Повторная страховка ничего не делает"), S.Player->RepairQuestTargetsAfterLoad(), 0);
	S.BanditBase->RefreshQuestItem();
	TestNotNull(TEXT("Ноутбук снова лежит на базе"), S.BanditBase->GetSpawnedQuestPickupForQA());
	const float MoneyBefore = S.Player->GetStats()->GetMoney();
	TestFalse(TEXT("Несделанное задание сдать нельзя — награды дважды не будет"), S.Quests->TurnInQuest(Second.QuestId));
	TestEqual(TEXT("Монеты не изменились"), S.Player->GetStats()->GetMoney(), MoneyBefore);

	// Логово волков: шкуры собраны не все, а логово зачищено — взводится тоже.
	S.Quests->RestoreQuests({ First });
	TestTrue(TEXT("Первое задание берётся"), S.Quests->AcceptQuest(First.QuestId));
	S.WolfDen->MarkClearedForQA();
	TestEqual(TEXT("Логово при несобранных шкурах взводится заново"), S.Player->RepairQuestTargetsAfterLoad(), 1);
	TestEqual(TEXT("Логово снова ждёт героя"), S.WolfDen->GetOccupancyForQA(), EEnemyBaseOccupancy::Dormant);

	// Сданное задание базу не трогает и ноутбук не кладёт.
	FQuest SecondDone = Second;
	SecondDone.State = EQuestState::TurnedIn;
	S.Quests->RestoreQuests({ FirstDone, SecondDone });
	S.BanditBase->MarkClearedForQA();
	TestEqual(TEXT("Сданное задание базу не взводит"), S.Player->RepairQuestTargetsAfterLoad(), 0);
	S.BanditBase->RefreshQuestItem();
	TestNull(TEXT("Задание сдано — ноутбука на базе нет"), S.BanditBase->GetSpawnedQuestPickupForQA());

	S.Teardown();
	ScientistTest::DeleteSlot(S.Slot);
	return true;
}

#endif // CONTRARY_WITH_QA_CHEATS

#endif // WITH_DEV_AUTOMATION_TESTS
