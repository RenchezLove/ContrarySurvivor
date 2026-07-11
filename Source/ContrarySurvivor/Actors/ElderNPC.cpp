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
	OfferedQuest.Description = TEXT("Волки одолели деревню. Перебей стаю у логова на западе и принеси мне три волчьих шкуры. Награда: 150 монет.");
	OfferedQuest.Type = EQuestType::Collect;
	OfferedQuest.KillTargetTag = NAME_None;
	OfferedQuest.TargetCount = 0;                 // kill-цели нет: гейт — по шкурам
	OfferedQuest.RequiredItemName = TEXT("Шкура волка"); // имя предмета совпадает с дропом волка (WolfCharacter)
	OfferedQuest.RequiredItemCount = 3;
	OfferedQuest.ItemObjectiveLabel = TEXT("Собрать шкуры волков"); // текст метки на карте (с прогрессом x/3)
	OfferedQuest.RewardMoney = 150.0f;
	OfferedQuest.State = EQuestState::NotStarted;
	// Этап D: метка цели квеста на HUD — логово волков (BP_WolfDen несёт QuestMarkerTag="WolfDen").
	OfferedQuest.MapMarkerTag = FName(TEXT("WolfDen"));

	// КВЕСТ 2 (DRAFT): зачистить базу бандитов на севере (убить 3 бандитов) и принести Ноутбук.
	// Стороны света — по камере игрока (верх кадра при спауне ГГ = север; конвенция Рината 07-02).
	// Тип Deliver: завершённость = KILL-цель (3 бандита) И ITEM-цель (1 Ноутбук в рюкзаке).
	// Ноутбук изымается при сдаче. Награда 250 монет (DRAFT — больше за более тяжёлый квест).
	SecondQuest.QuestId = FName(TEXT("ClearBanditBase"));
	SecondQuest.Title = TEXT("Зачистить базу бандитов");
	SecondQuest.Description = TEXT("Бандиты засели на базе к северу от деревни. Перебей их (троих) и забери ноутбук - принеси его мне. Награда: 250 монет.");
	SecondQuest.Type = EQuestType::Deliver;
	SecondQuest.KillTargetTag = FName(TEXT("Bandit"));
	SecondQuest.TargetCount = 3;
	SecondQuest.KillObjectiveLabel = TEXT("Перебить бандитов"); // текст метки на карте (с прогрессом x/3)
	SecondQuest.RequiredItemName = TEXT("Ноутбук"); // имя предмета совпадает со спавном ноутбука (AMasterEnemyBase, BP_BanditBase)
	SecondQuest.RequiredItemCount = 1;
	SecondQuest.ItemObjectiveLabel = TEXT("Забрать ноутбук"); // текст метки, когда бандиты перебиты, а ноутбук ещё не взят
	SecondQuest.RewardMoney = 250.0f;
	SecondQuest.State = EQuestState::NotStarted;
	// Этап D: метка цели квеста — база бандитов (BP_BanditBase несёт QuestMarkerTag="BanditBase").
	SecondQuest.MapMarkerTag = FName(TEXT("BanditBase"));

	// КВЕСТ 3 (Этап F, ADR-044 п.1-2): крючок сюжета подаётся ЭТИМ диалогом после сдачи ноутбука
	// (кв.2): староста прямо говорит, что на ноутбуке — данные о тех, кто охотится за героем, и
	// даёт стороннюю активность на время «расследования». Текст Description утверждён game-lead.
	// Collect: 3 «Шкуры волка» (волки появились в НОВОМ месте — второе логово), награда 100 (DRAFT).
	ThirdQuest.QuestId = FName(TEXT("HidesForTrader"));
	ThirdQuest.Title = TEXT("Шкуры для торговца");
	ThirdQuest.Description = TEXT("Дай мне время покопаться в ноутбуке. Одно ясно уже сейчас: на нём — данные о тех, кто за тобой охотится. Узнаю, кто именно — расскажу. А пока не сиди без дела: волков снова видели в округе, торговец хорошо платит за шкуры. Принеси три — я передам.");
	ThirdQuest.Type = EQuestType::Collect;
	ThirdQuest.KillTargetTag = NAME_None;
	ThirdQuest.TargetCount = 0;                  // kill-цели нет: гейт — по шкурам (как кв.1)
	ThirdQuest.RequiredItemName = TEXT("Шкура волка"); // имя совпадает с дропом волка (WolfCharacter)
	ThirdQuest.RequiredItemCount = 3;
	ThirdQuest.ItemObjectiveLabel = TEXT("Добыть волчьи шкуры");
	ThirdQuest.RewardMoney = 100.0f;
	ThirdQuest.State = EQuestState::NotStarted;
	// Второе логово волков: BP_WolfDen с QuestMarkerTag="WolfDen2" ставит на карту game-lead.
	// Пока актора с тегом нет, DrawQuestTargetMarker мягко ничего не рисует (проверено кодом HUD).
	ThirdQuest.MapMarkerTag = FName(TEXT("WolfDen2"));
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
	// Выдача по порядку: кв.2 — после сдачи кв.1, кв.3 (Этап F) — после сдачи кв.2 (TurnedIn).
	if (PlayerQuests)
	{
		const FQuest* Q1 = PlayerQuests->FindQuest(OfferedQuest.QuestId);
		if (Q1 && Q1->State == EQuestState::TurnedIn)
		{
			const FQuest* Q2 = PlayerQuests->FindQuest(SecondQuest.QuestId);
			if (Q2 && Q2->State == EQuestState::TurnedIn)
			{
				return ThirdQuest;
			}
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
