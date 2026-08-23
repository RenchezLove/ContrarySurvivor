// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты ГРУППОВОГО ОБЫСКА ТЕЛ (задание издателя 11.08.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.GroupSearch; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается (пункты задания дословно в комментариях тестов):
//   * п.3.1 — куча из четырёх тел в радиусе забирается ОДНИМ заходом, тело за радиусом не
//     трогается; контейнер-«отдельное хранилище» (bStandaloneStash, ADR-076 п.2 — граница
//     «ящики обыскиваются отдельно»; обычные мешки с 22.08 в группу ВХОДЯТ) не попадает;
//   * п.3.1 — подсказка называет число тел: «Обыскать (4) — E», при одном теле числа нет;
//   * п.3.2 — обысканное тело уходит в землю и исчезает (математика погружения + живой
//     тик мира: тело реально опускается и удаляется);
//   * п.3.3 — сводка одной строкой «Получено: …», а не сообщение на каждый предмет;
//   * п.3.4 — когда рюкзак предмет не принял, тело остаётся ПОЛНЫМ и игроку об этом
//     говорят словами. Настоящего отказа в игре сегодня нет (вместимости у рюкзака не
//     существует), поэтому ветка проверяется суррогатом: окно без игрока = принимать
//     некому. Это ЯВНОЕ допущение теста, а не имитация вместимости;
//   * отчёт Рината 23.08 п.5 (растворение) — математика непрозрачности, живой цикл
//     растворения на трупе (мгновенный сдвиг -> пауза -> таяние -> удаление) и откат
//     на уход в землю, когда материал растворения не загрузился.
//
// НЕ покрывается headless: сам вид окна и всплывашки (нужен живой Slate), поведение
// рэгдолла при погружении (у тела в проекте нет физ.ассета — см. отчёт cpp-dev).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h" // тела в тестах — НАСТОЯЩИЕ трупы волков
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/UI/CorpseLootWidget.h"
#include "AConsumableItem.h"
#include "AMasterInventoryItem.h"
#include "UInventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"  // растворение: проверка подмены материала меша
#include "Materials/Material.h"                // растворение: транзиентный материал-родитель
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h" // FDamageEvent (смертельный урон волку)
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags GroupSearchTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// ---------------------------------------------------------------------------
// Транзиентный игровой мир (тот же каркас, что Build121/Adr063 — обвязки static
// в своих .cpp, поэтому копия, а не общий заголовок).
// ---------------------------------------------------------------------------
namespace GroupSearchTestWorld
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

	// Кадр тестового мира. Инкремент GFrameCounter обязателен: тик-функции ставятся в
	// очередь не чаще одного раза на значение счётчика (TickTaskManager.cpp), а весь
	// RunTest идёт внутри одного кадра движка — без этого компонент тикнул бы один раз.
	static void TickWorld(UWorld* World, float DeltaSeconds)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, DeltaSeconds);
	}

	// Прогнать РОВНО столько игрового времени, сколько попросили. ⛔ Один кадр даёт максимум
	// 0.4 с: мир режет шаг по MaxUndilatedFrameTime настроек мира (LevelTick.cpp:1352 ->
	// AWorldSettings::FixupDeltaSeconds), поэтому «тикнуть на 1.2 с» одним вызовом нельзя —
	// поймано прогоном 13.08, тело не успевало опуститься. Идём шагами по 0.2 с.
	static void AdvanceWorld(UWorld* World, float Seconds)
	{
		const float Step = 0.2f;
		for (float Passed = 0.0f; Passed < Seconds - KINDA_SMALL_NUMBER; Passed += Step)
		{
			TickWorld(World, FMath::Min(Step, Seconds - Passed));
		}
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

	static AConsumableItem* SpawnConsumable(UWorld* World, EConsumableType Type, int32 Stack)
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

	// ТЕЛО = настоящий труп волка, полученный штатным путём: смертельный урон -> HandleDeath
	// -> DropLoot кладёт шкуру и деньги В ТЕЛО и ставит его в реестр обыскиваемых.
	// Голый AActor тут не годится: у него НЕТ корневого компонента, поэтому положение всегда
	// читается как начало координат — расстояния до тел мерить нечем (поймано прогоном
	// 13.08, первая версия этих тестов). Заодно контейнер приезжает так же, как в игре:
	// созданным в конструкторе персонажа, а не прицепленным на ходу.
	static UCorpseLootComponent* SpawnBody(UWorld* World, const FVector& Loc)
	{
		AWolfCharacter* Wolf = Spawn<AWolfCharacter>(World, Loc);
		if (!Wolf)
		{
			return nullptr;
		}
		Wolf->TakeDamage(1000.0f, FDamageEvent(), nullptr, nullptr);
		UCorpseLootComponent* Body = Wolf->FindComponentByClass<UCorpseLootComponent>();
		if (Body)
		{
			// Тесты этого файла доказывают путь УХОДА В ЗЕМЛЮ (п.3.2 издателя) — растворение
			// (Ринат 23.08 п.5, включено по умолчанию) выключаем явно, иначе с появлением
			// материала M_CorpseDissolve в контенте тела начали бы растворяться и проверки
			// IsSinking лгали бы. У растворения свои тесты ниже (Dissolve*).
			Body->bDissolveWhenSearched = false;
		}
		return Body;
	}

	// Сколько штук лежит в рюкзаке всего (стаки сливаются — считаем штуки, а не записи).
	static int32 CountItemsInBackpack(const UInventoryComponent* Inventory)
	{
		int32 Count = 0;
		if (!Inventory)
		{
			return Count;
		}
		for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
		{
			if (IsValid(Item))
			{
				Count += Item->GetStackCount();
			}
		}
		return Count;
	}
}

