// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // EQuestState (кэш состояния для diff-обновления)
#include "DialogScreenWidget.generated.h"

class UTextBlock;
class UButton;
class AElderNPC;
class APlayerCharacter;

/**
 * Экран диалога со старостой на UMG (ADR-048, этап 2). Логика — здесь; раскладку
 * WBP_Dialog строит Ринат по схеме docs/contrary-survivor/umg-layout-guide.md.
 * Кубики цепляются по ТОЧНЫМ именам (BindWidgetOptional: нет кубика — предупреждение
 * в лог, не краш).
 *
 * Поток состояний — тот же, что Canvas DrawDialog/HandleDialogClick:
 *   NotStarted -> реплика-описание квеста + [Принять]/[Отказаться];
 *   Active     -> «Ты ещё не закончил…» + [Закрыть];
 *   Completed  -> «Отлично! …» + [Сдать (+награда)];
 *   TurnedIn   -> «Спасибо…» + [Закрыть].
 * Реплики состояний — EditAnywhere-поля AElderNPC (у каждого старосты свои);
 * действия — существующие UQuestComponent::OfferQuest/AcceptQuest/TurnInQuest.
 * Закрытие — делегат OnCloseRequested (подписан контроллер, CloseDialog).
 */
UCLASS()
class CONTRARYSURVIVOR_API UDialogScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязка данных после CreateWidget (староста + игрок) и первое обновление.
	void InitDialog(AElderNPC* InElder, APlayerCharacter* InPlayer);

	// [Отказаться]/[Закрыть] нажаты — подписан контроллер (CloseDialog).
	FSimpleMulticastDelegate OnCloseRequested;

	// --- Настройки (Class Defaults WBP_Dialog; владение переехало из HUD — ADR-048) ---

	// Кнопка сдачи собирается кодом: Prefix + награда + Suffix = «[ Сдать (+150) ]».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "1"))
	FString TurnInPrefix = TEXT("[ Сдать (+");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "2"))
	FString TurnInSuffix = TEXT(") ]");

protected:
	virtual void NativeOnInitialized() override;

	// Каждый кадр сверяет актуальный квест/состояние и перестраивает тексты/кнопки
	// ТОЛЬКО при изменении (сдали кв.1 — тут же предлагается кв.2, как в Canvas-пути).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleAcceptClicked();
	UFUNCTION() void HandleDeclineClicked();
	UFUNCTION() void HandleTurnInClicked();
	UFUNCTION() void HandleCloseClicked();

	// Полное обновление: реплика + имя NPC + видимость кнопок по состоянию квеста.
	void RefreshDialog();

	// --- Кубики WBP_Dialog (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Имя NPC в шапке («СТАРОСТА» — из поля старосты).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NPCNameText;

	// Реплика старосты (включи Auto Wrap Text в WBP — длинные реплики многострочные).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReplicaText;

	// Кнопки-ответы. Код переключает их видимость по состоянию квеста; подписи
	// [Принять]/[Отказаться]/[Закрыть] пишет Ринат в WBP, подпись [Сдать (+N)] ставит код.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AcceptButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DeclineButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TurnInButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	// Подпись кнопки сдачи (лежит ВНУТРИ TurnInButton) — «[ Сдать (+150) ]».
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnInText;

private:
	UPROPERTY()
	TObjectPtr<AElderNPC> Elder;

	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// Кэш для diff-обновления (перестраиваемся только при смене квеста/состояния).
	FName LastQuestId = NAME_None;
	EQuestState LastState = EQuestState::NotStarted;
	bool bEverRefreshed = false;
};
