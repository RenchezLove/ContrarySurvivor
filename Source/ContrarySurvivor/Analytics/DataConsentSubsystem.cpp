// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Analytics/DataConsentSettings.h"
#include "ContrarySurvivor/Ads/YandexAdService.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // слот ConsentWidgetClass (ТЗ 08-07)
#include "ContrarySurvivor/UI/ConsentScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

namespace DataConsentLocal
{
	// Как часто проверяем, готов ли экран показать окно согласия. Ожидание короткое:
	// вьюпорт и контроллер игрока появляются в первые доли секунды после старта карты.
	static constexpr float WaitStepSeconds = 0.25f;

	// Экран согласия должен лежать ВЫШЕ всех прочих окон, включая стартовый экран (Z=70).
	static constexpr int32 ConsentZOrder = 90;

	// Сколько шагов ждём готовности экрана, прежде чем сдаться (примерно полминуты).
	// Страховка от бесконечного ожидания там, где экрана нет вовсе.
	static constexpr int32 MaxWaitSteps = 120;
}

void UDataConsentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Память с ответом игрока живёт в подсистеме статистики (там же лежит её слот), поэтому
	// она обязана быть готова раньше нас.
	Collection.InitializeDependency<UAnalyticsSubsystem>();

	// Рекламная служба обязана быть готова раньше нас ПО ТОЙ ЖЕ ПРИЧИНЕ: при сохранённом
	// ответе игрока ApplyConsentToServices зовётся прямо отсюда, и без этой строки порядок
	// создания подсистем случаен — GetSubsystem<UYandexAdService> в повторных запусках
	// возвращал пусто, согласие тихо пропадало и SDK не поднимался никогда (дефект с
	// телефона 08-08: реклама работала только в первой сессии после установки).
	Collection.InitializeDependency<UYandexAdService>();

	const EDataConsentState State = GetConsentState();
	if (State != EDataConsentState::Unknown)
	{
		// Игрок уже отвечал в прошлые запуски — просто применяем его решение.
		UE_LOG(LogTemp, Display, TEXT("Consent: сохранённый ответ игрока - %s"),
			State == EDataConsentState::Accepted ? TEXT("согласие") : TEXT("отказ"));
		ApplyConsentToServices(State == EDataConsentState::Accepted);
		return;
	}

	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	if (Settings && !Settings->bAskConsentOnFirstLaunch)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Consent: экран согласия выключен в настройках проекта - согласия нет, статистика молчит"));
		ApplyConsentToServices(false);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("Consent: игрок ещё не отвечал - ждём готовности экрана, чтобы спросить"));

	// Показать окно прямо здесь нельзя: на этом шаге ещё нет ни вьюпорта, ни контроллера
	// игрока. Ждём их короткими шагами и показываем при первой возможности. Тикер движка
	// принимает именно функцию (UE 5.5, Ticker.h:56), поэтому оборачиваем вызов лямбдой —
	// тот же приём, что в UYandexAdService.
	TWeakObjectPtr<UDataConsentSubsystem> WeakThis(this);
	WaitHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("DataConsentWait"),
		DataConsentLocal::WaitStepSeconds,
		[WeakThis](float DeltaTime) -> bool
		{
			UDataConsentSubsystem* Self = WeakThis.Get();
			return Self ? Self->TryShowConsentScreen(DeltaTime) : false;
		});
}

void UDataConsentSubsystem::Deinitialize()
{
	if (WaitHandle.IsValid())
	{
		FTSTicker::RemoveTicker(WaitHandle);
		WaitHandle.Reset();
	}
	CloseConsentScreen();

	Super::Deinitialize();
}

UDataConsentSubsystem* UDataConsentSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDataConsentSubsystem>() : nullptr;
}

EDataConsentState UDataConsentSubsystem::GetConsentState() const
{
	UGameInstance* GI = GetGameInstance();
	UAnalyticsSubsystem* Analytics = GI ? GI->GetSubsystem<UAnalyticsSubsystem>() : nullptr;
	return Analytics ? Analytics->GetStoredConsentState() : EDataConsentState::Unknown;
}

void UDataConsentSubsystem::SetConsent(bool bGranted)
{
	UGameInstance* GI = GetGameInstance();
	if (UAnalyticsSubsystem* Analytics = GI ? GI->GetSubsystem<UAnalyticsSubsystem>() : nullptr)
	{
		// Запись решения и включение/выключение статистики — одним местом (там лежит слот).
		Analytics->SetDataConsent(bGranted);
	}

	ApplyConsentToServices(bGranted);
}

void UDataConsentSubsystem::ApplyConsentToServices(bool bGranted)
{
	UGameInstance* GI = GetGameInstance();
	if (UYandexAdService* Ads = GI ? GI->GetSubsystem<UYandexAdService>() : nullptr)
	{
		Ads->ApplyUserConsent(bGranted);
	}
}

