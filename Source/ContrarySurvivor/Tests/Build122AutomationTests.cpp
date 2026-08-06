// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны Build 1.2.2 (сокет хвата + размещаемый лут). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Build122; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE:
//   - контракт сокета WeaponGripSocket на ОБОИХ скелетах-дублях (есть, кость R_Hand,
//     scale=(1,1,1), трансформы совпадают) — ассетная половина контракта -normalize;
//   - арифметика поправки ножа: константы конструктора AMeleeWeapon = точная инверсия
//     сокет-трансформа этой волны (композиция даёт тождество -> нож лежит по кости);
//   - предупреждение о ПУСТОМ пикапе, размещённом на карте (LogQA Warning), и его
//     ОТСУТСТВИЕ у рантайм-спавна (DropLoot/мешок смерти наполняются ПОСЛЕ BeginPlay);
//   - обыск мешка-пикапа (ТЗ Рината про BP_Picup): содержимое лежит в общем контейнере
//     UCorpseLootComponent, по умолчанию открывается окно (а не мгновенный забор), забор
//     ИДЁТ ПОШТУЧНО, опустевший мешок исчезает, в реестр обыскиваемых трупов он не встаёт;
//   - переключатель «забирать всё сразу» возвращает прежнее поведение Collect;
//   - доводка 05-08: предел количества в сделке ПРОДАЖИ равен размеру стопки предмета
//     (баг «Вода х3 продаётся по одной штуке»);
//   - подсказка взаимодействия 08-06: склейка «действие — способ» для компьютера и для
//     тач-слоя, тексты действий и шаблон берутся из настроек контроллера;
//   - экран смерти после замечаний издателя (ADR-059): причина смерти одной фразой
//     («Тебя убил волк» / «Ты умер от жажды») и фраза потерь без нулевых частей;
//   - подготовка к публикационной сборке (Б5): договор выключателя отладочного харнесса
//     и адрес канала кнопки «Написать мне», приходящий из конфига.
// НЕ покрывается headless (нужен PIE): фактическая поза оружия в ладони на анимируемом
//   персонаже (скелетные меши в тест-мире не грузятся), клики по плиткам самого окна
//   обыска (Slate) — за живым осмотром Рината/лида.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AMeleeWeapon.h"
#include "AAmmoItem.h"
#include "AArmorTiers.h"
#include "AConsumableItem.h"
#include "AMasterInventoryItem.h"
#include "APistol.h"
#include "AQuestItem.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/UI/ShopScreenWidget.h"
#include "Animation/Skeleton.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/DeathScreenWidget.h"
#include "ContrarySurvivor/UI/EndOfStoryWidget.h"   // UEndOfStorySettings: адрес канала из конфига
#include "ContrarySurvivor/Debug/QADebug.h"         // CONTRARY_WITH_QA_CHEATS: договор выключателя
#include "Internationalization/Culture.h"           // FCulture::GetName — запомнить культуру теста
#include "Internationalization/Internationalization.h"
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags Build122TestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace Build122GripSocket
{
	// Оба скелета-дубля гуманоида (те же пути, что в AddGripSocketCommandlet.cpp).
	static const TCHAR* PlayerSkeletonPath =
		TEXT("/Game/TestContentAndCode/PreProduction/HeadAndSkeletonfbx_Head_Skeleton.HeadAndSkeletonfbx_Head_Skeleton");
	static const TCHAR* BanditSkeletonPath =
		TEXT("/Game/Characters/Shared/Humanoid/HeadAndSkeletonfbx_Head_Skeleton.HeadAndSkeletonfbx_Head_Skeleton");

	static const USkeletalMeshSocket* FindGripSocket(const USkeleton* Skeleton)
	{
		for (const USkeletalMeshSocket* Socket : Skeleton->Sockets)
		{
			if (Socket && Socket->SocketName == FName(TEXT("WeaponGripSocket")))
			{
				return Socket;
			}
		}
		return nullptr;
	}
}

// Транзиентный игровой мир (копия обвязки Build121TestWorld — она static в своём .cpp).
namespace Build122TestWorld
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

	// Расходник-«данные» с заданным типом и стаком (как его кладёт лут в мешок).
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

	// Живые записи реестра обыскиваемых трупов ИМЕННО этого мира (реестр статический,
	// в нём могут доживать записи прошлых тестов).
	static int32 CountValidCorpses(UWorld* World)
	{
		int32 Count = 0;
		for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : UCorpseLootComponent::GetSearchableCorpses())
		{
			if (Ptr.IsValid() && Ptr->GetWorld() == World)
			{
				++Count;
			}
		}
		return Count;
	}
}

// ===========================================================================
// 1. Контракт сокета хвата на обоих скелетах: есть, кость R_Hand, scale=(1,1,1),
//    трансформы совпадают (иначе игрок и бандит держат оружие по-разному).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122GripSocketAssetContractTest,
	"ContrarySurvivor.Build122.GripSocket.AssetContract", Build122TestFlags)

