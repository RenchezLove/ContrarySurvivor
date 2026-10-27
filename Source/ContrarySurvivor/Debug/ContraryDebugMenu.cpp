// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Debug/ContraryDebugMenu.h"

#if CONTRARY_WITH_QA_CHEATS

#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Actors/QuestBarrier.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Actors/VillageZone.h"
#include "ContrarySurvivor/Chapter2/AnomalyFormation.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Chapter2/MercenaryEncounter.h"
#include "ContrarySurvivor/Chapter2/ToxicAnomalyZone.h"
#include "ContrarySurvivor/Chapter2/VeraNPC.h"
#include "ContrarySurvivor/Chapter2/StoryNoteItem.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/MapExplorationComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Controllers/EnemyAIController.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h"
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h"
#include "UInventoryComponent.h"
#include "AAmmoItem.h"
#include "AMasterInventoryItem.h"
#include "APistol.h"
#include "ARangedWeapon.h"
#include "AShotgun.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ---------------------------------------------------------------------------
// Действия меню (без интерфейса)
// ---------------------------------------------------------------------------

namespace ContraryDebugTools
{

const TCHAR* const TeleportTagPrefix = TEXT("DebugTP_");
const float BarrierTeleportOutsideDistance = 450.0f;

void CollectTeleportPoints(UWorld* World, TArray<FContraryDebugTeleportPoint>& OutPoints)
{
	OutPoints.Reset();
	if (!World)
	{
		return;
	}

	// 1) Деревня: центр зоны деревни; зоны на уровне нет — место старосты.
	bool bVillageFound = false;
	for (TActorIterator<AVillageZone> It(World); It; ++It)
	{
		FContraryDebugTeleportPoint Point;
		Point.Label = TEXT("Деревня");
		Point.Location = It->GetActorLocation();
		OutPoints.Add(Point);
		bVillageFound = true;
		break;
	}
	if (!bVillageFound)
	{
		for (TActorIterator<AElderNPC> It(World); It; ++It)
		{
			FContraryDebugTeleportPoint Point;
			Point.Label = TEXT("Деревня");
			Point.Location = It->GetActorLocation();
			OutPoints.Add(Point);
			break;
		}
	}

	// 2) Базы врагов: название места берём у самой базы (поле «Название места» чертежа);
	// базы с одинаковым названием нумеруются в порядке имён акторов — номера не скачут
	// от запуска к запуску.
	TArray<AMasterEnemyBase*> Bases;
	for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
	{
		Bases.Add(*It);
	}
	Bases.Sort([](const AMasterEnemyBase& A, const AMasterEnemyBase& B) { return A.GetName() < B.GetName(); });

	const FTextProperty* NameProp = FindFProperty<FTextProperty>(AMasterEnemyBase::StaticClass(), TEXT("BaseDisplayName"));
	TMap<FString, int32> NameTotals;
	TArray<FString> BaseNames;
	for (const AMasterEnemyBase* Base : Bases)
	{
		FString Name = NameProp ? NameProp->GetPropertyValue_InContainer(Base).ToString() : FString();
		if (Name.IsEmpty())
		{
			Name = Base->GetName();
		}
		BaseNames.Add(Name);
		++NameTotals.FindOrAdd(Name);
	}
	TMap<FString, int32> NameSeen;
	for (int32 Index = 0; Index < Bases.Num(); ++Index)
	{
		const FString& Name = BaseNames[Index];
		FContraryDebugTeleportPoint Point;
		Point.Label = (NameTotals[Name] > 1)
			? FString::Printf(TEXT("%s %d"), *Name, ++NameSeen.FindOrAdd(Name))
			: Name;
		Point.Location = Bases[Index]->GetActorLocation();
		OutPoints.Add(Point);
	}

	// 3) Акторы с меткой DebugTP_<Имя> — новые точки добавляются на уровне без кода.
	const FString Prefix(TeleportTagPrefix);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(Prefix) && TagString.Len() > Prefix.Len())
			{
				FContraryDebugTeleportPoint Point;
				Point.Label = TagString.RightChop(Prefix.Len()).Replace(TEXT("_"), TEXT(" "));
				Point.Location = It->GetActorLocation();
				OutPoints.Add(Point);
				break;
			}
		}
	}
	// 4) Перед шлагбаумом задания: снаружи, на стороне ближайшего охранника (он и говорит с
	// героем через закрытый шлагбаум). Ось X шлагбаума — поперёк забора.
	for (TActorIterator<AQuestBarrier> It(World); It; ++It)
	{
		const FVector BarrierLocation = It->GetActorLocation();
		const AScientistGuardNPC* NearestGuard = nullptr;
		double NearestDistSq = FMath::Square(2000.0);
		for (TActorIterator<AScientistGuardNPC> GuardIt(World); GuardIt; ++GuardIt)
		{
			const double DistSq = FVector::DistSquared2D(GuardIt->GetActorLocation(), BarrierLocation);
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				NearestGuard = *GuardIt;
			}
		}
		if (!NearestGuard)
		{
			continue;
		}
		const FVector Across = It->GetActorForwardVector().GetSafeNormal2D();
		const double Side = FVector::DotProduct(NearestGuard->GetActorLocation() - BarrierLocation, Across) >= 0.0 ? 1.0 : -1.0;
		FContraryDebugTeleportPoint Point;
		Point.Label = TEXT("База учёных — перед шлагбаумом");
		Point.Location = BarrierLocation + Across * Side * BarrierTeleportOutsideDistance;
		Point.bExact = true;
		OutPoints.Add(Point);
	}
}