// ===========================================================================
// 1. п.3.1: «Игрок нажимает "Обыскать" один раз и получает содержимое всех необысканных
//    тел в радиусе, а не одного ближайшего». Куча из четырёх тел — один заход.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupSearchFourBodiesTest,
	"ContrarySurvivor.GroupSearch.FourBodiesTakenAtOnce", GroupSearchTestFlags)

bool FGroupSearchFourBodiesTest::RunTest(const FString& Parameters)
{
	UWorld* World = GroupSearchTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		APlayerCharacter* Player = GroupSearchTestWorld::Spawn<APlayerCharacter>(World);
		UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
		UStatsComponent* Stats = Player ? Player->FindComponentByClass<UStatsComponent>() : nullptr;

		// Куча: тело-якорь и ещё три в пределах 600 см от него (радиус группы по умолчанию).
		UCorpseLootComponent* Anchor = GroupSearchTestWorld::SpawnBody(World, FVector(1000.f, 0.f, 100.f));
		UCorpseLootComponent* Near1 = GroupSearchTestWorld::SpawnBody(World, FVector(1200.f, 0.f, 100.f));
		UCorpseLootComponent* Near2 = GroupSearchTestWorld::SpawnBody(World, FVector(1000.f, 400.f, 100.f));
		UCorpseLootComponent* Near3 = GroupSearchTestWorld::SpawnBody(World, FVector(1500.f, 0.f, 100.f));

		if (!Player || !Inventory || !Stats || !Anchor || !Near1 || !Near2 || !Near3)
		{
			AddError(TEXT("Игрок или тела не создались в тестовом мире"));
			GroupSearchTestWorld::Destroy(World);
			return false;
		}

		const TArray<UCorpseLootComponent*> Group =
			UCorpseLootComponent::CollectSearchableGroup(Anchor->GetOwner(), 600.0f);
		TestEqual(TEXT("В группу попали все четыре тела"), Group.Num(), 4);
		if (Group.Num() > 0)
		{
			TestTrue(TEXT("Первым в группе идёт подсвеченное тело"), Group[0] == Anchor);
		}

		// Деньги у волка случайные (5..15 на тело) — эталон берём с самих тел, а не выдумываем.
		const float MoneyBefore = Stats->GetMoney();
		const float MoneyInGroup = Anchor->GetMoney() + Near1->GetMoney()
			+ Near2->GetMoney() + Near3->GetMoney();
		const int32 ItemsBefore = GroupSearchTestWorld::CountItemsInBackpack(Inventory);

		UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
		if (!TestNotNull(TEXT("Окно обыска создано"), Window))
		{
			GroupSearchTestWorld::Destroy(World);
			return false;
		}
		Window->InitCorpseLootGroup(Group, Player);

		const FCorpseLootTakenSummary Summary = Window->TakeAllFromGroup();

		// ГЛАВНОЕ: одно нажатие «Забрать всё» опустошило ВСЮ кучу.
		TestFalse(TEXT("Тело-якорь пусто"), Anchor->HasLoot());
		TestFalse(TEXT("Второе тело пусто"), Near1->HasLoot());
		TestFalse(TEXT("Третье тело пусто"), Near2->HasLoot());
		TestFalse(TEXT("Четвёртое тело пусто"), Near3->HasLoot());

		TestEqual(TEXT("Деньги всех четырёх тел ушли игроку"), Stats->GetMoney(), MoneyBefore + MoneyInGroup);
		TestEqual(TEXT("Все четыре шкуры лежат в рюкзаке"),
			GroupSearchTestWorld::CountItemsInBackpack(Inventory), ItemsBefore + 4);

		// Сводка складывает одинаковые названия в одну запись (п.3.3).
		TestEqual(TEXT("В сводке одна запись предметов"), Summary.Items.Num(), 1);
		if (Summary.Items.Num() == 1)
		{
			TestEqual(TEXT("В записи названа шкура волка"),
				Summary.Items[0].Key.ToString(), TEXT("Шкура волка"));
			TestEqual(TEXT("Штук в записи — четыре"), Summary.Items[0].Value, 4);
		}
		TestEqual(TEXT("В сводке деньги всей группы"), Summary.Money, FMath::RoundToInt32(MoneyInGroup));
		TestFalse(TEXT("Рюкзак принял всё"), Summary.bBackpackRefused);

		// п.3.2: опустевшие тела сразу пошли в землю (исчезновение проверяет отдельный тест).
		TestTrue(TEXT("Обысканное тело начало уходить в землю"), Anchor->IsSinking());
		TestTrue(TEXT("Уходит в землю и последнее тело кучи"), Near3->IsSinking());
	}

	GroupSearchTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 2. п.3.1: тело ЗА радиусом группы одним нажатием не забирается («в радиусе»).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupSearchRadiusTest,
	"ContrarySurvivor.GroupSearch.BodyOutsideRadiusUntouched", GroupSearchTestFlags)

