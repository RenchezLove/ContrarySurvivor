// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты вида кнопок главного меню по макету 08-09
// (`concept-art/menu-mockups/menu-background-A-check.png`, живой осмотр Рината: «цвет кнопок
// и рамка старые, не как на этой картинке»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.MenuLook; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * подложки под столбиком кнопок нет вовсе — обе краски рамки и панели прозрачны
//     (именно эту холодную рамку Ринат назвал «старой»);
//   * кисть кнопки собирается ровно из полей стиля: заливка, тонкая рамка, скругление —
//     а значит правка поля меняет вид, а не остаётся мёртвой настройкой;
//   * верхний пункт «Продолжить» выделен другой парой цветов, чем остальные;
//   * заголовок «С ВОЗВРАЩЕНИЕМ» и подпись про сохранение в обычном меню спрятаны, а в
//     переспросе «Начать заново?» показываются — иначе вопрос о стирании остался бы немым.
//
// Один и тот же метод UStartScreenWidget::MakeMenuButtonStyle красит и кодовое дерево, и
// живой ассет (его зовёт генератор окон), поэтому проверка вида здесь накрывает оба пути.
//
// НЕ покрывается headless: как это выглядит на устройстве — судит Ринат живьём по кадру.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags MenuLookTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Подложки нет, цвета кнопок берутся из полей ---------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuButtonStyleFromMockupTest,
	"ContrarySurvivor.MenuLook.ButtonStyleComesFromFields", MenuLookTestFlags)

bool FMenuButtonStyleFromMockupTest::RunTest(const FString& Parameters)
{
	const FStartScreenStyle Style;

	// Подложки под кнопками на макете нет: и кант, и фон панели полностью прозрачны.
	TestEqual(TEXT("Рамки-подложки вокруг кнопок нет"), Style.FrameColor.A, 0.0f);
	TestEqual(TEXT("Фона-подложки под кнопками нет"), Style.PanelColor.A, 0.0f);

	// Обычная кнопка: заливка полупрозрачная и тёплая (красного больше, чем синего).
	TestTrue(TEXT("Заливка обычной кнопки полупрозрачна"),
		Style.ButtonFillColor.A > 0.0f && Style.ButtonFillColor.A < 1.0f);
	TestTrue(TEXT("Заливка обычной кнопки тёплая (красного больше синего)"),
		Style.ButtonFillColor.R > Style.ButtonFillColor.B);
	TestTrue(TEXT("Рамка обычной кнопки светлее её заливки"),
		Style.ButtonBorderColor.R > Style.ButtonFillColor.R);
	TestTrue(TEXT("Подпись обычной кнопки почти белая"),
		Style.ButtonTextColor.R > 0.7f && Style.ButtonTextColor.G > 0.7f);

	// Выделенный верхний пункт: своя оранжевая пара, заметно светлее обычной кнопки.
	TestTrue(TEXT("Выделенная кнопка гораздо светлее обычной"),
		Style.PrimaryButtonFillColor.R > Style.ButtonFillColor.R * 10.0f);
	TestTrue(TEXT("Рамка выделенной кнопки светлее её заливки"),
		Style.PrimaryButtonBorderColor.R > Style.PrimaryButtonFillColor.R);
	TestTrue(TEXT("Заливка выделенной кнопки оранжевая (красного больше зелёного, зелёного больше синего)"),
		Style.PrimaryButtonFillColor.R > Style.PrimaryButtonFillColor.G
		&& Style.PrimaryButtonFillColor.G > Style.PrimaryButtonFillColor.B);

	// Кисть кнопки собирается ИЗ ПОЛЕЙ — правка поля меняет вид обоих путей.
	const FButtonStyle Normal = UStartScreenWidget::MakeMenuButtonStyle(Style, /*bPrimary=*/false);
	TestEqual(TEXT("Кнопка рисуется скруглённым прямоугольником"),
		Normal.Normal.DrawAs, ESlateBrushDrawType::RoundedBox);
	TestEqual(TEXT("Заливка обычной кнопки взята из поля"),
		Normal.Normal.TintColor.GetSpecifiedColor(), Style.ButtonFillColor);
	TestEqual(TEXT("Цвет рамки взят из поля"),
		Normal.Normal.OutlineSettings.Color.GetSpecifiedColor(), Style.ButtonBorderColor);
	TestEqual(TEXT("Толщина рамки взята из поля"),
		Normal.Normal.OutlineSettings.Width, Style.ButtonBorderWidth);
	TestEqual(TEXT("Скругление углов взято из поля"),
		static_cast<float>(Normal.Normal.OutlineSettings.CornerRadii.X), Style.ButtonCornerRadius);

	const FButtonStyle Primary = UStartScreenWidget::MakeMenuButtonStyle(Style, /*bPrimary=*/true);
	TestEqual(TEXT("Заливка выделенной кнопки взята из своего поля"),
		Primary.Normal.TintColor.GetSpecifiedColor(), Style.PrimaryButtonFillColor);
	TestNotEqual(TEXT("Выделенная кнопка отличается от обычной"),
		Primary.Normal.TintColor.GetSpecifiedColor(), Normal.Normal.TintColor.GetSpecifiedColor());
	TestEqual(TEXT("Подпись выделенной кнопки — свой цвет"),
		UStartScreenWidget::MenuButtonTextColor(Style, true), Style.PrimaryButtonTextColor);
	TestEqual(TEXT("Подпись обычной кнопки — свой цвет"),
		UStartScreenWidget::MenuButtonTextColor(Style, false), Style.ButtonTextColor);

	// Кнопка крупная и под палец: на макете она заметно шире, чем выше, и выше 80 точек.
	TestTrue(TEXT("Кнопка крупная по высоте"), Style.ButtonSize.Y >= 80.0f);
	TestTrue(TEXT("Кнопка вытянута в ширину"), Style.ButtonSize.X > Style.ButtonSize.Y * 3.0f);
	TestTrue(TEXT("Между кнопками есть зазор"), Style.ButtonSpacing > 0.0f);
	return true;
}

