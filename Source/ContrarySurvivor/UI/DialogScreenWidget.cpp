// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DialogScreenWidget.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
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
		NPCNameText->SetText(FText::FromString(Elder->GetDialogueDisplayName()));
	}

	// Реплика по состоянию (тексты — EditAnywhere-поля старосты; сборка Active-строки кодом,
	// формат не в редакторе: Prefix + название + « — » + прогресс + «.»).
	FString NPCText;
	switch (State)
	{
		case EQuestState::NotStarted:
			NPCText = Offered.Description;
			break;
		case EQuestState::Active:
		{
			FString ObjStr;
			if (QData.TargetCount > 0)
			{
				ObjStr += FString::Printf(TEXT("%s: %d/%d"),
					*QData.KillTargetTag.ToString(), QData.Progress, QData.TargetCount);
			}
			if (QData.RequiredItemCount > 0)
			{
				if (!ObjStr.IsEmpty()) { ObjStr += TEXT(", "); }
				ObjStr += FString::Printf(TEXT("%s: %d/%d"),
					*QData.RequiredItemName, QData.ItemProgress, QData.RequiredItemCount);
			}
			NPCText = FString::Printf(TEXT("%s%s — %s."),
				*Elder->GetDialogueActivePrefix(), *QData.Title, *ObjStr);
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
		ReplicaText->SetText(FText::FromString(NPCText));
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
		TurnInText->SetText(FText::FromString(FString::Printf(TEXT("%s%.0f%s"),
			*TurnInPrefix, QData.RewardMoney, *TurnInSuffix)));
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
