// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Engine/Engine.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"

#define LOCTEXT_NAMESPACE "ContrarySettings"

UContrarySurvivorGameUserSettings::UContrarySurvivorGameUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UContrarySurvivorGameUserSettings* UContrarySurvivorGameUserSettings::Get()
{
	// GetGameUserSettings создаёт объект классом GEngine->GameUserSettingsClass (его задаёт
	// строка GameUserSettingsClassName в Config/DefaultEngine.ini). Если строки нет, движок
	// создаст базовый UGameUserSettings — Cast честно вернёт null, и игра поедет на значениях
	// по умолчанию, а не упадёт.
	if (!GEngine)
	{
		return nullptr;
	}
	return Cast<UContrarySurvivorGameUserSettings>(GEngine->GetGameUserSettings());
}

// ---------------------------------------------------------------------------
// Чистые правила
// ---------------------------------------------------------------------------

int32 UContrarySurvivorGameUserSettings::SnapResolutionScalePercent(int32 Percent)
{
	// Сначала к ближайшему шагу, потом в границы: иначе округление могло бы вынести
	// значение за верхнюю границу (например 104 → 100 после клампа, а не 110).
	const int32 Snapped = FMath::RoundToInt(static_cast<float>(Percent) / ResolutionScaleStepPercent)
		* ResolutionScaleStepPercent;
	return FMath::Clamp(Snapped, MinResolutionScalePercent, MaxResolutionScalePercent);
}

EContraryGraphicsPreset UContrarySurvivorGameUserSettings::ChooseAutoPreset(int32 MemoryGB, int32 CpuCores)
{
	// Пороги стартовые (тюнинг — по замерам на живых устройствах). Логика простая: мало
	// памяти ЛИБО мало ядер — низкое качество; много и того и другого — высокое; всё
	// остальное — среднее. Нули/мусор от платформы (устройство не сообщило) трактуем как
	// слабое железо: лучше отдать игроку низкое качество и полный кадр, чем красивую слайдшоу.
	if (MemoryGB <= 3 || CpuCores <= 4)
	{
		return EContraryGraphicsPreset::Low;
	}
	if (MemoryGB >= 8 && CpuCores >= 8)
	{
		return EContraryGraphicsPreset::High;
	}
	return EContraryGraphicsPreset::Medium;
}

int32 UContrarySurvivorGameUserSettings::ScalabilityLevelFor(EContraryGraphicsPreset Preset)
{
	switch (Preset)
	{
	case EContraryGraphicsPreset::Low:  return 0;
	case EContraryGraphicsPreset::High: return 2;
	default:                    return 1; // Medium и Auto (живое значение считает GetEffectivePreset)
	}
}

float UContrarySurvivorGameUserSettings::FrameRateLimitFor(bool bLimitTo30)
{
	return bLimitTo30 ? 30.0f : 0.0f;
}

FText UContrarySurvivorGameUserSettings::MakeResolutionScaleLabel(int32 Percent, const FIntPoint& ScreenSize)
{
	const int32 SafePercent = SnapResolutionScalePercent(Percent);

	// Числа пикселей печатаем БЕЗ разбивки на разряды: иначе «1120» превращается в «1 120»
	// (FText::AsNumber по умолчанию группирует разряды по локали) и подпись читается как
	// два числа вместо одного.
	const FNumberFormattingOptions& Plain = FNumberFormattingOptions::DefaultNoGrouping();

	// Размера экрана ещё нет (headless, окно не создано) — показываем только проценты,
	// выдумывать пиксели нельзя.
	if (ScreenSize.X <= 0 || ScreenSize.Y <= 0)
	{
		FFormatNamedArguments OnlyPercent;
		OnlyPercent.Add(TEXT("Percent"), FText::AsNumber(SafePercent, &Plain));
		return FText::Format(LOCTEXT("ResolutionScaleShort", "{Percent}%"), OnlyPercent);
	}

	const int32 PixelsX = FMath::RoundToInt32(ScreenSize.X * SafePercent / 100.0f);
	const int32 PixelsY = FMath::RoundToInt32(ScreenSize.Y * SafePercent / 100.0f);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Percent"), FText::AsNumber(SafePercent, &Plain));
	Args.Add(TEXT("Width"), FText::AsNumber(PixelsX, &Plain));
	Args.Add(TEXT("Height"), FText::AsNumber(PixelsY, &Plain));
	return FText::Format(LOCTEXT("ResolutionScaleFull", "{Percent}% — это {Width} на {Height}"), Args);
}

