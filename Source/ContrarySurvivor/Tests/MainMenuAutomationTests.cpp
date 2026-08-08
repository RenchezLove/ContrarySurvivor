// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны «Главное меню» (ADR-062, спека glavnoe-menu-spec.md,
// подход 1: каркас меню + поведение запуска). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.MainMenu; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * «Продолжить» без сейва не показывается вовсе (Collapsed — места не занимает,
//     «не гаснет серым»), с сейвом — виден;
//   * переспрос «Начать заново?» идёт ТОЛЬКО поверх существующего сейва: первый клик
//     ничего не стирает, «Отмена» возвращает обычный выбор, стирающий сигнал уходит
//     только со второго явного нажатия; без сейва — сразу новая игра, без переспроса;
//   * адрес «Сообщества» читается из конфига (не зашит в код), чистится от пробелов;
//     пустой адрес прячет пункт целиком;
//   * меню показывается со второго запуска после установки либо при найденном сейве;
//     маркер запуска в памяти на установку взводится ровно один раз.
//
// НЕ покрывается headless: живой вид меню и реальные клики по кнопкам (Slate без живого
// запуска не кликается — обработчики зовутся напрямую, паттерн остальных UI-тестов проекта).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/StartScreenWidget.h"

static constexpr EAutomationTestFlags MainMenuTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. «Продолжить» и переспрос — чистые правила видимости -----------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainMenuContinueVisibilityTest,
	"ContrarySurvivor.MainMenu.ContinueHiddenWithoutSave", MainMenuTestFlags)

bool FMainMenuContinueVisibilityTest::RunTest(const FString& Parameters)
{
	// Спека: «Если сохранения нет — пункт не показывается вовсе (не гаснет серым)».
	// Collapsed, а не Hidden: пункт не оставляет пустого места в колонке меню.
	TestEqual(TEXT("Без сейва «Продолжить» схлопнут (места не занимает)"),
		UStartScreenWidget::ContinueVisibilityFor(false), ESlateVisibility::Collapsed);
	TestEqual(TEXT("С сейвом «Продолжить» виден"),
		UStartScreenWidget::ContinueVisibilityFor(true), ESlateVisibility::Visible);

	// Переспрос «Начать заново?» защищает существующий прогресс; без сейва стирать нечего.
	TestTrue(TEXT("Поверх сейва «Новая игра» переспрашивает"),
		UStartScreenWidget::ShouldConfirmNewGame(true));
	TestFalse(TEXT("Без сейва переспроса нет"),
		UStartScreenWidget::ShouldConfirmNewGame(false));
	return true;
}

// --- 2. Адрес «Сообщества» — из конфига, пустой прячет пункт ----------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainMenuCommunityUrlTest,
	"ContrarySurvivor.MainMenu.CommunityUrlComesFromConfig", MainMenuTestFlags)

bool FMainMenuCommunityUrlTest::RunTest(const FString& Parameters)
{
	UMainMenuSettings* Settings = GetMutableDefault<UMainMenuSettings>();
	if (!TestNotNull(TEXT("Настройки главного меню доступны"), Settings))
	{
		return false;
	}

	const FString Saved = Settings->CommunityUrl;

	// Пусто (так в конфиге сейчас) — пункт «Сообщество» спрятан целиком.
	Settings->CommunityUrl = FString();
	TestTrue(TEXT("Пустой адрес отдаётся пустым"), UMainMenuSettings::GetCommunityUrl().IsEmpty());
	TestEqual(TEXT("Без адреса пункт «Сообщество» спрятан"),
		UStartScreenWidget::CommunityVisibilityFor(UMainMenuSettings::GetCommunityUrl()),
		ESlateVisibility::Collapsed);

	// Строка из одних пробелов адресом не считается.
	Settings->CommunityUrl = TEXT("   ");
	TestTrue(TEXT("Пробелы адресом не считаются"), UMainMenuSettings::GetCommunityUrl().IsEmpty());

	// Адрес вписали — пункт сразу готов показаться, лишние пробелы срезаются.
	Settings->CommunityUrl = TEXT("  https://t.me/contrary_survivor  ");
	TestEqual(TEXT("Адрес из настройки приходит без пробелов"),
		UMainMenuSettings::GetCommunityUrl(), TEXT("https://t.me/contrary_survivor"));
	TestEqual(TEXT("С адресом пункт «Сообщество» виден"),
		UStartScreenWidget::CommunityVisibilityFor(UMainMenuSettings::GetCommunityUrl()),
		ESlateVisibility::Visible);

	Settings->CommunityUrl = Saved; // не оставлять след другим тестам
	return true;
}

// --- 3. Переспрос «Новая игра»: сигнал стирания уходит только со второго нажатия --------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainMenuNewGameConfirmFlowTest,
	"ContrarySurvivor.MainMenu.NewGameConfirmFlow", MainMenuTestFlags)

