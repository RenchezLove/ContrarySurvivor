// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты Б3 (задание издателя после ревизии: «загрузка сохранения при
// старте»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.SaveLoad; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE (полный слой C++ работает и в headless-мире — Build122TestWorld/этот
// файл создают транзиентный UWorld с BeginPlay):
//   - HasSaveGame() отличает файл слота БЕЗ реального прогресса (как пишет FlushPlayTime —
//     только накопитель времени, bHasData=false) от НАСТОЯЩЕГО сейва (SaveGame(), bHasData=true);
//   - «Продолжить» (LoadGameForContinue) восстанавливает статы/деньги/позицию, содержимое
//     рюкзака (включая тип расходника и стак) и экипированную броню, журнал квестов;
//   - «Новая игра» поверх существующего сейва (ResetToNewGame) честно стирает слот и приводит
//     живые статы к стартовым значениям новой игры;
//   - LoadGameForContinue() на пустом/отсутствующем сейве не роняет игру (просто false).
// Тесты пишут в ИЗОЛИРОВАННЫЕ слоты (ASaveTestPlayerCharacter::UseTestSaveSlot), НЕ в боевой
// 'ContrarySave' — иначе прогон затирал бы реальное сохранение на диске (ADR-058: у Рината в
// нём живой прогресс с телефона). Слот подчищается до и после каждого теста.
// НЕ покрывается headless: сам стартовый экран (клики Slate-кнопок «Продолжить»/«Новая игра») —
// нужен живой PIE/осмотр Рината, как и остальные UI-окна проекта.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Tests/SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AConsumableItem.h"
#include "AArmorTiers.h"
#include "AArmor.h"
#include "UInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags SaveLoadTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки Build121TestWorld/Build122TestWorld — она static
// в своих .cpp, дублируется по установленному в проекте паттерну «свой мир на файл теста»).
namespace SaveLoadTestWorld
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
			WorldSettings->NotifyBeginPlay(); // мир без GameMode сам begun-play не ставит
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

	// Расходник-«данные» с заданным типом и стаком (как его кладёт лут/выдача в рюкзак).
	static AConsumableItem* SpawnConsumable(UWorld* World, EConsumableType Type, int32 Stack = 1)
	{
		AConsumableItem* Item = Spawn<AConsumableItem>(World);
		if (Item)
		{
			Item->ConsumableType = Type;
			Item->ItemName = AConsumableItem::GetDefaultDisplayName(Type);
			Item->ItemDisplayText = AConsumableItem::GetDefaultDisplayText(Type);
			Item->StackCount = Stack;
			Item->SetActorHiddenInGame(true);
			Item->SetActorEnableCollision(false);
		}
		return Item;
	}

	// Именованный тестовый слот сейва — ЗАВЕДОМО не 'ContrarySave' (боевой слот). Каждый тест
	// подчищает свой слот DeleteGameInSlot и до, и после — повторный/параллельный прогон не мешает.
	static FString TestSlot(const TCHAR* Tag)
	{
		return FString::Printf(TEXT("Test_SaveLoad_%s"), Tag);
	}
}

// ===========================================================================
// 1. HasSaveGame() отличает файл слота БЕЗ реального прогресса (как его создаёт FlushPlayTime —
//    только накопитель игрового времени) от НАСТОЯЩЕГО сейва (SaveGame(), bHasData=true).
//    Находка лида по журналу с телефона: файл появляется уже через минуту игры, а «есть что
//    продолжать» проверялось раньше по голому факту существования файла — этот тест закрывает
//    ровно этот баг.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveLoadHasRealDataTest,
	"ContrarySurvivor.SaveLoad.HasSaveGame.RealDataVsPlaytimeOnly", SaveLoadTestFlags)

