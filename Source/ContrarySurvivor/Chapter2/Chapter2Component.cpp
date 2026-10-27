// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Chapter2/ToxicAnomalyZone.h"
#include "ContrarySurvivor/Chapter2/AnomalyFormation.h"
#include "ContrarySurvivor/Chapter2/AnomalyFormationItem.h"
#include "ContrarySurvivor/Chapter2/VeraNPC.h"
#include "ContrarySurvivor/Chapter2/MercenaryEncounter.h"
#include "ContrarySurvivor/Chapter2/ScientistGearItem.h"
#include "ContrarySurvivor/Chapter2/StoryNoteItem.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h" // метка учёного, передавшего героя Вере
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "UInventoryComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Retention/OnboardingComponent.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"
#include "ContrarySurvivor/Debug/QADebug.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UChapter2Component::UChapter2Component()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Правила считаются по секундам: десяти раз в секунду хватает и писку, и шкалам.
	PrimaryComponentTick.TickInterval = 0.1f;
	AirSeconds = UChapter2Settings::Get()->AirCapacitySeconds;
}

void UChapter2Component::BeginPlay()
{
	Super::BeginPlay();
	RefreshSampleActors();
}

void UChapter2Component::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Step(DeltaTime, GatherEnv());

	// Подбор касанием и расчётом: если герой уже стоял в образце, когда тот появился, событие
	// входа в сферу не придёт — поэтому касание проверяется и здесь.
	if (IsSamplesQuestActive())
	{
		if (APlayerCharacter* Player = GetPlayer())
		{
			const FVector Loc = Player->GetActorLocation();
			for (TActorIterator<AAnomalyFormation> It(GetWorld()); It; ++It)
			{
				if (It->IsFormationVisible() && FVector::Dist2D(Loc, It->GetActorLocation()) <= It->GetTouchRadius()
					&& FMath::Abs(Loc.Z - It->GetActorLocation().Z) <= 300.0f)
				{
					It->TryCollect(Player);
				}
			}
		}
	}
}

APlayerCharacter* UChapter2Component::GetPlayer() const
{
	return Cast<APlayerCharacter>(GetOwner());
}

UQuestComponent* UChapter2Component::GetQuests() const
{
	const APlayerCharacter* Player = GetPlayer();
	return Player ? Player->GetQuests() : nullptr;
}

UChapter2Component::FEnv UChapter2Component::GatherEnv() const
{
	FEnv Env;
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return Env;
	}
	const FVector Loc = Owner->GetActorLocation();
	const float EdgeDistance = UChapter2Settings::Get()->MaskButtonEdgeDistance;
	Env.HeroLocation = Loc;

	const AToxicAnomalyZone* NearestZone = nullptr;
	for (TActorIterator<AToxicAnomalyZone> It(World); It; ++It)
	{
		const float Dist = It->DistanceTo(Loc);
		Env.bInZone |= (Dist <= 0.0f);
		Env.bNearZone |= (Dist <= EdgeDistance);
		if (Env.ZoneDistance < 0.0f || Dist < Env.ZoneDistance)
		{
			Env.ZoneDistance = Dist;
			NearestZone = *It;
		}
	}
	// Свои числа ближайшей низины (урон, дальность детектора) — настройки её экземпляра.
	if (NearestZone)
	{
		Env.DamageNoMaskPerSecond = NearestZone->GetDamageNoMaskOverride();
		Env.DamageNoAirPerSecond = NearestZone->GetDamageNoAirOverride();
		Env.DetectorRange = NearestZone->GetDetectorRangeOverride();
	}
	for (TActorIterator<AAirRefillZone> It(World); It; ++It)
	{
		Env.bInRefill |= It->IsInside(Loc);
	}
	for (TActorIterator<AAnomalyFormation> It(World); It; ++It)
	{
		if (!It->IsFormationVisible() || IsSamplePicked(It->GetFormationId()))
		{
			continue;
		}
		Env.SampleLocations.Add(It->GetActorLocation());
		const float Dist = FVector::Dist(Loc, It->GetActorLocation());
		if (Env.NearestSampleDistance < 0.0f || Dist < Env.NearestSampleDistance)
		{
			Env.NearestSampleDistance = Dist;
		}
	}
	return Env;
}

