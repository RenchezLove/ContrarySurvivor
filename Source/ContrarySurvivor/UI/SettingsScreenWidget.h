// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h"
#include "Styling/SlateTypes.h" // FSliderStyle — вид ползунков общий для кода и генератора
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
 * Все управляющие элементы экрана настроек, в которые игрок тычет пальцем. Список нужен,
 * чтобы требование «в любой элемент можно попасть пальцем» проверялось МЕХАНИЧЕСКИ по всему
 * экрану, а не выборочно: автотест перебирает это перечисление целиком.
 * ⚠ Добавил элемент на экран — добавь его сюда, иначе он выпадет из проверки.
 */
UENUM()
enum class EContrarySettingsControl : uint8
{
	PresetLow,
	PresetMedium,
	PresetHigh,
	PresetAuto,
	ResolutionSlider,
	ResolutionMinus,
	ResolutionPlus,
	FrameLimit,
	FpsCounter,
	MusicSlider,
	EffectsSlider,
	SensitivitySlider,
	OpacitySlider,
	Vibration,
	ReportBug,
	ResetProgress,
	Close,
	ConfirmYes,
	ConfirmNo,
	MAX_None UMETA(Hidden) // хвост для перебора в тесте, элементом экрана не является
};

/**
 * Размеры экрана настроек «под палец» (жалоба Рината 08-09: «как само окно побольше сделать,
 * так и кнопки и расстояния между ними; сейчас тяжеловато по ним пальцами попадать»).
 *
 * ОТКУДА ЧИСЛА. Требование измеримое: сторона области нажатия не меньше 9 мм на настоящем
 * экране. Телефон Рината — 720 на 1600 точек при плотности 320 точек на дюйм, поэтому
 * 9 мм = 9/25.4*320 = 113.4 физической точки. Движок рисует интерфейс в СВОИХ точках и
 * умножает их на масштаб (правило «по короткой стороне», кривая UIScaleCurve из
 * BaseEngine.ini): при короткой стороне 720 масштаб 0.666, значит одна точка интерфейса —
 * это 0.666 физической. Отсюда порог 113.4/0.666 = 170.3 точки интерфейса; берём 172 с
 * небольшим запасом. Считает это MinTouchPointsFor — числа в тесте не зашиты, он спрашивает
 * масштаб у самого движка.
 *
 * Поля живут в Class Defaults окна: Ринат меняет размер мышкой, без правки кода.
 */
USTRUCT(BlueprintType)
struct FSettingsTouchLayout
{
	GENERATED_BODY()

	// Размер самого окна настроек, точек интерфейса. Было 520x620 — на телефоне это чуть
	// больше половины экрана, всё в нём мелкое.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Размер окна настроек", DisplayPriority = "1"))
	FVector2D PanelSize = FVector2D(1500.0f, 960.0f);

	// Минимальная сторона области нажатия любого элемента (те самые 9 мм).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Наименьшая сторона кнопки под палец", ClampMin = "1.0", DisplayPriority = "2"))
	float MinTouchSize = 172.0f;

	// Зазор между соседними строками. Не меньше половины высоты элемента: промах должен
	// уходить в пустоту, а не в соседний переключатель.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Зазор между строками", ClampMin = "0.0", DisplayPriority = "3"))
	float RowGap = 88.0f;

	// Ширина строки-кнопки и ползунка во всю строку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Ширина строки", ClampMin = "1.0", DisplayPriority = "4"))
	float RowWidth = 1360.0f;

	// Ширина кнопок переспроса («Да, сбросить» / «Отмена») — они лежат в своём окошке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Ширина кнопки переспроса", ClampMin = "1.0", DisplayPriority = "5"))
	float ConfirmButtonWidth = 620.0f;

	// --- Ползунки. Область нажатия у них та же, что у кнопок (высота строки), а вот САМА
	// полоска остаётся тонкой на вид — палец жмёт по всей высоте строки, глаз видит аккуратную
	// линию. Штатная полоска движка — 2 точки, на телефоне это волосок толщиной чуть больше
	// одного пикселя: видно её плохо, поэтому задаём свою. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Толщина полоски ползунка", ClampMin = "1.0", DisplayPriority = "6"))
	float SliderBarThickness = 14.0f;

	// Сторона бегунка (кружка, который таскают). Отдельно от области нажатия: тянуть можно
	// в любом месте строки, а бегунок просто должен быть хорошо виден.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Размеры",
		meta = (DisplayName = "Размер бегунка", ClampMin = "1.0", DisplayPriority = "7"))
	float SliderHandleSize = 56.0f;

	// --- Кегли шрифтов. Прежние (24/17/15/17) на телефоне давали строку около миллиметра
	// высотой: читать можно, попасть — нет. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Заголовок окна", ClampMin = "8", DisplayPriority = "1"))
	int32 TitleFontSize = 52;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Заголовок раздела", ClampMin = "8", DisplayPriority = "2"))
	int32 HeaderFontSize = 42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Подписи и значения", ClampMin = "8", DisplayPriority = "3"))
	int32 LabelFontSize = 34;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Шрифты",
		meta = (DisplayName = "Надписи на кнопках", ClampMin = "8", DisplayPriority = "4"))
	int32 ButtonFontSize = 36;
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

	// Размеры окна и элементов «под палец» (жалоба 08-09). Правятся в Class Defaults окна.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FSettingsTouchLayout TouchLayout;

	// --- Чистые правила (покрыты автотестами ContrarySurvivor.Settings) ---

	// Порог области нажатия в ТОЧКАХ ИНТЕРФЕЙСА для экрана заданного размера: сколько точек
	// занимает требуемый физический размер. Масштаб интерфейса берётся у самого движка
	// (UUserInterfaceSettings::GetDPIScaleBasedOnSize) — своей копии кривой мы не заводим.
	// Millimeters — требуемый размер в миллиметрах, ScreenDpi — плотность экрана устройства,
	// ViewportSize — размер вьюпорта в физических точках (для телефона Рината 1600x720).
	static float MinTouchPointsFor(float Millimeters, float ScreenDpi, FIntPoint ViewportSize);

	// Габарит области нажатия конкретного элемента экрана настроек. ОДНО МЕСТО ПРАВДЫ:
	// по нему строит кодовое дерево, по нему же генератор кладёт размеры в живой ассет,
	// и его же перебирает автотест по всему перечислению элементов.
	static FVector2D TouchBoxFor(const FSettingsTouchLayout& Layout, EContrarySettingsControl Control);

	// Вид ползунка: заметная полоска и крупный бегунок. Область нажатия здесь ни при чём —
	// её задаёт габаритная коробка вокруг ползунка (TouchBoxFor), а ползунок движка ловит
	// касание по ВСЕЙ своей площади (SSlider::OnMouseButtonDown переводит точку касания в
	// значение по всей отведённой геометрии). Одно место правды на оба пути, как и размеры.
	static FSliderStyle MakeSliderStyle(const FSettingsTouchLayout& Layout);

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
	// Control задаёт габарит области нажатия (TouchBoxFor) — размеры строк не разбросаны
	// по коду, а приходят из одного места вместе с живым ассетом.
	UButton* MakeRowButton(UVerticalBox* Column, const FName& BaseName, const FText& Caption,
		TObjectPtr<UTextBlock>& OutCaption, EContrarySettingsControl Control);
	USlider* MakeRowSlider(UVerticalBox* Column, const FName& BaseName, const FText& Label,
		TObjectPtr<UTextBlock>& OutValueText, EContrarySettingsControl Control);
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