bool FMainMenuNewGameConfirmFlowTest::RunTest(const FString& Parameters)
{
	// Виджет без экрана: обработчики зовутся напрямую (живой Slate в headless не поднимается),
	// дерева нет — методы обязаны переживать пустые указатели кубиков.
	UStartScreenWidget* Menu = NewObject<UStartScreenWidget>();
	if (!TestNotNull(TEXT("Виджет меню создан"), Menu))
	{
		return false;
	}

	int32 NewGameSignals = 0;
	int32 ContinueSignals = 0;
	Menu->OnNewGameRequested.AddLambda([&NewGameSignals]() { ++NewGameSignals; });
	Menu->OnContinueRequested.AddLambda([&ContinueSignals]() { ++ContinueSignals; });

	// Поверх сейва: первый клик — только переспрос, прогресс цел.
	Menu->SetHasSave(true);
	Menu->HandleNewGameClicked();
	TestTrue(TEXT("Первый клик включил переспрос"), Menu->IsConfirmingNewGame());
	TestEqual(TEXT("Первый клик ничего не стирает"), NewGameSignals, 0);

	// «Отмена» (та же кнопка «Продолжить» в режиме переспроса) возвращает обычный выбор.
	Menu->HandleContinueClicked();
	TestFalse(TEXT("«Отмена» закрыла переспрос"), Menu->IsConfirmingNewGame());
	TestEqual(TEXT("«Отмена» не грузит сейв"), ContinueSignals, 0);
	TestEqual(TEXT("После отмены прогресс цел"), NewGameSignals, 0);

	// Два явных нажатия подряд — стирающий сигнал уходит ровно один раз.
	Menu->HandleNewGameClicked();
	Menu->HandleNewGameClicked();
	TestEqual(TEXT("Второе нажатие даёт ровно один стирающий сигнал"), NewGameSignals, 1);

	// Обычный «Продолжить» (вне переспроса) шлёт свой сигнал владельцу.
	TestFalse(TEXT("После подтверждения переспрос закрыт"), Menu->IsConfirmingNewGame());
	Menu->HandleContinueClicked();
	TestEqual(TEXT("«Продолжить» шлёт сигнал загрузки"), ContinueSignals, 1);
	return true;
}

// --- 4. Без сейва «Новая игра» стартует сразу, без переспроса ---------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainMenuNewGameWithoutSaveTest,
	"ContrarySurvivor.MainMenu.NewGameWithoutSaveSkipsConfirm", MainMenuTestFlags)

bool FMainMenuNewGameWithoutSaveTest::RunTest(const FString& Parameters)
{
	UStartScreenWidget* Menu = NewObject<UStartScreenWidget>();
	if (!TestNotNull(TEXT("Виджет меню создан"), Menu))
	{
		return false;
	}

	int32 NewGameSignals = 0;
	Menu->OnNewGameRequested.AddLambda([&NewGameSignals]() { ++NewGameSignals; });

	// Сейва нет — стирать нечего: сразу новая игра, переспрос не включается.
	Menu->SetHasSave(false);
	Menu->HandleNewGameClicked();
	TestFalse(TEXT("Переспрос без сейва не включается"), Menu->IsConfirmingNewGame());
	TestEqual(TEXT("Новая игра стартует с первого нажатия"), NewGameSignals, 1);
	return true;
}

// --- 5. Меню — со второго запуска; маркер запуска взводится один раз --------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainMenuSecondLaunchRuleTest,
	"ContrarySurvivor.MainMenu.MenuShownFromSecondLaunch", MainMenuTestFlags)

bool FMainMenuSecondLaunchRuleTest::RunTest(const FString& Parameters)
{
	// Спека: самый первый запуск после установки — сразу во вступление, без меню.
	TestFalse(TEXT("Самый первый запуск после установки — без меню"),
		AContrarySurvivorPlayerController::ShouldShowMainMenuOnLaunch(false, false));
	// Со второго запуска меню открывается всегда, даже если сейв стёрт «Новой игрой».
	TestTrue(TEXT("Повторный запуск без сейва — меню показывается"),
		AContrarySurvivorPlayerController::ShouldShowMainMenuOnLaunch(true, false));
	// Найденный сейв сам доказывает прошлый запуск (обновление со сборки без маркера) —
	// прежнее поведение Б3 «сейв найден → экран выбора» сохраняется.
	TestTrue(TEXT("Сейв без маркера запусков — меню показывается"),
		AContrarySurvivorPlayerController::ShouldShowMainMenuOnLaunch(false, true));
	TestTrue(TEXT("Повторный запуск с сейвом — меню показывается"),
		AContrarySurvivorPlayerController::ShouldShowMainMenuOnLaunch(true, true));

	// Маркер «игра уже запускалась» в памяти на установку взводится ровно один раз;
	// диска тест не касается (объект в памяти, боевой слот ContraryAnalytics не трогаем).
	UAnalyticsProfileSave* Save = NewObject<UAnalyticsProfileSave>();
	if (!TestNotNull(TEXT("Объект памяти на установку создан"), Save))
	{
		return false;
	}
	TestFalse(TEXT("Свежая установка: запуска ещё не было"), Save->bGameLaunchedBefore);
	TestTrue(TEXT("Первый запуск помечается"), Save->MarkGameLaunched());
	TestTrue(TEXT("Маркер взведён"), Save->bGameLaunchedBefore);
	TestFalse(TEXT("Повторная пометка не задваивается"), Save->MarkGameLaunched());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
