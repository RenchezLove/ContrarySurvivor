// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты двустволки (поручение Рината 09.10.2026): выстрел дробью, свой вид патрона,
// два патрона в стволах, сохранение двух видов патронов. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Shotgun; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат:
//   - дробин за выстрел столько, сколько задано, и ни одна не выходит за конус разброса;
//   - в упор в цель приходит весь залп, на дистанции часть уходит мимо, дальше дальности
//     оружия не долетает ничего; урон наносится одним вызовом на цель (сумма её дробин);
//   - пистолет стреляет как раньше: одна пуля, один вызов урона;
//   - два выстрела — и стволы пусты, третьего нет без перезарядки;
//   - перезарядка, покупка и подбор различают вид патрона: дробь не уходит в пистолет;
//   - сохранение и «Продолжить» возвращают оба вида патронов и ружьё в слоте.
//
// ⚠ Точные числа баланса (урон, число дробин, угол) здесь НЕ закреплены — они черновые и
// правятся в чертеже; тесты читают их с оружия. Таблица предметов может как содержать строку
// дробового патрона, так и не содержать её — оба случая проходят одни и те же проверки.
// Слот сохранения ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter) — боевой слот не трогается.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CombatTestProbe.h"
#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "UInventoryComponent.h"
#include "AAmmoItem.h"
#include "APistol.h"
#include "AShotgun.h"
#include "ARangedWeapon.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags ShotgunTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки CombatAutomationTests.cpp — статики свои на файл).
namespace ShotgunTestWorld
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

	// Один кадр мира. Счётчик кадров движка двигаем сами — см. CombatTestWorld::TickWorld.
	static void TickWorld(UWorld* World, float DeltaSeconds)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, DeltaSeconds);
	}

	// Промотать игровое время (один кадр тестового мира даёт не больше 0,4 с).
	static void AdvanceTime(UWorld* World, float Seconds)
	{
		for (float Left = Seconds; Left > 0.0f; Left -= 0.25f)
		{
			TickWorld(World, 0.25f);
		}
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

	// Мишень: актор с блокирующим боксом размером с человека (как капсула врага, которая
	// блокирует канал видимости — по нему летят и пуля, и дробь).
	static AActor* SpawnTarget(UWorld* World, const FVector& Loc)
	{
		AActor* Target = Spawn<AActor>(World, Loc);
		if (!Target)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Target);
		Box->InitBoxExtent(FVector(34.0f, 34.0f, 88.0f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Target->SetRootComponent(Box);
		Box->RegisterComponent();
		Target->SetActorLocation(Loc);
		return Target;
	}

	// Сколько секунд ждать между выстрелами, чтобы точно вышла пауза оружия (с запасом —
	// скорострельность черновая и может стать ниже).
	static constexpr float BetweenShotsSeconds = 5.0f;

	// Число штук во всех пачках рюкзака, НЕ относящихся ни к одному из двух видов
	// (страховка: патрон не должен «потеряться» в третий вид).
	static int32 CountForeignAmmo(const APlayerCharacter* Player)
	{
		int32 Total = 0;
		const UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (!Inv)
		{
			return 0;
		}
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (const AAmmoItem* Ammo = Cast<AAmmoItem>(Item))
			{
				const FName Type = Ammo->GetAmmoType();
				if (Type != ContraryAmmo::DefaultType() && Type != ContraryAmmo::Shotgun12gType())
				{
					Total += Ammo->GetStackCount();
				}
			}
		}
		return Total;
	}
}

// ===========================================================================
// 1. Число дробин и конус разброса; пистолет остался одиночной пулей.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotgunPelletConeTest,
	"ContrarySurvivor.Shotgun.PelletCountAndSpreadStayInsideCone", ShotgunTestFlags)

