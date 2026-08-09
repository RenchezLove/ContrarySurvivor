// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/SettingsScreenWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/UI/StartScreenWidget.h" // UMainMenuSettings: адрес «Сообщить об ошибке»
#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h" // номер версии сборки для отчёта
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h" // масштаб интерфейса от размера экрана (порог «под палец»)
#include "HAL/PlatformProcess.h" // FPlatformProcess::LaunchURL («Сообщить об ошибке»)
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SettingsScreenWidget"

namespace
{
	// Вид кодового дерева-фолбэка. Живой вид экрана правится мышкой в WBP_Settings (ADR-048),
	// поэтому отдельной настраиваемой структуры стиля здесь нет — только разумные значения,
	// повторяющие палитру остальных модальных окон (главное меню, меню паузы).
	const FLinearColor DimColor(0.0f, 0.0f, 0.0f, 0.7f);
	const FLinearColor FrameColor(0.8f, 0.65f, 0.25f, 0.9f);
	const FLinearColor PanelColor(0.06f, 0.07f, 0.09f, 0.97f);
	const FLinearColor TitleColor(1.0f, 0.85f, 0.2f, 1.0f);
	const FLinearColor LabelColor(0.85f, 0.85f, 0.85f, 1.0f);
	const FLinearColor ValueColor(1.0f, 0.97f, 0.7f, 1.0f);
	const FLinearColor ButtonTextColor(0.05f, 0.05f, 0.05f, 1.0f);

}

// ---------------------------------------------------------------------------
// Чистые правила
// ---------------------------------------------------------------------------

float USettingsScreenWidget::MinTouchPointsFor(float Millimeters, float ScreenDpi, FIntPoint ViewportSize)
{
	// Физический размер -> точки экрана: миллиметры делим на 25.4 (дюйм) и умножаем на плотность.
	const float DevicePixels = (Millimeters / 25.4f) * FMath::Max(ScreenDpi, 1.0f);

	// Точки экрана -> точки интерфейса: движок множит наши точки на масштаб, который сам
	// считает по размеру вьюпорта (правило и кривая из настроек проекта). Спрашиваем его,
	// а не держим свою копию кривой — иначе расчёт разъедется с игрой при первой же правке.
	float UiScale = 1.0f;
	if (const UUserInterfaceSettings* UiSettings = GetDefault<UUserInterfaceSettings>())
	{
		UiScale = UiSettings->GetDPIScaleBasedOnSize(ViewportSize);
	}
	return DevicePixels / FMath::Max(UiScale, 0.01f);
}

FVector2D USettingsScreenWidget::TouchBoxFor(const FSettingsTouchLayout& Layout, EContrarySettingsControl Control)
{
	const float Side = FMath::Max(Layout.MinTouchSize, 1.0f);
	const float Wide = FMath::Max(Layout.RowWidth, Side);

	switch (Control)
	{
	// Кнопки шага масштаба — с одним знаком внутри («−» и «+»), поэтому квадратные:
	// узкими их делать нельзя, палец должен попадать по обеим сторонам.
	case EContrarySettingsControl::ResolutionMinus:
	case EContrarySettingsControl::ResolutionPlus:
		return FVector2D(Side, Side);

	// Кнопки переспроса стоят парой в своём окошке — им своя ширина.
	case EContrarySettingsControl::ConfirmYes:
	case EContrarySettingsControl::ConfirmNo:
		return FVector2D(FMath::Max(Layout.ConfirmButtonWidth, Side), Side);

	// Всё остальное — строки во всю ширину списка: и кнопки-переключатели, и ползунки.
	// Ползунку высота нужна не меньше, чем кнопке: тянуть его надо тем же пальцем.
	default:
		return FVector2D(Wide, Side);
	}
}

ESlateVisibility USettingsScreenWidget::ReportBugVisibilityFor(const FString& BugReportUrl)
{
	return BugReportUrl.TrimStartAndEnd().IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
}

int32 USettingsScreenWidget::SliderValueToResolutionPercent(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.0f, 1.0f);
	const float Percent = FMath::Lerp(
		static_cast<float>(UContrarySurvivorGameUserSettings::MinResolutionScalePercent),
		static_cast<float>(UContrarySurvivorGameUserSettings::MaxResolutionScalePercent),
		Clamped);
	// Шаг 10 — правило одно на весь проект, живёт в настройках (спека: «с шагом 10»).
	return UContrarySurvivorGameUserSettings::SnapResolutionScalePercent(FMath::RoundToInt32(Percent));
}