bool FBuild122GripSocketAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace Build122GripSocket;

	USkeleton* PlayerSkeleton = LoadObject<USkeleton>(nullptr, PlayerSkeletonPath);
	USkeleton* BanditSkeleton = LoadObject<USkeleton>(nullptr, BanditSkeletonPath);
	if (!TestNotNull(TEXT("Скелет игрока (PreProduction) загрузился"), PlayerSkeleton)
		|| !TestNotNull(TEXT("Скелет бандита (Shared) загрузился"), BanditSkeleton))
	{
		return false;
	}

	const USkeletalMeshSocket* PlayerSocket = FindGripSocket(PlayerSkeleton);
	const USkeletalMeshSocket* BanditSocket = FindGripSocket(BanditSkeleton);
	if (!TestNotNull(TEXT("WeaponGripSocket есть на скелете игрока"), PlayerSocket)
		|| !TestNotNull(TEXT("WeaponGripSocket есть на скелете бандита"), BanditSocket))
	{
		return false;
	}

	TestEqual(TEXT("Сокет игрока сидит на кости R_Hand"),
		PlayerSocket->BoneName, FName(TEXT("R_Hand")));
	TestEqual(TEXT("Сокет бандита сидит на кости R_Hand"),
		BanditSocket->BoneName, FName(TEXT("R_Hand")));

	// Build 1.2.2: scale строго 1 — иначе масштабируются цифровые поправки хвата.
	TestTrue(TEXT("Scale сокета игрока = (1,1,1)"),
		PlayerSocket->RelativeScale.Equals(FVector::OneVector));
	TestTrue(TEXT("Scale сокета бандита = (1,1,1)"),
		BanditSocket->RelativeScale.Equals(FVector::OneVector));

	TestTrue(TEXT("Позиции сокета на двух скелетах совпадают"),
		PlayerSocket->RelativeLocation.Equals(BanditSocket->RelativeLocation));
	TestTrue(TEXT("Повороты сокета на двух скелетах совпадают"),
		PlayerSocket->RelativeRotation.Equals(BanditSocket->RelativeRotation));
	return true;
}

// ===========================================================================
// 2. Арифметика поправки ножа: константы AMeleeWeapon = инверсия сокет-трансформа
//    ЭТОЙ волны (литералы = поза Рината, срез 08-02). Композиция «поправка ∘ сокет»
//    обязана дать тождество: нож ложится ровно по кости R_Hand, как до правки сокета.
//    Тест про МАТЕМАТИКУ констант, не про живой ассет: будущая перенастройка сокета
//    Ринатом тест не сломает (дрейф ассетов ловят -verify и тест №1).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122KnifeGripOffsetMathTest,
	"ContrarySurvivor.Build122.GripSocket.KnifeOffsetMath", Build122TestFlags)

bool FBuild122KnifeGripOffsetMathTest::RunTest(const FString& Parameters)
{
	// Поза сокета, подобранная Ринатом (Shared-скелет, коммит 30aedb1; срез 08-02).
	const FTransform SocketTM(
		FRotator(0.000341, -179.999728, -0.000062),
		FVector(-0.000000, -0.061311, 0.000000),
		FVector::OneVector);

	const AMeleeWeapon* KnifeCDO = GetDefault<AMeleeWeapon>();
	if (!TestNotNull(TEXT("CDO ножа доступен"), KnifeCDO))
	{
		return false;
	}
	const FTransform KnifeFixTM(
		KnifeCDO->GetGripOffsetRotation(), KnifeCDO->GetGripOffsetLocation(), FVector::OneVector);

	// Та же композиция, что в EquipWeapon: итог = ПоправкаОружия ∘ Сокет.
	const FTransform Composed = KnifeFixTM * SocketTM;
	TestTrue(FString::Printf(TEXT("Сдвиг композиции ~0 (фактически %s)"),
			*Composed.GetLocation().ToString()),
		Composed.GetLocation().IsNearlyZero(0.001));
	const double AngleDeg = FMath::RadiansToDegrees(Composed.GetRotation().GetAngle());
	TestTrue(FString::Printf(TEXT("Поворот композиции ~0 градусов (фактически %.6f)"), AngleDeg),
		AngleDeg < 0.01);

	// Пистолет — эталон сокета: у базового оружия поправка обязана быть нулевой.
	const AMasterWeapon* BaseCDO = GetDefault<AMasterWeapon>();
	TestTrue(TEXT("Поправка базового оружия (пистолета) нулевая"),
		BaseCDO->GetGripOffsetLocation().IsNearlyZero()
			&& BaseCDO->GetGripOffsetRotation().IsNearlyZero());

	// Задел на двуручное: по умолчанию сокет левой руки ПУСТ (одноручное).
	TestEqual(TEXT("LeftHandGripSocketName по умолчанию пуст"),
		BaseCDO->GetLeftHandGripSocketName(), FName(NAME_None));
	return true;
}

// ===========================================================================
// 3. Пустой РАЗМЕЩЁННЫЙ пикап предупреждает в LogQA ровно один раз; рантайм-спавн
//    (мешок смерти/DropLoot наполняются ПОСЛЕ BeginPlay) — молчит.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122PickupPlacedEmptyWarnTest,
	"ContrarySurvivor.Build122.Pickup.PlacedEmptyWarning", Build122TestFlags)

bool FBuild122PickupPlacedEmptyWarnTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Ровно ОДНО предупреждение: от «размещённого» пустого пикапа, не от рантайм-спавна.
	AddExpectedMessagePlain(TEXT("размещён на карте ПУСТЫМ"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/1);

	// «Размещённый на карте» пикап: отложенный спавн, флаг bNetStartup ДО BeginPlay —
	// то же состояние, что у актора уровня после ULevel::InitializeNetworkActors
	// (Level.cpp:3327; BeginPlay уровня идёт позже инициализации).
	APickup* Placed = World->SpawnActorDeferred<APickup>(APickup::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Отложенный спавн пикапа удался"), Placed))
	{
		Build122TestWorld::Destroy(World);
		return false;
	}
	Placed->bNetStartup = true;
	Placed->FinishSpawning(FTransform::Identity); // BeginPlay внутри — здесь ждём Warning

	// Рантайм-спавн (как DropLoot/мешок смерти): в BeginPlay пуст — предупреждать НЕЛЬЗЯ.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APickup* Runtime = World->SpawnActor<APickup>(
		APickup::StaticClass(), FVector(100.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
	TestNotNull(TEXT("Рантайм-спавн пикапа удался"), Runtime);

	Build122TestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 4. Мешок-пикап обыскивается как труп (ТЗ Рината про BP_Picup): содержимое лежит в общем
//    контейнере, по умолчанию открывается окно (а не мгновенный забор), брать можно
//    ПОШТУЧНО, опустевший мешок исчезает. В реестр обыскиваемых трупов мешок не встаёт —
//    иначе один и тот же лут предлагался бы двумя разными интерактивами.
//    Забор эмулируем так, как его делает окно: TakeItem/TakeMoney контейнера + рюкзак/статы
//    (сами кнопки окна — Slate, headless не кликаются).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122PickupSearchWindowTest,
	"ContrarySurvivor.Build122.Pickup.SearchTakeOneByOne", Build122TestFlags)

bool FBuild122PickupSearchWindowTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		APlayerCharacter* Player = Build122TestWorld::Spawn<APlayerCharacter>(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
		APickup* Bag = Build122TestWorld::Spawn<APickup>(World, FVector(200.f, 0.f, 100.f));
		AConsumableItem* Food = Build122TestWorld::SpawnConsumable(World, EConsumableType::Food);
		AConsumableItem* Water = Build122TestWorld::SpawnConsumable(World, EConsumableType::Water);

		TestNotNull(TEXT("Рюкзак игрока"), Inv);
		TestNotNull(TEXT("Статы игрока"), Stats);
		TestNotNull(TEXT("Мешок-пикап"), Bag);
		TestNotNull(TEXT("Еда"), Food);
		TestNotNull(TEXT("Вода"), Water);

		if (Inv && Stats && Bag && Food && Water)
		{
			const int32 CorpsesBefore = Build122TestWorld::CountValidCorpses(World);

			TArray<AMasterInventoryItem*> BagItems;
			BagItems.Add(Food);
			BagItems.Add(Water);
			Bag->InitLootBag(BagItems, /*Money=*/50.0f);

			UCorpseLootComponent* Container = Bag->GetLootContainer();
			TestNotNull(TEXT("У мешка есть контейнер обыска"), Container);
			if (!Container)
			{
				Build122TestWorld::Destroy(World);
				return false;
			}

			TestTrue(TEXT("По умолчанию мешок открывает окно обыска"), Bag->UsesSearchWindow());
			TestTrue(TEXT("В мешке есть что забирать"), Bag->HasLoot());
			TestEqual(TEXT("Деньги легли в контейнер"), Container->GetMoney(), 50.0f);
			TestEqual(TEXT("Оба предмета легли в контейнер"), Container->GetLootItems().Num(), 2);
			TestEqual(TEXT("Мешок НЕ встал в реестр обыскиваемых трупов"),
				Build122TestWorld::CountValidCorpses(World), CorpsesBefore);

			// Забор ПОШТУЧНО: первый предмет уходит игроку, мешок остаётся лежать.
			const float MoneyBefore = Stats->GetMoney();
			TestTrue(TEXT("Первый предмет вынут из мешка"), Container->TakeItem(Food));
			TestTrue(TEXT("Первый предмет лёг в рюкзак"), Inv->AddItem(Food));
			TestTrue(TEXT("Мешок остался на земле — забрали не всё"), IsValid(Bag));
			TestEqual(TEXT("В мешке остался один предмет"), Container->GetLootItems().Num(), 1);
			TestTrue(TEXT("Остаток мешка виден клавише действия"), Bag->HasLoot());

			// Деньги — отдельная плитка окна.
			Stats->AddMoney(Container->TakeMoney());
			TestEqual(TEXT("Деньги начислены игроку"), Stats->GetMoney(), MoneyBefore + 50.0f);
			TestTrue(TEXT("Мешок жив, пока в нём лежит второй предмет"), IsValid(Bag));

			// Последняя вещь — мешок опустел и исчезает (как раньше исчезал после подбора).
			TestTrue(TEXT("Второй предмет вынут из мешка"), Container->TakeItem(Water));
			TestTrue(TEXT("Второй предмет лёг в рюкзак"), Inv->AddItem(Water));
			TestFalse(TEXT("Обысканный до конца мешок исчез"), IsValid(Bag));
		}
		else
		{
			bOk = false;
		}
	}
	Build122TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 5. Переключатель «забирать всё сразу» возвращает прежнее поведение (Collect отдаёт
//    деньги и предметы одним нажатием и уничтожает пикап), а брошенное содержимое
//    умирает вместе с мешком — скрытые акторы-данные не должны оставаться в мире.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122PickupInstantCollectTest,
	"ContrarySurvivor.Build122.Pickup.InstantCollectSwitch", Build122TestFlags)

bool FBuild122PickupInstantCollectTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		APlayerCharacter* Player = Build122TestWorld::Spawn<APlayerCharacter>(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
		APickup* Bag = Build122TestWorld::Spawn<APickup>(World, FVector(300.f, 0.f, 100.f));
		AConsumableItem* Food = Build122TestWorld::SpawnConsumable(World, EConsumableType::Food);

		if (Inv && Stats && Bag && Food)
		{
			Bag->bInstantCollect = true;
			TestFalse(TEXT("Переключатель убирает окно обыска"), Bag->UsesSearchWindow());

			Bag->InitLoot(/*Money=*/30.0f, Food);
			const float MoneyBefore = Stats->GetMoney();
			const int32 EntriesBefore = Inv->GetInventoryItems().Num();

			TestTrue(TEXT("Мгновенный подбор прошёл целиком"), Bag->Collect(Player));
			TestEqual(TEXT("Деньги начислены"), Stats->GetMoney(), MoneyBefore + 30.0f);
			TestEqual(TEXT("Предмет попал в рюкзак"), Inv->GetInventoryItems().Num(), EntriesBefore + 1);
			TestFalse(TEXT("Пикап уничтожен после подбора"), IsValid(Bag));

			// Не забранное содержимое: раньше его чистил APickup::EndPlay, теперь — EndPlay
			// контейнера. Проверяем, что предмет не остаётся висеть в мире.
			APickup* Abandoned = Build122TestWorld::Spawn<APickup>(World, FVector(400.f, 0.f, 100.f));
			AConsumableItem* Lost = Build122TestWorld::SpawnConsumable(World, EConsumableType::Water);
			if (Abandoned && Lost)
			{
				Abandoned->InitLoot(/*Money=*/0.0f, Lost);
				World->DestroyActor(Abandoned);
				TestFalse(TEXT("Брошенное содержимое уничтожено вместе с мешком"), IsValid(Lost));
			}
			else
			{
				bOk = false;
			}
		}
		else
		{
			bOk = false;
		}
	}
	Build122TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 6. Предел количества в сделке ПРОДАЖИ = размер стопки (баг Рината 05-08: стопка
//    «Вода х3» открывалась окном «Количество 1 из 1», кнопки плюс/минус упирались в кламп).
//    Правило проверяется на чистой функции UShopScreenWidget::GetSellQtyMax — сам виджет
//    без окна и контроллера headless не создать, а правило целиком лежит в ней.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122ShopSellQtyMaxTest,
	"ContrarySurvivor.Build122.Shop.SellQtyMaxFromStack", Build122TestFlags)

bool FBuild122ShopSellQtyMaxTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		// Стопка расходника — то, на чём баг и поймали.
		AConsumableItem* Water = Build122TestWorld::SpawnConsumable(World, EConsumableType::Water, /*Stack=*/3);
		// Патроны — прежнее поведение не должно измениться.
		AAmmoItem* Ammo = Build122TestWorld::Spawn<AAmmoItem>(World);
		// Броня — нестакающийся предмет, продаётся ровно одной штукой.
		AHeadArmorT1* Helmet = Build122TestWorld::Spawn<AHeadArmorT1>(World);

		if (Water && Ammo && Helmet)
		{
			TestTrue(TEXT("Вода стакается"), Water->IsStackable());
			TestEqual(TEXT("Стопка воды из 3 продаётся по 3"), UShopScreenWidget::GetSellQtyMax(Water), 3);

			Water->StackCount = 1;
			TestEqual(TEXT("Одна бутылка воды — предел 1"), UShopScreenWidget::GetSellQtyMax(Water), 1);

			Ammo->StackCount = 7;
			TestEqual(TEXT("Пачка патронов из 7 продаётся по 7"), UShopScreenWidget::GetSellQtyMax(Ammo), 7);

			TestFalse(TEXT("Броня не стакается"), Helmet->IsStackable());
			TestEqual(TEXT("Нестакающийся предмет — предел 1"), UShopScreenWidget::GetSellQtyMax(Helmet), 1);

			// Пустой предмет не должен ронять окно.
			TestEqual(TEXT("Без предмета предел 1"), UShopScreenWidget::GetSellQtyMax(nullptr), 1);
		}
		else
		{
			bOk = false;
		}
	}
	Build122TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 7. Дыра в экономике (проверка Рината на телефоне 05-08: вода покупалась за 5 и
//    продавалась за 6). Правило: цена выкупа СТРОГО ниже цены покупки того же предмета —
//    и так для КАЖДОЙ позиции прайс-листа, а не только для воды. Тест перебирает весь
//    прайс-лист живого торговца, поэтому новая позиция с перекосом уронит его сама.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122ShopBuybackTest,
	"ContrarySurvivor.Build122.Shop.BuybackBelowPrice", Build122TestFlags)

bool FBuild122ShopBuybackTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		AMasterTrader* Trader = Build122TestWorld::Spawn<AMasterTrader>(World);
		if (Trader)
		{
			const TArray<FShopEntry>& Catalog = Trader->GetCatalog();
			TestTrue(TEXT("Прайс-лист не пуст"), Catalog.Num() > 0);

			int32 Checked = 0;
			for (const FShopEntry& Entry : Catalog)
			{
				if (Entry.Kind != EShopEntryKind::Item || !Entry.ItemClass)
				{
					continue; // патроны идут по своей цене за штуку, их проверяем отдельно
				}

				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AMasterInventoryItem* Item = World->SpawnActor<AMasterInventoryItem>(
					Entry.ItemClass, FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
				if (!Item)
				{
					bOk = false;
					continue;
				}
				// Тип расходника задаёт позиция каталога — как это делает покупка.
				if (Entry.bApplyConsumableType)
				{
					if (AConsumableItem* Cons = Cast<AConsumableItem>(Item))
					{
						Cons->ConsumableType = Entry.ConsumableType;
					}
				}

				const float Buyback = Trader->GetSellValue(Item);
				// Печатаем весь прайс-лист в лог теста: по нему лид и Ринат сверяют числа
				// глазами, не собирая игру.
				UE_LOG(LogQA, Display, TEXT("QA: ПРАЙС '%s': покупка %.0f, выкуп %.0f"),
					*Entry.DisplayName, Entry.Price, Buyback);
				TestTrue(*FString::Printf(TEXT("'%s': выкуп %.0f строго ниже цены покупки %.0f"),
					*Entry.DisplayName, Buyback, Entry.Price), Buyback < Entry.Price);
				TestTrue(*FString::Printf(TEXT("'%s': выкуп больше нуля"), *Entry.DisplayName),
					Buyback > 0.0f);
				++Checked;

				Item->Destroy();
			}
			TestTrue(TEXT("Проверена хотя бы одна позиция прайс-листа"), Checked > 0);

			// Патроны: покупка за штуку дороже выкупа за штуку.
			for (const FShopEntry& Entry : Catalog)
			{
				if (Entry.Kind == EShopEntryKind::Ammo && Entry.AmmoAmount > 0)
				{
					const float PricePerRound = Entry.Price / static_cast<float>(Entry.AmmoAmount);
					TestTrue(TEXT("Патрон выкупается дешевле, чем продаётся"),
						Trader->GetAmmoSellPerRound() < PricePerRound);
				}
			}

			// Шкура волка — особая цена 15 монет (решение лида 05-08 по РИ-28). Ключ предмета
			// служебный, дословно как у дропа волка (WolfCharacter::QuestLootItemName).
			AQuestItem* Pelt = Build122TestWorld::Spawn<AQuestItem>(World);
			AQuestItem* Laptop = Build122TestWorld::Spawn<AQuestItem>(World);
			if (Pelt && Laptop)
			{
				Pelt->ItemName = TEXT("Шкура волка");
				TestEqual(TEXT("Шкура волка выкупается за 15"), Trader->GetSellValue(Pelt), 15.0f);
				TestTrue(TEXT("Шкура осталась квестовым предметом"),
					Pelt->GetItemCategory() == EItemCategory::Quest);

				// Прочие квестовые вещи особой цены не получили — идут по категории, как раньше.
				Laptop->ItemName = TEXT("Ноутбук");
				TestEqual(TEXT("Ноутбук по-прежнему по цене категории"),
					Trader->GetSellValue(Laptop), 2.0f);
			}
			else
			{
				bOk = false;
			}
		}
		else
		{
			bOk = false;
		}
	}
	Build122TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 8. Старт без огнестрела (решение Рината 05-08): у нового игрока только нож, пистолет
//    покупается у торговца за 150. Проверяем, что игра при этом не остаётся безоружной
//    (нож сразу в руках) и что купленный огнестрел занимает пустой слот оружия, а не
//    лежит в рюкзаке мёртвым грузом.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122StartWithoutFirearmTest,
	"ContrarySurvivor.Build122.Player.StartWithoutFirearm", Build122TestFlags)

