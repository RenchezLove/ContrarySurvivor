// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-проверки правки Рината 27.10.2026 (глава 2): «Перенеси наши настройки отравленной
// низины … выше в настройках… Сам BP переименуй… Образцы тоже переименуй в аномальные
// образования. Сами аномальные образования сделай предметом. Этот предмет попадает в инвентарь,
// когда игрок берёт его.»
//   1) подобранное аномальное образование лежит в сумке предметом [226]/[247] (одна стопка),
//      продать, выбросить и переложить нельзя, при сдаче задания Вера их забирает; тексты
//      [216], [217], [236] новые;
//   2) старое сохранение (образцы подобраны, задание не сдано, в сумке пусто, в журнале старые
//      тексты) — после «Продолжить» предметы в сумке и тексты новые; повторное «Продолжить» не
//      удваивает предметы;
//   3) разделы настроек зоны первыми (метаданные PrioritizeCategories у классов);
//   4) переименование без потерь: уровень L_World_C даёт экземпляр зоны нового класса с прежними
//      настройками (снимок до правки — logs/zone_dump_2710_before.txt), чертёж тоже.
//
// Запуск: -ExecCmds="Automation RunTests ContrarySurvivor.Chapter2.Formation2710; Quit".
// Слоты сохранения ИЗОЛИРОВАННЫЕ (ASaveTestPlayerCharacter), в конце удаляются.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Chapter2/AnomalyFormation.h"
#include "ContrarySurvivor/Chapter2/AnomalyFormationItem.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Chapter2/ToxicAnomalyZone.h"
#include "ContrarySurvivor/Chapter2/VeraNPC.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "AMasterInventoryItem.h"
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags Chapter2FormationTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace Chapter2FormationTest
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

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}

	static FQuest LaptopTurnedIn()
	{
		FQuest Laptop;
		Laptop.QuestId = ContraryQuestIds::DeliverLaptopToScientists;
		Laptop.TurnInNpcTag = ScientistBaseTags::QuestScientist;
		Laptop.TargetCount = 0;
		Laptop.State = EQuestState::TurnedIn;
		return Laptop;
	}

	// Предметы «Аномальное образование» в сумке (записи, не штуки).
	static TArray<AMasterInventoryItem*> FormationEntries(UInventoryComponent* Inventory)
	{
		TArray<AMasterInventoryItem*> Out;
		if (Inventory)
		{
			for (AMasterInventoryItem* Item : Inventory->GetInventoryItems())
			{
				if (IsValid(Item) && AAnomalyFormationItem::IsFormationKey(Item->ItemName))
				{
					Out.Add(Item);
				}
			}
		}
		return Out;
	}

	static FString ObjectiveLine(const FQuest& Q)
	{
		return QuestObjectiveText::BuildObjectives(Q, INVTEXT("{Objective} {Done} из {Total}"), INVTEXT(", ")).ToString();
	}

	template <typename TProperty, typename TValue>
	static TValue GetProp(const UObject* Object, const TCHAR* Name, bool& bFound)
	{
		const TProperty* Prop = Object ? FindFProperty<TProperty>(Object->GetClass(), Name) : nullptr;
		bFound = (Prop != nullptr);
		return Prop ? static_cast<TValue>(Prop->GetPropertyValue_InContainer(Object)) : TValue();
	}
}

// ===========================================================================
// 1. Подобранное образование — предмет в сумке; продать и выбросить нельзя; Вера забирает.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2FormationItemTest, "ContrarySurvivor.Chapter2.Formation2710.ItemInBag", Chapter2FormationTestFlags)

