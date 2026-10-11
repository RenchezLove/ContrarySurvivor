// Fill out your copyright notice in the Description page of Project Settings.

#include "TalkerNPC.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Data/ContraryQuestLibrary.h" // задание из таблицы заданий (как у старосты)
#include "ContrarySurvivor/ContrarySurvivor.h"          // LogQA
#include "ContrarySurvivor/Actors/ScientistNPCs.h"      // метка квестового учёного (задание, которым кончается сюжет)
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"   // сообщение о конце сюжета

ATalkerNPC::ATalkerNPC()
{
	// Стоит на месте: тик не нужен, управление ИИ подавлено, урон не проходит — всё как у старосты.
	PrimaryActorTick.bCanEverTick = false;
	bImmuneToDamage = true;
	AutoPossessAI = EAutoPossessAI::Disabled;

	InteractTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(GetRootComponent());
	InteractTrigger->InitSphereRadius(InteractRadius);
	InteractTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractTrigger->SetGenerateOverlapEvents(true);
	InteractTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATalkerNPC::OnInteractBeginOverlap);
	InteractTrigger->OnComponentEndOverlap.AddDynamic(this, &ATalkerNPC::OnInteractEndOverlap);

	// Выравнивание модульного меша под капсулу — как у старосты и торговца.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	}
}

void ATalkerNPC::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Торс и ноги следуют за позой головы — тот же механизм, что у старосты.
	if (USkeletalMeshComponent* Head = GetMesh())
	{
		if (TorsoMesh)
		{
			TorsoMesh->SetLeaderPoseComponent(Head);
		}
		if (LegsMesh)
		{
			LegsMesh->SetLeaderPoseComponent(Head);
		}
	}

	// Радиус зоны — с размещённого экземпляра (конструктор видел только значение класса).
	if (InteractTrigger)
	{
		InteractTrigger->SetSphereRadius(InteractRadius);
	}

	// Метка собеседника — ещё и обычная метка актора: по ней метка цели задания находит
	// персонажа тем же поиском, что и любую другую цель.
	if (!NpcTag.IsNone())
	{
		Tags.AddUnique(NpcTag);
	}
}

void ATalkerNPC::BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const
{
	OutView.SpeakerName = DialogueDisplayName;
	OutView.Replica = IdleText;
	OutView.PrimaryLabel = FText::GetEmpty();
	OutView.CloseLabel = CloseReplyText;
}

void ATalkerNPC::OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->SetNearbyTalker(this);
	}
}

void ATalkerNPC::OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->ClearNearbyTalker(this);
	}
}

// ---------------------------------------------------------------------------
// Собеседник с заданием
// ---------------------------------------------------------------------------

AQuestGiverNPC::AQuestGiverNPC()
{
	OfferedQuest.TargetCount = 0; // у структуры по умолчанию цель убийств 5 — здесь её нет
	OfferedQuest.RewardMoney = 0.0f;
	// Задания собеседников, кроме старосты, по умолчанию побочные (10.10.2026).
	OfferedQuest.Kind = EQuestKind::Side;
}

void AQuestGiverNPC::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Задание из таблицы заданий главнее встроенного — тот же порядок, что у старосты.
	FQuest FromTable;
	if (!QuestRow.IsNone() && ContraryQuests::BuildQuestFromRow(QuestRow, FromTable))
	{
		OfferedQuest = FromTable;
	}

	// Своё задание сдаётся самому собеседнику: метка цели на экране при готовом задании
	// поведёт к нему, а не к старосте.
	if (OffersQuest() && OfferedQuest.TurnInNpcTag.IsNone())
	{
		OfferedQuest.TurnInNpcTag = NpcTag;
	}

	// Кто выдал — для строки «Выдал» в окне КПК.
	if (OffersQuest())
	{
		OfferedQuest.GiverNpcTag = NpcTag;
	}
}

const FQuest* AQuestGiverNPC::FindForeignQuestToReceive(const APlayerCharacter* Player) const
{
	const UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Quests || NpcTag.IsNone())
	{
		return nullptr;
	}
	for (const FQuest& Q : Quests->GetQuests())
	{
		if (Q.State == EQuestState::Completed && Q.TurnInNpcTag == NpcTag
			&& !(OffersQuest() && Q.QuestId == OfferedQuest.QuestId))
		{
			return &Q;
		}
	}
	return nullptr;
}

void AQuestGiverNPC::OnTalkOpened(APlayerCharacter* Player)
{
	ForeignRewardThisTalk = -1.0f;
	bStoryEndingQuestReceivedThisTalk = false;
	bOwnQuestTurnedInThisTalk = false;

	// Своё задание попадает в журнал при первом разговоре (повторно — ничего не меняет).
	if (OffersQuest() && Player && Player->GetQuests())
	{
		Player->GetQuests()->OfferQuest(OfferedQuest);
	}
}

