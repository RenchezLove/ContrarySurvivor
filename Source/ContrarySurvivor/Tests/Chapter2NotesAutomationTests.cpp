// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-проверки правок Рината 26.10.2026 (глава 2):
//   1) фотография из ноутбука — крупно по центру над окном разговора, на 16:9 и 20:9 не
//      заходит на окно; места мало — окно разговора опускается;
//   2) записка наёмника: лежит в добыче его тела, при подборе уходит в «Заметки» (не в сумку),
//      сообщение [245]; тело исчезло — записка всё равно в заметках; переживает «Продолжить»;
//   2б) фото с Верой уходит в «Заметки» после показа в разговоре; старое сохранение с концом
//      главы получает его само;
//   - вкладка «Заметки» смартфона: третья кнопка рядом с «Заданиями» и «Картой», список
//     заметок, текст записки [239] поверх бумаги, у фото — только картинка;
//   3) низина: области тумана (одна по умолчанию, кнопки «добавить» и «удалить последнюю»),
//      слои тумана с цветом и плотностью области; число образцов из низины — и цель задания
//      [216] тоже; недостающие места — случайные точки по числу-зерну; свой урон низины.
//
// Запуск: -ExecCmds="Automation RunTests ContrarySurvivor.Chapter2.Notes2610; Quit".
// Слоты сохранения ИЗОЛИРОВАННЫЕ (ASaveTestPlayerCharacter), в конце удаляются.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Chapter2/AnomalySample.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Chapter2/MercenaryEncounter.h"
#include "ContrarySurvivor/Chapter2/PoisonZone.h"
#include "ContrarySurvivor/Chapter2/StoryNoteItem.h"
#include "ContrarySurvivor/Chapter2/VeraNPC.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/CorpseLootWidget.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "AMasterInventoryItem.h"
#include "UInventoryComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags Chapter2NotesTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace Chapter2NotesTest
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

	static void GiveLaptopTurnedIn(APlayerCharacter* Player)
	{
		FQuest Laptop;
		Laptop.QuestId = ContraryQuestIds::DeliverLaptopToScientists;
		Laptop.TurnInNpcTag = ScientistBaseTags::QuestScientist;
		Laptop.TargetCount = 0;
		Laptop.State = EQuestState::TurnedIn;
		Player->GetQuests()->RestoreQuests({ Laptop });
	}

	static UChapter2Component::FEnv Outside()
	{
		UChapter2Component::FEnv E;
		return E;
	}

	// Поставить значение защищённого поля через систему свойств движка (как правка в панели).
	template <typename TProperty, typename TValue>
	static bool SetProp(UObject* Object, const TCHAR* Name, const TValue& Value)
	{
		TProperty* Prop = Object ? FindFProperty<TProperty>(Object->GetClass(), Name) : nullptr;
		if (!Prop)
		{
			return false;
		}
		Prop->SetPropertyValue_InContainer(Object, Value);
		return true;
	}

	// Прямоугольник элемента в холсте-родителе (якорь в точке, без растяжения).
	static FBox2D PointAnchoredBox(const UWidget* Widget)
	{
		const UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		if (!CanvasSlot)
		{
			return FBox2D(ForceInit);
		}
		const FVector2D Pos = CanvasSlot->GetPosition();
		const FVector2D Size = CanvasSlot->GetSize();
		const FVector2D Min = Pos - Size * CanvasSlot->GetAlignment();
		return FBox2D(Min, Min + Size);
	}
}

// ===========================================================================
// 1. Фотография из ноутбука: крупно по центру над окном разговора.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2PhotoAboveDialogTest, "ContrarySurvivor.Chapter2.Notes2610.PhotoAboveDialog", Chapter2NotesTestFlags)

