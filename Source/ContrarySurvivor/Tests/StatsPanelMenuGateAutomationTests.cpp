// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест регресса 08-09: «полос здоровья, голода, жажды и денег в игре
// НЕТ ВООБЩЕ». Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.StatsPanelGate; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается: панель статов, спрятанная на время главного меню, ВОЗВРАЩАЕТСЯ
// после его закрытия, и при этом САМ виджет ни на миг не становится невидимым для Slate.
//
// Почему второе так же важно, как первое (корень регресса, найден 08-09):
//   * Slate зовёт Tick виджета только при отрисовке (UE 5.5, SWidget::Paint — единственное
//     место вызова Tick; в режиме инвалидации — FSlateInvalidationRoot::PaintFastPath_
//     UpdateNextWidget, который явно пропускает всё, у чего Visibility.IsVisible() == false);
//   * значит SetVisibility(Collapsed) на самом UUserWidget убивает его NativeTick, и виджет
//     уже НИКОГДА не разворачивается обратно — ровно это и произошло на телефоне: панель
//     свернула себя в кадре 1 при открытии меню и не вернулась после «Продолжить»;
//   * лечение: прячется корень ДЕРЕВА (база USelfHidingWidget), а решение принимает HUD
//     снаружи (AContrarySurvivorHUD::ApplyMainMenuGateToStatsPanel).
//
// НЕ покрывается headless: реальная отрисовка на устройстве — за ней следит диагностика
// QA: STATS-PANEL, которую HUD пишет каждую секунду и при каждой смене состояния.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/PlayerStatsWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags StatsPanelGateTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace StatsPanelGateTest
{
	static UWorld* CreateWorld()
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

	static void DestroyWorld(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}

	// Виден ли виджет для Slate: только видимые получают Paint, а вместе с ним и Tick.
	static bool IsTickableBySlate(const UWidget* Widget)
	{
		return Widget
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed
			&& Widget->GetVisibility() != ESlateVisibility::Hidden;
	}

	// Панель из настоящего ассета, если он на диске; иначе — из кодового класса с
	// самодельным корнем дерева (тест обязан работать и в копии проекта без ассетов).
	static UPlayerStatsWidget* CreatePanel(UWorld* World, bool& bOutFromAsset)
	{
		UClass* PanelClass = StaticLoadClass(UPlayerStatsWidget::StaticClass(), nullptr,
			TEXT("/Game/UI/WBP_PlayerStats.WBP_PlayerStats_C"));
		bOutFromAsset = (PanelClass != nullptr);

		UPlayerStatsWidget* Panel = CreateWidget<UPlayerStatsWidget>(World,
			PanelClass ? PanelClass : UPlayerStatsWidget::StaticClass());
		if (!Panel || !Panel->WidgetTree)
		{
			return Panel;
		}
		if (!Panel->WidgetTree->RootWidget)
		{
			// У кодового класса дерева нет — собираем минимальное, иначе прятать нечего.
			Panel->WidgetTree->RootWidget =
				Panel->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		}
		return Panel;
	}
}

// --- Панель возвращается после закрытия меню --------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsPanelReturnsAfterMenuTest,
	"ContrarySurvivor.StatsPanelGate.PanelReturnsAfterMenuClosed", StatsPanelGateTestFlags)

bool FStatsPanelReturnsAfterMenuTest::RunTest(const FString& Parameters)
{
	UWorld* World = StatsPanelGateTest::CreateWorld();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		bool bFromAsset = false;
		UPlayerStatsWidget* Panel = StatsPanelGateTest::CreatePanel(World, bFromAsset);
		if (TestNotNull(TEXT("Панель статов создана"), Panel)
			&& TestNotNull(TEXT("Дерево панели живо"), Panel->WidgetTree.Get()))
		{
			UWidget* Root = Panel->WidgetTree->RootWidget;
			if (TestNotNull(TEXT("Корень дерева панели есть — есть что прятать"), Root))
			{
				const ESlateVisibility ShownRootVisibility = Root->GetVisibility();

				TestTrue(TEXT("До меню содержимое панели показано"), Panel->IsStatsContentVisible());

				// --- меню открылось ---
				Panel->ApplyMainMenuGate(/*bMainMenuOnScreen=*/true);

				TestFalse(TEXT("Меню на экране — содержимое панели спрятано"),
					Panel->IsStatsContentVisible());
				TestEqual(TEXT("Спрятан именно корень дерева"),
					Root->GetVisibility(), ESlateVisibility::Collapsed);

				// ⛔ Главный смысл теста: САМА панель обязана остаться видимой для Slate,
				// иначе она перестанет тикать и уже не оживёт (регресс 08-09).
				TestTrue(TEXT("Сама панель не свернула себя и продолжает получать тик"),
					StatsPanelGateTest::IsTickableBySlate(Panel));

				// --- меню закрылось ---
				Panel->ApplyMainMenuGate(/*bMainMenuOnScreen=*/false);

				TestTrue(TEXT("Меню закрылось — содержимое панели вернулось"),
					Panel->IsStatsContentVisible());
				TestEqual(TEXT("Видимость корня восстановлена ровно та, что была"),
					Root->GetVisibility(), ShownRootVisibility);
				TestTrue(TEXT("Панель по-прежнему видима для Slate"),
					StatsPanelGateTest::IsTickableBySlate(Panel));

				// Повторный цикл: гейт идемпотентен, состояние не «залипает».
				Panel->ApplyMainMenuGate(true);
				Panel->ApplyMainMenuGate(true);
				Panel->ApplyMainMenuGate(false);
				TestTrue(TEXT("После второго цикла панель снова видна"),
					Panel->IsStatsContentVisible());
				TestEqual(TEXT("После второго цикла видимость корня прежняя"),
					Root->GetVisibility(), ShownRootVisibility);
			}
		}

		AddInfo(bFromAsset
			? TEXT("Панель взята из живого ассета /Game/UI/WBP_PlayerStats")
			: TEXT("Ассета WBP_PlayerStats нет — проверено на кодовом классе панели"));
	}

	StatsPanelGateTest::DestroyWorld(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