bool TeleportPawnNear(APawn* Pawn, const FVector& Point, bool bExactFirst)
{
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	// Рядом с точкой, а не в неё саму: в центре базы или метки обычно что-то стоит.
	// Движок сам отодвигает пешку из преграды (TeleportTo с проверкой места); не вышло —
	// пробуем следующее место вокруг точки.
	static const FVector2D Offsets[] = {
		FVector2D(250.0, 0.0), FVector2D(0.0, 250.0), FVector2D(-250.0, 0.0), FVector2D(0.0, -250.0),
		FVector2D(450.0, 450.0), FVector2D(-450.0, -450.0), FVector2D(0.0, 0.0)
	};
	const float HalfHeight = Pawn->GetSimpleCollisionHalfHeight();
	const int32 OffsetCount = UE_ARRAY_COUNT(Offsets);
	for (int32 Step = 0; Step < OffsetCount; ++Step)
	{
		// Точное место: последняя запись списка (сама точка) пробуется первой.
		const FVector2D& Offset = Offsets[bExactFirst ? (Step + OffsetCount - 1) % OffsetCount : Step];
		const float X = Point.X + Offset.X;
		const float Y = Point.Y + Offset.Y;
		const float Z = SpawnPlacement::ResolveSpawnZ(World, X, Y, HalfHeight + 10.0f, TEXT("DebugTP"), Pawn);
		if (Pawn->TeleportTo(FVector(X, Y, Z), Pawn->GetActorRotation(), /*bIsATest=*/false, /*bNoCheck=*/false))
		{
			if (UPawnMovementComponent* Move = Pawn->GetMovementComponent())
			{
				Move->StopMovementImmediately();
			}
			return true;
		}
	}
	return false;
}

ARangedWeapon* GiveFirearm(APlayerCharacter* Player, FName PreferredRow,
	TSubclassOf<ARangedWeapon> FallbackClass, int32 ExtraAmmo)
{
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!World || !Inventory)
	{
		return nullptr;
	}

	// Сначала строка таблицы предметов (там чертёж с настройками из редактора), иначе класс кода.
	const FTransform SpawnTM(Player->GetActorRotation(), Player->GetActorLocation());
	AMasterInventoryItem* FromRow = ContraryItems::SpawnItemFromRow(World, PreferredRow, 0, SpawnTM, Player);
	ARangedWeapon* Weapon = Cast<ARangedWeapon>(FromRow);
	if (FromRow && !Weapon)
	{
		FromRow->Destroy(); // строка есть, но её класс — не огнестрел: ошибка данных, берём класс кода
	}
	if (!Weapon && FallbackClass)
	{
		Weapon = Cast<ARangedWeapon>(ContraryItems::SpawnItem(World, FallbackClass, SpawnTM, Player));
	}
	if (!Weapon)
	{
		return nullptr;
	}

	Weapon->SetActorHiddenInGame(true);
	Weapon->SetActorEnableCollision(false);
	Inventory->AddItem(Weapon);

	// В слот СВОЕГО вида (короткое или длинное): пустой — занять, занятый — поменять стволы
	// местами; второй слот не трогается. Затем в руки.
	if (!Player->TryAdoptRangedWeapon(Weapon))
	{
		Player->SwapRangedWeaponFromBackpack(Weapon);
	}
	if (Player->IsSlotFirearm(Weapon))
	{
		Player->SelectFirearmSlot(Weapon->GetFirearmSlot());
	}

	if (ExtraAmmo > 0)
	{
		Player->AddAmmoToInventory(ExtraAmmo, Weapon->GetAmmoType());
	}
	return Weapon;
}

int32 KillEnemiesNear(APlayerCharacter* Player, float Radius)
{
	if (!Player)
	{
		return 0;
	}
	const FVector Origin = Player->GetActorLocation();
	const float RadiusSq = Radius * Radius;

	// Сначала собираем, потом бьём: смерть врага меняет реестр контроллеров.
	TArray<APawn*> Victims;
	for (const TWeakObjectPtr<AEnemyAIController>& WeakCtrl : AEnemyAIController::GetActiveControllers())
	{
		const AEnemyAIController* Ctrl = WeakCtrl.Get();
		APawn* Enemy = Ctrl ? Ctrl->GetPawn() : nullptr;
		if (IsValid(Enemy) && Enemy->GetWorld() == Player->GetWorld()
			&& FVector::DistSquared(Origin, Enemy->GetActorLocation()) <= RadiusSq)
		{
			Victims.Add(Enemy);
		}
	}
	for (APawn* Enemy : Victims)
	{
		if (IsValid(Enemy))
		{
			Enemy->TakeDamage(1000000.0f, FDamageEvent(), Player->GetController(), Player);
		}
	}
	return Victims.Num();
}