float USettingsScreenWidget::ResolutionPercentToSliderValue(int32 Percent)
{
	const int32 Snapped = UContrarySurvivorGameUserSettings::SnapResolutionScalePercent(Percent);
	const int32 Span = UContrarySurvivorGameUserSettings::MaxResolutionScalePercent
		- UContrarySurvivorGameUserSettings::MinResolutionScalePercent;
	return static_cast<float>(Snapped - UContrarySurvivorGameUserSettings::MinResolutionScalePercent)
		/ static_cast<float>(Span);
}

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void USettingsScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	// Дерево владельца из WBP уже построено (детект как в StartScreenWidget/EndOfStoryWidget).
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ TitleText, TEXT("TitleText") },
			{ PresetValueText, TEXT("PresetValueText") },
			{ PresetLowButton, TEXT("PresetLowButton") }, { PresetMediumButton, TEXT("PresetMediumButton") },
			{ PresetHighButton, TEXT("PresetHighButton") }, { PresetAutoButton, TEXT("PresetAutoButton") },
			{ ResolutionSlider, TEXT("ResolutionSlider") }, { ResolutionValueText, TEXT("ResolutionValueText") },
			{ ResolutionMinusButton, TEXT("ResolutionMinusButton") },
			{ ResolutionPlusButton, TEXT("ResolutionPlusButton") },
			{ FrameLimitButton, TEXT("FrameLimitButton") }, { FrameLimitText, TEXT("FrameLimitText") },
			{ FpsCounterButton, TEXT("FpsCounterButton") }, { FpsCounterText, TEXT("FpsCounterText") },
			{ MusicSlider, TEXT("MusicSlider") }, { MusicValueText, TEXT("MusicValueText") },
			{ EffectsSlider, TEXT("EffectsSlider") }, { EffectsValueText, TEXT("EffectsValueText") },
			{ SensitivitySlider, TEXT("SensitivitySlider") }, { SensitivityValueText, TEXT("SensitivityValueText") },
			{ OpacitySlider, TEXT("OpacitySlider") }, { OpacityValueText, TEXT("OpacityValueText") },
			{ VibrationButton, TEXT("VibrationButton") }, { VibrationText, TEXT("VibrationText") },
			{ ReportBugButton, TEXT("ReportBugButton") }, { ReportBugText, TEXT("ReportBugText") },
			{ ResetProgressButton, TEXT("ResetProgressButton") }, { ResetProgressText, TEXT("ResetProgressText") },
			{ CloseButton, TEXT("CloseButton") }, { CloseText, TEXT("CloseText") },
			{ ConfirmPanel, TEXT("ConfirmPanel") }, { ConfirmTitleText, TEXT("ConfirmTitleText") },
			{ ConfirmYesButton, TEXT("ConfirmYesButton") }, { ConfirmNoButton, TEXT("ConfirmNoButton") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("SettingsScreenWidget: кубик %s не найден в WBP_Settings — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики и ползунки — в обоих путях: из WBP приходят сами кубики, обработчики всё равно наши.
	if (PresetLowButton)    { PresetLowButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandlePresetLowClicked); }
	if (PresetMediumButton) { PresetMediumButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandlePresetMediumClicked); }
	if (PresetHighButton)   { PresetHighButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandlePresetHighClicked); }
	if (PresetAutoButton)   { PresetAutoButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandlePresetAutoClicked); }
	if (ResolutionMinusButton) { ResolutionMinusButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleResolutionMinusClicked); }
	if (ResolutionPlusButton)  { ResolutionPlusButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleResolutionPlusClicked); }
	if (FrameLimitButton)   { FrameLimitButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleFrameLimitClicked); }
	if (FpsCounterButton)   { FpsCounterButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleFpsCounterClicked); }
	if (VibrationButton)    { VibrationButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleVibrationClicked); }
	if (ReportBugButton)    { ReportBugButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleReportBugClicked); }
	if (ResetProgressButton) { ResetProgressButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleResetProgressClicked); }
	if (ConfirmYesButton)   { ConfirmYesButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleConfirmYesClicked); }
	if (ConfirmNoButton)    { ConfirmNoButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleConfirmNoClicked); }
	if (CloseButton)        { CloseButton->OnClicked.AddDynamic(this, &USettingsScreenWidget::HandleCloseClicked); }

	if (ResolutionSlider)  { ResolutionSlider->OnValueChanged.AddDynamic(this, &USettingsScreenWidget::HandleResolutionSliderChanged); }
	if (MusicSlider)       { MusicSlider->OnValueChanged.AddDynamic(this, &USettingsScreenWidget::HandleMusicSliderChanged); }
	if (EffectsSlider)     { EffectsSlider->OnValueChanged.AddDynamic(this, &USettingsScreenWidget::HandleEffectsSliderChanged); }
	if (SensitivitySlider) { SensitivitySlider->OnValueChanged.AddDynamic(this, &USettingsScreenWidget::HandleSensitivitySliderChanged); }
	if (OpacitySlider)     { OpacitySlider->OnValueChanged.AddDynamic(this, &USettingsScreenWidget::HandleOpacitySliderChanged); }
}

void USettingsScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Экран может открываться не один раз за сессию — значения и подписи освежаются при
	// каждом появлении (паттерн меню паузы и главного меню).
	RefreshFromSettings();
}

