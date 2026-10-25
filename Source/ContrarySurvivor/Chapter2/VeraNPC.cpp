// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/VeraNPC.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

AVeraNPC::AVeraNPC()
{
	NpcTag = Chapter2Ids::VeraTag;
	// [210], [211]
	DialogueDisplayName = NSLOCTEXT("Chapter2", "VeraName", "ВЕРА");
	NPCMarkerLabel = NSLOCTEXT("Chapter2", "VeraMarker", "Вера");
	InteractPromptAction = NSLOCTEXT("Chapter2", "VeraPrompt", "Поговорить с Верой");

	// До сдачи ноутбука своей реплики у Веры нет — та же занятость, что у учёных ([035]).
	IdleText = NSLOCTEXT("Chapter2", "VeraIdle",
		"Простите, я занята: приборы с самого утра показывают что-то странное. Если у вас нет ко мне дела, не отвлекайте меня.");

	// [212], [213]
	DealText = NSLOCTEXT("Chapter2", "VeraDeal",
		"Так это вы принесли ноутбук. Я Вера, руковожу этим лагерем. Защиту на нём ставили серьёзные люди, но мы её вскроем.\n"
		"Только у меня к вам встречная просьба: людей мало, а работы много. Поможете нам — а мы за это расшифруем ноутбук. Договорились?");
	DealReplyText = NSLOCTEXT("Chapter2", "VeraDealReply", "Договорились.");

	// [227]
	ActiveText = NSLOCTEXT("Chapter2", "VeraActive",
		"Образцы в низине, к востоку от лагеря. Если воздух кончится — возвращайтесь в лагерь, мы заправим баллон.");

	// ЧЕРНОВИК, в РЕПЛИКИ.txt такой нет: образцы собраны, а наёмник на дороге жив.
	WaitMercenaryText = NSLOCTEXT("Chapter2", "VeraWaitMercenary",
		"Образцы у вас? Хорошо. Но мне передали, что на дороге из низины вас кто-то ждёт. Будьте осторожны.");

	// [228], [250], [251], [252]
	SamplesText = NSLOCTEXT("Chapter2", "VeraSamples",
		"Три образца… Отлично. А пока вы ходили, мы вскрыли ноутбук. Бандиты возили для Куратора ящики. В ящиках были такие же образцы, как эти.\n"
		"Кому-то очень нужно то, что растёт в аномалиях. И, кажется, вы как-то с этим связаны.");
	LastFileText = NSLOCTEXT("Chapter2", "VeraLastFile",
		"Значит, они знают вас лучше, чем вы сами. Тогда вам нужно это увидеть: мы вскрыли последний файл ноутбука.");
	LastFileReplyText = NSLOCTEXT("Chapter2", "VeraLastFileReply", "Это же я. А рядом… ты?");
	ChapterEndText = NSLOCTEXT("Chapter2", "VeraChapterEnd",
		"Да. Мы были знакомы — раньше, до того как вы всё забыли. Я надеялась, что вы никогда этого не увидите.\n"
		"Не сейчас. Отдохните, а завтра я расскажу всё, что смогу.");
	AfterText = NSLOCTEXT("Chapter2", "VeraAfter", "Не сейчас. Отдохните, а завтра я расскажу всё, что смогу.");

	// Задание «Образцы из аномалий»: три образца — счётчик цели (событие «образец подобран»).
	SamplesQuest.QuestId = Chapter2Ids::SamplesQuest;
	SamplesQuest.Title = NSLOCTEXT("Quest", "SamplesTitle", "Образцы из аномалий");
	// [214]
	SamplesQuest.Description = NSLOCTEXT("Quest", "SamplesDescription",
		"К востоку от лагеря есть отравленная низина: там когда-то разлилась какая-то химия, и с тех пор в ней растут аномальные образования. Нам нужны три образца.\n"
		"Дышать там нельзя, поэтому возьмите дыхательный аппарат — воздуха в баллоне хватит ненадолго, следите за ним. И вот детектор: чем ближе образец, тем чаще он пищит.");
	SamplesQuest.Type = EQuestType::Collect;
	SamplesQuest.Kind = EQuestKind::Main; // сюжет главы 2
	SamplesQuest.KillTargetTag = Chapter2Ids::SampleObjectiveTag;
	SamplesQuest.TargetCount = 3;
	SamplesQuest.KillObjectiveLabel = NSLOCTEXT("Quest", "SamplesObjectiveLabel", "Найти в отравленной низине образцы");
	// [216]
	SamplesQuest.ObjectiveFormatOverride = NSLOCTEXT("Quest", "SamplesObjective", "Найти в отравленной низине образцы ({Done} из {Total})");
	// [217]
	SamplesQuest.TurnInHintText = NSLOCTEXT("Quest", "SamplesTurnInHint", "Отнести образцы Вере");
	SamplesQuest.MapMarkerTag = Chapter2Ids::LowlandMarkerTag;
	SamplesQuest.RequiredItemCount = 0;
	SamplesQuest.RewardMoney = 0.0f; // плата — расшифровка ноутбука
	// [215], [229]
	SamplesQuest.AcceptReplyText = NSLOCTEXT("Quest", "SamplesAccept", "Принесу.");
	SamplesQuest.TurnInReplyText = NSLOCTEXT("Quest", "SamplesTurnIn", "По дороге сюда меня ждал человек Куратора. Он знал меня.");
	SamplesQuest.GiverNpcTag = Chapter2Ids::VeraTag;
	SamplesQuest.TurnInNpcTag = Chapter2Ids::VeraTag;
}