bool FBuild122StartWithoutFirearmTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		APlayerCharacter* Player = Build122TestWorld::Spawn<APlayerCharacter>(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (Player && Inv)
		{
			// 1) Новый игрок: огнестрела нет, но руки не пустые — в них нож.
			TestNull(TEXT("Слот огнестрела на старте пуст"), Player->GetRangedWeaponInstance());
			TestNotNull(TEXT("В руках есть оружие"), Player->GetCurrentWeapon());
			TestTrue(TEXT("В руках именно нож (ближний бой)"),
				Cast<ARangedWeapon>(Player->GetCurrentWeapon()) == nullptr);
			// Перезарядка без огнестрела не должна ничего ломать.
			Player->ReloadCurrentWeapon();
			TestNull(TEXT("Перезарядка не выдала огнестрел"), Player->GetRangedWeaponInstance());
			// Кнопка «Оружие» без второго ствола просто ничего не делает.
			AMasterWeapon* BeforeSwitch = Player->GetCurrentWeapon();
			Player->SwitchWeapon();
			TestEqual(TEXT("Переключать не на что — оружие в руках прежнее"),
				Player->GetCurrentWeapon(), BeforeSwitch);

			// 2) Купленный пистолет занимает пустой слот оружия и уходит из рюкзака.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			APistol* Bought = World->SpawnActor<APistol>(APistol::StaticClass(),
				FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
			if (Bought)
			{
				Bought->SetActorHiddenInGame(true);
				Bought->SetActorEnableCollision(false);
				Inv->AddItem(Bought);
				TestTrue(TEXT("Пистолет забран из рюкзака в слот оружия"),
					Player->TryAdoptRangedWeapon(Bought));
				TestEqual(TEXT("Слот огнестрела занял купленный пистолет"),
					Player->GetRangedWeaponInstance(), static_cast<AMasterWeapon*>(Bought));
				TestFalse(TEXT("В рюкзаке пистолета больше нет"),
					Inv->GetInventoryItems().Contains(Bought));

				// Теперь кнопка «Оружие» переключает на огнестрел и обратно.
				Player->SwitchWeapon();
				TestEqual(TEXT("После переключения в руках пистолет"),
					Player->GetCurrentWeapon(), static_cast<AMasterWeapon*>(Bought));

				// 3) Второй ствол слот не отбирает — остаётся в рюкзаке.
				APistol* Second = World->SpawnActor<APistol>(APistol::StaticClass(),
					FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
				if (Second)
				{
					Inv->AddItem(Second);
					TestFalse(TEXT("Второй пистолет слот не занимает"),
						Player->TryAdoptRangedWeapon(Second));
					TestTrue(TEXT("Второй пистолет остался в рюкзаке"),
						Inv->GetInventoryItems().Contains(Second));
				}
				else
				{
					bOk = false;
				}
			}
			else
			{
				bOk = false;
			}
		}
		else
		{
			bOk = false;
		}
	}
	Build122TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 9. Вступление старосты после добавления реплики про пистолет (05-08): порядок событий
//    не поехал (сперва подарок-аптечка, потом старт квеста, крючок — последней репликой),
//    у каждой реплики есть текст и подпись кнопки. Слова не проверяем: формулировки
//    утверждает Ринат, а тест не должен падать от правки текста.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122ElderIntroOrderTest,
	"ContrarySurvivor.Build122.Dialog.ElderIntroOrder", Build122TestFlags)

