// Fill out your copyright notice in the Description page of Project Settings.

// Первая встреча со старостой после удаления реплики про пистолет (слова Рината 11.10.2026:
// «Реплики 020 и 069 вообще удали. Только смотри чтобы квест был проходимым. Т.е. удали без
// последствий для проходимости диалога и квеста»).
//
// Разговор проходится НАСТОЯЩИМ окном разговора (UDialogScreenWidget) без кнопок на экране:
// нажатие основной кнопки — тот же обработчик, что в игре. Проверяется: все шаги идут подряд
// до закрытия окна, реплики про пистолет нет, аптечка выдана, задание про волков взято,
// отметка «вступление сыграно» записана; разговор, прерванный посередине, и сохранение, где
// вступление уже сыграно, не зависают и вступление не повторяют.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/DialogScreenWidget.h"
#include "UInventoryComponent.h"
#include "AConsumableItem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"

static constexpr EAutomationTestFlags ElderIntroTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace ElderIntroTest
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

	static int32 CountMedkits(const APlayerCharacter* Player)
	{
		int32 Count = 0;
		for (const AMasterInventoryItem* Item : Player->GetInventory()->GetInventoryItems())
		{
			const AConsumableItem* Consumable = Cast<AConsumableItem>(Item);
			if (Consumable && Consumable->ConsumableType == EConsumableType::Medkit)
			{
				Count += FMath::Max(1, Consumable->GetStackCount());
			}
		}
		return Count;
	}

	// Один разговор со старостой: окно, счётчик закрытий и то, что делает контроллер при открытии.
	struct FTalk
	{
		UDialogScreenWidget* Window = nullptr;
		int32 CloseRequests = 0;

		void Open(AElderNPC* Elder, APlayerCharacter* Player)
		{
			// Контроллер при открытии разговора предлагает журналу текущее задание старосты.
			Player->GetQuests()->OfferQuest(Elder->GetQuestForPlayer(Player->GetQuests()));
			Window = NewObject<UDialogScreenWidget>(GetTransientPackage());
			Window->AddToRoot();
			CloseRequests = 0;
			Window->OnCloseRequested.AddLambda([this]() { ++CloseRequests; });
			Window->InitDialog(Elder, Player);
		}

		void Close()
		{
			if (Window)
			{
				Window->OnCloseRequested.Clear();
				Window->RemoveFromRoot();
				Window = nullptr;
			}
		}
	};
}

// ===========================================================================
// 1. Первая встреча проходит от начала до конца; реплики про пистолет нет ни в одном шаге.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElderIntroWalkthroughTest,
	"ContrarySurvivor.ElderIntro.FirstMeetingRunsToTheEndWithoutPistolLine", ElderIntroTestFlags)

bool FElderIntroWalkthroughTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryElderIntroWalkQA");
	ElderIntroTest::DeleteSlot(Slot);
	UWorld* World = ElderIntroTest::Create();
	ASaveTestPlayerCharacter* Player = ElderIntroTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	AElderNPC* Elder = ElderIntroTest::Spawn<AElderNPC>(World, FVector(300.0, 0.0, 100.0));
	if (!World || !Player || !Elder || !Player->GetQuests() || !Player->GetInventory())
	{
		AddError(TEXT("Не заспавнены игрок и староста"));
		ElderIntroTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	const FName WolvesId = Elder->GetOfferedQuest().QuestId;
	const UQuestComponent* Quests = Player->GetQuests();
	const TArray<FElderIntroLine>& Lines = Elder->GetIntroLines();

	// Состав вступления: пять шагов, аптечка на третьем, задание на четвёртом, про пистолет — нигде.
	TestEqual(TEXT("Во вступлении пять реплик (шестая, про пистолет, удалена)"), Lines.Num(), 5);
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FString Said = Lines[Index].NPCText.ToString() + TEXT(" ") + Lines[Index].ButtonLabel.ToString();
		TestFalse(*FString::Printf(TEXT("Шаг %d: реплика и ответ не пустые"), Index + 1),
			Lines[Index].NPCText.IsEmpty() || Lines[Index].ButtonLabel.IsEmpty());
		TestFalse(*FString::Printf(TEXT("Шаг %d: про пистолет ни слова («%s»)"), Index + 1, *Said), Said.Contains(TEXT("пистолет")));
		TestFalse(*FString::Printf(TEXT("Шаг %d: ответа «Учту.» нет"), Index + 1), Lines[Index].ButtonLabel.ToString().Contains(TEXT("Учту")));
	}
	if (Lines.Num() == 5)
	{
		TestEqual(TEXT("Аптечка выдаётся на третьей реплике"), Lines[2].Action, EElderIntroAction::GiveGift);
		TestEqual(TEXT("Задание про волков берётся на четвёртой реплике"), Lines[3].Action, EElderIntroAction::StartQuest);
		TestEqual(TEXT("Последняя реплика ничего не запускает"), Lines[4].Action, EElderIntroAction::None);
		TestTrue(TEXT("Сразу после задания идёт реплика про чужих людей"), Lines[4].NPCText.ToString().Contains(TEXT("чужие люди")));
	}

	// Проход настоящим окном: нажимаем основную кнопку, пока окно не попросит закрыть себя.
	ElderIntroTest::FTalk Talk;
	Talk.Open(Elder, Player);
	TestTrue(TEXT("Первая встреча начинается со вступления"), Talk.Window->IsInIntroForQA());
	int32 Presses = 0;
	while (Talk.CloseRequests == 0 && Presses < 20)
	{
		const int32 Step = Talk.Window->GetIntroStepForQA();
		TestEqual(*FString::Printf(TEXT("Нажатие %d приходится на шаг %d"), Presses + 1, Presses + 1), Step, Presses);
		const int32 MedkitsBefore = ElderIntroTest::CountMedkits(Player);
		const FQuest* Before = Quests->FindQuest(WolvesId);
		const EQuestState StateBefore = Before ? Before->State : EQuestState::NotStarted;

		Talk.Window->PressPrimaryForQA();
		++Presses;

		const FQuest* After = Quests->FindQuest(WolvesId);
		if (Lines.IsValidIndex(Step) && Lines[Step].Action == EElderIntroAction::GiveGift)
		{
			TestEqual(TEXT("По кнопке «Взять аптечку» в сумке появилась одна аптечка"), ElderIntroTest::CountMedkits(Player), MedkitsBefore + 1);
		}
		else
		{
			TestEqual(*FString::Printf(TEXT("Шаг %d аптечек не выдаёт"), Step + 1), ElderIntroTest::CountMedkits(Player), MedkitsBefore);
		}
		if (Lines.IsValidIndex(Step) && Lines[Step].Action == EElderIntroAction::StartQuest)
		{
			TestTrue(TEXT("По кнопке «Взяться за дело» задание про волков взято"), After && After->State == EQuestState::Active);
		}
		else
		{
			TestTrue(*FString::Printf(TEXT("Шаг %d состояние задания не меняет"), Step + 1), After && After->State == StateBefore);
		}
	}
	TestEqual(TEXT("Разговор закрылся после пятого нажатия"), Presses, 5);
	TestEqual(TEXT("Окно попросило закрыть себя ровно один раз"), Talk.CloseRequests, 1);
	Talk.Close();

	TestEqual(TEXT("После разговора в сумке одна аптечка"), ElderIntroTest::CountMedkits(Player), 1);
	const FQuest* Wolves = Quests->FindQuest(WolvesId);
	TestTrue(TEXT("После разговора задание про волков взято"), Wolves && Wolves->State == EQuestState::Active);
	const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject();
	if (TestNotNull(TEXT("Сохранение читается"), Save))
	{
		TestTrue(TEXT("Отметка «вступление сыграно» записана"), Save->bElderHookShown);
		TestTrue(TEXT("Отметка «аптечка выдана» записана"), Save->bElderFirstGiftGiven);
	}

	// Повторный разговор: вступления нет, аптечка второй раз не выдаётся, задание не сбрасывается.
	Talk.Open(Elder, Player);
	TestFalse(TEXT("Во втором разговоре вступление не повторяется"), Talk.Window->IsInIntroForQA());
	Talk.Window->PressPrimaryForQA();
	TestEqual(TEXT("Аптечка по-прежнему одна"), ElderIntroTest::CountMedkits(Player), 1);
	Wolves = Quests->FindQuest(WolvesId);
	TestTrue(TEXT("Задание про волков по-прежнему взято"), Wolves && Wolves->State == EQuestState::Active);
	Talk.Close();

	ElderIntroTest::Destroy(World);
	ElderIntroTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 2. Разговор прерван посередине и сохранения, записанные до удаления реплики: ничего не
