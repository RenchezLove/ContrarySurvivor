// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ContrarySurvivorGameUserSettings.generated.h"

/**
 * Пресет качества картинки (спека glavnoe-menu-spec.md, раздел «Экран настроек»):
 * «Низкое», «Среднее», «Высокое», «Авто». «Авто» — не отдельное качество, а признак
 * «выбери сам по устройству»: живое значение считает GetEffectivePreset().
 */
UENUM(BlueprintType)
enum class EContraryGraphicsPreset : uint8
{
	Low    UMETA(DisplayName = "Низкое"),
	Medium UMETA(DisplayName = "Среднее"),
	High   UMETA(DisplayName = "Высокое"),
	Auto   UMETA(DisplayName = "Авто"),
};

/**
 * Настройки игрока (волна «Главное меню» 08-08, подход 2; ADR-062).
 *
 * ПОЧЕМУ НАСЛЕДНИК UGameUserSettings, А НЕ СВОЯ ПОДСИСТЕМА (решение и его цена):
 *  - сохранение между запусками достаётся даром: UPROPERTY(Config) пишутся в
 *    GameUserSettings.ini движком (SaveSettings/LoadSettings), свой файл сейва не нужен;
 *  - графические рычаги (пресет качества, масштаб разрешения, предел кадров) уже реализованы
 *    в базовом классе и применяются СРАЗУ, без перезапуска (ApplySettings → Scalability);
 *  - класс подставляется движку строкой конфига (Config/DefaultEngine.ini, раздел
 *    [/Script/Engine.Engine], ключ GameUserSettingsClassName), после чего штатный
 *    UGameUserSettings::GetGameUserSettings() отдаёт уже НАШ объект — отдельного синглтона нет.
 * Цена решения: объект живёт в движке, а не в игровом мире, поэтому у него нет мира и
 * контекста игрока — всё, что требует мира (громкость живого звука, прозрачность тач-слоя),
 * применяет владелец экрана настроек (контроллер), а здесь только хранится и отдаётся.
 *
 * ⚠ Порядок применения графики важен: пресет качества (SetOverallScalabilityLevel) перетирает
 * масштаб разрешения внутри ScalabilityQuality (Scalability::FQualityLevels::SetFromSingleQualityLevel
 * ставит и ResolutionQuality), поэтому масштаб игрока выставляется ПОСЛЕ пресета. Иначе ползунок
 * игрока дрался бы с пресетом — ровно то, что спека запрещает («ползунок НЕЗАВИСИМЫЙ от пресета»).
 */