bool FGroupSearchRadiusTest::RunTest(const FString& Parameters)
{
	UWorld* World = GroupSearchTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		APlayerCharacter* Player = GroupSearchTestWorld::Spawn<APlayerCharacter>(World);
		UCorpseLootComponent* Anchor = GroupSearchTestWorld::SpawnBody(World, FVector(0.f, 0.f, 100.f));
		UCorpseLootComponent* Inside = GroupSearchTestWorld::SpawnBody(World, FVector(590.f, 0.f, 100.f));
		UCorpseLootComponent* Outside = GroupSearchTestWorld::SpawnBody(World, FVector(900.f, 0.f, 100.f));

		if (!Player || !Anchor || !Inside || !Outside)
		{
			AddError(TEXT("Игрок или тела не создались в тестовом мире"));
			GroupSearchTestWorld::Destroy(World);
			return false;
		}

		const TArray<UCorpseLootComponent*> Group =
			UCorpseLootComponent::CollectSearchableGroup(Anchor->GetOwner(), 600.0f);
		TestEqual(TEXT("В группе только тела ближе 600 см"), Group.Num(), 2);
		TestFalse(TEXT("Дальнее тело в группу не попало"), Group.Contains(Outside));

		UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
		if (Window)
		{
			Window->InitCorpseLootGroup(Group, Player);
			Window->TakeAllFromGroup();
		}

		TestFalse(TEXT("Ближнее тело обыскано"), Inside->HasLoot());
		TestTrue(TEXT("Дальнее тело осталось нетронутым"), Outside->HasLoot());
		TestTrue(TEXT("Деньги дальнего тела на месте"), Outside->GetMoney() > 0.0f);
		TestEqual(TEXT("Шкура дальнего тела на месте"), Outside->GetLootItems().Num(), 1);
		TestFalse(TEXT("Дальнее тело в землю не уходит"), Outside->IsSinking());
	}

	GroupSearchTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Ящики и схроны обыскиваются отдельно от кучи тел. История: п.3.1 издателя держал вне
