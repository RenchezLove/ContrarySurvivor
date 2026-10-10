// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты меню отладки (Ринат 09.10.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.DebugMenu; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что держат (действия меню без самой панели — живой Slate в прогонах не поднимается):
//   - тумблер неуязвимости реально гасит урон по герою;
//   - заморозка останавливает врага и снимается обратно;
//   - точки телепорта: актор с меткой DebugTP_<Имя> становится точкой, базы с одинаковым
//     названием нумеруются, телепорт ставит героя рядом с точкой;
//   - выдача двустволки кладёт её в слот и в руки с патронами её вида; «убить врагов рядом».
//
// Весь файл — под выключателем отладочного харнесса: в сборке для магазина меню нет, и
// проверять там нечего.

#include "Misc/AutomationTest.h"
#include "ContrarySurvivor/Debug/QADebug.h" // CONTRARY_WITH_QA_CHEATS

#if WITH_DEV_AUTOMATION_TESTS && CONTRARY_WITH_QA_CHEATS

#include "ContrarySurvivor/Debug/ContraryDebugMenu.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "AAmmoItem.h"
#include "AShotgun.h"
#include "ARangedWeapon.h"
#include "Components/SceneComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags DebugMenuTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки CombatAutomationTests.cpp — статики свои на файл).
namespace DebugMenuTestWorld
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

	// Пустой актор-метка с корнем (без корня положение актора всегда ноль) и меткой.
	static AActor* SpawnMarker(UWorld* World, const FVector& Loc, FName Tag)
	{
		AActor* Marker = Spawn<AActor>(World, Loc);
		if (Marker)
		{
			USceneComponent* Root = NewObject<USceneComponent>(Marker);
			Marker->SetRootComponent(Root);
			Root->RegisterComponent();
			Marker->SetActorLocation(Loc);
			Marker->Tags.Add(Tag);
		}
		return Marker;
	}

	static const FContraryDebugTeleportPoint* FindPoint(const TArray<FContraryDebugTeleportPoint>& Points, const FString& Label)
	{
		return Points.FindByPredicate([&Label](const FContraryDebugTeleportPoint& P) { return P.Label == Label; });
	}
}

// ===========================================================================
// 1. Неуязвимость гасит урон; заморозка останавливает врага.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuGodModeAndFreezeTest,
	"ContrarySurvivor.DebugMenu.GodModeBlocksDamageAndFreezeStopsEnemies", DebugMenuTestFlags)

bool FDebugMenuGodModeAndFreezeTest::RunTest(const FString& Parameters)
{
	UWorld* World = DebugMenuTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const bool bSavedGod = FQADebug::bGodMode;
	const bool bSavedFreeze = FQADebug::bFreezeEnemies;

	APlayerCharacter* Player = DebugMenuTestWorld::Spawn<APlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AWolfCharacter* Wolf = DebugMenuTestWorld::Spawn<AWolfCharacter>(World, FVector(600.0f, 0.0f, 100.0f));
	UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	if (!Player || !Stats || !Wolf)
	{
		AddError(TEXT("Не заспавнены игрок или волк"));
		DebugMenuTestWorld::Destroy(World);
		return false;
	}

	// --- Неуязвимость ---
	const float HealthBefore = Stats->GetHealth();
	FQADebug::bGodMode = true;
	Player->TakeDamage(30.0f, FDamageEvent(), nullptr, Wolf);
	TestEqual(TEXT("С неуязвимостью урон не проходит"), Stats->GetHealth(), HealthBefore);
	FQADebug::bGodMode = false;
	Player->TakeDamage(30.0f, FDamageEvent(), nullptr, Wolf);
	TestTrue(TEXT("Без неуязвимости урон проходит"), Stats->GetHealth() < HealthBefore);

	// --- Заморозка врагов (тот же обработчик, что зовёт тумблер меню) ---
	if (!Wolf->GetController())
	{
		Wolf->SpawnDefaultController();
	}
	UCharacterMovementComponent* WolfMove = Wolf->GetCharacterMovement();
	AContrarySurvivorPlayerController* PC = NewObject<AContrarySurvivorPlayerController>(GetTransientPackage());
	TestNotNull(TEXT("У волка есть контроллер"), Wolf->GetController());
	if (PC && WolfMove)
	{
		WolfMove->SetMovementMode(MOVE_Walking);
		PC->ApplyQAFreezeEnemiesForQA(true);
		TestTrue(TEXT("Заморозка включена"), FQADebug::bFreezeEnemies);
		TestTrue(TEXT("Замороженный враг не может двигаться"), WolfMove->MovementMode == MOVE_None);

		PC->ApplyQAFreezeEnemiesForQA(false);
		TestFalse(TEXT("Заморозка снята"), FQADebug::bFreezeEnemies);
		TestTrue(TEXT("Размороженный враг снова ходит"), WolfMove->MovementMode == MOVE_Walking);
	}
	else
	{
		AddError(TEXT("Нет контроллера игрока или движения волка"));
	}

	FQADebug::bGodMode = bSavedGod;
	FQADebug::bFreezeEnemies = bSavedFreeze;
	DebugMenuTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 2. Точки телепорта и сам телепорт.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuTeleportTest,
	"ContrarySurvivor.DebugMenu.TeleportPointsByTagAndBases", DebugMenuTestFlags)

