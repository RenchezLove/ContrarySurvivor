// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ContrarySurvivor/UI/OnboardingHintWidget.h" // FOnboardingHintStyle (стиль тоста)
#include "OnboardingComponent.generated.h"

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

	// --- Тексты подсказок (директива Рината 07-18: настраиваются в BP_PlayerCharacter).
	// Дефолты дословно прежние зашитые; клавиши в них сверены с реальными биндингами проекта:
	// движение W/A/S/D (IMC_Default), атака — клик (IA_Fire=ЛКМ), смена оружия Q, подбор E,
	// инвентарь I/Tab (legacy ActionMapping, Config/DefaultInput.ini). ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "3"))
	FString HintTextMovement = TEXT("Передвижение — W, A, S, D. Атака — клик по врагу. Смена оружия — Q");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "4"))
	FString HintTextPickup = TEXT("Нажми E, чтобы подобрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "5"))
	FString HintTextElder = TEXT("Поговори со старостой — у него есть работа");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "6"))
	FString HintTextInventory = TEXT("Слева — слоты брони. Броня снижает урон — следи за строкой «Защита»");

	// СТРОГО эта формулировка (ADR-044 п.3): БЕЗ «можно вернуться и забрать».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "7"))
	FString HintTextDeath = TEXT("Часть монет утрачена. Расходники обронены на месте гибели.");

	// Стиль тоста (цвет плашки/текста, шрифт, позиция, размер) — применяется при создании виджета.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (DisplayPriority = "8"))
	FOnboardingHintStyle HintStyle;

protected:
	virtual void BeginPlay() override;

private:
	// Текст подсказки — из EditAnywhere-полей выше.
	FString GetHintText(EOnboardingHint Hint) const;

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
