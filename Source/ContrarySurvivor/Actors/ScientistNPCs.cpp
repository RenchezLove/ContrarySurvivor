// Fill out your copyright notice in the Description page of Project Settings.

#include "ScientistNPCs.h"
#include "ContrarySurvivor/Actors/QuestBarrier.h" // правило «шлагбаум открыт» — одно на шлагбаум и охранника
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"          // плата за просмотр зеркалится в сохранение
#include "ContrarySurvivor/Ads/AdService.h"                  // слой рекламы: показ с наградой
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"   // события точки показа
#include "ContrarySurvivor/ContrarySurvivor.h"               // LogQA
#include "AAmmoItem.h"                                       // имя строки патронов 12-го калибра

namespace
{
	// Имя точки показа в событиях аналитики (ad:scientist_helmet:...). Совпадает с местом показа.
	const TCHAR* HelmetAdPoint = TEXT("scientist_helmet");

	// Строки таблицы предметов, на которые ссылаются задания и прайс-лист базы.
	const FName ShotgunRow(TEXT("shotgun_toz34"));
	const FName GuardRadioRow(TEXT("guard_radio"));
}

// ---------------------------------------------------------------------------
// Охранник базы
// ---------------------------------------------------------------------------

AScientistGuardNPC::AScientistGuardNPC()
{
	// Вступление, пока героя на базу не пускают. Черновик, утверждает Ринат.
	NotVouchedText = NSLOCTEXT("Scientists", "GuardNotVouched",
		"Стой. Дальше находится база учёных. Староста за тебя не поручился, поэтому пропустить тебя я не могу. Хотя… если ты мне поможешь, я тебя пропущу.");
	NpcTag = ScientistBaseTags::Guard;
	DialogueDisplayName = NSLOCTEXT("Scientists", "GuardName", "ОХРАННИК");
	NPCMarkerLabel = NSLOCTEXT("Scientists", "GuardMarker", "Охранник");
	InteractPromptAction = NSLOCTEXT("Scientists", "GuardPrompt", "Поговорить с охранником");
	CloseReplyText = NSLOCTEXT("Scientists", "GuardClose", "Не сейчас.");

	// ЗАДАНИЕ ОХРАННИКА (черновик; слова Рината 10.10: «принести какой-то предмет, который он
	// обронил, удирая от волков. Сам предмет находится неподалёку от логова волков (на юге от
	// деревни)»; предмет — рация, решение лида). Тип «собрать»: готовность считается по рации
	// в сумке, при сдаче рация изымается. Награда — двустволка и четыре патрона, монет нет.
	bOffersQuest = true;
	// Задание лежит в таблице заданий строкой с тем же именем (11.10.2026): тексты правятся
	// там. Значения ниже действуют, только если строки в таблице нет.
	QuestRow = ContraryQuestIds::FetchGuardRadio;
	OfferedQuest.QuestId = ContraryQuestIds::FetchGuardRadio;
	OfferedQuest.Title = NSLOCTEXT("Quest", "GuardRadioTitle", "Рация охранника");
	OfferedQuest.Description = NSLOCTEXT("Quest", "GuardRadioDescription",
		"Стой. Дальше находится база учёных, а я стою здесь на посту.\n"
		"Слушай, раз уж ты пришёл, помоги мне. За мной гнались волки из южного логова, и я на бегу потерял рацию. "
		"Она лежит где-то у логова, к югу от деревни. Сам я уйти с поста не могу, а за рацию учёных отвечаю я. Принеси её — и я отдам тебе двустволку и патроны к ней.");
	OfferedQuest.Type = EQuestType::Collect;
	OfferedQuest.Kind = EQuestKind::Side; // к сюжету старосты не относится (10.10.2026)
	OfferedQuest.KillTargetTag = NAME_None;
	OfferedQuest.TargetCount = 0;
	// Служебный ключ рации = имя её строки в таблице предметов (колонка старого ключа у новых
	// предметов пустая) — по нему же сходится счёт предмета в сумке.
	OfferedQuest.RequiredItemName = GuardRadioRow.ToString();
	OfferedQuest.RequiredItemCount = 1;
	OfferedQuest.ItemObjectiveLabel = NSLOCTEXT("Quest", "GuardRadioObjective", "Найти рацию охранника");
	OfferedQuest.MapMarkerTag = ScientistBaseTags::GuardRadioSpot;
	OfferedQuest.RewardMoney = 0.0f;
	OfferedQuest.TurnInHintText = NSLOCTEXT("Quest", "GuardRadioTurnInHint", "верни рацию охраннику");
	OfferedQuest.AcceptReplyText = NSLOCTEXT("Quest", "GuardRadioAccept", "Поищу твою рацию.");
	OfferedQuest.TurnInReplyText = NSLOCTEXT("Quest", "GuardRadioTurnIn", "Держи свою рацию.");
	OfferedQuest.CloseReplyText = NSLOCTEXT("Quest", "GuardRadioClose", "Не сейчас.");

	FQuestItemGrant Shotgun;
	Shotgun.ItemRow = ShotgunRow;
	Shotgun.Count = 1;
	OfferedQuest.RewardItems.Add(Shotgun);
	FQuestItemGrant Shells;
	Shells.ItemRow = ContraryAmmo::Shotgun12gType();
	Shells.Count = 4;
	OfferedQuest.RewardItems.Add(Shells);
	// Двустволку игроку помогают поставить в слот: кнопка сумки, плитка, кнопка смены оружия.
	OfferedQuest.RewardEquipItemRow = ShotgunRow;

	QuestActiveText = NSLOCTEXT("Scientists", "GuardActive",
		"Рация лежит всё там же, у волчьего логова к югу от деревни. Без неё я здесь как без рук: если что-то случится, я не смогу никого позвать.");
	QuestCompletedText = NSLOCTEXT("Scientists", "GuardCompleted",
		"Нашёл? Давай её сюда. Мы договаривались, так что двустволка твоя.");
	// 11.10.2026 (Ринат: «из диалога с ним не следует, что он вручил нам двустволку. Надо
	// сделать так чтобы он прямо об этом сказал»): реплика сразу после сдачи рации.
	QuestJustTurnedInText = NSLOCTEXT("Scientists", "GuardJustTurnedIn",
		"Вот это дело, рация цела. Держи двустволку и патроны к ней — ты их заслужил. Проходи, на базе тебя ждут.");
	QuestTurnedInText = NSLOCTEXT("Scientists", "GuardTurnedIn",
		"Рация снова у меня, всё в порядке. Проходи. Учёные — люди странные, но тебя они не обидят.");

	// Герой несёт ноутбук учёным (третье задание старосты): пропускает, шлагбаум открыт.
	LaptopPassText = NSLOCTEXT("Scientists", "GuardLaptopPass",
		"Ты с ноутбуком от старосты? Проходи, учёные тебя ждут. Шлагбаум открыт.");
	LaptopPassOfferText = NSLOCTEXT("Scientists", "GuardLaptopPassOffer",
		"Слушай, раз уж ты пришёл, помоги мне. За мной гнались волки из южного логова, и я на бегу потерял рацию. "
		"Она лежит где-то у логова, к югу от деревни. Сам я уйти с поста не могу, а за рацию учёных отвечаю я. Принеси её — и я отдам тебе двустволку и патроны к ней.");
}