namespace
{
	// Общая часть сюжетных кнопок. bBanditQuestReady: false — задание на бандитов ещё не
	// взято, true — взято и готово к сдаче.
	bool SetElderStoryState(APlayerCharacter* Player, bool bBanditQuestReady)
	{
		UWorld* World = Player ? Player->GetWorld() : nullptr;
		UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
		UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
		AElderNPC* Elder = nullptr;
		if (World)
		{
			for (TActorIterator<AElderNPC> It(World); It; ++It)
			{
				Elder = *It;
				break;
			}
		}
		if (!Elder || !Quests || !Inventory)
		{
			UE_LOG(LogQA, Warning, TEXT("QA: STORY - на уровне нет старосты либо у героя нет журнала заданий или сумки"));
			return false;
		}

		AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController());
		if (PC)
		{
			PC->CloseDialog(); // открытый разговор показывал бы уже несуществующее состояние
		}

		const FQuest& First = Elder->GetOfferedQuest();
		const FQuest& Second = Elder->GetSecondQuest();

		// 1) Предметы заданий, которых в новом состоянии быть не должно: ноутбук и предметы
		// прочих заданий журнала (рация охранника). Шкуры первого задания не трогаем — это товар.
		TSet<FString> QuestItemKeys;
		auto AddKey = [&QuestItemKeys, &First](const FQuest& Quest)
		{
			if (Quest.QuestId != First.QuestId && !Quest.RequiredItemName.IsEmpty())
			{
				QuestItemKeys.Add(Quest.RequiredItemName);
			}
		};
		AddKey(Second);
		AddKey(Elder->GetThirdQuest());
		for (const FQuest& Quest : Quests->GetQuests())
		{
			AddKey(Quest);
		}
		for (AMasterInventoryItem* Item : Inventory->GetInventoryItems()) // копия списка
		{
			if (Item && QuestItemKeys.Contains(Item->ItemName))
			{
				Inventory->RemoveItem(Item);
				Item->Destroy();
			}
		}

		// 2) Журнал: только цепочка старосты, первое задание сдано.
		FQuest FirstDone = First;
		FirstDone.Progress = First.TargetCount;
		FirstDone.ItemProgress = First.RequiredItemCount;
		FirstDone.State = EQuestState::TurnedIn;
		Quests->RestoreQuests({ FirstDone });
		// Глава 2 — как до неё: аппарат, образцы, наёмник и конец главы сбрасываются.
		if (UChapter2Component* Chapter2 = Player->GetChapter2())
		{
			Chapter2->ResetForNewGame();
		}

		bool bOk = true;
		if (bBanditQuestReady)
		{
			// Тем же путём, что в игре: предложено, взято, бандиты убиты, ноутбук в сумке.
			Quests->OfferQuest(Second);
			bOk = Quests->AcceptQuest(Second.QuestId);
			for (int32 Kill = 0; bOk && Kill < Second.TargetCount; ++Kill)
			{
				Quests->NotifyKill(Second.KillTargetTag);
			}
			if (bOk && Second.RequiredItemCount > 0)
			{
				const FName ItemRow = ContraryItems::FindRowNameByLegacyKey(Second.RequiredItemName);
				const int32 Given = Player->GiveItemFromRow(ItemRow, Second.RequiredItemCount);
				if (Given < Second.RequiredItemCount)
				{
					UE_LOG(LogQA, Warning, TEXT("QA: STORY - предмет задания '%s' не выдан (строка таблицы предметов '%s')"),
						*Second.RequiredItemName, *ItemRow.ToString());
				}
			}
			Quests->SyncInventoryQuests(Inventory);
			const FQuest* InLog = Quests->FindQuest(Second.QuestId);
			bOk = InLog && InLog->State == EQuestState::Completed;
		}
		if (UChapter2Component* Chapter2 = Player->GetChapter2())
		{
			Chapter2->SyncStoryState(); // снаряжение учёных и цель «Поговорить с Верой» — прочь
		}

		// 3) База бандитов — как до задания: иначе на зачищенной базе его не выполнить.
		TArray<FName> ResetBaseIds;
		if (!bBanditQuestReady && !Second.MapMarkerTag.IsNone())
		{
			for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
			{
				if (It->GetQuestMarkerTag() == Second.MapMarkerTag)
				{
					It->ResetForNewGame();
					ResetBaseIds.Add(It->GetBaseSaveId());
				}
			}
		}

		// 4) Признаки сюжета в сохранении.
		if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			Save->bElderNotebookHintShown = false;
			Save->ElderNotebookWaitEndPlayTime = -1.0f;
			Save->bEndOfStoryShown = false;
			// Вступительная сценка старосты и его подарок к этому месту сюжета уже позади.
			Save->bElderHookShown = true;
			Save->bElderFirstGiftGiven = true;
			Save->EnemyBaseStates.RemoveAll([&ResetBaseIds](const FSavedEnemyBaseState& State)
				{ return ResetBaseIds.Contains(State.BaseId); });
			Player->WriteSaveObject(Save);
		}
		Player->ReloadStoryStateFromSave();
		Player->NotifyElderGiftGiven();

		// 5) Запланированное или показанное сообщение о конце сюжета — забыть.
		if (PC)
		{
			if (AContrarySurvivorHUD* CSHUD = PC->GetHUD<AContrarySurvivorHUD>())
			{
				CSHUD->ResetEndOfStoryState();
			}
		}

		// 6) К старосте — и всё в сохранение, чтобы «Продолжить» дало то же самое.
		if (!TeleportPawnNear(Player, Elder->GetActorLocation()))
		{
			UE_LOG(LogQA, Warning, TEXT("QA: STORY - рядом со старостой нет свободного места, герой остался где был"));
		}
		if (!Player->SaveGame())
		{
			UE_LOG(LogQA, Warning, TEXT("QA: STORY - состояние не записалось в сохранение"));
			bOk = false;
		}
		return bOk;
	}
}

