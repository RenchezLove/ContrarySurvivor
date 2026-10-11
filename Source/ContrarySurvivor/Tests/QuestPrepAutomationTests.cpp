// Fill out your copyright notice in the Description page of Project Settings.

// Подготовительный шаг задания на бандитов (слова Рината 11.10.2026: «Перед тем как игрок
// пойдет на бандитов - надо как-то явно намекнуть, что бы он купил пистолет у торговца. Может
// вообще это сделать частью квеста… Староста скажет… что для игрока он договорился о скидке»,
// «Дополнительно нужно предусмотреть случай когда у ГГ нет денег на пистолет»).
//
// Проверяется вся ветка шагов: обычная покупка со скидкой, пистолет уже есть, нет денег — долг
// и списание с награды, старое сохранение со взятым заданием, бандиты убиты до покупки; скидка
// не действует вне шага и на другие товары. Мигание плиток и кнопки сумки — живой вид, его
// прогон не показывает: проверяются правила, по которым мигание включается.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryItemRow.h"
#include "ContrarySurvivor/Data/ContraryQuestLibrary.h"
#include "ContrarySurvivor/Data/ContraryQuestRow.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "ContrarySurvivor/UI/QuestTargets.h"
#include "UInventoryComponent.h"
#include "AMeleeWeapon.h"
#include "APistol.h"
#include "AQuestItem.h"
#include "ARangedWeapon.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags QuestPrepTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace QuestPrepTest
{
	static const FName PistolRow(TEXT("pistol"));
	static const FName KnifeRow(TEXT("knife"));
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

	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World ? World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params) : nullptr;
	}

	// Проверочные таблицы: предметы — пистолет, нож и ноутбук; таблица заданий пустая, поэтому
	// задания старосты берутся из его значений по умолчанию.
	struct FScopedTables
	{
		UContraryDataSettings* Settings;
		TSoftObjectPtr<UDataTable> SavedItems, SavedQuests;
		UDataTable* Items;
		UDataTable* Quests;

		FScopedTables()
			: Settings(GetMutableDefault<UContraryDataSettings>())
		{
			SavedItems = Settings->ItemTable;
			SavedQuests = Settings->QuestTable;
			Items = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_ItemsForQuestPrepQA"));
			Items->RowStruct = FContraryItemRow::StaticStruct();

			FContraryItemRow Pistol;
			Pistol.DisplayText = FText::FromString(TEXT("QA пистолет"));
			Pistol.ItemClass = APistol::StaticClass();
			Pistol.Category = EItemCategory::Weapon;
			Pistol.MaxStackCount = 1;
			Pistol.Price = 150.0f;
			Items->AddRow(PistolRow, Pistol);

			FContraryItemRow Knife = Pistol;
			Knife.DisplayText = FText::FromString(TEXT("QA нож"));
			Knife.ItemClass = AMeleeWeapon::StaticClass();
			Knife.Price = 50.0f;
			Items->AddRow(KnifeRow, Knife);

			FContraryItemRow Laptop;
			Laptop.LegacyKey = LaptopKey;
			Laptop.DisplayText = FText::FromString(TEXT("QA ноутбук"));
			Laptop.ItemClass = AQuestItem::StaticClass();
			Laptop.Category = EItemCategory::Quest;
			Laptop.MaxStackCount = 999;
			Items->AddRow(FName(TEXT("laptop")), Laptop);

			Quests = NewObject<UDataTable>(GetTransientPackage(), TEXT("DT_QuestsForQuestPrepQA"));
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

	static FShopEntry MakeEntry(FName Row, float Price, UClass* ItemClass)
	{
		FShopEntry Entry;
		Entry.DisplayName = Row.ToString();
		Entry.AnalyticsId = Row.ToString();
		Entry.Price = Price;
		Entry.Kind = EShopEntryKind::Item;
		Entry.ItemClass = ItemClass;
		Entry.ItemRow = Row;
		return Entry;
	}

	static void SetMoney(APlayerCharacter* Player, float Amount)
	{
		UStatsComponent* Stats = Player->GetStats();
		Stats->SpendMoney(Stats->GetMoney());
		Stats->AddMoney(Amount);
	}

	static AMasterInventoryItem* FindInBag(const APlayerCharacter* Player, FName Row)
	{
		for (AMasterInventoryItem* Item : Player->GetInventory()->GetInventoryItems())
		{
			if (APlayerCharacter::DoesItemMatchRow(Item, Row))
			{
				return Item;
			}
		}
		return nullptr;
	}

	static const FQuest* QuestOf(const APlayerCharacter* Player, FName QuestId)
	{
		return Player->GetQuests()->FindQuest(QuestId);
	}

	// Все участники одной проверки. Первое задание старосты уже сдано, второе предложено.
	struct FStage
	{
		FString Slot;
		UWorld* World = nullptr;
		ASaveTestPlayerCharacter* Player = nullptr;
		AElderNPC* Elder = nullptr;
		AMasterTrader* Trader = nullptr;
		AMasterEnemyBase* BanditBase = nullptr;
		UQuestComponent* Quests = nullptr;
		FName Q2 = NAME_None;
		FShopEntry Pistol, Knife;

		bool Setup(FAutomationTestBase& Test, const TCHAR* InSlot, bool bOfferSecond = true)
		{
			Slot = InSlot;
			if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
			{
				UGameplayStatics::DeleteGameInSlot(Slot, 0);
			}
			World = Create();
			Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
			Elder = Spawn<AElderNPC>(World, FVector(2000.0, 0.0, 100.0));
			Trader = Spawn<AMasterTrader>(World, FVector(3000.0, 0.0, 100.0));
			BanditBase = Spawn<AMasterEnemyBase>(World, FVector(9000.0, 0.0, 0.0));
			Quests = Player ? Player->GetQuests() : nullptr;
			if (!World || !Player || !Elder || !Trader || !BanditBase || !Quests || !Player->GetStats() || !Player->GetInventory())
			{
				Test.AddError(TEXT("Не заспавнены участники проверки"));
				return false;
			}
			Player->UseTestSaveSlot(Slot);
			if (const FNameProperty* MarkerProp = FindFProperty<FNameProperty>(AMasterEnemyBase::StaticClass(), TEXT("QuestMarkerTag")))
			{
				MarkerProp->SetPropertyValue_InContainer(BanditBase, Elder->GetSecondQuest().MapMarkerTag);
			}
			Q2 = Elder->GetSecondQuest().QuestId;
			Pistol = MakeEntry(PistolRow, 150.0f, APistol::StaticClass());
			Knife = MakeEntry(KnifeRow, 50.0f, AMeleeWeapon::StaticClass());
			if (bOfferSecond)
			{
				FQuest FirstDone = Elder->GetOfferedQuest();
				FirstDone.State = EQuestState::TurnedIn;
				Quests->RestoreQuests({ FirstDone });
				Quests->OfferQuest(Elder->GetSecondQuest());
			}
			return true;
		}

		void Teardown()
		{
			Destroy(World);
			if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
			{
				UGameplayStatics::DeleteGameInSlot(Slot, 0);
			}
		}

		EQuestPrepStep Step() const
		{
			const FQuest* Quest = QuestOf(Player, Q2);
			return Quest ? Quest->PrepStep : EQuestPrepStep::NotChecked;
		}

		FString Objectives() const
		{
			const FQuest* Quest = QuestOf(Player, Q2);
			return Quest ? QuestObjectiveText::BuildObjectives(*Quest, FText::FromString(TEXT("{Objective} {Done} из {Total}")),
				FText::FromString(TEXT(", "))).ToString() : FString();
		}

		AActor* Target() const
		{
			const FQuest* Quest = QuestOf(Player, Q2);
			return Quest ? QuestTargets::FindTargetActor(World, *Quest) : nullptr;
		}

		// Перебить бандитов и забрать ноутбук прямыми вызовами.
		void FinishObjectives()
		{
			const FQuest& Def = Elder->GetSecondQuest();
			for (int32 Kill = 0; Kill < Def.TargetCount; ++Kill)
			{
				Quests->NotifyKill(Def.KillTargetTag);
			}
			for (int32 Index = 0; Index < Def.RequiredItemCount; ++Index)
			{
				if (AQuestItem* Item = Spawn<AQuestItem>(World, Player->GetActorLocation()))
				{
					Item->ItemName = Def.RequiredItemName;
					Player->GetInventory()->AddItem(Item);
				}
			}
			Quests->SyncInventoryQuests(Player->GetInventory());
		}
	};
}

// ===========================================================================
// Чистое правило шага и выплаты.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPrepRuleTest,
	"ContrarySurvivor.QuestPrep.StepRuleAndPayout", QuestPrepTestFlags)

