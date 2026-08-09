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

	// Староста неуязвим (замысел «как торговец» — см. комментарий ниже про ECC_Visibility;
	// дефект 08-08: три удара ножом убивали старосту, диалог умирал, игрок проходил сквозь).
	bImmuneToDamage = true;

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
	// шкуры ИЗЫМАЮТСЯ (UQuestComponent::TurnInQuest). Награда — см. ADR-065 ниже.
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
	// ADR-065 (решение Рината, числа посчитаны game-lead по реальному прайсу торговца):
	// награда 150 -> 200. Замысел — «первое оружие покупает сам игрок», поэтому награда
	// обязана покрыть не только пистолет, но и припасы, иначе игрок уходит в лес голым.
	// Арифметика: стартовые 50 + награда 200 = 250; пистолет 150 оставляет 100; бинт 12 +
	// консервы 12 + вода 5 + 20 патронов по 2 = 69, свободных 31 (плюс 15 за необязательную
	// четвёртую шкуру). Цену пистолета и награду второго квеста НЕ трогаем.
	OfferedQuest.RewardMoney = 200.0f;
	OfferedQuest.State = EQuestState::NotStarted;
	// Этап D: метка цели квеста на HUD — логово волков (BP_WolfDen несёт QuestMarkerTag="WolfDen").
	OfferedQuest.MapMarkerTag = FName(TEXT("WolfDen"));
	// Build 1 (решение Рината 07-24): кнопки диалога — ответы героя, не служебные подписи.
	// В интро кв.1 принимается своей репликой («Взяться за дело»), эти — для обычного потока
	// (повторный заход, когда интро уже показано). Черновики, утверждает Ринат.
	OfferedQuest.AcceptReplyText = NSLOCTEXT("Quest", "Q1AcceptReply", "Хорошо, берусь.");
	OfferedQuest.TurnInReplyText = NSLOCTEXT("Quest", "Q1TurnInReply", "Готово. Вот, что ты просил.");
	OfferedQuest.CloseReplyText  = NSLOCTEXT("Quest", "Q1CloseReply", "Мне пора.");

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
	// Build 1: ответы героя на кнопках (см. кв.1). Черновики, утверждает Ринат.
	SecondQuest.AcceptReplyText = NSLOCTEXT("Quest", "Q2AcceptReply", "Хорошо, берусь.");
	SecondQuest.TurnInReplyText = NSLOCTEXT("Quest", "Q2TurnInReply", "Готово. Вот, что ты просил.");
	SecondQuest.CloseReplyText  = NSLOCTEXT("Quest", "Q2CloseReply", "Мне пора.");

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
	// Намёк про пистолет (Build 1.2.2, дополнение Рината 05-08). Нужен потому, что с этой
	// волны новый игрок начинает БЕЗ огнестрела, с одним ножом, и должен понимать, откуда
	// берётся ствол. Сказано голосом старосты, а не инструкцией: нож против волка плох, у
	// торговца ствол есть, дело за деньгами. Ключ НОВЫЙ (ElderIntro3b), соседние реплики не
	// перенумерованы — иначе разошлись бы уже собранные переводы.
	IntroLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderIntro3b",
			"Только на одном ноже далеко не уедешь — волка ножом брать это не охота, а драка.\n"
			"У торговца под навесом пистолет с прошлого завоза лежит, никто в деревне такую цену не потянул. Продай ему шкуры, накопи — и ствол твой."),
		NSLOCTEXT("Dialog", "ElderIntroBtn3b", "Учту."),
		EElderIntroAction::None));
	IntroLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderIntro4",
			"И вот что странно… За последние недели чужаки всё идут и идут к нам. Будто гонит их что-то. Или кто-то. Не моего ума дело. Ступай."),
		NSLOCTEXT("Dialog", "ElderIntroBtn4", "Закрыть"),
		EElderIntroAction::None)); // последняя реплика-крючок: показывается один раз (bElderHookShown)

	// СЦЕНКА ПОСЛЕ СДАЧИ НОУТБУКА (Build 1.2, формулировка Рината 07-31). Реплика 1 несёт
	// сюжетный крючок ADR-044 («данные о тех, кто охотится») — СОХРАНЕНА. Реплики 2-3 — НЕ новый
	// квест, а сообщение о способе заработка: пока староста копается в ноутбуке, игрок может
	// охотиться на волков (снова видели к югу) и сдавать шкуры торговцу (продажа шкур уже
	// работает через магазин); у торговца новый завоз — новая броня сейчас, оружие скоро.
	// Черновики, утверждает Ринат. Показывается один раз (bElderNotebookHintShown);
	// Action у этих реплик не действует.
	NotebookHintLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderNotebookHint0",
			"Дай мне время покопаться в ноутбуке. Одно ясно уже сейчас: на нём — данные о тех, кто за тобой охотится. Узнаю, кто именно — расскажу."),
		NSLOCTEXT("Dialog", "ElderNotebookHintBtn0", "Буду ждать."),
		EElderIntroAction::None));
	NotebookHintLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderNotebookHint1",
			"А пока я копаюсь в этой машине, без дела сидеть не обязательно. К югу от деревни снова видели волков — поохоться, добудь шкур. Для жителей волки — беда, а торговец за шкуры хорошо платит."),
		NSLOCTEXT("Dialog", "ElderNotebookHintBtn1", "Займусь."),
		EElderIntroAction::None));
	NotebookHintLines.Add(MakeLine(
		NSLOCTEXT("Dialog", "ElderNotebookHint2",
			"И к торговцу загляни. Ему недавно пришёл новый завоз: появилась добротная броня, а скоро, обещает, будет и оружие. Сдашь шкуры — как раз будет на что справить снаряжение."),
		NSLOCTEXT("Dialog", "ElderNotebookHintBtn2", "Понял. Загляну на юг."),
		EElderIntroAction::None));
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
	// Выдача по порядку: кв.2 — после сдачи кв.1 (TurnedIn). Кв.2 — последний: после его сдачи
	// формальных квестов больше нет (Build 1: бывший кв.3 заменён сценкой-намёком
	// NotebookHintLines, её ведёт панель диалога).
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

bool AElderNPC::ShouldUseFirstQuestCompletedText(const FName& QuestId, const FName& FirstQuestId,
	bool bFirstQuestTextIsSet)
{
	// Совет про торговца уместен ровно один раз — когда староста платит за ПЕРВОЕ задание.
	// При сдаче второго квеста игрок давно у торговца был, и повтор звучал бы глупо.
	return bFirstQuestTextIsSet && !QuestId.IsNone() && QuestId == FirstQuestId;
}

const FText& AElderNPC::GetCompletedTextForQuest(const FName& QuestId) const
{
	return ShouldUseFirstQuestCompletedText(QuestId, OfferedQuest.QuestId, !FirstQuestCompletedText.IsEmpty())
		? FirstQuestCompletedText : DialogueCompletedText;
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
