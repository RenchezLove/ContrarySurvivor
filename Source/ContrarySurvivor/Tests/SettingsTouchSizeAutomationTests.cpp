// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест «в настройки можно попасть пальцем» (жалоба Рината 08-09:
// «как само окно побольше сделать, так и кнопки и расстояния между ними; сейчас тяжеловато
// по ним пальцами попадать»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.SettingsTouch; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Требование измеримое: сторона области нажатия любого управляющего элемента — не меньше
// 9 мм на настоящем экране, а зазор между соседними строками — не меньше половины высоты
// строки (промах должен уходить в пустоту, а не в соседний переключатель).
//
// Порог тест НЕ зашивает числом: он берёт параметры телефона Рината (1600 на 720 точек в
// альбомной ориентации, плотность 320 точек на дюйм) и спрашивает масштаб интерфейса у
// самого движка (UUserInterfaceSettings::GetDPIScaleBasedOnSize), то есть считает по той же
// кривой, по которой игра рисует. Меняется кривая или экран — меняется и порог.
//
// Проверка идёт по ВСЕМУ перечислению EContrarySettingsControl, а не по выбранным кнопкам:
// добавили элемент на экран, забыли про палец — тест упадёт.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/UI/SettingsScreenWidget.h"
#include "Engine/UserInterfaceSettings.h"

static constexpr EAutomationTestFlags SettingsTouchTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace SettingsTouchTest
{
	// Телефон Рината: 720 на 1600 точек, игра идёт в альбомной ориентации.
	static const FIntPoint PhoneViewport(1600, 720);
	static constexpr float PhoneDpi = 320.0f;
	static constexpr float RequiredMillimeters = 9.0f;

	// Человеческое имя элемента для сообщения об ошибке.
	static FString ControlName(EContrarySettingsControl Control)
	{
		switch (Control)
		{
		case EContrarySettingsControl::PresetLow:         return TEXT("качество «Низкое»");
		case EContrarySettingsControl::PresetMedium:      return TEXT("качество «Среднее»");
		case EContrarySettingsControl::PresetHigh:        return TEXT("качество «Высокое»");
		case EContrarySettingsControl::PresetAuto:        return TEXT("качество «Авто»");
		case EContrarySettingsControl::ResolutionSlider:  return TEXT("ползунок масштаба разрешения");
		case EContrarySettingsControl::ResolutionMinus:   return TEXT("кнопка «−» масштаба");
		case EContrarySettingsControl::ResolutionPlus:    return TEXT("кнопка «+» масштаба");
		case EContrarySettingsControl::FrameLimit:        return TEXT("ограничение кадров");
		case EContrarySettingsControl::FpsCounter:        return TEXT("счётчик кадров");
		case EContrarySettingsControl::MusicSlider:       return TEXT("громкость музыки");
		case EContrarySettingsControl::EffectsSlider:     return TEXT("громкость эффектов");
		case EContrarySettingsControl::SensitivitySlider: return TEXT("чувствительность управления");
		case EContrarySettingsControl::OpacitySlider:     return TEXT("прозрачность экранных кнопок");
		case EContrarySettingsControl::Vibration:         return TEXT("вибрация");
		case EContrarySettingsControl::ReportBug:         return TEXT("сообщить об ошибке");
		case EContrarySettingsControl::ResetProgress:     return TEXT("сбросить прогресс");
		case EContrarySettingsControl::Close:             return TEXT("назад");
		case EContrarySettingsControl::ConfirmYes:        return TEXT("переспрос: да, сбросить");
		case EContrarySettingsControl::ConfirmNo:         return TEXT("переспрос: отмена");
		default:                                          return TEXT("неизвестный элемент");
		}
	}
}

// --- 1. Каждый элемент экрана настроек больше порога «под палец» ---------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsEveryControlFitsFingerTest,
	"ContrarySurvivor.SettingsTouch.EveryControlFitsFinger", SettingsTouchTestFlags)

