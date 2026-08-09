// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты экрана настроек (волна «Главное меню» 08-08, подход 2; ADR-062,
// спека docs/contrary-survivor/glavnoe-menu-spec.md, раздел «Экран настроек»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Settings; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * масштаб разрешения живёт в границах 50–100 с шагом 10 (спека), любое число прижимается;
//   * значения по умолчанию — те, что записаны в ADR-062 (масштаб 100%, предел кадров 30,
//     счётчик кадров ВЫКЛЮЧЕН, пресет «Авто»);
//   * настройки переживают перезапуск игры: записанное в файл настроек читается обратно
//     (файл временный, боевой GameUserSettings.ini разработчика тесты не трогают);
//   * «Авто» выбирает пресет по железу и показывает игроку, что выбрано;
//   * подпись ползунка масштаба считает реальные пиксели экрана («70% — это 504 на 1120»);
//   * пункт «Сообщить об ошибке» без адреса в конфиге спрятан целиком;
//   * «Сбросить прогресс» стирает только после ДВОЙНОГО переспроса;
//   * движок реально создаёт НАШ класс настроек (строка GameUserSettingsClassName в
//     Config/DefaultEngine.ini) — иначе весь экран настроек работал бы вхолостую.
//
// НЕ покрывается headless: живой вид экрана и настоящие касания (Slate без запущенной игры не
// кликается — обработчики зовутся напрямую, паттерн остальных UI-тестов проекта), а также
// фактическое влияние пресета на картинку (это проверяется глазами на устройстве).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h"
#include "ContrarySurvivor/UI/SettingsScreenWidget.h"
#include "ContrarySurvivor/UI/StartScreenWidget.h" // UMainMenuSettings::BugReportUrl
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static constexpr EAutomationTestFlags SettingsTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Масштаб разрешения: границы 50–100 и шаг 10 --------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsResolutionStepTest,
	"ContrarySurvivor.Settings.ResolutionScaleStepAndRange", SettingsTestFlags)

bool FSettingsResolutionStepTest::RunTest(const FString& Parameters)
{
	using USettings = UContrarySurvivorGameUserSettings;

	// Спека: «от 50 до 100 процентов с шагом 10».
	TestEqual(TEXT("Нижняя граница остаётся 50"), USettings::SnapResolutionScalePercent(50), 50);
	TestEqual(TEXT("Верхняя граница остаётся 100"), USettings::SnapResolutionScalePercent(100), 100);
	TestEqual(TEXT("Ниже границы прижимается к 50"), USettings::SnapResolutionScalePercent(10), 50);
	TestEqual(TEXT("Выше границы прижимается к 100"), USettings::SnapResolutionScalePercent(500), 100);
	TestEqual(TEXT("73 округляется к 70"), USettings::SnapResolutionScalePercent(73), 70);
	TestEqual(TEXT("76 округляется к 80"), USettings::SnapResolutionScalePercent(76), 80);
	TestEqual(TEXT("Шаг ровно 10"), USettings::ResolutionScaleStepPercent, 10);

	// Ползунок и проценты — одна и та же величина: перевод туда-обратно ничего не теряет.
	for (int32 Percent = 50; Percent <= 100; Percent += 10)
	{
		const float SliderValue = USettingsScreenWidget::ResolutionPercentToSliderValue(Percent);
		TestEqual(FString::Printf(TEXT("Ползунок возвращает те же %d%%"), Percent),
			USettingsScreenWidget::SliderValueToResolutionPercent(SliderValue), Percent);
	}
	// Палец останавливается между шагами — значение прилипает к ближайшему шагу.
	TestEqual(TEXT("Промежуточное положение ползунка прилипает к шагу"),
		USettingsScreenWidget::SliderValueToResolutionPercent(0.43f), 70);
	TestEqual(TEXT("Левый край ползунка — 50%"),
		USettingsScreenWidget::SliderValueToResolutionPercent(0.0f), 50);
	TestEqual(TEXT("Правый край ползунка — 100%"),
		USettingsScreenWidget::SliderValueToResolutionPercent(1.0f), 100);
	return true;
}

