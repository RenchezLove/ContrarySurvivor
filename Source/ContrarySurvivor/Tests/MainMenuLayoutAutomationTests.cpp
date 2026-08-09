// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты переделки главного меню 08-09 (кадр с телефона `menu-live-0809.png`,
// разбор game-lead). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.MenuLayout; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * фон меню НИКОГДА не остаётся прозрачным: картинки нет — красим сплошной заливкой
//     (дефект с телефона: сквозь меню была видна живая игра, спека требует статичный фон);
//   * логотипа нет — кубик схлопнут, а не пустой прямоугольник посреди экрана;
//   * при открытом главном меню постоянная панель статов скрыта целиком и возвращается,
//     когда меню закрылось (дефект с телефона: поверх меню висели полосы и деньги).
//
// НЕ покрывается headless: сама раскладка «логотип слева, кнопки справа» и то, как она
// выглядит на разных экранах — это судит Ринат живьём по кадру с устройства.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "ContrarySurvivor/UI/PlayerStatsWidget.h"
#include "Engine/Texture2D.h"

static constexpr EAutomationTestFlags MenuLayoutTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Фон меню не бывает прозрачным -----------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuBackgroundNeverTransparentTest,
	"ContrarySurvivor.MenuLayout.BackgroundNeverShowsGame", MenuLayoutTestFlags)

bool FMenuBackgroundNeverTransparentTest::RunTest(const FString& Parameters)
{
	// Картинку фона художник ещё готовит. Пока её нет, меню обязано закрываться сплошной
	// заливкой: игрок не должен видеть сквозь меню живой мир игры.
	const TSoftObjectPtr<UTexture2D> NoTexture;
	TestTrue(TEXT("Картинки нет — красим сплошной заливкой"),
		UStartScreenWidget::ShouldFillBackgroundWithColor(NoTexture));

	// Картинка задана — заливка не нужна, рисуем её.
	const TSoftObjectPtr<UTexture2D> SomeTexture(FSoftObjectPath(TEXT("/Game/UI/Backgrounds/T_MenuBg.T_MenuBg")));
	TestFalse(TEXT("Картинка задана — рисуем картинку, а не заливку"),
		UStartScreenWidget::ShouldFillBackgroundWithColor(SomeTexture));

	// Цвет заливки по умолчанию — фирменный тёмный, и он ОБЯЗАН быть непрозрачным.
	const FStartScreenStyle DefaultStyle;
	TestEqual(TEXT("Заливка фона полностью непрозрачна"), DefaultStyle.BackgroundFallbackColor.A, 1.0f);

	// Слой-барьер поверх фона по умолчанию прозрачный: затемнять картинку он не должен,
	// его дело — ловить касания мимо кнопок.
	TestEqual(TEXT("Барьер касаний сам ничего не затемняет"), DefaultStyle.DimColor.A, 0.0f);
	return true;
}

// --- 2. Логотип: нет текстуры — нет кубика -------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuLogoVisibilityTest,
	"ContrarySurvivor.MenuLayout.LogoHiddenWithoutTexture", MenuLayoutTestFlags)

bool FMenuLogoVisibilityTest::RunTest(const FString& Parameters)
{
	// Логотипа игры отдельной картинкой в проекте пока нет вовсе — меню обязано это пережить.
	const TSoftObjectPtr<UTexture2D> NoTexture;
	TestEqual(TEXT("Без текстуры логотип не рисуется и места не занимает"),
		UStartScreenWidget::LogoVisibilityFor(NoTexture), ESlateVisibility::Collapsed);

	const TSoftObjectPtr<UTexture2D> SomeTexture(FSoftObjectPath(TEXT("/Game/UI/Logo/T_GameLogo.T_GameLogo")));
	TestEqual(TEXT("С текстурой логотип виден и касания не перехватывает"),
		UStartScreenWidget::LogoVisibilityFor(SomeTexture), ESlateVisibility::HitTestInvisible);
	return true;
}

// --- 3. Игровой интерфейс прячется на время меню -------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsPanelHiddenUnderMenuTest,
	"ContrarySurvivor.MenuLayout.StatsPanelHiddenWhileMenuOpen", MenuLayoutTestFlags)

bool FStatsPanelHiddenUnderMenuTest::RunTest(const FString& Parameters)
{
	// Дефект с телефона: поверх главного меню оставались полосы здоровья, голода, жажды и
	// деньги. Collapsed, а не Hidden — панель не должна ни рисоваться, ни занимать место.
	TestEqual(TEXT("Меню на экране — панель статов скрыта целиком"),
		UPlayerStatsWidget::VisibilityForMainMenu(true), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Меню закрылось — панель вернулась"),
		UPlayerStatsWidget::VisibilityForMainMenu(false), ESlateVisibility::HitTestInvisible);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
