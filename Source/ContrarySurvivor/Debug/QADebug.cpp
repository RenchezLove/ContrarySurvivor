// Fill out your copyright notice in the Description page of Project Settings.

#include "QADebug.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

// --- Определения статических членов ---
bool FQADebug::bGodMode = false;
bool FQADebug::bForceDrop = false;
bool FQADebug::bOverlayVisible = false;
int32 FQADebug::MaxMessages = 8;
TArray<FString> FQADebug::Messages;

void FQADebug::QA(const UObject* /*WorldCtx*/, const FString& Msg, bool bScreen)
{
	// 1) В лог + немедленный flush (строка не теряется при паузах/тормозах PIE).
	//    ВСЕ события идут в лог-файл (в т.ч. спам — шаги/реген/частые тики).
	UE_LOG(LogQA, Display, TEXT("%s"), *Msg);
	if (GLog)
	{
		GLog->Flush();
	}

	// 2) В экранный оверлей попадают ТОЛЬКО важные (курируемые) события (bScreen=true),
	//    чтобы спам не вытеснял полезные строки из кольцевого буфера.
	if (!bScreen)
	{
		return;
	}

	// 2a) Сжатие дублей: если новое сообщение совпадает с последним (без суффикса-счётчика) —
	//     не добавляем строку, а наращиваем счётчик повторов и показываем "<msg> (xN)".
	if (Messages.Num() > 0)
	{
		FString& Last = Messages.Last();

		// Восстанавливаем «базовую» часть последней строки и её текущий счётчик.
		FString LastBase = Last;
		int32 LastCount = 1;
		int32 OpenIdx = INDEX_NONE;
		if (Last.EndsWith(TEXT(")")) && Last.FindLastChar(TEXT('('), OpenIdx))
		{
			// Ожидаемый формат суффикса: " (xN)".
			const FString Suffix = Last.Mid(OpenIdx); // "(xN)"
			if (Suffix.StartsWith(TEXT("(x")))
			{
				const FString NumStr = Suffix.Mid(2, Suffix.Len() - 3); // между "(x" и ")"
				if (!NumStr.IsEmpty() && NumStr.IsNumeric())
				{
					LastCount = FCString::Atoi(*NumStr);
					LastBase = Last.Left(OpenIdx).TrimEnd();
				}
			}
		}

		if (LastBase == Msg)
		{
			Last = FString::Printf(TEXT("%s (x%d)"), *Msg, LastCount + 1);
			return; // строку НЕ добавляем — повтор не вытесняет остальные
		}
	}

	// 2b) Новая строка -> кольцевой буфер последних MaxMessages сообщений.
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
