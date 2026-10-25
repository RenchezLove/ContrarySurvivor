// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chapter2ScreenWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UProgressBar;
class UBorder;
class UWidget;
class UChapter2Component;

/** Какой слой экрана главы 2 рисует этот виджет (у каждого своя высота над миром). */
UENUM()
enum class EChapter2ScreenLayer : uint8
{
	MaskGlass, // стекло маски поверх мира, под панелями (картинка поверх экрана, без обработки кадра)
	Bars,      // шкала воздуха [220] и детектор [225]
	Story      // фотография из ноутбука и экран «Конец второй главы» [253] — поверх окна разговора
};

/**
 * Экран главы 2 (25.10.2026). Обычные элементы интерфейса, дерево строится кодом; что
 * показывать, виджет каждый кадр спрашивает у состояния героя (UChapter2Component).
 * Сам виджет никогда не прячется целиком — прячутся его части (иначе пропал бы его тик).
 * Числа, цвета и картинки — UChapter2Settings.
 */
UCLASS()
class CONTRARYSURVIVOR_API UChapter2ScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Выбрать слой и построить его дерево. Звать сразу после создания, ДО постановки на экран:
	// дерево должно существовать, когда движок собирает виджет для показа.
	void InitLayer(EChapter2ScreenLayer InLayer);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	UChapter2Component* FindChapter2() const;
	static void SetShown(UWidget* Widget, bool bShown);

	EChapter2ScreenLayer Layer = EChapter2ScreenLayer::Bars;
	bool bTreeBuilt = false;

	UPROPERTY(Transient)
	TObjectPtr<UImage> GlassImage;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> AirRow;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> AirBar;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DetectorRow;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> DetectorBar;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PhotoImage;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EndCard;

	// Картинки ищутся один раз (поле настроек или файл по ожидаемому пути).
	bool bGlassResolved = false;
	bool bPhotoResolved = false;
	bool bHasGlass = false;
	bool bHasPhoto = false;
};