void USettingsScreenWidget::RefreshFromSettings()
{
	// Переспрос сброса не должен «переживать» закрытие экрана: открылись заново — обычный вид.
	ResetConfirmStage = EResetConfirmStage::None;

	const UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get();
	if (Settings)
	{
		if (ResolutionSlider)
		{
			ResolutionSlider->SetValue(ResolutionPercentToSliderValue(Settings->GetResolutionScalePercent()));
		}
		if (MusicSlider)       { MusicSlider->SetValue(Settings->GetMusicVolume()); }
		if (EffectsSlider)     { EffectsSlider->SetValue(Settings->GetEffectsVolume()); }
		if (SensitivitySlider)
		{
			const float Span = UContrarySurvivorGameUserSettings::MaxControlSensitivity
				- UContrarySurvivorGameUserSettings::MinControlSensitivity;
			SensitivitySlider->SetValue((Settings->GetControlSensitivity()
				- UContrarySurvivorGameUserSettings::MinControlSensitivity) / Span);
		}
		if (OpacitySlider)
		{
			const float Span = UContrarySurvivorGameUserSettings::MaxTouchButtonsOpacity
				- UContrarySurvivorGameUserSettings::MinTouchButtonsOpacity;
			OpacitySlider->SetValue((Settings->GetTouchButtonsOpacity()
				- UContrarySurvivorGameUserSettings::MinTouchButtonsOpacity) / Span);
		}
	}

	RefreshLabels();
	RefreshConfirmPanel();
}

// ---------------------------------------------------------------------------
// Подписи
// ---------------------------------------------------------------------------

FIntPoint USettingsScreenWidget::GetViewportPixelSize() const
{
	// Спека требует показать «итоговые пиксели на этом экране» — берём реальный размер
	// вьюпорта. Его может не быть (headless/тест) — тогда подпись честно остаётся без пикселей.
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D Size = FVector2D::ZeroVector;
		GEngine->GameViewport->GetViewportSize(Size);
		return FIntPoint(FMath::RoundToInt32(Size.X), FMath::RoundToInt32(Size.Y));
	}
	return FIntPoint::ZeroValue;
}