FText UContrarySurvivorGameUserSettings::MakePresetName(EContraryGraphicsPreset Preset)
{
	switch (Preset)
	{
	case EContraryGraphicsPreset::Low:    return LOCTEXT("PresetLow", "Низкое");
	case EContraryGraphicsPreset::Medium: return LOCTEXT("PresetMedium", "Среднее");
	case EContraryGraphicsPreset::High:   return LOCTEXT("PresetHigh", "Высокое");
	default:                      return LOCTEXT("PresetAuto", "Авто");
	}
}

FText UContrarySurvivorGameUserSettings::MakePresetLabel(EContraryGraphicsPreset Chosen, EContraryGraphicsPreset Effective)
{
	if (Chosen != EContraryGraphicsPreset::Auto)
	{
		return MakePresetName(Chosen);
	}

	// Спека: при «Авто» игра «показывает игроку, какой выбран» — иначе выбор молчаливый.
	// Имя качества внутри скобок — со строчной буквы: это часть фразы, а не заголовок кнопки.
	FFormatNamedArguments Args;
	Args.Add(TEXT("Effective"), MakePresetName(Effective).ToLower());
	return FText::Format(LOCTEXT("PresetAutoWithValue", "Авто ({Effective})"), Args);
}

FText UContrarySurvivorGameUserSettings::MakePercentLabel(float Value01)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Percent"), FText::AsNumber(
		FMath::RoundToInt32(FMath::Clamp(Value01, 0.0f, 10.0f) * 100.0f),
		&FNumberFormattingOptions::DefaultNoGrouping()));
	return FText::Format(LOCTEXT("PercentLabel", "{Percent}%"), Args);
}

// ---------------------------------------------------------------------------
// Живые значения
// ---------------------------------------------------------------------------

EContraryGraphicsPreset UContrarySurvivorGameUserSettings::DetectDevicePreset()
{
	const int32 MemoryGB = static_cast<int32>(FPlatformMemory::GetPhysicalGBRam());
	const int32 CpuCores = FPlatformMisc::NumberOfCores();
	return ChooseAutoPreset(MemoryGB, CpuCores);
}

EContraryGraphicsPreset UContrarySurvivorGameUserSettings::GetEffectivePreset() const
{
	return QualityPreset == EContraryGraphicsPreset::Auto ? DetectDevicePreset() : QualityPreset;
}

void UContrarySurvivorGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	QualityPreset = EContraryGraphicsPreset::Auto;
	ResolutionScalePercent = DefaultResolutionScalePercent;
	bLimitFrameRateTo30 = true;
	bShowFpsCounter = false;
	MusicVolume = DefaultVolume;
	EffectsVolume = DefaultVolume;
	ControlSensitivity = DefaultControlSensitivity;
	TouchButtonsOpacity = DefaultTouchButtonsOpacity;
	bVibrationEnabled = true;
}

void UContrarySurvivorGameUserSettings::ApplyContrarySettings()
{
	// 1. Пресет качества. SetOverallScalabilityLevel ставит ВСЕ уровни разом, включая
	//    ResolutionQuality — поэтому масштаб игрока идёт следующим шагом и перекрывает его.
	SetOverallScalabilityLevel(ScalabilityLevelFor(GetEffectivePreset()));

	// 2. Масштаб разрешения — независимый рычаг игрока (спека).
	SetResolutionScaleValueEx(static_cast<float>(SnapResolutionScalePercent(ResolutionScalePercent)));

	// 3. Предел кадров.
	SetFrameRateLimit(FrameRateLimitFor(bLimitFrameRateTo30));

	// ApplySettings применяет разрешение и масштабируемость и САМ пишет файл настроек
	// (GameUserSettings.cpp: ApplyResolutionSettings + ApplyNonResolutionSettings + SaveSettings) —
	// отдельный SaveSettings не нужен, это и есть «сохраняется между запусками».
	ApplySettings(/*bCheckForCommandLineOverrides=*/false);

	UE_LOG(LogQA, Display,
		TEXT("QA: settings applied (preset %s, scale %d%%, fps limit %s, fps counter %s)"),
		*MakePresetLabel(QualityPreset, GetEffectivePreset()).ToString(),
		SnapResolutionScalePercent(ResolutionScalePercent),
		bLimitFrameRateTo30 ? TEXT("30") : TEXT("off"),
		bShowFpsCounter ? TEXT("on") : TEXT("off"));
}

