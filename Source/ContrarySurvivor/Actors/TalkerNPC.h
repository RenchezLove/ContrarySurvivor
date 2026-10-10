// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // база: модульный гуманоид (как староста)
#include "InteractableNPCInterface.h"                            // метка находимости над головой
#include "ContrarySurvivor/Components/QuestComponent.h"          // FQuest (задание собеседника)
#include "TalkerNPC.generated.h"

class USphereComponent;
class APlayerCharacter;

/**
 * Что окно разговора показывает прямо сейчас: имя, реплика и кнопки. Собеседник собирает это
 * сам из своего состояния (ATalkerNPC::BuildTalkView), окно только рисует — поэтому всё
 * поведение разговора проверяется автотестом без окна.
 *
 * Кнопки: есть PrimaryLabel — показываются две («основной ответ» и «уйти»); нет — одна «уйти».
 */
struct FNpcTalkView
{
	FText SpeakerName;
	FText Replica;

	// Основной ответ героя («Берусь», «Вот ноутбук», «Согласиться»). Пусто — основной кнопки нет.
	FText PrimaryLabel;

	// Ответ, закрывающий разговор («Мне пора», «Не сейчас»). Пусто — подпись из самого окна.
	FText CloseLabel;

	bool IsSameAs(const FNpcTalkView& Other) const
	{
		return SpeakerName.EqualTo(Other.SpeakerName) && Replica.EqualTo(Other.Replica)
			&& PrimaryLabel.EqualTo(Other.PrimaryLabel) && CloseLabel.EqualTo(Other.CloseLabel);
	}
};