void UChapter2Component::Step(float Dt, const FEnv& Env)
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	LastEnv = Env;
	APlayerCharacter* Player = GetPlayer();
	UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	const bool bAlive = Stats && !Stats->IsDead();

	SyncStoryState();
	RefreshSampleActors();

	// Заправка на базе учёных: на входе баллон полон и [224] — один раз за вход; пока герой
	// на базе, воздух не тратится (иначе баллон заправлялся бы и объявлялся каждый тик).
	const bool bRefilling = bApparatusGiven && Env.bInRefill;
	if (bRefilling)
	{
		RefillAir(/*bAnnounce=*/!bWasInRefill);
	}
	bWasInRefill = Env.bInRefill;

	// Вход в низину без аппарата — [221], раз за вход.
	if (Env.bInZone && !bWasInZone && !bMaskOn && bAlive)
	{
		ShowMessage(S->NoMaskInZoneText);
	}
	bWasInZone = Env.bInZone;

	// Воздух тратится, пока аппарат надет (на базе учёных — нет).
	if (bMaskOn && !bRefilling)
	{
		const float Before = AirSeconds;
		AirSeconds = Chapter2Rules::DrainAir(AirSeconds, bMaskOn, Dt);
		if (!bLowAirWarned && Before > S->AirLowThresholdSeconds && AirSeconds <= S->AirLowThresholdSeconds && AirSeconds > 0.0f)
		{
			bLowAirWarned = true;
			ShowMessage(S->AirLowText);
		}
		if (Before > 0.0f && AirSeconds <= 0.0f)
		{
			ShowMessage(S->AirEmptyText);
		}
	}

	// Урон в низине — раз в секунду (решение лида 23.10).
	const float Dps = Chapter2Rules::ZoneDamagePerSecond(Env.bInZone, bMaskOn, AirSeconds,
		Env.DamageNoMaskPerSecond >= 0.0f ? Env.DamageNoMaskPerSecond : S->ZoneDamageNoMaskPerSecond,
		Env.DamageNoAirPerSecond >= 0.0f ? Env.DamageNoAirPerSecond : S->ZoneDamageNoAirPerSecond);
	if (Dps > 0.0f && bAlive)
	{
		DamageAccum += FMath::Max(0.0f, Dt);
		while (DamageAccum >= 1.0f && Stats && !Stats->IsDead())
		{
			DamageAccum -= 1.0f;
			if (!FQADebug::bGodMode)
			{
				Stats->ApplyDamage(Dps);
			}
		}
	}
	else
	{
		DamageAccum = 0.0f;
	}

	// Детектор-локатор (правка Рината 25.10 вечер): на месте левого стика, пока задание идёт,
	// образцы собраны не все и герой в низине или рядом с ней. Писк — как раньше: чем ближе
	// образец, тем чаще.
	const bool bQuestActive = IsSamplesQuestActive();
	const float DetectorRange = Env.DetectorRange > 0.0f ? Env.DetectorRange : S->DetectorRange;
	SignalStrength = bQuestActive ? Chapter2Rules::SignalStrength(Env.NearestSampleDistance, DetectorRange) : 0.0f;
	const int32 SamplesLeft = bQuestActive ? FMath::Max(0, GetSamplesTotal() - GetSamplesDone()) : 0;
	bDetectorShown = bApparatusGiven && Chapter2Rules::ShouldShowLocator(bQuestActive, SamplesLeft,
		Env.bInZone, Env.bNearZone ? 0.0f : Env.ZoneDistance, S->LocatorShowDistance);
	if (bDetectorShown)
	{
		// Отсчёт до стрелки-подсказки идёт, пока локатор на экране; находка начинает его заново.
		LocatorSecondsWithoutPickup += FMath::Max(0.0f, Dt);
	}
	const float Interval = Chapter2Rules::BeepInterval(SignalStrength, S->BeepIntervalNear, S->BeepIntervalFar);
	if (Interval > 0.0f && bAlive)
	{
		// Ближе — пауза короче сразу, не дожидаясь конца прежней длинной паузы.
		BeepTimer = FMath::Min(BeepTimer, Interval) - FMath::Max(0.0f, Dt);
		if (BeepTimer <= 0.0f)
		{
			++BeepCount;
			PlayBeep();
			BeepTimer = Interval;
		}
	}
	else
	{
		BeepTimer = 0.0f;
	}

	// Экран конца главы.
	if (ChapterEndCardRemaining > 0.0f)
	{
		ChapterEndCardRemaining -= FMath::Max(0.0f, Dt);
		if (ChapterEndCardRemaining <= 0.0f)
		{
			ChapterEndCardRemaining = 0.0f;
			FinishChapterEndCard();
		}
	}
}