// --- 2. Значения по умолчанию (ADR-062) --------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsDefaultsTest,
	"ContrarySurvivor.Settings.DefaultsMatchSpec", SettingsTestFlags)

bool FSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	UContrarySurvivorGameUserSettings* Settings = NewObject<UContrarySurvivorGameUserSettings>();
	if (!TestNotNull(TEXT("Объект настроек создан"), Settings))
	{
		return false;
	}
	// Заводские значения (движок зовёт этот же метод при создании объекта и при порче файла).
	Settings->SetToDefaults();

	TestEqual(TEXT("Пресет по умолчанию — «Авто»"), Settings->GetQualityPreset(), EContraryGraphicsPreset::Auto);
	TestEqual(TEXT("Масштаб разрешения по умолчанию 100%"), Settings->GetResolutionScalePercent(), 100);
	TestTrue(TEXT("Ограничение кадров по умолчанию включено (30)"), Settings->IsFrameRateLimitedTo30());
	TestFalse(TEXT("Счётчик кадров по умолчанию ВЫКЛЮЧЕН (спека)"), Settings->IsFpsCounterShown());
	TestEqual(TEXT("Громкость музыки по умолчанию полная"), Settings->GetMusicVolume(), 1.0f);
	TestEqual(TEXT("Громкость эффектов по умолчанию полная"), Settings->GetEffectsVolume(), 1.0f);
	TestEqual(TEXT("Чувствительность по умолчанию обычная"), Settings->GetControlSensitivity(), 1.0f);
	TestEqual(TEXT("Прозрачность кнопок по умолчанию прежняя (0.5)"), Settings->GetTouchButtonsOpacity(), 0.5f);
	TestTrue(TEXT("Вибрация по умолчанию включена"), Settings->IsVibrationEnabled());
	return true;
}

// --- 3. Настройки переживают перезапуск --------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsPersistTest,
	"ContrarySurvivor.Settings.SavedValuesSurviveRestart", SettingsTestFlags)