bool FQuestPrepRuleTest::RunTest(const FString& Parameters)
{
	FQuest Quest;
	Quest.QuestId = FName(TEXT("QA"));
	Quest.State = EQuestState::Active;

	TestEqual(TEXT("У задания без предмета шагов нет"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::None), EQuestPrepStep::Done);

	Quest.PrepItemRow = FName(TEXT("pistol"));
	TestEqual(TEXT("Предмета нет — шаг покупки"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::None), EQuestPrepStep::Buy);
	TestEqual(TEXT("Предмет уже в сумке при первой сверке — шаги засчитаны сразу"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::InBag), EQuestPrepStep::Done);
	TestEqual(TEXT("Предмет уже в слоте при первой сверке — шаги засчитаны сразу"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::InSlot), EQuestPrepStep::Done);

	Quest.PrepStep = EQuestPrepStep::Buy;
	TestEqual(TEXT("На шаге покупки предмет появился в сумке — шаг слота"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::InBag), EQuestPrepStep::Equip);
	TestEqual(TEXT("На шаге покупки предмет сразу в слоте — шаги пройдены"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::InSlot), EQuestPrepStep::Done);
	Quest.PrepStep = EQuestPrepStep::Equip;
	TestEqual(TEXT("На шаге слота предмет продан — снова шаг покупки"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::None), EQuestPrepStep::Buy);
	TestEqual(TEXT("На шаге слота предмет в слоте — шаги пройдены"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::InSlot), EQuestPrepStep::Done);
	Quest.PrepStep = EQuestPrepStep::Done;
	TestEqual(TEXT("Пройденные шаги заново не начинаются, даже если предмет сняли"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::None), EQuestPrepStep::Done);

	Quest.PrepStep = EQuestPrepStep::Buy;
	TestTrue(TEXT("Шаг показывается, пока задание идёт"), UQuestComponent::IsPrepStepShown(Quest));
	Quest.State = EQuestState::Completed;
	TestFalse(TEXT("Цели задания выполнены — шаг больше не показывается"), UQuestComponent::IsPrepStepShown(Quest));
	TestEqual(TEXT("У готового задания шаг не двигается"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::InBag), EQuestPrepStep::Buy);
	Quest.State = EQuestState::NotStarted;
	Quest.PrepStep = EQuestPrepStep::NotChecked;
	TestFalse(TEXT("Задание не взято — шага нет"), UQuestComponent::IsPrepStepShown(Quest));
	TestEqual(TEXT("У невзятого задания шаг не двигается"),
		UQuestComponent::ComputePrepStep(Quest, EQuestPrepPossession::None), EQuestPrepStep::NotChecked);

	Quest.RewardMoney = 250.0f;
	TestEqual(TEXT("Без долга выплата равна награде"), Quest.GetPayoutMoney(), 250.0f);
	Quest.PrepDebt = 70.0f;
	TestEqual(TEXT("Долг списывается с награды"), Quest.GetPayoutMoney(), 180.0f);
	Quest.PrepDebt = 900.0f;
	TestEqual(TEXT("Награда не уходит в минус"), Quest.GetPayoutMoney(), 0.0f);

	TestEqual(TEXT("Событие статистики шага задания"),
		UAnalyticsSubsystem::MakeQuestStepEventId(FName(TEXT("ClearBanditBase")), TEXT("prep_bought_debt")),
		FString(TEXT("quest:step:ClearBanditBase:prep_bought_debt")));
	return true;
}