bool AScientistGuardNPC::IsCarryingLaptop(const TArray<FQuest>& Journal, FName LaptopQuestId)
{
	return !LaptopQuestId.IsNone() && Journal.ContainsByPredicate([LaptopQuestId](const FQuest& Q)
		{ return Q.QuestId == LaptopQuestId && UQuestComponent::IsInActivePhase(Q); });
}

bool AScientistGuardNPC::ShouldStartWithRefusal(const TArray<FQuest>& Journal, FName LaptopQuestId, FName OwnQuestId)
{
	for (const FQuest& Quest : Journal)
	{
		if (Quest.QuestId == OwnQuestId && Quest.State != EQuestState::NotStarted)
		{
			return false; // задание про рацию уже взято — уговор есть
		}
	}
	return !AQuestBarrier::ShouldBeOpen(Journal, LaptopQuestId, OwnQuestId);
}

void AScientistGuardNPC::BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const
{
	// Кнопки и ветки своего задания — как у любого собеседника с заданием; меняется только реплика.
	Super::BuildTalkView(Player, OutView);

	// Героя на базу ещё не пускают и задание про рацию не взято: сначала отказ («староста за
	// тебя не поручился… хотя, поможешь — пропущу»), затем просьба про рацию. Кнопка взять
	// задание остаётся — её ставит общая ветка выше.
	if (const UQuestComponent* GateQuests = Player ? Player->GetQuests() : nullptr)
	{
		if (OffersQuest() && !NotVouchedText.IsEmpty()
			&& ShouldStartWithRefusal(GateQuests->GetQuests(), LaptopQuestId, OfferedQuest.QuestId))
		{
			const FText Ask = LaptopPassOfferText.IsEmpty() ? OutView.Replica : LaptopPassOfferText;
			OutView.Replica = FText::Format(INVTEXT("{0}\n{1}"), NotVouchedText, Ask);
			return;
		}
	}

	const UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	if (!Quests || LaptopPassText.IsEmpty() || !IsCarryingLaptop(Quests->GetQuests(), LaptopQuestId))
	{
		return;
	}

	FText Rest;
	if (OffersQuest())
	{
		const FQuest* Own = Quests->FindQuest(OfferedQuest.QuestId);
		const EQuestState OwnState = Own ? Own->State : EQuestState::NotStarted;
		if (OwnState == EQuestState::TurnedIn)
		{
			return; // «Рация при мне, порядок. Проходи…» — и так пропускает
		}
		Rest = (OwnState == EQuestState::NotStarted && !LaptopPassOfferText.IsEmpty())
			? LaptopPassOfferText : OutView.Replica;
	}
	OutView.Replica = Rest.IsEmpty() ? LaptopPassText
		: FText::Format(INVTEXT("{0}\n{1}"), LaptopPassText, Rest);
}