bool SetStoryElderOffersBanditQuest(APlayerCharacter* Player)
{
	return SetElderStoryState(Player, /*bBanditQuestReady=*/false);
}

bool SetStoryReadyToTurnInBanditLaptop(APlayerCharacter* Player)
{
	return SetElderStoryState(Player, /*bBanditQuestReady=*/true);
}

bool SkipElderNotebookWait(APlayerCharacter* Player)
{
	return Player && Player->SkipElderNotebookWait();
}

// ---------------------------------------------------------------------------
// Глава 2: точки задания учёных
// ---------------------------------------------------------------------------

namespace
{
	template <typename T>
	T* FirstActorOf(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		TActorIterator<T> It(World);
		return It ? *It : nullptr;
	}

	// Точка снаружи отравленной низины у её края, со стороны Toward (база учёных): у края
	// видна кнопка «Надеть дыхательный аппарат», а урона ещё нет.
	bool PoisonZoneEdgeSpot(UWorld* World, const FVector& Toward, FVector& OutLocation)
	{
		const AToxicAnomalyZone* Zone = FirstActorOf<AToxicAnomalyZone>(World);
		const UBoxComponent* Box = Zone ? Zone->GetVolume() : nullptr;
		if (!Box)
		{
			return false;
		}
		const FVector Center = Box->GetComponentLocation();
		FVector Dir = (Toward - Center).GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = FVector(-1.0, 0.0, 0.0);
		}
		// Сколько идти от середины коробки до её края по этому направлению (коробка может быть
		// повёрнута — считаем в её осях).
		const FVector Local = Box->GetComponentTransform().InverseTransformVectorNoScale(Dir);
		const FVector Extent = Box->GetScaledBoxExtent();
		double ToEdge = TNumericLimits<double>::Max();
		if (!FMath::IsNearlyZero(Local.X))
		{
			ToEdge = FMath::Min(ToEdge, Extent.X / FMath::Abs(Local.X));
		}
		if (!FMath::IsNearlyZero(Local.Y))
		{
			ToEdge = FMath::Min(ToEdge, Extent.Y / FMath::Abs(Local.Y));
		}
		if (ToEdge >= TNumericLimits<double>::Max())
		{
			return false;
		}
		const float Outside = FMath::Clamp(UChapter2Settings::Get()->MaskButtonEdgeDistance * 0.4f, 100.0f, 400.0f);
		OutLocation = Center + Dir * (ToEdge + Outside);
		return true;
	}
}

FString GetChapter2DebugPointLabel(EChapter2DebugPoint Point)
{
	switch (Point)
	{
		case EChapter2DebugPoint::TalkToVera:        return TEXT("Глава 2: ноутбук сдан учёному — идти к Вере");
		case EChapter2DebugPoint::SamplesTaken:      return TEXT("Глава 2: задание Веры взято (у края низины)");
		case EChapter2DebugPoint::SamplesCollected:  return TEXT("Глава 2: все образцы собраны (перед наёмником)");
		case EChapter2DebugPoint::MercenaryDefeated: return TEXT("Глава 2: наёмник побеждён (сдать задание Вере)");
	}
	return FString();
}

bool FindChapter2DebugSpot(UWorld* World, EChapter2DebugPoint Point, FVector& OutLocation)
{
	const AVeraNPC* Vera = FirstActorOf<AVeraNPC>(World);
	switch (Point)
	{
		case EChapter2DebugPoint::TalkToVera:
		{
			// У учёного, которому только что сдан ноутбук: дальше герой идёт к Вере по метке.
			if (const AScientistQuestNPC* Scientist = FirstActorOf<AScientistQuestNPC>(World))
			{
				OutLocation = Scientist->GetActorLocation();
				return true;
			}
			break;
		}
		case EChapter2DebugPoint::SamplesTaken:
		{
			if (PoisonZoneEdgeSpot(World, Vera ? Vera->GetActorLocation() : FVector::ZeroVector, OutLocation))
			{
				return true;
			}
			break;
		}
		case EChapter2DebugPoint::SamplesCollected:
		{
			// Чуть дальше расстояния разговора от места появления наёмника, со стороны низины:
			// наёмник появляется сразу (герой вне низины), разговор — через пару шагов.
			const AMercenaryEncounter* Encounter = FirstActorOf<AMercenaryEncounter>(World);
			const AToxicAnomalyZone* Zone = FirstActorOf<AToxicAnomalyZone>(World);
			if (Encounter)
			{
				const FVector From = Encounter->GetActorLocation();
				FVector Dir = Zone ? (Zone->GetActorLocation() - From).GetSafeNormal2D() : FVector::ZeroVector;
				if (Dir.IsNearlyZero())
				{
					Dir = FVector(1.0, 0.0, 0.0);
				}
				const FVector Spot = From + Dir * (Encounter->GetTalkDistance() + 500.0f);
				if (!Zone || Zone->DistanceTo(Spot) > 0.0f)
				{
					OutLocation = Spot;
					return true;
				}
			}
			// Наёмника на уровне нет или точка попала в низину — у края низины.
			if (PoisonZoneEdgeSpot(World, Vera ? Vera->GetActorLocation() : FVector::ZeroVector, OutLocation))
			{
				return true;
			}
			break;
		}
		case EChapter2DebugPoint::MercenaryDefeated:
			break;
	}
	// Последний выбор для любой точки — у Веры.
	if (Vera)
	{
		OutLocation = Vera->GetActorLocation();
		return true;
	}
	return false;
}