bool AVeraNPC::IsLaptopTurnedIn(const APlayerCharacter* Player)
{
	const UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	return Quests && Quests->IsQuestTurnedIn(ContraryQuestIds::DeliverLaptopToScientists);
}

EVeraTalkStage AVeraNPC::ComputeOpeningStage(bool bLaptopTurnedIn, const FQuest* InSamplesQuest,
	bool bMercenaryDefeated, bool bChapterEndShown)
{
	if (!bLaptopTurnedIn)
	{
		return EVeraTalkStage::Idle;
	}
	if (!InSamplesQuest || InSamplesQuest->State == EQuestState::NotStarted)
	{
		return EVeraTalkStage::Deal;
	}
	switch (InSamplesQuest->State)
	{
		case EQuestState::Active:
			return EVeraTalkStage::Active;
		case EQuestState::Completed:
			return bMercenaryDefeated ? EVeraTalkStage::Samples : EVeraTalkStage::WaitMerc;
		case EQuestState::TurnedIn:
			return bChapterEndShown ? EVeraTalkStage::After : EVeraTalkStage::LastFile;
		default:
			return EVeraTalkStage::Deal;
	}
}

void AVeraNPC::OnTalkOpened(APlayerCharacter* Player)
{
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UChapter2Component* Chapter2 = Player ? Player->GetChapter2() : nullptr;
	const bool bLaptop = IsLaptopTurnedIn(Player);

	// Задание попадает в журнал при первом разговоре после сдачи ноутбука (повтор — без изменений).
	FQuest Definition = SamplesQuest;
	Definition.GiverNpcTag = NpcTag;
	Definition.TurnInNpcTag = NpcTag;
	if (bLaptop && Quests)
	{
		Quests->OfferQuest(Definition);
	}

	Stage = ComputeOpeningStage(bLaptop, Quests ? Quests->FindQuest(SamplesQuest.QuestId) : nullptr,
		Chapter2 && Chapter2->IsMercenaryDefeated(), Chapter2 && Chapter2->IsChapterEndShown());
	if (Stage == EVeraTalkStage::WaitMerc && WaitMercenaryText.IsEmpty())
	{
		Stage = EVeraTalkStage::Samples;
	}

	if (Chapter2)
	{
		// На уровне нет зоны заправки — баллон заправляет сама Вера.
		if (!Chapter2->HasRefillZoneInWorld())
		{
			Chapter2->RefillAir(/*bAnnounce=*/true);
		}
		Chapter2->SetPhotoShown(Stage == EVeraTalkStage::LastFile);
	}
}

void AVeraNPC::OnTalkClosed(APlayerCharacter* Player)
{
	if (UChapter2Component* Chapter2 = Player ? Player->GetChapter2() : nullptr)
	{
		Chapter2->SetPhotoShown(false);
		if (Stage == EVeraTalkStage::ChapterEnd)
		{
			Chapter2->StartChapterEnd(); // экран «Конец второй главы» и запись в сохранение
		}
	}
}

void AVeraNPC::BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const
{
	Super::BuildTalkView(Player, OutView);
	switch (Stage)
	{
		case EVeraTalkStage::Deal:
			OutView.Replica = DealText;
			OutView.PrimaryLabel = DealReplyText;
			break;
		case EVeraTalkStage::Offer:
			OutView.Replica = SamplesQuest.Description;
			OutView.PrimaryLabel = SamplesQuest.AcceptReplyText;
			break;
		case EVeraTalkStage::Active:
			OutView.Replica = ActiveText;
			break;
		case EVeraTalkStage::WaitMerc:
			OutView.Replica = WaitMercenaryText;
			break;
		case EVeraTalkStage::Samples:
			OutView.Replica = SamplesText;
			OutView.PrimaryLabel = SamplesQuest.TurnInReplyText;
			break;
		case EVeraTalkStage::LastFile:
			OutView.Replica = LastFileText;
			OutView.PrimaryLabel = LastFileReplyText;
			break;
		case EVeraTalkStage::ChapterEnd:
			OutView.Replica = ChapterEndText;
			break;
		case EVeraTalkStage::After:
			OutView.Replica = AfterText;
			break;
		default:
			break; // Idle — реплика без повода
	}
}

bool AVeraNPC::HandleTalkPrimary(APlayerCharacter* Player)
{
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UChapter2Component* Chapter2 = Player ? Player->GetChapter2() : nullptr;
	if (!Quests)
	{
		return true;
	}
	switch (Stage)
	{
		case EVeraTalkStage::Deal:
			Stage = EVeraTalkStage::Offer;
			return false;
		case EVeraTalkStage::Offer:
			if (Quests->AcceptQuest(SamplesQuest.QuestId))
			{
				// Решение лида 23.10: дыхательный аппарат и детектор выдаются при взятии задания.
				if (Chapter2)
				{
					Chapter2->GiveApparatus();
					Chapter2->RefreshSampleActors();
				}
				UE_LOG(LogQA, Display, TEXT("QA: Вера выдала задание «Образцы из аномалий»"));
			}
			Stage = EVeraTalkStage::Active;
			return false;
		case EVeraTalkStage::Samples:
			if (Quests->TurnInQuest(SamplesQuest.QuestId, NpcTag))
			{
				UE_LOG(LogQA, Display, TEXT("QA: Вера приняла образцы"));
				Stage = EVeraTalkStage::LastFile;
				if (Chapter2)
				{
					Chapter2->SetPhotoShown(true); // фотография из ноутбука на экране
				}
			}
			return false;
		case EVeraTalkStage::LastFile:
			Stage = EVeraTalkStage::ChapterEnd;
			if (Chapter2)
			{
				Chapter2->SetPhotoShown(false);
			}
			return false;
		default:
			return true;
	}
}