// ===========================================================================
// Обычный путь: взял задание — цель «купи пистолет» ведёт к торговцу, пистолет со скидкой,
// после покупки — «поставь в слот», после слота — обычные цели задания.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPrepNormalPathTest,
	"ContrarySurvivor.QuestPrep.BuyWithDiscountThenEquip", QuestPrepTestFlags)

bool FQuestPrepNormalPathTest::RunTest(const FString& Parameters)
{
	QuestPrepTest::FScopedTables Tables;
	QuestPrepTest::FStage S;
	if (!S.Setup(*this, TEXT("ContraryQuestPrepNormalQA")))
	{
		S.Teardown();
		return false;
	}
	const FQuest& Def = S.Elder->GetSecondQuest();
	const float WolvesReward = S.Elder->GetOfferedQuest().RewardMoney;
	TestEqual(TEXT("Задание на бандитов просит пистолет"), Def.PrepItemRow, QuestPrepTest::PistolRow);
	TestEqual(TEXT("Цена со скидкой — половина награды за волков"), Def.PrepDealPrice, WolvesReward * 0.5f);
	TestFalse(TEXT("У шага покупки есть текст цели"), Def.PrepBuyObjectiveText.IsEmpty());
	TestFalse(TEXT("У шага слота есть текст цели"), Def.PrepEquipObjectiveText.IsEmpty());
	TestFalse(TEXT("У скидки есть подпись"), Def.PrepDealText.IsEmpty());
	TestFalse(TEXT("У продажи в долг есть подпись"), Def.PrepDebtText.IsEmpty());

	bool bTraderSellsPistol = false;
	for (const FShopEntry& Entry : S.Trader->GetCatalog())
	{
		bTraderSellsPistol |= (Entry.ItemRow == QuestPrepTest::PistolRow);
	}
	TestTrue(TEXT("У торговца в продаже есть пистолет"), bTraderSellsPistol);

	QuestPrepTest::SetMoney(S.Player, 500.0f);

	// Задание только предложено: шага и скидки ещё нет.
	S.Player->UpdateQuestPrepSteps();
	TestEqual(TEXT("До взятия задания шаг не начат"), S.Step(), EQuestPrepStep::NotChecked);
	TestEqual(TEXT("До взятия задания пистолет стоит обычную цену"), S.Player->GetShopBuyPrice(S.Pistol), 150.0f);

	TestTrue(TEXT("Задание берётся"), S.Quests->AcceptQuest(S.Q2));
	S.Player->UpdateQuestPrepSteps();
	TestEqual(TEXT("После взятия — шаг покупки"), S.Step(), EQuestPrepStep::Buy);
	TestEqual(TEXT("Цель на экране — купить пистолет"), S.Objectives(), Def.PrepBuyObjectiveText.ToString());
	TestTrue(TEXT("Цель ведёт к торговцу"), S.Target() == S.Trader);

	const FQuest* Deal = nullptr;
	TestEqual(TEXT("Пистолет стоит цену со скидкой"), S.Player->GetShopBuyPrice(S.Pistol, &Deal), Def.PrepDealPrice);
	TestTrue(TEXT("Скидку даёт задание на бандитов"), Deal && Deal->QuestId == S.Q2);
	Deal = nullptr;
	TestEqual(TEXT("Нож скидки не получает"), S.Player->GetShopBuyPrice(S.Knife, &Deal), 50.0f);
	TestNull(TEXT("У ножа сделки по заданию нет"), Deal);

	TestTrue(TEXT("Пистолет куплен"), S.Player->Shop_BuyEntryQty(S.Pistol, 1));
	TestEqual(TEXT("Списана цена со скидкой"), S.Player->GetStats()->GetMoney(), 500.0f - Def.PrepDealPrice);
	AMasterInventoryItem* Bought = QuestPrepTest::FindInBag(S.Player, QuestPrepTest::PistolRow);
	TestNotNull(TEXT("Пистолет лежит в сумке"), Bought);
	TestEqual(TEXT("После покупки — шаг слота"), S.Step(), EQuestPrepStep::Equip);
	TestEqual(TEXT("Цель на экране — поставить пистолет в слот"), S.Objectives(), Def.PrepEquipObjectiveText.ToString());
	TestNull(TEXT("На шаге слота метки в мире нет"), S.Target());
	TestTrue(TEXT("Плитка купленного пистолета в сумке мигает"), S.Player->IsQuestPrepItemToEquip(Bought));
	TestTrue(TEXT("После закрытия магазина мигает кнопка сумки (идёт шаг слота)"),
		S.Quests->IsAnyPrepStepShown(EQuestPrepStep::Equip));
	TestEqual(TEXT("Долга нет"), QuestPrepTest::QuestOf(S.Player, S.Q2)->PrepDebt, 0.0f);

	// Скидка — на одну покупку.
	TestEqual(TEXT("Второй пистолет стоит обычную цену"), S.Player->GetShopBuyPrice(S.Pistol), 150.0f);

	TestTrue(TEXT("Пистолет встаёт в слот"), Bought && S.Player->TryAdoptRangedWeapon(Bought));
	S.Player->UpdateQuestPrepSteps();
	TestEqual(TEXT("Пистолет в слоте — шаги пройдены"), S.Step(), EQuestPrepStep::Done);
	TestFalse(TEXT("Плитка больше не мигает"), S.Player->IsQuestPrepItemToEquip(Bought));
	TestFalse(TEXT("Кнопке сумки мигать незачем"), S.Quests->IsAnyPrepStepShown(EQuestPrepStep::Equip));
	TestTrue(TEXT("Цель на экране — снова бандиты"), S.Objectives().Contains(Def.KillObjectiveLabel.ToString()));
	TestTrue(TEXT("Цель ведёт к лагерю бандитов"), S.Target() == S.BanditBase);
	TestEqual(TEXT("После шагов скидки нет"), S.Player->GetShopBuyPrice(S.Pistol), 150.0f);

	// Задание сдаётся как раньше, награда полная.
	const float MoneyBefore = S.Player->GetStats()->GetMoney();
	S.FinishObjectives();
	TestTrue(TEXT("Задание сдаётся"), S.Quests->TurnInQuest(S.Q2));
	TestEqual(TEXT("Награда выплачена целиком"), S.Player->GetStats()->GetMoney(), MoneyBefore + Def.RewardMoney);

	S.Teardown();
	return true;
}

