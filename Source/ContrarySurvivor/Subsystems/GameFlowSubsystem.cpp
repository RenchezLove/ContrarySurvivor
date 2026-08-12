// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Subsystems/GameFlowSubsystem.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UGameFlowSubsystem* UGameFlowSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UGameFlowSubsystem>() : nullptr;
}

void UGameFlowSubsystem::SetWorldEntryIntent(EContraryWorldEntryIntent Intent)
{
	WorldEntryIntent = Intent;
	UE_LOG(LogQA, Display, TEXT("QA: намерение перехода в мир = %s"), *IntentToString(Intent));
}

EContraryWorldEntryIntent UGameFlowSubsystem::ConsumeWorldEntryIntent()
{
	const EContraryWorldEntryIntent Taken = WorldEntryIntent;
	WorldEntryIntent = EContraryWorldEntryIntent::None;
	return Taken;
}

FString UGameFlowSubsystem::IntentToString(EContraryWorldEntryIntent Intent)
{
	switch (Intent)
	{
		case EContraryWorldEntryIntent::NewGame:  return TEXT("новая игра");
		case EContraryWorldEntryIntent::Continue: return TEXT("продолжить");
		default:                                  return TEXT("нет");
	}
}
