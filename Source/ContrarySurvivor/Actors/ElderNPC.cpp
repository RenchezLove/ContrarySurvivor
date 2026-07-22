// Fill out your copyright notice in the Description page of Project Settings.

#include "ElderNPC.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h" // GetMesh()/TorsoMesh/LegsMesh (Leader Pose)
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"          // признак «подарок уже выдан»
#include "ContrarySurvivor/Retention/OnboardingComponent.h"  // тост «Получено: Бинт»

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
	OfferedQuest.Title = NSLOCTEXT("Quest", "Q1Title", "Шкуры волков");
	// Build 1: сама речь первой встречи (приветствие, аптечка, крючок) переехала в скриптовое
	// интро IntroLines (реплики по очереди, чинит «свалку реплик» — баг издателя). Описание
	// квеста теперь — ТОЛЬКО задача (как у кв.2/кв.3): его читают в журнале/на HUD, не в интро.
	OfferedQuest.Description = NSLOCTEXT("Quest", "Q1Description",
		"Волки совсем осмелели, у самой околицы рыщут. Разори логово на западе, принеси три шкуры.");
	OfferedQuest.Type = EQuestType::Collect;
	OfferedQuest.KillTargetTag = NAME_None;
	OfferedQuest.TargetCount = 0;                 // kill-цели нет: гейт — по шкурам
	OfferedQuest.RequiredItemName = TEXT("Шкура волка"); // имя предмета совпадает с дропом волка (WolfCharacter)
	OfferedQuest.RequiredItemCount = 3;
	OfferedQuest.ItemObjectiveLabel = NSLOCTEXT("Quest", "Q1ItemObjective", "Собрать шкуры волков"); // текст метки на карте (с прогрессом x/3)
	OfferedQuest.RewardMoney = 150.0f;
	OfferedQuest.State = EQuestState::NotStarted;
	// Этап D: метка цели квеста на HUD — логово волков (BP_WolfDen несёт QuestMarkerTag="WolfDen").
	OfferedQuest.MapMarkerTag = FName(TEXT("WolfDen"));

	// КВЕСТ 2 (DRAFT): зачистить базу бандитов на севере (убить 3 бандитов) и принести Ноутбук.
	// Стороны света — по камере игрока (верх кадра при спауне ГГ = север; конвенция Рината 07-02).
	// Тип Deliver: завершённость = KILL-цель (3 бандита) И ITEM-цель (1 Ноутбук в рюкзаке).
	// Ноутбук изымается при сдаче. Награда 250 монет (DRAFT — больше за более тяжёлый квест).
	SecondQuest.QuestId = FName(TEXT("ClearBanditBase"));
	SecondQuest.Title = NSLOCTEXT("Quest", "Q2Title", "Зачистить базу бандитов");
	// Сумма награды из описания УБРАНА намеренно: её дописывает панель диалога из поля
	// RewardMoney (иначе при смене баланса текст соврал бы), и вписанная руками строка
	// показывалась бы второй раз.
	SecondQuest.Description = NSLOCTEXT("Quest", "Q2Description", "Бандиты засели на базе к северу от деревни. Перебей их (троих) и забери ноутбук - принеси его мне.");
	SecondQuest.Type = EQuestType::Deliver;
	SecondQuest.KillTargetTag = FName(TEXT("Bandit"));
	SecondQuest.TargetCount = 3;
	SecondQuest.KillObjectiveLabel = NSLOCTEXT("Quest", "Q2KillObjective", "Перебить бандитов"); // текст метки на карте (с прогрессом x/3)
	SecondQuest.RequiredItemName = TEXT("Ноутбук"); // имя предмета совпадает со спавном ноутбука (AMasterEnemyBase, BP_BanditBase)
	SecondQuest.RequiredItemCount = 1;
	SecondQuest.ItemObjectiveLabel = NSLOCTEXT("Quest", "Q2ItemObjective", "Забрать ноутбук"); // текст метки, когда бандиты перебиты, а ноутбук ещё не взят
	SecondQuest.RewardMoney = 250.0f;
	SecondQuest.State = EQuestState::NotStarted;
	// Этап D: метка цели квеста — база бандитов (BP_BanditBase несёт QuestMarkerTag="BanditBase").
	SecondQuest.MapMarkerTag = FName(TEXT("BanditBase"));

	// КВЕСТ 3 (Этап F, ADR-044 п.1-2): крючок сюжета подаётся ЭТИМ диалогом после сдачи ноутбука
	// (кв.2): староста прямо говорит, что на ноутбуке — данные о тех, кто охотится за героем, и
	// даёт стороннюю активность на время «расследования». Тон реплики — мягкое ПРЕДЛОЖЕНИЕ, не
	// приказ (фидбек Рината 07-12: староста предлагает занятие «если хочешь», не командует).
	// Collect: 3 «Шкуры волка» (волки появились в НОВОМ месте — второе логово), награда 100 (DRAFT).
	ThirdQuest.QuestId = FName(TEXT("HidesForTrader"));
	ThirdQuest.Title = NSLOCTEXT("Quest", "Q3Title", "Шкуры для торговца");
	ThirdQuest.Description = NSLOCTEXT("Quest", "Q3Description", "Дай мне время покопаться в ноутбуке. Одно ясно уже сейчас: на нём — данные о тех, кто за тобой охотится. Узнаю, кто именно — расскажу. А пока, если хочешь, есть дело: волков снова видели в округе, а торговец хорошо платит за шкуры. Можешь принести мне три шкуры — я передам ему.");
	ThirdQuest.Type = EQuestType::Collect;
	ThirdQuest.KillTargetTag = NAME_None;
	ThirdQuest.TargetCount = 0;                  // kill-цели нет: гейт — по шкурам (как кв.1)
	ThirdQuest.RequiredItemName = TEXT("Шкура волка"); // имя совпадает с дропом волка (WolfCharacter)
	ThirdQuest.RequiredItemCount = 3;
	ThirdQuest.ItemObjectiveLabel = NSLOCTEXT("Quest", "Q3ItemObjective", "Добыть волчьи шкуры");
	ThirdQuest.RewardMoney = 100.0f;
	ThirdQuest.State = EQuestState::NotStarted;
	// Второе логово волков: BP_WolfDen с QuestMarkerTag="WolfDen2" ставит на карту game-lead.
	// Пока актора с тегом нет, DrawQuestTargetMarker мягко ничего не рисует (проверено кодом HUD).
	ThirdQuest.MapMarkerTag = FName(TEXT("WolfDen2"));

	// СКРИПТОВОЕ ИНТРО ПЕРВОЙ ВСТРЕЧИ (Build 1, ТЗ издателя раздел 3 — текст ДОСЛОВНО).
	// Реплики идут ПО ОЧЕРЕДИ, под каждой ровно ОДНА кнопка-ответ игрока (ветвления нет — решение
	// Рината: всегда один вариант). Ветку выбора «Из столицы» / «Сам не помню» издатель отложил;
	// оставлен ответ «Из столицы» — он согласован с интро-текстом («Столица осталась позади»).
	// Эффекты (аптечка, старт квеста) срабатывают по нажатию кнопки соответствующей реплики.
	auto MakeLine = [](const FText& NPC, const FText& Btn, EElderIntroAction Act)
	{
		FElderIntroLine Line;
		Line.NPCText = NPC;
		Line.ButtonLabel = Btn;
		Line.Action = Act;
		return Line;
	};
	IntroLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderIntro0", "Живой. В наших краях уже редкость. Откуда бредёшь такой?"),
		NSLOCTEXT("Dialog", "ElderIntroBtn0", "Из столицы."),
		EElderIntroAction::None));
	IntroLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderIntro1", "Столица… Оттуда давно одни дурные вести доходят."),
		NSLOCTEXT("Dialog", "ElderIntroBtn1", "Дальше"),
		EElderIntroAction::None));
	IntroLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderIntro2", "Ладно. Держи, затяни раны — до утра иначе не дотянешь."),
		NSLOCTEXT("Dialog", "ElderIntroBtn2", "Взять аптечку"),
		EElderIntroAction::GiveGift)); // выдаёт аптечку тем же путём, что магазин (GiveConsumableToBackpack)
	IntroLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderIntro3",
			"Только вот что. Времена нынче лихие, чужаков у нас не привечают. Хочешь остаться — покажи, чего стоишь.\n"
			"Волки совсем осмелели, у самой околицы рыщут. Разори логово на западе, принеси три шкуры. Тогда и поговорим по-людски."),
		NSLOCTEXT("Dialog", "ElderIntroBtn3", "Взяться за дело"),
		EElderIntroAction::StartQuest)); // запускает существующий квест «Шкуры волков»
	IntroLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderIntro4",
			"И вот что странно… За последние недели чужаки всё идут и идут к нам. Будто гонит их что-то. Или кто-то. Не моего ума дело. Ступай."),
		NSLOCTEXT("Dialog", "ElderIntroBtn4", "Закрыть"),
		EElderIntroAction::None)); // последняя реплика-крючок: показывается один раз (bElderHookShown)
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