// ===========================================================================
// Пистолет уже есть в момент взятия задания (в сумке или в слоте) — шагов игрок не видит.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPrepAlreadyHasPistolTest,
	"ContrarySurvivor.QuestPrep.AlreadyHasPistolSkipsSteps", QuestPrepTestFlags)

bool FQuestPrepAlreadyHasPistolTest::RunTest(const FString& Parameters)
{
	QuestPrepTest::FScopedTables Tables;
	for (const bool bInSlot : { false, true })
	{
		const TCHAR* Where = bInSlot ? TEXT("в слоте") : TEXT("в сумке");
		QuestPrepTest::FStage S;
		if (!S.Setup(*this, TEXT("ContraryQuestPrepHasQA")))
		{
			S.Teardown();
			return false;
		}
		QuestPrepTest::SetMoney(S.Player, 500.0f);
		TestTrue(TEXT("Пистолет куплен заранее по обычной цене"), S.Player->Shop_BuyEntryQty(S.Pistol, 1));
		TestEqual(TEXT("Заранее — обычная цена"), S.Player->GetStats()->GetMoney(), 350.0f);
		AMasterInventoryItem* Owned = QuestPrepTest::FindInBag(S.Player, QuestPrepTest::PistolRow);
		if (bInSlot)
		{
			TestTrue(TEXT("Пистолет встал в слот"), Owned && S.Player->TryAdoptRangedWeapon(Owned));
		}

		TestTrue(TEXT("Задание берётся"), S.Quests->AcceptQuest(S.Q2));
		S.Player->UpdateQuestPrepSteps();
		TestEqual(*FString::Printf(TEXT("Пистолет %s — шаги засчитаны сразу"), Where), S.Step(), EQuestPrepStep::Done);
		TestTrue(*FString::Printf(TEXT("Пистолет %s — цель сразу бандиты"), Where), S.Target() == S.BanditBase);
		TestEqual(*FString::Printf(TEXT("Пистолет %s — скидки нет"), Where), S.Player->GetShopBuyPrice(S.Pistol), 150.0f);
		TestFalse(*FString::Printf(TEXT("Пистолет %s — плитка не мигает"), Where), S.Player->IsQuestPrepItemToEquip(Owned));
		TestFalse(*FString::Printf(TEXT("Пистолет %s — кнопка сумки не мигает"), Where),
			S.Quests->IsAnyPrepStepShown(EQuestPrepStep::Equip) || S.Quests->IsAnyPrepStepShown(EQuestPrepStep::Buy));
		S.Teardown();
	}
	return true;
}