UCLASS(Config = GameUserSettings, configdonotcheckdefaults)
class CONTRARYSURVIVOR_API UContrarySurvivorGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UContrarySurvivorGameUserSettings(const FObjectInitializer& ObjectInitializer);

	// Наш объект настроек (движок создаёт его по GameUserSettingsClassName). Возвращает null,
	// если строки в конфиге нет или движка ещё нет (headless-утилиты) — вызывающий обязан
	// пережить null: игра без настроек играется на значениях по умолчанию.
	static UContrarySurvivorGameUserSettings* Get();

	// ------------------------------------------------------------------
	// Чистые правила (без движка и без объекта — гоняются автотестами headless)
	// ------------------------------------------------------------------

	// Масштаб разрешения: спека — «от 50 до 100 процентов с шагом 10». Любое пришедшее число
	// прижимается к диапазону и округляется к ближайшему шагу (73 → 70, 76 → 80).
	static int32 SnapResolutionScalePercent(int32 Percent);

	// Автовыбор пресета по устройству. Считаем по двум величинам, которые есть на любой
	// платформе: объём оперативной памяти (ГБ) и число ядер процессора. Пороги — стартовые
	// (целимся в слабые телефоны, ADR-058: планка 24 кадра), тюнинг по замерам на устройствах.
	static EContraryGraphicsPreset ChooseAutoPreset(int32 MemoryGB, int32 CpuCores);

	// Уровень масштабируемости движка для пресета (0 — низкое, 1 — среднее, 2 — высокое).
	// Для Auto отдаёт средний уровень: живое значение считает GetEffectivePreset().
	static int32 ScalabilityLevelFor(EContraryGraphicsPreset Preset);

	// Предел кадров: спека — «30» либо «без ограничения» (0 = предела нет).
	static float FrameRateLimitFor(bool bLimitTo30);

	// Подпись под ползунком масштаба: «70% — это 504 на 1120» (спека, дословный пример).
	static FText MakeResolutionScaleLabel(int32 Percent, const FIntPoint& ScreenSize);

	// Подпись выбранного пресета. Для «Авто» показывает, что именно выбрано за игрока
	// («Авто (среднее)») — спека: «показывает игроку, какой выбран».
	static FText MakePresetLabel(EContraryGraphicsPreset Chosen, EContraryGraphicsPreset Effective);

	// Человеческое имя пресета для кнопки («Низкое»/«Среднее»/«Высокое»/«Авто»).
	static FText MakePresetName(EContraryGraphicsPreset Preset);

	// Доля 0..1 в подпись «70%» (громкость, чувствительность, прозрачность).
	static FText MakePercentLabel(float Value01);

	// ------------------------------------------------------------------
	// Значения настроек (читают экран настроек и потребители в игре)
	// ------------------------------------------------------------------

	EContraryGraphicsPreset GetQualityPreset() const { return QualityPreset; }

	// Живой пресет: для «Авто» — выбранный по этому устройству, иначе выбор игрока.
	EContraryGraphicsPreset GetEffectivePreset() const;

	int32 GetResolutionScalePercent() const { return ResolutionScalePercent; }
	bool IsFrameRateLimitedTo30() const { return bLimitFrameRateTo30; }
	bool IsFpsCounterShown() const { return bShowFpsCounter; }
	float GetMusicVolume() const { return MusicVolume; }
	float GetEffectsVolume() const { return EffectsVolume; }
	float GetControlSensitivity() const { return ControlSensitivity; }
	float GetTouchButtonsOpacity() const { return TouchButtonsOpacity; }
	bool IsVibrationEnabled() const { return bVibrationEnabled; }

	// ------------------------------------------------------------------
	// Изменение настроек. Каждый метод применяет значение СРАЗУ и сохраняет его
	// (спека: «применяется СРАЗУ, без перезапуска игры, и сохраняется между запусками»).
	// ------------------------------------------------------------------

	void SetQualityPreset(EContraryGraphicsPreset NewPreset);
	void SetResolutionScalePercent(int32 NewPercent);
	void SetFrameRateLimitedTo30(bool bLimit);
	void SetFpsCounterShown(bool bShow);
	void SetMusicVolume(float NewVolume);
	void SetEffectsVolume(float NewVolume);
	void SetControlSensitivity(float NewSensitivity);
	void SetTouchButtonsOpacity(float NewOpacity);
	void SetVibrationEnabled(bool bEnabled);

	// Применить ВСЁ разом: графику (пресет → масштаб → предел кадров) с записью на диск.
	// Зовётся при запуске игры и после любой правки на экране настроек.
	void ApplyContrarySettings();

	// ------------------------------------------------------------------
	// Безопасные статические читалки для потребителей в игре: объекта настроек может не быть
	// (headless, тест, строка конфига не прописана) — тогда отдаём значение по умолчанию.
	// ------------------------------------------------------------------

	static float GetEffectsVolumeSafe();
	static float GetMusicVolumeSafe();
	static float GetControlSensitivitySafe();
	static float GetTouchButtonsOpacitySafe();
	static bool IsFpsCounterShownSafe();
	static bool IsVibrationEnabledSafe();

	// --- Границы ползунков (одно место правды для экрана настроек и автотестов) ---

	static constexpr int32 MinResolutionScalePercent = 50;
	static constexpr int32 MaxResolutionScalePercent = 100;
	static constexpr int32 ResolutionScaleStepPercent = 10;

	static constexpr float MinControlSensitivity = 0.5f;
	static constexpr float MaxControlSensitivity = 2.0f;

	static constexpr float MinTouchButtonsOpacity = 0.2f;
	static constexpr float MaxTouchButtonsOpacity = 1.0f;

	// Значения по умолчанию (ADR-062: масштаб 100%, предел кадров 30; счётчик кадров выключен).
	static constexpr int32 DefaultResolutionScalePercent = 100;
	static constexpr float DefaultControlSensitivity = 1.0f;
	static constexpr float DefaultTouchButtonsOpacity = 0.5f; // прежняя TouchIdleOpacity контроллера
	static constexpr float DefaultVolume = 1.0f;

	// Сброс к заводским значениям (движок зовёт при порче/устаревании файла настроек).
	virtual void SetToDefaults() override;

protected:
	// --- Графика ---

	UPROPERTY(Config)
	EContraryGraphicsPreset QualityPreset = EContraryGraphicsPreset::Auto;

	UPROPERTY(Config)
	int32 ResolutionScalePercent = DefaultResolutionScalePercent;

	UPROPERTY(Config)
	bool bLimitFrameRateTo30 = true;

	UPROPERTY(Config)
	bool bShowFpsCounter = false;

	// --- Звук ---

	UPROPERTY(Config)
	float MusicVolume = DefaultVolume;

	UPROPERTY(Config)
	float EffectsVolume = DefaultVolume;

	// --- Управление ---

	UPROPERTY(Config)
	float ControlSensitivity = DefaultControlSensitivity;

	UPROPERTY(Config)
	float TouchButtonsOpacity = DefaultTouchButtonsOpacity;

	UPROPERTY(Config)
	bool bVibrationEnabled = true;

private:
	// Пресет, выбранный по железу этого устройства (память + ядра).
	static EContraryGraphicsPreset DetectDevicePreset();
};