//    группы ЛЮБОЙ мешок-пикап, но ADR-076 п.2 (решение лида 22.08, кейс Рината «в лагере
//    два мешка») завёл мешки В группу — граница «отдельно» теперь проходит по галочке
//    «Отдельное хранилище» (bStandaloneStash; так живут база Report1 п.11 и машина п.9).
//    Тест держит текущую границу: хранилище в группу не попадает и содержимое сохраняет.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupSearchIgnoresBoxTest,
	"ContrarySurvivor.GroupSearch.BoxStaysOutOfGroup", GroupSearchTestFlags)

bool FGroupSearchIgnoresBoxTest::RunTest(const FString& Parameters)
{
	UWorld* World = GroupSearchTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		APlayerCharacter* Player = GroupSearchTestWorld::Spawn<APlayerCharacter>(World);
		UCorpseLootComponent* Body = GroupSearchTestWorld::SpawnBody(World, FVector(0.f, 0.f, 100.f));

		// Ящик-«отдельное хранилище»: контейнер на пикапе с галочкой bStandaloneStash —
		// так сегодня устроены стационарные хранилища (база/машина).
		APickup* Box = GroupSearchTestWorld::Spawn<APickup>(World, FVector(200.f, 0.f, 100.f));
		AConsumableItem* BoxFood = GroupSearchTestWorld::SpawnConsumable(World, EConsumableType::Food, 1);
		if (!Player || !Body || !Box || !BoxFood)
		{
			AddError(TEXT("Игрок, тело или ящик не создались в тестовом мире"));
			GroupSearchTestWorld::Destroy(World);
			return false;
		}
		Box->InitLootBag(TArray<AMasterInventoryItem*>({ BoxFood }), /*Money=*/25.0f);

		UCorpseLootComponent* BoxContainer = Box->GetLootContainer();
		if (!TestNotNull(TEXT("У ящика есть контейнер обыска"), BoxContainer))
		{
			GroupSearchTestWorld::Destroy(World);
			return false;
		}
		BoxContainer->bStandaloneStash = true; // граница «обыскивается отдельно» (ADR-076 п.2)

		const TArray<UCorpseLootComponent*> Group =
			UCorpseLootComponent::CollectSearchableGroup(Body->GetOwner(), 600.0f);
		TestEqual(TEXT("В группе только тело"), Group.Num(), 1);
		TestFalse(TEXT("Хранилище в группу тел не попало"), Group.Contains(BoxContainer));

		UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
		if (Window)
		{
			Window->InitCorpseLootGroup(Group, Player);
			Window->TakeAllFromGroup();
		}

		TestFalse(TEXT("Тело обыскано"), Body->HasLoot());
		TestTrue(TEXT("Содержимое ящика не тронуто"), BoxContainer->HasLoot());
		TestEqual(TEXT("Деньги ящика на месте"), BoxContainer->GetMoney(), 25.0f);
		TestFalse(TEXT("Ящик в землю не уходит (это поведение ТЕЛА)"), BoxContainer->IsSinking());
	}

	GroupSearchTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 4. п.3.1 дословно: «Надпись показывает количество: Обыскать (4). Когда тело одно —
//    просто Обыскать, без числа в скобках».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupSearchPromptCountTest,
	"ContrarySurvivor.GroupSearch.PromptShowsBodyCount", GroupSearchTestFlags)

