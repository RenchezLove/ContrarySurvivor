// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnboardingComponent.generated.h"

class UOnboardingHintWidget;
class UContrarySaveGame;

// Контекстные подсказки онбординга (Этап F1). Каждая показывается ОДИН раз за профиль
// (флаг в сейве). Порядок значений — как флаги в UContrarySaveGame.
UENUM()
enum class EOnboardingHint : uint8
{
	Movement,   // старт новой игры: передвижение/атака/смена оружия
	Pickup,     // первый доступный подбор предмета
	Elder,      // первое приближение к старосте
	Inventory,  // первое открытие инвентаря
	Death,      // первая смерть (текст СТРОГО по ADR-044 п.3 — без «можно вернуться и забрать»)
	Count UMETA(Hidden)
};

/**
 * Онбординг первых минут (Этап F1, план v2). Живёт на APlayerCharacter.
 *
 * Показывает лёгкие UMG-всплывашки (UOnboardingHintWidget) по событиям, которые дёргает
 * контроллер (подбор/староста/инвентарь/смерть) и сам компонент (стартовая подсказка
 * движения по таймеру BeginPlay). Скрытие — по таймеру HintDuration или по ЛЮБОМУ вводу
 * (AnyKey-биндинг контроллера -> DismissCurrentHint). Флаги «показано» персистятся в
 * UContrarySaveGame сразу при показе.
 */
UCLASS(ClassGroup = (Retention), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UOnboardingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnboardingComponent();

	// Показать подсказку, если она ещё не показывалась этому профилю. Идемпотентно
	// (повторные вызовы каждый тик безвредны — флаг проверяется в памяти, без чтения диска).
	void TryShowHint(EOnboardingHint Hint);

	// Скрыть активную подсказку (любой ввод игрока / смена экрана).
	void DismissCurrentHint();

	// Время показа подсказки до автоскрытия, сек (задание: ~6-8).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "1.0", DisplayPriority = "1"))
	float HintDuration = 7.0f;

	// Задержка стартовой подсказки движения после начала игры, сек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "0.0", DisplayPriority = "2"))
	float MovementHintDelay = 1.5f;

protected:
	virtual void BeginPlay() override;

private:
	// Текст подсказки. Клавиши сверены с реальными биндингами проекта: движение W/A/S/D
	// (IMC_Default), атака — клик (IA_Fire=ЛКМ), смена оружия Q, подбор E, инвентарь I/Tab
	// (legacy ActionMapping, Config/DefaultInput.ini).
	static FString GetHintText(EOnboardingHint Hint);

	// Записать флаг «показано» в слот сейва (load-or-create, правит только свой флаг).
	void PersistShownFlag(EOnboardingHint Hint);

	void ShowWidget(const FString& Text);
	void HideActiveWidget();

	// Кэш флагов «показано» в памяти (грузится один раз в BeginPlay из сейва): TryShowHint
	// зовётся из Tick контроллера — читать слот с диска каждый тик нельзя.
	bool bShown[static_cast<int32>(EOnboardingHint::Count)] = {};

	UPROPERTY()
	TObjectPtr<UOnboardingHintWidget> ActiveWidget;

	FTimerHandle HideTimer;
	FTimerHandle MovementHintTimer;
};
