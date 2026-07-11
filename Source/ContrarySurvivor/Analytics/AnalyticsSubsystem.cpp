// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

#if WITH_GAMEANALYTICS
#include "GameAnalytics.h" // официальный плагин GameAnalytics (Plugins/GameAnalytics, ADR-038)
#endif

void UAnalyticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_GAMEANALYTICS
	// Ключи — ТОЛЬКО из локальных файлов (репо публичный, ADR-013). Нет файлов — тихо выкл.
	FString GameKey, SecretKey;
	const FString GameKeyPath = FPaths::Combine(KeysFolder, TEXT("GameKey.txt"));
	const FString SecretKeyPath = FPaths::Combine(KeysFolder, TEXT("SecretKey.txt"));
	FFileHelper::LoadFileToString(GameKey, *GameKeyPath);
	FFileHelper::LoadFileToString(SecretKey, *SecretKeyPath);
	GameKey.TrimStartAndEndInline();
	SecretKey.TrimStartAndEndInline();

	if (GameKey.IsEmpty() || SecretKey.IsEmpty())
	{
		UE_LOG(LogTemp, Display,
			TEXT("Analytics: keys not found in '%s' - analytics silently disabled"), *KeysFolder);
		return;
	}

	// GA-синглтон живёт на процесс: повторный Initialize (второй PIE-запуск в той же сессии
	// редактора) SDK не нужен — просто включаем отправку у уже настроенного инстанса.
	static bool bSDKInitializedThisProcess = false;
	if (UGameAnalytics* GA = UGameAnalytics::GetInstance())
	{
		if (!bSDKInitializedThisProcess)
		{
			// Значения ключей в лог НЕ выводим — только длины (диагностика формата).
			UE_LOG(LogTemp, Display, TEXT("Analytics: initializing GameAnalytics (key lens %d/%d)"),
				GameKey.Len(), SecretKey.Len());
			GA->ConfigureAutoDetectAppVersion(true);
			GA->Initialize(GameKey, SecretKey); // старт сессии SDK делает сам
			bSDKInitializedThisProcess = true;
		}
		bEnabled = true;
	}
#else
	UE_LOG(LogTemp, Display, TEXT("Analytics: built without GameAnalytics plugin - disabled"));
#endif
}

UAnalyticsSubsystem* UAnalyticsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAnalyticsSubsystem>() : nullptr;
}

void UAnalyticsSubsystem::RecordPlayerDeath()
{
	SendDesignEvent(TEXT("player:death"));
}

void UAnalyticsSubsystem::RecordQuestAccepted(FName QuestId)
{
	SendDesignEvent(FString::Printf(TEXT("quest:accept:%s"), *SanitizeEventPart(QuestId.ToString())));
}

void UAnalyticsSubsystem::RecordQuestTurnedIn(FName QuestId)
{
	SendDesignEvent(FString::Printf(TEXT("quest:turnin:%s"), *SanitizeEventPart(QuestId.ToString())));
}

void UAnalyticsSubsystem::RecordPurchase(const FString& ItemName, float TotalPrice)
{
	SendDesignEvent(FString::Printf(TEXT("shop:buy:%s"), *SanitizeEventPart(ItemName)),
		TotalPrice, /*bWithValue=*/true);
}

void UAnalyticsSubsystem::RecordEnemyKill(const FString& EnemyType)
{
	SendDesignEvent(FString::Printf(TEXT("combat:kill:%s"), *SanitizeEventPart(EnemyType)));
}

void UAnalyticsSubsystem::RecordDailyLogin(int32 StreakDay)
{
	SendDesignEvent(TEXT("retention:daily_login"), static_cast<float>(StreakDay), /*bWithValue=*/true);
}

void UAnalyticsSubsystem::SendDesignEvent(const FString& EventId, float Value, bool bWithValue)
{
	if (!bEnabled)
	{
		return; // тихо: без ключей/плагина события не отправляются и не спамят лог
	}
#if WITH_GAMEANALYTICS
	if (UGameAnalytics* GA = UGameAnalytics::GetInstance())
	{
		if (bWithValue)
		{
			GA->AddDesignEventWithValue(EventId, Value);
		}
		else
		{
			GA->AddDesignEvent(EventId);
		}
	}
#endif
}

FString UAnalyticsSubsystem::SanitizeEventPart(const FString& Raw)
{
	FString Out;
	Out.Reserve(Raw.Len());
	for (const TCHAR C : Raw)
	{
		const bool bAllowed = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z')
			|| (C >= '0' && C <= '9') || C == '_' || C == '-' || C == '.';
		Out.AppendChar(bAllowed ? C : TEXT('_'));
	}
	// Пустых сегментов GA не любит; ограничим и длину (лимит GA на сегмент — 64).
	if (Out.IsEmpty())
	{
		Out = TEXT("unknown");
	}
	return Out.Left(48);
}