bool FGroupSearchPromptCountTest::RunTest(const FString& Parameters)
{
	// Контроллер создаётся NewObject (не Spawn): нужны только поля и чистая склейка —
	// тот же приём, что в тестах подсказки Build 1.2.2.
	AContrarySurvivorPlayerController* PC =
		NewObject<AContrarySurvivorPlayerController>(GetTransientPackage());
	if (!TestNotNull(TEXT("Контроллер создан"), PC))
	{
		return false;
	}

	const FText Action = PC->GetInteractActionText(EInteractKind::Corpse);
	const FText How = PC->GetInteractKeyName();
	const FText GroupFormat = PC->InteractPromptCorpseGroupFormat;

	TestEqual(TEXT("Четыре тела — число в скобках"),
		AContrarySurvivorPlayerController::FormatCorpseGroupAction(GroupFormat, Action, 4).ToString(),
		TEXT("Обыскать (4)"));
	TestEqual(TEXT("Одно тело — подпись как была"),
		AContrarySurvivorPlayerController::FormatCorpseGroupAction(GroupFormat, Action, 1).ToString(),
		TEXT("Обыскать"));
	TestEqual(TEXT("Тел нет — подпись как была"),
		AContrarySurvivorPlayerController::FormatCorpseGroupAction(GroupFormat, Action, 0).ToString(),
		TEXT("Обыскать"));

	// Подсказка целиком — той же склейкой, что в живой игре.
	TestEqual(TEXT("Подсказка у кучи тел на компьютере"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(PC->GetInteractPromptFormat(),
			AContrarySurvivorPlayerController::FormatCorpseGroupAction(GroupFormat, Action, 4), How).ToString(),
		TEXT("Обыскать (4) — E"));
	TestEqual(TEXT("Подсказка у одного тела не изменилась"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(PC->GetInteractPromptFormat(),
			AContrarySurvivorPlayerController::FormatCorpseGroupAction(GroupFormat, Action, 1), How).ToString(),
		TEXT("Обыскать — E"));

	// Число в подписи — настройка, а не зашитая строка.
	PC->InteractPromptCorpseGroupFormat = FText::FromString(TEXT("{Action} х{Count}"));
	TestEqual(TEXT("Подпись собирается из настройки"),
		AContrarySurvivorPlayerController::FormatCorpseGroupAction(
			PC->InteractPromptCorpseGroupFormat, Action, 7).ToString(),
		TEXT("Обыскать х7"));

	return true;
}

// ===========================================================================
// 5. п.3.2: «Обысканное тело меняется на вид» — пустое тело уходит в землю и исчезает.
//    Проверяем и чистую математику, и живой мир (тело реально опускается и удаляется).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupSearchSinkTest,
	"ContrarySurvivor.GroupSearch.SearchedBodySinksAndDisappears", GroupSearchTestFlags)

bool FGroupSearchSinkTest::RunTest(const FString& Parameters)
{
	// Математика погружения: 1 с пауза, 2 с спуск, 150 см глубина (дефолты компонента).
	TestEqual(TEXT("До паузы тело стоит на месте"),
		UCorpseLootComponent::GetSinkDepthAtTime(0.5f, 1.0f, 2.0f, 150.0f), 0.0f);
	TestEqual(TEXT("Ровно в конце паузы — ещё ноль"),
		UCorpseLootComponent::GetSinkDepthAtTime(1.0f, 1.0f, 2.0f, 150.0f), 0.0f);
	TestEqual(TEXT("Середина спуска — половина глубины"),
		UCorpseLootComponent::GetSinkDepthAtTime(2.0f, 1.0f, 2.0f, 150.0f), 75.0f);
	TestEqual(TEXT("Конец спуска — вся глубина"),
		UCorpseLootComponent::GetSinkDepthAtTime(3.0f, 1.0f, 2.0f, 150.0f), 150.0f);
	TestEqual(TEXT("Дальше глубина не растёт"),
		UCorpseLootComponent::GetSinkDepthAtTime(9.0f, 1.0f, 2.0f, 150.0f), 150.0f);

	UWorld* World = GroupSearchTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		UCorpseLootComponent* Body = GroupSearchTestWorld::SpawnBody(World, FVector(0.f, 0.f, 500.f));
		AActor* BodyActor = Body ? Body->GetOwner() : nullptr;
		if (!Body || !BodyActor)
		{
			AddError(TEXT("Тело не создалось в тестовом мире"));
			GroupSearchTestWorld::Destroy(World);
			return false;
		}

		const float StartZ = BodyActor->GetActorLocation().Z;

		// Пока в теле что-то есть — оно лежит.
		Body->TakeMoney();
		TestFalse(TEXT("Тело с предметом в землю не уходит"), Body->IsSinking());

		// Забрали последнее — тело обыскано.
		const TArray<AMasterInventoryItem*> Items = Body->GetLootItems();
		for (AMasterInventoryItem* Item : Items)
		{
			Body->TakeItem(Item);
		}
		TestTrue(TEXT("Пустое тело пошло в землю"), Body->IsSinking());

		// Пауза (1 с): тело ещё на месте.
		GroupSearchTestWorld::AdvanceWorld(World, 0.8f);
		TestTrue(TEXT("В паузе тело не двигалось"),
			FMath::IsNearlyEqual(BodyActor->GetActorLocation().Z, StartZ, 0.01f));

		// Середина спуска (всего 2 с = половина глубины): тело ниже, но ещё существует.
		GroupSearchTestWorld::AdvanceWorld(World, 1.2f);
		TestTrue(TEXT("Тело опускается"), BodyActor->GetActorLocation().Z < StartZ - 30.0f);
		TestTrue(TEXT("В середине спуска тело ещё в мире"), IsValid(BodyActor));

		// Конец (всего 3.2 с > пауза 1 с + спуск 2 с): тело исчезло.
		GroupSearchTestWorld::AdvanceWorld(World, 1.2f);
		TestFalse(TEXT("Обысканное тело исчезло"), IsValid(BodyActor));
	}

	GroupSearchTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 6. п.3.3: «показать, что именно упало в рюкзак — сводкой, а не четырьмя отдельными
//    сообщениями подряд». Проверяем сборку ОДНОЙ строки и русский счёт монет.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupSearchSummaryLineTest,
	"ContrarySurvivor.GroupSearch.SummaryIsOneLine", GroupSearchTestFlags)