bool FShotgunPelletConeTest::RunTest(const FString& Parameters)
{
	const AShotgun* ShotgunCDO = GetDefault<AShotgun>();
	const APistol* PistolCDO = GetDefault<APistol>();
	if (!TestNotNull(TEXT("Класс двустволки есть"), ShotgunCDO) || !TestNotNull(TEXT("Класс пистолета есть"), PistolCDO))
	{
		return false;
	}

	// Двустволка стреляет дробью, пистолет — нет (значения по умолчанию базы не тронуты).
	TestTrue(TEXT("Двустволка стреляет дробью"), ShotgunCDO->IsPelletWeapon());
	TestTrue(TEXT("У двустволки больше одной дробины"), ShotgunCDO->GetPelletsPerShot() > 1);
	TestTrue(TEXT("У двустволки ненулевой угол разброса"), ShotgunCDO->GetPelletSpreadAngle() > 0.0f);
	TestEqual(TEXT("В стволах двустволки два патрона"), ShotgunCDO->GetMaxAmmoInClip(), 2);
	TestFalse(TEXT("Пистолет дробью не стреляет"), PistolCDO->IsPelletWeapon());
	TestEqual(TEXT("У пистолета одна пуля за выстрел"), PistolCDO->GetPelletsPerShot(), 1);
	TestEqual(TEXT("У пистолета нет конуса дроби"), PistolCDO->GetPelletSpreadAngle(), 0.0f);

	// Залп в упор сильнее одного выстрела пистолета (смысл, а не точное число).
	const float Salvo = ShotgunCDO->GetDamage() * static_cast<float>(ShotgunCDO->GetPelletsPerShot());
	TestTrue(TEXT("Залп двустволки в упор сильнее выстрела пистолета"), Salvo > PistolCDO->GetDamage());
	TestTrue(TEXT("Одна дробина слабее пули пистолета"), ShotgunCDO->GetDamage() < PistolCDO->GetDamage());

	// Конус: много залпов в произвольном направлении, ни одна дробина не выходит за половину угла.
	const int32 Count = ShotgunCDO->GetPelletsPerShot();
	const float FullAngle = ShotgunCDO->GetPelletSpreadAngle();
	const FVector Aim = FVector(0.6f, -0.7f, 0.2f).GetSafeNormal();
	float MaxSeenDeg = 0.0f;
	bool bAllInside = true;
	bool bCountOk = true;
	bool bUnitOk = true;
	TArray<FVector> Dirs;
	for (int32 Salvos = 0; Salvos < 300; ++Salvos)
	{
		ARangedWeapon::BuildPelletDirections(Aim, Count, FullAngle, Dirs);
		bCountOk &= (Dirs.Num() == Count);
		for (const FVector& Dir : Dirs)
		{
			bUnitOk &= FMath::IsNearlyEqual(Dir.Size(), 1.0, 1.e-3);
			const float Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir, Aim), -1.0, 1.0)));
			MaxSeenDeg = FMath::Max(MaxSeenDeg, Deg);
			bAllInside &= (Deg <= FullAngle * 0.5f + 0.05f);
		}
	}
	TestTrue(TEXT("В каждом залпе ровно заданное число дробин"), bCountOk);
	TestTrue(TEXT("Направления дробин единичной длины"), bUnitOk);
	TestTrue(TEXT("Ни одна дробина не вышла за конус разброса"), bAllInside);
	TestTrue(TEXT("Дробь действительно разлетается (не одна линия)"), MaxSeenDeg > FullAngle * 0.25f);
	AddInfo(FString::Printf(TEXT("дробин %d, полный угол %.1f, наибольшее отклонение за 300 залпов %.2f"),
		Count, FullAngle, MaxSeenDeg));

	// Одна дробина без угла — это ровно линия прицеливания (путь обычной пули).
	ARangedWeapon::BuildPelletDirections(Aim, 1, 0.0f, Dirs);
	TestEqual(TEXT("Одна дробина без угла — одно направление"), Dirs.Num(), 1);
	if (Dirs.Num() == 1)
	{
		TestTrue(TEXT("Оно совпадает с линией прицеливания"), Dirs[0].Equals(Aim, 1.e-4));
	}
	return true;
}

// ===========================================================================
// 2. В упор попадает весь залп, на дистанции — часть, дальше дальности — ничего.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotgunSalvoByDistanceTest,
	"ContrarySurvivor.Shotgun.SalvoHitsAllCloseAndPartFar", ShotgunTestFlags)