bool AElderNPC::TryGiveFirstMeetingGift(APlayerCharacter* Player)
{
	if (!Player || FirstGiftCount <= 0)
	{
		return false;
	}

	// Признак «уже выдал» — в сейве игрока (см. комментарий к методу в заголовке).
	UContrarySaveGame* Save = Player->LoadOrCreateSaveObject();
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("Elder '%s': подарок не выдан — сохранение не открылось."), *GetName());
		return false;
	}
	if (Save->bElderFirstGiftGiven)
	{
		// Самая частая причина «бинт не приходит»: подарок уже выдавался в этом профиле
		// (признак живёт в сохранении и нигде не сбрасывается). Это не поломка, а замысел
		// «один раз за профиль» — но со стороны выглядит как молчащий механизм.
		UE_LOG(LogTemp, Log, TEXT("Elder '%s': подарок не выдан — в этом профиле уже выдавался."), *GetName());
		return false;
	}

	// ПРИВЯЗКА К СОСТОЯНИЮ КВЕСТА УБРАНА (баг владельца 2026-07-20: бинт не приходил).
	// Раньше подарок выдавался только пока кв.1 не взят — задумка была «дать вместе с
	// приветственной репликой». Но у любого игрока с прогрессом квест уже взят или сдан,
	// гейт закрыт, и механизм молчал, хотя вызывался исправно. Теперь условие ровно одно:
	// не выдавали в этом профиле. Это и проще, и ближе к исходному замыслу «один раз при
	// первой встрече» — признак в сохранении сам по себе означает «первая встреча прошла».
	const int32 Given = Player->GiveConsumableToBackpack(FirstGiftConsumableType, FirstGiftCount);
	if (Given <= 0)
	{
		// Рюкзак предмет не принял — признак НЕ ставим, попробуем в следующий раз.
		UE_LOG(LogTemp, Warning, TEXT("Elder '%s': first meeting gift NOT given (backpack refused item)"), *GetName());
		return false;
	}

	Save->bElderFirstGiftGiven = true;
	Player->WriteSaveObject(Save);

	// Сообщаем игроку тем же тостом, что и подсказки онбординга: без этого предмет
	// появляется в рюкзаке молча и игрок его не замечает.
	if (!FirstGiftHintFormat.IsEmpty())
	{
		if (UOnboardingComponent* Hints = Player->GetOnboarding())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Item"), AConsumableItem::GetDefaultDisplayText(FirstGiftConsumableType));
			Args.Add(TEXT("Count"), FText::AsNumber(Given));
			Hints->ShowTransientHint(FText::Format(FirstGiftHintFormat, Args));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Elder '%s': first meeting gift given (%d item(s))"), *GetName(), Given);
	return true;
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
