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
 *   TurnedIn   -> короткое напоминание про ноутбук/волков (фолбэк «Спасибо…») + [Закрыть].
 * Build 1: подписи кнопок — реплики героя из полей квеста (AcceptReplyText и др.); после
 * сдачи кв.2 один раз играется сценка-намёк NotebookHintLines (замена формального кв.3).
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

	// Подпись кнопки сдачи: {Reward} — награда монетами. Квадратные скобки вокруг всей
	// надписи убраны (ADR-049: выглядели как временная затычка), скобки вокруг награды
	// оставлены — они по делу, показывают прибавку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "1"))
	FText TurnInFormat = NSLOCTEXT("Dialog", "TurnInFormat", "Сдать (+{Reward})");

	// Реплика о незаконченном квесте: {Prefix} — начало фразы со старосты, {Title} —
	// название квеста, {Objectives} — строка целей.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "2", MultiLine = "true"))
	FText ActiveReplicaFormat = NSLOCTEXT("Dialog", "ActiveReplicaFormat", "{Prefix}{Title} — {Objectives}.");

	// Одна цель квеста: {Objective} — что сделать, {Done} — сделано, {Total} — сколько надо.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "3"))
	FText ObjectiveFormat = NSLOCTEXT("Quest", "ObjectiveFormat", "{Objective} {Done} из {Total}");

	// Между двумя целями, когда их у квеста две.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "4"))
	FText ObjectiveSeparator = NSLOCTEXT("Quest", "ObjectiveSeparator", ", ");

	// Ранний сюжетный крючок дописывается к реплике первого квеста через этот разделитель.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "5"))
	FText EarlyHookSeparator = NSLOCTEXT("Dialog", "EarlyHookSeparator", " ");

	// Строка про награду, дописывается к описанию квеста, пока игрок его не взял (решение
	// владельца 2026-07-20 «награду в диалоге указывай»): {Reward} — сумма из поля награды
	// самого квеста, а не вписанная руками, поэтому при смене баланса текст не соврёт.
	// Пусто — строка не дописывается.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "6", MultiLine = "true"))
	FText RewardLineFormat = NSLOCTEXT("Dialog", "RewardLineFormat", "Награда: {Reward} монет.");

	// Между описанием квеста и строкой про награду (по умолчанию — пустая строка).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "7"))
	FText RewardLineSeparator = NSLOCTEXT("Dialog", "RewardLineSeparator", "\n\n");

	// Подпись кнопки принятия квеста в ОБЫЧНОМ диалоге (предложение кв.2/кв.3). В скриптовом
	// интро первой встречи подпись берётся из самой реплики (FElderIntroLine::ButtonLabel).
	// Действует только если в WBP_Dialog есть текстовый кубик AcceptText внутри AcceptButton;
	// нет кубика — кнопка показывает статичную подпись, которую Ринат написал в дизайнере.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Texts", meta = (DisplayPriority = "8"))
	FText AcceptButtonLabel = NSLOCTEXT("Dialog", "AcceptButtonLabel", "Принять");

	// --- Крестик закрытия — ВСЕГДА в углу (ADR-076 п.3). П.0 отчёта 23.08 (ADR-077,
	// системный разворот): кубики DialogCloseCrossButton/DialogCloseCrossText живут В АССЕТЕ
	// (добавляет -augment генератора), Ринат двигает и стилизует их мышкой; код ТОЛЬКО
	// вешает обработчик, ставит знак и управляет видимостью. Кодовое создание УДАЛЕНО —
	// как и кодовый стек кнопок (он и «уронил» кнопки на кадре Рината): раскладка кнопок
	// диалога теперь целиком дизайнерская, перенос длинных реплик — свойством кубиков
	// подписи в ассете (ставит тот же -augment). ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Крестик", meta = (DisplayPriority = "1",
		DisplayName = "Крестик закрытия всегда виден"))
	bool bShowCloseCross = true;

	// Знак «×» (U+00D7, есть в штатном шрифте). Текст — данные, код ставит его на подпись.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Крестик", meta = (DisplayPriority = "2",
		DisplayName = "Знак на крестике"))
	FText CloseCrossLabel = NSLOCTEXT("Dialog", "CloseCross", "×");

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

	// --- Скриптовое интро первой встречи (Build 1, ТЗ издателя раздел 3) ---

	// Нужно ли играть интро СЕЙЧАС: у старосты есть реплики интро, первый квест ещё предложен
	// (NotStarted, не принят) и крючок в этом профиле ещё не показан. Решается один раз в InitDialog.
	bool ShouldPlayIntro() const;

	// Показать текущую реплику интро (IntroLines[IntroStep]): текст + единственная кнопка-ответ,
	// остальные кнопки скрыты. На последней реплике-крючке ставит признак bElderHookShown в сейв.
	void RefreshIntroLine();

	// Нажата кнопка текущей реплики: выполнить её эффект (аптечка/старт квеста) и перейти к
	// следующей реплике; после последней — закрыть диалог.
	void AdvanceIntro();

	// --- Сценка-намёк после сдачи кв.2 (Build 1: замена формального кв.3 намёком в диалоге) ---

	// Пора ли играть сценку СЕЙЧАС: у старосты есть реплики намёка, кв.2 сдан (TurnedIn) и
	// признак bElderNotebookHintShown в сейве ещё не стоит. Зовётся из InitDialog и после
	// успешной сдачи квеста (HandleTurnInClicked) — сценка начинается в той же сессии диалога.
	bool ShouldPlayNotebookHint() const;

	// Показать текущую реплику сценки (NotebookHintLines[NotebookHintStep]) — по образцу
	// RefreshIntroLine. На последней реплике ставит bElderNotebookHintShown в сейв.
	void RefreshNotebookHintLine();

	// Нажата кнопка текущей реплики сценки: перейти к следующей; после последней — закрыть
	// диалог. Эффектов (Action) у реплик сценки нет — поле игнорируется.
	void AdvanceNotebookHint();

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

	// Подпись кнопки-ответа (лежит ВНУТРИ AcceptButton). В скриптовом интро код ставит сюда
	// подпись текущей реплики («Из столицы.», «Взяться за дело», «Закрыть» и т.д.); в обычном
	// диалоге — реплика принятия из квеста (AcceptReplyText), фолбэк — AcceptButtonLabel.
	// Нет кубика — показывается статичная подпись из WBP_Dialog.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AcceptText;

	// Подпись кнопки отказа (лежит ВНУТРИ DeclineButton). Build 1: сюда ставится реплика героя,
	// закрывающая диалог (CloseReplyText квеста, «Мне пора.» — отказ и есть «уйти»). Нет
	// кубика — статичная подпись из WBP_Dialog.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DeclineText;

	// Подпись кнопки закрытия (лежит ВНУТРИ CloseButton). Build 1: реплика героя CloseReplyText
	// квеста («Мне пора.»). Нет кубика — статичная подпись из WBP_Dialog.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseText;

	// Крестик закрытия — кубики ИЗ АССЕТА (-augment; П.0 ADR-077: код их не создаёт и не
	// двигает). Нет в ассете — предупреждение в лог, крестика не будет до прогона генератора.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DialogCloseCrossButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DialogCloseCrossText;

	// Вешает обработчик на крестик из ассета, ставит знак и видимость. NativeOnInitialized.
	void BindCloseCross();

