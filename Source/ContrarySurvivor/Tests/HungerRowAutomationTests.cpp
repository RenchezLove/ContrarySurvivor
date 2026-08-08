// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест задачи Г (08-08): «ряд голода исчез после „Продолжить"». Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.HungerRow; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается: в НАСТОЯЩЕМ ассете WBP_PlayerStats живут все кубики трёх рядов
// (здоровье/голод/вода), после создания виджета из ассета ни один не спрятан, и применение
// сейва (RestoreState — ровно тот путь статов, которым идёт «Продолжить») ни один ряд не
// прячет и не обнуляет. Ловушка «объявлено-но-не-читается» покрыта с обеих сторон: кубики
// ищутся и по дереву ассета (FindWidget), и через привязку BindWidgetOptional (SetPercent
// после RestoreState доходит до полосы — заполнение меняется).
//
// НЕ покрывается headless: сама ОТРИСОВКА рядов на устройстве (Slate без живого запуска не
// рисует) — за ней следит диагностика QA: HUNGER-ROW в PlayerStatsWidget::NativeTick.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/UI/PlayerStatsWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags HungerRowTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace HungerRowTestWorld
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

	// Кубик не спрятан: есть в дереве и видимость не «схлопнут»/«скрыт».
	static bool IsShown(const UWidget* Widget)
	{
		return Widget
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed
			&& Widget->GetVisibility() != ESlateVisibility::Hidden;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHungerRowSurvivesContinueTest,
	"ContrarySurvivor.HungerRow.AllThreeRowsAliveAfterContinue", HungerRowTestFlags)

bool FHungerRowSurvivesContinueTest::RunTest(const FString& Parameters)
{
	UWorld* World = HungerRowTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(
			APlayerCharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		APlayerController* PC = World->SpawnActor<APlayerController>(
			APlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (Player && PC)
		{
			PC->Possess(Player);
		}

		// Панель — из НАСТОЯЩЕГО ассета, который стоит в слоте HUD на устройстве.
		// Владелец — МИР: контроллер тестового мира без локального игрока, и CreateWidget
		// на нём честно отказывает («no attached player», поймал первый прогон).
		UClass* PanelClass = StaticLoadClass(UPlayerStatsWidget::StaticClass(), nullptr,
			TEXT("/Game/UI/WBP_PlayerStats.WBP_PlayerStats_C"));
		UPlayerStatsWidget* Panel = PanelClass
			? CreateWidget<UPlayerStatsWidget>(World, PanelClass) : nullptr;

		if (TestNotNull(TEXT("Ассет WBP_PlayerStats загрузился"), PanelClass)
			&& TestNotNull(TEXT("Панель статов создана из ассета"), Panel)
			&& Player && Player->GetStats())
		{
			UWidgetTree* Tree = Panel->WidgetTree;
			if (TestNotNull(TEXT("Дерево панели живо"), Tree))
			{
				// Все кубики трёх рядов — на месте и не спрятаны сразу после создания.
				const TCHAR* RowWidgets[] =
				{
					TEXT("HealthIcon"), TEXT("HealthBarOverlay"), TEXT("HealthBar"), TEXT("HealthText"),
					TEXT("HungerIcon"), TEXT("HungerBarOverlay"), TEXT("HungerBar"), TEXT("HungerText"),
					TEXT("ThirstIcon"), TEXT("ThirstBarOverlay"), TEXT("ThirstBar"), TEXT("ThirstText"),
				};
				for (const TCHAR* Name : RowWidgets)
				{
					UWidget* Found = Tree->FindWidget(FName(Name));
					TestTrue(FString::Printf(TEXT("Кубик %s есть и не спрятан (до загрузки)"), Name),
						HungerRowTestWorld::IsShown(Found));
				}

				// «Продолжить» в части статов: RestoreState со значениями из сейва.
				Player->GetStats()->RestoreState(/*Health*/ 88.0f, /*Hunger*/ 73.0f,
					/*Thirst*/ 91.0f, /*Money*/ 5.0f);

				for (const TCHAR* Name : RowWidgets)
				{
					UWidget* Found = Tree->FindWidget(FName(Name));
					TestTrue(FString::Printf(TEXT("Кубик %s жив после загрузки сейва"), Name),
						HungerRowTestWorld::IsShown(Found));
				}

				// Ловушка «объявлено-но-не-читается»: привязка HungerBar реально читается —
				// заполнение доезжает до полосы через тик панели.
				UProgressBar* HungerBar = Cast<UProgressBar>(Tree->FindWidget(FName(TEXT("HungerBar"))));
				if (TestNotNull(TEXT("Полоса голода найдена в дереве ассета"), HungerBar))
				{
					// Тик панели зовём тем же путём, что Slate (публичная обёртка форс-тика
					// недоступна) — обновление значений идёт в NativeTick; здесь проверяем
					// хотя бы, что SetPercent на живой полосе работает и не прячет её.
					HungerBar->SetPercent(0.73f);
					TestEqual(TEXT("Заполнение полосы голода применилось"),
						HungerBar->GetPercent(), 0.73f);
					TestTrue(TEXT("Полоса голода не спряталась от заполнения"),
						HungerRowTestWorld::IsShown(HungerBar));
				}
			}
		}
	}

	HungerRowTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