// ===========================================================================
// Денег не хватает: торговец отдаёт пистолет в долг, остаток списывается с награды; долг
// переживает запись и чтение сохранения; в долг продаётся только этот пистолет.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPrepDebtTest,
	"ContrarySurvivor.QuestPrep.NoMoneyBuysOnCreditAndRewardPaysDebt", QuestPrepTestFlags)

bool FQuestPrepDebtTest::RunTest(const FString& Parameters)
{
	QuestPrepTest::FScopedTables Tables;
	QuestPrepTest::FStage S;
	if (!S.Setup(*this, TEXT("ContraryQuestPrepDebtQA")))
	{
		S.Teardown();
		return false;
	}
	const FQuest& Def = S.Elder->GetSecondQuest();
	QuestPrepTest::SetMoney(S.Player, 30.0f);
	TestTrue(TEXT("Задание берётся"), S.Quests->AcceptQuest(S.Q2));
	S.Player->UpdateQuestPrepSteps();

	TestFalse(TEXT("Нож без монет не продаётся"), S.Player->Shop_BuyEntryQty(S.Knife, 1));
	TestEqual(TEXT("Отказ монет не тронул"), S.Player->GetStats()->GetMoney(), 30.0f);
	TestFalse(TEXT("Два пистолета в долг не продаются"), S.Player->Shop_BuyEntryQty(S.Pistol, 2));
	TestNull(TEXT("После отказа пистолета в сумке нет"), QuestPrepTest::FindInBag(S.Player, QuestPrepTest::PistolRow));

	TestTrue(TEXT("Пистолет продан в долг"), S.Player->Shop_BuyEntryQty(S.Pistol, 1));
	TestEqual(TEXT("Игрок отдал все монеты, что были"), S.Player->GetStats()->GetMoney(), 0.0f);
	TestNotNull(TEXT("Пистолет лежит в сумке"), QuestPrepTest::FindInBag(S.Player, QuestPrepTest::PistolRow));
	const float ExpectedDebt = Def.PrepDealPrice - 30.0f;
	const FQuest* InLog = QuestPrepTest::QuestOf(S.Player, S.Q2);
	TestEqual(TEXT("Долг — остаток цены со скидкой"), InLog->PrepDebt, ExpectedDebt);
	TestEqual(TEXT("После покупки — шаг слота"), S.Step(), EQuestPrepStep::Equip);
	TestEqual(TEXT("Выплата по заданию уменьшена на долг"), InLog->GetPayoutMoney(), Def.RewardMoney - ExpectedDebt);

	// Долг пишется в сохранение вместе с журналом.
	const FString SaveSlot = TEXT("ContraryQuestPrepDebtRoundTripQA");
	UContrarySaveGame* Save = Cast<UContrarySaveGame>(UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
	if (TestNotNull(TEXT("Объект сохранения создан"), Save))
	{
		Save->Quests = S.Quests->GetQuests();
		TestTrue(TEXT("Сохранение записано"), UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0));
		const UContrarySaveGame* Loaded = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
		const FQuest* LoadedQuest = Loaded ? Loaded->Quests.FindByPredicate([&S](const FQuest& Quest) { return Quest.QuestId == S.Q2; }) : nullptr;
		if (TestNotNull(TEXT("Задание прочитано из сохранения"), LoadedQuest))
		{
			TestEqual(TEXT("Долг пережил сохранение"), LoadedQuest->PrepDebt, ExpectedDebt);
			TestEqual(TEXT("Шаг пережил сохранение"), LoadedQuest->PrepStep, EQuestPrepStep::Equip);
			TestTrue(TEXT("Отметка «скидка использована» пережила сохранение"), LoadedQuest->bPrepDealUsed);
			TestEqual(TEXT("Предмет шага пережил сохранение"), LoadedQuest->PrepItemRow, QuestPrepTest::PistolRow);
		}
		UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
	}

	S.FinishObjectives();
	TestTrue(TEXT("Задание сдаётся"), S.Quests->TurnInQuest(S.Q2));
	TestEqual(TEXT("Награда пришла за вычетом долга"), S.Player->GetStats()->GetMoney(), Def.RewardMoney - ExpectedDebt);

	S.Teardown();
	return true;
}