bool SetStoryChapter2(APlayerCharacter* Player, EChapter2DebugPoint Point)
{
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	UChapter2Component* Chapter2 = Player ? Player->GetChapter2() : nullptr;
	AElderNPC* Elder = FirstActorOf<AElderNPC>(World);
	AVeraNPC* Vera = FirstActorOf<AVeraNPC>(World);
	if (!Elder || !Vera || !Quests || !Inventory || !Chapter2)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: STORY chapter2 - на уровне нет старосты или Веры либо у героя нет журнала, сумки или главы 2"));
		return false;
	}

	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController());
	if (PC)
	{
		PC->CloseDialog(); // открытый разговор показывал бы уже несуществующее состояние
	}

	// 1) Журнал: цепочка старосты сдана целиком (третье задание — ноутбук учёным), рация
	// охранника сдана (шлагбаум открыт). Предметы этих заданий из сумки убираются.
	TArray<FQuest> Chain = { Elder->GetOfferedQuest(), Elder->GetSecondQuest(), Elder->GetThirdQuest() };
	if (const AScientistGuardNPC* Guard = FirstActorOf<AScientistGuardNPC>(World))
	{
		Chain.Add(Guard->GetOfferedQuest());
	}
	TSet<FString> QuestItemKeys;
	for (const FQuest& Quest : Quests->GetQuests())
	{
		if (Quest.QuestId != Elder->GetOfferedQuest().QuestId && !Quest.RequiredItemName.IsEmpty())
		{
			QuestItemKeys.Add(Quest.RequiredItemName);
		}
	}
	TArray<FQuest> Journal;
	for (const FQuest& Quest : Chain)
	{
		if (Quest.QuestId.IsNone())
		{
			continue;
		}
		if (Quest.QuestId != Elder->GetOfferedQuest().QuestId && !Quest.RequiredItemName.IsEmpty())
		{
			QuestItemKeys.Add(Quest.RequiredItemName); // шкуры первого задания — товар, их не трогаем
		}
		FQuest Done = Quest;
		Done.Progress = Quest.TargetCount;
		Done.ItemProgress = Quest.RequiredItemCount;
		Done.State = EQuestState::TurnedIn;
		Journal.Add(Done);
	}
	for (AMasterInventoryItem* Item : Inventory->GetInventoryItems()) // копия списка
	{
		if (Item && QuestItemKeys.Contains(Item->ItemName))
		{
			Inventory->RemoveItem(Item);
			Item->Destroy();
		}
	}
	Quests->RestoreQuests(Journal);

	// 2) Глава 2 с чистого листа (наёмник убран, образцы снова на местах), затем — до точки.
	Chapter2->ResetForNewGame();
	bool bOk = Quests->IsQuestTurnedIn(ContraryQuestIds::DeliverLaptopToScientists);
	if (Point != EChapter2DebugPoint::TalkToVera)
	{
		// Тем же путём, что в разговоре с Верой: задание предложено и взято, аппарат выдан.
		Quests->OfferQuest(Vera->GetSamplesQuest());
		bOk &= Quests->AcceptQuest(Chapter2Ids::SamplesQuest);
		Chapter2->GiveApparatus();
		Chapter2->SyncSamplesTarget(); // сколько образцов — решает низина на уровне
	}
	if (Point == EChapter2DebugPoint::SamplesCollected || Point == EChapter2DebugPoint::MercenaryDefeated)
	{
		// Образцы уровня — по их именам (подбор пишется в сохранение по имени); не хватает
		// образцов на уровне — недостающие засчитываются под запасными именами.
		TArray<FName> Ids;
		for (TActorIterator<AAnomalyFormation> It(World); It; ++It)
		{
			Ids.AddUnique(It->GetFormationId());
		}
		Ids.Sort(FNameLexicalLess());
		for (int32 Index = 1; Index <= Chapter2->GetSamplesTotal(); ++Index)
		{
			Ids.AddUnique(FName(*FString::Printf(TEXT("DebugSample%d"), Index)));
		}
		for (const FName& Id : Ids)
		{
			if (Chapter2->GetSamplesDone() >= Chapter2->GetSamplesTotal())
			{
				break;
			}
			Chapter2->CollectSample(Id);
		}
		const FQuest* Samples = Chapter2->FindSamplesQuest();
		bOk &= Samples && Samples->State == EQuestState::Completed;
	}
	if (Point == EChapter2DebugPoint::MercenaryDefeated)
	{
		Chapter2->NotifyMercenaryDefeated();
		// Записка из добычи наёмника — сразу в «Заметках» (правка Рината 26.10.2026): тела в этой
		// точке нет, обыскивать некого.
		Chapter2->AddNote(StoryNoteIds::MercenaryNote, /*bAnnounce=*/false);
	}
	Chapter2->SyncStoryState(); // цель [230] и снаряжение в сумке — по новому журналу
	Chapter2->RefreshSampleActors();

	// 3) Признаки сюжета в сохранении: сценки старосты позади, ожидания нет, сообщение о конце
	// сюжета не показано.
	if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		Save->bElderNotebookHintShown = true;
		Save->ElderNotebookWaitEndPlayTime = -1.0f;
		Save->bEndOfStoryShown = false;
		Save->bElderHookShown = true;
		Save->bElderFirstGiftGiven = true;
		Player->WriteSaveObject(Save);
	}
	Player->ReloadStoryStateFromSave();
	Player->NotifyElderGiftGiven();
	if (PC)
	{
		if (AContrarySurvivorHUD* CSHUD = PC->GetHUD<AContrarySurvivorHUD>())
		{
			CSHUD->ResetEndOfStoryState();
		}
	}

	// 4) На место точки — и всё в сохранение, чтобы «Продолжить» дало то же самое.
	FVector Spot;
	if (!FindChapter2DebugSpot(World, Point, Spot) || !TeleportPawnNear(Player, Spot, /*bExactFirst=*/true))
	{
		UE_LOG(LogQA, Warning, TEXT("QA: STORY chapter2 - место точки не нашлось или занято, герой остался где был"));
	}
	if (!Player->SaveGame())
	{
		UE_LOG(LogQA, Warning, TEXT("QA: STORY chapter2 - состояние не записалось в сохранение"));
		bOk = false;
	}
	UE_LOG(LogQA, Display, TEXT("QA: STORY chapter2 точка '%s' %s"), *GetChapter2DebugPointLabel(Point), bOk ? TEXT("OK") : TEXT("FAIL"));
	return bOk;
}