bool FShotgunSalvoByDistanceTest::RunTest(const FString& Parameters)
{
	UWorld* World = ShotgunTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FVector Origin(0.0f, 0.0f, 100.0f);
	AShotgun* Shotgun = ShotgunTestWorld::Spawn<AShotgun>(World, Origin);
	if (!TestNotNull(TEXT("Двустволка заспавнена"), Shotgun))
	{
		ShotgunTestWorld::Destroy(World);
		return false;
	}

	// Дальность оружия читаем рефлексией (поле защищённое, отдельного геттера у базы нет).
	float WeaponRange = 0.0f;
	if (const FFloatProperty* RangeProp = FindFProperty<FFloatProperty>(AShotgun::StaticClass(), TEXT("Range")))
	{
		WeaponRange = RangeProp->GetPropertyValue_InContainer(Shotgun);
	}
	TestTrue(TEXT("Дальность двустволки задана"), WeaponRange > 300.0f);

	// Три мишени в трёх разных сторонах: вплотную, на дистанции и за пределом дальности.
	const float FarDistance = WeaponRange * 0.9f;
	AActor* CloseTarget = ShotgunTestWorld::SpawnTarget(World, Origin + FVector(150.0f, 0.0f, 0.0f));
	AActor* FarTarget = ShotgunTestWorld::SpawnTarget(World, Origin + FVector(0.0f, FarDistance, 0.0f));
	AActor* BeyondTarget = ShotgunTestWorld::SpawnTarget(World, Origin + FVector(-(WeaponRange + 250.0f), 0.0f, 0.0f));
	if (!CloseTarget || !FarTarget || !BeyondTarget)
	{
		AddError(TEXT("Не заспавнены мишени"));
		ShotgunTestWorld::Destroy(World);
		return false;
	}
	ShotgunTestWorld::TickWorld(World, 0.05f);

	const int32 Pellets = Shotgun->GetPelletsPerShot();
	TMap<AActor*, int32> Hits;
	TArray<FVector> Ends;

	// Вплотную: весь залп в мишени, раз за разом.
	bool bAllCloseHit = true;
	for (int32 i = 0; i < 20; ++i)
	{
		Shotgun->TracePellets(Origin, FVector(1.0f, 0.0f, 0.0f), Hits, Ends);
		bAllCloseHit &= (Ends.Num() == Pellets) && (Hits.FindRef(CloseTarget) == Pellets);
	}
	TestTrue(TEXT("В упор в мишень приходит весь залп"), bAllCloseHit);

	// На дистанции: часть дробин в мишени, часть мимо.
	int32 FarHits = 0;
	const int32 FarSalvos = 60;
	for (int32 i = 0; i < FarSalvos; ++i)
	{
		Shotgun->TracePellets(Origin, FVector(0.0f, 1.0f, 0.0f), Hits, Ends);
		FarHits += Hits.FindRef(FarTarget);
	}
	AddInfo(FString::Printf(TEXT("на дистанции %.0f попало %d дробин из %d"), FarDistance, FarHits, FarSalvos * Pellets));
	TestTrue(TEXT("На дистанции часть дробин попадает"), FarHits > 0);
	TestTrue(TEXT("На дистанции часть дробин уходит мимо"), FarHits < FarSalvos * Pellets);

	// За пределом дальности: не долетает ничего.
	int32 BeyondHits = 0;
	for (int32 i = 0; i < 20; ++i)
	{
		Shotgun->TracePellets(Origin, FVector(-1.0f, 0.0f, 0.0f), Hits, Ends);
		BeyondHits += Hits.FindRef(BeyondTarget);
	}
	TestEqual(TEXT("Дальше дальности оружия дробь не долетает"), BeyondHits, 0);

	ShotgunTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Выстрел: урон одним вызовом на цель (сумма дробин), пистолет — одна пуля, как раньше;
//    два выстрела — и стволы пусты.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotgunFireAndEmptyTest,
	"ContrarySurvivor.Shotgun.FireDamageAndTwoShotsThenEmpty", ShotgunTestFlags)

