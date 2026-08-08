// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h"
#include "Kismet/GameplayStatics.h"

UAnalyticsProfileSave* UAnalyticsProfileSave::LoadOrCreate()
{
	UAnalyticsProfileSave* Save = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(), GetUserIndex()))
	{
		Save = Cast<UAnalyticsProfileSave>(
			UGameplayStatics::LoadGameFromSlot(GetSlotName(), GetUserIndex()));
	}
	if (!Save)
	{
		Save = Cast<UAnalyticsProfileSave>(
			UGameplayStatics::CreateSaveGameObject(UAnalyticsProfileSave::StaticClass()));
	}
	return Save;
}

bool UAnalyticsProfileSave::Write(UAnalyticsProfileSave* Save)
{
	if (!Save)
	{
		return false;
	}
	return UGameplayStatics::SaveGameToSlot(Save, GetSlotName(), GetUserIndex());
}

bool UAnalyticsProfileSave::MarkFirstLaunchReported()
{
	if (bFirstLaunchReported)
	{
		return false;
	}
	bFirstLaunchReported = true;
	return true;
}

bool UAnalyticsProfileSave::MarkGameLaunched()
{
	if (bGameLaunchedBefore)
	{
		return false;
	}
	bGameLaunchedBefore = true;
	return true;
}

bool UAnalyticsProfileSave::MarkTutorialStepReported(const FString& StepId)
{
	if (StepId.IsEmpty() || ReportedTutorialSteps.Contains(StepId))
	{
		return false;
	}
	ReportedTutorialSteps.Add(StepId);
	return true;
}

bool UAnalyticsProfileSave::MarkTutorialCompletedReported()
{
	if (bTutorialCompletedReported)
	{
		return false;
	}
	bTutorialCompletedReported = true;
	return true;
}