bool FChapter2PhotoAboveDialogTest::RunTest(const FString& Parameters)
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	TestTrue(TEXT("Наименьшая высота фотографии — около трети экрана"),
		S->PhotoMinHeightFraction >= 0.3f && S->PhotoMinHeightFraction <= 0.4f);

	// Окно разговора — как в файле WBP_Dialog (срез раскладки 26.10: подложка 1589,6 на 600,3,
	// привязана к низу середины экрана, поднята на 48,6) и крестик закрытия в правом верхнем углу
	// экрана (72 на 72, середина в 56 точках от правого и верхнего края).
	auto DialogOn = [](double ScreenW)
	{
		const double W = 1589.6, H = 600.3, Lift = 48.6;
		const double Left = ScreenW * 0.5 - 9.8 - W * 0.5;
		return FBox2D(FVector2D(Left, 1080.0 - Lift - H), FVector2D(Left + W, 1080.0 - Lift));
	};
	auto CrossOn = [](double ScreenW)
	{
		return FBox2D(FVector2D(ScreenW - 56.0 - 36.0, 56.0 - 36.0), FVector2D(ScreenW - 56.0 + 36.0, 56.0 + 36.0));
	};
	auto Apart = [](const FBox2D& A, const FBox2D& B)
	{
		return A.Max.X <= B.Min.X + 0.01 || A.Min.X >= B.Max.X - 0.01 || A.Max.Y <= B.Min.Y + 0.01 || A.Min.Y >= B.Max.Y - 0.01;
	};

	for (const double ScreenW : { 1920.0, 2400.0 })
	{
		const FVector2D Screen(ScreenW, 1080.0);
		const TCHAR* Name = ScreenW > 2000.0 ? TEXT("Телефон 20:9") : TEXT("ПК 16:9");
		for (const float Aspect : { 1.5f, 1.0f, 1.78f })
		{
			const FBox2D Dialog = DialogOn(ScreenW);
			const Chapter2Rules::FPhotoPlacement Place = Chapter2Rules::PlacePhotoAboveDialog(Screen, Dialog, Aspect,
				S->PhotoMinHeightFraction, S->PhotoMargin);
			const FBox2D Photo = Place.Photo;
			if (!TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: место для фотографии есть"), Name, Aspect), Photo.bIsValid))
			{
				continue;
			}
			const FVector2D Size = Photo.GetSize();
			AddInfo(FString::Printf(TEXT("%s, пропорции %.2f: фотография %.0f на %.0f в (%.0f, %.0f); окно разговора с %.0f"),
				Name, Aspect, Size.X, Size.Y, Photo.Min.X, Photo.Min.Y, Dialog.Min.Y));
			// Крупная: не ниже 35 % высоты экрана (главный порог) и по площади не меньше 5 % экрана даже для
			// квадратной фотографии на вытянутом телефоне (защита от прежней беды «фото маленькое сбоку»).
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: фотография крупная — высота %.0f не меньше %.0f"), Name, Aspect,
				Size.Y, S->PhotoMinHeightFraction * 1080.0 - 0.5), Size.Y >= S->PhotoMinHeightFraction * 1080.0 - 0.5);
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: площадь фотографии не меньше 5 %% экрана"), Name, Aspect),
				Photo.GetArea() >= 0.05 * Screen.X * Screen.Y);
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: фотография по центру экрана"), Name, Aspect),
				FMath::IsNearlyEqual(Photo.GetCenter().X, Screen.X * 0.5, 0.5));
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: фотография в верхней части экрана"), Name, Aspect),
				Photo.Min.Y <= S->PhotoMargin + 0.5);
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: пропорции сохранены"), Name, Aspect),
				FMath::IsNearlyEqual(Size.X / Size.Y, static_cast<double>(Aspect), 0.01));
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: фотография целиком на экране"), Name, Aspect),
				Photo.Min.X >= -0.01 && Photo.Min.Y >= -0.01 && Photo.Max.X <= Screen.X + 0.01 && Photo.Max.Y <= Screen.Y + 0.01);
			// Окно — там, где оно будет с учётом поджатия.
			FBox2D DialogNow = Dialog;
			if (Place.DialogTop > 0.0)
			{
				DialogNow.Min.Y = Place.DialogTop;
			}
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: фотография не заходит на окно разговора"), Name, Aspect), Apart(Photo, DialogNow));
			TestTrue(FString::Printf(TEXT("%s, пропорции %.2f: фотография не заходит на крестик закрытия"), Name, Aspect), Apart(Photo, CrossOn(ScreenW)));
		}
	}

	// Окно разговора высокое: места над ним мало — окно опускается под фотографию.
	{
		const FVector2D Screen(1920.0, 1080.0);
		const FBox2D Tall(FVector2D(165.0, 200.0), FVector2D(1755.0, 1031.0));
		const Chapter2Rules::FPhotoPlacement Place = Chapter2Rules::PlacePhotoAboveDialog(Screen, Tall, 1.5f,
			S->PhotoMinHeightFraction, S->PhotoMargin);
		TestTrue(TEXT("Высокое окно: фотография всё равно не меньше 35 % экрана"),
			Place.Photo.bIsValid && Place.Photo.GetSize().Y >= S->PhotoMinHeightFraction * 1080.0 - 0.5);
		TestTrue(TEXT("Высокое окно: окно разговора опускается под фотографию"),
			Place.DialogTop > Tall.Min.Y && Place.DialogTop >= Place.Photo.Max.Y);
	}
	// Окна разговора нет — фотография по центру экрана.
	{
		const Chapter2Rules::FPhotoPlacement Place = Chapter2Rules::PlacePhotoAboveDialog(FVector2D(1920.0, 1080.0), FBox2D(ForceInit), 1.5f,
			S->PhotoMinHeightFraction, S->PhotoMargin);
		TestTrue(TEXT("Без окна разговора фотография по центру экрана"),
			Place.Photo.bIsValid && Place.Photo.GetCenter().Equals(FVector2D(960.0, 540.0), 0.5) && Place.DialogTop <= 0.0);
	}
	return true;
}

// ===========================================================================
// 2. Записка наёмника: добыча тела -> «Заметки», сообщение [245], сохранение.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2MercenaryNoteTest, "ContrarySurvivor.Chapter2.Notes2610.MercenaryNote", Chapter2NotesTestFlags)