// ---------------------------------------------------------------------------
// Квестовый учёный
// ---------------------------------------------------------------------------

AScientistQuestNPC::AScientistQuestNPC()
{
	NpcTag = ScientistBaseTags::QuestScientist;
	DialogueDisplayName = NSLOCTEXT("Scientists", "QuestScientistName", "УЧЁНЫЙ");
	NPCMarkerLabel = NSLOCTEXT("Scientists", "QuestScientistMarker", "Учёный");
	InteractPromptAction = NSLOCTEXT("Scientists", "QuestScientistPrompt", "Поговорить с учёным");

	// До прихода с ноутбуком — короткая нейтральная реплика.
	IdleText = NSLOCTEXT("Scientists", "QuestScientistIdle",
		"Простите, я занят: приборы с самого утра показывают что-то странное. Если у вас нет ко мне дела, не отвлекайте меня.");

	// Приём ноутбука (третье задание старосты сдаётся этому учёному).
	ForeignTurnInText = NSLOCTEXT("Scientists", "QuestScientistReceive",
		"Ноутбук бандитов? Любопытно… Староста открыл только малую часть, и это неудивительно: такую защиту бандиты сами поставить не могли. Давайте его сюда, мы посмотрим.");
	ForeignTurnInReplyText = NSLOCTEXT("Scientists", "QuestScientistReceiveReply", "Вот ноутбук.");
	ForeignTurnInDoneText = NSLOCTEXT("Scientists", "QuestScientistReceived",
		"Благодарю вас. Вот {Reward} монет: нам и самим важно знать, кто здесь хозяйничает.\nРасшифровкой займётся наша руководитель, Вера. Зайдите к ней — она хотела с вами поговорить.");

	// Глава 2 (утверждено Ринатом 23.10.2026): ноутбуком занимается Вера ([038]).
	AfterForeignTurnInText = NSLOCTEXT("Scientists", "QuestScientistLater",
		"Ноутбуком занимается Вера. Если у вас к нам дело, говорите с ней.");

	// 25.10.2026: сюжет продолжается второй главой — сообщение о конце сюжета выходит после
	// последнего разговора с Верой (UChapter2Component::StartChapterEnd), а не здесь.
	bForeignTurnInEndsStory = false;
}

// ---------------------------------------------------------------------------
// Учёный со шлемом (просмотр рекламы за валюту)
// ---------------------------------------------------------------------------