bool FShotgunFireAndEmptyTest::RunTest(const FString& Parameters)
{
	UWorld* World = ShotgunTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FVector Origin(0.0f, 0.0f, 100.0f);
	AShotgun* Shotgun = ShotgunTestWorld::Spawn<AShotgun>(World, Origin);
	APistol* Pistol = ShotgunTestWorld::Spawn<APistol>(World, Origin);
	AActor* Target = ShotgunTestWorld::SpawnTarget(World, Origin + FVector(150.0f, 0.0f, 0.0f));
	UCombatTestProbe* Probe = NewObject<UCombatTestProbe>();
	if (!Shotgun || !Pistol || !Target || !Probe)
	{
		AddError(TEXT("Не заспавнены оружие, мишень или зонд"));
		ShotgunTestWorld::Destroy(World);
		return false;
	}
	Probe->AddToRoot(); // мир тикает — зонд не должен уйти в сборку мусора посреди теста
	Target->OnTakeAnyDamage.AddDynamic(Probe, &UCombatTestProbe::HandleAnyDamage);
	ShotgunTestWorld::TickWorld(World, 0.05f);

	// Запас при ружье в игре сразу переливается в рюкзак (взятие в слот) — повторяем это,
	// иначе пустые стволы сами дозарядились бы из запаса.
	Shotgun->DrainReserveAmmo();
	Pistol->DrainReserveAmmo();

	// --- Пистолет: одна пуля, один вызов урона, минус один патрон (поведение не изменилось) ---
	const int32 PistolClipBefore = Pistol->GetCurrentAmmoInClip();
	Pistol->Fire(Target);
	TestEqual(TEXT("Пистолет: один вызов урона"), Probe->AnyDamageCount, 1);
	TestEqual(TEXT("Пистолет: урон одной пули"), Probe->LastAnyDamage, Pistol->GetDamage());
	TestEqual(TEXT("Пистолет: израсходован один патрон"), Pistol->GetCurrentAmmoInClip(), PistolClipBefore - 1);

	// --- Двустволка, первый выстрел: весь залп в мишени одним вызовом урона ---
	const int32 Pellets = Shotgun->GetPelletsPerShot();
	const float SalvoDamage = Shotgun->GetDamage() * static_cast<float>(Pellets);
	TestEqual(TEXT("Двустволка заряжена двумя патронами"), Shotgun->GetCurrentAmmoInClip(), 2);

	Shotgun->Fire(Target);
	TestEqual(TEXT("Залп: вылетело заданное число дробин"), Shotgun->GetLastShotPelletsFired(), Pellets);
	TestEqual(TEXT("Залп в упор: попали все дробины"), Shotgun->GetLastShotPelletsHit(), Pellets);
	TestEqual(TEXT("Залп: урон нанесён ОДНИМ вызовом, а не по разу на дробину"), Probe->AnyDamageCount, 2);
	TestEqual(TEXT("Залп в упор: урон = урон дробины x число дробин"), Probe->LastAnyDamage, SalvoDamage);
	TestEqual(TEXT("После первого выстрела остался один патрон"), Shotgun->GetCurrentAmmoInClip(), 1);

	// Сразу второй раз не стреляет — пауза между выстрелами.
	Shotgun->Fire(Target);
	TestEqual(TEXT("Без паузы второй выстрел не проходит"), Shotgun->GetCurrentAmmoInClip(), 1);
	TestEqual(TEXT("Без паузы урона нет"), Probe->AnyDamageCount, 2);

	// --- Второй выстрел после паузы: стволы пусты ---
	ShotgunTestWorld::AdvanceTime(World, ShotgunTestWorld::BetweenShotsSeconds);
	Shotgun->Fire(Target);
	TestEqual(TEXT("Второй выстрел прошёл"), Probe->AnyDamageCount, 3);
	TestEqual(TEXT("После двух выстрелов стволы пусты"), Shotgun->GetCurrentAmmoInClip(), 0);

	// --- Третьего выстрела нет: патронов в стволах и в запасе нет ---
	ShotgunTestWorld::AdvanceTime(World, ShotgunTestWorld::BetweenShotsSeconds);
	Shotgun->Fire(Target);
	TestEqual(TEXT("Третий выстрел без перезарядки не происходит"), Probe->AnyDamageCount, 3);
	TestEqual(TEXT("Стволы по-прежнему пусты"), Shotgun->GetCurrentAmmoInClip(), 0);
	TestFalse(TEXT("Стрелять нечем"), Shotgun->CanFire());

	// --- Перезарядка из запаса возвращает ровно два патрона ---
	Shotgun->AddReserveAmmo(5);
	Shotgun->Reload();
	TestEqual(TEXT("Перезарядка зарядила оба ствола"), Shotgun->GetCurrentAmmoInClip(), 2);
	TestEqual(TEXT("Из запаса ушло ровно два патрона"), Shotgun->GetCurrentAmmoReserve(), 3);

	Target->OnTakeAnyDamage.RemoveDynamic(Probe, &UCombatTestProbe::HandleAnyDamage);
	Probe->RemoveFromRoot();
	ShotgunTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 4. Вид патрона: рюкзак, покупка, подбор и перезарядка не путают дробь с пистолетными;
//    смена ствола в занятом слоте.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotgunAmmoTypeTest,
	"ContrarySurvivor.Shotgun.EachWeaponUsesItsOwnAmmoType", ShotgunTestFlags)

