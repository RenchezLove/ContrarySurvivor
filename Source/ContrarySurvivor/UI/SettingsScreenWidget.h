// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h"
#include "SettingsScreenWidget.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class USlider;
class UScrollBox;
class UVerticalBox;
class USizeBox;
class UWidget;

/**
 * Стадия переспроса при сбросе прогресса (спека: «с ДВОЙНЫМ переспросом, чтобы случайно не
 * стереть сохранение»). Стирающий сигнал уходит владельцу ТОЛЬКО из стадии Second.
 */
UENUM(BlueprintType)
enum class EResetConfirmStage : uint8
{
	None,   // переспроса нет, показан обычный экран настроек
	First,  // «Сбросить весь прогресс?»
	Second, // «Точно стереть? Вернуть прогресс будет нельзя»
};

/**
 * Размеры и шрифты экрана настроек. Значения по умолчанию — прежние, те самые, что были до
 * волны «под палец» (Ринат 08-09: «Верни как было… ТОЛЬКО СДЕЛАЙ ИХ НАСТРАИВАЕМЫМИ»).
 * Порог в девять миллиметров и всё, что его держало, снято: размер Ринат выбирает сам.
 *
 * ГДЕ ЭТО ДЕЙСТВУЕТ. Живой экран правится мышкой в WBP_Settings, и код туда не лезет
 * (ADR-048) — там эти поля не при чём. Поля ведут кодовое дерево-запаску (когда ассета нет)
 * и служат значениями по умолчанию для генератора, когда он создаёт окно с нуля.
 */
USTRUCT(BlueprintType)
struct FSettingsScreenLayout
{
	GENERATED_BODY()

	// Размер окна настроек, точек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Размер окна", DisplayPriority = "1"))
	FVector2D PanelSize = FVector2D(520.0f, 620.0f);

	// Габарит строки-кнопки (переключатели «Ограничение кадров», «Вибрация» и прочие).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Размер кнопки-строки", DisplayPriority = "2"))
	FVector2D RowButtonSize = FVector2D(320.0f, 52.0f);

	// Габарит квадратных кнопок шага «−» и «+» у масштаба разрешения.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Размер кнопки шага", DisplayPriority = "3"))
	FVector2D SmallButtonSize = FVector2D(56.0f, 52.0f);

	// Зазор между строками списка, точек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Зазор между строками", ClampMin = "0.0", DisplayPriority = "4"))
	float RowGap = 8.0f;

	// Отступ над заголовком раздела («Звук», «Управление», «Прочее»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Отступ над заголовком раздела", ClampMin = "0.0", DisplayPriority = "5"))
	float SectionGap = 14.0f;

	// --- Кегли шрифтов ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Заголовок окна", ClampMin = "8", DisplayPriority = "1"))
	int32 TitleFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Заголовок раздела", ClampMin = "8", DisplayPriority = "2"))
	int32 HeaderFontSize = 17;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Подписи и значения", ClampMin = "8", DisplayPriority = "3"))
	int32 LabelFontSize = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Надписи на кнопках", ClampMin = "8", DisplayPriority = "4"))
	int32 ButtonFontSize = 17;
};

/**
 * Экран настроек (волна «Главное меню» 08-08, подход 2; спека glavnoe-menu-spec.md, раздел
 * «Экран настроек»). Открывается пунктом «Настройки» главного меню.
 *
 * Состав по спеке: пресет качества (Низкое/Среднее/Высокое/Авто), НЕЗАВИСИМЫЙ ползунок
 * масштаба разрешения 50–100% с шагом 10 и подписью в пикселях, ограничение кадров (30 либо
 * без ограничения), счётчик кадров (по умолчанию выключен), громкость музыки и эффектов,
 * чувствительность управления, прозрачность экранных кнопок, вибрация, «Сообщить об ошибке»
 * (адрес из конфига — пусто, пункт спрятан) и «Сбросить прогресс» с двойным переспросом.
 *
 * Два пути дерева — как у остальных окон (ADR-048):
 *  - назначен слот SettingsScreenWidgetClass на контроллере → дерево из WBP_Settings, кубики
 *    приходят по BindWidgetOptional-именам, вид правится мышкой в дизайнере;
 *  - слот пуст → кодовое дерево (BuildCodeTree), тот же состав и те же имена кубиков.
 *
 * Значения виджет читает и пишет напрямую в UContrarySurvivorGameUserSettings (там же они
 * применяются и сохраняются). Наружу виджет сообщает только о трёх вещах: закрыться,
 * подтверждённый сброс прогресса и «настройки изменились» — последнее нужно владельцу, чтобы
 * освежить то, что живёт в мире (прозрачность тач-слоя, громкость играющего фона).
 */
