// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UVerticalBox;

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
	UButton* MakeMenuButton(UVerticalBox* Column, const FString& Label, const FName& BaseName);
};
