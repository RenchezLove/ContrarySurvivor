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
 * Дерево целиком строится в C++ (WidgetTree), .uasset не нужен — создаётся напрямую
 * CreateWidget<ULimpIndicatorWidget>(PC, ULimpIndicatorWidget::StaticClass()).
 */
UCLASS()
class CONTRARYSURVIVOR_API ULimpIndicatorWidget : public USelfHidingWidget
{
	GENERATED_BODY()

public:
	// Текст плашки (компактный индикатор либо развёрнутая разовая подсказка) — готовый
	// переводимый FText (ADR-050), собирает APlayerCharacter из своих EditAnywhere-полей.
	void SetIndicatorText(const FText& Text);

	// Применяет стиль к уже построенному дереву (NativeOnInitialized отработал в CreateWidget
	// с дефолтами). Зовёт APlayerCharacter сразу после создания виджета.
	void ApplyStyle(const FLimpIndicatorStyle& Style);

protected:
	virtual void NativeOnInitialized() override;

	// Видимость ведёт сам (паттерн постоянных панелей ADR-048): содержимое видно, только пока
	// игрок хромает и не открыт модальный экран. Игрока берёт у владеющего контроллера каждый
	// тик — переживает респаун. Прячется корень дерева, не сам виджет (USelfHidingWidget).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Фиксирует ширину плашки; высота растёт за текстом (авто-перенос).
	UPROPERTY()
	TObjectPtr<USizeBox> WidthBox;

	UPROPERTY()
	TObjectPtr<UBorder> Plate;

	UPROPERTY()
	TObjectPtr<UTextBlock> IndicatorText;
};