bool FGroupSearchSummaryLineTest::RunTest(const FString& Parameters)
{
	UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
	if (!TestNotNull(TEXT("Окно обыска создано"), Window))
	{
		return false;
	}

	FCorpseLootTakenSummary Summary;
	Summary.AddItem(FText::FromString(TEXT("Шкура волка")), 3);
	Summary.AddItem(FText::FromString(TEXT("Шкура волка")), 1); // одинаковые складываются
	Summary.Money = 12;

	const FString Line = Window->BuildTakenSummaryText(Summary).ToString();
	TestEqual(TEXT("Сводка одной строкой"), Line, TEXT("Получено: Шкура волка x4, 12 монет"));
	TestFalse(TEXT("Переносов строк в сводке нет"), Line.Contains(TEXT("\n")));

	// Одна штука — без «x1».
	FCorpseLootTakenSummary Single;
	Single.AddItem(FText::FromString(TEXT("Аптечка")), 1);
	TestEqual(TEXT("Одна штука — просто название"),
		Window->BuildTakenSummaryText(Single).ToString(), TEXT("Получено: Аптечка"));

	// Русский счёт монет.
	TestEqual(TEXT("1 монета"),
		UCorpseLootWidget::PickMoneyWord(1, Window->MoneyWordOne, Window->MoneyWordFew, Window->MoneyWordMany).ToString(),
		TEXT("монета"));
	TestEqual(TEXT("3 монеты"),
		UCorpseLootWidget::PickMoneyWord(3, Window->MoneyWordOne, Window->MoneyWordFew, Window->MoneyWordMany).ToString(),
		TEXT("монеты"));
	TestEqual(TEXT("12 монет"),
		UCorpseLootWidget::PickMoneyWord(12, Window->MoneyWordOne, Window->MoneyWordFew, Window->MoneyWordMany).ToString(),
		TEXT("монет"));
	TestEqual(TEXT("21 монета"),
		UCorpseLootWidget::PickMoneyWord(21, Window->MoneyWordOne, Window->MoneyWordFew, Window->MoneyWordMany).ToString(),
		TEXT("монета"));

	// Только деньги — тоже одна строка.
	FCorpseLootTakenSummary MoneyOnly;
	MoneyOnly.Money = 1;
	TestEqual(TEXT("Сводка только с деньгами"),
		Window->BuildTakenSummaryText(MoneyOnly).ToString(), TEXT("Получено: 1 монета"));

	// Брать было нечего — сообщения нет вовсе.
	TestTrue(TEXT("Пустая сводка ничего не показывает"),
		Window->BuildTakenSummaryText(FCorpseLootTakenSummary()).IsEmpty());

	// Огнестрел: напоминание уезжает ТОЙ ЖЕ строкой (ADR-063 п.2), а не вторым сообщением.
	FCorpseLootTakenSummary WithGun;
	WithGun.AddItem(FText::FromString(TEXT("Пистолет")), 1);
	WithGun.bFirearmTaken = true;
	const FString GunLine = Window->BuildTakenSummaryText(WithGun).ToString();
	TestEqual(TEXT("Строка с огнестрелом"), GunLine,
		TEXT("Получено: Пистолет (оружие наденьте в инвентаре)"));
	TestFalse(TEXT("Переносов строк нет и здесь"), GunLine.Contains(TEXT("\n")));

	return true;
}