bool FSettingsEveryControlFitsFingerTest::RunTest(const FString& Parameters)
{
	const FSettingsTouchLayout Layout;

	const float Threshold = USettingsScreenWidget::MinTouchPointsFor(
		SettingsTouchTest::RequiredMillimeters, SettingsTouchTest::PhoneDpi, SettingsTouchTest::PhoneViewport);

	// Масштаб интерфейса на этом экране — для отчёта: по нему видно, из чего вышел порог.
	float UiScale = 1.0f;
	if (const UUserInterfaceSettings* UiSettings = GetDefault<UUserInterfaceSettings>())
	{
		UiScale = UiSettings->GetDPIScaleBasedOnSize(SettingsTouchTest::PhoneViewport);
	}
	AddInfo(FString::Printf(
		TEXT("Экран %dx%d точек, плотность %.0f на дюйм, масштаб интерфейса %.3f — порог %.1f точки интерфейса на %.0f мм"),
		SettingsTouchTest::PhoneViewport.X, SettingsTouchTest::PhoneViewport.Y,
		SettingsTouchTest::PhoneDpi, UiScale, Threshold, SettingsTouchTest::RequiredMillimeters));

	TestTrue(TEXT("Порог посчитался положительным"), Threshold > 0.0f);

	// Заявленная наименьшая сторона обязана покрывать посчитанный порог: если однажды
	// поле уменьшат «на глазок», тест это поймает.
	TestTrue(FString::Printf(
		TEXT("Поле «наименьшая сторона кнопки» (%.0f) не меньше порога %.1f"), Layout.MinTouchSize, Threshold),
		Layout.MinTouchSize >= Threshold);

	// Главная проверка: перебор ВСЕХ элементов экрана.
	const int32 ControlCount = static_cast<int32>(EContrarySettingsControl::MAX_None);
	TestTrue(TEXT("Список элементов экрана не пуст"), ControlCount > 0);
	for (int32 Index = 0; Index < ControlCount; ++Index)
	{
		const EContrarySettingsControl Control = static_cast<EContrarySettingsControl>(Index);
		const FVector2D Box = USettingsScreenWidget::TouchBoxFor(Layout, Control);
		const FString Name = SettingsTouchTest::ControlName(Control);

		TestTrue(FString::Printf(TEXT("Ширина элемента «%s» (%.0f) не меньше порога %.1f"),
			*Name, Box.X, Threshold), Box.X >= Threshold);
		TestTrue(FString::Printf(TEXT("Высота элемента «%s» (%.0f) не меньше порога %.1f"),
			*Name, Box.Y, Threshold), Box.Y >= Threshold);

		// Зазор до соседней строки — не меньше половины высоты этой строки.
		TestTrue(FString::Printf(TEXT("Зазор (%.0f) не меньше половины высоты элемента «%s» (%.0f)"),
			Layout.RowGap, *Name, Box.Y * 0.5f), Layout.RowGap >= Box.Y * 0.5f);
	}

	// Ползунок: область нажатия у него та же, что у кнопки-строки (проверена перебором выше),
	// а сама полоска остаётся тонкой на вид — но НЕ волоском. Штатные 2 точки движка на этом
	// телефоне дают чуть больше одного пикселя: видно плохо, поэтому толщину задаём свою.
	const FSliderStyle SliderStyle = USettingsScreenWidget::MakeSliderStyle(Layout);
	TestEqual(TEXT("Толщина полоски ползунка взята из поля"),
		SliderStyle.BarThickness, Layout.SliderBarThickness);
	TestTrue(TEXT("Полоска ползунка толще штатного волоска в 2 точки"),
		SliderStyle.BarThickness > 2.0f);
	TestTrue(TEXT("Полоска ползунка тоньше половины строки — она не превращается в брусок"),
		SliderStyle.BarThickness < Layout.MinTouchSize * 0.5f);
	TestEqual(TEXT("Размер бегунка взят из поля"),
		static_cast<float>(SliderStyle.NormalThumbImage.ImageSize.X), Layout.SliderHandleSize);
	TestTrue(TEXT("Бегунок заметный"), Layout.SliderHandleSize >= 24.0f);
	return true;
}

// --- 2. Само окно стало больше и вмещает строки ---------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsPanelIsBigEnoughTest,
	"ContrarySurvivor.SettingsTouch.PanelIsBigEnough", SettingsTouchTestFlags)

bool FSettingsPanelIsBigEnoughTest::RunTest(const FString& Parameters)
{
	const FSettingsTouchLayout Layout;

	// Окно должно быть шире самой широкой строки — иначе строка вылезет за подложку.
	const FVector2D WidestRow = USettingsScreenWidget::TouchBoxFor(Layout, EContrarySettingsControl::FrameLimit);
	TestTrue(FString::Printf(TEXT("Окно (%.0f) шире самой широкой строки (%.0f)"),
		Layout.PanelSize.X, WidestRow.X), Layout.PanelSize.X > WidestRow.X);

	// В окно должно помещаться хотя бы три строки с зазорами — иначе список нечитаем.
	const float ThreeRows = 3.0f * WidestRow.Y + 2.0f * Layout.RowGap;
	TestTrue(FString::Printf(TEXT("В окно (%.0f по высоте) влезает хотя бы три строки (%.0f)"),
		Layout.PanelSize.Y, ThreeRows), Layout.PanelSize.Y >= ThreeRows);

	// Окно обязано помещаться на экран телефона целиком: высота холста интерфейса при
	// альбомных 1600x720 — это 720, делённые на масштаб интерфейса.
	float UiScale = 1.0f;
	if (const UUserInterfaceSettings* UiSettings = GetDefault<UUserInterfaceSettings>())
	{
		UiScale = UiSettings->GetDPIScaleBasedOnSize(SettingsTouchTest::PhoneViewport);
	}
	const float CanvasHeight = SettingsTouchTest::PhoneViewport.Y / FMath::Max(UiScale, 0.01f);
	const float CanvasWidth = SettingsTouchTest::PhoneViewport.X / FMath::Max(UiScale, 0.01f);
	AddInfo(FString::Printf(TEXT("Холст интерфейса на телефоне: %.0f на %.0f точки"), CanvasWidth, CanvasHeight));
	TestTrue(FString::Printf(TEXT("Окно по высоте (%.0f) помещается на холст (%.0f)"),
		Layout.PanelSize.Y, CanvasHeight), Layout.PanelSize.Y <= CanvasHeight);
	TestTrue(FString::Printf(TEXT("Окно по ширине (%.0f) помещается на холст (%.0f)"),
		Layout.PanelSize.X, CanvasWidth), Layout.PanelSize.X <= CanvasWidth);

	// И на обычном экране 1920x1080 тоже (там масштаб интерфейса равен единице).
	TestTrue(TEXT("Окно помещается и на экране 1920 на 1080"),
		Layout.PanelSize.X <= 1920.0f && Layout.PanelSize.Y <= 1080.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