void USettingsScreenWidget::RefreshLabels()
{
	const UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get();

	if (TitleText)
	{
		TitleText->SetText(LOCTEXT("Title", "НАСТРОЙКИ"));
	}
	if (GraphicsHeaderText) { GraphicsHeaderText->SetText(LOCTEXT("GraphicsHeader", "Картинка")); }
	if (SoundHeaderText)    { SoundHeaderText->SetText(LOCTEXT("SoundHeader", "Звук")); }
	if (ControlsHeaderText) { ControlsHeaderText->SetText(LOCTEXT("ControlsHeader", "Управление")); }
	if (MiscHeaderText)     { MiscHeaderText->SetText(LOCTEXT("MiscHeader", "Прочее")); }

	if (PresetLowText)    { PresetLowText->SetText(UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::Low)); }
	if (PresetMediumText) { PresetMediumText->SetText(UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::Medium)); }
	if (PresetHighText)   { PresetHighText->SetText(UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::High)); }
	if (PresetAutoText)   { PresetAutoText->SetText(UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::Auto)); }

	if (PresetValueText)
	{
		// Спека: при «Авто» игрок обязан видеть, какой пресет выбран за него.
		const FText Value = Settings
			? UContrarySurvivorGameUserSettings::MakePresetLabel(Settings->GetQualityPreset(), Settings->GetEffectivePreset())
			: UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::Auto);
		FFormatNamedArguments Args;
		Args.Add(TEXT("Value"), Value);
		PresetValueText->SetText(FText::Format(LOCTEXT("PresetRow", "Качество картинки: {Value}"), Args));
	}

	if (ResolutionLabelText)
	{
		ResolutionLabelText->SetText(LOCTEXT("ResolutionRow", "Масштаб разрешения"));
	}
	if (ResolutionValueText)
	{
		const int32 Percent = Settings ? Settings->GetResolutionScalePercent()
			: UContrarySurvivorGameUserSettings::DefaultResolutionScalePercent;
		ResolutionValueText->SetText(
			UContrarySurvivorGameUserSettings::MakeResolutionScaleLabel(Percent, GetViewportPixelSize()));
	}
	if (ResolutionMinusText) { ResolutionMinusText->SetText(LOCTEXT("Minus", "−")); }
	if (ResolutionPlusText)  { ResolutionPlusText->SetText(LOCTEXT("Plus", "+")); }

	if (FrameLimitText)
	{
		const bool bLimited = Settings ? Settings->IsFrameRateLimitedTo30() : true;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Value"), bLimited
			? LOCTEXT("FrameLimit30", "30 кадров")
			: LOCTEXT("FrameLimitOff", "без ограничения"));
		FrameLimitText->SetText(FText::Format(LOCTEXT("FrameLimitRow", "Ограничение кадров: {Value}"), Args));
	}
	if (FpsCounterText)
	{
		const bool bShown = Settings ? Settings->IsFpsCounterShown() : false;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Value"), bShown ? LOCTEXT("On", "включён") : LOCTEXT("Off", "выключен"));
		FpsCounterText->SetText(FText::Format(LOCTEXT("FpsCounterRow", "Счётчик кадров: {Value}"), Args));
	}

	if (MusicLabelText)   { MusicLabelText->SetText(LOCTEXT("MusicRow", "Громкость музыки")); }
	if (EffectsLabelText) { EffectsLabelText->SetText(LOCTEXT("EffectsRow", "Громкость эффектов")); }
	if (MusicValueText)
	{
		MusicValueText->SetText(UContrarySurvivorGameUserSettings::MakePercentLabel(
			Settings ? Settings->GetMusicVolume() : UContrarySurvivorGameUserSettings::DefaultVolume));
	}
	if (EffectsValueText)
	{
		EffectsValueText->SetText(UContrarySurvivorGameUserSettings::MakePercentLabel(
			Settings ? Settings->GetEffectsVolume() : UContrarySurvivorGameUserSettings::DefaultVolume));
	}

	if (SensitivityLabelText) { SensitivityLabelText->SetText(LOCTEXT("SensitivityRow", "Чувствительность управления")); }
	if (OpacityLabelText)     { OpacityLabelText->SetText(LOCTEXT("OpacityRow", "Прозрачность экранных кнопок")); }
	if (SensitivityValueText)
	{
		SensitivityValueText->SetText(UContrarySurvivorGameUserSettings::MakePercentLabel(
			Settings ? Settings->GetControlSensitivity() : UContrarySurvivorGameUserSettings::DefaultControlSensitivity));
	}
	if (OpacityValueText)
	{
		OpacityValueText->SetText(UContrarySurvivorGameUserSettings::MakePercentLabel(
			Settings ? Settings->GetTouchButtonsOpacity() : UContrarySurvivorGameUserSettings::DefaultTouchButtonsOpacity));
	}
	if (VibrationText)
	{
		const bool bOn = Settings ? Settings->IsVibrationEnabled() : true;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Value"), bOn ? LOCTEXT("VibrationOn", "включена") : LOCTEXT("VibrationOff", "выключена"));
		VibrationText->SetText(FText::Format(LOCTEXT("VibrationRow", "Вибрация: {Value}"), Args));
	}

	if (ReportBugText)      { ReportBugText->SetText(LOCTEXT("ReportBug", "Сообщить об ошибке")); }
	if (ResetProgressText)  { ResetProgressText->SetText(LOCTEXT("ResetProgress", "Сбросить прогресс")); }
	if (CloseText)          { CloseText->SetText(LOCTEXT("Close", "Назад")); }

	// Спека: «Сообщить об ошибке» ведёт в Telegram «с просьбой указать номер версии сборки».
	// Номер берём из того же места, что меню паузы и главное меню (UDataConsentSubsystem).
	if (ReportBugHintText)
	{
		const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this);
		FFormatNamedArguments Args;
		Args.Add(TEXT("Version"), Consent ? Consent->GetBuildVersionText() : FText::GetEmpty());
		ReportBugHintText->SetText(FText::Format(
			LOCTEXT("ReportBugHint", "В сообщении укажите номер сборки: {Version}"), Args));
	}

	// Пункт «Сообщить об ошибке» без адреса в конфиге спрятан целиком — то же правило,
	// что у «Сообщества» главного меню (ADR-062).
	const ESlateVisibility BugVisibility = ReportBugVisibilityFor(UMainMenuSettings::GetEffectiveBugReportUrl());
	SetRowVisibility(ReportBugButton, BugVisibility);
	SetRowVisibility(ReportBugHintText, BugVisibility);
}