// ===========================================================================
// Старое сохранение: задание на бандитов взято до обновления и описания шага не несёт.
// Пистолета нет — шаги появляются; пистолет есть — пропущены; сданное задание не меняется.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPrepOldSaveTest,
	"ContrarySurvivor.QuestPrep.OldSaveWithTakenQuest", QuestPrepTestFlags)

bool FQuestPrepOldSaveTest::RunTest(const FString& Parameters)
{
	QuestPrepTest::FScopedTables Tables;

	auto MakeOldQuest = [](const FQuest& Def, EQuestState State)
	{
		FQuest Old = Def; // как его записала прежняя версия игры: полей подготовки нет
		Old.PrepItemRow = NAME_None;
		Old.PrepDealPrice = 0.0f;
		Old.PrepBuyObjectiveText = FText::GetEmpty();
		Old.PrepEquipObjectiveText = FText::GetEmpty();
		Old.PrepDealText = FText::GetEmpty();
		Old.PrepDebtText = FText::GetEmpty();
		Old.PrepStep = EQuestPrepStep::NotChecked;
		Old.State = State;
		return Old;
	};

	// Пистолета нет — шаги появляются.
	{
		QuestPrepTest::FStage S;
		if (!S.Setup(*this, TEXT("ContraryQuestPrepOldQA"), /*bOfferSecond=*/false))
		{
			S.Teardown();
			return false;
		}
		QuestPrepTest::SetMoney(S.Player, 500.0f);
		S.Quests->RestoreQuests({ MakeOldQuest(S.Elder->GetSecondQuest(), EQuestState::Active) });
		TestTrue(TEXT("Старое задание описания шага не несёт"), QuestPrepTest::QuestOf(S.Player, S.Q2)->PrepItemRow.IsNone());
		S.Player->UpdateQuestPrepSteps();
		TestEqual(TEXT("Описание шага донесено из задания старосты"),
			QuestPrepTest::QuestOf(S.Player, S.Q2)->PrepItemRow, QuestPrepTest::PistolRow);
		TestEqual(TEXT("Старое сохранение без пистолета — шаг покупки"), S.Step(), EQuestPrepStep::Buy);
		TestEqual(TEXT("Скидка действует и для старого сохранения"),
			S.Player->GetShopBuyPrice(S.Pistol), S.Elder->GetSecondQuest().PrepDealPrice);
		TestTrue(TEXT("Цель ведёт к торговцу"), S.Target() == S.Trader);
		S.Teardown();
	}

	// Пистолет в слоте — шаги пропущены.
	{
		QuestPrepTest::FStage S;
		if (!S.Setup(*this, TEXT("ContraryQuestPrepOldQA"), /*bOfferSecond=*/false))
		{
			S.Teardown();
			return false;
		}
		QuestPrepTest::SetMoney(S.Player, 500.0f);
		S.Player->Shop_BuyEntryQty(S.Pistol, 1);
		AMasterInventoryItem* Owned = QuestPrepTest::FindInBag(S.Player, QuestPrepTest::PistolRow);
		TestTrue(TEXT("Пистолет встал в слот"), Owned && S.Player->TryAdoptRangedWeapon(Owned));
		S.Quests->RestoreQuests({ MakeOldQuest(S.Elder->GetSecondQuest(), EQuestState::Active) });
		S.Player->UpdateQuestPrepSteps();
		TestEqual(TEXT("Старое сохранение с пистолетом — шаги пропущены"), S.Step(), EQuestPrepStep::Done);
		TestTrue(TEXT("Цель — бандиты"), S.Target() == S.BanditBase);
		S.Teardown();
	}

	// Задание уже сдано — ничего не меняется.
	{
		QuestPrepTest::FStage S;
		if (!S.Setup(*this, TEXT("ContraryQuestPrepOldQA"), /*bOfferSecond=*/false))
		{
			S.Teardown();
			return false;
		}
		S.Quests->RestoreQuests({ MakeOldQuest(S.Elder->GetSecondQuest(), EQuestState::TurnedIn) });
		S.Player->UpdateQuestPrepSteps();
		const FQuest* Done = QuestPrepTest::QuestOf(S.Player, S.Q2);
		TestEqual(TEXT("Сданное задание осталось сданным"), Done->State, EQuestState::TurnedIn);
		TestFalse(TEXT("У сданного задания шаг не показывается"), UQuestComponent::IsPrepStepShown(*Done));
		TestEqual(TEXT("У сданного задания скидки нет"), S.Player->GetShopBuyPrice(S.Pistol), 150.0f);
		TestFalse(TEXT("Сданное задание сверок не ждёт"), S.Quests->HasPendingPrep());
		S.Teardown();
	}
	return true;
}