/**
 * Собеседник (10.10.2026, база учёных): общий родитель персонажей, с которыми герой
 * разговаривает в ТОМ ЖЕ окне, что со старостой (UDialogScreenWidget), — охранник базы,
 * учёные. Сам по себе говорит одну нейтральную реплику; наследники переопределяют
 * BuildTalkView и HandleTalkPrimary.
 *
 * Устроен как староста и торговец: модульный гуманоид, неуязвим, не блокирует выстрелы,
 * не попадает в автоприцел (нет компонента статов), рядом с ним герой получает подсказку
 * «поговорить» — зону задаёт сфера InteractTrigger.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ATalkerNPC : public AMasterHumanoidCharacter, public IInteractableNPCInterface
{
	GENERATED_BODY()

public:
	ATalkerNPC();

	// Метка собеседника латиницей: по ней задание знает, кому его сдавать (FQuest::TurnInNpcTag),
	// а метка цели на экране находит персонажа. Она же добавляется в обычные метки актора.
	FName GetNpcTag() const { return NpcTag; }

	// Что показать в окне разговора прямо сейчас. Зовётся каждый кадр, пока окно открыто.
	virtual void BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const;

	// Нажат основной ответ. true — разговор закрыть, false — окно остаётся (покажет новую реплику).
	virtual bool HandleTalkPrimary(APlayerCharacter* Player) { return true; }

	// Разговор открылся / закрылся (контроллер зовёт оба).
	virtual void OnTalkOpened(APlayerCharacter* Player) {}
	virtual void OnTalkClosed(APlayerCharacter* Player) {}

	// Текст подсказки действия рядом с персонажем («Поговорить с охранником»).
	const FText& GetInteractPromptAction() const { return InteractPromptAction; }

	// --- IInteractableNPCInterface (метка находимости) ---
	virtual FText GetNPCMarkerLabel() const override { return NPCMarkerLabel; }
	virtual float GetNPCMarkerZOffset() const override { return NPCMarkerZOffset; }

protected:
	virtual void PostInitializeComponents() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "1",
		DisplayName = "Метка собеседника (латиницей)",
		ToolTip = "По этой метке задание знает, кому его сдавать, а метка цели на экране находит персонажа. У каждого собеседника своя."))
	FName NpcTag = NAME_None;

	// Имя в шапке окна разговора.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "2",
		DisplayName = "Имя в окне разговора"))
	FText DialogueDisplayName = NSLOCTEXT("Talker", "DefaultName", "НЕЗНАКОМЕЦ");

	// Реплика, когда сказать по делу нечего. Черновик, утверждает Ринат.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "3", MultiLine = "true",
		DisplayName = "Реплика без повода"))
	FText IdleText = NSLOCTEXT("Talker", "DefaultIdle", "Не мешай, у меня работа.");

	// Ответ героя, закрывающий разговор. Пусто — подпись кнопки из самого окна.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "4",
		DisplayName = "Ответ героя: уйти"))
	FText CloseReplyText = NSLOCTEXT("Talker", "DefaultClose", "Мне пора.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "5",
		DisplayName = "Подсказка действия рядом с персонажем"))
	FText InteractPromptAction = NSLOCTEXT("Talker", "DefaultPrompt", "Поговорить");

	// Подпись и подъём метки находимости над головой.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "6",
		DisplayName = "Подпись метки над головой"))
	FText NPCMarkerLabel = NSLOCTEXT("Talker", "DefaultMarker", "Собеседник");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "7"))
	float NPCMarkerZOffset = 320.0f;

	// Зона разговора: пересечение только с героем, движению и выстрелам не мешает.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Talker")
	USphereComponent* InteractTrigger;

	// Радиус зоны разговора (см). Черновик.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker", meta = (DisplayPriority = "8"))
	float InteractRadius = 220.0f;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

/**
 * Собеседник с заданием (10.10.2026). Умеет две вещи, вместе или по отдельности:
 *  1) выдать и принять СВОЁ задание (bOffersQuest) — охранник базы учёных с рацией;
 *  2) принять ЧУЖОЕ задание, которое велено сдать именно ему (FQuest::TurnInNpcTag равен его
 *     метке) — квестовый учёный принимает ноутбук от старосты.
 *
 * Задание — обычный FQuest в журнале героя (UQuestComponent): счёт предметов, изъятие при
 * сдаче, награда, сохранение и события аналитики работают теми же путями, что у старосты.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AQuestGiverNPC : public ATalkerNPC
{
	GENERATED_BODY()

public:
	AQuestGiverNPC();

	virtual void BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const override;
	virtual bool HandleTalkPrimary(APlayerCharacter* Player) override;
	virtual void OnTalkOpened(APlayerCharacter* Player) override;
	virtual void OnTalkClosed(APlayerCharacter* Player) override;

	// Задание, сдачей которого сейчас заканчивается сюжет: то, что сдаётся квестовому учёному
	// (ноутбук). Одно место правды для экрана игры (страховка по сохранению) и собеседника.
	static bool IsStoryEndingQuest(const FQuest& Quest);

	// Пора ли планировать сообщение о конце сюжета при закрытии этого разговора — чистое
	// правило для автотеста (самого экрана игры в нём нет).
	bool ShouldAnnounceStoryEndOnClose() const { return bForeignTurnInEndsStory && bStoryEndingQuestReceivedThisTalk; }

	const FQuest& GetOfferedQuest() const { return OfferedQuest; }
	bool OffersQuest() const { return bOffersQuest && !OfferedQuest.QuestId.IsNone(); }

protected:
	virtual void PostInitializeComponents() override;

	// --- Своё задание ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Задание", meta = (DisplayPriority = "1",
		DisplayName = "Выдаёт своё задание"))
	bool bOffersQuest = false;

	// Строка таблицы заданий. Нашлась и собралась — она главнее задания ниже (как у старосты).
	// Пусто или строки нет — действует задание ниже.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Задание", meta = (DisplayPriority = "2",
		DisplayName = "Задание (строка таблицы заданий, необязательно)"))
	FName QuestRow = NAME_None;

	// Задание, если строки таблицы нет. Числа и тексты — черновик, правятся здесь без кода.
	// «Кому сдавать» подставляется само: метка этого собеседника.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Задание", meta = (DisplayPriority = "3",
		DisplayName = "Задание (если строки таблицы нет)"))
	FQuest OfferedQuest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Задание", meta = (DisplayPriority = "4", MultiLine = "true",
		DisplayName = "Реплика: задание взято, не выполнено"))
	FText QuestActiveText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Задание", meta = (DisplayPriority = "5", MultiLine = "true",
		DisplayName = "Реплика: задание выполнено, ждёт сдачи"))
	FText QuestCompletedText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Задание", meta = (DisplayPriority = "6", MultiLine = "true",
		DisplayName = "Реплика: задание сдано"))
	FText QuestTurnedInText;

	// --- Приём чужого задания (сдаётся этому собеседнику) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Приём задания", meta = (DisplayPriority = "1", MultiLine = "true",
		DisplayName = "Реплика: герой принёс то, что велено сдать"))
	FText ForeignTurnInText;

	// Ответ героя на кнопке сдачи, если у самого задания такой реплики нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Приём задания", meta = (DisplayPriority = "2",
		DisplayName = "Ответ героя: отдать"))
	FText ForeignTurnInReplyText = NSLOCTEXT("Talker", "ForeignTurnInReply", "Держи.");

	// Сразу после приёма, в том же разговоре. {Reward} — награда монетами.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Приём задания", meta = (DisplayPriority = "3", MultiLine = "true",
		DisplayName = "Реплика: сразу после приёма"))
	FText ForeignTurnInDoneText;

	// Все следующие разговоры после приёма. ТОЧКА РАСШИРЕНИЯ: когда появится продолжение,
	// сюда встаёт своё задание этого собеседника (bOffersQuest) — оно показывается раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Приём задания", meta = (DisplayPriority = "4", MultiLine = "true",
		DisplayName = "Реплика: после приёма, при следующих разговорах"))
	FText AfterForeignTurnInText;

	// Решение лида 10.10.2026: сообщение о конце сюжета выходит после того, как этот
	// собеседник принял задание и разговор закрылся (с той же задержкой, что раньше).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Приём задания", meta = (DisplayPriority = "5",
		DisplayName = "После приёма показать сообщение о конце сюжета"))
	bool bForeignTurnInEndsStory = false;

private:
	// В этом разговоре принято задание, которым заканчивается сюжет.
	bool bStoryEndingQuestReceivedThisTalk = false;

	// Чужое готовое задание, которое герой может сдать этому собеседнику прямо сейчас.
	const FQuest* FindForeignQuestToReceive(const APlayerCharacter* Player) const;

	// Награда монетами за принятое в ЭТОМ разговоре чужое задание (меньше нуля — не принимал).
	// Сбрасывается при открытии разговора.
	float ForeignRewardThisTalk = -1.0f;
};