bool FShotgunAmmoTypeTest::RunTest(const FString& Parameters)
{
	UWorld* World = ShotgunTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FName Type9 = ContraryAmmo::DefaultType();
	const FName Type12 = ContraryAmmo::Shotgun12gType();
	const FVector Origin(0.0f, 0.0f, 100.0f);

	APlayerCharacter* Player = ShotgunTestWorld::Spawn<APlayerCharacter>(World, Origin);
	UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
	UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	AActor* Target = ShotgunTestWorld::SpawnTarget(World, Origin + FVector(250.0f, 0.0f, 0.0f));
	if (!Player || !Inv || !Stats || !Target)
	{
		AddError(TEXT("Не заспавнены игрок или мишень"));
		ShotgunTestWorld::Destroy(World);
		return false;
	}

	// Вид по умолчанию у оружия и пачки — прежний пистолетный.
	TestEqual(TEXT("Пистолет заряжается пистолетным патроном"), GetDefault<APistol>()->GetAmmoType(), Type9);
	TestEqual(TEXT("Двустволка заряжается дробовым патроном"), GetDefault<AShotgun>()->GetAmmoType(), Type12);
	TestEqual(TEXT("Базовая пачка — пистолетные патроны"), GetDefault<AAmmoItem>()->GetAmmoType(), Type9);
	TestEqual(TEXT("Пачка дроби — дробовые патроны"), GetDefault<AShotgunAmmoItem>()->GetAmmoType(), Type12);

	// --- Рюкзак ведёт два вида раздельно ---
	Player->AddAmmoToInventory(20); // без вида — прежний вызов, пистолетные
	Player->AddAmmoToInventory(6, Type12);
	TestEqual(TEXT("Пистолетных патронов 20 (вызов без вида)"), Player->GetReserveAmmoInInventory(), 20);
	TestEqual(TEXT("Пистолетных патронов 20 (вызов с видом)"), Player->GetReserveAmmoInInventory(Type9), 20);
	TestEqual(TEXT("Дробовых патронов 6"), Player->GetReserveAmmoInInventory(Type12), 6);

	// --- Покупка: позиция со строкой дробового патрона кладёт дробь, без строки — пистолетные ---
	Stats->AddMoney(1000.0f);
	FShopEntry Shells;
	Shells.DisplayName = Type12.ToString();
	Shells.Kind = EShopEntryKind::Ammo;
	Shells.AmmoAmount = 1;
	Shells.Price = 5.0f;
	Shells.ItemRow = Type12;
	TestTrue(TEXT("Покупка дроби прошла"), Player->Shop_BuyEntryQty(Shells, 4));
	FShopEntry OldAmmo;
	OldAmmo.DisplayName = TEXT("Патроны 9мм");
	OldAmmo.Kind = EShopEntryKind::Ammo;
	OldAmmo.AmmoAmount = 1;
	OldAmmo.Price = 2.0f;
	TestTrue(TEXT("Покупка пистолетных патронов прошла"), Player->Shop_BuyEntryQty(OldAmmo, 3));
	TestEqual(TEXT("После покупок дроби 10"), Player->GetReserveAmmoInInventory(Type12), 10);
	TestEqual(TEXT("После покупок пистолетных 23"), Player->GetReserveAmmoInInventory(Type9), 23);

	// --- Подбор: пачка дроби, созданная голым классом (как лут), попадает в свой вид ---
	if (AMasterInventoryItem* Loot = ContraryItems::SpawnItem(World, AShotgunAmmoItem::StaticClass(),
		FTransform(Origin), nullptr, [](AMasterInventoryItem& Item) { Item.StackCount = 5; }))
	{
		Loot->SetActorHiddenInGame(true);
		Loot->SetActorEnableCollision(false);
		Inv->AddItem(Loot);
	}
	else
	{
		AddError(TEXT("Не создана пачка дроби для подбора"));
	}
	TestEqual(TEXT("После подбора дроби 15"), Player->GetReserveAmmoInInventory(Type12), 15);
	TestEqual(TEXT("Подбор дроби не изменил пистолетные"), Player->GetReserveAmmoInInventory(Type9), 23);

	// --- Ружьё в слот: его запас приходит дробью, пистолетные не тронуты ---
	AShotgun* Shotgun = ShotgunTestWorld::Spawn<AShotgun>(World, Origin);
	if (!TestNotNull(TEXT("Двустволка заспавнена"), Shotgun))
	{
		ShotgunTestWorld::Destroy(World);
		return false;
	}
	const int32 ShotgunCameWith = Shotgun->GetCurrentAmmoReserve();
	Shotgun->SetActorHiddenInGame(true);
	Shotgun->SetActorEnableCollision(false);
	Inv->AddItem(Shotgun);
	TestTrue(TEXT("Двустволка заняла пустой слот"), Player->TryAdoptRangedWeapon(Shotgun));
	int32 Expected12 = 15 + ShotgunCameWith;
	TestEqual(TEXT("Запас ружья лёг в рюкзак дробью"), Player->GetReserveAmmoInInventory(Type12), Expected12);
	TestEqual(TEXT("Запас ружья не добавил пистолетных"), Player->GetReserveAmmoInInventory(Type9), 23);

	if (Player->GetCurrentWeapon() != Shotgun)
	{
		Player->SwitchWeapon();
	}
	TestTrue(TEXT("Двустволка в руках"), Player->GetCurrentWeapon() == Shotgun);

	// --- Два выстрела, перезарядка: из рюкзака уходит ровно две дроби, пистолетные целы ---
	Player->FireCurrentWeapon(Target);
	ShotgunTestWorld::AdvanceTime(World, ShotgunTestWorld::BetweenShotsSeconds);
	Player->FireCurrentWeapon(Target);
	TestEqual(TEXT("После двух выстрелов стволы пусты"), Shotgun->GetCurrentAmmoInClip(), 0);
	Player->ReloadCurrentWeapon();
	Expected12 -= 2;
	TestEqual(TEXT("Перезарядка зарядила оба ствола"), Shotgun->GetCurrentAmmoInClip(), 2);
	TestEqual(TEXT("Перезарядка взяла две дроби"), Player->GetReserveAmmoInInventory(Type12), Expected12);
	TestEqual(TEXT("Перезарядка ружья не тронула пистолетные"), Player->GetReserveAmmoInInventory(Type9), 23);
	TestEqual(TEXT("Запас при ружье после перезарядки пуст"), Shotgun->GetCurrentAmmoReserve(), 0);

	// --- Без дроби ружьё не заряжается пистолетными патронами ---
	const int32 AllShells = Player->TakeAmmoFromInventory(9999, Type12);
	TestEqual(TEXT("Изъята вся дробь и только она"), AllShells, Expected12);
	TestEqual(TEXT("Пистолетные патроны при изъятии дроби целы"), Player->GetReserveAmmoInInventory(Type9), 23);
	ShotgunTestWorld::AdvanceTime(World, ShotgunTestWorld::BetweenShotsSeconds);
	Player->FireCurrentWeapon(Target);
	ShotgunTestWorld::AdvanceTime(World, ShotgunTestWorld::BetweenShotsSeconds);
	Player->FireCurrentWeapon(Target);
	Player->ReloadCurrentWeapon();
	TestEqual(TEXT("Без дроби стволы остаются пустыми"), Shotgun->GetCurrentAmmoInClip(), 0);
	TestEqual(TEXT("Пистолетные патроны в ружьё не ушли"), Player->GetReserveAmmoInInventory(Type9), 23);
	Player->AddAmmoToInventory(4, Type12);

	// --- Второй ствол другого вида (10.10.2026, два слота огнестрела): пистолет встаёт в СВОЙ
	// слот рядом с ружьём, ружьё никуда не уходит и остаётся в руках. Раньше слот был один и
	// пистолет вытеснял ружьё в рюкзак — это поведение намеренно заменено. ---
	APistol* Pistol = ShotgunTestWorld::Spawn<APistol>(World, Origin);
	if (!TestNotNull(TEXT("Пистолет заспавнен"), Pistol))
	{
		ShotgunTestWorld::Destroy(World);
		return false;
	}
	const int32 PistolCameWith = Pistol->GetCurrentAmmoReserve();
	Pistol->SetActorHiddenInGame(true);
	Pistol->SetActorEnableCollision(false);
	Inv->AddItem(Pistol);
	TestTrue(TEXT("Пистолет встаёт в свой слот при занятом слоте ружья"), Player->TryAdoptRangedWeapon(Pistol));
	TestTrue(TEXT("В слоте короткого оружия пистолет"), Player->GetFirearmInSlot(EFirearmSlot::Sidearm) == Pistol);
	TestTrue(TEXT("Ружьё осталось в слоте длинного оружия"), Player->GetFirearmInSlot(EFirearmSlot::Long) == Shotgun);
	TestTrue(TEXT("В руках по-прежнему ружьё"), Player->GetCurrentWeapon() == Shotgun);
	TestFalse(TEXT("Ружьё в рюкзак не ушло"), Inv->GetInventoryItems().Contains(Shotgun));
	Player->SwitchWeapon();
	TestTrue(TEXT("Кнопка смены оружия берёт пистолет"), Player->GetCurrentWeapon() == Pistol);
	TestFalse(TEXT("Пистолета в рюкзаке больше нет"), Inv->GetInventoryItems().Contains(Pistol));
	const int32 Expected9 = 23 + PistolCameWith;
	TestEqual(TEXT("Запас пистолета лёг пистолетными патронами"), Player->GetReserveAmmoInInventory(Type9), Expected9);
	TestEqual(TEXT("Запас пистолета не добавил дроби"), Player->GetReserveAmmoInInventory(Type12), 4);

	// --- Пистолет расходует и заряжает только свои патроны ---
	const int32 PistolClip = Pistol->GetCurrentAmmoInClip();
	Player->FireCurrentWeapon(Target);
	TestEqual(TEXT("Пистолет израсходовал один патрон"), Pistol->GetCurrentAmmoInClip(), PistolClip - 1);
	Player->ReloadCurrentWeapon();
	TestEqual(TEXT("Пистолет дозаряжен"), Pistol->GetCurrentAmmoInClip(), PistolClip);
	TestEqual(TEXT("Перезарядка пистолета взяла один свой патрон"), Player->GetReserveAmmoInInventory(Type9), Expected9 - 1);
	TestEqual(TEXT("Перезарядка пистолета не тронула дробь"), Player->GetReserveAmmoInInventory(Type12), 4);

	TestEqual(TEXT("Патронов неизвестного вида в рюкзаке нет"), ShotgunTestWorld::CountForeignAmmo(Player), 0);

	ShotgunTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 5. Сохранение и «Продолжить»: оба вида патронов и ружьё в слоте возвращаются.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotgunSaveLoadAmmoTypesTest,
	"ContrarySurvivor.Shotgun.SaveLoadKeepsBothAmmoTypes", ShotgunTestFlags)

bool FShotgunSaveLoadAmmoTypesTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContraryShotgunQA");
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0); // хвост прошлого прогона
	}

	UWorld* World = ShotgunTestWorld::Create();
	if (!World)
	{
		AddError(TEXT("Не создан тестовый мир"));
		return false;
	}

	const FName Type9 = ContraryAmmo::DefaultType();
	const FName Type12 = ContraryAmmo::Shotgun12gType();
	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	// ⛔ Игрок появляется ВДАЛИ от костра: рядом с костром автосохранение сработало бы прямо
	// при появлении — раньше, чем тест переключит игрока на свой слот, то есть в боевой слот.
	const FVector PlayerLoc(4000.0f, 0.0f, 100.0f);
	const FVector NearCampfireLoc = CampfireLoc + FVector(100.0f, 0.0f, 0.0f);
	if (ACampfire* Campfire = ShotgunTestWorld::Spawn<ACampfire>(World, CampfireLoc))
	{
		// Антиспам автосохранения костра — в ноль: «сессии» теста идут в одном времени мира.
		if (FFloatProperty* CooldownProp = FindFProperty<FFloatProperty>(ACampfire::StaticClass(), TEXT("AutoSaveCooldown")))
		{
			CooldownProp->SetPropertyValue_InContainer(Campfire, 0.0f);
		}
	}

	// --- Сессия 1: два вида патронов без оружия — сохранение ---
	{
		ASaveTestPlayerCharacter* P1 = ShotgunTestWorld::Spawn<ASaveTestPlayerCharacter>(World, PlayerLoc);
		if (!P1)
		{
			AddError(TEXT("Не заспавнен игрок сессии 1"));
			ShotgunTestWorld::Destroy(World);
			return false;
		}
		P1->UseTestSaveSlot(Slot);
		P1->AddAmmoToInventory(21, Type9);
		P1->AddAmmoToInventory(7, Type12);
		P1->SetActorLocation(NearCampfireLoc, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Сохранение сессии 1 прошло"), P1->SaveGame());
		P1->Destroy();
	}

	// --- Сессия 2: «Продолжить» возвращает оба вида ровно; берём ружьё в слот и сохраняем ---
	int32 ShellsWithShotgun = 0;
	{
		ASaveTestPlayerCharacter* P2 = ShotgunTestWorld::Spawn<ASaveTestPlayerCharacter>(World, PlayerLoc);
		if (!P2)
		{
			AddError(TEXT("Не заспавнен игрок сессии 2"));
			ShotgunTestWorld::Destroy(World);
			return false;
		}
		P2->UseTestSaveSlot(Slot);
		TestTrue(TEXT("«Продолжить» сессии 2 отработал"), P2->LoadGameForContinue());
		TestEqual(TEXT("Пистолетные патроны вернулись ровно"), P2->GetReserveAmmoInInventory(Type9), 21);
		TestEqual(TEXT("Дробовые патроны вернулись ровно"), P2->GetReserveAmmoInInventory(Type12), 7);
		TestEqual(TEXT("Патроны не ушли в неизвестный вид"), ShotgunTestWorld::CountForeignAmmo(P2), 0);

		AShotgun* Shotgun = ShotgunTestWorld::Spawn<AShotgun>(World, PlayerLoc);
		if (Shotgun && P2->GetInventory())
		{
			Shotgun->SetActorHiddenInGame(true);
			Shotgun->SetActorEnableCollision(false);
			P2->GetInventory()->AddItem(Shotgun);
			TestTrue(TEXT("Двустволка взята в слот"), P2->TryAdoptRangedWeapon(Shotgun));
		}
		else
		{
			AddError(TEXT("Не заспавнена двустволка"));
		}
		ShellsWithShotgun = P2->GetReserveAmmoInInventory(Type12);
		TestTrue(TEXT("Сохранение сессии 2 прошло"), P2->SaveGame());
		P2->Destroy();
	}

	// --- Сессия 3: ружьё вернулось в слот со своим видом патрона, патроны на месте ---
	{
		ASaveTestPlayerCharacter* P3 = ShotgunTestWorld::Spawn<ASaveTestPlayerCharacter>(World, PlayerLoc);
		if (!P3)
		{
			AddError(TEXT("Не заспавнен игрок сессии 3"));
			ShotgunTestWorld::Destroy(World);
			return false;
		}
		P3->UseTestSaveSlot(Slot);
		TestTrue(TEXT("«Продолжить» сессии 3 отработал"), P3->LoadGameForContinue());

		const ARangedWeapon* Restored = Cast<ARangedWeapon>(P3->GetRangedWeaponInstance());
		TestNotNull(TEXT("Ствол вернулся в слот"), Restored);
		if (Restored)
		{
			TestTrue(TEXT("В слоте именно двустволка"), Restored->IsA(AShotgun::StaticClass()));
			TestEqual(TEXT("Двустволка по-прежнему заряжается дробью"), Restored->GetAmmoType(), Type12);
			TestTrue(TEXT("Двустволка стреляет дробью и после загрузки"), Restored->IsPelletWeapon());
		}
		TestEqual(TEXT("Пистолетные патроны на месте"), P3->GetReserveAmmoInInventory(Type9), 21);
		// Восстановленный ствол заново отдаёт свой запас пачкой (так устроено и у пистолета),
		// поэтому дроби не меньше, чем было сохранено.
		TestTrue(TEXT("Дробовые патроны на месте"), P3->GetReserveAmmoInInventory(Type12) >= ShellsWithShotgun);
		TestEqual(TEXT("Патроны не ушли в неизвестный вид"), ShotgunTestWorld::CountForeignAmmo(P3), 0);
		P3->Destroy();
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	ShotgunTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