// ===========================================================================
// Шаг ведёт, а не запирает: бандиты убиты и ноутбук взят до покупки пистолета — убийства
// засчитаны, задание сдаётся, шаг и скидка пропадают.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPrepBanditsFirstTest,
	"ContrarySurvivor.QuestPrep.BanditsKilledBeforePurchase", QuestPrepTestFlags)

bool FQuestPrepBanditsFirstTest::RunTest(const FString& Parameters)
{
	QuestPrepTest::FScopedTables Tables;
	QuestPrepTest::FStage S;
	if (!S.Setup(*this, TEXT("ContraryQuestPrepBanditsQA")))
	{
		S.Teardown();
		return false;
	}
	const FQuest& Def = S.Elder->GetSecondQuest();
	QuestPrepTest::SetMoney(S.Player, 500.0f);
	TestTrue(TEXT("Задание берётся"), S.Quests->AcceptQuest(S.Q2));
	S.Player->UpdateQuestPrepSteps();
	TestEqual(TEXT("Идёт шаг покупки"), S.Step(), EQuestPrepStep::Buy);

	S.Quests->NotifyKill(Def.KillTargetTag);
	TestEqual(TEXT("Убийство на шаге покупки засчитано"), QuestPrepTest::QuestOf(S.Player, S.Q2)->Progress, 1);
	S.Player->UpdateQuestPrepSteps();
	TestEqual(TEXT("Шаг покупки остаётся, пока задание идёт"), S.Step(), EQuestPrepStep::Buy);

	for (int32 Kill = 1; Kill < Def.TargetCount; ++Kill)
	{
		S.Quests->NotifyKill(Def.KillTargetTag);
	}
	if (AQuestItem* Laptop = QuestPrepTest::Spawn<AQuestItem>(S.World, S.Player->GetActorLocation()))
	{
		Laptop->ItemName = Def.RequiredItemName;
		S.Player->GetInventory()->AddItem(Laptop);
	}
	S.Quests->SyncInventoryQuests(S.Player->GetInventory());
	S.Player->UpdateQuestPrepSteps();

	const FQuest* InLog = QuestPrepTest::QuestOf(S.Player, S.Q2);
	TestEqual(TEXT("Цели выполнены без пистолета — задание готово к сдаче"), InLog->State, EQuestState::Completed);
	TestFalse(TEXT("Шаг покупки больше не показывается"), UQuestComponent::IsPrepStepShown(*InLog));
	TestTrue(TEXT("Готовое задание ведёт к старосте"), S.Target() == S.Elder);
	TestEqual(TEXT("Скидка вне шага не действует"), S.Player->GetShopBuyPrice(S.Pistol), 150.0f);

	const float MoneyBefore = S.Player->GetStats()->GetMoney();
	TestTrue(TEXT("Задание сдаётся"), S.Quests->TurnInQuest(S.Q2));
	TestEqual(TEXT("Награда полная"), S.Player->GetStats()->GetMoney(), MoneyBefore + Def.RewardMoney);
	TestEqual(TEXT("После сдачи скидки нет"), S.Player->GetShopBuyPrice(S.Pistol), 150.0f);

	S.Teardown();
	return true;
}