// ---------------------------------------------------------------------------
// Изменение настроек (каждая правка применяется сразу и сохраняется)
// ---------------------------------------------------------------------------

void UContrarySurvivorGameUserSettings::SetQualityPreset(EContraryGraphicsPreset NewPreset)
{
	QualityPreset = NewPreset;
	ApplyContrarySettings();
}

void UContrarySurvivorGameUserSettings::SetResolutionScalePercent(int32 NewPercent)
{
	ResolutionScalePercent = SnapResolutionScalePercent(NewPercent);
	ApplyContrarySettings();
}

void UContrarySurvivorGameUserSettings::SetFrameRateLimitedTo30(bool bLimit)
{
	bLimitFrameRateTo30 = bLimit;
	ApplyContrarySettings();
}

void UContrarySurvivorGameUserSettings::SetFpsCounterShown(bool bShow)
{
	bShowFpsCounter = bShow;
	// Счётчик рисует тач-слой (UTouchControlsWidget::UpdateFpsText читает это значение каждый
	// кадр) — применять нечего, только сохранить.
	SaveSettings();
}

void UContrarySurvivorGameUserSettings::SetMusicVolume(float NewVolume)
{
	MusicVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	// Живой фоновый звук перенастраивает владелец экрана (у объекта настроек нет мира).
	SaveSettings();
}

void UContrarySurvivorGameUserSettings::SetEffectsVolume(float NewVolume)
{
	EffectsVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	SaveSettings();
}

void UContrarySurvivorGameUserSettings::SetControlSensitivity(float NewSensitivity)
{
	ControlSensitivity = FMath::Clamp(NewSensitivity, MinControlSensitivity, MaxControlSensitivity);
	SaveSettings();
}

void UContrarySurvivorGameUserSettings::SetTouchButtonsOpacity(float NewOpacity)
{
	TouchButtonsOpacity = FMath::Clamp(NewOpacity, MinTouchButtonsOpacity, MaxTouchButtonsOpacity);
	SaveSettings();
}

void UContrarySurvivorGameUserSettings::SetVibrationEnabled(bool bEnabled)
{
	bVibrationEnabled = bEnabled;
	SaveSettings();
}

// ---------------------------------------------------------------------------
// Безопасные читалки для потребителей в игре
// ---------------------------------------------------------------------------

float UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe()
{
	const UContrarySurvivorGameUserSettings* Settings = Get();
	return Settings ? Settings->GetEffectsVolume() : DefaultVolume;
}

float UContrarySurvivorGameUserSettings::GetMusicVolumeSafe()
{
	const UContrarySurvivorGameUserSettings* Settings = Get();
	return Settings ? Settings->GetMusicVolume() : DefaultVolume;
}

float UContrarySurvivorGameUserSettings::GetControlSensitivitySafe()
{
	const UContrarySurvivorGameUserSettings* Settings = Get();
	return Settings ? Settings->GetControlSensitivity() : DefaultControlSensitivity;
}

float UContrarySurvivorGameUserSettings::GetTouchButtonsOpacitySafe()
{
	const UContrarySurvivorGameUserSettings* Settings = Get();
	return Settings ? Settings->GetTouchButtonsOpacity() : DefaultTouchButtonsOpacity;
}

bool UContrarySurvivorGameUserSettings::IsFpsCounterShownSafe()
{
	const UContrarySurvivorGameUserSettings* Settings = Get();
	// Настроек нет — счётчика нет (спека: по умолчанию ВЫКЛЮЧЕН).
	return Settings ? Settings->IsFpsCounterShown() : false;
}

bool UContrarySurvivorGameUserSettings::IsVibrationEnabledSafe()
{
	const UContrarySurvivorGameUserSettings* Settings = Get();
	return Settings ? Settings->IsVibrationEnabled() : true;
}

#undef LOCTEXT_NAMESPACE
