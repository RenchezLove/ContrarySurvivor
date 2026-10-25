// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Chapter2ScreenWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UProgressBar;
class UBorder;
class UWidget;
class UChapter2Component;
class UTouchControlsWidget;

/** Какой слой экрана главы 2 рисует этот виджет (у каждого своя высота над миром). */
UENUM()
enum class EChapter2ScreenLayer : uint8
{
	MaskGlass, // стекло маски поверх мира, под панелями и кнопками (картинка, без обработки кадра)
	Bars,      // шкала воздуха [220]
	Locator,   // детектор-локатор на месте левого стика [225] — сразу под сенсорным слоем
	Story      // фотография из ноутбука и экран «Конец первой главы» [253] — поверх окна разговора
};

/**
 * Экран главы 2 (25.10.2026). Обычные элементы интерфейса, дерево строится кодом; что
 * показывать, виджет каждый кадр спрашивает у состояния героя (UChapter2Component).
 * Сам виджет никогда не прячется целиком — прячутся его части (иначе пропал бы его тик).
 * Числа, цвета и картинки — UChapter2Settings.
 *
 * Детектор-локатор (правка Рината 25.10 вечер): «левый стик превращался в некое подобие
 * локатора. Над ним появлялась бы надпись "детектор", сам стик чернел, от центра стика
 * расходились бы "волны" (зеленая) как у локатора, и вращалась бы линия… На экране локатора
 * артефакты как бы излучают сигналы и локатор их ловит… Со временем… появляется стрелка
 * (слегка прозрачная)». Рисуется простыми фигурами Slate (круги, линии, точки) — без
 * материалов и картинок, дёшево для слабых телефонов. Стоит ровно на круге стика (стик
 * сенсорного слоя на это время крупнее и без своей подложки); экранного стика нет (ПК) — в
 * левом нижнем углу на месте стика.
 */
UCLASS()
class CONTRARYSURVIVOR_API UChapter2ScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Выбрать слой и построить его дерево. Звать сразу после создания, ДО постановки на экран:
	// дерево должно существовать, когда движок собирает виджет для показа.
	void InitLayer(EChapter2ScreenLayer InLayer);

	EChapter2ScreenLayer GetLayer() const { return Layer; }

	// Для автопроверок: корень слоя и картинка стекла (нажатия сквозь стекло проходят).
	UWidget* GetRootForTest() const;
	UImage* GetGlassImageForTest() const { return GlassImage; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void BuildTree();
	UChapter2Component* FindChapter2() const;
	UTouchControlsWidget* FindTouchLayer() const;
	static void SetShown(UWidget* Widget, bool bShown);

	void TickLocator(const FGeometry& MyGeometry, float InDeltaTime, const UChapter2Component* Chapter2, bool bHidden);
	void TickPhoto(const FGeometry& MyGeometry, const UChapter2Component* Chapter2, bool bMenu);

	EChapter2ScreenLayer Layer = EChapter2ScreenLayer::Bars;
	bool bTreeBuilt = false;

	UPROPERTY(Transient)
	TObjectPtr<UImage> GlassImage;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> AirRow;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> AirBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LocatorLabel;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PhotoImage;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EndCard;

	// Картинки ищутся один раз (поле настроек или файл по ожидаемому пути).
	bool bGlassResolved = false;
	bool bPhotoResolved = false;
	bool bHasGlass = false;
	bool bHasPhoto = false;
	float PhotoAspect = 1.5f;

	// --- Локатор: что рисовать в следующем кадре (считается в тике, рисуется в NativePaint) ---

	struct FLocatorSignal
	{
		FVector2D Dir = FVector2D(0.0, -1.0); // направление на экране локатора (длина 1)
		float Range01 = 1.0f;                 // 0 — в центре, 1 — на краю круга
		float Strength = 0.0f;                // сила сигнала 0..1 (ближе — сильнее)
	};

	bool bLocatorVisible = false;
	FVector2D LocatorCenter = FVector2D::ZeroVector; // в точках этого виджета
	float LocatorRadius = 0.0f;
	float LocatorTime = 0.0f;
	float NearestStrength = 0.0f;
	TArray<FLocatorSignal> LocatorSignals;
	bool bLocatorArrow = false;
	FVector2D LocatorArrowDir = FVector2D(0.0, -1.0);

	// Кисти локатора: круг экрана и точки сигналов (собираются один раз из настроек).
	FSlateBrush LocatorDiscBrush;
	FSlateBrush LocatorDotBrush;
};