UCLASS()
class CONTRARYSURVIVOR_API USettingsScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// «Назад» — владелец закрывает экран.
	FSimpleMulticastDelegate OnCloseRequested;

	// Двойной переспрос пройден до конца — владелец стирает сохранение.
	FSimpleMulticastDelegate OnResetProgressConfirmed;

	// Любая правка настройки: владелец освежает живые потребители (тач-слой, звук).
	FSimpleMulticastDelegate OnSettingsChanged;

	// Идёт ли сейчас переспрос сброса и на какой стадии (для владельца и автотестов).
	EResetConfirmStage GetResetConfirmStage() const { return ResetConfirmStage; }

	// Размеры и шрифты окна (правятся в Class Defaults; живой экран — мышкой в дизайнере).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FSettingsScreenLayout Layout;

	// --- Чистые правила (покрыты автотестами ContrarySurvivor.Settings) ---

	// «Сообщить об ошибке»: пустой/пробельный адрес в конфиге — пункт спрятан целиком
	// (то же правило, что у «Сообщества» главного меню — ADR-062).
	static ESlateVisibility ReportBugVisibilityFor(const FString& BugReportUrl);

	// Значение ползунка 0..1 → проценты масштаба разрешения (50..100 с шагом 10).
	static int32 SliderValueToResolutionPercent(float SliderValue);

	// Проценты масштаба → значение ползунка 0..1.
	static float ResolutionPercentToSliderValue(int32 Percent);

	// --- Обработчики. ПУБЛИЧНЫЕ намеренно: их зовут и клики кнопок, и headless-тесты
	// (живой Slate в Automation-тестах проекта не поднимается — паттерн StartScreenWidget). ---

	UFUNCTION() void HandlePresetLowClicked();
	UFUNCTION() void HandlePresetMediumClicked();
	UFUNCTION() void HandlePresetHighClicked();
	UFUNCTION() void HandlePresetAutoClicked();

	UFUNCTION() void HandleResolutionMinusClicked();
	UFUNCTION() void HandleResolutionPlusClicked();
	UFUNCTION() void HandleResolutionSliderChanged(float NewValue);

	UFUNCTION() void HandleFrameLimitClicked();
	UFUNCTION() void HandleFpsCounterClicked();

	UFUNCTION() void HandleMusicSliderChanged(float NewValue);
	UFUNCTION() void HandleEffectsSliderChanged(float NewValue);
	UFUNCTION() void HandleSensitivitySliderChanged(float NewValue);
	UFUNCTION() void HandleOpacitySliderChanged(float NewValue);
	UFUNCTION() void HandleVibrationClicked();

	UFUNCTION() void HandleReportBugClicked();
	UFUNCTION() void HandleResetProgressClicked();
	UFUNCTION() void HandleConfirmYesClicked();
	UFUNCTION() void HandleConfirmNoClicked();
	UFUNCTION() void HandleCloseClicked();

	// Перечитать все значения из настроек и переписать подписи (зовёт владелец при открытии).
	void RefreshFromSettings();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// Модальный барьер: клик/тап мимо кнопок в мир не проходит (как у главного меню).
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

