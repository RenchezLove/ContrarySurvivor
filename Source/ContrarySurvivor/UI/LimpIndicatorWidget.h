// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "LimpIndicatorWidget.generated.h"

class UTextBlock;
class UBorder;
class USizeBox;

/**
 * Стиль плашки индикатора хромоты. Живёт EditAnywhere-полем на APlayerCharacter (виджет
 * строится из C++-класса и в Details не виден — настройка на владельце, паттерн
 * FOnboardingHintStyle/FDailyRewardStyle). Палитра — как у тостов онбординга (тёмная
 * подложка), текст — приглушённый красный в тон HP-бару.
 */
USTRUCT(BlueprintType)
struct FLimpIndicatorStyle
{
	GENERATED_BODY()

	// Цвет плашки-подложки (тёмная полупрозрачная).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator")
	FLinearColor PlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	// Цвет текста («ранен» — приглушённый красный, в тон полоске здоровья).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator")
	FLinearColor TextColor = FLinearColor(1.0f, 0.45f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator", meta = (ClampMin = "8"))
	int32 FontSize = 15;

	// Внутренние отступы плашки: X — по горизонтали, Y — по вертикали.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator")
	FVector2D PlatePadding = FVector2D(12.0f, 8.0f);

	// Якорь на экране в долях (0/0 = левый верх) и смещение от якоря в пикселях.
	// Дефолт — сразу ПОД стеком статов игрока (HP/голод/жажда/патроны/деньги; Canvas-метрики:
	// отступ 24, стек ~180 px высотой). Позиция — knob Рината.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator")
	FVector2D ScreenAnchor = FVector2D(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator")
	FVector2D ScreenOffset = FVector2D(24.0f, 210.0f);

	// Ширина плашки (px), в тон ширине баров статов (320 + отступы). Высота НЕ задаётся:
	// растёт за текстом — развёрнутая разовая подсказка переносится на 2-3 строки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator", meta = (ClampMin = "50"))
	float BoxWidth = 344.0f;
};

/**
 * Состояние разовой расшифровки хромоты — «раз за игровую сессию». Чистая логика без UObject,
 * вынесена из APlayerCharacter под headless-тест (паттерн DailyRewardLogic: показ виджета
 * тестируется только в PIE, а решение «показывать ли» — тестируемо без мира).
 */
struct FLimpFirstHintState
{
	// Показ уже запланирован/состоялся в этой сессии.
	bool bConsumed = false;

	// true РОВНО один раз: игрок хромает И управление свободно (не интро, не модальный экран).
	bool ShouldTrigger(bool bLimping, bool bControlFree)
	{
		if (bConsumed || !bLimping || !bControlFree)
		{
			return false;
		}
		bConsumed = true;
		return true;
	}

	// Хромота прошла раньше показа — подсказка не потрачена, вернётся при следующем ранении.
	void Rearm() { bConsumed = false; }
};

/**
 * Экранный индикатор хромоты (Build 1, приёмка Рината 07-27): компактная плашка «Ранен:
 * скорость снижена» под стеком статов, видна, пока игрок хромает; при первом входе в хромоту
 * на ней же разово показывается развёрнутое объяснение (текстами владеет APlayerCharacter).
 *
 * ТЗ Рината 08-07 — два пути, как у UEndOfStoryWidget (архитектура ADR-048):
 *  - создан из WBP_LimpIndicator (слот LimpIndicatorWidgetClass на HUD) → дерево владельца
 *    из дизайнера (плашка/шрифт/позиция правятся мышкой), стиль код не перекрашивает;
 *    текст плашки ставит код (SetIndicatorText) — он зависит от состояния;
 *  - ассета нет / слот пуст → прежний кодовый вид (BuildCodeTree + FLimpIndicatorStyle).
 */
UCLASS()
class CONTRARYSURVIVOR_API ULimpIndicatorWidget : public USelfHidingWidget
{
	GENERATED_BODY()

public:
	// Текст плашки (компактный индикатор либо развёрнутая разовая подсказка) — готовый
	// переводимый FText (ADR-050). Публичный для случаев, когда вызывающему код нужен
	// произвольный текст; для двух штатных состояний хромоты — см. ApplyStateText ниже.
	void SetIndicatorText(const FText& Text);

	// Ставит один из двух штатных текстов индикатора (WoundedText/FirstHintText) — сам решает,
	// какой: true = развёрнутая разовая подсказка, false = постоянная строка «ранен». Владелец
	// (APlayerCharacter) ведёт только МОМЕНТ переключения (по правилам игры), сами тексты ему
	// знать не нужно — они целиком на виджете.
	void ApplyStateText(bool bExpandedHint);

	// Тайминги разовой подсказки для владельца (таймеры показа/скрытия ставит APlayerCharacter,
	// значения — отсюда, поле ниже он больше не хранит).
	float GetFirstHintDelay() const { return FirstHintDelay; }
	float GetFirstHintDuration() const { return FirstHintDuration; }

	// --- Тексты, тайминги и стиль индикатора хромоты (директива владельца 08-29: «перенеси
	// внутрь окна, чтобы крутить мышью там же» — ADR-077 п.0, внешний вид ЛЮБОГО окна
	// настраивается в его собственном WBP). Раньше жили на APlayerCharacter — теперь здесь,
	// видны в Class Defaults ассета WBP_LimpIndicator. Правила игры (порог HP/скорость/звук
	// хромоты) остались на персонаже — это не вид окна, а механика. ---

	// Постоянная строка, пока игрок ранен: плашка под стеком статов, видна всё время хромоты.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator", meta = (DisplayPriority = "1",
		DisplayName = "Постоянная строка, пока игрок ранен"))
	FText WoundedText = NSLOCTEXT("LimpIndicator", "IndicatorText", "Ранен: скорость снижена");