// ===========================================================================
// 7. п.3.4: «Сказать словами, что влезло не всё, и не трогать те тела, из которых не
//    удалось забрать». ВМЕСТИМОСТИ у рюкзака сегодня НЕТ, настоящий отказ невозможен —
//    поэтому ветка проверяется суррогатом: окно без игрока = принимать некому. Проверяем
//    ровно два обещания: тело осталось полным и в землю не пошло, а строка про это есть.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroupSearchBackpackRefusedTest,
	"ContrarySurvivor.GroupSearch.RefusedItemLeavesBodyUntouched", GroupSearchTestFlags)

bool FGroupSearchBackpackRefusedTest::RunTest(const FString& Parameters)
{
	UWorld* World = GroupSearchTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		UCorpseLootComponent* Body = GroupSearchTestWorld::SpawnBody(World, FVector(0.f, 0.f, 100.f));
		UCorpseLootWidget* Window = NewObject<UCorpseLootWidget>();
		if (!Body || !Window)
		{
			AddError(TEXT("Тело или окно не создались"));
			GroupSearchTestWorld::Destroy(World);
			return false;
		}

		// Игрока нет — принимать лут некому: это и есть суррогат отказа рюкзака.
		Window->InitCorpseLootGroup(TArray<UCorpseLootComponent*>({ Body }), /*InPlayer=*/nullptr);
		const FCorpseLootTakenSummary Summary = Window->TakeAllFromGroup();

		TestTrue(TEXT("Отказ рюкзака отмечен"), Summary.bBackpackRefused);
		TestTrue(TEXT("Тело осталось полным"), Body->HasLoot());
		TestEqual(TEXT("Предмет остался в теле"), Body->GetLootItems().Num(), 1);
		TestFalse(TEXT("Нетронутое тело в землю не уходит"), Body->IsSinking());

		// Игроку об этом говорят словами (одной строкой).
		const FString Line = Window->BuildTakenSummaryText(Summary).ToString();
		TestTrue(TEXT("В строке сказано, что влезло не всё"), Line.Contains(TEXT("влезло не всё")));
		TestFalse(TEXT("Переносов строк нет"), Line.Contains(TEXT("\n")));
	}

	GroupSearchTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 8. Отчёт Рината 23.08 п.5: математика растворения. «Сразу на 35% прозрачнее» — стартовая
//    непрозрачность 0.65; до паузы держится; за длительность равномерно тает до нуля.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorpseDissolveMathTest,
	"ContrarySurvivor.GroupSearch.DissolveOpacityMath", GroupSearchTestFlags)

bool FCorpseDissolveMathTest::RunTest(const FString& Parameters)
{
	// Значения Рината: 35% мгновенно, пауза 1 с, растворение 4 с.
	TestEqual(TEXT("Мгновенный сдвиг: сразу 65% непрозрачности"),
		UCorpseLootComponent::GetDissolveOpacityAtTime(0.0f, 0.35f, 1.0f, 4.0f), 0.65f);
	TestEqual(TEXT("До конца паузы тело не тает"),
		UCorpseLootComponent::GetDissolveOpacityAtTime(1.0f, 0.35f, 1.0f, 4.0f), 0.65f);
	TestEqual(TEXT("Середина растворения — половина от стартовой"),
		UCorpseLootComponent::GetDissolveOpacityAtTime(3.0f, 0.35f, 1.0f, 4.0f), 0.325f);
	TestEqual(TEXT("Конец растворения — полная прозрачность"),
		UCorpseLootComponent::GetDissolveOpacityAtTime(5.0f, 0.35f, 1.0f, 4.0f), 0.0f);
	TestEqual(TEXT("После конца ниже нуля не уходит"),
		UCorpseLootComponent::GetDissolveOpacityAtTime(99.0f, 0.35f, 1.0f, 4.0f), 0.0f);

	// Защита от настроек: нулевая длительность — исчезает сразу после паузы; доля
	// прозрачности больше единицы зажимается (тело просто сразу невидимо).
	TestEqual(TEXT("Нулевая длительность — сразу ноль после паузы"),
		UCorpseLootComponent::GetDissolveOpacityAtTime(1.1f, 0.35f, 1.0f, 0.0f), 0.0f);
	TestEqual(TEXT("Доля больше единицы зажата — старт с нуля"),
		UCorpseLootComponent::GetDissolveOpacityAtTime(0.0f, 1.5f, 1.0f, 4.0f), 0.0f);
	return true;
}