float UChapter2Component::GetAirFraction() const
{
	const float Cap = UChapter2Settings::Get()->AirCapacitySeconds;
	return Cap > 0.0f ? FMath::Clamp(AirSeconds / Cap, 0.0f, 1.0f) : 0.0f;
}

void UChapter2Component::GiveApparatus()
{
	if (!bApparatusGiven)
	{
		bApparatusGiven = true;
		AirSeconds = UChapter2Settings::Get()->AirCapacitySeconds;
		bLowAirWarned = false;
		UE_LOG(LogQA, Display, TEXT("QA: chapter2 дыхательный аппарат выдан (воздуха %.0f с)"), AirSeconds);
	}
}

bool UChapter2Component::CanToggleMask() const
{
	if (!bApparatusGiven)
	{
		return false;
	}
	return bMaskOn ? !LastEnv.bInZone : (LastEnv.bInZone || LastEnv.bNearZone);
}

FText UChapter2Component::GetMaskActionText() const
{
	if (!CanToggleMask())
	{
		return FText::GetEmpty();
	}
	const UChapter2Settings* S = UChapter2Settings::Get();
	return bMaskOn ? S->TakeOffMaskText : S->PutOnMaskText;
}

bool UChapter2Component::ToggleMask()
{
	if (!CanToggleMask())
	{
		return false;
	}
	bMaskOn = !bMaskOn;
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 аппарат %s (воздуха %.0f с)"), bMaskOn ? TEXT("надет") : TEXT("снят"), AirSeconds);
	return true;
}

void UChapter2Component::RefillAir(bool bAnnounce)
{
	if (!bApparatusGiven)
	{
		return;
	}
	const float Cap = UChapter2Settings::Get()->AirCapacitySeconds;
	if (AirSeconds >= Cap - 0.01f)
	{
		return;
	}
	AirSeconds = Cap;
	bLowAirWarned = false;
	if (bAnnounce)
	{
		ShowMessage(UChapter2Settings::Get()->AirRefilledText);
	}
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 баллон заправлен"));
}

bool UChapter2Component::HasRefillZoneInWorld() const
{
	UWorld* World = GetWorld();
	return World && TActorIterator<AAirRefillZone>(World);
}

const FQuest* UChapter2Component::FindSamplesQuest() const
{
	const UQuestComponent* Quests = GetQuests();
	return Quests ? Quests->FindQuest(Chapter2Ids::SamplesQuest) : nullptr;
}

bool UChapter2Component::IsSamplesQuestActive() const
{
	const FQuest* Q = FindSamplesQuest();
	return Q && Q->State == EQuestState::Active;
}

int32 UChapter2Component::GetSamplesDone() const
{
	const FQuest* Q = FindSamplesQuest();
	if (!Q)
	{
		return 0;
	}
	// Сданное задание — все образцы у Веры.
	return Q->State == EQuestState::TurnedIn ? Q->TargetCount : Q->Progress;
}

int32 UChapter2Component::GetSamplesTotal() const
{
	const FQuest* Q = FindSamplesQuest();
	return Q ? Q->TargetCount : 0;
}

bool UChapter2Component::CollectSample(FName SampleId)
{
	if (SampleId.IsNone() || IsSamplePicked(SampleId) || !IsSamplesQuestActive())
	{
		return false;
	}
	PickedSamples.Add(SampleId);
	LocatorSecondsWithoutPickup = 0.0f; // нашёл — стрелка-подсказка снова ждёт своё время
	if (UQuestComponent* Quests = GetQuests())
	{
		Quests->NotifyObjectiveEvent(Chapter2Ids::SampleObjectiveTag); // ход задания пишет сохранение сам
	}
	// Подобранное образование сразу лежит в сумке предметом (правка Рината 27.10.2026).
	SyncFormationsInBag();
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 образец %s подобран (%d из %d)"),
		*SampleId.ToString(), GetSamplesDone(), GetSamplesTotal());
	RefreshSampleActors();
	return true;
}