bool FBuild122ElderIntroOrderTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		AElderNPC* Elder = Build122TestWorld::Spawn<AElderNPC>(World);
		if (Elder)
		{
			const TArray<FElderIntroLine>& Lines = Elder->GetIntroLines();
			TestTrue(TEXT("Вступление не пустое"), Lines.Num() > 0);

			int32 GiftIndex = INDEX_NONE;
			int32 QuestIndex = INDEX_NONE;
			int32 GiftCount = 0;
			int32 QuestCount = 0;
			for (int32 i = 0; i < Lines.Num(); ++i)
			{
				TestFalse(*FString::Printf(TEXT("Реплика %d не пустая"), i), Lines[i].NPCText.IsEmpty());
				TestFalse(*FString::Printf(TEXT("У реплики %d есть подпись кнопки"), i),
					Lines[i].ButtonLabel.IsEmpty());

				if (Lines[i].Action == EElderIntroAction::GiveGift)
				{
					GiftIndex = (GiftIndex == INDEX_NONE) ? i : GiftIndex;
					++GiftCount;
				}
				else if (Lines[i].Action == EElderIntroAction::StartQuest)
				{
					QuestIndex = (QuestIndex == INDEX_NONE) ? i : QuestIndex;
					++QuestCount;
				}
			}

			TestEqual(TEXT("Аптечка выдаётся ровно один раз"), GiftCount, 1);
			TestEqual(TEXT("Квест стартует ровно один раз"), QuestCount, 1);
			TestTrue(TEXT("Сперва аптечка, потом квест"),
				GiftIndex != INDEX_NONE && QuestIndex != INDEX_NONE && GiftIndex < QuestIndex);
			TestTrue(TEXT("После старта квеста есть ещё реплики (намёк и крючок)"),
				QuestIndex < Lines.Num() - 1);
			TestTrue(TEXT("Последняя реплика-крючок ничего не запускает"),
				Lines.Last().Action == EElderIntroAction::None);
		}
		else
		{
			bOk = false;
		}
	}
	Build122TestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 10. Подсказка взаимодействия (задача Рината 08-06): подсказка ВСЕГДА называет и