// ===========================================================================
// 9. Отчёт Рината 23.08 п.5: живой цикл растворения. Материал — транзиентный (в памяти,
//    не с диска): тест не зависит от того, создал ли оператор M_CorpseDissolve. Труп
//    получает живой материал на меш, лежит на месте (не тонет!) и после паузы с
//    длительностью удаляется из мира.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorpseDissolveCycleTest,
	"ContrarySurvivor.GroupSearch.DissolveCycleRemovesBody", GroupSearchTestFlags)

bool FCorpseDissolveCycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = GroupSearchTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		UCorpseLootComponent* Body = GroupSearchTestWorld::SpawnBody(World, FVector(0.f, 0.f, 100.f));
		if (!Body)
		{
			AddError(TEXT("Тело не создалось"));
			GroupSearchTestWorld::Destroy(World);
			return false;
		}
		AActor* BodyActor = Body->GetOwner();
		const float StartZ = BodyActor->GetActorLocation().Z;

		UMaterial* DissolveParent = NewObject<UMaterial>(GetTransientPackage(),
			TEXT("M_DissolveQA"));
		Body->bDissolveWhenSearched = true; // SpawnBody выключил ради тестов погружения
		Body->DissolveMaterial = DissolveParent;
		Body->DissolveDelay = 0.4f;
		Body->DissolveDuration = 0.8f;

		Body->StartSearchedSink();
		TestTrue(TEXT("Пошло растворение"), Body->IsDissolving());
		TestFalse(TEXT("Погружение при растворении не запускается"), Body->IsSinking());

		// Меш трупа рисуется живым материалом растворения (родитель — наш транзиентный).
		if (const ACharacter* BodyCharacter = Cast<ACharacter>(BodyActor))
		{
			const USkeletalMeshComponent* Mesh = BodyCharacter->GetMesh();
			if (Mesh && Mesh->GetNumMaterials() > 0)
			{
				TestNotNull(TEXT("В слоте меша — живой материал растворения"),
					Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)));
			}
		}

		// Середина: тело ещё в мире и НЕ опустилось (растворение не двигает).
		GroupSearchTestWorld::AdvanceWorld(World, 0.6f);
		TestTrue(TEXT("В середине растворения тело ещё в мире"), IsValid(BodyActor));
		TestTrue(TEXT("Растворяющееся тело не тонет"),
			FMath::IsNearlyEqual(BodyActor->GetActorLocation().Z, StartZ, 0.01f));

		// Конец (всего 1.4 с > пауза 0.4 + растворение 0.8): тело удалено из сцены.
		GroupSearchTestWorld::AdvanceWorld(World, 0.8f);
		TestFalse(TEXT("Растворившееся тело удалено из мира"), IsValid(BodyActor));
	}

	GroupSearchTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 10. Отчёт Рината 23.08 п.5, запасной путь: материал растворения не задан — тело честно
//     уходит в землю по-старому (п.3.2 издателя), а не зависает навсегда обысканным.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorpseDissolveFallbackTest,
	"ContrarySurvivor.GroupSearch.DissolveFallsBackToSinkWithoutMaterial", GroupSearchTestFlags)

bool FCorpseDissolveFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = GroupSearchTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		UCorpseLootComponent* Body = GroupSearchTestWorld::SpawnBody(World, FVector(0.f, 0.f, 100.f));
		if (!Body)
		{
			AddError(TEXT("Тело не создалось"));
			GroupSearchTestWorld::Destroy(World);
			return false;
		}

		Body->bDissolveWhenSearched = true;
		Body->DissolveMaterial.Reset(); // пустая ссылка: LoadSynchronous вернёт null без походов на диск

		Body->StartSearchedSink();
		TestFalse(TEXT("Без материала растворение не запускается"), Body->IsDissolving());
		TestTrue(TEXT("Тело откатилось на уход в землю"), Body->IsSinking());
	}

	GroupSearchTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