void UChapter2Component::RefreshSampleActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const bool bQuestActive = IsSamplesQuestActive();
	for (TActorIterator<AAnomalyFormation> It(World); It; ++It)
	{
		It->SetFormationVisible(bQuestActive && !IsSamplePicked(It->GetFormationId()));
	}
}

int32 UChapter2Component::GetZoneSampleTotal(const UWorld* World)
{
	int32 Total = 0;
	if (World)
	{
		for (TActorIterator<AToxicAnomalyZone> It(World); It; ++It)
		{
			Total += It->GetFormationCount();
		}
	}
	return Total;
}

void UChapter2Component::SyncSamplesTarget()
{
	UQuestComponent* Quests = GetQuests();
	const FQuest* Samples = FindSamplesQuest();
	const int32 Total = GetZoneSampleTotal(GetWorld());
	if (!Quests || !Samples || Total <= 0 || Samples->TargetCount == Total)
	{
		return;
	}
	// Готовое к сдаче и сданное задание не трогаем: их счёт уже засчитан (сохранение, где
	// собраны три образца, не должно стать «2 из 5» после правки низины).
	if (Samples->State != EQuestState::NotStarted && Samples->State != EQuestState::Active)
	{
		return;
	}
	if (Quests->SetObjectiveTarget(Chapter2Ids::SamplesQuest, Total))
	{
		UE_LOG(LogQA, Display, TEXT("QA: chapter2 цель задания образцов — %d (по низинам уровня)"), Total);
	}
}

bool UChapter2Component::AddNote(FName NoteId, bool bAnnounce)
{
	if (NoteId.IsNone() || Notes.Contains(NoteId))
	{
		return false;
	}
	Notes.Add(NoteId);
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 заметка '%s' добавлена в смартфон (всего %d)"), *NoteId.ToString(), Notes.Num());
	if (bAnnounce && NoteId == StoryNoteIds::MercenaryNote)
	{
		ShowMessage(UChapter2Settings::Get()->MercenaryNotePickupText);
	}
	// Найденная заметка переживает «Продолжить» сразу, не дожидаясь следующей записи.
	if (APlayerCharacter* Player = GetPlayer())
	{
		Player->RequestAutoSave(TEXT("заметка"));
	}
	return true;
}

void UChapter2Component::NotePhotoSeen()
{
	AddNote(StoryNoteIds::VeraPhoto, /*bAnnounce=*/false);
}

FQuest UChapter2Component::MakeTalkToVeraQuest()
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	FQuest Q;
	Q.QuestId = Chapter2Ids::TalkToVeraQuest;
	// [230] — и название, и цель: задание одно-единственное «подойти и поговорить».
	Q.Title = S->TalkToVeraText;
	Q.Type = EQuestType::Deliver;
	Q.Kind = EQuestKind::Main; // сюжет: оранжевая метка, как у остальных основных заданий
	Q.KillTargetTag = Chapter2Ids::TalkToVeraQuest; // цель засчитывает только взятие задания Веры
	Q.TargetCount = 1;
	Q.KillObjectiveLabel = S->TalkToVeraText;
	Q.ObjectiveFormatOverride = S->TalkToVeraText;
	// Метка ведёт к самой Вере: у неё такая же обычная метка актора (AVeraNPC).
	Q.MapMarkerTag = Chapter2Ids::VeraTag;
	Q.GiverNpcTag = ScientistBaseTags::QuestScientist; // «Выдал: Учёный» — он и отправил к Вере
	Q.TurnInNpcTag = Chapter2Ids::VeraTag;
	Q.RewardMoney = 0.0f;
	return Q;
}