bool RevealWholeMap(APlayerCharacter* Player)
{
	UMapExplorationComponent* Exploration = Player ? Player->GetMapExploration() : nullptr;
	if (!Exploration)
	{
		return false;
	}
	Exploration->RevealAll();
	return true;
}

bool HideMapAgain(APlayerCharacter* Player)
{
	UMapExplorationComponent* Exploration = Player ? Player->GetMapExploration() : nullptr;
	if (!Exploration)
	{
		return false;
	}
	Exploration->ResetToVillage();
	return true;
}

} // namespace ContraryDebugTools

// ---------------------------------------------------------------------------
// Панель
// ---------------------------------------------------------------------------

namespace
{
	const float DebugMenuWidth = 340.0f;
	const float DebugMenuKillRadius = 2500.0f;

	FSlateFontInfo DebugMenuFont(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}
}

void SContraryDebugMenu::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	SlideAnimation = FCurveSequence(0.0f, 0.18f, ECurveEaseFunction::QuadOut);

	// Раскладка под палец (телефон) — тот же набор кубиков, увеличенный масштабатором целиком:
	// ширина, шрифт, отметки тумблеров и полоса прокрутки растут вместе.
	TSharedRef<SDPIScaler> Panel = SNew(SDPIScaler)
		.DPIScale(this, &SContraryDebugMenu::GetPanelScale)
		[
			SNew(SBox)
			.WidthOverride(DebugMenuWidth)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f))
				.Padding(FMargin(12.0f))
				[
					SNew(SVerticalBox)

					// Шапка вне прокрутки: заголовок и кнопка «Закрыть» видны всегда.
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("ОТЛАДКА")))
							.Font(DebugMenuFont(13, /*bBold=*/true))
							.ColorAndOpacity(FLinearColor::White)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredHeight_Lambda([this]() { return bTouchLayout ? FOptionalSize(TouchRowMinHeight) : FOptionalSize(); })
							.MinDesiredWidth(96.0f)
							[
								SNew(SButton)
								.IsFocusable(false)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked(this, &SContraryDebugMenu::HandleCloseClicked)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Закрыть")))
									.Font(DebugMenuFont(12, /*bBold=*/true))
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Открыть и закрыть: клавиша «ъ» или касание экрана тремя пальцами")))
						.Font(DebugMenuFont(10))
						.AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]

					// Длинный список листается пальцем (прокрутка касанием включена у SScrollBox
					// по умолчанию); полоса шире обычной, чтобы было видно, что список длиннее.
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SScrollBox)
						.ScrollBarThickness(FVector2D(8.0, 8.0))
						+ SScrollBox::Slot()
						[
							SAssignNew(ContentBox, SVerticalBox)
						]
					]
				]
			]
		];
	Panel->SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::CreateSP(
		this, &SContraryDebugMenu::GetSlideTransform));

	// Корень растянут на весь экран, но нажатия ловит только сама панель у правого края —
	// по остальному экрану игра кликается как обычно.
	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	[
		Panel
	];
	SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SContraryDebugMenu::GetPanelVisibility));

	RebuildContent();
}

APlayerCharacter* SContraryDebugMenu::GetPlayer() const
{
	const AContrarySurvivorPlayerController* PC = Controller.Get();
	return PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
}