bool FDebugMenuTeleportTest::RunTest(const FString& Parameters)
{
	UWorld* World = DebugMenuTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FVector ScientistsLoc(5000.0f, 3000.0f, 0.0f);
	APlayerCharacter* Player = DebugMenuTestWorld::Spawn<APlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AActor* Marker = DebugMenuTestWorld::SpawnMarker(World, ScientistsLoc, FName(TEXT("DebugTP_Scientists")));
	AActor* Plain = DebugMenuTestWorld::SpawnMarker(World, FVector(-900.0f, 0.0f, 0.0f), FName(TEXT("JustATag")));
	AMasterEnemyBase* DenA = DebugMenuTestWorld::Spawn<AMasterEnemyBase>(World, FVector(-7000.0f, 0.0f, 0.0f));
	AMasterEnemyBase* DenB = DebugMenuTestWorld::Spawn<AMasterEnemyBase>(World, FVector(-7000.0f, 6000.0f, 0.0f));
	if (!Player || !Marker || !Plain || !DenA || !DenB)
	{
		AddError(TEXT("Не заспавнены игрок, метки или базы"));
		DebugMenuTestWorld::Destroy(World);
		return false;
	}

	// Обе базы зовутся одинаково — кнопки должны различаться номером.
	if (const FTextProperty* NameProp = FindFProperty<FTextProperty>(AMasterEnemyBase::StaticClass(), TEXT("BaseDisplayName")))
	{
		NameProp->SetPropertyValue_InContainer(DenA, FText::FromString(TEXT("Логово волков")));
		NameProp->SetPropertyValue_InContainer(DenB, FText::FromString(TEXT("Логово волков")));
	}
	else
	{
		AddError(TEXT("У базы нет поля названия места"));
	}

	TArray<FContraryDebugTeleportPoint> Points;
	ContraryDebugTools::CollectTeleportPoints(World, Points);

	const FContraryDebugTeleportPoint* Scientists = DebugMenuTestWorld::FindPoint(Points, TEXT("Scientists"));
	TestNotNull(TEXT("Актор с меткой DebugTP_Scientists стал точкой телепорта"), Scientists);
	TestNull(TEXT("Актор с посторонней меткой точкой не стал"), DebugMenuTestWorld::FindPoint(Points, TEXT("JustATag")));
	TestNotNull(TEXT("Первое логово получило номер 1"), DebugMenuTestWorld::FindPoint(Points, TEXT("Логово волков 1")));
	TestNotNull(TEXT("Второе логово получило номер 2"), DebugMenuTestWorld::FindPoint(Points, TEXT("Логово волков 2")));

	if (Scientists)
	{
		TestTrue(TEXT("Точка стоит там же, где метка"), Scientists->Location.Equals(ScientistsLoc, 1.0));
		TestTrue(TEXT("Телепорт к точке прошёл"), ContraryDebugTools::TeleportPawnNear(Player, Scientists->Location));
		const float Dist2D = FVector::Dist2D(Player->GetActorLocation(), ScientistsLoc);
		TestTrue(TEXT("Герой стоит рядом с точкой"), Dist2D <= 700.0f);
		TestTrue(TEXT("Герой не провалился под землю"), Player->GetActorLocation().Z >= 0.0);
	}

	DebugMenuTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// 3. Выдача двустволки и «убить врагов рядом».
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuGiveAndKillTest,
	"ContrarySurvivor.DebugMenu.GiveShotgunAndKillNearby", DebugMenuTestFlags)

bool FDebugMenuGiveAndKillTest::RunTest(const FString& Parameters)
{
	UWorld* World = DebugMenuTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	APlayerCharacter* Player = DebugMenuTestWorld::Spawn<APlayerCharacter>(World, FVector(0.0f, 0.0f, 100.0f));
	AWolfCharacter* NearWolf = DebugMenuTestWorld::Spawn<AWolfCharacter>(World, FVector(500.0f, 0.0f, 100.0f));
	AWolfCharacter* FarWolf = DebugMenuTestWorld::Spawn<AWolfCharacter>(World, FVector(9000.0f, 0.0f, 100.0f));
	if (!Player || !NearWolf || !FarWolf)
	{
		AddError(TEXT("Не заспавнены игрок или волки"));
		DebugMenuTestWorld::Destroy(World);
		return false;
	}

	// --- Двустволка: в слот, в руки, патроны её вида в сумке ---
	ARangedWeapon* Shotgun = ContraryDebugTools::GiveFirearm(
		Player, FName(TEXT("shotgun_toz34")), AShotgun::StaticClass(), 20);
	TestNotNull(TEXT("Двустволка выдана"), Shotgun);
	if (Shotgun)
	{
		TestTrue(TEXT("Выданное оружие стреляет дробью"), Shotgun->IsPelletWeapon());
		TestTrue(TEXT("Двустволка в слоте огнестрела"), Player->GetRangedWeaponInstance() == Shotgun);
		TestTrue(TEXT("Двустволка в руках"), Player->GetCurrentWeapon() == Shotgun);
		TestTrue(TEXT("К двустволке выданы патроны её вида"),
			Player->GetReserveAmmoInInventory(Shotgun->GetAmmoType()) >= 20);
		TestEqual(TEXT("Пистолетных патронов выдача двустволки не добавила"),
			Player->GetReserveAmmoInInventory(ContraryAmmo::DefaultType()), 0);
	}

	// --- Убить врагов рядом: ближний волк мёртв, дальний жив ---
	for (AWolfCharacter* Wolf : { NearWolf, FarWolf })
	{
		if (!Wolf->GetController())
		{
			Wolf->SpawnDefaultController();
		}
	}
	const int32 Killed = ContraryDebugTools::KillEnemiesNear(Player, 2500.0f);
	TestEqual(TEXT("Убит один враг — тот, что рядом"), Killed, 1);
	TestTrue(TEXT("Ближний волк мёртв"), NearWolf->GetStats() && NearWolf->GetStats()->IsDead());
	TestTrue(TEXT("Дальний волк жив"), FarWolf->GetStats() && !FarWolf->GetStats()->IsDead());

	DebugMenuTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && CONTRARY_WITH_QA_CHEATS