void UChapter2Component::SyncStoryState()
{
	UQuestComponent* Quests = GetQuests();
	if (!Quests)
	{
		return;
	}
	const FQuest* Samples = FindSamplesQuest();

	// 1) «Поговорить с Верой» [230]: ноутбук сдан учёному, а задание Веры ещё не взято.
	const bool bLaptopDone = Quests->IsQuestTurnedIn(ContraryQuestIds::DeliverLaptopToScientists);
	const bool bWantTalk = bLaptopDone && (!Samples || Samples->State == EQuestState::NotStarted);
	const bool bHasTalk = Quests->FindQuest(Chapter2Ids::TalkToVeraQuest) != nullptr;
	if (bWantTalk && !bHasTalk)
	{
		Quests->AddActiveQuest(MakeTalkToVeraQuest());
	}
	else if (!bWantTalk && bHasTalk)
	{
		Quests->RemoveQuest(Chapter2Ids::TalkToVeraQuest);
	}

	// 2) Цель задания образцов — по числу образцов низин уровня.
	SyncSamplesTarget();

	// 3) Снаряжение учёных: в сумке, пока задание идёт; сдано — Вера забирает его обратно.
	Samples = FindSamplesQuest(); // журнал мог поменяться выше
	if (bApparatusGiven && Samples && Samples->State == EQuestState::TurnedIn)
	{
		bApparatusGiven = false;
		bMaskOn = false;
		UE_LOG(LogQA, Display, TEXT("QA: chapter2 задание сдано — Вера забрала баллон и детектор"));
	}
	SyncGearInBag(bApparatusGiven && Samples && UQuestComponent::IsInActivePhase(*Samples));

	// 4) Аномальные образования в сумке — столько, сколько собрано; сдано — Вера их забрала.
	SyncFormationsInBag();

	// 5) Тексты задания в журнале — как у Веры (сохранение могло быть записано до правки
	//    текстов [216]/[217] 27.10.2026; обычно это одна сверка строк).
	if (Samples)
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AVeraNPC> It(World); It; ++It)
			{
				It->RefreshJournalTexts(Quests);
				break;
			}
		}
	}
}

int32 UChapter2Component::CountGearInBag() const
{
	const APlayerCharacter* Player = GetPlayer();
	const UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Inventory)
	{
		return 0;
	}
	int32 Count = 0;
	for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
	{
		if (IsValid(Item) && AScientistGearItem::IsGearKey(Item->ItemName))
		{
			++Count;
		}
	}
	return Count;
}

void UChapter2Component::SyncGearInBag(bool bWant)
{
	APlayerCharacter* Player = GetPlayer();
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	UWorld* World = GetWorld();
	if (!Inventory || !World)
	{
		return;
	}

	bool bHasAir = false;
	bool bHasDetector = false;
	for (AMasterInventoryItem* Item : Inventory->GetInventoryItems()) // копия списка
	{
		if (!IsValid(Item) || !AScientistGearItem::IsGearKey(Item->ItemName))
		{
			continue;
		}
		if (!bWant)
		{
			// Сюжет забирает своё — мимо запрета «выбросить нельзя».
			Inventory->RemoveItem(Item);
			Item->Destroy();
			continue;
		}
		const bool bAir = Item->ItemName.Equals(Chapter2Ids::AirTankKey, ESearchCase::CaseSensitive);
		bHasAir |= bAir;
		bHasDetector |= !bAir;
	}
	if (!bWant)
	{
		return;
	}

	const FTransform SpawnTM(Player->GetActorRotation(), Player->GetActorLocation());
	for (const TCHAR* Key : { Chapter2Ids::AirTankKey, Chapter2Ids::DetectorKey })
	{
		const bool bAir = (Key == Chapter2Ids::AirTankKey);
		if (bAir ? bHasAir : bHasDetector)
		{
			continue;
		}
		const FString KeyString(Key);
		AMasterInventoryItem* Item = ContraryItems::SpawnItem(World, AScientistGearItem::StaticClass(), SpawnTM, Player,
			[&KeyString](AMasterInventoryItem& It) { It.ItemName = KeyString; });
		if (!Item)
		{
			continue;
		}
		// Предмет сумки — данные, а не объект сцены.
		Item->SetActorHiddenInGame(true);
		Item->SetActorEnableCollision(false);
		Inventory->AddItem(Item);
		UE_LOG(LogQA, Display, TEXT("QA: chapter2 в сумку положено снаряжение учёных '%s'"), *KeyString);
	}
}

int32 UChapter2Component::GetWantedFormationsInBag() const
{
	const FQuest* Samples = FindSamplesQuest();
	if (!Samples || !UQuestComponent::IsInActivePhase(*Samples))
	{
		return 0; // не взято или уже сдано Вере
	}
	return FMath::Max(0, GetSamplesDone());
}

int32 UChapter2Component::CountFormationsInBag() const
{
	const APlayerCharacter* Player = GetPlayer();
	const UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	int32 Count = 0;
	if (Inventory)
	{
		for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
		{
			if (IsValid(Item) && AAnomalyFormationItem::IsFormationKey(Item->ItemName))
			{
				Count += FMath::Max(0, Item->StackCount);
			}
		}
	}
	return Count;
}