bool FChapter2FormationItemTest::RunTest(const FString& Parameters)
{
	using namespace Chapter2FormationTest;
	const FString Slot = TEXT("ContraryChapter2FormationQA");
	DeleteSlot(Slot);
	UWorld* World = Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AVeraNPC* Vera = Spawn<AVeraNPC>(World, FVector(500.0f, 0.0f, 100.0f));
	TArray<AAnomalyFormation*> Formations;
	for (int32 i = 0; i < 3; ++i)
	{
		AAnomalyFormation* F = Spawn<AAnomalyFormation>(World, FVector(10000.0f + i * 500.0f, 0.0f, 50.0f));
		if (F)
		{
			F->InitFormation(FName(*FString::Printf(TEXT("Sample%d"), i + 1)), nullptr);
		}
		Formations.Add(F);
	}
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Player || !Vera || !C || !Quests || !Inventory || Formations.Contains(nullptr))
	{
		AddError(TEXT("Не заспавнены герой, Вера или аномальные образования"));
		Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	C->ResetForNewGame();
	Quests->RestoreQuests({ LaptopTurnedIn() });
	const UChapter2Settings* S = UChapter2Settings::Get();
	const FName QId = Chapter2Ids::SamplesQuest;

	// Тексты дословно из РЕПЛИКИ.txt (27.10.2026).
	TestEqual(TEXT("[226] название предмета"), S->FormationNameText.ToString(), FString(TEXT("Аномальное образование")));
	TestEqual(TEXT("[247] описание предмета"), S->FormationDescriptionText.ToString(),
		FString(TEXT("Светящийся нарост из отравленной низины. Учёные Веры ждут такие образцы для исследований.")));
	TestEqual(TEXT("[236] инструкция в смартфоне"), S->SamplesGuideText.ToString(), FString(TEXT(
		"Отравленная низина лежит к востоку от лагеря учёных. Перед входом наденьте маску и следите за запасом воздуха: когда он кончится, начнёт убывать здоровье. Пустой баллон заправят в лагере.\n"
		"В низине левый стик превращается в детектор. Идите туда, откуда приходят сигналы. Соберите аномальные образования и отнесите их Вере.")));

	// Взять задание у Веры.
	FNpcTalkView View;
	Vera->OnTalkOpened(Player);
	Vera->BuildTalkView(Player, View);
	Vera->HandleTalkPrimary(Player); // [213]
	Vera->BuildTalkView(Player, View);
	Vera->HandleTalkPrimary(Player); // [215]
	Vera->OnTalkClosed(Player);
	if (!TestEqual(TEXT("Задание взято"), Quests->FindQuest(QId) ? Quests->FindQuest(QId)->State : EQuestState::NotStarted, EQuestState::Active))
	{
		Destroy(World);
		DeleteSlot(Slot);
		return false;
	}
	if (const FQuest* InLog = Quests->FindQuest(QId))
	{
		TestEqual(TEXT("[216] строка цели"), ObjectiveLine(*InLog), FString(TEXT("Собрать в отравленной низине аномальные образования (0 из 3)")));
		TestEqual(TEXT("[217] куда нести"), InLog->TurnInHintText.ToString(), FString(TEXT("Отнести аномальные образования Вере")));
	}
	TestEqual(TEXT("До подбора образований в сумке нет"), C->CountFormationsInBag(), 0);

	// Подбор: образование сразу в сумке.
	C->RefreshSampleActors();
	TestTrue(TEXT("Первое образование подобрано"), Formations[0]->TryCollect(Player));
	TestEqual(TEXT("После подбора в сумке одно образование"), C->CountFormationsInBag(), 1);
	TArray<AMasterInventoryItem*> Entries = FormationEntries(Inventory);
	AMasterInventoryItem* Item = Entries.Num() > 0 ? Entries[0] : nullptr;
	if (!TestNotNull(TEXT("Предмет «Аномальное образование» в сумке"), Item))
	{
		Destroy(World);
		DeleteSlot(Slot);
		return false;
	}
	TestTrue(TEXT("Предмет — класса AAnomalyFormationItem"), Item->IsA<AAnomalyFormationItem>());
	TestTrue(TEXT("Предмет сюжетный"), Item->bStoryItem);
	TestEqual(TEXT("Категория — задание (не теряется при смерти)"), Item->GetItemCategory(), EItemCategory::Quest);
	TestEqual(TEXT("Название в сумке — [226]"), Item->GetItemDisplayText().ToString(), FString(TEXT("Аномальное образование")));
	TestEqual(TEXT("Описание в сумке — [247]"), Item->ItemDescription, S->FormationDescriptionText.ToString());
	TestEqual(TEXT("Значок — /Game/UI/Chapter2/T_IconFormation (мягкая ссылка)"), Item->GetItemIcon().ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/UI/Chapter2/T_IconFormation.T_IconFormation")));
	TestTrue(TEXT("Предмет сумки скрыт в мире"), Item->IsHidden());

	TestTrue(TEXT("Второе образование подобрано"), Formations[1]->TryCollect(Player));
	TestEqual(TEXT("Два образования в сумке"), C->CountFormationsInBag(), 2);
	TestEqual(TEXT("Образования — одна стопка (одна плитка)"), FormationEntries(Inventory).Num(), 1);
	TestEqual(TEXT("Счётчик цели [216] — два"), C->GetSamplesDone(), 2);

	// Продать, выбросить — нельзя; объяснение своё (не про снаряжение учёных).
	Item = FormationEntries(Inventory).Num() > 0 ? FormationEntries(Inventory)[0] : nullptr;
	if (Item)
	{
		const float MoneyBefore = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
		Player->Inv_DropItem(Item);
		TestEqual(TEXT("Образование не выбрасывается"), C->CountFormationsInBag(), 2);
		Player->Shop_SellItem(Item, 50.0f);
		Player->Shop_SellItemQty(Item, 50.0f, 2);
		TestEqual(TEXT("Образование не продаётся"), C->CountFormationsInBag(), 2);
		TestEqual(TEXT("Денег не прибавилось"), Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f, MoneyBefore);
		FText Reason;
		TestFalse(TEXT("Торговец образование не берёт"), GetDefault<AMasterTrader>()->CanBuyFromPlayer(Item, Player, Reason));
		TestEqual(TEXT("Торговец объясняет своей надписью про образование"), Reason.ToString(), S->FormationLockedText.ToString());
		TestNotEqual(TEXT("Надпись не про снаряжение учёных [235]"), Reason.ToString(), S->StoryItemLockedText.ToString());
		TestTrue(TEXT("Отказ для образования (тот же отказ закрывает перекладывание в хранилище)"), Player->RefuseStoryItem(Item));
	}

	// Пропало из сумки (любой путь) — возвращается само по счётчику.
	for (AMasterInventoryItem* Entry : FormationEntries(Inventory))
	{
		Inventory->RemoveItem(Entry);
		Entry->Destroy();
	}
	C->SyncStoryState();
	TestEqual(TEXT("Пропавшие образования вернулись в сумку по счётчику"), C->CountFormationsInBag(), 2);

	// Третье — задание готово к сдаче; образования всё ещё в сумке.
	TestTrue(TEXT("Третье образование подобрано"), Formations[2]->TryCollect(Player));
	TestEqual(TEXT("Задание готово к сдаче"), Quests->FindQuest(QId)->State, EQuestState::Completed);
	if (const FQuest* InLog = Quests->FindQuest(QId))
	{
		TestEqual(TEXT("[216] строка цели — три из трёх"), ObjectiveLine(*InLog), FString(TEXT("Собрать в отравленной низине аномальные образования (3 из 3)")));
	}
	C->SyncStoryState();
	TestEqual(TEXT("Готово к сдаче — три образования в сумке"), C->CountFormationsInBag(), 3);

	// Сдача Вере: она забирает образования.
	C->NotifyMercenaryDefeated();
	Vera->OnTalkOpened(Player);
	TestEqual(TEXT("После боя Вера принимает образования"), Vera->GetStage(), EVeraTalkStage::Samples);
	TestFalse(TEXT("Сдача не закрывает разговор"), Vera->HandleTalkPrimary(Player));
	TestEqual(TEXT("Задание сдано"), Quests->FindQuest(QId)->State, EQuestState::TurnedIn);
	TestEqual(TEXT("Вера забрала образования — в сумке их нет"), C->CountFormationsInBag(), 0);
	TestEqual(TEXT("Ни одной записи образования в сумке"), FormationEntries(Inventory).Num(), 0);
	Vera->OnTalkClosed(Player);

	Destroy(World);
	DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 2. Старое сохранение: образцы подобраны, задание не сдано, в сумке пусто, тексты старые.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2FormationOldSaveTest, "ContrarySurvivor.Chapter2.Formation2710.OldSave", Chapter2FormationTestFlags)

bool FChapter2FormationOldSaveTest::RunTest(const FString& Parameters)
{
	using namespace Chapter2FormationTest;
	const FString Slot = TEXT("ContraryChapter2FormationOldQA");
	DeleteSlot(Slot);
	UWorld* World = Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	AVeraNPC* Vera = Spawn<AVeraNPC>(World, FVector(500.0f, 0.0f, 100.0f));
	ASaveTestPlayerCharacter* P1 = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UChapter2Component* C1 = P1 ? P1->GetChapter2() : nullptr;
	UQuestComponent* Q1 = P1 ? P1->GetQuests() : nullptr;
	if (!Vera || !P1 || !C1 || !Q1 || !P1->GetInventory())
	{
		AddError(TEXT("Не заспавнены герой или Вера"));
		Destroy(World);
		return false;
	}
	P1->UseTestSaveSlot(Slot);
	C1->ResetForNewGame();
	const FName QId = Chapter2Ids::SamplesQuest;

	// Журнал как в сохранении до 27.10.2026: тексты [216]/[217] старые, подобраны Sample1 и Sample2.
	FQuest Old = Vera->GetSamplesQuest();
	Old.KillObjectiveLabel = FText::FromString(TEXT("Найти в отравленной низине образцы"));
	Old.ObjectiveFormatOverride = FText::FromString(TEXT("Найти в отравленной низине образцы ({Done} из {Total})"));
	Old.TurnInHintText = FText::FromString(TEXT("Отнести образцы Вере"));
	Old.GiverNpcTag = Chapter2Ids::VeraTag;
	Old.TurnInNpcTag = Chapter2Ids::VeraTag;
	Old.State = EQuestState::Active;
	Old.Progress = 2;
	Q1->RestoreQuests({ LaptopTurnedIn(), Old });
	UContrarySaveGame* Snapshot = NewObject<UContrarySaveGame>(GetTransientPackage());
	Snapshot->bChapter2ApparatusGiven = true;
	Snapshot->Chapter2AirSeconds = 60.0f;
	Snapshot->Chapter2PickedSamples = { FName(TEXT("Sample1")), FName(TEXT("Sample2")) };
	C1->RestoreFromSave(Snapshot);
	// В старой версии предметов не было: сумку очищаем от того, что могла положить новая.
	for (AMasterInventoryItem* Entry : FormationEntries(P1->GetInventory()))
	{
		P1->GetInventory()->RemoveItem(Entry);
		Entry->Destroy();
	}
	TestEqual(TEXT("Как в старом сохранении — образований в сумке нет"), C1->CountFormationsInBag(), 0);
	TestTrue(TEXT("Сохранение записано"), P1->SaveGame());
	if (const UContrarySaveGame* Saved = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		TestEqual(TEXT("В слоте подобраны два образца (имена прежние)"), Saved->Chapter2PickedSamples.Num(), 2);
		bool bOldText = false;
		for (const FQuest& Q : Saved->Quests)
		{
			bOldText |= (Q.QuestId == QId && Q.ObjectiveFormatOverride.ToString().StartsWith(TEXT("Найти в отравленной низине образцы")));
		}
		TestTrue(TEXT("В слоте журнал со старым текстом [216]"), bOldText);
	}
	P1->Destroy();

	// «Продолжить» новой версией.
	ASaveTestPlayerCharacter* P2 = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UChapter2Component* C2 = P2 ? P2->GetChapter2() : nullptr;
	UQuestComponent* Q2 = P2 ? P2->GetQuests() : nullptr;
	if (!P2 || !C2 || !Q2)
	{
		AddError(TEXT("Не заспавнен второй герой"));
		Destroy(World);
		DeleteSlot(Slot);
		return false;
	}
	P2->UseTestSaveSlot(Slot);
	TestTrue(TEXT("«Продолжить» отработал"), P2->LoadGameForContinue());
	C2->Step(0.1f, UChapter2Component::FEnv());
	TestEqual(TEXT("Старое сохранение: два подобранных образования — два предмета в сумке"), C2->CountFormationsInBag(), 2);
	TestEqual(TEXT("Старое сохранение: одна стопка"), FormationEntries(P2->GetInventory()).Num(), 1);
	TestTrue(TEXT("Подбор по прежнему имени Sample1 помнится"), C2->IsSamplePicked(TEXT("Sample1")));
	if (const FQuest* InLog = Q2->FindQuest(QId))
	{
		TestEqual(TEXT("Задание по-прежнему идёт"), InLog->State, EQuestState::Active);
		TestEqual(TEXT("Старое сохранение: текст [216] новый"), ObjectiveLine(*InLog),
			FString(TEXT("Собрать в отравленной низине аномальные образования (2 из 3)")));
		TestEqual(TEXT("Старое сохранение: текст [217] новый"), InLog->TurnInHintText.ToString(), FString(TEXT("Отнести аномальные образования Вере")));
	}
	else
	{
		AddError(TEXT("После «Продолжить» задания в журнале нет"));
	}

	// Новое сохранение с предметами: повторное «Продолжить» не удваивает их.
	TestTrue(TEXT("Сохранение с предметами записано"), P2->SaveGame());
	if (const UContrarySaveGame* Saved = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		int32 InSlot = 0;
		for (const FSavedInventoryEntry& E : Saved->InventoryEntries)
		{
			if (AAnomalyFormationItem::IsFormationKey(E.ItemName))
			{
				InSlot += E.StackCount;
				TestTrue(TEXT("В слоте предмет записан своим классом"), E.ClassPath.Contains(TEXT("AnomalyFormationItem")));
			}
		}
		TestEqual(TEXT("В слоте два образования"), InSlot, 2);
	}
	P2->Destroy();
	ASaveTestPlayerCharacter* P3 = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UChapter2Component* C3 = P3 ? P3->GetChapter2() : nullptr;
	if (P3 && C3)
	{
		P3->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Второе «Продолжить» отработал"), P3->LoadGameForContinue());
		C3->Step(0.1f, UChapter2Component::FEnv());
		C3->Step(0.1f, UChapter2Component::FEnv());
		TestEqual(TEXT("Второе «Продолжить»: образований по-прежнему два"), C3->CountFormationsInBag(), 2);
		TestEqual(TEXT("Второе «Продолжить»: одна стопка"), FormationEntries(P3->GetInventory()).Num(), 1);
	}

	Destroy(World);
	DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 3. Разделы настроек зоны — первыми (метаданные PrioritizeCategories).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2ZoneCategoriesTest, "ContrarySurvivor.Chapter2.Formation2710.SettingsFirst", Chapter2FormationTestFlags)

bool FChapter2ZoneCategoriesTest::RunTest(const FString& Parameters)
{
#if WITH_EDITORONLY_DATA
	auto Check = [this](const UClass* Class, const TArray<FString>& Expected)
	{
		TArray<FString> Got;
		Class->GetPrioritizeCategories(Got);
		TestEqual(FString::Printf(TEXT("%s: разделы первыми"), *Class->GetName()), FString::Join(Got, TEXT(" | ")), FString::Join(Expected, TEXT(" | ")));
		// Каждый раздел из списка правда есть у свойств класса (иначе панель его выкинет,
		// ObjectPropertyNode.cpp:623-629), и в имени нет пробела (список делится пробелом).
		for (const FString& Category : Expected)
		{
			bool bUsed = false;
			for (TFieldIterator<FProperty> It(Class); It && !bUsed; ++It)
			{
				bUsed = It->GetMetaData(TEXT("Category")) == Category;
			}
			TestTrue(FString::Printf(TEXT("%s: раздел «%s» есть у свойств"), *Class->GetName(), *Category), bUsed);
			TestFalse(FString::Printf(TEXT("%s: в имени раздела «%s» нет пробела"), *Class->GetName(), *Category), Category.Contains(TEXT(" ")));
		}
	};
	Check(AToxicAnomalyZone::StaticClass(), { TEXT("Туман"), TEXT("Аномальные_образования"), TEXT("Урон"), TEXT("Детектор"), TEXT("Зона") });
	Check(UPoisonFogAreaComponent::StaticClass(), { TEXT("Туман") });
	Check(UAnomalyFormationSpotComponent::StaticClass(), { TEXT("Аномальное_образование") });
	Check(AAnomalyFormation::StaticClass(), { TEXT("Аномальное_образование") });

	TestEqual(TEXT("Подпись класса зоны"), AToxicAnomalyZone::StaticClass()->GetMetaData(TEXT("DisplayName")), FString(TEXT("Зона отравленного воздуха")));
	TestEqual(TEXT("Подпись класса образования"), AAnomalyFormation::StaticClass()->GetMetaData(TEXT("DisplayName")), FString(TEXT("Аномальное образование")));
	TestEqual(TEXT("Раздел «Аномальные_образования» панель показывает с пробелом"),
		FName::NameToDisplayString(TEXT("Аномальные_образования"), false), FString(TEXT("Аномальные образования")));
#else
	AddInfo(TEXT("Метаданных классов в этой сборке нет — проверка только в редакторе"));
#endif
	return true;
}

// ===========================================================================
// 4. Переименование без потерь: L_World_C и чертёж зоны с прежними настройками.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2ZoneRedirectsTest, "ContrarySurvivor.Chapter2.Formation2710.LevelKeepsZoneSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FChapter2ZoneRedirectsTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	using namespace Chapter2FormationTest;
	auto Near = [](const FVector& A, const FVector& B) { return A.Equals(B, 0.5); };

	// Проверка одной зоны: что было до переименования (снимок logs/zone_dump_2710_before.txt).
	struct FExpect
	{
		FVector Spot[3];
		FVector FogHalf;
		FVector FogRel;
		bool bLayout = true; // false: уровень — расстановку и числа Ринат правит сам, сверяем только перенос классов и имён
	};
	auto CheckZone = [&](const AToxicAnomalyZone* Zone, const TCHAR* Who, const FExpect& E)
	{
		bool bFound = false;
		if (E.bLayout)
		{
		TestEqual(FString::Printf(TEXT("%s: аномальных образований 3"), Who), GetProp<FIntProperty, int32>(Zone, TEXT("FormationCount"), bFound), 3);
		TestTrue(FString::Printf(TEXT("%s: свойство FormationCount есть"), Who), bFound);
		TestEqual(FString::Printf(TEXT("%s: число-зерно 2610"), Who), GetProp<FIntProperty, int32>(Zone, TEXT("RandomPlacesSeed"), bFound), 2610);
		}
		TestTrue(FString::Printf(TEXT("%s: ставить на землю"), Who), GetProp<FBoolProperty, bool>(Zone, TEXT("bSnapFormationsToGround"), bFound));
		const FObjectPropertyBase* GlowProp = FindFProperty<FObjectPropertyBase>(Zone->GetClass(), TEXT("FormationGlowMaterial"));
		const UObject* Glow = GlowProp ? GlowProp->GetObjectPropertyValue_InContainer(Zone) : nullptr;
		TestEqual(FString::Printf(TEXT("%s: материал свечения M_AnomalyGlow"), Who), Glow ? Glow->GetPathName() : FString(),
			FString(TEXT("/Game/Environment/Chapter2/M_AnomalyGlow.M_AnomalyGlow")));
		const FClassProperty* ClassProp = FindFProperty<FClassProperty>(Zone->GetClass(), TEXT("FormationClass"));
		const UObject* FormationClass = ClassProp ? ClassProp->GetObjectPropertyValue_InContainer(Zone) : nullptr;
		TestTrue(FString::Printf(TEXT("%s: чертёж образования — класс AAnomalyFormation (было AnomalySample)"), Who),
			FormationClass == AAnomalyFormation::StaticClass());
		for (int32 i = 0; i < 3; ++i)
		{
			const FString SpotName = FString::Printf(TEXT("SampleSpot%d"), i + 1);
			const UAnomalyFormationSpotComponent* Spot = FindObject<UAnomalyFormationSpotComponent>(const_cast<AToxicAnomalyZone*>(Zone), *SpotName);
			if (!TestNotNull(FString::Printf(TEXT("%s: место %s нового класса"), Who, *SpotName), Spot))
			{
				continue;
			}
			if (E.bLayout)
			{
				TestTrue(FString::Printf(TEXT("%s: %s на прежнем месте %s (сейчас %s)"), Who, *SpotName, *E.Spot[i].ToString(),
					*Spot->GetRelativeLocation().ToString()), Near(Spot->GetRelativeLocation(), E.Spot[i]));
			}
			bool bIdFound = false;
			TestEqual(FString::Printf(TEXT("%s: %s — имя для сохранения прежнее"), Who, *SpotName),
				GetProp<FNameProperty, FName>(Spot, TEXT("FormationId"), bIdFound), FName(*FString::Printf(TEXT("Sample%d"), i + 1)));
		}
		const UPoisonFogAreaComponent* Fog = FindObject<UPoisonFogAreaComponent>(const_cast<AToxicAnomalyZone*>(Zone), TEXT("FogArea1"));
		if (TestNotNull(FString::Printf(TEXT("%s: область тумана FogArea1"), Who), Fog) && E.bLayout)
		{
			TestTrue(FString::Printf(TEXT("%s: размер области тумана %s (сейчас %s)"), Who, *E.FogHalf.ToString(), *Fog->AreaHalfSize.ToString()),
				Near(Fog->AreaHalfSize, E.FogHalf));
			TestTrue(FString::Printf(TEXT("%s: место области тумана"), Who), Near(Fog->GetRelativeLocation(), E.FogRel));
			TestEqual(FString::Printf(TEXT("%s: плотность тумана 0,45"), Who), Fog->Density, 0.45f);
		}
	};

	// Чертёж: оператор переименует BP_PoisonZone в BP_ToxicAnomalyZone — проверяем тот, что есть.
	UClass* BpClass = nullptr;
	for (const TCHAR* Path : { TEXT("/Game/Environment/Chapter2/BP_ToxicAnomalyZone.BP_ToxicAnomalyZone_C"),
		TEXT("/Game/Environment/Chapter2/BP_PoisonZone.BP_PoisonZone_C") })
	{
		const FString Package = FPackageName::ObjectPathToPackageName(FString(Path));
		if (FPackageName::DoesPackageExist(Package))
		{
			BpClass = LoadObject<UClass>(nullptr, Path);
			if (BpClass)
			{
				AddInfo(FString::Printf(TEXT("Чертёж зоны: %s"), Path));
				break;
			}
		}
	}
	if (TestNotNull(TEXT("Чертёж зоны загружен"), BpClass))
	{
		TestTrue(TEXT("Чертёж зоны — наследник AToxicAnomalyZone (было APoisonZone)"), BpClass->IsChildOf(AToxicAnomalyZone::StaticClass()));
		TArray<FString> Prioritized;
		BpClass->GetPrioritizeCategories(Prioritized);
		TestTrue(TEXT("Чертёж унаследовал порядок разделов (туман первым)"), Prioritized.Num() > 0 && Prioritized[0] == TEXT("Туман"));
		if (const AToxicAnomalyZone* Cdo = Cast<AToxicAnomalyZone>(BpClass->GetDefaultObject()))
		{
			CheckZone(Cdo, TEXT("Чертёж"), FExpect{ { FVector(900.0, 400.0, -300.0), FVector(-300.0, -1000.0, -300.0), FVector(-1300.0, 300.0, -300.0) },
				FVector(1800.0, 1800.0, 120.0), FVector(0.0, 0.0, -180.0) });
		}
	}

	// Уровень: экземпляр зоны с правками Рината.
	UPackage* MapPackage = LoadPackage(nullptr, TEXT("/Game/Maps/L_World_C"), LOAD_None);
	UWorld* Map = MapPackage ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
	if (!TestNotNull(TEXT("Уровень L_World_C загружен"), Map) || !Map->PersistentLevel)
	{
		return false;
	}
	TArray<AToxicAnomalyZone*> Zones;
	const AVeraNPC* LevelVera = nullptr;
	for (AActor* Actor : Map->PersistentLevel->Actors)
	{
		if (AToxicAnomalyZone* Zone = Cast<AToxicAnomalyZone>(Actor))
		{
			Zones.Add(Zone);
		}
		if (const AVeraNPC* V = Cast<AVeraNPC>(Actor))
		{
			LevelVera = V;
		}
	}
	TestEqual(TEXT("На уровне одна зона отравленного воздуха"), Zones.Num(), 1);
	if (Zones.Num() > 0)
	{
		const AToxicAnomalyZone* Zone = Zones[0];
		AddInfo(FString::Printf(TEXT("Экземпляр: %s, класс %s, место %s"), *Zone->GetActorLabel(), *Zone->GetClass()->GetName(),
			*Zone->GetActorLocation().ToString()));
		TestTrue(TEXT("Экземпляр — из чертежа зоны"), BpClass && Zone->GetClass() == BpClass);
		CheckZone(Zone, TEXT("Уровень"), FExpect{ { FVector(120.0, -180.0, -300.0), FVector(-490.0, -1660.0, -300.0), FVector(-1910.0, 770.0, -300.0) },
			FVector(2300.0, 2300.0, 120.0), FVector(0.0, 0.0, -180.0), /*bLayout=*/false });
	}
	if (TestNotNull(TEXT("Вера на уровне"), LevelVera))
	{
		TestEqual(TEXT("Вера на уровне: [216]"), LevelVera->GetSamplesQuest().ObjectiveFormatOverride.ToString(),
			FString(TEXT("Собрать в отравленной низине аномальные образования ({Done} из {Total})")));
		TestEqual(TEXT("Вера на уровне: [217]"), LevelVera->GetSamplesQuest().TurnInHintText.ToString(), FString(TEXT("Отнести аномальные образования Вере")));
	}
#else
	AddInfo(TEXT("Проверка уровня — только в редакторе"));
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
