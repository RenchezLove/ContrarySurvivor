// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Components/MapExplorationComponent.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h" // южное логово для района поиска
#include "ContrarySurvivor/Actors/VillageZone.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h" // просьба записать игру после отметки района
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/PdaMapMath.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "GameFramework/Actor.h"

namespace
{
	// Радиус в мире -> радиусы в долях карты (по осям свои: карта может быть не квадратной).
	FVector2D RadiusToMapUV(float RadiusWorld, const UContraryPdaSettings* Settings)
	{
		const FVector2D Span = Settings->MapWorldBottomRight - Settings->MapWorldTopLeft;
		return FVector2D(
			FMath::IsNearlyZero(Span.X) ? 0.0 : RadiusWorld / FMath::Abs(Span.X),
			FMath::IsNearlyZero(Span.Y) ? 0.0 : RadiusWorld / FMath::Abs(Span.Y));
	}
}

UMapExplorationComponent::UMapExplorationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.25f; // уточняется из настроек в BeginPlay
}

void UMapExplorationComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickInterval(FMath::Max(0.05f, UContraryPdaSettings::Get()->MapRevealInterval));
	EnsureFog();
}

void UMapExplorationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RevealAroundOwner();
	AdvanceSearchArea(DeltaTime);
}

void UMapExplorationComponent::EnsureFog()
{
	const FIntPoint Size = UContraryPdaSettings::Get()->MapFogGridSize;
	const int32 WantWidth = FMath::Clamp(Size.X, FPdaMapFog::MinSide, FPdaMapFog::MaxSide);
	const int32 WantHeight = FMath::Clamp(Size.Y, FPdaMapFog::MinSide, FPdaMapFog::MaxSide);
	if (!Fog.IsValid())
	{
		ResetToVillage();
	}
	else if (Fog.GetWidth() != WantWidth || Fog.GetHeight() != WantHeight)
	{
		Fog.ResampleTo(WantWidth, WantHeight);
		++Revision;
	}
}

FVector2D UMapExplorationComponent::FindVillageCenter() const
{
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AVillageZone> It(World); It; ++It)
		{
			const FVector Location = It->GetActorLocation();
			return FVector2D(Location.X, Location.Y);
		}
	}
	return UContraryPdaSettings::Get()->VillageCenterFallback;
}

void UMapExplorationComponent::ResetToVillage()
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	Fog.Init(Settings->MapFogGridSize.X, Settings->MapFogGridSize.Y);
	const FVector2D Center = FindVillageCenter();
	Fog.RevealCircle(
		PdaMap::WorldToMapUV(Center, Settings->MapWorldTopLeft, Settings->MapWorldBottomRight),
		RadiusToMapUV(Settings->VillageRevealRadius, Settings));
	++Revision;
	ClearSearchArea(); // новая игра или запись без карты: района поиска нет
}

bool UMapExplorationComponent::RevealAroundWorld(const FVector2D& WorldLocation, float RadiusWorld)
{
	EnsureFog();
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const bool bChanged = Fog.RevealCircle(
		PdaMap::WorldToMapUV(WorldLocation, Settings->MapWorldTopLeft, Settings->MapWorldBottomRight),
		RadiusToMapUV(RadiusWorld, Settings));
	if (bChanged)
	{
		++Revision;
	}
	return bChanged;
}

bool UMapExplorationComponent::RevealAroundOwner()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	const FVector Location = Owner->GetActorLocation();
	return RevealAroundWorld(FVector2D(Location.X, Location.Y), UContraryPdaSettings::Get()->MapRevealRadius);
}

void UMapExplorationComponent::RevealAll()
{
	EnsureFog();
	if (Fog.RevealAll())
	{
		++Revision;
	}
}

bool UMapExplorationComponent::IsWorldLocationRevealed(const FVector2D& WorldLocation) const
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	return Fog.IsRevealedAt(PdaMap::WorldToMapUV(WorldLocation, Settings->MapWorldTopLeft, Settings->MapWorldBottomRight));
}

void UMapExplorationComponent::WriteToSave(UContrarySaveGame* Save) const
{
	if (!Save)
	{
		return;
	}
	Save->MapFogWidth = Fog.IsValid() ? Fog.GetWidth() : 0;
	Save->MapFogHeight = Fog.IsValid() ? Fog.GetHeight() : 0;
	Fog.Pack(Save->MapFogRuns);
	Save->MapSearchAreaRemaining = SearchAreaRemaining;
	Save->MapSearchAreaCenter = SearchAreaCenter;
	Save->bMapSearchAreaPhoneUnseen = bSearchAreaPhoneUnseen;
	Save->bMapSearchAreaMapUnseen = bSearchAreaMapUnseen;
}

