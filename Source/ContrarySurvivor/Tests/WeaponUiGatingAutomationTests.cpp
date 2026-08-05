// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест по находке лида 08-05 (после Б3): на телефоне счётчик патронов
// «12/48» и иконка пистолета над кнопкой «ОРУЖИЕ» показывались, хотя у игрока в руках был
// нож, а слот огнестрела в рюкзаке пустовал (скриншоты phone-b6-01/05). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.WeaponUiGating; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Оба живых места, что показывают патроны/иконку оружия (UPlayerStatsWidget::NativeTick,
// UTouchControlsWidget::UpdateWeaponIcon, а также легаси-Canvas AContrarySurvivorHUD::
// DrawPlayerStats), решают ОДНИМ И ТЕМ ЖЕ условием: Cast<ARangedWeapon>(Player->GetCurrentWeapon()).
// Виджеты сами headless не тикаются (нужен вьюпорт/PIE — как весь Slate UI проекта), но их
// ОБЩЕЕ условие — чистая функция от состояния игрока, и его можно и нужно проверить без UI.
// Player.StartWithoutFirearm (Build122AutomationTests.cpp) уже покрывает старт и переход
// НА пистолет; здесь — то, чего там не было: переход ОБРАТНО на нож (условие обязано снова
// стать «нет дальнобоя»), а не залипать в «однажды был пистолет — навсегда пистолет».
//
// НЕ покрывается headless: сам видимый счётчик/иконка на экране (Slate) — только живой PIE
// или осмотр на устройстве, как и весь остальной UI проекта.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ARangedWeapon.h"
#include "APistol.h"
#include "AMeleeWeapon.h"
#include "UInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags WeaponUiGatingTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки Build121TestWorld/Build122TestWorld/SaveLoadTestWorld —
// каждый файл теста держит свою, паттерн проекта).
namespace WeaponUiGatingTestWorld
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
}

// ===========================================================================
// Условие показа патронов/иконки оружия (Cast<ARangedWeapon>(GetCurrentWeapon())) отражает
// РЕАЛЬНОЕ оружие в руках на всех трёх шагах: старт (нож, условие ложно), после подбора и
// переключения на пистолет (условие истинно), после переключения ОБРАТНО на нож (условие
// снова ложно — не залипает в «однажды видели пистолет»).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponUiGatingRoundTripTest,
	"ContrarySurvivor.WeaponUiGating.RangedGateFollowsCurrentWeapon", WeaponUiGatingTestFlags)

bool FWeaponUiGatingRoundTripTest::RunTest(const FString& Parameters)
{
	UWorld* World = WeaponUiGatingTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	bool bOk = true;
	{
		APlayerCharacter* Player = WeaponUiGatingTestWorld::Spawn<APlayerCharacter>(World);
		UInventoryComponent* Inv = Player ? Player->GetInventory() : nullptr;
		if (Player && Inv)
		{
			// 1) Старт: нож в руках, дальнобоя нет — условие показа патронов/иконки ложно.
			TestNull(TEXT("Слот огнестрела на старте пуст"), Player->GetRangedWeaponInstance());
			TestFalse(TEXT("Старт: условие «дальнобой в руках» ложно (нож)"),
				Cast<ARangedWeapon>(Player->GetCurrentWeapon()) != nullptr);

			// 2) Подобрали пистолет (как из мешка/трупа) и переключились на него.
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			APistol* Bought = World->SpawnActor<APistol>(APistol::StaticClass(),
				FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Sp);
			TestNotNull(TEXT("Пистолет заспавнен"), Bought);
			if (Bought)
			{
				Inv->AddItem(Bought);
				TestTrue(TEXT("Пистолет занял слот огнестрела"), Player->TryAdoptRangedWeapon(Bought));
				Player->SwitchWeapon();
				TestTrue(TEXT("После подбора: условие «дальнобой в руках» истинно (пистолет)"),
					Cast<ARangedWeapon>(Player->GetCurrentWeapon()) != nullptr);

				// 3) Переключились ОБРАТНО на нож — условие обязано вернуться в «ложно», а
				//    не остаться залипшим в «когда-то видели пистолет» (ровно то, что
				//    наблюдалось на устройстве: патроны/иконка не пропадали).
				Player->SwitchWeapon();
				TestNull(TEXT("В руках снова НЕ дальнобойное оружие"),
					Cast<ARangedWeapon>(Player->GetCurrentWeapon()));
				TestFalse(TEXT("После возврата к ножу: условие «дальнобой в руках» снова ложно"),
					Cast<ARangedWeapon>(Player->GetCurrentWeapon()) != nullptr);
				TestNotNull(TEXT("Слот огнестрела остался занят (не потерян, просто не в руках)"),
					Player->GetRangedWeaponInstance());
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
	WeaponUiGatingTestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
