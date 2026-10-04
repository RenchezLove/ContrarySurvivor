// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/AdGameArea.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/ViewportSplitScreen.h" // FSplitscreenData, ESplitScreenType

namespace AdGameArea
{
	namespace
	{
		// Что сейчас применено к живой игре. Масштаб интерфейса — общая настройка движка
		// (в редакторе одна на все запуски игры), поэтому исходное значение запоминаем при
		// первом сжатии и возвращаем ровно его.
		float GAppliedFraction = 0.0f;
		float GBaseApplicationScale = 1.0f;
	}

	FArea Compute(float BannerHeightFraction)
	{
		FArea Area;
		const float Fraction = FMath::Clamp(BannerHeightFraction, 0.0f, MaxBannerFraction);
		if (Fraction <= 0.0f)
		{
			return Area; // весь экран
		}
		// Равномерное сжатие: та же доля по ширине и высоте, верх экрана, по центру.
		const double Scale = 1.0 - static_cast<double>(Fraction);
		Area.Size = FVector2D(Scale, Scale);
		Area.Origin = FVector2D(static_cast<double>(Fraction) * 0.5, 0.0);
		return Area;
	}

	void FBannerState::OnRequested(FName InPlacement)
	{
		Placement = InPlacement;
		HeightFraction = 0.0f;
	}

	bool FBannerState::OnLoaded(float InHeightFraction)
	{
		if (Placement.IsNone() || InHeightFraction <= 0.0f)
		{
			return false;
		}
		HeightFraction = FMath::Min(InHeightFraction, MaxBannerFraction);
		return true;
	}

	void FBannerState::OnLoadFailed()
	{
		HeightFraction = 0.0f;
	}

	bool FBannerState::OnHidden(FName InPlacement)
	{
		if (Placement.IsNone() || Placement != InPlacement)
		{
			return false;
		}
		Reset();
		return true;
	}

	void FBannerState::Reset()
	{
		Placement = NAME_None;
		HeightFraction = 0.0f;
	}

	void Apply(UGameInstance* GameInstance, float BannerHeightFraction)
	{
		const float Fraction = FMath::Clamp(BannerHeightFraction, 0.0f, MaxBannerFraction);
		if (FMath::IsNearlyEqual(Fraction, GAppliedFraction, 0.0005f))
		{
			return;
		}

		// 1) Масштаб интерфейса: окна уменьшаются на ту же долю, что и область игры.
		if (UUserInterfaceSettings* UiSettings = GetMutableDefault<UUserInterfaceSettings>())
		{
			if (GAppliedFraction <= 0.0f)
			{
				GBaseApplicationScale = UiSettings->ApplicationScale;
			}
			UiSettings->ApplicationScale = GBaseApplicationScale * (1.0f - Fraction);
		}
		GAppliedFraction = Fraction;

		// 2) Область вывода игрока — запись «без разделения экрана, игрок 0».
		UGameViewportClient* ViewportClient = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
		if (ViewportClient
			&& ViewportClient->SplitscreenInfo.IsValidIndex(ESplitScreenType::None)
			&& ViewportClient->SplitscreenInfo[ESplitScreenType::None].PlayerData.Num() > 0)
		{
			const FArea Area = Compute(Fraction);
			FPerPlayerSplitscreenData& PlayerData = ViewportClient->SplitscreenInfo[ESplitScreenType::None].PlayerData[0];
			PlayerData.SizeX = static_cast<float>(Area.Size.X);
			PlayerData.SizeY = static_cast<float>(Area.Size.Y);
			PlayerData.OriginX = static_cast<float>(Area.Origin.X);
			PlayerData.OriginY = static_cast<float>(Area.Origin.Y);
			ViewportClient->LayoutPlayers();
		}

		UE_LOG(LogQA, Display, TEXT("QA: AD game area %s (banner height %.3f of screen)"),
			Fraction > 0.0f ? TEXT("shrunk") : TEXT("full screen"), Fraction);
	}
}