AScientistHelmetNPC::AScientistHelmetNPC()
{
	NpcTag = ScientistBaseTags::HelmetScientist;
	DialogueDisplayName = NSLOCTEXT("Scientists", "HelmetName", "УЧЁНЫЙ");
	NPCMarkerLabel = NSLOCTEXT("Scientists", "HelmetMarker", "Учёный со шлемом");
	InteractPromptAction = NSLOCTEXT("Scientists", "HelmetPrompt", "Поговорить с учёным");

	OfferText = NSLOCTEXT("Scientists", "HelmetOffer",
		"Не хотите помочь науке? Наденьте наш шлем виртуальной реальности и досмотрите запись до конца, а мы снимем показания приборов. За это мы заплатим вам {Reward} монет.");
	AcceptReplyText = NSLOCTEXT("Scientists", "HelmetAccept", "Согласиться");
	DeclineReplyText = NSLOCTEXT("Scientists", "HelmetDecline", "Не сейчас");
	RunningText = NSLOCTEXT("Scientists", "HelmetRunning", "Надевайте шлем и смотрите до конца.");
	PaidText = NSLOCTEXT("Scientists", "HelmetPaid", "Отлично, данные записаны. Вот ваши {Reward} монет.");
	AgainReplyText = NSLOCTEXT("Scientists", "HelmetAgain", "Давайте ещё раз");
	NotFinishedText = NSLOCTEXT("Scientists", "HelmetNotFinished",
		"Вы сняли шлем раньше времени, поэтому запись не засчитана и платить нам не за что.");
	NotReadyText = NSLOCTEXT("Scientists", "HelmetNotReady",
		"Простите, прибор сейчас не готов. Загляните чуть позже.");
	CoolingText = NSLOCTEXT("Scientists", "HelmetCooling",
		"Прибор перегрелся и остывает. Подождите ещё минут {Minutes}.");
}

bool AScientistHelmetNPC::CanStartExperiment(const FDateTime& NowUtc) const
{
	return AdSeries::CanWatch(Series, NowUtc, MaxViewsInRow, static_cast<double>(BreakMinutes) * 60.0);
}

void AScientistHelmetNPC::OnTalkOpened(APlayerCharacter* Player)
{
	// Новый разговор начинается с предложения (незаконченный показ не трогаем).
	if (Stage != EHelmetTalkStage::Running)
	{
		Stage = EHelmetTalkStage::Offer;
	}
}

void AScientistHelmetNPC::BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const
{
	Super::BuildTalkView(Player, OutView);

	const FDateTime NowUtc = FDateTime::UtcNow();
	const double BreakSeconds = static_cast<double>(BreakMinutes) * 60.0;
	const bool bCanWatch = CanStartExperiment(NowUtc);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Reward"), FText::AsNumber(FMath::RoundToInt32(
		Stage == EHelmetTalkStage::Paid ? LastPaid : RewardMoney)));
	// Минуты ожидания — вверх до целой, чтобы «подождите минут 0» не появлялось.
	Args.Add(TEXT("Minutes"), FText::AsNumber(FMath::Max(1, FMath::CeilToInt32(
		AdSeries::BreakSecondsLeft(Series, NowUtc, MaxViewsInRow, BreakSeconds) / 60.0))));

	switch (Stage)
	{
		case EHelmetTalkStage::Running:
			OutView.Replica = RunningText;
			break;
		case EHelmetTalkStage::Paid:
			OutView.Replica = FText::Format(PaidText, Args);
			OutView.PrimaryLabel = bCanWatch ? AgainReplyText : FText::GetEmpty();
			break;
		case EHelmetTalkStage::NotFinished:
			OutView.Replica = NotFinishedText;
			OutView.PrimaryLabel = bCanWatch ? AgainReplyText : FText::GetEmpty();
			break;
		case EHelmetTalkStage::NotReady:
			OutView.Replica = NotReadyText;
			break;
		case EHelmetTalkStage::Offer:
		default:
			if (bCanWatch)
			{
				OutView.Replica = FText::Format(OfferText, Args);
				OutView.PrimaryLabel = AcceptReplyText;
				OutView.CloseLabel = DeclineReplyText;
			}
			else
			{
				OutView.Replica = FText::Format(CoolingText, Args);
			}
			break;
	}
}

bool AScientistHelmetNPC::HandleTalkPrimary(APlayerCharacter* Player)
{
	StartExperiment(Player);
	return false; // окно остаётся: в нём появится итог испытания
}