void UChapter2Component::SyncFormationsInBag()
{
	APlayerCharacter* Player = GetPlayer();
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	UWorld* World = GetWorld();
	if (!Inventory || !World)
	{
		return;
	}
	const int32 Want = GetWantedFormationsInBag();
	int32 Have = CountFormationsInBag();
	if (Have == Want)
	{
		return;
	}

	// Лишние (задание сдано — Вера забрала; или сохранение дало больше, чем собрано) — прочь.
	// Сюжет забирает своё мимо запрета «выбросить нельзя».
	if (Have > Want)
	{
		int32 Extra = Have - Want;
		for (AMasterInventoryItem* Item : Inventory->GetInventoryItems()) // копия списка
		{
			if (Extra <= 0)
			{
				break;
			}
			if (!IsValid(Item) || !AAnomalyFormationItem::IsFormationKey(Item->ItemName))
			{
				continue;
			}
			const int32 Take = FMath::Min(Extra, FMath::Max(0, Item->StackCount));
			Item->StackCount -= Take;
			Extra -= Take;
			if (Item->StackCount <= 0)
			{
				Inventory->RemoveItem(Item);
				Item->Destroy();
			}
		}
		UE_LOG(LogQA, Display, TEXT("QA: chapter2 аномальных образований в сумке %d (было %d)"), CountFormationsInBag(), Have);
		return;
	}

	// Недостающие — одной стопкой (AddItem сам сольёт её с уже лежащей). В сумке нет предела
	// по числу вещей, поэтому сюжетный предмет помещается всегда.
	const int32 Missing = Want - Have;
	const FTransform SpawnTM(Player->GetActorRotation(), Player->GetActorLocation());
	AMasterInventoryItem* Item = ContraryItems::SpawnItem(World, AAnomalyFormationItem::StaticClass(), SpawnTM, Player,
		[Missing](AMasterInventoryItem& It) { It.ItemName = Chapter2Ids::FormationItemKey; It.StackCount = Missing; });
	if (!Item)
	{
		return;
	}
	// Предмет сумки — данные, а не объект сцены.
	Item->SetActorHiddenInGame(true);
	Item->SetActorEnableCollision(false);
	Inventory->AddItem(Item);
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 в сумку положено аномальных образований: %d (всего %d)"), Missing, CountFormationsInBag());
}

void UChapter2Component::ResetMercenaryEncounters()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AMercenaryEncounter> It(World); It; ++It)
		{
			It->ResetEncounter();
		}
	}
}

bool UChapter2Component::IsHintArrowShown() const
{
	FVector Unused;
	return bDetectorShown && GetNearestSampleLocation(Unused)
		&& Chapter2Rules::ShouldShowHintArrow(LocatorSecondsWithoutPickup, UChapter2Settings::Get()->LocatorHintArrowDelay);
}

bool UChapter2Component::GetNearestSampleLocation(FVector& OutLocation) const
{
	double BestSq = -1.0;
	for (const FVector& Location : LastEnv.SampleLocations)
	{
		const double DistSq = FVector::DistSquared2D(Location, LastEnv.HeroLocation);
		if (BestSq < 0.0 || DistSq < BestSq)
		{
			BestSq = DistSq;
			OutLocation = Location;
		}
	}
	return BestSq >= 0.0;
}

bool UChapter2Component::ShouldSpawnMercenary() const
{
	return Chapter2Rules::ShouldSpawnMercenary(GetSamplesDone(), GetSamplesTotal(), bMercenaryDefeated, LastEnv.bInZone)
		&& FindSamplesQuest() && FindSamplesQuest()->State == EQuestState::Completed;
}

void UChapter2Component::NotifyMercenaryDefeated()
{
	if (bMercenaryDefeated)
	{
		return;
	}
	bMercenaryDefeated = true;
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 наёмник побеждён"));
	if (APlayerCharacter* Player = GetPlayer())
	{
		Player->RequestAutoSave(TEXT("наёмник побеждён"));
	}
}

void UChapter2Component::StartChapterEnd()
{
	bPhotoShown = false;
	if (bChapterEndShown)
	{
		return;
	}
	bChapterEndShown = true;
	NotePhotoSeen(); // фото из ноутбука показано в разговоре — оно в «Заметках»
	ChapterEndCardRemaining = FMath::Max(1.0f, UChapter2Settings::Get()->ChapterEndCardSeconds);
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 конец главы"));
	if (APlayerCharacter* Player = GetPlayer())
	{
		Player->RequestAutoSave(TEXT("конец главы"));
	}
}

