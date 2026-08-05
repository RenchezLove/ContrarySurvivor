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
//     (баг «Вода х3 продаётся по одной штуке»).
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
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Characters/MasterTrader.h"
#include "ContrarySurvivor/UI/ShopScreenWidget.h"
#include "Animation/Skeleton.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
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
		}
		else
		{
			bOk = false;
		}
	}
	Build122TestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