TOptional<FSlateRenderTransform> SContraryDebugMenu::GetSlideTransform() const
{
	// 0 — панель целиком за правым краем, 1 — на месте. Сдвиг — в единицах родителя, поэтому
	// ширина панели берётся уже увеличенной.
	const float Alpha = SlideAnimation.GetLerp();
	return FSlateRenderTransform(FVector2f((1.0f - Alpha) * DebugMenuWidth * GetPanelScale(), 0.0f));
}

EVisibility SContraryDebugMenu::GetPanelVisibility() const
{
	return (bOpen || SlideAnimation.IsPlaying()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

float SContraryDebugMenu::GetPanelScale() const
{
	return bTouchLayout ? TouchLayoutScale : 1.0f;
}

FReply SContraryDebugMenu::HandleCloseClicked()
{
	// Закрываем через контроллер — он же вернёт игровой режим ввода.
	if (AContrarySurvivorPlayerController* PC = Controller.Get())
	{
		if (PC->IsDebugMenuOpen())
		{
			PC->OnQAToggleDebugMenu();
		}
	}
	else
	{
		SetOpen(false);
	}
	return FReply::Handled();
}

void SContraryDebugMenu::SetOpen(bool bInOpen, bool bInTouchLayout)
{
	if (bOpen == bInOpen)
	{
		return;
	}
	bOpen = bInOpen;
	if (bOpen)
	{
		// Раскладку меняем только при показе: на уезжающей панели она не должна прыгать.
		bTouchLayout = bInTouchLayout;
		RebuildContent();
		SlideAnimation.Play(AsShared());
	}
	else
	{
		SlideAnimation.PlayReverse(AsShared());
	}
}

void SContraryDebugMenu::RebuildContent()
{
	if (!ContentBox.IsValid())
	{
		return;
	}
	ContentBox->ClearChildren();

	TWeakObjectPtr<AContrarySurvivorPlayerController> WeakPC = Controller;
	TSharedPtr<SVerticalBox> Box = ContentBox;

	auto AddHeader = [Box](const TCHAR* Text)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(DebugMenuFont(12, /*bBold=*/true))
			.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.35f))
		];
	};

	// Раскладка под палец: строка не ниже TouchRowMinHeight (до увеличения масштабатором).
	// PreciseTap — нажатие засчитывается, только если палец не повели: иначе это прокрутка
	// списка, а не нажатие (SlateEnums.h, EButtonTouchMethod).
	const FOptionalSize RowMinHeight = bTouchLayout ? FOptionalSize(TouchRowMinHeight) : FOptionalSize();

	auto AddToggle = [Box, RowMinHeight](const TCHAR* Text, TFunction<bool()> IsOn, TFunction<void(bool)> Set)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SBox)
			.MinDesiredHeight(RowMinHeight)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsFocusable(false)
				.TouchMethod(EButtonTouchMethod::PreciseTap)
				.IsChecked_Lambda([IsOn]() { return IsOn() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Set](ECheckBoxState State) { Set(State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock)
					.Text(FText::FromString(Text))
					.Font(DebugMenuFont(12))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
		];
	};

	auto AddButton = [Box, RowMinHeight](const FString& Text, TFunction<void()> OnClick)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SBox)
			.MinDesiredHeight(RowMinHeight)
			[
				SNew(SButton)
				.IsFocusable(false)
				.TouchMethod(EButtonTouchMethod::PreciseTap)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[
					SNew(STextBlock)
					.Text(FText::FromString(Text))
					.Font(DebugMenuFont(12))
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
				]
			]
		];
	};

	auto PlayerOf = [](const TWeakObjectPtr<AContrarySurvivorPlayerController>& PC) -> APlayerCharacter*
	{
		return PC.IsValid() ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	};

	// --- Тумблеры ---
	AddHeader(TEXT("Режимы"));
	AddToggle(TEXT("Неуязвимость героя"),
		[]() { return FQADebug::bGodMode; },
		[WeakPC](bool bOn)
		{
			FQADebug::bGodMode = bOn;
			FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: GODMODE %s (меню)"), bOn ? TEXT("on") : TEXT("off")), /*bScreen=*/true);
		});
	AddToggle(TEXT("Заморозить всех врагов"),
		[]() { return FQADebug::bFreezeEnemies; },
		[WeakPC](bool bOn)
		{
			if (AContrarySurvivorPlayerController* PC = WeakPC.Get())
			{
				PC->ApplyQAFreezeEnemies(bOn);
			}
		});
	AddToggle(TEXT("Счётчик кадров"),
		[]() { return UContrarySurvivorGameUserSettings::IsFpsCounterShownSafe(); },
		[](bool bOn)
		{
			if (UContrarySurvivorGameUserSettings* Settings = UContrarySurvivorGameUserSettings::Get())
			{
				Settings->SetFpsCounterShown(bOn);
			}
		});

	// --- Герой ---
	AddHeader(TEXT("Герой"));
	AddButton(TEXT("Здоровье 100 (и голод, жажда)"), [WeakPC]()
	{
		if (AContrarySurvivorPlayerController* PC = WeakPC.Get())
		{
			PC->OnQAFullStats();
		}
	});
	AddButton(TEXT("Деньги +1000"), [WeakPC, PlayerOf]()
	{
		if (APlayerCharacter* Player = PlayerOf(WeakPC))
		{
			if (UStatsComponent* Stats = Player->GetStats())
			{
				Stats->AddMoney(1000.0f);
			}
		}
	});
	AddButton(TEXT("Двустволка + 20 патронов"), [WeakPC, PlayerOf]()
	{
		ContraryDebugTools::GiveFirearm(PlayerOf(WeakPC), FName(TEXT("shotgun_toz34")), AShotgun::StaticClass(), 20);
	});
	AddButton(TEXT("Пистолет + 24 патрона"), [WeakPC, PlayerOf]()
	{
		ContraryDebugTools::GiveFirearm(PlayerOf(WeakPC), FName(TEXT("pistol")), APistol::StaticClass(), 24);
	});
	AddButton(TEXT("Убить врагов рядом"), [WeakPC, PlayerOf]()
	{
		const int32 Killed = ContraryDebugTools::KillEnemiesNear(PlayerOf(WeakPC), DebugMenuKillRadius);
		FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: KILL NEARBY (%d)"), Killed), /*bScreen=*/true);
	});

	// --- Сюжет ---
	AddHeader(TEXT("Сюжет"));
	AddButton(TEXT("Сюжет: староста выдаёт задание на бандитов"), [WeakPC, PlayerOf]()
	{
		const bool bOk = ContraryDebugTools::SetStoryElderOffersBanditQuest(PlayerOf(WeakPC));
		FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: STORY bandit quest offer %s"), bOk ? TEXT("OK") : TEXT("FAIL")), /*bScreen=*/true);
	});
	AddButton(TEXT("Сюжет: сдать старосте ноутбук бандитов"), [WeakPC, PlayerOf]()
	{
		const bool bOk = ContraryDebugTools::SetStoryReadyToTurnInBanditLaptop(PlayerOf(WeakPC));
		FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: STORY laptop turn-in %s"), bOk ? TEXT("OK") : TEXT("FAIL")), /*bScreen=*/true);
	});
	AddButton(TEXT("Сюжет: пропустить ожидание у старосты"), [WeakPC, PlayerOf]()
	{
		const bool bOk = ContraryDebugTools::SkipElderNotebookWait(PlayerOf(WeakPC));
		FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: STORY skip elder wait %s"), bOk ? TEXT("OK") : TEXT("ожидание не идёт")), /*bScreen=*/true);
	});

	// --- Глава 2: задание учёных «Образцы из аномалий» ---
	AddHeader(TEXT("Глава 2"));
	for (const ContraryDebugTools::EChapter2DebugPoint Point : { ContraryDebugTools::EChapter2DebugPoint::TalkToVera,
		ContraryDebugTools::EChapter2DebugPoint::SamplesTaken, ContraryDebugTools::EChapter2DebugPoint::SamplesCollected,
		ContraryDebugTools::EChapter2DebugPoint::MercenaryDefeated })
	{
		AddButton(ContraryDebugTools::GetChapter2DebugPointLabel(Point), [WeakPC, PlayerOf, Point]()
		{
			const bool bOk = ContraryDebugTools::SetStoryChapter2(PlayerOf(WeakPC), Point);
			FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: STORY chapter2 %d %s"), static_cast<int32>(Point), bOk ? TEXT("OK") : TEXT("FAIL")), /*bScreen=*/true);
		});
	}

	// --- Карта КПК ---
	AddHeader(TEXT("Карта"));
	AddButton(TEXT("Карта: открыть всю"), [WeakPC, PlayerOf]()
	{
		const bool bOk = ContraryDebugTools::RevealWholeMap(PlayerOf(WeakPC));
		FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: MAP reveal all %s"), bOk ? TEXT("OK") : TEXT("FAIL")), /*bScreen=*/true);
	});
	AddButton(TEXT("Карта: закрыть заново (останется деревня)"), [WeakPC, PlayerOf]()
	{
		const bool bOk = ContraryDebugTools::HideMapAgain(PlayerOf(WeakPC));
		FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: MAP hide again %s"), bOk ? TEXT("OK") : TEXT("FAIL")), /*bScreen=*/true);
	});

	// --- Телепорт ---
	AddHeader(TEXT("Телепорт"));
	TArray<FContraryDebugTeleportPoint> Points;
	ContraryDebugTools::CollectTeleportPoints(WeakPC.IsValid() ? WeakPC->GetWorld() : nullptr, Points);
	for (const FContraryDebugTeleportPoint& Point : Points)
	{
		const FVector Location = Point.Location;
		const FString Label = Point.Label;
		const bool bExact = Point.bExact;
		AddButton(Label, [WeakPC, PlayerOf, Location, Label, bExact]()
		{
			const bool bOk = ContraryDebugTools::TeleportPawnNear(PlayerOf(WeakPC), Location, bExact);
			FQADebug::QA(WeakPC.Get(), FString::Printf(TEXT("QA: TELEPORT '%s' %s"), *Label, bOk ? TEXT("OK") : TEXT("FAIL")), /*bScreen=*/true);
		});
	}
	if (Points.Num() == 0)
	{
		Box->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("На уровне нет точек телепорта")))
			.Font(DebugMenuFont(11))
			.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
		];
	}
}

#endif // CONTRARY_WITH_QA_CHEATS