// ===========================================================================
// Настоящие таблицы проекта: строка задания на бандитов несёт шаг покупки, предмет шага есть
// в таблице предметов, цена со скидкой — половина награды за волков и ниже обычной цены.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestPrepRealTablesTest,
	"ContrarySurvivor.QuestPrep.ProjectTablesCarryTheStep", QuestPrepTestFlags)

bool FQuestPrepRealTablesTest::RunTest(const FString& Parameters)
{
	const AElderNPC* ElderDefaults = GetDefault<AElderNPC>();
	FQuest Wolves, Bandits;
	if (!TestTrue(TEXT("Строка задания на волков читается из таблицы заданий"),
			ContraryQuests::BuildQuestFromRow(ElderDefaults->GetOfferedQuest().QuestId, Wolves))
		|| !TestTrue(TEXT("Строка задания на бандитов читается из таблицы заданий"),
			ContraryQuests::BuildQuestFromRow(ElderDefaults->GetSecondQuest().QuestId, Bandits)))
	{
		return false;
	}
	const FContraryItemRow* ItemRow = ContraryItems::FindRow(Bandits.PrepItemRow);
	TestFalse(TEXT("В таблице у задания на бандитов задан предмет шага"), Bandits.PrepItemRow.IsNone());
	if (TestNotNull(TEXT("Предмет шага есть в таблице предметов"), ItemRow))
	{
		TestTrue(*FString::Printf(TEXT("Цена со скидкой (%.0f) ниже обычной цены предмета (%.0f)"), Bandits.PrepDealPrice, ItemRow->Price),
			Bandits.PrepDealPrice > 0.0f && Bandits.PrepDealPrice < ItemRow->Price);
	}
	TestEqual(TEXT("Цена со скидкой — половина награды за волков"), Bandits.PrepDealPrice, Wolves.RewardMoney * 0.5f);
	TestTrue(TEXT("Долг не может превысить награду за бандитов"), Bandits.PrepDealPrice <= Bandits.RewardMoney);
	TestFalse(TEXT("Текст цели шага покупки задан"), Bandits.PrepBuyObjectiveText.IsEmpty());
	TestFalse(TEXT("Текст цели шага слота задан"), Bandits.PrepEquipObjectiveText.IsEmpty());
	TestFalse(TEXT("Подпись скидки задана"), Bandits.PrepDealText.IsEmpty());
	TestFalse(TEXT("Подпись продажи в долг задана"), Bandits.PrepDebtText.IsEmpty());
	TestTrue(TEXT("У задания на волков шага нет"), Wolves.PrepItemRow.IsNone());
	AddInfo(FString::Printf(TEXT("Цена со скидкой %.0f, реплика выдачи: %s"), Bandits.PrepDealPrice, *Bandits.Description.ToString()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
