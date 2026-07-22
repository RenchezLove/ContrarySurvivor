// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DialogScreenWidget.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h" // признак «крючок показан» (bElderHookShown)
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

	// Решаем ОДИН раз на всю сессию диалога: играть ли скриптовое интро первой встречи. Дальше
	// поток ведётся по IntroStep и НЕ переоценивается по состоянию квеста — иначе после «Взяться
	// за дело» квест стал бы Active и последняя реплика-крючок не показалась бы в этой же сессии.
	bInIntroSequence = ShouldPlayIntro();
	IntroStep = 0;
	LastShownIntroStep = INDEX_NONE;

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

	// Build 1: пока идёт скриптовое интро первой встречи — реплики по очереди, обычный поток по
	// состоянию квеста отключён (см. RefreshIntroLine/AdvanceIntro).
	if (bInIntroSequence)
	{
		RefreshIntroLine();
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
			// Только предложение квеста. Прощальная фраза-крючок сюда БОЛЬШЕ НЕ ПОПАДАЕТ:
			// по замыслу она говорится вслед, после согласия, а вместе с предложением
			// превращала разговор в простыню (баг владельца 2026-07-20).
			NPCText = Offered.Description;
			// Сколько заплатят — берём из поля награды квеста, а не из текста описания:
			// поменяется баланс — строка поменяется сама.
			if (!RewardLineFormat.IsEmpty() && Offered.RewardMoney > 0.0f)
			{
				FFormatNamedArguments RewardArgs;
				RewardArgs.Add(TEXT("Reward"), FText::AsNumber(FMath::RoundToInt32(Offered.RewardMoney)));
				NPCText = FText::Join(RewardLineSeparator,
					NPCText, FText::Format(RewardLineFormat, RewardArgs));
			}
			break;
		}
		case EQuestState::Active:
		{
			// Цели — общий сборщик (тот же, что у трекера квеста): человеческие подписи
			// вместо служебных тегов, единый формат в одном месте.
			FFormatNamedArguments Args;
			Args.Add(TEXT("Prefix"), Elder->GetDialogueActivePrefix());
			Args.Add(TEXT("Title"), QData.Title);
			Args.Add(TEXT("Objectives"),
				QuestObjectiveText::BuildObjectives(QData, ObjectiveFormat, ObjectiveSeparator));
			NPCText = FText::Format(ActiveReplicaFormat, Args);

			// Build 1: крючок «кто-то гонит чужаков» БОЛЬШЕ не дописывается сюда. Раньше он
			// добавлялся к реплике активного квеста и потому повторялся при каждом повторном
			// разговоре (баг издателя). Теперь крючок — последняя реплика скриптового интро и
			// показывается ОДИН раз (признак bElderHookShown в сейве).
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

	// Подпись кнопки принятия обычного предложения квеста (кв.2/кв.3) — из настройки панели.
	if (AcceptText && State == EQuestState::NotStarted)
	{
		AcceptText->SetText(AcceptButtonLabel);
	}

	if (TurnInText && State == EQuestState::Completed)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Reward"), FText::AsNumber(FMath::RoundToInt32(QData.RewardMoney)));
		TurnInText->SetText(FText::Format(TurnInFormat, Args));
	}
}

void UDialogScreenWidget::HandleAcceptClicked()
{
	// В скриптовом интро AcceptButton — единственная кнопка-ответ: она ведёт диалог по репликам.
	if (bInIntroSequence)
	{
		AdvanceIntro();
		return;
	}

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

// ---------------------------------------------------------------------------
// Скриптовое интро первой встречи (Build 1, ТЗ издателя раздел 3)
// ---------------------------------------------------------------------------

bool UDialogScreenWidget::ShouldPlayIntro() const
{
	if (!Elder || !Player)
	{
		return false;
	}
	if (Elder->GetIntroLines().Num() == 0)
	{
		return false; // у старосты нет реплик интро — работает обычный диалог
	}

	UQuestComponent* Quests = Player->GetQuests();
	if (!Quests)
	{
		return false;
	}

	// Интро — только ПЕРВАЯ встреча: первый квест уже предложен журналу (OfferQuest на открытии
	// диалога), но ещё не принят. Принял/сдал — интро не повторяем, идёт обычный поток по состоянию.
	const FQuest* Q1 = Quests->FindQuest(Elder->GetOfferedQuest().QuestId);
	if (!Q1 || Q1->State != EQuestState::NotStarted)
	{
		return false;
	}

	// Крючок этого профиля уже показан — интро больше не играем (страховка от повтора крючка).
	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		if (Save->bElderHookShown)
		{
			return false;
		}
	}
	return true;
}

