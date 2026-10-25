// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/MercenaryEncounter.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/EnemyAIController.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

AMercenaryEncounter::AMercenaryEncounter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	NpcTag = Chapter2Ids::MercenaryTag;
	// [240]
	DialogueDisplayName = NSLOCTEXT("Chapter2", "MercenaryName", "НАЁМНИК");
	NPCMarkerLabel = FText::GetEmpty();
	InteractPromptAction = FText::GetEmpty();

	// [241]–[244]. Вторая кнопка окна («уйти») тоже начинает бой — уйти от наёмника нельзя.
	MeetText = NSLOCTEXT("Chapter2", "MercenaryMeet",
		"Ну наконец-то, Седьмой. Куратор очень хотел, чтобы ты вернулся. Пойдёшь сам — или тебя донести?");
	MeetReplyText = NSLOCTEXT("Chapter2", "MercenaryMeetReply", "Я тебя не знаю.");
	ThreatText = NSLOCTEXT("Chapter2", "MercenaryThreat",
		"Зато мы тебя знаем. Лучше, чем ты сам себя помнишь. Значит, по-плохому.");
	FightReplyText = NSLOCTEXT("Chapter2", "MercenaryFightReply", "Попробуй.");
	CloseReplyText = FightReplyText;

	MercenaryClass = TSoftClassPtr<AEnemyCharacter>(FSoftObjectPath(TEXT("/Game/Characters/Bandit/BP_EnemyBandit.BP_EnemyBandit_C")));

	// Сама точка встречи невидима, ни с чем не сталкивается и подсказку «поговорить» не даёт:
	// разговор открывается сам, когда герой подошёл к наёмнику.
	SetActorHiddenInGame(true);
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (InteractTrigger)
	{
		InteractTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractTrigger->SetGenerateOverlapEvents(false);
	}
}

void AMercenaryEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateEncounter(Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)));
}

void AMercenaryEncounter::UpdateEncounter(APlayerCharacter* Player)
{
	UChapter2Component* Chapter2 = Player ? Player->GetChapter2() : nullptr;
	if (!Chapter2 || Chapter2->IsMercenaryDefeated())
	{
		return;
	}

	AEnemyCharacter* Merc = Mercenary.Get();
	UStatsComponent* MercStats = Merc ? Merc->GetStats() : nullptr;
	const bool bAlive = MercStats && !MercStats->IsDead();

	if (bMercenarySpawned && !bAlive)
	{
		Chapter2->NotifyMercenaryDefeated();
		return;
	}
	if (!bMercenarySpawned)
	{
		if (Chapter2->ShouldSpawnMercenary())
		{
			SpawnMercenary();
		}
		return;
	}
	if (bFightStarted)
	{
		return;
	}

	// Герой выстрелил первым — разговора не будет.
	if (MercStats->GetHealth() < MercStats->GetMaxHealth())
	{
		StartFight();
		return;
	}

	if (!bTalkDone && FVector::Dist2D(Player->GetActorLocation(), Merc->GetActorLocation()) <= TalkDistance)
	{
		AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController());
		if (PC && !PC->IsAnyModalUIOpen())
		{
			PC->OpenTalk(this);
		}
	}
}

void AMercenaryEncounter::SpawnMercenary()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UClass* Class = TestClass.Get();
	if (!Class)
	{
		Class = MercenaryClass.LoadSynchronous();
	}
	if (!Class)
	{
		// Чертёж не нашёлся — наёмник всё равно появляется, иначе главу не пройти.
		UE_LOG(LogQA, Warning, TEXT("QA: chapter2 чертёж наёмника '%s' не загрузился — беру голый класс врага"),
			*MercenaryClass.ToString());
		Class = AEnemyCharacter::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AEnemyCharacter* Merc = World->SpawnActor<AEnemyCharacter>(Class, GetActorLocation(), GetActorRotation(), Params);
	if (!Merc)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: chapter2 наёмник не появился"));
		return;
	}
	bMercenarySpawned = true;
	Mercenary = Merc;
	Merc->Tags.AddUnique(Chapter2Ids::MercenaryTag);

	// Сильнее обычного бандита: своё здоровье и урон.
	if (UStatsComponent* MercStats = Merc->GetStats())
	{
		MercStats->InitHealth(MercenaryHealth, /*bSetToMax=*/true);
	}
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(Merc->GetController()))
	{
		AI->ApplyCombatOverride(MercenaryMeleeDamage, MercenaryRangedDamage, MercenaryHitChance);
	}

	// Другая окраска снаряжения.
	if (MercenaryOutfitMaterial)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		Merc->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* MeshComp : Meshes)
		{
			for (int32 Index = 0; Index < MeshComp->GetNumMaterials(); ++Index)
			{
				MeshComp->SetMaterial(Index, MercenaryOutfitMaterial);
			}
		}
	}

	// До разговора наёмник стоит и ждёт героя.
	FreezeMercenary(true);
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 наёмник появился (%s, здоровье %.0f)"), *Class->GetName(), MercenaryHealth);
}

void AMercenaryEncounter::FreezeMercenary(bool bFrozen)
{
	AEnemyCharacter* Merc = Mercenary.Get();
	AController* MercController = Merc ? Merc->GetController() : nullptr;
	if (!MercController)
	{
		return;
	}
	MercController->SetActorTickEnabled(!bFrozen);
	if (bFrozen)
	{
		if (AAIController* AI = Cast<AAIController>(MercController))
		{
			AI->StopMovement();
		}
	}
}

void AMercenaryEncounter::StartFight()
{
	if (bFightStarted)
	{
		return;
	}
	bFightStarted = true;
	bTalkDone = true;
	FreezeMercenary(false);
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 бой с наёмником"));
}

void AMercenaryEncounter::OnTalkOpened(APlayerCharacter* Player)
{
	TalkStep = 0;
	// Наёмник поворачивается к герою.
	if (AEnemyCharacter* Merc = Mercenary.Get())
	{
		if (Player)
		{
			FVector ToHero = Player->GetActorLocation() - Merc->GetActorLocation();
			ToHero.Z = 0.0f;
			if (!ToHero.IsNearlyZero())
			{
				Merc->SetActorRotation(ToHero.Rotation());
			}
		}
	}
}

void AMercenaryEncounter::OnTalkClosed(APlayerCharacter* Player)
{
	StartFight();
}

void AMercenaryEncounter::BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const
{
	Super::BuildTalkView(Player, OutView);
	OutView.Replica = (TalkStep == 0) ? MeetText : ThreatText;
	OutView.PrimaryLabel = (TalkStep == 0) ? MeetReplyText : FText::GetEmpty(); // на втором шаге одна кнопка «Попробуй.»
	OutView.CloseLabel = FightReplyText;
}

bool AMercenaryEncounter::HandleTalkPrimary(APlayerCharacter* Player)
{
	if (TalkStep == 0)
	{
		TalkStep = 1;
		return false;
	}
	return true; // «Попробуй.» — разговор закрывается, начинается бой
}