void USettingsScreenWidget::RefreshConfirmPanel()
{
	const bool bAsking = ResetConfirmStage != EResetConfirmStage::None;

	if (ConfirmPanel)
	{
		ConfirmPanel->SetVisibility(bAsking ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bAsking)
	{
		return;
	}

	// Двойной переспрос: два РАЗНЫХ вопроса подряд, второй прямо говорит о необратимости.
	if (ConfirmTitleText)
	{
		ConfirmTitleText->SetText(ResetConfirmStage == EResetConfirmStage::First
			? LOCTEXT("ResetAsk1", "Сбросить весь прогресс?")
			: LOCTEXT("ResetAsk2", "Точно стереть? Вернуть прогресс будет нельзя."));
	}
	if (ConfirmYesText)
	{
		ConfirmYesText->SetText(ResetConfirmStage == EResetConfirmStage::First
			? LOCTEXT("ResetYes1", "Да, сбросить")
			: LOCTEXT("ResetYes2", "Да, стереть навсегда"));
	}
	if (ConfirmNoText)
	{
		ConfirmNoText->SetText(LOCTEXT("ResetNo", "Отмена"));
	}
}

void USettingsScreenWidget::NotifyChanged()
{
	RefreshLabels();
	OnSettingsChanged.Broadcast();
}

// ---------------------------------------------------------------------------
// Обработчики
// ---------------------------------------------------------------------------

void USettingsScreenWidget::HandlePresetLowClicked()
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetQualityPreset(EContraryGraphicsPreset::Low);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandlePresetMediumClicked()
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetQualityPreset(EContraryGraphicsPreset::Medium);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandlePresetHighClicked()
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetQualityPreset(EContraryGraphicsPreset::High);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandlePresetAutoClicked()
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetQualityPreset(EContraryGraphicsPreset::Auto);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleResolutionMinusClicked()
{
	UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	Settings->SetResolutionScalePercent(Settings->GetResolutionScalePercent()
		- UContrarySurvivorGameUserSettings::ResolutionScaleStepPercent);
	if (ResolutionSlider)
	{
		ResolutionSlider->SetValue(ResolutionPercentToSliderValue(Settings->GetResolutionScalePercent()));
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleResolutionPlusClicked()
{
	UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	Settings->SetResolutionScalePercent(Settings->GetResolutionScalePercent()
		+ UContrarySurvivorGameUserSettings::ResolutionScaleStepPercent);
	if (ResolutionSlider)
	{
		ResolutionSlider->SetValue(ResolutionPercentToSliderValue(Settings->GetResolutionScalePercent()));
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleResolutionSliderChanged(float NewValue)
{
	UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	const int32 Percent = SliderValueToResolutionPercent(NewValue);
	if (Percent == Settings->GetResolutionScalePercent())
	{
		return; // палец ведёт внутри одного шага — лишний раз графику не пересобираем
	}
	Settings->SetResolutionScalePercent(Percent);
	NotifyChanged();
}

void USettingsScreenWidget::HandleFrameLimitClicked()
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetFrameRateLimitedTo30(!Settings->IsFrameRateLimitedTo30());
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleFpsCounterClicked()
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetFpsCounterShown(!Settings->IsFpsCounterShown());
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleMusicSliderChanged(float NewValue)
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetMusicVolume(NewValue);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleEffectsSliderChanged(float NewValue)
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetEffectsVolume(NewValue);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleSensitivitySliderChanged(float NewValue)
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		const float Span = UContrarySurvivorGameUserSettings::MaxControlSensitivity
			- UContrarySurvivorGameUserSettings::MinControlSensitivity;
		Settings->SetControlSensitivity(
			UContrarySurvivorGameUserSettings::MinControlSensitivity + FMath::Clamp(NewValue, 0.0f, 1.0f) * Span);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleOpacitySliderChanged(float NewValue)
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		const float Span = UContrarySurvivorGameUserSettings::MaxTouchButtonsOpacity
			- UContrarySurvivorGameUserSettings::MinTouchButtonsOpacity;
		Settings->SetTouchButtonsOpacity(
			UContrarySurvivorGameUserSettings::MinTouchButtonsOpacity + FMath::Clamp(NewValue, 0.0f, 1.0f) * Span);
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleVibrationClicked()
{
	if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
	{
		Settings->SetVibrationEnabled(!Settings->IsVibrationEnabled());
	}
	NotifyChanged();
}

void USettingsScreenWidget::HandleReportBugClicked()
{
	const FString Url = UMainMenuSettings::GetEffectiveBugReportUrl();
	if (Url.IsEmpty())
	{
		// Пункт при пустом адресе спрятан целиком, штатно сюда не попасть — строка в журнал.
		UE_LOG(LogQA, Display, TEXT("QA: settings REPORT BUG clicked (no url in config)"));
		return;
	}
	FString Error;
	FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
	UE_LOG(LogQA, Display, TEXT("QA: settings REPORT BUG clicked, url '%s'%s%s"),
		*Url, Error.IsEmpty() ? TEXT("") : TEXT(", error: "), *Error);
}

void USettingsScreenWidget::HandleResetProgressClicked()
{
	// Первое нажатие НИЧЕГО не стирает — открывает первый вопрос (спека: двойной переспрос).
	ResetConfirmStage = EResetConfirmStage::First;
	RefreshConfirmPanel();
	UE_LOG(LogQA, Display, TEXT("QA: settings RESET asked (stage 1)"));
}

void USettingsScreenWidget::HandleConfirmYesClicked()
{
	if (ResetConfirmStage == EResetConfirmStage::First)
	{
		// Второй вопрос — и он тоже ничего не стирает.
		ResetConfirmStage = EResetConfirmStage::Second;
		RefreshConfirmPanel();
		UE_LOG(LogQA, Display, TEXT("QA: settings RESET asked (stage 2)"));
		return;
	}
	if (ResetConfirmStage == EResetConfirmStage::Second)
	{
		// Стадия закрывается ДО сигнала владельцу: кешированный виджет при следующем показе
		// обязан открыться обычным экраном (та же ловушка, что у переспроса «Новая игра»).
		ResetConfirmStage = EResetConfirmStage::None;
		RefreshConfirmPanel();
		UE_LOG(LogQA, Display, TEXT("QA: settings RESET confirmed — wiping save"));
		OnResetProgressConfirmed.Broadcast();
	}
}

void USettingsScreenWidget::HandleConfirmNoClicked()
{
	ResetConfirmStage = EResetConfirmStage::None;
	RefreshConfirmPanel();
}

void USettingsScreenWidget::HandleCloseClicked()
{
	// Открытый переспрос закрывается вместе с экраном — «Назад» ничего не стирает.
	ResetConfirmStage = EResetConfirmStage::None;
	RefreshConfirmPanel();
	OnCloseRequested.Broadcast();
}

// ---------------------------------------------------------------------------
// Кодовое дерево-фолбэк
// ---------------------------------------------------------------------------

void USettingsScreenWidget::SetRowVisibility(UWidget* Widget, ESlateVisibility InVisibility)
{
	if (!Widget)
	{
		return;
	}
	UWidget* Row = Widget;
	if (USizeBox* Box = Cast<USizeBox>(Widget->GetParent()))
	{
		Row = Box;
	}
	Row->SetVisibility(InVisibility);
}

UTextBlock* USettingsScreenWidget::MakeSectionHeader(UVerticalBox* Column, const FName& Name, const FText& Caption)
{
	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Header->SetText(Caption);
	Header->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, TouchLayout.HeaderFontSize)));
	Header->SetColorAndOpacity(FSlateColor(TitleColor));
	if (UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, TouchLayout.RowGap, 0.0f, TouchLayout.RowGap * 0.25f));
	}
	return Header;
}