void AScientistHelmetNPC::StartExperiment(APlayerCharacter* Player)
{
	if (!Player || Stage == EHelmetTalkStage::Running)
	{
		return;
	}
	if (!CanStartExperiment(FDateTime::UtcNow()))
	{
		Stage = EHelmetTalkStage::Offer; // покажет «прибор остывает»
		return;
	}

	UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this);
	IAdService* Ads = AdService::Get(this);
	if (!Ads || !Ads->IsRewardedReadyFor(AdPlacements::ScientistHelmet))
	{
		// Показ не загрузился: вежливый отказ, платы нет.
		Stage = EHelmetTalkStage::NotReady;
		if (Analytics)
		{
			Analytics->RecordAdNotShown(HelmetAdPoint, TEXT("no_ad"));
		}
		UE_LOG(LogQA, Display, TEXT("QA: HELMET-AD not ready - no reward"));
		return;
	}

	if (Analytics)
	{
		Analytics->RecordAdStage(HelmetAdPoint, TEXT("button_clicked"));
		Analytics->RecordAdStage(HelmetAdPoint, TEXT("started"));
	}

	Stage = EHelmetTalkStage::Running;
	ExperimentPlayer = Player;
	UE_LOG(LogQA, Display, TEXT("QA: HELMET-AD started"));
	Ads->ShowRewarded(AdPlacements::ScientistHelmet,
		FSimpleDelegate::CreateUObject(this, &AScientistHelmetNPC::HandleAdSuccess),
		FSimpleDelegate::CreateUObject(this, &AScientistHelmetNPC::HandleAdFail));
}

void AScientistHelmetNPC::HandleAdSuccess()
{
	CompleteExperiment(ExperimentPlayer.Get(), /*bWatched=*/true, FDateTime::UtcNow());
}

void AScientistHelmetNPC::HandleAdFail()
{
	CompleteExperiment(ExperimentPlayer.Get(), /*bWatched=*/false, FDateTime::UtcNow());
}

void AScientistHelmetNPC::CompleteExperiment(APlayerCharacter* Player, bool bWatched, const FDateTime& NowUtc)
{
	ExperimentPlayer.Reset();
	UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this);

	if (!bWatched || !Player || !Player->GetStats())
	{
		Stage = EHelmetTalkStage::NotFinished;
		if (Analytics)
		{
			Analytics->RecordAdStage(HelmetAdPoint, TEXT("dismissed"));
		}
		UE_LOG(LogQA, Display, TEXT("QA: HELMET-AD not finished - no reward"));
		return;
	}

	AdSeries::RegisterView(Series, NowUtc, static_cast<double>(BreakMinutes) * 60.0);

	// Плата — в живые монеты и зеркалом в сохранение, тем же путём, что удвоение ежедневной
	// награды: иначе смерть вернула бы монеты из сохранения и просмотр пропал бы зря.
	LastPaid = RewardMoney;
	Player->GetStats()->AddMoney(RewardMoney);
	if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		if (Save->bHasData)
		{
			Save->Money += RewardMoney;
			Player->WriteSaveObject(Save);
		}
	}

	Stage = EHelmetTalkStage::Paid;
	if (Analytics)
	{
		Analytics->RecordAdStage(HelmetAdPoint, TEXT("completed"), RewardMoney, /*bWithValue=*/true);
	}
	UE_LOG(LogQA, Display, TEXT("QA: HELMET-AD completed - paid %.0f, views in series %d of %d"),
		RewardMoney, Series.ViewsInSeries, MaxViewsInRow);
}

// ---------------------------------------------------------------------------
// Учёный-торговец
// ---------------------------------------------------------------------------

AScientistTrader::AScientistTrader()
{
	NPCMarkerLabel = NSLOCTEXT("Scientists", "TraderMarker", "Учёный-торговец");

	// Свой состав прайс-листа (слова Рината 10.10: двустволка «должна появиться в продаже у
	// учёного-торговца»; расходники — как у деревенского). Цена меньше нуля = из таблицы
	// предметов. Состав и цены правятся на чертеже в поле «Состав прайс-листа».
	CatalogRows.Reset();
	const FName Rows[] = {
		ShotgunRow, ContraryAmmo::Shotgun12gType(),
		FName(TEXT("bandage")), FName(TEXT("water_bottle")), FName(TEXT("canned_food")) };
	for (const FName& RowName : Rows)
	{
		FShopCatalogRef Ref;
		Ref.ItemRow = RowName;
		CatalogRows.Add(Ref);
	}
	RebuildCatalog();
}