void AQuestGiverNPC::BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const
{
	Super::BuildTalkView(Player, OutView);

	const UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;

	// 1) Герой принёс то, что велено сдать этому собеседнику.
	if (const FQuest* Foreign = FindForeignQuestToReceive(Player))
	{
		OutView.Replica = ForeignTurnInText;
		OutView.PrimaryLabel = !Foreign->TurnInReplyText.IsEmpty() ? Foreign->TurnInReplyText : ForeignTurnInReplyText;
		if (!Foreign->CloseReplyText.IsEmpty())
		{
			OutView.CloseLabel = Foreign->CloseReplyText;
		}
		return;
	}

	// 2) Только что принял — благодарит и платит, в том же разговоре.
	if (ForeignRewardThisTalk >= 0.0f)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Reward"), FText::AsNumber(FMath::RoundToInt32(ForeignRewardThisTalk)));
		OutView.Replica = FText::Format(ForeignTurnInDoneText, Args);
		return;
	}

	// 3) Своё задание — по его состоянию в журнале героя.
	if (OffersQuest())
	{
		const FQuest* InLog = Quests ? Quests->FindQuest(OfferedQuest.QuestId) : nullptr;
		const FQuest& QData = InLog ? *InLog : OfferedQuest;
		if (!QData.CloseReplyText.IsEmpty())
		{
			OutView.CloseLabel = QData.CloseReplyText;
		}
		switch (InLog ? InLog->State : EQuestState::NotStarted)
		{
			case EQuestState::NotStarted:
				OutView.Replica = QData.Description;
				OutView.PrimaryLabel = !QData.AcceptReplyText.IsEmpty() ? QData.AcceptReplyText
					: NSLOCTEXT("Talker", "FallbackAcceptReply", "Берусь.");
				break;
			case EQuestState::Active:
				OutView.Replica = QuestActiveText;
				break;
			case EQuestState::Completed:
				OutView.Replica = QuestCompletedText;
				OutView.PrimaryLabel = !QData.TurnInReplyText.IsEmpty() ? QData.TurnInReplyText : ForeignTurnInReplyText;
				break;
			case EQuestState::TurnedIn:
				// В том разговоре, где задание сдано, собеседник говорит про награду прямо;
				// во всех следующих — обычную реплику «задание сдано».
				OutView.Replica = (bOwnQuestTurnedInThisTalk && !QuestJustTurnedInText.IsEmpty())
					? QuestJustTurnedInText : QuestTurnedInText;
				break;
			default:
				break;
		}
		return;
	}

	// 4) Чужое задание уже принято раньше — реплика-заглушка до продолжения.
	if (Quests && Quests->HasTurnedInQuestForReceiver(NpcTag) && !AfterForeignTurnInText.IsEmpty())
	{
		OutView.Replica = AfterForeignTurnInText;
	}
	// 5) Иначе — нейтральная реплика базового класса.
}

bool AQuestGiverNPC::IsStoryEndingQuest(const FQuest& Quest)
{
	return !Quest.TurnInNpcTag.IsNone() && Quest.TurnInNpcTag == ScientistBaseTags::QuestScientist;
}

bool AQuestGiverNPC::IsStoryEndingQuestTurnedIn(const TArray<FQuest>& Journal)
{
	return Journal.ContainsByPredicate([](const FQuest& Q)
		{ return Q.State == EQuestState::TurnedIn && IsStoryEndingQuest(Q); });
}

bool AQuestGiverNPC::IsStoryEndingQuestTaken(const TArray<FQuest>& Journal)
{
	return Journal.ContainsByPredicate([](const FQuest& Q)
		{ return Q.State != EQuestState::NotStarted && IsStoryEndingQuest(Q); });
}

void AQuestGiverNPC::OnTalkClosed(APlayerCharacter* Player)
{
	// Разговор, в котором принят ноутбук, закрылся (реплика-заглушка прочитана) — экран игры
	// планирует сообщение о конце сюжета с прежней задержкой; один раз за сохранение
	// (признак в сохранении ведёт сам экран).
	if (ShouldAnnounceStoryEndOnClose() && Player)
	{
		if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			if (AContrarySurvivorHUD* CSHUD = PC->GetHUD<AContrarySurvivorHUD>())
			{
				CSHUD->NotifyStoryEpilogueFinished();
			}
		}
	}
	bStoryEndingQuestReceivedThisTalk = false;
}

bool AQuestGiverNPC::HandleTalkPrimary(APlayerCharacter* Player)
{
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Quests)
	{
		return true;
	}

	// Тот же порядок, что в BuildTalkView: сначала приём чужого задания.
	if (const FQuest* Foreign = FindForeignQuestToReceive(Player))
	{
		const FName ForeignId = Foreign->QuestId;
		const float Reward = Foreign->RewardMoney;
		if (Quests->TurnInQuest(ForeignId, NpcTag))
		{
			ForeignRewardThisTalk = Reward;
			if (const FQuest* Received = Quests->FindQuest(ForeignId))
			{
				bStoryEndingQuestReceivedThisTalk = IsStoryEndingQuest(*Received);
			}
			UE_LOG(LogQA, Display, TEXT("QA: talker '%s' принял задание %s"), *NpcTag.ToString(), *ForeignId.ToString());
		}
		return false;
	}

	if (OffersQuest())
	{
		const FQuest* InLog = Quests->FindQuest(OfferedQuest.QuestId);
		if (InLog && InLog->State == EQuestState::NotStarted)
		{
			Quests->AcceptQuest(OfferedQuest.QuestId);
			return false;
		}
		if (InLog && InLog->State == EQuestState::Completed)
		{
			bOwnQuestTurnedInThisTalk = Quests->TurnInQuest(OfferedQuest.QuestId, NpcTag);
			return false;
		}
	}
	return true;
}