//     действие, и способ его выполнить. На компьютере способ — клавиша («Обыскать — E»),
//     при показанном тач-слое — подпись экранной кнопки («Обыскать — ДЕЙСТВИЕ»).
//     Тач-слой headless не поднимается (нужен Slate), поэтому обе подсказки собираются
//     той же статической склейкой, что и в живой игре, из РЕАЛЬНЫХ настроек контроллера.
//     Контроллер создаётся NewObject (не Spawn): BeginPlay поднимает виджеты и ввод,
//     тесту нужны только поля и чистая склейка.
// ===========================================================================
namespace Build122Prompt
{
	static AContrarySurvivorPlayerController* MakeController()
	{
		return NewObject<AContrarySurvivorPlayerController>(GetTransientPackage());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122InteractPromptTouchTest,
	"ContrarySurvivor.Build122.InteractPrompt.TouchLayerNamesScreenButton", Build122TestFlags)

bool FBuild122InteractPromptTouchTest::RunTest(const FString& Parameters)
{
	AContrarySurvivorPlayerController* PC = Build122Prompt::MakeController();
	if (!TestNotNull(TEXT("Контроллер создан"), PC))
	{
		return false;
	}

	// Подпись способа на телефоне = подпись экранной кнопки ДЕЙСТВИЕ.
	const FText How = PC->GetInteractTouchButtonName();
	TestEqual(TEXT("Способ на телефоне — подпись экранной кнопки"), How.ToString(), TEXT("ДЕЙСТВИЕ"));

	const FText Prompt = AContrarySurvivorPlayerController::FormatInteractPrompt(
		PC->GetInteractPromptFormat(),
		PC->GetInteractActionText(EInteractKind::Corpse), How);
	TestEqual(TEXT("Подсказка у трупа на телефоне"), Prompt.ToString(), TEXT("Обыскать — ДЕЙСТВИЕ"));

	// Пустое поле подписи — способ берётся у самой кнопки (страховка от расхождения).
	PC->InteractPromptTouchButtonName = FText::GetEmpty();
	TestEqual(TEXT("Пустое поле — подпись приходит от кнопки"),
		PC->GetInteractTouchButtonName().ToString(), TEXT("ДЕЙСТВИЕ"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122InteractPromptKeyTest,
	"ContrarySurvivor.Build122.InteractPrompt.DesktopNamesKey", Build122TestFlags)

bool FBuild122InteractPromptKeyTest::RunTest(const FString& Parameters)
{
	AContrarySurvivorPlayerController* PC = Build122Prompt::MakeController();
	if (!TestNotNull(TEXT("Контроллер создан"), PC))
	{
		return false;
	}

	// Клавиша по умолчанию = реальная привязка «Interact» из Config/DefaultInput.ini.
	const FText How = PC->GetInteractKeyName();
	TestEqual(TEXT("Способ на компьютере — клавиша E"), How.ToString(), TEXT("E"));

	const FText Format = PC->GetInteractPromptFormat();
	TestEqual(TEXT("Подсказка у трупа на компьютере"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(
			Format, PC->GetInteractActionText(EInteractKind::Corpse), How).ToString(),
		TEXT("Обыскать — E"));
	TestEqual(TEXT("Подсказка у старосты называет собеседника"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(
			Format, PC->GetInteractActionText(EInteractKind::Elder), How).ToString(),
		TEXT("Поговорить со старостой — E"));
	TestEqual(TEXT("Подсказка у торговца"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(
			Format, PC->GetInteractActionText(EInteractKind::Trader), How).ToString(),
		TEXT("Торговать — E"));

	// Нечего делать — подсказки нет вовсе (одинокое тире на экран не уезжает).
	TestTrue(TEXT("Без действия подсказка пустая"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(
			Format, PC->GetInteractActionText(EInteractKind::None), How).IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122InteractPromptActionFieldsTest,
	"ContrarySurvivor.Build122.InteractPrompt.ActionTextIsEditable", Build122TestFlags)

bool FBuild122InteractPromptActionFieldsTest::RunTest(const FString& Parameters)
{
	AContrarySurvivorPlayerController* PC = Build122Prompt::MakeController();
	if (!TestNotNull(TEXT("Контроллер создан"), PC))
	{
		return false;
	}

	// Мешок с окном обыска подписывается как труп, мгновенный подбор — как подбор.
	TestEqual(TEXT("Мгновенный подбор"),
		PC->GetInteractActionText(EInteractKind::Pickup, /*bPickupUsesSearchWindow=*/false).ToString(),
		TEXT("Подобрать"));
	TestEqual(TEXT("Мешок с окном обыска"),
		PC->GetInteractActionText(EInteractKind::Pickup, /*bPickupUsesSearchWindow=*/true).ToString(),
		TEXT("Обыскать"));

	// Подмена текста действия через настройку доходит до собранной подсказки.
	PC->InteractPromptPickupAction = FText::FromString(TEXT("Забрать"));
	TestEqual(TEXT("Подсказка после подмены текста действия"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(
			PC->GetInteractPromptFormat(),
			PC->GetInteractActionText(EInteractKind::Pickup), PC->GetInteractKeyName()).ToString(),
		TEXT("Забрать — E"));

	// Подмена шаблона склейки — тоже настройка, а не зашитая строка.
	PC->InteractPromptFormat = FText::FromString(TEXT("[{How}] {Action}"));
	TestEqual(TEXT("Подсказка после подмены шаблона"),
		AContrarySurvivorPlayerController::FormatInteractPrompt(
			PC->GetInteractPromptFormat(),
			PC->GetInteractActionText(EInteractKind::Trader), PC->GetInteractKeyName()).ToString(),
		TEXT("[E] Торговать"));

	return true;
}

// ===========================================================================
// 11. Экран смерти после замечаний издателя (ADR-059): причина смерти — цельная фраза,
//     нулевые числа во фразу потерь не попадают. Сам виджет headless не поднимается
//     (нужен Slate), поэтому проверяем ЧИСТЫЕ функции сборки текста на объекте,
//     созданном NewObject, — те же, что зовёт живой экран.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122DeathCauseTextTest,
	"ContrarySurvivor.Build122.DeathScreen.CauseIsWholePhrase", Build122TestFlags)

bool FBuild122DeathCauseTextTest::RunTest(const FString& Parameters)
{
	UDeathScreenWidget* Screen = NewObject<UDeathScreenWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Экран смерти создан"), Screen))
	{
		return false;
	}

	// Полные шкалы = смерть точно не от голода и не от жажды.
	const float Full = 100.0f;

	TestEqual(TEXT("Убил волк"),
		Screen->BuildDeathCauseText(NSLOCTEXT("Death", "KillerWolf", "Волк"), Full, Full).ToString(),
		TEXT("Тебя убил волк"));
	TestEqual(TEXT("Убил бандит"),
		Screen->BuildDeathCauseText(NSLOCTEXT("Death", "KillerBandit", "Бандит"), Full, Full).ToString(),
		TEXT("Тебя убил бандит"));

	// Врага не было: причину называют опустевшие шкалы (раньше здесь стояло
	// «Убийца Неизвестно» — замечание 9 ревизии, ADR-058).
	const FText Unknown = NSLOCTEXT("Death", "KillerUnknown", "Неизвестно");
	TestEqual(TEXT("Смерть от жажды"),
		Screen->BuildDeathCauseText(Unknown, 0.0f, Full).ToString(), TEXT("Ты умер от жажды"));
	TestEqual(TEXT("Смерть от голода"),
		Screen->BuildDeathCauseText(Unknown, Full, 0.0f).ToString(), TEXT("Ты умер от голода"));
	TestEqual(TEXT("Пустые обе шкалы — называем жажду (она бьёт чаще)"),
		Screen->BuildDeathCauseText(Unknown, 0.0f, 0.0f).ToString(), TEXT("Ты умер от жажды"));
	TestEqual(TEXT("Ни врага, ни пустых шкал — общая фраза"),
		Screen->BuildDeathCauseText(Unknown, Full, Full).ToString(), TEXT("Ты не пережил эту вылазку"));

	// Имя собственное можно оставить с заглавной буквы, сняв галку.
	Screen->bLowercaseKillerName = false;
	TestEqual(TEXT("Без приведения к строчной имя идёт как записано"),
		Screen->BuildDeathCauseText(FText::FromString(TEXT("Кузьмич")), Full, Full).ToString(),
		TEXT("Тебя убил Кузьмич"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122DeathLossPhraseTest,
	"ContrarySurvivor.Build122.DeathScreen.LossPhraseHidesZero", Build122TestFlags)

bool FBuild122DeathLossPhraseTest::RunTest(const FString& Parameters)
{
	UDeathScreenWidget* Screen = NewObject<UDeathScreenWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Экран смерти создан"), Screen))
	{
		return false;
	}

	const FText RespawnFormat = Screen->RespawnSubFormat;

	// Терять нечего — фразы нет вовсе, подстроку экран прячет.
	TestTrue(TEXT("Ноль и ноль — пустая фраза"),
		Screen->BuildLossPhrase(RespawnFormat, /*Items=*/0, /*Money=*/0).IsEmpty());

	// Предметов нет — про предметы во фразе НИ СЛОВА («0 предм.» больше не бывает).
	const FString MoneyOnly = Screen->BuildLossPhrase(RespawnFormat, /*Items=*/0, /*Money=*/25).ToString();
	TestTrue(TEXT("Фраза без предметов начинается с «Потеряешь»"), MoneyOnly.StartsWith(TEXT("Потеряешь ")));
	TestTrue(TEXT("Фраза без предметов называет сумму"), MoneyOnly.Contains(TEXT("25")));
	TestFalse(TEXT("Фраза без предметов о предметах молчит"), MoneyOnly.Contains(TEXT("предм")));
	TestFalse(TEXT("Ноль во фразу не попал"), MoneyOnly.Contains(TEXT("0")));

	// Денег нет — молчим про деньги.
	const FString ItemsOnly = Screen->BuildLossPhrase(RespawnFormat, /*Items=*/3, /*Money=*/0).ToString();
	TestTrue(TEXT("Фраза без денег называет число предметов"), ItemsOnly.Contains(TEXT("3")));
	TestFalse(TEXT("Фраза без денег о монетах молчит"), ItemsOnly.Contains(TEXT("монет")));

	// Есть и то и другое — обе части и соединитель.
	const FString Both = Screen->BuildLossPhrase(RespawnFormat, /*Items=*/3, /*Money=*/25).ToString();
	TestTrue(TEXT("Полная фраза называет сумму"), Both.Contains(TEXT("25")));
	TestTrue(TEXT("Полная фраза называет число предметов"), Both.Contains(TEXT("3")));
	TestTrue(TEXT("Полная фраза соединяет части словом «и»"), Both.Contains(TEXT(" и ")));

	// Золотая кнопка собирается тем же механизмом — числа под кнопками читаются парой.
	const FString Saved = Screen->BuildLossPhrase(Screen->SaveBackpackSubFormat, /*Items=*/3, /*Money=*/20).ToString();
	TestTrue(TEXT("Подстрока золотой кнопки начинается с «Сохранишь»"), Saved.StartsWith(TEXT("Сохранишь ")));
	TestTrue(TEXT("Подстрока золотой кнопки называет сумму"), Saved.Contains(TEXT("20")));

	// Склонение слова по числу (русская культура). Если культура недоступна, проверку
	// пропускаем — она про язык, а не про логику экрана.
	FInternationalization& I18N = FInternationalization::Get();
	const FString PrevCulture = I18N.GetCurrentCulture()->GetName();
	if (I18N.SetCurrentCulture(TEXT("ru")))
	{
		TestEqual(TEXT("Одна монета"),
			Screen->BuildLossPhrase(RespawnFormat, 0, 1).ToString(), TEXT("Потеряешь 1 монету"));
		TestEqual(TEXT("Две монеты"),
			Screen->BuildLossPhrase(RespawnFormat, 0, 2).ToString(), TEXT("Потеряешь 2 монеты"));
		TestEqual(TEXT("Пять монет"),
			Screen->BuildLossPhrase(RespawnFormat, 0, 5).ToString(), TEXT("Потеряешь 5 монет"));
		TestEqual(TEXT("Один предмет"),
			Screen->BuildLossPhrase(RespawnFormat, 1, 0).ToString(), TEXT("Потеряешь 1 предмет"));
		TestEqual(TEXT("Три предмета"),
			Screen->BuildLossPhrase(RespawnFormat, 3, 0).ToString(), TEXT("Потеряешь 3 предмета"));
		TestEqual(TEXT("Семь предметов"),
			Screen->BuildLossPhrase(RespawnFormat, 7, 0).ToString(), TEXT("Потеряешь 7 предметов"));
		I18N.SetCurrentCulture(PrevCulture);
	}
	else
	{
		AddInfo(TEXT("Русская культура в этой сборке недоступна — проверку склонений пропустили."));
	}

	return true;
}

// ===========================================================================
// 12. Подготовка к публикационной сборке (Б5 задания издателя).
//     Сами тесты живут только в отладочных сборках, поэтому «отсутствие клавиш в Shipping»
//     проверяется не здесь, а сборкой с выключенным CONTRARY_WITH_QA_CHEATS. Здесь
//     закрепляем ДОГОВОР выключателя: он объявлен, в обычной сборке включён, и завязан
//     именно на признак публикационной сборки. Если кто-то случайно оторвёт одно от
//     другого, тест это покажет.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122ShippingSwitchTest,
	"ContrarySurvivor.Build122.Shipping.DebugSwitchContract", Build122TestFlags)

bool FBuild122ShippingSwitchTest::RunTest(const FString& Parameters)
{
#ifndef CONTRARY_WITH_QA_CHEATS
	AddError(TEXT("Выключатель отладочного харнесса CONTRARY_WITH_QA_CHEATS не объявлен"));
#else
	// Тест исполняется только в отладочной сборке, значит харнесс обязан быть включён.
	TestTrue(TEXT("В отладочной сборке отладочные клавиши на месте"), CONTRARY_WITH_QA_CHEATS != 0);
	// И обязан гаснуть ровно в публикационной: договор «ноль тогда и только тогда, когда Shipping».
	TestEqual(TEXT("Выключатель завязан на признак публикационной сборки"),
		static_cast<bool>(CONTRARY_WITH_QA_CHEATS), static_cast<bool>(!UE_BUILD_SHIPPING));
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122ChannelUrlFromConfigTest,
	"ContrarySurvivor.Build122.EndOfStory.ChannelUrlComesFromConfig", Build122TestFlags)

bool FBuild122ChannelUrlFromConfigTest::RunTest(const FString& Parameters)
{
	UEndOfStorySettings* Settings = GetMutableDefault<UEndOfStorySettings>();
	if (!TestNotNull(TEXT("Настройки карточки конца сюжета доступны"), Settings))
	{
		return false;
	}

	const FString Saved = Settings->ChannelUrl;

	// Пусто (так в конфиге сейчас) — кнопка «Написать мне» остаётся с прежним поведением.
	Settings->ChannelUrl = FString();
	TestTrue(TEXT("Пустой адрес отдаётся пустым"), UEndOfStorySettings::GetChannelUrl().IsEmpty());

	// Адрес вписали — тот же адрес и приходит к кнопке, лишние пробелы срезаются.
	Settings->ChannelUrl = TEXT("  https://t.me/contrary_survivor  ");
	TestEqual(TEXT("Адрес из настройки доходит до кнопки без пробелов"),
		UEndOfStorySettings::GetChannelUrl(), TEXT("https://t.me/contrary_survivor"));

	// Строка из одних пробелов адресом не считается.
	Settings->ChannelUrl = TEXT("   ");
	TestTrue(TEXT("Пробелы адресом не считаются"), UEndOfStorySettings::GetChannelUrl().IsEmpty());

	Settings->ChannelUrl = Saved; // не оставлять след другим тестам
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