void UMapExplorationComponent::RestoreFromSave(const UContrarySaveGame* Save)
{
	FPdaMapFog Loaded;
	if (Save && Save->MapFogWidth > 0 && Save->MapFogHeight > 0
		&& Loaded.Unpack(Save->MapFogWidth, Save->MapFogHeight, Save->MapFogRuns))
	{
		Fog = MoveTemp(Loaded);
		++Revision;
		EnsureFog(); // размер сетки в настройках мог смениться после записи
	}
	else
	{
		// Сохранение записано до появления карты-исследования (или запись испорчена): деревня
		// и место, где герой стоит сейчас.
		ResetToVillage();
		RevealAroundOwner();
	}

	// Район поиска: отметка, отсчёт и мигания продолжаются с того, на чём записаны.
	SearchAreaRemaining = Save ? FMath::Max(0.0f, Save->MapSearchAreaRemaining) : 0.0f;
	SearchAreaCenter = Save ? Save->MapSearchAreaCenter : FVector2D::ZeroVector;
	bSearchAreaPhoneUnseen = Save && Save->bMapSearchAreaPhoneUnseen && SearchAreaRemaining > 0.0f;
	bSearchAreaMapUnseen = Save && Save->bMapSearchAreaMapUnseen && SearchAreaRemaining > 0.0f;
}

// ===========================================================================
// Район поиска на карте
// ===========================================================================

bool UMapExplorationComponent::MarkSouthWolfDenArea()
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
		{
			if (It->GetQuestMarkerTag() == Settings->SouthWolfDenMarkerTag)
			{
				const FVector Location = It->GetActorLocation();
				MarkSearchArea(FVector2D(Location.X, Location.Y), Settings->SearchAreaSeconds);
				return true;
			}
		}
	}
	UE_LOG(LogQA, Warning, TEXT("QA: map search area NOT marked - на уровне нет базы с меткой '%s'"),
		*Settings->SouthWolfDenMarkerTag.ToString());
	return false;
}

void UMapExplorationComponent::MarkSearchArea(const FVector2D& WorldCenter, float Seconds)
{
	if (Seconds <= 0.0f)
	{
		return;
	}
	SearchAreaCenter = WorldCenter;
	SearchAreaRemaining = Seconds;
	bSearchAreaPhoneUnseen = true;
	bSearchAreaMapUnseen = true;
	UE_LOG(LogQA, Display, TEXT("QA: map search area MARKED at (%.0f, %.0f) for %.0f s"),
		WorldCenter.X, WorldCenter.Y, Seconds);
	// Отметка должна пережить выход из игры сразу после разговора.
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		Player->RequestAutoSave(TEXT("район поиска отмечен на карте"));
	}
}

void UMapExplorationComponent::ClearSearchArea()
{
	SearchAreaRemaining = 0.0f;
	bSearchAreaPhoneUnseen = false;
	bSearchAreaMapUnseen = false;
}

void UMapExplorationComponent::NotePhoneOpened()
{
	bSearchAreaPhoneUnseen = false;
}

void UMapExplorationComponent::NoteMapTabShown()
{
	bSearchAreaPhoneUnseen = false;
	bSearchAreaMapUnseen = false;
}

void UMapExplorationComponent::AdvanceSearchArea(float DeltaSeconds)
{
	if (!IsSearchAreaMarked())
	{
		return;
	}
	const TCHAR* Why = nullptr;
	SearchAreaRemaining -= FMath::Max(0.0f, DeltaSeconds);
	if (SearchAreaRemaining <= 0.0f)
	{
		Why = TEXT("время вышло");
	}
	else if (const AActor* Owner = GetOwner())
	{
		const FVector Location = Owner->GetActorLocation();
		if (FVector2D::Distance(FVector2D(Location.X, Location.Y), SearchAreaCenter)
			<= UContraryPdaSettings::Get()->SearchAreaArrivalRadius)
		{
			Why = TEXT("герой пришёл");
		}
	}
	if (Why)
	{
		ClearSearchArea();
		UE_LOG(LogQA, Display, TEXT("QA: map search area CLEARED (%s)"), Why);
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
		{
			Player->RequestAutoSave(TEXT("район поиска снят с карты"));
		}
	}
}