bool FSettingsPersistTest::RunTest(const FString& Parameters)
{
	// Тест работает на ВРЕМЕННЫХ файлах настроек: боевой GameUserSettings.ini разработчика
	// (и настройки редактора) остаются нетронутыми. Проверяем обе стороны:
	//   файл с диска -> объект (так игра поднимает настройки при следующем запуске),
	//   объект -> файл -> второй объект (так игра их сохраняет).
	const FString SavedIni = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Test") / TEXT("SettingsRoundtripIn.ini"));

	const FString SectionName = UContrarySurvivorGameUserSettings::StaticClass()->GetPathName();
	const FString FileText = FString::Printf(TEXT(
		"[%s]\r\n"
		"QualityPreset=High\r\n"
		"ResolutionScalePercent=70\r\n"
		"bLimitFrameRateTo30=False\r\n"
		"bShowFpsCounter=True\r\n"
		"MusicVolume=0.300000\r\n"
		"EffectsVolume=0.400000\r\n"
		"ControlSensitivity=1.500000\r\n"
		"TouchButtonsOpacity=0.800000\r\n"
		"bVibrationEnabled=False\r\n"), *SectionName);

	if (!TestTrue(TEXT("Временный файл настроек записан"),
		FFileHelper::SaveStringToFile(FileText, *SavedIni)))
	{
		return false;
	}
	// Сбрасываем возможный кеш этого файла — читать обязаны с диска.
	GConfig->UnloadFile(SavedIni);

	UContrarySurvivorGameUserSettings* Loaded = NewObject<UContrarySurvivorGameUserSettings>();
	Loaded->SetToDefaults();
	Loaded->LoadConfig(nullptr, *SavedIni);

	TestEqual(TEXT("Пресет прочитан из файла"), Loaded->GetQualityPreset(), EContraryGraphicsPreset::High);
	TestEqual(TEXT("Масштаб разрешения прочитан из файла"), Loaded->GetResolutionScalePercent(), 70);
	TestFalse(TEXT("Снятое ограничение кадров прочитано"), Loaded->IsFrameRateLimitedTo30());
	TestTrue(TEXT("Включённый счётчик кадров прочитан"), Loaded->IsFpsCounterShown());
	TestEqual(TEXT("Громкость музыки прочитана"), Loaded->GetMusicVolume(), 0.3f, 0.001f);
	TestEqual(TEXT("Громкость эффектов прочитана"), Loaded->GetEffectsVolume(), 0.4f, 0.001f);
	TestEqual(TEXT("Чувствительность прочитана"), Loaded->GetControlSensitivity(), 1.5f, 0.001f);
	TestEqual(TEXT("Прозрачность кнопок прочитана"), Loaded->GetTouchButtonsOpacity(), 0.8f, 0.001f);
	TestFalse(TEXT("Выключенная вибрация прочитана"), Loaded->IsVibrationEnabled());

	// Обратная сторона (запись). Саму запись файла делает движок:
	// UGameUserSettings::SaveSettings() зовёт SaveConfig(CPF_Config, GGameUserSettingsIni), и
	// туда попадают ТОЛЬКО свойства с флагом Config. Поэтому здесь проверяем ровно то, что
	// зависит от нас: каждое поле экрана настроек объявлено с UPROPERTY(Config). Потеряется
	// флаг — настройка молча перестанет сохраняться между запусками, и тест это поймает.
	// (Боевой файл настроек разработчика тесты при этом не трогают вовсе.)
	static const TCHAR* SavedFieldNames[] =
	{
		TEXT("QualityPreset"), TEXT("ResolutionScalePercent"), TEXT("bLimitFrameRateTo30"),
		TEXT("bShowFpsCounter"), TEXT("MusicVolume"), TEXT("EffectsVolume"),
		TEXT("ControlSensitivity"), TEXT("TouchButtonsOpacity"), TEXT("bVibrationEnabled"),
	};
	for (const TCHAR* FieldName : SavedFieldNames)
	{
		const FProperty* Field = FindFProperty<FProperty>(
			UContrarySurvivorGameUserSettings::StaticClass(), FieldName);
		if (TestNotNull(*FString::Printf(TEXT("Поле настроек %s существует"), FieldName), Field))
		{
			TestTrue(*FString::Printf(TEXT("Поле %s сохраняется между запусками (флаг Config)"), FieldName),
				Field->HasAnyPropertyFlags(CPF_Config));
		}
	}

	// Прибираем за собой: временные файлы не копим.
	GConfig->UnloadFile(SavedIni);
	IFileManager::Get().Delete(*SavedIni, /*RequireExists=*/false, /*EvenReadOnly=*/true);
	return true;
}

// --- 4. «Авто»: выбор пресета по железу --------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsAutoPresetTest,
	"ContrarySurvivor.Settings.AutoPresetByDevice", SettingsTestFlags)

bool FSettingsAutoPresetTest::RunTest(const FString& Parameters)
{
	using USettings = UContrarySurvivorGameUserSettings;

	// Слабый телефон (мало памяти ИЛИ мало ядер) — низкое качество.
	TestEqual(TEXT("2 ГБ памяти — низкое"), USettings::ChooseAutoPreset(2, 8), EContraryGraphicsPreset::Low);
	TestEqual(TEXT("4 ядра — низкое"), USettings::ChooseAutoPreset(8, 4), EContraryGraphicsPreset::Low);
	// Устройство ничего о себе не сообщило — считаем слабым, а не сильным.
	TestEqual(TEXT("Неизвестное железо — низкое"), USettings::ChooseAutoPreset(0, 0), EContraryGraphicsPreset::Low);
	// Середина.
	TestEqual(TEXT("6 ГБ и 6 ядер — среднее"), USettings::ChooseAutoPreset(6, 6), EContraryGraphicsPreset::Medium);
	// Сильное устройство.
	TestEqual(TEXT("8 ГБ и 8 ядер — высокое"), USettings::ChooseAutoPreset(8, 8), EContraryGraphicsPreset::High);
	TestEqual(TEXT("12 ГБ и 8 ядер — высокое"), USettings::ChooseAutoPreset(12, 8), EContraryGraphicsPreset::High);

	// «Авто» — не самостоятельное качество: живое значение всегда одно из трёх настоящих.
	UContrarySurvivorGameUserSettings* Settings = NewObject<UContrarySurvivorGameUserSettings>();
	Settings->SetToDefaults();
	const EContraryGraphicsPreset Effective = Settings->GetEffectivePreset();
	TestTrue(TEXT("При «Авто» живой пресет — низкое, среднее или высокое"),
		Effective == EContraryGraphicsPreset::Low || Effective == EContraryGraphicsPreset::Medium
		|| Effective == EContraryGraphicsPreset::High);
	return true;
}

