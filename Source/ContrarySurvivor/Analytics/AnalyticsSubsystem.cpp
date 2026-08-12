// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h"
#include "Kismet/GameplayStatics.h"
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
	if (!ResolveKeys())
	{
		return; // без ключей отправлять некуда, причина уже написана в журнал
	}

	// Б6: до ответа игрока на экране согласия SDK не поднимаем и НИЧЕГО не отправляем —
	// согласие за игрока не ставится (условие издателя по РИ-30). Ответ придёт от
	// UDataConsentSubsystem вызовом SetDataConsent.
	const EDataConsentState Consent = GetStoredConsentState();
	if (Consent == EDataConsentState::Accepted)
	{
		StartSdkAndEnable();
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Analytics: статистика ждёт согласия игрока (сохранённый ответ: %s)"),
			Consent == EDataConsentState::Declined ? TEXT("отказ") : TEXT("ещё не спрашивали"));
	}
#else
	UE_LOG(LogTemp, Display, TEXT("Analytics: built without GameAnalytics plugin - disabled"));
#endif
}

bool UAnalyticsSubsystem::ResolveKeys()
{
	// Ключи (ADR-013: репо публичный, значений в исходниках нет). Сначала вшитые компилятором —
	// это единственный источник, который доезжает до собранной игры на телефоне (Б4); если
	// сборка делалась без папки ключей, откатываемся на прежнее чтение файлов с диска.
	FString KeySource;

#if CONTRARY_GA_KEYS_COMPILED_IN
	ResolvedGameKey = TEXT(CONTRARY_GA_GAME_KEY);
	ResolvedSecretKey = TEXT(CONTRARY_GA_SECRET_KEY);
	KeySource = TEXT("вшиты в сборку на этапе компиляции");
#endif

	if (ResolvedGameKey.IsEmpty() || ResolvedSecretKey.IsEmpty())
	{
		const FString GameKeyPath = FPaths::Combine(KeysFolder, TEXT("GameKey.txt"));
		const FString SecretKeyPath = FPaths::Combine(KeysFolder, TEXT("SecretKey.txt"));
		FFileHelper::LoadFileToString(ResolvedGameKey, *GameKeyPath);
		FFileHelper::LoadFileToString(ResolvedSecretKey, *SecretKeyPath);
		ResolvedGameKey.TrimStartAndEndInline();
		ResolvedSecretKey.TrimStartAndEndInline();
		KeySource = FString::Printf(TEXT("прочитаны из файлов папки '%s'"), *KeysFolder);
	}

	if (ResolvedGameKey.IsEmpty() || ResolvedSecretKey.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Analytics: КЛЮЧИ НЕ НАЙДЕНЫ - в сборку не вшиты (CONTRARY_GA_KEYS_COMPILED_IN=%d) и файлов GameKey.txt/SecretKey.txt нет в '%s'. Аналитика выключена, события не отправляются."),
			static_cast<int32>(CONTRARY_GA_KEYS_COMPILED_IN), *KeysFolder);
		return false;
	}

	// Значения ключей в журнал НЕ выводим никогда — только источник и длины (диагностика формата).
	UE_LOG(LogTemp, Display, TEXT("Analytics: ключи найдены (%s), длины %d/%d символов."),
		*KeySource, ResolvedGameKey.Len(), ResolvedSecretKey.Len());
	return true;
}

void UAnalyticsSubsystem::StartSdkAndEnable()
{
#if WITH_GAMEANALYTICS
	if (ResolvedGameKey.IsEmpty() || ResolvedSecretKey.IsEmpty())
	{
		return;
	}

	// GA-синглтон живёт на процесс: повторный Initialize (второй PIE-запуск в той же сессии
	// редактора) SDK не нужен — просто включаем отправку у уже настроенного инстанса.
	static bool bSDKInitializedThisProcess = false;
	if (UGameAnalytics* GA = UGameAnalytics::GetInstance())
	{
		if (!bSDKInitializedThisProcess)
		{
			UE_LOG(LogTemp, Display, TEXT("Analytics: initializing GameAnalytics"));
			GA->ConfigureAutoDetectAppVersion(true);
			GA->Initialize(ResolvedGameKey, ResolvedSecretKey); // старт сессии SDK делает сам
			bSDKInitializedThisProcess = true;
		}
		bEnabled = true;

		// Б4: самый первый запуск игры на устройстве — ровно один раз за установку. Если
		// согласие дано не сразу, событие уходит именно сейчас, а не теряется.
		RecordFirstLaunchIfNeeded();
	}
#endif
}

