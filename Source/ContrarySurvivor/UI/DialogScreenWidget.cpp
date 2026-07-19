// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DialogScreenWidget.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UDialogScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (AcceptButton)
	{
		AcceptButton->OnClicked.AddDynamic(this, &UDialogScreenWidget::HandleAcceptClicked);
	}
	if (DeclineButton)
	{
		DeclineButton->OnClicked.AddDynamic(this, &UDialogScreenWidget::HandleDeclineClicked);
	}
	if (TurnInButton)
	{
		TurnInButton->OnClicked.AddDynamic(this, &UDialogScreenWidget::HandleTurnInClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UDialogScreenWidget::HandleCloseClicked);
	}
	if (!AcceptButton || !CloseButton)
	{
		// Без этих кнопок диалог не пройти мышью/пальцем (остаются QA-клавиши G/H и клавиша E).
		UE_LOG(LogQA, Warning,
			TEXT("DialogScreenWidget: не найдены кубики %s%s в WBP_Dialog — часть ответов недоступна"),
			!AcceptButton ? TEXT("AcceptButton ") : TEXT(""),
			!CloseButton ? TEXT("CloseButton") : TEXT(""));
	}
}

void UDialogScreenWidget::InitDialog(AElderNPC* InElder, APlayerCharacter* InPlayer)
{
	Elder = InElder;
	Player = InPlayer;
	bEverRefreshed = false; // форс-обновление на первом тике/сразу
	RefreshDialog();
}

void UDialogScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshDialog(); // внутри diff: перестраивает только при смене квеста/состояния
}

void UDialogScreenWidget::RefreshDialog()
{
	if (!Elder || !Player)
	{
		return;
	}

	UQuestComponent* Quests = Player->GetQuests();

	// Актуальный квест старосты по порядку + синхронизация журнала КАЖДОЕ обновление
	// (идемпотентно): сдали кв.1 в этой же сессии диалога — тут же предлагается кв.2,
	// и [Принять] работает без переоткрытия (перенос поведения Canvas DrawDialog).
	const FQuest& Offered = Elder->GetQuestForPlayer(Quests);
	if (Quests)
	{
		Quests->OfferQuest(Offered);
	}
	const FQuest* InLog = Quests ? Quests->FindQuest(Offered.QuestId) : nullptr;
	const EQuestState State = InLog ? InLog->State : EQuestState::NotStarted;
	const FQuest& QData = InLog ? *InLog : Offered;

	// Diff: тексты и видимость кнопок трогаем только при реальной смене.
	if (bEverRefreshed && LastQuestId == QData.QuestId && LastState == State)
	{
		return;
	}
	bEverRefreshed = true;
	LastQuestId = QData.QuestId;
	LastState = State;

	if (NPCNameText)
	{
		NPCNameText->SetText(Elder->GetDialogueDisplayName());
	}

	// Реплика по состоянию: сами фразы — редактируемые поля старосты, сборка строки с
	// числами — формат этой панели (ADR-050).
	FText NPCText;
	switch (State)
	{
		case EQuestState::NotStarted:
		{
			NPCText = FText::FromString(Offered.Description);
			// Ранний сюжетный крючок (ADR-049 п.2) — только в ПЕРВОМ квесте, пока игрок его
			// не взял. Полный крючок по-прежнему после сдачи ноутбука (кв.3), ADR-044.
			if (Offered.QuestId == Elder->GetOfferedQuest().QuestId
				&& !Elder->GetDialogueEarlyHookText().IsEmpty())
			{
				NPCText = FText::Join(EarlyHookSeparator,
					NPCText, Elder->GetDialogueEarlyHookText());
			}
			break;
		}
		case EQuestState::Active:
		{
			// Цели — общий сборщик (тот же, что у трекера квеста): человеческие подписи
			// вместо служебных тегов, единый формат в одном месте.
			FFormatNamedArguments Args;
			Args.Add(TEXT("Prefix"), Elder->GetDialogueActivePrefix());
			Args.Add(TEXT("Title"), FText::FromString(QData.Title));
			Args.Add(TEXT("Objectives"),
				QuestObjectiveText::BuildObjectives(QData, ObjectiveFormat, ObjectiveSeparator));
			NPCText = FText::Format(ActiveReplicaFormat, Args);
			break;
		}
		case EQuestState::Completed:
			NPCText = Elder->GetDialogueCompletedText();
			break;
		case EQuestState::TurnedIn:
			NPCText = Elder->GetDialogueTurnedInText();
			break;
		default:
			break;
	}
	if (ReplicaText)
	{
		ReplicaText->SetText(NPCText);
	}

	// Видимость кнопок по состоянию (как набор кнопок Canvas DrawDialog).
	auto SetShown = [](UButton* Button, bool bShown)
	{
		if (Button)
		{
			Button->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	};
	SetShown(AcceptButton,  State == EQuestState::NotStarted);
	SetShown(DeclineButton, State == EQuestState::NotStarted);
	SetShown(TurnInButton,  State == EQuestState::Completed);
	SetShown(CloseButton,   State == EQuestState::Active || State == EQuestState::TurnedIn);

	if (TurnInText && State == EQuestState::Completed)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Reward"), FText::AsNumber(FMath::RoundToInt32(QData.RewardMoney)));
		TurnInText->SetText(FText::Format(TurnInFormat, Args));
	}
}

void UDialogScreenWidget::HandleAcceptClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: dialog choice ACCEPT (UMG)"));
	if (Player && Elder)
	{
		if (UQuestComponent* Quests = Player->GetQuests())
		{
			// Журнал синхронизирован в RefreshDialog (OfferQuest идемпотентен) — принимаем.
			Quests->AcceptQuest(Elder->GetQuestForPlayer(Quests).QuestId);
		}
	}
}

void UDialogScreenWidget::HandleDeclineClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: dialog choice DECLINE (UMG)"));
	OnCloseRequested.Broadcast();
}

void UDialogScreenWidget::HandleTurnInClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: dialog choice TURN IN (UMG)"));
	if (Player && Elder)
	{
		if (UQuestComponent* Quests = Player->GetQuests())
		{
			Quests->TurnInQuest(Elder->GetQuestForPlayer(Quests).QuestId);
		}
	}
}

void UDialogScreenWidget::HandleCloseClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: dialog choice CLOSE (UMG)"));
	OnCloseRequested.Broadcast();
}
