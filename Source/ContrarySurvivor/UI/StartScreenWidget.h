// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartScreenWidget.generated.h"

class UButton;
class UVerticalBox;
class UBorder;
class UTextBlock;
class USizeBox;

/**
 * Стиль стартового экрана (Б3: издатель убрал полноценное меню из первой выкладки, ADR-059 —
 * «делай минимум: два пункта, ничего лишнего»). Живёт EditAnywhere-полем на контроллере, дерево
 * строится из C++-класса без BP-наследника (паттерн FPauseMenuStyle/UPauseMenuWidget).
 *
 * Локализация (ADR-050): подписи — FText с дефолтами через NSLOCTEXT (LOCTEXT в значении по
 * умолчанию UHT запрещает — UhtTextProperty.cs:104).
 */
USTRUCT(BlueprintType)
struct FStartScreenStyle
{
	GENERATED_BODY()

	// Затемнение экрана под панелью.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor DimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	// Золотой кант панели и тёмный фон панели (палитра модалок HUD, как у меню паузы).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor FrameColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor PanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.97f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText TitleText = NSLOCTEXT("StartScreenWidget", "TitleText", "С ВОЗВРАЩЕНИЕМ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (ClampMin = "8"))
	int32 TitleFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	// Б3: найден сейв с настоящим прогрессом — предлагаем «Продолжить» или честно начать заново.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText SubtitleText = NSLOCTEXT("StartScreenWidget", "SubtitleText", "Найдено сохранение прошлой игры.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (ClampMin = "8"))
	int32 SubtitleFontSize = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor SubtitleColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText ContinueText = NSLOCTEXT("StartScreenWidget", "ContinueText", "Продолжить");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText NewGameText = NSLOCTEXT("StartScreenWidget", "NewGameText", "Новая игра");

	// --- Подтверждение «Новая игра» (решение лида 08-05: случайное касание на телефоне
	// стирает чужой прогресс без возможности отмены — нужен один явный переспрос). Первый клик
	// по «Новая игра» НЕ стирает сейв — панель переключается в этот режим (те же две кнопки,
	// подписи меняются); «Отмена» возвращает обычный выбор, «Да» стирает по-настоящему. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmTitleText = NSLOCTEXT("StartScreenWidget", "ConfirmTitleText", "ТОЧНО ЗАНОВО?");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmSubtitleText = NSLOCTEXT("StartScreenWidget", "ConfirmSubtitleText",
		"Прежний прогресс будет стёрт без возможности отмены.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmYesText = NSLOCTEXT("StartScreenWidget", "ConfirmYesText", "Да, начать заново");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmCancelText = NSLOCTEXT("StartScreenWidget", "ConfirmCancelText", "Отмена");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (ClampMin = "8"))
	int32 ButtonFontSize = 19;

	// Цвет подписей кнопок (тёмный — на светлой штатной кнопке UButton).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor ButtonTextColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	// Габарит кнопки под палец (SizeBox: у UButton 5.5 нет SetPadding).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FVector2D ButtonSize = FVector2D(280.0f, 58.0f);
};

/**
 * Стартовый экран (Б3: «Продолжить» / «Новая игра»). Показывается ТОЛЬКО когда найден сейв с
 * реальным прогрессом (AContrarySurvivorPlayerController::MaybeStartIntro) — нет сейва/сейв
 * пустой -> экран не создаётся вовсе, игра сразу начинает новую игру (издатель убрал
 * полноценное меню из объёма первой выкладки, ADR-059).
 *
 * Дерево целиком строится в C++ (WidgetTree), без BP-наследника — паттерн UPauseMenuWidget.
 * Виджет ТОЛЬКО рисует и сообщает о нажатиях; владелец (контроллер) решает, что грузить/стирать.
 */
UCLASS()
class CONTRARYSURVIVOR_API UStartScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// «Продолжить» — владелец грузит сейв (APlayerCharacter::LoadGameForContinue), интро не играет.
	FSimpleMulticastDelegate OnContinueRequested;

	// «Новая игра» — владелец стирает сейв (ResetToNewGame) и запускает интро с нуля.
	FSimpleMulticastDelegate OnNewGameRequested;

	// Применяет стиль к уже построенному дереву (NativeOnInitialized отработал в CreateWidget
	// с дефолтами). Зовёт контроллер сразу после создания виджета (OpenStartScreen). Запоминает
	// стиль (CachedStyle) — переспрос «Новая игра» и отмена переключают подписи без пересоздания.
	void ApplyStyle(const FStartScreenStyle& Style);

protected:
	virtual void NativeOnInitialized() override;

	// Модальный барьер: клик/тап мимо кнопок гасится здесь и в мир не проходит.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	// «Продолжить» в обычном режиме; в режиме переспроса эта же кнопка подписана «Отмена»
	// и возвращает обычный выбор, а не грузит сейв.
	UFUNCTION()
	void HandleContinueClicked();

	// «Новая игра»: первый клик ТОЛЬКО включает переспрос (сейв ещё цел); в режиме переспроса
	// эта же кнопка подписана «Да, начать заново» и уже по-настоящему стирает прогресс.
	UFUNCTION()
	void HandleNewGameClicked();

private:
	// Кнопка меню с подписью, обёрнутая в SizeBox тач-размера, добавленная в колонку.
	UButton* MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName);

	// Подписи панели/кнопок для текущего режима (обычный выбор либо переспрос «Новая игра»).
	// Цвета/размеры не трогает — та же ApplyStyle с другим набором текстов.
	void ApplyChoiceLabels(const FStartScreenStyle& Style);
	void ApplyConfirmLabels(const FStartScreenStyle& Style);

	// Стиль, переданный ApplyStyle — нужен, чтобы переключаться между обычным выбором и
	// переспросом «Новая игра» без пересоздания дерева.
	FStartScreenStyle CachedStyle;

	// Идёт переспрос «Точно начать заново?» (кнопки временно переподписаны).
	bool bConfirmingNewGame = false;

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
	TObjectPtr<UTextBlock> SubtitleBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> ContinueLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> NewGameLabel;

	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> ButtonBoxes;
};
