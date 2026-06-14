// Fill out your copyright notice in the Description page of Project Settings.

#include "QADebug.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

// --- Определения статических членов ---
bool FQADebug::bGodMode = false;
bool FQADebug::bForceDrop = false;
bool FQADebug::bOverlayVisible = false;
int32 FQADebug::MaxMessages = 8;
TArray<FString> FQADebug::Messages;

void FQADebug::QA(const UObject* /*WorldCtx*/, const FString& Msg)
{
	// 1) В лог + немедленный flush (строка не теряется при паузах/тормозах PIE).
	UE_LOG(LogQA, Display, TEXT("%s"), *Msg);
	if (GLog)
	{
		GLog->Flush();
	}

	// 2) Кольцевой буфер последних MaxMessages сообщений для экранного оверлея.
	Messages.Add(Msg);
	const int32 Cap = FMath::Max(1, MaxMessages);
	while (Messages.Num() > Cap)
	{
		Messages.RemoveAt(0);
	}
}

const TArray<FString>& FQADebug::GetMessages()
{
	return Messages;
}

void FQADebug::ResetMessages()
{
	Messages.Reset();
}