// --- 5. Подписи: пиксели экрана и показ выбора «Авто» -------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsLabelsTest,
	"ContrarySurvivor.Settings.LabelsShowPixelsAndAutoChoice", SettingsTestFlags)

bool FSettingsLabelsTest::RunTest(const FString& Parameters)
{
	using USettings = UContrarySurvivorGameUserSettings;

	// Дословный пример из спеки: «70% — это 504 на 1120» на экране 720x1600.
	TestEqual(TEXT("Подпись масштаба считает реальные пиксели"),
		USettings::MakeResolutionScaleLabel(70, FIntPoint(720, 1600)).ToString(),
		FString(TEXT("70% — это 504 на 1120")));
	// Размера экрана ещё нет — пиксели не выдумываем.
	TestEqual(TEXT("Без размера экрана — только проценты"),
		USettings::MakeResolutionScaleLabel(70, FIntPoint::ZeroValue).ToString(),
		FString(TEXT("70%")));

	// Спека: при «Авто» игрок видит, какой пресет выбран за него.
	TestEqual(TEXT("«Авто» показывает выбранное качество"),
		USettings::MakePresetLabel(EContraryGraphicsPreset::Auto, EContraryGraphicsPreset::Medium).ToString(),
		FString(TEXT("Авто (среднее)")));
	TestEqual(TEXT("Ручной выбор показывается как есть"),
		USettings::MakePresetLabel(EContraryGraphicsPreset::Low, EContraryGraphicsPreset::High).ToString(),
		FString(TEXT("Низкое")));
	TestEqual(TEXT("Доля переводится в проценты"),
		USettings::MakePercentLabel(0.7f).ToString(), FString(TEXT("70%")));
	return true;
}

// --- 6. Предел кадров и уровни качества движка --------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsFrameLimitTest,
	"ContrarySurvivor.Settings.FrameLimitAndScalabilityMapping", SettingsTestFlags)

bool FSettingsFrameLimitTest::RunTest(const FString& Parameters)
{
	using USettings = UContrarySurvivorGameUserSettings;

	// Спека: «Ограничение кадров: 30 или без ограничения» (0 у движка = предела нет).
	TestEqual(TEXT("Включено — предел 30 кадров"), USettings::FrameRateLimitFor(true), 30.0f);
	TestEqual(TEXT("Выключено — предела нет"), USettings::FrameRateLimitFor(false), 0.0f);

	TestEqual(TEXT("Низкое — нулевой уровень качества движка"),
		USettings::ScalabilityLevelFor(EContraryGraphicsPreset::Low), 0);
	TestEqual(TEXT("Среднее — первый уровень"),
		USettings::ScalabilityLevelFor(EContraryGraphicsPreset::Medium), 1);
	TestEqual(TEXT("Высокое — второй уровень"),
		USettings::ScalabilityLevelFor(EContraryGraphicsPreset::High), 2);
	return true;
}

// --- 7. «Сообщить об ошибке» — адрес из конфига -------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsReportBugUrlTest,
	"ContrarySurvivor.Settings.ReportBugHiddenWithoutUrl", SettingsTestFlags)