void UChapter2Component::FinishChapterEndCard()
{
	// После экрана «Конец первой главы» — прежнее сообщение о конце сюжета текущей версии
	// ([123]) со своей задержкой и правилом «один раз за сохранение».
	const APlayerCharacter* Player = GetPlayer();
	if (const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr)
	{
		if (AContrarySurvivorHUD* CSHUD = PC->GetHUD<AContrarySurvivorHUD>())
		{
			CSHUD->NotifyStoryEpilogueFinished();
		}
	}
}

void UChapter2Component::ShowMessage(const FText& Text) const
{
	if (Text.IsEmpty())
	{
		return;
	}
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 сообщение: %s"), *Text.ToString());
	if (const APlayerCharacter* Player = GetPlayer())
	{
		if (UOnboardingComponent* Hints = Player->GetOnboarding())
		{
			Hints->ShowTransientHint(Text);
		}
	}
}

void UChapter2Component::PlayBeep() const
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	if (S->BeepSound.IsNull())
	{
		return;
	}
	if (USoundBase* Sound = S->BeepSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Sound, S->BeepVolume);
	}
}

void UChapter2Component::WriteToSave(UContrarySaveGame* Save) const
{
	if (!Save)
	{
		return;
	}
	Save->bChapter2ApparatusGiven = bApparatusGiven;
	Save->bChapter2MaskOn = bMaskOn;
	Save->Chapter2AirSeconds = AirSeconds;
	Save->Chapter2PickedSamples = PickedSamples;
	Save->bChapter2MercenaryDefeated = bMercenaryDefeated;
	Save->bChapter2EndShown = bChapterEndShown;
	Save->StoryNotes = Notes;
}

void UChapter2Component::RestoreFromSave(const UContrarySaveGame* Save)
{
	if (!Save)
	{
		ResetForNewGame();
		return;
	}
	const float Cap = UChapter2Settings::Get()->AirCapacitySeconds;
	bApparatusGiven = Save->bChapter2ApparatusGiven;
	bMaskOn = bApparatusGiven && Save->bChapter2MaskOn;
	AirSeconds = bApparatusGiven ? FMath::Clamp(Save->Chapter2AirSeconds, 0.0f, Cap) : Cap;
	PickedSamples = Save->Chapter2PickedSamples;
	bMercenaryDefeated = Save->bChapter2MercenaryDefeated;
	bChapterEndShown = Save->bChapter2EndShown;
	Notes = Save->StoryNotes;
	// Сохранение записано до заметок (или наёмник побеждён, а тело с запиской уже исчезло —
	// тела в сохранение не пишутся): записка всё равно у героя, фото — после конца главы.
	if (bMercenaryDefeated && !Notes.Contains(StoryNoteIds::MercenaryNote))
	{
		Notes.Add(StoryNoteIds::MercenaryNote);
	}
	if (bChapterEndShown && !Notes.Contains(StoryNoteIds::VeraPhoto))
	{
		Notes.Add(StoryNoteIds::VeraPhoto);
	}
	bLowAirWarned = AirSeconds <= UChapter2Settings::Get()->AirLowThresholdSeconds;
	bWasInZone = false;
	bWasInRefill = false;
	DamageAccum = 0.0f;
	bPhotoShown = false;
	ChapterEndCardRemaining = 0.0f;
	RefreshSampleActors();
}

void UChapter2Component::ResetForNewGame()
{
	bApparatusGiven = false;
	bMaskOn = false;
	AirSeconds = UChapter2Settings::Get()->AirCapacitySeconds;
	PickedSamples.Reset();
	bMercenaryDefeated = false;
	bChapterEndShown = false;
	Notes.Reset();
	bWasInZone = false;
	bWasInRefill = false;
	bLowAirWarned = false;
	DamageAccum = 0.0f;
	BeepTimer = 0.0f;
	SignalStrength = 0.0f;
	bDetectorShown = false;
	LocatorSecondsWithoutPickup = 0.0f;
	bPhotoShown = false;
	ChapterEndCardRemaining = 0.0f;
	ResetMercenaryEncounters();
	RefreshSampleActors();
}