bool UDataConsentSubsystem::TryShowConsentScreen(float /*DeltaTime*/)
{
	if (ConsentWidget)
	{
		return false; // уже показали, ожидание больше не нужно
	}

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !GI->GetGameViewportClient())
	{
		if (++WaitSteps >= DataConsentLocal::MaxWaitSteps)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Consent: экран так и не появился - спросить согласие не удалось, статистика остаётся молчать"));
			return false; // прекращаем ожидание, согласия по-прежнему нет
		}
		return true; // ещё не готовы — ждём следующий шаг
	}

	// ТЗ Рината 08-07: слот WBP-класса на HUD (ADR-048). Назначен — экран согласия живёт
	// на дереве владельца из дизайнера; пуст — прежний кодовый вид. Тексты согласия в обоих
	// путях идут из настроек проекта (дословно из источника истины, издатель проверяет).
	UClass* ConsentClass = UConsentScreenWidget::StaticClass();
	if (const AContrarySurvivorHUD* Hud = Cast<AContrarySurvivorHUD>(PC->GetHUD()))
	{
		if (Hud->ConsentWidgetClass)
		{
			ConsentClass = Hud->ConsentWidgetClass;
		}
	}
	ConsentWidget = CreateWidget<UConsentScreenWidget>(PC, ConsentClass);
	if (!ConsentWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Consent: не удалось создать экран согласия"));
		return false;
	}

	if (const UDataConsentSettings* Settings = UDataConsentSettings::Get())
	{
		ConsentWidget->ApplyStyle(Settings->ConsentScreenStyle);
	}
	ConsentWidget->OnAccepted.AddUObject(this, &UDataConsentSubsystem::HandleAccepted);
	ConsentWidget->OnDeclined.AddUObject(this, &UDataConsentSubsystem::HandleDeclined);
	ConsentWidget->OnPolicyRequested.AddUObject(this, &UDataConsentSubsystem::HandlePolicyRequested);
	ConsentWidget->AddToViewport(DataConsentLocal::ConsentZOrder);

	// Пока игрок не ответил, за спиной у экрана ничего происходить не должно. Если пауза уже
	// стояла (стартовый экран «Продолжить»/«Новая игра»), после ответа её не снимаем.
	bWasPausedBefore = UGameplayStatics::IsGamePaused(PC);
	if (!bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, true);
	}

	// Управление — тот же набор, что у стартового экрана и меню паузы (OpenStartScreen):
	// мышь не запирается во вьюпорте и не прячется, на телефоне работает палец.
	bCursorShownBefore = PC->bShowMouseCursor;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;

	UE_LOG(LogTemp, Display, TEXT("Consent: экран согласия показан (первый запуск)"));
	return false; // ожидание закончено
}

void UDataConsentSubsystem::CloseConsentScreen()
{
	if (!ConsentWidget)
	{
		return;
	}

	if (ConsentWidget->IsInViewport())
	{
		ConsentWidget->RemoveFromParent();
	}
	ConsentWidget = nullptr;

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	if (!bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, false);
	}

	// Управление возвращаем в том виде, в каком оно было до экрана. Если пауза осталась
	// (под нами ещё открыт стартовый экран), окну по-прежнему нужны нажатия — оставляем
	// смешанный режим, иначе возвращаем игре чистый игровой ввод.
	if (UGameplayStatics::IsGamePaused(PC))
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = bCursorShownBefore;
	}
}

void UDataConsentSubsystem::HandleAccepted()
{
	UE_LOG(LogTemp, Display, TEXT("Consent: игрок нажал «Принимаю»"));
	SetConsent(true);
	CloseConsentScreen();
}

void UDataConsentSubsystem::HandleDeclined()
{
	UE_LOG(LogTemp, Display, TEXT("Consent: игрок нажал «Не сейчас»"));
	SetConsent(false);
	CloseConsentScreen();
}

void UDataConsentSubsystem::HandlePolicyRequested()
{
	OpenPrivacyPolicy();
}

bool UDataConsentSubsystem::HasPrivacyPolicyUrl() const
{
	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	return Settings && !Settings->PrivacyPolicyUrl.TrimStartAndEnd().IsEmpty();
}

void UDataConsentSubsystem::OpenPrivacyPolicy() const
{
	if (!HasPrivacyPolicyUrl())
	{
		// Ссылки пока нет — молчим. Пустую страницу игроку не показываем (указание game-lead).
		UE_LOG(LogTemp, Display,
			TEXT("Consent: адрес политики не задан в настройках проекта - открывать нечего"));
		return;
	}

	const FString Url = UDataConsentSettings::Get()->PrivacyPolicyUrl.TrimStartAndEnd();
	FString Error;
	FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
	if (Error.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("Consent: открыта политика конфиденциальности"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Consent: не удалось открыть политику - %s"), *Error);
	}
}

FText UDataConsentSubsystem::GetBuildVersionText() const
{
	// Версию берём из тех же настроек Android, что уходят в магазин
	// (Config/DefaultEngine.ini, [/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]).
	FString VersionName;
	FString StoreVersion;
	GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"),
		TEXT("VersionDisplayName"), VersionName, GEngineIni);
	GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"),
		TEXT("StoreVersion"), StoreVersion, GEngineIni);
	if (VersionName.IsEmpty())
	{
		VersionName = TEXT("1.0.0");
	}
	if (StoreVersion.IsEmpty())
	{
		StoreVersion = TEXT("1");
	}

	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	const FText Format = Settings ? Settings->PauseMenuVersionFormat
		: NSLOCTEXT("DataConsentSubsystem", "VersionFallback", "Версия {Version} (сборка {Build})");

	FFormatNamedArguments Args;
	Args.Add(TEXT("Version"), FText::FromString(VersionName));
	Args.Add(TEXT("Build"), FText::FromString(StoreVersion));
	return FText::Format(Format, Args);
}