	// РАЗОВАЯ развёрнутая подсказка (раз за игровую сессию, при первом входе в хромоту со
	// свободным управлением): объясняет причину и что скорость ВЕРНЁТСЯ после лечения.
	// Названные способы лечения сверены с кодом: аптечка лечит напрямую (AConsumableItem,
	// тип Medkit), еда и вода лечат понемногу (UStatsComponent::Food/WaterHealthRestoreAmount).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator", meta = (DisplayPriority = "2",
		DisplayName = "Разовая развёрнутая подсказка при первом ранении", MultiLine = "true"))
	FText FirstHintText = NSLOCTEXT("LimpIndicator", "FirstHintText",
		"Тебя сильно потрепали: пока здоровья мало, герой хромает и идёт медленно. Подлечись — аптечкой, едой или водой — и скорость вернётся.");

	// Задержка развёрнутой подсказки после того, как управление стало свободным (сек), чтобы не
	// спорить за внимание с подсказкой движения — та всплывает ровно в момент передачи управления.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator", meta = (ClampMin = "0.0", DisplayPriority = "3",
		DisplayName = "Задержка развёрнутой подсказки, сек"))
	float FirstHintDelay = 2.5f;

	// Сколько секунд висит развёрнутая подсказка; затем плашка сжимается до постоянного индикатора.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator", meta = (ClampMin = "1.0", DisplayPriority = "4",
		DisplayName = "Сколько секунд висит развёрнутая подсказка"))
	float FirstHintDuration = 8.0f;

	// Стиль плашки: цвета, шрифт, позиция/отступы на экране, ширина. Действует ТОЛЬКО когда
	// ассета нет и дерево строится кодом (BuildCodeTree, см. ApplyStyle ниже); при дереве из
	// WBP_LimpIndicator вид целиком в дизайнере, код его не трогает (ADR-077 п.0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LimpIndicator", meta = (DisplayPriority = "5"))
	FLimpIndicatorStyle Style;

protected:
	virtual void NativeOnInitialized() override;

	// Видимость ведёт сам (паттерн постоянных панелей ADR-048): содержимое видно, только пока
	// игрок хромает и не открыт модальный экран. Игрока берёт у владеющего контроллера каждый
	// тик — переживает респаун. Прячется корень дерева, не сам виджет (USelfHidingWidget).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_LimpIndicator.
	void BuildCodeTree();

	// Раскладывает собственное поле Style по уже построенному кодовому дереву (зовётся из
	// NativeOnInitialized СРАЗУ после BuildCodeTree — дерево обязано быть готово). При
	// дизайнер-дереве ничего не делает — вид целиком в ассете (было публичным ApplyStyle,
	// параметр убран: с 08-29 виджет владеет своим стилем сам, снаружи он больше не задаётся).
	void ApplyStyle();

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	// Фиксирует ширину плашки; высота растёт за текстом (авто-перенос).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> WidthBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Plate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> IndicatorText;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;
};