EDataConsentState UAnalyticsSubsystem::GetStoredConsentState()
{
	const UAnalyticsProfileSave* Save = LoadOrCreateProfileSave();
	return Save ? Save->ConsentState : EDataConsentState::Unknown;
}

void UAnalyticsSubsystem::SetDataConsent(bool bGranted)
{
	UAnalyticsProfileSave* Save = LoadOrCreateProfileSave();
	if (Save)
	{
		Save->ConsentState = bGranted ? EDataConsentState::Accepted : EDataConsentState::Declined;
		WriteProfileSave(Save);
	}

	if (bGranted)
	{
		StartSdkAndEnable();
		UE_LOG(LogTemp, Display, TEXT("Analytics: игрок дал согласие - статистика включена"));
	}
	else
	{
		// Уже поднятый SDK погасить нельзя, но отправку прекращаем немедленно: все события
		// проходят через SendDesignEvent, а он смотрит на этот признак.
		bEnabled = false;
		UE_LOG(LogTemp, Display, TEXT("Analytics: игрок отказал - статистика ничего не отправляет"));
	}
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

void UAnalyticsSubsystem::RecordAdStage(const FString& Point, const FString& Stage,
	float Value, bool bWithValue)
{
	SendDesignEvent(FString::Printf(TEXT("ad:%s:%s"),
		*SanitizeEventPart(Point), *SanitizeEventPart(Stage)), Value, bWithValue);
}

void UAnalyticsSubsystem::RecordAdNotShown(const FString& Point, const FString& Reason)
{
	SendDesignEvent(FString::Printf(TEXT("ad:%s:not_shown:%s"),
		*SanitizeEventPart(Point), *SanitizeEventPart(Reason)));
}

// --- Окно «Поддержать автора» (задание издателя, решение Рината 11.08.2026) ---

FString UAnalyticsSubsystem::MakeSupportWindowOpenedEventId(const FString& Source)
{
	return FString::Printf(TEXT("support:window_opened:%s"), *SanitizeEventPart(Source));
}

FString UAnalyticsSubsystem::MakeSupportAdStartedEventId()
{
	return TEXT("support:ad_started");
}

FString UAnalyticsSubsystem::MakeSupportAdCompletedEventId()
{
	return TEXT("support:ad_completed");
}

FString UAnalyticsSubsystem::MakeSupportLinkOpenedEventId()
{
	return TEXT("support:link_opened");
}

void UAnalyticsSubsystem::RecordSupportWindowOpened(const FString& Source)
{
	SendDesignEvent(MakeSupportWindowOpenedEventId(Source));
}

void UAnalyticsSubsystem::RecordSupportAdStarted()
{
	SendDesignEvent(MakeSupportAdStartedEventId());
}

void UAnalyticsSubsystem::RecordSupportAdCompleted()
{
	SendDesignEvent(MakeSupportAdCompletedEventId());
}

void UAnalyticsSubsystem::RecordSupportLinkOpened()
{
	SendDesignEvent(MakeSupportLinkOpenedEventId());
}

void UAnalyticsSubsystem::RecordShopSellCompleted(float Amount)
{
	SendDesignEvent(TEXT("shop:sell_completed"), Amount, /*bWithValue=*/true);
}

void UAnalyticsSubsystem::RecordDailyRewardClaimed(int32 StreakDay)
{
	SendDesignEvent(TEXT("retention:daily_reward_claimed"),
		static_cast<float>(StreakDay), /*bWithValue=*/true);
}

void UAnalyticsSubsystem::RecordTutorialStep(const FString& StepId, int32 StepIndex)
{
	if (!bEnabled)
	{
		return; // без ключей/плагина ни события, ни записи служебного слота
	}

	const FString SafeStepId = SanitizeEventPart(StepId);
	UAnalyticsProfileSave* Save = LoadOrCreateProfileSave();
	if (!Save || !Save->MarkTutorialStepReported(SafeStepId))
	{
		return; // этот шаг за текущую установку игры уже отправляли
	}

	SendDesignEvent(MakeTutorialStepEventId(SafeStepId), static_cast<float>(StepIndex), /*bWithValue=*/true);
	WriteProfileSave(Save);
	UE_LOG(LogTemp, Log, TEXT("Analytics: шаг обучения '%s' (номер %d) отправлен впервые за установку"),
		*SafeStepId, StepIndex);
}

void UAnalyticsSubsystem::RecordTutorialCompleted(int32 TotalSteps)
{
	if (!bEnabled)
	{
		return;
	}

	UAnalyticsProfileSave* Save = LoadOrCreateProfileSave();
	if (!Save || !Save->MarkTutorialCompletedReported())
	{
		return; // завершение обучения за эту установку уже отправляли
	}

	SendDesignEvent(MakeTutorialCompletedEventId(), static_cast<float>(TotalSteps), /*bWithValue=*/true);
	WriteProfileSave(Save);
	UE_LOG(LogTemp, Log, TEXT("Analytics: обучение пройдено целиком (%d шагов) - событие отправлено"),
		TotalSteps);
}

bool UAnalyticsSubsystem::MarkLaunchAndCheckWasLaunchedBefore()
{
	UAnalyticsProfileSave* Save = LoadOrCreateProfileSave();
	if (!Save)
	{
		// Память на установку не читается (крайний случай) — считаем запуск повторным:
		// лишнее меню безобиднее, чем пропуск меню у игрока со стажем.
		return true;
	}
	if (Save->MarkGameLaunched())
	{
		WriteProfileSave(Save);
		return false; // самый первый запуск после установки
	}
	return true;
}

void UAnalyticsSubsystem::RecordFirstLaunchIfNeeded()
{
	UAnalyticsProfileSave* Save = LoadOrCreateProfileSave();
	if (!Save || !Save->MarkFirstLaunchReported())
	{
		return; // игра на этом устройстве уже запускалась
	}

	SendDesignEvent(MakeFirstLaunchEventId());
	WriteProfileSave(Save);
	UE_LOG(LogTemp, Display, TEXT("Analytics: первый запуск игры на устройстве - отправлено '%s'"),
		*MakeFirstLaunchEventId());
}

FString UAnalyticsSubsystem::MakeFirstLaunchEventId()
{
	return TEXT("app:first_launch");
}

FString UAnalyticsSubsystem::MakeTutorialStepEventId(const FString& StepId)
{
	return FString::Printf(TEXT("tutorial:step:%s"), *SanitizeEventPart(StepId));
}

FString UAnalyticsSubsystem::MakeTutorialCompletedEventId()
{
	return TEXT("tutorial:completed");
}

bool UAnalyticsSubsystem::AreKeysCompiledIn()
{
#if CONTRARY_GA_KEYS_COMPILED_IN
	return true;
#else
	return false;
#endif
}

int32 UAnalyticsSubsystem::GetCompiledGameKeyLength()
{
#if CONTRARY_GA_KEYS_COMPILED_IN
	return FCString::Strlen(TEXT(CONTRARY_GA_GAME_KEY));
#else
	return 0;
#endif
}

int32 UAnalyticsSubsystem::GetCompiledSecretKeyLength()
{
#if CONTRARY_GA_KEYS_COMPILED_IN
	return FCString::Strlen(TEXT(CONTRARY_GA_SECRET_KEY));
#else
	return 0;
#endif
}

UAnalyticsProfileSave* UAnalyticsSubsystem::LoadOrCreateProfileSave()
{
	if (!ProfileSave)
	{
		ProfileSave = UAnalyticsProfileSave::LoadOrCreate();
	}
	return ProfileSave;
}

void UAnalyticsSubsystem::WriteProfileSave(UAnalyticsProfileSave* Save)
{
	if (Save && !UAnalyticsProfileSave::Write(Save))
	{
		UE_LOG(LogTemp, Warning, TEXT("Analytics: не удалось записать служебный слот '%s'"),
			UAnalyticsProfileSave::GetSlotName());
	}
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