private:
	// Кодовое дерево-фолбэк (имена кубиков совпадают с генерируемым WBP_Settings).
	void BuildCodeTree();

	// Строка «подпись + кнопка» и строка «подпись + ползунок + значение» кодового дерева.
	// Выходной параметр — TObjectPtr: сюда пишутся поля-кубики самого виджета.
	UButton* MakeRowButton(UVerticalBox* Column, const FName& BaseName, const FText& Caption,
		TObjectPtr<UTextBlock>& OutCaption);
	USlider* MakeRowSlider(UVerticalBox* Column, const FName& BaseName, const FText& Label,
		TObjectPtr<UTextBlock>& OutValueText);
	UTextBlock* MakeSectionHeader(UVerticalBox* Column, const FName& Name, const FText& Caption);

	// Подписи «живут» значениями: пресет, масштаб (с пикселями), предел кадров, счётчик,
	// громкости, чувствительность, прозрачность, вибрация.
	void RefreshLabels();

	// Показ/скрытие панели переспроса и её текст под текущую стадию.
	void RefreshConfirmPanel();

	// Размер игрового экрана в пикселях для подписи масштаба (0×0, если вьюпорта ещё нет).
	FIntPoint GetViewportPixelSize() const;

	// Скрыть/показать строку целиком (в кодовом дереве кнопка обёрнута в SizeBox — прятать
	// надо обёртку, иначе останется пустое место; урок AmmoRow).
	static void SetRowVisibility(UWidget* Widget, ESlateVisibility InVisibility);

	// Значение настроек изменилось: сообщить владельцу и переписать подписи.
	void NotifyChanged();

	EResetConfirmStage ResetConfirmStage = EResetConfirmStage::None;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized).
	bool bDesignerTree = false;

	// Дерево построили МЫ (кодовая запаска). ⛔ Запись подписей решается по этому признаку, а не
	// по детекту дизайнера: тот живёт в NativeOnInitialized, которую движок зовёт не всегда
	// (UI/OwnerTextGuard.h).
	bool bCodeTreeBuilt = false;

	// Шаблоны смешанных строк («Качество картинки: {Value}»), как их набрал владелец в WBP.
	// Снимаются ОДИН раз до первой записи; есть {Value} — код подставляет значение в текст
	// владельца, нет — берёт свою формулировку.
	void CaptureOwnerRowFormats();
	bool bOwnerRowFormatsSaved = false;
	FText OwnerPresetRowFormat;
	FText OwnerFrameLimitRowFormat;
	FText OwnerFpsCounterRowFormat;
	FText OwnerVibrationRowFormat;

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> DimBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	// Графика
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GraphicsHeaderText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PresetValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PresetLowButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PresetLowText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PresetMediumButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PresetMediumText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PresetHighButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PresetHighText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PresetAutoButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PresetAutoText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResolutionLabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> ResolutionSlider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResolutionValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ResolutionMinusButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResolutionMinusText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ResolutionPlusButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResolutionPlusText;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> FrameLimitButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FrameLimitText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> FpsCounterButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FpsCounterText;

	// Звук
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SoundHeaderText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MusicLabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> MusicSlider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MusicValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EffectsLabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> EffectsSlider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EffectsValueText;

	// Управление
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ControlsHeaderText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SensitivityLabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> SensitivitySlider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SensitivityValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OpacityLabelText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> OpacitySlider;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OpacityValueText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> VibrationButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> VibrationText;

	// Прочее
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MiscHeaderText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ReportBugButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ReportBugText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ReportBugHintText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ResetProgressButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResetProgressText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CloseText;

	// Панель двойного переспроса поверх экрана.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> ConfirmPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ConfirmTitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ConfirmYesButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ConfirmYesText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ConfirmNoButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ConfirmNoText;

	// Рамки и обёртки кодового дерева (в WBP их нет — там одна плашка PanelPlate).
	UPROPERTY() TObjectPtr<UBorder> FrameBorder;
	UPROPERTY() TObjectPtr<UBorder> PanelBorder;
	UPROPERTY() TArray<TObjectPtr<USizeBox>> ButtonBoxes;
};