private:

	UPROPERTY()
	TObjectPtr<AElderNPC> Elder;

	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// Подписи кнопок, с которыми окно пришло из дизайнера. Реплики героя приходят данными из
	// квеста и временно занимают эти кубики; когда у квеста своей реплики нет, вернуть надо
	// подпись ВЛАДЕЛЬЦА, а не значение из C++ и не реплику прошлого квеста (UI/OwnerTextGuard.h).
	FText OwnerAcceptText;
	FText OwnerDeclineText;
	FText OwnerTurnInText;
	FText OwnerCloseText;

	// Кэш для diff-обновления (перестраиваемся только при смене квеста/состояния).
	FName LastQuestId = NAME_None;
	EQuestState LastState = EQuestState::NotStarted;
	bool bEverRefreshed = false;

	// --- Состояние скриптового интро (латчится в InitDialog на всю сессию диалога) ---

	// Идёт ли сейчас скриптовое интро (решается один раз при открытии). Пока true — диалог
	// показывает реплики по очереди, а обычный поток по состоянию квеста не работает.
	bool bInIntroSequence = false;

	// Индекс текущей реплики интро в IntroLines старосты.
	int32 IntroStep = 0;

	// Какая реплика уже выведена на экран — чтобы не переустанавливать текст каждый кадр.
	int32 LastShownIntroStep = INDEX_NONE;

	// --- Состояние сценки-намёка после сдачи кв.2 (Build 1, по образцу интро) ---

	// Идёт ли сценка-намёк. Включается в InitDialog (диалог открыт, когда кв.2 уже сдан, а намёк
	// не показан) либо сразу после сдачи кв.2 в этой же сессии диалога (HandleTurnInClicked).
	bool bInNotebookHintSequence = false;

	// Индекс текущей реплики сценки в NotebookHintLines старосты.
	int32 NotebookHintStep = 0;

	// Какая реплика сценки уже на экране (защита от переустановки текста каждый кадр).
	int32 LastShownNotebookHintStep = INDEX_NONE;
};