bool FSettingsReportBugUrlTest::RunTest(const FString& Parameters)
{
	UMainMenuSettings* Settings = GetMutableDefault<UMainMenuSettings>();
	if (!TestNotNull(TEXT("Настройки главного меню доступны"), Settings))
	{
		return false;
	}
	const FString Saved = Settings->BugReportUrl;

	// Пусто (так в конфиге сейчас) — пункт спрятан целиком, как «Сообщество» (ADR-062).
	Settings->BugReportUrl = FString();
	TestTrue(TEXT("Пустой адрес отдаётся пустым"), UMainMenuSettings::GetBugReportUrl().IsEmpty());
	TestEqual(TEXT("Без адреса пункт спрятан"),
		USettingsScreenWidget::ReportBugVisibilityFor(UMainMenuSettings::GetBugReportUrl()),
		ESlateVisibility::Collapsed);

	Settings->BugReportUrl = TEXT("   ");
	TestTrue(TEXT("Пробелы адресом не считаются"), UMainMenuSettings::GetBugReportUrl().IsEmpty());

	Settings->BugReportUrl = TEXT("  https://t.me/contrary_bugs  ");
	TestEqual(TEXT("Адрес приходит без пробелов"),
		UMainMenuSettings::GetBugReportUrl(), TEXT("https://t.me/contrary_bugs"));
	TestEqual(TEXT("С адресом пункт виден"),
		USettingsScreenWidget::ReportBugVisibilityFor(UMainMenuSettings::GetBugReportUrl()),
		ESlateVisibility::Visible);

	Settings->BugReportUrl = Saved; // не оставлять след другим тестам
	return true;
}

// --- 7б. Свой адрес не задан — отчёты идут в чат сообщества (решение game-lead 08-09) -----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsReportBugFallbackTest,
	"ContrarySurvivor.Settings.ReportBugFallsBackToCommunity", SettingsTestFlags)

bool FSettingsReportBugFallbackTest::RunTest(const FString& Parameters)
{
	UMainMenuSettings* Settings = GetMutableDefault<UMainMenuSettings>();
	if (!TestNotNull(TEXT("Настройки главного меню доступны"), Settings))
	{
		return false;
	}
	const FString SavedBug = Settings->BugReportUrl;
	const FString SavedCommunity = Settings->CommunityUrl;

	// Канал у нас один: отдельный адрес для отчётов заполнять необязательно.
	Settings->BugReportUrl = FString();
	Settings->CommunityUrl = TEXT("https://t.me/contrary_survivor");
	TestEqual(TEXT("Без своего адреса отчёты идут в чат сообщества"),
		UMainMenuSettings::GetEffectiveBugReportUrl(), TEXT("https://t.me/contrary_survivor"));
	TestEqual(TEXT("И пункт «Сообщить об ошибке» при этом виден"),
		USettingsScreenWidget::ReportBugVisibilityFor(UMainMenuSettings::GetEffectiveBugReportUrl()),
		ESlateVisibility::Visible);

	// Свой адрес задан — он и побеждает (дверь на будущее: отдельный чат под ошибки).
	Settings->BugReportUrl = TEXT("https://t.me/contrary_bugs");
	TestEqual(TEXT("Свой адрес отчётов важнее адреса сообщества"),
		UMainMenuSettings::GetEffectiveBugReportUrl(), TEXT("https://t.me/contrary_bugs"));

	// Пусты обе строки — пункта нет.
	Settings->BugReportUrl = FString();
	Settings->CommunityUrl = FString();
	TestTrue(TEXT("Без обоих адресов отчётам уходить некуда"),
		UMainMenuSettings::GetEffectiveBugReportUrl().IsEmpty());
	TestEqual(TEXT("И пункт спрятан целиком"),
		USettingsScreenWidget::ReportBugVisibilityFor(UMainMenuSettings::GetEffectiveBugReportUrl()),
		ESlateVisibility::Collapsed);

	Settings->BugReportUrl = SavedBug; // не оставлять след другим тестам
	Settings->CommunityUrl = SavedCommunity;
	return true;
}

// --- 8. «Сбросить прогресс» — только после двойного переспроса ----------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsResetDoubleConfirmTest,
	"ContrarySurvivor.Settings.ResetProgressNeedsDoubleConfirm", SettingsTestFlags)