void UDialogScreenWidget::RefreshIntroLine()
{
	const TArray<FElderIntroLine>& Lines = Elder->GetIntroLines();
	if (!Lines.IsValidIndex(IntroStep))
	{
		// Реплик не осталось — закрываем (обычно закрытие происходит в AdvanceIntro).
		OnCloseRequested.Broadcast();
		return;
	}
	if (LastShownIntroStep == IntroStep)
	{
		return; // эта реплика уже на экране — не трогаем текст каждый кадр
	}
	LastShownIntroStep = IntroStep;

	const FElderIntroLine& Line = Lines[IntroStep];

	if (NPCNameText)
	{
		NPCNameText->SetText(Elder->GetDialogueDisplayName());
	}
	if (ReplicaText)
	{
		ReplicaText->SetText(Line.NPCText);
	}
	if (AcceptText)
	{
		AcceptText->SetText(Line.ButtonLabel);
	}

	// Ровно ОДНА кнопка-ответ (AcceptButton); остальные кнопки на время интро скрыты.
	auto SetShown = [](UButton* Button, bool bShown)
	{
		if (Button)
		{
			Button->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	};
	SetShown(AcceptButton,  true);
	SetShown(DeclineButton, false);
	SetShown(TurnInButton,  false);
	SetShown(CloseButton,   false);

	// Последняя реплика интро — сюжетный крючок. Помечаем «показан» в сейве, чтобы при повторном
	// разговоре он не повторялся (баг издателя). Пишем один раз за профиль.
	if (IntroStep == Lines.Num() - 1)
	{
		if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			if (!Save->bElderHookShown)
			{
				Save->bElderHookShown = true;
				Player->WriteSaveObject(Save);
			}
		}
	}
}

void UDialogScreenWidget::AdvanceIntro()
{
	if (!Elder || !Player)
	{
		OnCloseRequested.Broadcast();
		return;
	}

	const TArray<FElderIntroLine>& Lines = Elder->GetIntroLines();
	if (!Lines.IsValidIndex(IntroStep))
	{
		OnCloseRequested.Broadcast();
		return;
	}

	// Эффект текущей реплики срабатывает по нажатию её кнопки (действие игрока).
	switch (Lines[IntroStep].Action)
	{
		case EElderIntroAction::GiveGift:
			// Аптечка кладётся в рюкзак тем же путём, что покупки магазина (внутри —
			// GiveConsumableToBackpack), признак «уже выдал» в сейве — повторно не выдаётся.
			Elder->TryGiveFirstMeetingGift(Player);
			break;
		case EElderIntroAction::StartQuest:
			if (UQuestComponent* Quests = Player->GetQuests())
			{
				// Кв.1 уже в журнале (OfferQuest на открытии диалога) — принимаем «Шкуры волков».
				Quests->AcceptQuest(Elder->GetOfferedQuest().QuestId);
				UE_LOG(LogQA, Display, TEXT("QA: dialog intro START QUEST (UMG)"));
			}
			break;
		default:
			break;
	}

	++IntroStep;
	if (!Lines.IsValidIndex(IntroStep))
	{
		// Реплики кончились (последняя кнопка = «Закрыть») — закрываем диалог.
		OnCloseRequested.Broadcast();
		return;
	}
	RefreshIntroLine();
}
