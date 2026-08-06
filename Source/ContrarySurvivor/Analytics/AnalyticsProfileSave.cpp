// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h"

bool UAnalyticsProfileSave::MarkFirstLaunchReported()
{
	if (bFirstLaunchReported)
	{
		return false;
	}
	bFirstLaunchReported = true;
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