bool FSaveLoadHasRealDataTest::RunTest(const FString& Parameters)
{
	UWorld* World = SaveLoadTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	const FString Slot = SaveLoadTestWorld::TestSlot(TEXT("HasSave"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* Player = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		if (Player)
		{
			Player->UseTestSaveSlot(Slot);
			TestFalse(TEXT("Пустой слот: сейва с прогрессом нет"), Player->HasSaveGame());

			// Мимикрия FlushPlayTime: свежий объект слота с накопителем времени, bHasData НЕ ставится.
			if (UContrarySaveGame* PlaytimeOnly = Player->LoadOrCreateSaveObject())
			{
				PlaytimeOnly->TotalPlayTimeSeconds = 90.0f;
				TestTrue(TEXT("Запись накопителя времени прошла"), Player->WriteSaveObject(PlaytimeOnly));
			}
			else
			{
				bOk = false;
			}

			TestTrue(TEXT("Файл слота теперь существует на диске"),
				UGameplayStatics::DoesSaveGameExist(Slot, 0));
			TestFalse(TEXT("Но HasSaveGame() = false — накопитель времени не реальный прогресс"),
				Player->HasSaveGame());

			// Настоящее сохранение (как автосейв костра/пере-сейв смерти) ставит bHasData=true.
			TestTrue(TEXT("SaveGame() прошёл"), Player->SaveGame());
			TestTrue(TEXT("После SaveGame() HasSaveGame() = true"), Player->HasSaveGame());
		}
		else
		{
			bOk = false;
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveLoadTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 2. «Продолжить» (LoadGameForContinue) восстанавливает статы, деньги и позицию — персонаж А
//    сохраняет своё состояние «прошлой сессии», персонаж Б («новый запуск») грузит его целиком.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveLoadContinueRestoresStatsTest,
	"ContrarySurvivor.SaveLoad.Continue.RestoresStatsMoneyPosition", SaveLoadTestFlags)

bool FSaveLoadContinueRestoresStatsTest::RunTest(const FString& Parameters)
{
	UWorld* World = SaveLoadTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	const FString Slot = SaveLoadTestWorld::TestSlot(TEXT("ContinueStats"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		// Персонаж А «прошлой сессии»: свои статы/деньги/позиция, сохраняет и «выходит».
		ASaveTestPlayerCharacter* PlayerA = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(
			World, FVector(500.f, 0.f, 100.f));
		UStatsComponent* StatsA = PlayerA ? PlayerA->GetStats() : nullptr;
		if (PlayerA && StatsA)
		{
			PlayerA->UseTestSaveSlot(Slot);
			StatsA->RestoreState(/*Health=*/42.0f, /*Hunger=*/33.0f, /*Thirst=*/77.0f, /*Money=*/321.0f);
			TestTrue(TEXT("SaveGame() записал прогресс персонажа А"), PlayerA->SaveGame());
		}
		else
		{
			bOk = false;
		}

		// Персонаж Б «нового запуска»: BeginPlay уже отработал (дефолтные статы), затем «Продолжить».
		ASaveTestPlayerCharacter* PlayerB = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		UStatsComponent* StatsB = PlayerB ? PlayerB->GetStats() : nullptr;
		if (PlayerB && StatsB)
		{
			PlayerB->UseTestSaveSlot(Slot);
			TestTrue(TEXT("HasSaveGame() видит реальный прогресс персонажа А"), PlayerB->HasSaveGame());
			TestTrue(TEXT("LoadGameForContinue() прошёл"), PlayerB->LoadGameForContinue());

			TestEqual(TEXT("Здоровье восстановлено"), StatsB->GetHealth(), 42.0f);
			TestEqual(TEXT("Голод восстановлен"), StatsB->GetHunger(), 33.0f);
			TestEqual(TEXT("Жажда восстановлена"), StatsB->GetThirst(), 77.0f);
			TestEqual(TEXT("Деньги восстановлены"), StatsB->GetMoney(), 321.0f);
			TestEqual(TEXT("Позиция восстановлена (X)"), PlayerB->GetActorLocation().X, 500.0);
		}
		else
		{
			bOk = false;
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveLoadTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 3. «Продолжить» восстанавливает содержимое рюкзака (стак расходника С ПРАВИЛЬНЫМ ТИПОМ —
//    класс один на еду/воду/аптечку, тип на экземпляре) и экипированную броню (слот + защита).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveLoadContinueRestoresBackpackArmorTest,
	"ContrarySurvivor.SaveLoad.Continue.RestoresBackpackAndArmor", SaveLoadTestFlags)

bool FSaveLoadContinueRestoresBackpackArmorTest::RunTest(const FString& Parameters)
{
	UWorld* World = SaveLoadTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	const FString Slot = SaveLoadTestWorld::TestSlot(TEXT("ContinueBackpack"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* PlayerA = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		UInventoryComponent* InvA = PlayerA ? PlayerA->GetInventory() : nullptr;
		if (PlayerA && InvA)
		{
			PlayerA->UseTestSaveSlot(Slot);

			// Стак воды x3 в рюкзаке (не экипирована — тип должен пережить круг сохранения).
			AConsumableItem* Water = SaveLoadTestWorld::SpawnConsumable(World, EConsumableType::Water, /*Stack=*/3);
			TestNotNull(TEXT("Вода заспавнена"), Water);
			if (Water)
			{
				InvA->AddItem(Water);
			}

			// Надетый шлем Т1.
			AHeadArmorT1* Helmet = SaveLoadTestWorld::Spawn<AHeadArmorT1>(World);
			TestNotNull(TEXT("Шлем заспавнен"), Helmet);
			if (Helmet)
			{
				InvA->AddItem(Helmet);
				PlayerA->EquipArmor(Helmet);
				InvA->SetItemEquipped(Helmet, true);
			}

			TestTrue(TEXT("SaveGame() записал рюкзак и броню"), PlayerA->SaveGame());
		}
		else
		{
			bOk = false;
		}

		ASaveTestPlayerCharacter* PlayerB = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		UInventoryComponent* InvB = PlayerB ? PlayerB->GetInventory() : nullptr;
		if (PlayerB && InvB)
		{
			PlayerB->UseTestSaveSlot(Slot);
			TestTrue(TEXT("LoadGameForContinue() прошёл"), PlayerB->LoadGameForContinue());

			AConsumableItem* RestoredWater = nullptr;
			for (AMasterInventoryItem* Item : InvB->GetInventoryItems())
			{
				if (AConsumableItem* Cons = Cast<AConsumableItem>(Item))
				{
					RestoredWater = Cons;
					break;
				}
			}
			TestNotNull(TEXT("Вода нашлась в восстановленном рюкзаке"), RestoredWater);
			if (RestoredWater)
			{
				TestTrue(TEXT("Тип расходника восстановлен (Water, не дефолт Food)"),
					RestoredWater->ConsumableType == EConsumableType::Water);
				TestEqual(TEXT("Стак воды восстановлен x3"), RestoredWater->GetStackCount(), 3);
				TestFalse(TEXT("Вода не экипирована"), InvB->IsItemEquipped(RestoredWater));
			}

			AArmor* RestoredHead = PlayerB->GetEquippedArmor(EArmorSlot::Head);
			TestNotNull(TEXT("Шлем экипирован после восстановления"), RestoredHead);
			if (RestoredHead)
			{
				TestTrue(TEXT("Шлем помечен экипированным в рюкзаке"), InvB->IsItemEquipped(RestoredHead));
				TestTrue(TEXT("Суммарная защита выросла"), PlayerB->GetTotalArmorProtection() > 0.0f);
			}

			TestEqual(TEXT("В рюкзаке ровно 2 записи (вода + шлем)"), InvB->GetInventoryItems().Num(), 2);
		}
		else
		{
			bOk = false;
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveLoadTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 4. «Продолжить» восстанавливает журнал квестов (состояние Active + прогресс), а не только
//    статы/рюкзак — иначе принятый, но не сданный квест «слетал» бы при каждом перезапуске.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveLoadContinueRestoresQuestsTest,
	"ContrarySurvivor.SaveLoad.Continue.RestoresQuestState", SaveLoadTestFlags)

bool FSaveLoadContinueRestoresQuestsTest::RunTest(const FString& Parameters)
{
	UWorld* World = SaveLoadTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	const FString Slot = SaveLoadTestWorld::TestSlot(TEXT("ContinueQuests"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* PlayerA = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		UQuestComponent* QuestsA = PlayerA ? PlayerA->GetQuests() : nullptr;
		if (PlayerA && QuestsA)
		{
			PlayerA->UseTestSaveSlot(Slot);

			FQuest Quest;
			Quest.QuestId = TEXT("TestKillWolves");
			Quest.Title = FText::FromString(TEXT("Тестовый квест"));
			Quest.Type = EQuestType::Kill;
			Quest.KillTargetTag = TEXT("Wolf");
			Quest.TargetCount = 5;
			Quest.RewardMoney = 150.0f;

			QuestsA->OfferQuest(Quest);
			TestTrue(TEXT("Квест принят"), QuestsA->AcceptQuest(Quest.QuestId));
			QuestsA->NotifyKill(TEXT("Wolf"));
			QuestsA->NotifyKill(TEXT("Wolf"));

			const FQuest* Progressed = QuestsA->FindQuest(Quest.QuestId);
			TestNotNull(TEXT("Квест найден в журнале А"), Progressed);
			if (Progressed)
			{
				TestEqual(TEXT("Прогресс убийств 2/5"), Progressed->Progress, 2);
				TestTrue(TEXT("Квест ещё Active (не Completed)"), Progressed->State == EQuestState::Active);
			}

			TestTrue(TEXT("SaveGame() записал журнал квестов"), PlayerA->SaveGame());
		}
		else
		{
			bOk = false;
		}

		ASaveTestPlayerCharacter* PlayerB = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		UQuestComponent* QuestsB = PlayerB ? PlayerB->GetQuests() : nullptr;
		if (PlayerB && QuestsB)
		{
			PlayerB->UseTestSaveSlot(Slot);
			TestEqual(TEXT("До восстановления журнал Б пуст"), QuestsB->GetQuests().Num(), 0);
			TestTrue(TEXT("LoadGameForContinue() прошёл"), PlayerB->LoadGameForContinue());

			const FQuest* Restored = QuestsB->FindQuest(TEXT("TestKillWolves"));
			TestNotNull(TEXT("Квест восстановлен в журнале Б"), Restored);
			if (Restored)
			{
				TestTrue(TEXT("Состояние Active сохранилось"), Restored->State == EQuestState::Active);
				TestEqual(TEXT("Прогресс 2/5 сохранился"), Restored->Progress, 2);
				TestEqual(TEXT("Заголовок квеста сохранился (не пустой плейсхолдер)"),
					Restored->Title.ToString(), FString(TEXT("Тестовый квест")));
			}
		}
		else
		{
			bOk = false;
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveLoadTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 5. «Новая игра» поверх существующего сейва (ResetToNewGame): слот честно стирается (иначе
//    получилась бы каша из старых и новых данных, ТЗ издателя п.4), а живые статы приводятся
//    к стартовым значениям новой игры — ровно тем же, что и BeginPlay даёт игроку без сейва.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveLoadNewGameWipesSaveTest,
	"ContrarySurvivor.SaveLoad.NewGame.WipesSaveAndResetsStats", SaveLoadTestFlags)

bool FSaveLoadNewGameWipesSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = SaveLoadTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	const FString Slot = SaveLoadTestWorld::TestSlot(TEXT("NewGameWipe"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		// Персонаж со «старым» прогрессом — как если бы игрок уже сохранился однажды.
		ASaveTestPlayerCharacter* PlayerA = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		UStatsComponent* StatsA = PlayerA ? PlayerA->GetStats() : nullptr;
		if (PlayerA && StatsA)
		{
			PlayerA->UseTestSaveSlot(Slot);
			StatsA->RestoreState(/*Health=*/90.0f, /*Hunger=*/80.0f, /*Thirst=*/70.0f, /*Money=*/999.0f);
			TestTrue(TEXT("SaveGame() записал старый прогресс"), PlayerA->SaveGame());
		}
		else
		{
			bOk = false;
		}

		// Новый персонаж «текущего запуска» — HasSaveGame() был бы true (сейв реальный), но
		// игрок на стартовом экране нажал «Новая игра».
		ASaveTestPlayerCharacter* PlayerB = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		UStatsComponent* StatsB = PlayerB ? PlayerB->GetStats() : nullptr;
		if (PlayerB && StatsB)
		{
			PlayerB->UseTestSaveSlot(Slot);
			TestTrue(TEXT("Перед сбросом сейв виден"), PlayerB->HasSaveGame());

			PlayerB->ResetToNewGame();

			TestFalse(TEXT("После «Новая игра» сейв стёрт"), PlayerB->HasSaveGame());
			TestFalse(TEXT("Файла слота на диске больше нет"), UGameplayStatics::DoesSaveGameExist(Slot, 0));

			// Стартовые значения новой игры (см. APlayerCharacter::NewGameHealthFraction/
			// NewGameSurvivalFraction/StartingMoney — половина статов, GDD §7.6 деньги).
			TestEqual(TEXT("Здоровье приведено к половине максимума"),
				StatsB->GetHealth(), StatsB->GetMaxHealth() * 0.5f);
			TestEqual(TEXT("Голод приведён к половине"),
				StatsB->GetHunger(), StatsB->GetSurvivalMax() * 0.5f);
			TestEqual(TEXT("Жажда приведена к половине"),
				StatsB->GetThirst(), StatsB->GetSurvivalMax() * 0.5f);
			TestEqual(TEXT("Деньги — стартовые новой игры (50)"), StatsB->GetMoney(), 50.0f);
		}
		else
		{
			bOk = false;
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveLoadTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 6. Пустой/отсутствующий сейв: HasSaveGame() = false, LoadGameForContinue() возвращает false
//    и НЕ роняет игру (это путь «нет сейва -> сразу новая игра», без стартового экрана).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveLoadNoSaveMeansImmediateNewGameTest,
	"ContrarySurvivor.SaveLoad.NoSave.ContinueFailsSafely", SaveLoadTestFlags)

bool FSaveLoadNoSaveMeansImmediateNewGameTest::RunTest(const FString& Parameters)
{
	UWorld* World = SaveLoadTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	const FString Slot = SaveLoadTestWorld::TestSlot(TEXT("NoSave"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* Player = SaveLoadTestWorld::Spawn<ASaveTestPlayerCharacter>(World);
		if (Player)
		{
			Player->UseTestSaveSlot(Slot);
			TestFalse(TEXT("Сейва нет вовсе"), Player->HasSaveGame());
			TestFalse(TEXT("LoadGameForContinue() на пустом слоте возвращает false"),
				Player->LoadGameForContinue());
			TestFalse(TEXT("LoadGame() (респаун) на пустом слоте тоже false"), Player->LoadGame());
		}
		else
		{
			bOk = false;
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveLoadTestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
