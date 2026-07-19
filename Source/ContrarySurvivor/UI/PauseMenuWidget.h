// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UVerticalBox;
class UBorder;
class UTextBlock;

/**
 * Стиль меню паузы. Живёт EditAnywhere-полем на контроллере (виджет строится из C++-класса
 * и в Details не виден — паттерн FTouchControlsConfig; директива Рината 07-18). Дефолты
 * дословно повторяют прежние зашитые значения.
 *
 * Локализация (ADR-050): подписи — FText с дефолтами через NSLOCTEXT (LOCTEXT в значении
 * по умолчанию UHT запрещает — UhtTextProperty.cs:104). Дерево строится кодом, ассета в
 * дизайнере у панели нет, поэтому подпись и значение по кубикам не разделяются.
 */
USTRUCT(BlueprintType)
struct FPauseMenuStyle
{
	GENERATED_BODY()

	// Затемнение экрана под панелью.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor DimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	// Золотой кант панели и тёмный фон панели (палитра модалок HUD).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor FrameColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor PanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText TitleText = NSLOCTEXT("PauseMenuWidget", "TitleText", "ПАУЗА");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu", meta = (ClampMin = "8"))
	int32 TitleFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText ResumeText = NSLOCTEXT("PauseMenuWidget", "ResumeText", "Продолжить");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText QuitText = NSLOCTEXT("PauseMenuWidget", "QuitText", "Выход");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu", meta = (ClampMin = "8"))
	int32 ButtonFontSize = 19;

	// Цвет подписей кнопок (тёмный — на светлой штатной кнопке UButton).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor ButtonTextColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	// Габарит кнопки под палец (SizeBox: у UButton 5.5 нет SetPadding).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FVector2D ButtonSize = FVector2D(280.0f, 58.0f);
};

/**
 * Меню паузы (этап G, меню-минимум по решению game-lead: пауза + «Продолжить» + «Выход»).
 * Дерево целиком строится в C++ (WidgetTree), без BP-наследника — паттерн окон этапа F.
 *
 * Виджет ТОЛЬКО рисует и сообщает о нажатиях; паузу мира ставит/снимает владелец
 * (AContrarySurvivorPlayerController::OpenPauseMenu/ClosePauseMenu). Кнопки работают при
 * паузе: Slate игровой паузой не останавливается.
 */
UCLASS()
class CONTRARYSURVIVOR_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// «Продолжить» — владелец снимает паузу и убирает виджет.
	FSimpleMulticastDelegate OnResumeRequested;

	// «Выход» — владелец закрывает игру.
	FSimpleMulticastDelegate OnQuitRequested;

	// Применяет стиль к уже построенному дереву (NativeOnInitialized отработал в CreateWidget
	// с дефолтами). Зовёт контроллер сразу после создания виджета (OpenPauseMenu).
	void ApplyStyle(const FPauseMenuStyle& Style);

protected:
	virtual void NativeOnInitialized() override;

	// Модальный барьер: клик/тап мимо кнопок гасится здесь и в мир не проходит
	// (затемнение-подложка Visible ловит хит-тест, событие всплывает сюда).
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleQuitClicked();

private:
	// Кнопка меню с подписью, обёрнутая в SizeBox тач-размера (мин. высота под палец),
	// добавленная в колонку. Возвращает кнопку для подписки OnClicked.
	UButton* MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName);

	// Элементы дерева, которые перекрашивает ApplyStyle.
	UPROPERTY()
	TObjectPtr<UBorder> DimmerBorder;

	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResumeLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuitLabel;

	UPROPERTY()
	TArray<TObjectPtr<class USizeBox>> ButtonBoxes;
};
