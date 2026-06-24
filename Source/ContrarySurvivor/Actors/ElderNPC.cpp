// Fill out your copyright notice in the Description page of Project Settings.

#include "ElderNPC.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h" // GetMesh()/TorsoMesh/LegsMesh (Leader Pose)
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"

AElderNPC::AElderNPC()
{
	// Тик старосте не нужен (стоит на месте). База (AMasterHumanoidCharacter) включает тик — гасим.
	PrimaryActorTick.bCanEverTick = false;

	// Авто-AIController подавлен: статичный квест-NPC не должен управляться AI (иначе мог бы
	// крутиться/уезжать по дефолтному поведению). По риску из плана A3.
	AutoPossessAI = EAutoPossessAI::Disabled;

	// Триггер взаимодействия: overlap ТОЛЬКО по Pawn (игроку), QueryOnly + Ignore по всем каналам
	// → не блокирует движение/выстрелы и не ловит ECC_Visibility. Крепим к капсуле-корню Character'а
	// (по образцу AMasterTrader::InteractTrigger).
	InteractTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(GetRootComponent());
	InteractTrigger->InitSphereRadius(InteractRadius);
	InteractTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractTrigger->SetGenerateOverlapEvents(true);
	InteractTrigger->OnComponentBeginOverlap.AddDynamic(this, &AElderNPC::OnInteractBeginOverlap);
	InteractTrigger->OnComponentEndOverlap.AddDynamic(this, &AElderNPC::OnInteractEndOverlap);

	// Выравнивание модульного меша под капсулу (как AMasterTrader / AEnemyCharacter): Z=-90 ставит
	// ноги на дно капсулы, Yaw=-90 разворачивает меш лицом по +X. Сам меш/AnimBP назначает BP_Elder
	// (C++ больше НЕ хардкодит SK/ABP — это модульный гуманоид, A3 шаг 3).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	}

	// СОЗНАТЕЛЬНО НЕ блокируем ECC_Visibility на капсуле и НЕ добавляем UStatsComponent — староста
	// остаётся непростреливаемой и не попадает в авто-лок игрока (как торговец). См. шапку класса.

	// КВЕСТ 1 (DRAFT, замысел Рината): убить волков у Логова, собрать ШКУРЫ и принести старосте.
	// Тип Collect: завершённость считается по ITEM-цели (шкуры в рюкзаке); kill-цели нет
	// (TargetCount=0) — шкуры падают с волков, прогресс идёт по собранным шкурам. При сдаче
	// шкуры ИЗЫМАЮТСЯ (UQuestComponent::TurnInQuest). Награда 150 монет (DRAFT).
	// A1: число шкур 5 -> 3 (баланс демки).
	OfferedQuest.QuestId = FName(TEXT("KillWolves"));
	OfferedQuest.Title = TEXT("Шкуры волков");
	OfferedQuest.Description = TEXT("Волки одолели деревню. Перебей стаю у логова на севере и принеси мне три волчьих шкуры. Награда: 150 монет.");
	OfferedQuest.Type = EQuestType::Collect;
	OfferedQuest.KillTargetTag = NAME_None;
	OfferedQuest.TargetCount = 0;                 // kill-цели нет: гейт — по шкурам
	OfferedQuest.RequiredItemName = TEXT("Шкура волка"); // имя предмета совпадает с дропом волка (WolfCharacter)
	OfferedQuest.RequiredItemCount = 3;
	OfferedQuest.RewardMoney = 150.0f;
	OfferedQuest.State = EQuestState::NotStarted;

	// КВЕСТ 2 (DRAFT): зачистить базу бандитов на юге (убить 3 бандитов) и принести Ноутбук.
	// Тип Deliver: завершённость = KILL-цель (3 бандита) И ITEM-цель (1 Ноутбук в рюкзаке).
	// Ноутбук изымается при сдаче. Награда 250 монет (DRAFT — больше за более тяжёлый квест).
	SecondQuest.QuestId = FName(TEXT("ClearBanditBase"));
	SecondQuest.Title = TEXT("Зачистить базу бандитов");
	SecondQuest.Description = TEXT("Бандиты засели на базе к югу от деревни. Перебей их (троих) и забери ноутбук - принеси его мне. Награда: 250 монет.");
	SecondQuest.Type = EQuestType::Deliver;
	SecondQuest.KillTargetTag = FName(TEXT("Bandit"));
	SecondQuest.TargetCount = 3;
	SecondQuest.RequiredItemName = TEXT("Ноутбук"); // имя предмета совпадает со спавном ноутбука (AMasterEnemyBase, BP_BanditBase)
	SecondQuest.RequiredItemCount = 1;
	SecondQuest.RewardMoney = 250.0f;
	SecondQuest.State = EQuestState::NotStarted;
}

void AElderNPC::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Torso/Legs следуют за позой Head (корневой скелет мастер-базы) через Leader Pose —
	// тот же механизм, что у AMasterTrader: к этому моменту все компоненты (включая дефолты
	// BP_Elder) сконструированы. AnimBP на Head назначает оператор в BP.
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
}

const FQuest& AElderNPC::GetQuestForPlayer(const UQuestComponent* PlayerQuests) const
{
	// Выдача по порядку: кв.2 становится актуальным только после сдачи кв.1 (TurnedIn).
	if (PlayerQuests)
	{
		const FQuest* Q1 = PlayerQuests->FindQuest(OfferedQuest.QuestId);
		if (Q1 && Q1->State == EQuestState::TurnedIn)
		{
			return SecondQuest;
		}
	}
	return OfferedQuest;
}

void AElderNPC::OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->SetNearbyElder(this);
		UE_LOG(LogTemp, Log, TEXT("Elder '%s': player in range (press Interact to talk)"), *GetName());
	}
}

void AElderNPC::OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController()))
	{
		PC->ClearNearbyElder(this);
	}
}
