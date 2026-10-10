// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Debug/ContraryDebugMenu.h"

#if CONTRARY_WITH_QA_CHEATS

#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Actors/VillageZone.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Controllers/EnemyAIController.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h"
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h"
#include "UInventoryComponent.h"
#include "AAmmoItem.h"
#include "APistol.h"
#include "ARangedWeapon.h"
#include "AShotgun.h"
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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ---------------------------------------------------------------------------
// Действия меню (без интерфейса)
// ---------------------------------------------------------------------------

namespace ContraryDebugTools
{

const TCHAR* const TeleportTagPrefix = TEXT("DebugTP_");

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
}

bool TeleportPawnNear(APawn* Pawn, const FVector& Point)
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
	for (const FVector2D& Offset : Offsets)
	{
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

	TSharedRef<SBox> Panel = SNew(SBox)
		.WidthOverride(DebugMenuWidth)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f))
			.Padding(FMargin(12.0f))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(ContentBox, SVerticalBox)
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
	// 0 — панель целиком за правым краем, 1 — на месте.
	const float Alpha = SlideAnimation.GetLerp();
	return FSlateRenderTransform(FVector2f((1.0f - Alpha) * DebugMenuWidth, 0.0f));
}

EVisibility SContraryDebugMenu::GetPanelVisibility() const
{
	return (bOpen || SlideAnimation.IsPlaying()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

void SContraryDebugMenu::SetOpen(bool bInOpen)
{
	if (bOpen == bInOpen)
	{
		return;
	}
	bOpen = bInOpen;
	if (bOpen)
	{
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

	auto AddToggle = [Box](const TCHAR* Text, TFunction<bool()> IsOn, TFunction<void(bool)> Set)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SCheckBox)
			.IsFocusable(false)
			.IsChecked_Lambda([IsOn]() { return IsOn() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Set](ECheckBoxState State) { Set(State == ECheckBoxState::Checked); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text))
				.Font(DebugMenuFont(12))
				.ColorAndOpacity(FLinearColor::White)
			]
		];
	};

	auto AddButton = [Box](const FString& Text, TFunction<void()> OnClick)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text))
				.Font(DebugMenuFont(12))
			]
		];
	};

	auto PlayerOf = [](const TWeakObjectPtr<AContrarySurvivorPlayerController>& PC) -> APlayerCharacter*
	{
		return PC.IsValid() ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	};

	Box->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("ОТЛАДКА  (клавиша «ъ» — закрыть)")))
		.Font(DebugMenuFont(13, /*bBold=*/true))
		.ColorAndOpacity(FLinearColor::White)
	];

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

	// --- Телепорт ---
	AddHeader(TEXT("Телепорт"));
	TArray<FContraryDebugTeleportPoint> Points;
	ContraryDebugTools::CollectTeleportPoints(WeakPC.IsValid() ? WeakPC->GetWorld() : nullptr, Points);
	for (const FContraryDebugTeleportPoint& Point : Points)
	{
		const FVector Location = Point.Location;
		const FString Label = Point.Label;
		AddButton(Label, [WeakPC, PlayerOf, Location, Label]()
		{
			const bool bOk = ContraryDebugTools::TeleportPawnNear(PlayerOf(WeakPC), Location);
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