bool FChapter2MercenaryNoteTest::RunTest(const FString& Parameters)
{
	using namespace Chapter2NotesTest;
	const FString Slot = TEXT("ContraryChapter2NoteQA");
	DeleteSlot(Slot);
	UWorld* World = Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AVeraNPC* Vera = Spawn<AVeraNPC>(World, FVector(500.0f, 0.0f, 100.0f));
	AMercenaryEncounter* Encounter = Spawn<AMercenaryEncounter>(World, FVector(3000.0f, 0.0f, 100.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Player || !Vera || !Encounter || !C || !Quests || !Inventory)
	{
		AddError(TEXT("Не заспавнены герой, Вера или точка наёмника"));
		Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	Encounter->SetMercenaryClassForTest(AEnemyCharacter::StaticClass());
	const UChapter2Settings* S = UChapter2Settings::Get();

	// Наёмник побеждён: возвращает его тело (или null).
	auto DefeatMercenary = [&]() -> AEnemyCharacter*
	{
		C->ResetForNewGame();
		GiveLaptopTurnedIn(Player);
		Quests->OfferQuest(Vera->GetSamplesQuest());
		Quests->AcceptQuest(Chapter2Ids::SamplesQuest);
		C->GiveApparatus();
		for (const TCHAR* Id : { TEXT("NoteQA_A"), TEXT("NoteQA_B"), TEXT("NoteQA_C") })
		{
			C->CollectSample(Id);
		}
		C->Step(0.1f, Outside());
		Encounter->UpdateEncounter(Player);
		AEnemyCharacter* Merc = Encounter->GetMercenary();
		if (!Merc || !Merc->GetStats())
		{
			return nullptr;
		}
		Merc->GetStats()->ApplyDamage(100000.0f);
		Encounter->UpdateEncounter(Player); // заметит гибель: записка в добыче, наёмник побеждён
		return Merc;
	};

	// --- Тело обыскивают: записка уходит в заметки, а не в сумку ---
	AEnemyCharacter* Merc = DefeatMercenary();
	if (!TestNotNull(TEXT("Наёмник появился и погиб"), Merc))
	{
		Destroy(World);
		DeleteSlot(Slot);
		return false;
	}
	TestTrue(TEXT("Наёмник побеждён"), C->IsMercenaryDefeated());
	UCorpseLootComponent* Loot = Merc->FindComponentByClass<UCorpseLootComponent>();
	AStoryNoteItem* NoteItem = nullptr;
	if (Loot)
	{
		for (AMasterInventoryItem* Item : Loot->GetLootItems())
		{
			NoteItem = NoteItem ? NoteItem : Cast<AStoryNoteItem>(Item);
		}
	}
	if (!TestNotNull(TEXT("В добыче тела наёмника лежит записка"), NoteItem))
	{
		Destroy(World);
		DeleteSlot(Slot);
		return false;
	}
	TestEqual(TEXT("Записка в окне обыска называется [238]"), NoteItem->GetItemDisplayText().ToString(), FString(TEXT("Записка наёмника")));
	TestFalse(TEXT("До обыска записки в заметках нет"), C->HasNote(StoryNoteIds::MercenaryNote));

	UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
	Window->InitCorpseLoot(Loot, Player);
	const FCorpseLootTakenSummary Summary = Window->TakeAllFromGroup();
	TestTrue(TEXT("«Забрать всё» взяло записку"), Summary.bNoteTaken);
	TestTrue(TEXT("Записка — в заметках смартфона"), C->HasNote(StoryNoteIds::MercenaryNote));
	bool bNoteInBag = false;
	for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
	{
		bNoteInBag |= Cast<AStoryNoteItem>(Item) != nullptr;
	}
	TestFalse(TEXT("Записки нет среди вещей сумки (её нельзя продать или выбросить)"), bNoteInBag);
	TestFalse(TEXT("В теле записки больше нет"), Loot->GetLootItems().ContainsByPredicate([](const AMasterInventoryItem* I) { return Cast<AStoryNoteItem>(I) != nullptr; }));
	const FString Line = Window->BuildTakenSummaryText(Summary).ToString();
	TestTrue(FString::Printf(TEXT("Сообщение о подборе — [245] (вышло «%s»)"), *Line), Line.Contains(S->MercenaryNotePickupText.ToString()));
	TestEqual(TEXT("[245] дословно"), S->MercenaryNotePickupText.ToString(),
		FString(TEXT("Найдена записка. Прочитать её можно в смартфоне, во вкладке «Заметки».")));

	// Переживает «Продолжить».
	TestTrue(TEXT("Сохранение записалось"), Player->SaveGame());
	C->ResetForNewGame();
	TestFalse(TEXT("Новая игра — заметок нет"), C->HasNote(StoryNoteIds::MercenaryNote));
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		TestTrue(TEXT("В сохранении записка есть"), Save->StoryNotes.Contains(StoryNoteIds::MercenaryNote));
		C->RestoreFromSave(Save);
	}
	TestTrue(TEXT("После «Продолжить» записка в заметках"), C->HasNote(StoryNoteIds::MercenaryNote));

	// --- Тело исчезло, а записку не взяли — она всё равно в заметках ---
	Merc = DefeatMercenary();
	if (TestNotNull(TEXT("Второй наёмник появился и погиб"), Merc))
	{
		TestFalse(TEXT("Пока тело лежит, записки в заметках нет"), C->HasNote(StoryNoteIds::MercenaryNote));
		Merc->Destroy(); // не забранная добыча уничтожается вместе с телом
		Encounter->UpdateEncounter(Player);
		TestTrue(TEXT("Тело исчезло — записка всё равно в заметках"), C->HasNote(StoryNoteIds::MercenaryNote));
	}

	// --- Старое сохранение: наёмник побеждён, а заметок в нём ещё нет ---
	{
		UContrarySaveGame* Old = NewObject<UContrarySaveGame>();
		Old->bChapter2MercenaryDefeated = true;
		Old->bChapter2EndShown = false;
		C->RestoreFromSave(Old);
		TestTrue(TEXT("Старое сохранение после боя — записка в заметках"), C->HasNote(StoryNoteIds::MercenaryNote));
		TestFalse(TEXT("Старое сохранение до конца главы — фото нет"), C->HasNote(StoryNoteIds::VeraPhoto));
		Old->bChapter2EndShown = true;
		C->RestoreFromSave(Old);
		TestTrue(TEXT("Старое сохранение после конца главы — фото в заметках"), C->HasNote(StoryNoteIds::VeraPhoto));
	}

	Destroy(World);
	DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 2б. Фото с Верой: после показа в разговоре [250]–[252] — в «Заметках».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2VeraPhotoNoteTest, "ContrarySurvivor.Chapter2.Notes2610.VeraPhotoNote", Chapter2NotesTestFlags)

bool FChapter2VeraPhotoNoteTest::RunTest(const FString& Parameters)
{
	using namespace Chapter2NotesTest;
	const FString Slot = TEXT("ContraryChapter2PhotoNoteQA");
	DeleteSlot(Slot);
	UWorld* World = Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AVeraNPC* Vera = Spawn<AVeraNPC>(World, FVector(500.0f, 0.0f, 100.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Player || !Vera || !C || !Quests)
	{
		AddError(TEXT("Не заспавнены герой или Вера"));
		Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	C->ResetForNewGame();
	GiveLaptopTurnedIn(Player);
	Quests->OfferQuest(Vera->GetSamplesQuest());
	Quests->AcceptQuest(Chapter2Ids::SamplesQuest);
	C->GiveApparatus();
	for (const TCHAR* Id : { TEXT("PhotoQA_A"), TEXT("PhotoQA_B"), TEXT("PhotoQA_C") })
	{
		C->CollectSample(Id);
	}
	C->NotifyMercenaryDefeated();

	FNpcTalkView View;
	Vera->OnTalkOpened(Player);
	TestEqual(TEXT("Вера принимает образцы"), Vera->GetStage(), EVeraTalkStage::Samples);
	Vera->HandleTalkPrimary(Player); // [229] — сдать, на экране фото [250]
	TestEqual(TEXT("Последний файл — фото на экране"), Vera->GetStage(), EVeraTalkStage::LastFile);
	TestTrue(TEXT("Фото на экране"), C->IsPhotoShown());
	TestFalse(TEXT("Пока фото только показано, в заметках его нет"), C->HasNote(StoryNoteIds::VeraPhoto));
	Vera->HandleTalkPrimary(Player); // [251] — герой узнал себя
	TestTrue(TEXT("Фото увидено — оно в заметках"), C->HasNote(StoryNoteIds::VeraPhoto));
	TestFalse(TEXT("Записки наёмника этот путь не даёт"), C->HasNote(StoryNoteIds::MercenaryNote));
	Vera->OnTalkClosed(Player); // [252] закрыт — конец главы
	TestTrue(TEXT("Конец главы показан"), C->IsChapterEndShown());
	int32 PhotoCount = 0;
	for (const FName& Id : C->GetNotes())
	{
		PhotoCount += (Id == StoryNoteIds::VeraPhoto) ? 1 : 0;
	}
	TestEqual(TEXT("Фото в заметках одно"), PhotoCount, 1);

	FStoryNoteView Note;
	TestTrue(TEXT("Заметка с фото описана"), StoryNotes::Describe(StoryNoteIds::VeraPhoto, Note));
	TestEqual(TEXT("Подпись фото — [246]"), Note.Title.ToString(), FString(TEXT("Фото из ноутбука")));
	TestTrue(TEXT("У фото нет текста — только картинка"), Note.Body.IsEmpty());
	TestNotNull(TEXT("Картинка фото — T_PhotoHeroVera"), Note.Image);

	Destroy(World);
	DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 2в. Вкладка «Заметки» смартфона (окно из файла WBP_Pda и окно, построенное кодом).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2PdaNotesTabTest, "ContrarySurvivor.Chapter2.Notes2610.PdaNotesTab", Chapter2NotesTestFlags)

bool FChapter2PdaNotesTabTest::RunTest(const FString& Parameters)
{
	using namespace Chapter2NotesTest;
	UClass* AssetClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	if (!TestNotNull(TEXT("Окно смартфона /Game/UI/WBP_Pda загрузилось"), AssetClass))
	{
		return false;
	}
	const FString Slot = TEXT("ContraryChapter2PdaNotesQA");
	DeleteSlot(Slot);
	UWorld* World = Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	if (!Player || !C)
	{
		AddError(TEXT("Не заспавнен герой"));
		Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	C->ResetForNewGame();
	const UChapter2Settings* S = UChapter2Settings::Get();
	TestEqual(TEXT("[237] вкладка"), S->NotesTabText.ToString(), FString(TEXT("Заметки")));
	TestEqual(TEXT("[238] название записки"), S->MercenaryNoteTitleText.ToString(), FString(TEXT("Записка наёмника")));
	TestTrue(TEXT("[239] начало"), S->MercenaryNoteBodyText.ToString().StartsWith(TEXT("Сыч объявился у лагеря учёных.")));
	TestTrue(TEXT("[239] конец"), S->MercenaryNoteBodyText.ToString().EndsWith(TEXT("Фото прилагаю, чтобы не обознался.\n— К.")));
	// Свободная для текста область картинки записки note_mercenary_v3 (решение лида 26.10): 7 %, 31 %, 7 %, 6 %.
	TestTrue(TEXT("Отступы текста записки по умолчанию — под свободную область картинки"),
		S->NoteTextMargins == FMargin(0.07f, 0.31f, 0.07f, 0.06f));

	for (UClass* WindowClass : { AssetClass, static_cast<UClass*>(UPdaScreenWidget::StaticClass()) })
	{
		const bool bFile = (WindowClass == AssetClass);
		const TCHAR* What = bFile ? TEXT("Окно из файла") : TEXT("Окно, построенное кодом");
		C->ResetForNewGame();
		UPdaScreenWidget* Window = CreateWidget<UPdaScreenWidget>(World, WindowClass);
		if (!TestNotNull(*FString::Printf(TEXT("%s: создалось"), What), Window))
		{
			continue;
		}
		Window->InitPda(Player);

		UButton* NotesButton = Window->GetNotesTabButtonForTest();
		if (!TestNotNull(*FString::Printf(TEXT("%s: кнопка вкладки «Заметки» есть"), What), NotesButton))
		{
			continue;
		}
		const UTextBlock* Caption = Cast<UTextBlock>(Window->WidgetTree->FindWidget(TEXT("NotesTabText")));
		TestTrue(*FString::Printf(TEXT("%s: на кнопке написано [237] «Заметки»"), What),
			Caption && Caption->GetText().ToString() == TEXT("Заметки"));

		// Кнопка — в том же меню, что «Задания» и «Карта», того же размера, что «Карта», и не
		// налезает на другие кнопки меню.
		const UWidget* MapButton = Window->WidgetTree->FindWidget(TEXT("MapTabButton"));
		TestTrue(*FString::Printf(TEXT("%s: «Заметки» в том же меню, что «Карта»"), What),
			MapButton && NotesButton->GetParent() == MapButton->GetParent());
		const FBox2D NotesBox = PointAnchoredBox(NotesButton);
		TestTrue(*FString::Printf(TEXT("%s: «Заметки» того же размера, что «Карта»"), What),
			MapButton && NotesBox.GetSize().Equals(PointAnchoredBox(MapButton).GetSize(), 0.5));
		for (const TCHAR* Other : { TEXT("QuestsTabButton"), TEXT("MapTabButton"), TEXT("CloseButton") })
		{
			const FBox2D OtherBox = PointAnchoredBox(Window->WidgetTree->FindWidget(Other));
			const bool bApart = !OtherBox.bIsValid || NotesBox.Max.Y <= OtherBox.Min.Y + 0.5 || NotesBox.Min.Y >= OtherBox.Max.Y - 0.5
				|| NotesBox.Max.X <= OtherBox.Min.X + 0.5 || NotesBox.Min.X >= OtherBox.Max.X - 0.5;
			TestTrue(*FString::Printf(TEXT("%s: «Заметки» (%.0f..%.0f по высоте) не налезает на %s (%.0f..%.0f)"), What,
				NotesBox.Min.Y, NotesBox.Max.Y, Other, OtherBox.Min.Y, OtherBox.Max.Y), bApart);
		}
		if (const UCanvasPanel* Menu = Cast<UCanvasPanel>(NotesButton->GetParent()))
		{
			const UCanvasPanelSlot* MenuSlot = Cast<UCanvasPanelSlot>(Menu->Slot);
			const FVector2D MenuSize = MenuSlot ? MenuSlot->GetSize() : FVector2D::ZeroVector;
			TestTrue(*FString::Printf(TEXT("%s: «Заметки» внутри меню %.0f на %.0f"), What, MenuSize.X, MenuSize.Y),
				NotesBox.Min.X >= -0.5 && NotesBox.Min.Y >= -0.5 && NotesBox.Max.X <= MenuSize.X + 0.5 && NotesBox.Max.Y <= MenuSize.Y + 0.5);
		}
		const UWidget* NotesPanel = Window->GetNotesPanelForTest();
		const UWidget* QuestsPanel = Window->WidgetTree->FindWidget(TEXT("QuestsPanel"));
		TestTrue(*FString::Printf(TEXT("%s: вкладка заметок на месте вкладки «Задания»"), What),
			NotesPanel && QuestsPanel && NotesPanel->GetParent() == QuestsPanel->GetParent()
			&& PointAnchoredBox(NotesPanel).Min.Equals(PointAnchoredBox(QuestsPanel).Min, 0.5)
			&& PointAnchoredBox(NotesPanel).GetSize().Equals(PointAnchoredBox(QuestsPanel).GetSize(), 0.5));
		TestTrue(*FString::Printf(TEXT("%s: открывается на «Заданиях», вкладка заметок спрятана"), What),
			!Window->IsNotesTabShown() && NotesPanel && NotesPanel->GetVisibility() == ESlateVisibility::Collapsed);

		// Заметок нет — список пуст.
		Window->ShowNotesTab();
		TestTrue(*FString::Printf(TEXT("%s: вкладка заметок открыта"), What), Window->IsNotesTabShown() && !Window->IsMapTabShown());
		TestEqual(*FString::Printf(TEXT("%s: заметок пока нет"), What), Window->GetNoteRowCount(), 0);
		TestTrue(*FString::Printf(TEXT("%s: вкладка заметок видна, «Задания» и «Карта» спрятаны"), What),
			NotesPanel && NotesPanel->GetVisibility() != ESlateVisibility::Collapsed
			&& QuestsPanel && QuestsPanel->GetVisibility() == ESlateVisibility::Collapsed
			&& Window->WidgetTree->FindWidget(TEXT("MapPanel"))->GetVisibility() == ESlateVisibility::Collapsed);

		// Записка и фото — в списке, новые сверху.
		C->AddNote(StoryNoteIds::MercenaryNote, /*bAnnounce=*/false);
		C->AddNote(StoryNoteIds::VeraPhoto, /*bAnnounce=*/false);
		Window->ShowNotesTab();
		TestEqual(*FString::Printf(TEXT("%s: в списке две заметки"), What), Window->GetNoteRowCount(), 2);
		TestEqual(*FString::Printf(TEXT("%s: сверху — новая (фото)"), What), Window->GetSelectedNoteId(), StoryNoteIds::VeraPhoto);
		TestTrue(*FString::Printf(TEXT("%s: у фото — картинка без текста"), What),
			Window->IsNoteImageShownForTest() && Window->GetShownNoteTextForTest().IsEmpty());
		Window->SelectNote(StoryNoteIds::MercenaryNote);
		TestEqual(*FString::Printf(TEXT("%s: у записки — текст [239]"), What), Window->GetShownNoteTextForTest().ToString(),
			S->MercenaryNoteBodyText.ToString());
		TestTrue(*FString::Printf(TEXT("%s: у записки — бумага (картинка или светлый лист)"), What), Window->IsNoteImageShownForTest());

		// Назад к заданиям.
		Window->ShowTab(false);
		TestTrue(*FString::Printf(TEXT("%s: «Задания» снова видны, заметки спрятаны"), What),
			!Window->IsNotesTabShown() && NotesPanel->GetVisibility() == ESlateVisibility::Collapsed
			&& QuestsPanel->GetVisibility() != ESlateVisibility::Collapsed);
		Window->ShowTab(true);
		TestTrue(*FString::Printf(TEXT("%s: «Карта» открывается после заметок"), What), Window->IsMapTabShown());
	}

	Destroy(World);
	DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 3. Низина: области тумана, число образцов, свой урон.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChapter2PoisonZoneSettingsTest, "ContrarySurvivor.Chapter2.Notes2610.PoisonZoneSettings", Chapter2NotesTestFlags)

bool FChapter2PoisonZoneSettingsTest::RunTest(const FString& Parameters)
{
	using namespace Chapter2NotesTest;
	const FString Slot = TEXT("ContraryChapter2ZoneQA");
	DeleteSlot(Slot);
	UWorld* World = Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Низина с пятью образцами: ставится отложенно, как экземпляр на уровне с правленым полем.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	const FTransform ZoneXform(FVector(20000.0f, 0.0f, 300.0f));
	APoisonZone* Zone = World->SpawnActor<APoisonZone>(APoisonZone::StaticClass(), ZoneXform, Params);
	if (!TestNotNull(TEXT("Низина поставлена"), Zone))
	{
		Destroy(World);
		return false;
	}
	TestEqual(TEXT("По умолчанию в низине три образца"), Zone->GetSampleCount(), 3);
	TestTrue(TEXT("Число образцов — настройка экземпляра"), SetProp<FIntProperty>(Zone, TEXT("SampleCount"), 5));
	Zone->FinishSpawning(ZoneXform);
	TestEqual(TEXT("Настройка «Сколько образцов» — 5"), Zone->GetSampleCount(), 5);

	// --- Туман: одна область по умолчанию, слои с её цветом и плотностью ---
	TArray<UPoisonFogAreaComponent*> Areas = Zone->GetFogAreas();
	if (TestEqual(TEXT("Область тумана по умолчанию одна"), Areas.Num(), 1) && Areas[0])
	{
		const UPoisonFogAreaComponent* Area = Areas[0];
		TestTrue(TEXT("Цвет по умолчанию — нынешний зеленоватый (как в M_PoisonFog)"),
			Area->FogColor.Equals(FLinearColor(0.5f, 0.7f, 0.25f, 1.0f), 0.001f));
		TestEqual(TEXT("Слоёв столько, сколько задано в области"), Area->GetLayers().Num(), Area->LayerCount);
		TestTrue(TEXT("Слоёв немного (дёшево для телефона)"), Area->LayerCount >= 1 && Area->LayerCount <= 6);
		const float Expected = UPoisonFogAreaComponent::LayerOpacity(Area->Density, Area->LayerCount);
		bool bAllLayersOk = Area->GetLayers().Num() > 0;
		for (const UStaticMeshComponent* Layer : Area->GetLayers())
		{
			UMaterialInstanceDynamic* Fog = Layer ? Cast<UMaterialInstanceDynamic>(Layer->GetMaterial(0)) : nullptr;
			bAllLayersOk &= Fog && Fog->K2_GetVectorParameterValue(TEXT("FogColor")).Equals(Area->FogColor, 0.001f)
				&& FMath::IsNearlyEqual(Fog->K2_GetScalarParameterValue(TEXT("MaxOpacity")), Expected, 0.001f)
				&& Layer->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Layer->CastShadow
				&& Layer->GetAttachParent() == Area;
		}
		TestTrue(TEXT("Каждый слой — материал тумана с цветом и плотностью области, без столкновений и тени"), bAllLayersOk);
		// Сквозь все слои в середине — ровно плотность области.
		const float Through = 1.0f - FMath::Pow(1.0f - Expected, static_cast<float>(Area->LayerCount));
		TestTrue(TEXT("Сквозь все слои плотность — как задана"), FMath::IsNearlyEqual(Through, Area->Density, 0.001f));
	}

#if WITH_EDITOR
	// Кнопки «Добавить область тумана» и «Удалить последнюю».
	Zone->AddFogArea();
	Areas = Zone->GetFogAreas();
	TestEqual(TEXT("Кнопка добавила вторую область"), Areas.Num(), 2);
	UPoisonFogAreaComponent* Added = nullptr;
	for (UPoisonFogAreaComponent* Area : Areas)
	{
		Added = (Area && Area->GetFName() != TEXT("FogArea1")) ? Area : Added;
	}
	if (TestNotNull(TEXT("Новая область — отдельный компонент"), Added))
	{
		TestTrue(TEXT("Новая область — в списке добавленных экземпляру (сохраняется с уровнем)"), Zone->GetInstanceComponents().Contains(Added));
		TestEqual(TEXT("У новой области свои слои"), Added->GetLayers().Num(), Added->LayerCount);
		TestTrue(TEXT("Новая область того же зеленоватого цвета"), Added->FogColor.Equals(FLinearColor(0.5f, 0.7f, 0.25f, 1.0f), 0.001f));
		// Настройка новой области: цвет и плотность.
		Added->FogColor = FLinearColor(0.9f, 0.2f, 0.1f, 1.0f);
		Added->Density = 0.8f;
		Added->LayerCount = 2;
		Added->RebuildLayers();
		UMaterialInstanceDynamic* Fog = Added->GetLayers().Num() > 0 ? Cast<UMaterialInstanceDynamic>(Added->GetLayers()[0]->GetMaterial(0)) : nullptr;
		TestTrue(TEXT("Свой цвет и плотность новой области доходят до слоёв"), Added->GetLayers().Num() == 2 && Fog
			&& Fog->K2_GetVectorParameterValue(TEXT("FogColor")).Equals(FLinearColor(0.9f, 0.2f, 0.1f, 1.0f), 0.001f)
			&& FMath::IsNearlyEqual(Fog->K2_GetScalarParameterValue(TEXT("MaxOpacity")), UPoisonFogAreaComponent::LayerOpacity(0.8f, 2), 0.001f));
		TArray<TWeakObjectPtr<UStaticMeshComponent>> AddedLayers;
		for (UStaticMeshComponent* Layer : Added->GetLayers())
		{
			AddedLayers.Add(Layer);
		}
		Zone->RemoveLastFogArea();
		TestEqual(TEXT("«Удалить последнюю» убрала добавленную область"), Zone->GetFogAreas().Num(), 1);
		bool bLayersGone = true;
		for (const TWeakObjectPtr<UStaticMeshComponent>& Layer : AddedLayers)
		{
			bLayersGone &= !Layer.IsValid() || !Layer->IsRegistered();
		}
		TestTrue(TEXT("Слои удалённой области ушли вместе с ней"), bLayersGone);
	}
	Zone->RemoveLastFogArea();
	TestEqual(TEXT("Первую область кнопка не удаляет"), Zone->GetFogAreas().Num(), 1);
#endif

	// --- Образцы: пять мест (три места-компонента и две случайные точки по зерну) ---
	TArray<FVector> Places, PlacesAgain;
	TArray<FName> Ids, IdsAgain;
	Zone->ComputeSamplePlaces(Places, Ids);
	Zone->ComputeSamplePlaces(PlacesAgain, IdsAgain);
	TestEqual(TEXT("Мест столько, сколько образцов"), Places.Num(), 5);
	TestTrue(TEXT("Имена первых трёх — как у старых сохранений (Sample1–Sample3)"),
		Ids.Num() >= 3 && Ids[0] == TEXT("Sample1") && Ids[1] == TEXT("Sample2") && Ids[2] == TEXT("Sample3"));
	TestTrue(TEXT("Недостающие — Sample4 и Sample5"), Ids.Num() == 5 && Ids[3] == TEXT("Sample4") && Ids[4] == TEXT("Sample5"));
	bool bSame = Places.Num() == PlacesAgain.Num();
	for (int32 Index = 0; bSame && Index < Places.Num(); ++Index)
	{
		bSame &= Places[Index].Equals(PlacesAgain[Index], 0.01);
	}
	TestTrue(TEXT("Случайные места одни и те же при каждом запуске"), bSame);
	bool bInside = true;
	for (const FVector& Place : Places)
	{
		FVector Flat = Place;
		Flat.Z = Zone->GetActorLocation().Z;
		bInside &= Zone->IsInside(Flat);
	}
	TestTrue(TEXT("Все места — внутри низины"), bInside);
	TestEqual(TEXT("Низина поставила пять образцов"), Zone->GetSpawnedSamples().Num(), 5);
	TestEqual(TEXT("Образцов низин на уровне — пять"), UChapter2Component::GetZoneSampleTotal(World), 5);

	// --- Цель задания [216] берёт число из низины ---
	ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AVeraNPC* Vera = Spawn<AVeraNPC>(World, FVector(500.0f, 0.0f, 100.0f));
	UChapter2Component* C = Player ? Player->GetChapter2() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	if (Player && Vera && C && Quests && Stats)
	{
		Player->UseTestSaveSlot(Slot);
		C->ResetForNewGame();
		GiveLaptopTurnedIn(Player);
		Vera->OnTalkOpened(Player); // задание попадает в журнал
		const FQuest* Q = Quests->FindQuest(Chapter2Ids::SamplesQuest);
		TestTrue(TEXT("Предложенное задание ждёт пять образцов"), Q && Q->TargetCount == 5);
		Vera->HandleTalkPrimary(Player);
		Vera->HandleTalkPrimary(Player);
		Vera->OnTalkClosed(Player);
		Q = Quests->FindQuest(Chapter2Ids::SamplesQuest);
		if (TestNotNull(TEXT("Задание взято"), Q))
		{
			const FText Line = QuestObjectiveText::BuildObjectives(*Q, INVTEXT("{Objective} {Done} из {Total}"), INVTEXT(", "));
			TestEqual(TEXT("Строка цели [216] — с числом из низины"), Line.ToString(), FString(TEXT("Найти в отравленной низине образцы (0 из 5)")));
		}
		// Сохранение, где задание взято на три образца: цель приводится к пяти, собранное не теряется.
		Quests->SetObjectiveTarget(Chapter2Ids::SamplesQuest, 3);
		C->CollectSample(TEXT("Sample1"));
		C->SyncStoryState();
		Q = Quests->FindQuest(Chapter2Ids::SamplesQuest);
		TestTrue(TEXT("Старое сохранение «1 из 3» стало «1 из 5»"), Q && Q->TargetCount == 5 && Q->Progress == 1 && Q->State == EQuestState::Active);

		// Свой урон низины.
		TestTrue(TEXT("Свой урон включается галочкой"), SetProp<FBoolProperty>(Zone, TEXT("bOverrideDamage"), true));
		SetProp<FFloatProperty>(Zone, TEXT("DamageNoMaskPerSecond"), 20.0f);
		Player->SetActorLocation(Zone->GetActorLocation());
		const UChapter2Component::FEnv Env = C->GatherEnv();
		TestTrue(TEXT("Герой в низине"), Env.bInZone);
		TestEqual(TEXT("Урон без аппарата — свой у низины"), Env.DamageNoMaskPerSecond, 20.0f);
		const float Before = Stats->GetHealth();
		C->Step(1.0f, Env);
		TestTrue(FString::Printf(TEXT("За секунду без аппарата ушло 20 здоровья (было %.0f, стало %.0f)"), Before, Stats->GetHealth()),
			FMath::IsNearlyEqual(Before - Stats->GetHealth(), 20.0f, 0.5f));
	}
	else
	{
		AddError(TEXT("Не заспавнены герой или Вера"));
	}

	Destroy(World);
	DeleteSlot(Slot);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