UButton* USettingsScreenWidget::MakeRowButton(UVerticalBox* Column, const FName& BaseName,
	const FText& Caption, TObjectPtr<UTextBlock>& OutCaption, EContrarySettingsControl Control)
{
	// SizeBox задаёт габарит под палец (у UButton 5.5 нет SetPadding) — и он же прячется,
	// когда строка не нужна (иначе в колонке осталось бы пустое место).
	const FVector2D BoxSize = TouchBoxFor(TouchLayout, Control);
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Box"))));
	Box->SetWidthOverride(BoxSize.X);
	Box->SetHeightOverride(BoxSize.Y);
	ButtonBoxes.Add(Box);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), BaseName);
	Box->SetContent(Button);

	OutCaption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Label"))));
	OutCaption->SetText(Caption);
	OutCaption->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, TouchLayout.ButtonFontSize)));
	OutCaption->SetColorAndOpacity(FSlateColor(ButtonTextColor));
	OutCaption->SetJustification(ETextJustify::Center);
	Button->SetContent(OutCaption);

	if (UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		// Зазор снизу — не меньше половины высоты строки: промах мимо кнопки должен уходить
		// в пустоту, а не в соседний переключатель.
		BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TouchLayout.RowGap));
	}
	return Button;
}

USlider* USettingsScreenWidget::MakeRowSlider(UVerticalBox* Column, const FName& BaseName,
	const FText& Label, TObjectPtr<UTextBlock>& OutValueText, EContrarySettingsControl Control)
{
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Caption"))));
	LabelText->SetText(Label);
	LabelText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, TouchLayout.LabelFontSize)));
	LabelText->SetColorAndOpacity(FSlateColor(LabelColor));
	Column->AddChildToVerticalBox(LabelText);

	// Ползунок тоже в габаритной коробке: тянуть его надо тем же пальцем, что жать кнопки,
	// а собственная высота USlider — тонкая полоска в пару миллиметров.
	const FVector2D BoxSize = TouchBoxFor(TouchLayout, Control);
	USizeBox* SliderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Box"))));
	SliderBox->SetWidthOverride(BoxSize.X);
	SliderBox->SetHeightOverride(BoxSize.Y);
	ButtonBoxes.Add(SliderBox);

	USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), BaseName);
	Slider->SetStepSize(0.1f);
	SliderBox->SetContent(Slider);
	if (UVerticalBoxSlot* SliderSlot = Column->AddChildToVerticalBox(SliderBox))
	{
		SliderSlot->SetHorizontalAlignment(HAlign_Center);
		SliderSlot->SetPadding(FMargin(0.0f, TouchLayout.RowGap * 0.25f, 0.0f, 0.0f));
	}

	OutValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Value"))));
	OutValueText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, TouchLayout.LabelFontSize)));
	OutValueText->SetColorAndOpacity(FSlateColor(ValueColor));
	if (UVerticalBoxSlot* ValueSlot = Column->AddChildToVerticalBox(OutValueText))
	{
		ValueSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TouchLayout.RowGap));
	}
	return Slider;
}

void USettingsScreenWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SettingsRoot"));
	WidgetTree->RootWidget = Root;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы клик мимо кнопок не ушёл в мир.
	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	DimBorder->SetBrushColor(DimColor);
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimBorder))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsFrame"));
	FrameBorder->SetBrushColor(FrameColor);
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsPanel"));
	PanelBorder->SetBrushColor(PanelColor);
	PanelBorder->SetPadding(FMargin(24.0f, 18.0f));
	FrameBorder->SetContent(PanelBorder);

	// Настроек много, экран телефона низкий — список прокручивается пальцем.
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SettingsScroll"));
	PanelBorder->SetContent(Scroll);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsColumn"));
	Scroll->AddChild(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(LOCTEXT("Title", "НАСТРОЙКИ"));
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, TouchLayout.TitleFontSize)));
	TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TouchLayout.RowGap * 0.5f));
	}

	// Мелкая строка-подпись (не элемент управления): один вид на все такие строки.
	auto MakeInfoLine = [this, Column](const FName& Name, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Line->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, TouchLayout.LabelFontSize)));
		Line->SetColorAndOpacity(FSlateColor(Color));
		if (UVerticalBoxSlot* LineSlot = Column->AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TouchLayout.RowGap * 0.25f));
		}
		return Line;
	};

	// --- Картинка ---
	GraphicsHeaderText = MakeSectionHeader(Column, TEXT("GraphicsHeaderText"), LOCTEXT("GraphicsHeader", "Картинка"));

	PresetValueText = MakeInfoLine(TEXT("PresetValueText"), ValueColor);

	PresetLowButton = MakeRowButton(Column, TEXT("PresetLowButton"),
		UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::Low), PresetLowText,
		EContrarySettingsControl::PresetLow);
	PresetMediumButton = MakeRowButton(Column, TEXT("PresetMediumButton"),
		UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::Medium), PresetMediumText,
		EContrarySettingsControl::PresetMedium);
	PresetHighButton = MakeRowButton(Column, TEXT("PresetHighButton"),
		UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::High), PresetHighText,
		EContrarySettingsControl::PresetHigh);
	PresetAutoButton = MakeRowButton(Column, TEXT("PresetAutoButton"),
		UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset::Auto), PresetAutoText,
		EContrarySettingsControl::PresetAuto);

	ResolutionLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResolutionLabelText"));
	ResolutionLabelText->SetText(LOCTEXT("ResolutionRow", "Масштаб разрешения"));
	ResolutionLabelText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, TouchLayout.LabelFontSize)));
	ResolutionLabelText->SetColorAndOpacity(FSlateColor(LabelColor));
	Column->AddChildToVerticalBox(ResolutionLabelText);

	{
		const FVector2D ResBoxSize = TouchBoxFor(TouchLayout, EContrarySettingsControl::ResolutionSlider);
		USizeBox* ResBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ResolutionSliderBox"));
		ResBox->SetWidthOverride(ResBoxSize.X);
		ResBox->SetHeightOverride(ResBoxSize.Y);
		ButtonBoxes.Add(ResBox);

		ResolutionSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("ResolutionSlider"));
		// Шаг ползунка — ровно шаг спеки (10% от 50 до 100 = пять шагов на всю длину).
		ResolutionSlider->SetStepSize(1.0f / 5.0f);
		ResBox->SetContent(ResolutionSlider);
		if (UVerticalBoxSlot* ResSliderSlot = Column->AddChildToVerticalBox(ResBox))
		{
			ResSliderSlot->SetHorizontalAlignment(HAlign_Center);
			ResSliderSlot->SetPadding(FMargin(0.0f, TouchLayout.RowGap * 0.25f, 0.0f, 0.0f));
		}
	}

	ResolutionValueText = MakeInfoLine(TEXT("ResolutionValueText"), ValueColor);

	// Кнопки шага рядом с ползунком: на телефоне точное попадание пальцем в шаг ползунка
	// неудобно, а шаг фиксированный — «−» и «+» дают его гарантированно. Они квадратные
	// (в них один знак), но сторона у них та же, что высота строки — палец попадает.
	ResolutionMinusButton = MakeRowButton(Column, TEXT("ResolutionMinusButton"), LOCTEXT("Minus", "−"),
		ResolutionMinusText, EContrarySettingsControl::ResolutionMinus);
	ResolutionPlusButton = MakeRowButton(Column, TEXT("ResolutionPlusButton"), LOCTEXT("Plus", "+"),
		ResolutionPlusText, EContrarySettingsControl::ResolutionPlus);

	FrameLimitButton = MakeRowButton(Column, TEXT("FrameLimitButton"), FText::GetEmpty(), FrameLimitText,
		EContrarySettingsControl::FrameLimit);
	FpsCounterButton = MakeRowButton(Column, TEXT("FpsCounterButton"), FText::GetEmpty(), FpsCounterText,
		EContrarySettingsControl::FpsCounter);

	// --- Звук ---
	SoundHeaderText = MakeSectionHeader(Column, TEXT("SoundHeaderText"), LOCTEXT("SoundHeader", "Звук"));
	MusicSlider = MakeRowSlider(Column, TEXT("MusicSlider"), LOCTEXT("MusicRow", "Громкость музыки"),
		MusicValueText, EContrarySettingsControl::MusicSlider);
	EffectsSlider = MakeRowSlider(Column, TEXT("EffectsSlider"), LOCTEXT("EffectsRow", "Громкость эффектов"),
		EffectsValueText, EContrarySettingsControl::EffectsSlider);

	// --- Управление ---
	ControlsHeaderText = MakeSectionHeader(Column, TEXT("ControlsHeaderText"), LOCTEXT("ControlsHeader", "Управление"));
	SensitivitySlider = MakeRowSlider(Column, TEXT("SensitivitySlider"),
		LOCTEXT("SensitivityRow", "Чувствительность управления"), SensitivityValueText,
		EContrarySettingsControl::SensitivitySlider);
	OpacitySlider = MakeRowSlider(Column, TEXT("OpacitySlider"),
		LOCTEXT("OpacityRow", "Прозрачность экранных кнопок"), OpacityValueText,
		EContrarySettingsControl::OpacitySlider);
	VibrationButton = MakeRowButton(Column, TEXT("VibrationButton"), FText::GetEmpty(), VibrationText,
		EContrarySettingsControl::Vibration);

	// --- Прочее ---
	MiscHeaderText = MakeSectionHeader(Column, TEXT("MiscHeaderText"), LOCTEXT("MiscHeader", "Прочее"));
	ReportBugButton = MakeRowButton(Column, TEXT("ReportBugButton"), LOCTEXT("ReportBug", "Сообщить об ошибке"),
		ReportBugText, EContrarySettingsControl::ReportBug);

	ReportBugHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReportBugHintText"));
	ReportBugHintText->SetAutoWrapText(true);
	ReportBugHintText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, TouchLayout.LabelFontSize - 4)));
	ReportBugHintText->SetColorAndOpacity(FSlateColor(LabelColor));
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(ReportBugHintText))
	{
		HintSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TouchLayout.RowGap * 0.5f));
	}

	ResetProgressButton = MakeRowButton(Column, TEXT("ResetProgressButton"),
		LOCTEXT("ResetProgress", "Сбросить прогресс"), ResetProgressText,
		EContrarySettingsControl::ResetProgress);
	CloseButton = MakeRowButton(Column, TEXT("CloseButton"), LOCTEXT("Close", "Назад"), CloseText,
		EContrarySettingsControl::Close);

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
		// Прокрутка требует ограниченной высоты — размер окна берём из настроек «под палец».
		PanelSlot->SetSize(TouchLayout.PanelSize);
	}

	// --- Панель двойного переспроса поверх экрана (по умолчанию спрятана) ---
	ConfirmPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmPanel"));
	ConfirmPanel->SetBrushColor(PanelColor);
	ConfirmPanel->SetPadding(FMargin(24.0f, 20.0f));
	ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* ConfirmColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmColumn"));
	ConfirmPanel->SetContent(ConfirmColumn);

	ConfirmTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmTitleText"));
	ConfirmTitleText->SetAutoWrapText(true);
	ConfirmTitleText->SetJustification(ETextJustify::Center);
	ConfirmTitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, TouchLayout.HeaderFontSize)));
	ConfirmTitleText->SetColorAndOpacity(FSlateColor(TitleColor));
	if (UVerticalBoxSlot* ConfirmTitleSlot = ConfirmColumn->AddChildToVerticalBox(ConfirmTitleText))
	{
		ConfirmTitleSlot->SetHorizontalAlignment(HAlign_Center);
		ConfirmTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TouchLayout.RowGap * 0.5f));
	}
	ConfirmYesButton = MakeRowButton(ConfirmColumn, TEXT("ConfirmYesButton"), FText::GetEmpty(), ConfirmYesText,
		EContrarySettingsControl::ConfirmYes);
	ConfirmNoButton = MakeRowButton(ConfirmColumn, TEXT("ConfirmNoButton"), LOCTEXT("ResetNo", "Отмена"), ConfirmNoText,
		EContrarySettingsControl::ConfirmNo);

	if (UCanvasPanelSlot* ConfirmSlot = Root->AddChildToCanvas(ConfirmPanel))
	{
		ConfirmSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ConfirmSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ConfirmSlot->SetPosition(FVector2D::ZeroVector);
		ConfirmSlot->SetAutoSize(true);
	}
}

// --- Модальный барьер: события мимо кнопок не идут дальше в мир ---

FReply USettingsScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply USettingsScreenWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply USettingsScreenWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply USettingsScreenWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