bool FSettingsResetDoubleConfirmTest::RunTest(const FString& Parameters)
{
	// Виджет без экрана: обработчики зовутся напрямую (живой Slate в headless не поднимается),
	// дерева нет — методы обязаны переживать пустые указатели кубиков.
	USettingsScreenWidget* Screen = NewObject<USettingsScreenWidget>();
	if (!TestNotNull(TEXT("Экран настроек создан"), Screen))
	{
		return false;
	}

	int32 WipeSignals = 0;
	Screen->OnResetProgressConfirmed.AddLambda([&WipeSignals]() { ++WipeSignals; });

	// «Да» без вопроса не стирает ничего (защита от случайного вызова).
	Screen->HandleConfirmYesClicked();
	TestEqual(TEXT("Подтверждение без вопроса ничего не стирает"), WipeSignals, 0);

	// Первое нажатие — только первый вопрос.
	Screen->HandleResetProgressClicked();
	TestEqual(TEXT("Первое нажатие открыло первый вопрос"),
		Screen->GetResetConfirmStage(), EResetConfirmStage::First);
	TestEqual(TEXT("Первое нажатие ничего не стирает"), WipeSignals, 0);

	// «Отмена» на первом вопросе возвращает обычный экран, прогресс цел.
	Screen->HandleConfirmNoClicked();
	TestEqual(TEXT("«Отмена» закрыла переспрос"),
		Screen->GetResetConfirmStage(), EResetConfirmStage::None);
	TestEqual(TEXT("После отмены прогресс цел"), WipeSignals, 0);

	// Полный путь: вопрос — второй вопрос — и только теперь стирание.
	Screen->HandleResetProgressClicked();
	Screen->HandleConfirmYesClicked();
	TestEqual(TEXT("Второй вопрос открыт"),
		Screen->GetResetConfirmStage(), EResetConfirmStage::Second);
	TestEqual(TEXT("Второй вопрос ещё ничего не стирает"), WipeSignals, 0);

	Screen->HandleConfirmYesClicked();
	TestEqual(TEXT("Стирающий сигнал уходит ровно один раз"), WipeSignals, 1);
	TestEqual(TEXT("После стирания переспрос закрыт"),
		Screen->GetResetConfirmStage(), EResetConfirmStage::None);

	// «Отмена» на ВТОРОМ вопросе тоже спасает прогресс.
	Screen->HandleResetProgressClicked();
	Screen->HandleConfirmYesClicked();
	Screen->HandleConfirmNoClicked();
	TestEqual(TEXT("Отмена на втором вопросе не стирает"), WipeSignals, 1);
	TestEqual(TEXT("Экран вернулся в обычный вид"),
		Screen->GetResetConfirmStage(), EResetConfirmStage::None);

	// Уход «Назад» с открытым вопросом ничего не стирает и не оставляет вопрос висеть.
	Screen->HandleResetProgressClicked();
	int32 CloseSignals = 0;
	Screen->OnCloseRequested.AddLambda([&CloseSignals]() { ++CloseSignals; });
	Screen->HandleCloseClicked();
	TestEqual(TEXT("«Назад» закрывает экран"), CloseSignals, 1);
	TestEqual(TEXT("«Назад» снимает переспрос"),
		Screen->GetResetConfirmStage(), EResetConfirmStage::None);
	TestEqual(TEXT("«Назад» ничего не стирает"), WipeSignals, 1);
	return true;
}

// --- 9. Движок создаёт НАШ класс настроек -------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsEngineClassTest,
	"ContrarySurvivor.Settings.EngineCreatesOurSettingsClass", SettingsTestFlags)

bool FSettingsEngineClassTest::RunTest(const FString& Parameters)
{
	// Если строка GameUserSettingsClassName в Config/DefaultEngine.ini потеряется, движок
	// создаст базовый UGameUserSettings, и ВЕСЬ экран настроек станет пустышкой: значения
	// будет некуда писать. Этот тест ловит такую потерю сразу, а не на устройстве.
	UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get();
	TestNotNull(TEXT("Движок отдаёт наш класс настроек (строка в DefaultEngine.ini на месте)"), Settings);

	// Безопасные читалки обязаны работать даже без объекта настроек — на них живёт игра
	// в headless-тестах и на случай потери конфига.
	TestTrue(TEXT("Громкость эффектов в разумных границах"),
		UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe() >= 0.0f
		&& UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe() <= 1.0f);
	TestTrue(TEXT("Чувствительность в разумных границах"),
		UContrarySurvivorGameUserSettings::GetControlSensitivitySafe()
			>= UContrarySurvivorGameUserSettings::MinControlSensitivity
		&& UContrarySurvivorGameUserSettings::GetControlSensitivitySafe()
			<= UContrarySurvivorGameUserSettings::MaxControlSensitivity);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