//    зависает. Номер шага в сохранение не пишется — хранятся только отметки и журнал.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FElderIntroInterruptedAndOldSavesTest,
	"ContrarySurvivor.ElderIntro.InterruptedTalkAndOldSavesDoNotHang", ElderIntroTestFlags)

bool FElderIntroInterruptedAndOldSavesTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryElderIntroBreakQA");
	ElderIntroTest::DeleteSlot(Slot);
	UWorld* World = ElderIntroTest::Create();
	ASaveTestPlayerCharacter* Player = ElderIntroTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	AElderNPC* Elder = ElderIntroTest::Spawn<AElderNPC>(World, FVector(300.0, 0.0, 100.0));
	if (!World || !Player || !Elder || !Player->GetQuests() || !Player->GetInventory())
	{
		AddError(TEXT("Не заспавнены игрок и староста"));
		ElderIntroTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	const FName WolvesId = Elder->GetOfferedQuest().QuestId;
	UQuestComponent* Quests = Player->GetQuests();
	ElderIntroTest::FTalk Talk;

	// а) Игрок ушёл после второй реплики (до аптечки): следующая встреча начинает вступление заново.
	Talk.Open(Elder, Player);
	Talk.Window->PressPrimaryForQA();
	Talk.Window->PressPrimaryForQA();
	TestEqual(TEXT("Прерванный разговор стоит на третьей реплике"), Talk.Window->GetIntroStepForQA(), 2);
	Talk.Close();
	Talk.Open(Elder, Player);
	TestTrue(TEXT("После ухода до задания вступление начинается заново"), Talk.Window->IsInIntroForQA());
	TestEqual(TEXT("И начинается с первой реплики"), Talk.Window->GetIntroStepForQA(), 0);

	// б) Игрок взял аптечку и задание и ушёл, не дослушав последнюю реплику.
	for (int32 Press = 0; Press < 4; ++Press)
	{
		Talk.Window->PressPrimaryForQA();
	}
	TestEqual(TEXT("После четырёх нажатий окно ещё открыто — идёт последняя реплика"), Talk.CloseRequests, 0);
	const FQuest* Wolves = Quests->FindQuest(WolvesId);
	TestTrue(TEXT("Задание про волков взято"), Wolves && Wolves->State == EQuestState::Active);
	TestEqual(TEXT("Аптечка выдана один раз"), ElderIntroTest::CountMedkits(Player), 1);
	Talk.Close();
	Talk.Open(Elder, Player);
	TestFalse(TEXT("Задание взято — вступление больше не играет"), Talk.Window->IsInIntroForQA());
	Talk.Window->PressPrimaryForQA(); // обычный разговор «задание ещё не выполнено»: кнопка ничего не ломает
	Wolves = Quests->FindQuest(WolvesId);
	TestTrue(TEXT("Задание про волков осталось взятым"), Wolves && Wolves->State == EQuestState::Active);
	TestEqual(TEXT("Вторую аптечку староста не дал"), ElderIntroTest::CountMedkits(Player), 1);
	Talk.Close();

	// в) Старое сохранение: вступление сыграно (в нём было шесть реплик), а задание почему-то не
	//    взято — староста предлагает его обычным разговором, и оно берётся одной кнопкой.
	Quests->RestoreQuests(TArray<FQuest>());
	if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		Save->bElderHookShown = true;
		Save->bElderFirstGiftGiven = true;
		TestTrue(TEXT("Старое сохранение записано"), Player->WriteSaveObject(Save));
	}
	Talk.Open(Elder, Player);
	TestFalse(TEXT("Старое сохранение: вступление не повторяется"), Talk.Window->IsInIntroForQA());
	Talk.Window->PressPrimaryForQA();
	Wolves = Quests->FindQuest(WolvesId);
	TestTrue(TEXT("Старое сохранение: задание про волков берётся обычной кнопкой"), Wolves && Wolves->State == EQuestState::Active);
	TestEqual(TEXT("Старое сохранение: аптечка повторно не выдаётся"), ElderIntroTest::CountMedkits(Player), 1);
	Talk.Close();

	ElderIntroTest::Destroy(World);
	ElderIntroTest::DeleteSlot(Slot);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