// --- 2. Заголовок и подпись: спрятаны в меню, показаны в переспросе ------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuTitleHiddenUntilConfirmTest,
	"ContrarySurvivor.MenuLook.TitleHiddenUntilConfirm", MenuLookTestFlags)

bool FMenuTitleHiddenUntilConfirmTest::RunTest(const FString& Parameters)
{
	if (!GEngine)
	{
		return false;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
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

	{
		// Меню берём из ЖИВОГО ассета WBP_StartScreen — того самого, что стоит в слоте
		// контроллера на устройстве. Кодовое дерево-запаска здесь не годится: движок строит
		// его в NativeOnInitialized, а тот в headless не зовётся вовсе — UUserWidget::Initialize
		// пропускает его без игрового контекста (UserWidget.cpp:160, UE 5.5), которого у
		// тестового мира нет. У дерева из ассета кубики привязываются раньше и без контекста.
		// Владелец виджета — МИР: CreateWidget на контроллере без локального игрока отказывает.
		UClass* MenuClass = StaticLoadClass(UStartScreenWidget::StaticClass(), nullptr,
			TEXT("/Game/UI/WBP_StartScreen.WBP_StartScreen_C"));
		UStartScreenWidget* Menu = MenuClass ? CreateWidget<UStartScreenWidget>(World, MenuClass) : nullptr;
		if (!MenuClass)
		{
			AddWarning(TEXT("Ассета WBP_StartScreen нет на диске — правило заголовка проверить не на чем"));
		}
		if (TestNotNull(TEXT("Меню создано из живого ассета"), Menu)
			&& TestNotNull(TEXT("Дерево меню живо"), Menu->WidgetTree.Get()))
		{
			UWidget* Title = Menu->WidgetTree->FindWidget(TEXT("TitleText"));
			UWidget* Subtitle = Menu->WidgetTree->FindWidget(TEXT("SubtitleText"));
			if (TestNotNull(TEXT("Заголовок есть в дереве"), Title)
				&& TestNotNull(TEXT("Подпись есть в дереве"), Subtitle))
			{
				// Тот же порядок, что у контроллера при открытии меню: стиль, потом сейв.
				Menu->ApplyStyle(FStartScreenStyle());
				Menu->SetHasSave(true);

				TestEqual(TEXT("В обычном меню заголовка над кнопками нет"),
					Title->GetVisibility(), ESlateVisibility::Collapsed);
				TestEqual(TEXT("В обычном меню подписи про сохранение нет"),
					Subtitle->GetVisibility(), ESlateVisibility::Collapsed);

				// Первый клик по «Новая игра» поверх сейва включает переспрос — вопрос о
				// стирании обязан быть написан словами.
				Menu->HandleNewGameClicked();
				TestTrue(TEXT("Переспрос включился"), Menu->IsConfirmingNewGame());
				TestNotEqual(TEXT("В переспросе заголовок-вопрос показан"),
					Title->GetVisibility(), ESlateVisibility::Collapsed);
				TestNotEqual(TEXT("В переспросе пояснение показано"),
					Subtitle->GetVisibility(), ESlateVisibility::Collapsed);

				// «Отмена» возвращает обычное меню — и снова прячет обе строки.
				Menu->HandleContinueClicked();
				TestFalse(TEXT("Переспрос закрыт"), Menu->IsConfirmingNewGame());
				TestEqual(TEXT("После отмены заголовок снова спрятан"),
					Title->GetVisibility(), ESlateVisibility::Collapsed);
				TestEqual(TEXT("После отмены подпись снова спрятана"),
					Subtitle->GetVisibility(), ESlateVisibility::Collapsed);
			}
		}
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
