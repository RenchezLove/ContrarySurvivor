// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Retention/DailyRewardComponent.h"
#include "ContrarySurvivor/Retention/DailyRewardLogic.h"
#include "ContrarySurvivor/Ads/AdService.h"      // Build 1.2: «Забрать вдвое больше» (ТЗ №3)
#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h" // F3: событие ежедневного входа
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/ContrarySurvivor.h"   // LogQA
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // слот DailyRewardWidgetClass (ТЗ 08-07)
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/DailyRewardWidget.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Engine/World.h"

UDailyRewardComponent::UDailyRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDailyRewardComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EvaluateTimer, this,
			&UDailyRewardComponent::EvaluateOrDefer, FMath::Max(ShowWindowDelay, 0.01f), false);
	}
}

void UDailyRewardComponent::EvaluateOrDefer()
{
	// Build 1 (решение Рината 07-24): интро первой встречи ещё впереди (крючок не показан) —
	// окно награды портило бы атмосферу интро. Откладываем ВСЮ проверку (не только окно):
	// начислить молча и показать окно позже без начисления нельзя — начисление и окно идут
	// одним шагом EvaluateDailyReward.
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player)
	{
		if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			if (!Save->bElderHookShown)
			{
				bAwaitingElderDialog = true;
				UE_LOG(LogTemp, Log,
					TEXT("DailyReward: intro pending, window deferred until elder dialog closes"));
				return;
			}
		}
	}
	EvaluateDailyReward();
}

void UDailyRewardComponent::NotifyElderDialogClosed()
{
	if (!bAwaitingElderDialog)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EvaluateTimer, this,
			&UDailyRewardComponent::HandleDeferredShow, FMath::Max(PostIntroShowDelay, 0.01f), false);
	}
}

void UDailyRewardComponent::HandleDeferredShow()
{
	// За время задержки игрок успел открыть другое модальное окно (или умер — экран смерти) —
	// поверх не лезем, но и не бросаем: повторяем попытку сами по таймеру. Ждать СЛЕДУЮЩЕГО
	// диалога старосты нельзя — его может не быть, и баннер терялся (живой PIE 07-24:
	// инвентарь открыт через секунду после интро — показ отменился навсегда).
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	AContrarySurvivorPlayerController* PC = Player
		? Cast<AContrarySurvivorPlayerController>(Player->GetController()) : nullptr;
	if (!PC || PC->IsAnyModalUIOpen())
	{
		const float RetryIn = FMath::Max(DeferredRetryDelay, 0.1f);
		UE_LOG(LogTemp, Log, TEXT("DailyReward: deferred window postponed (%s), retry in %.1f s"),
			PC ? TEXT("modal UI open") : TEXT("no controller"), RetryIn);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(EvaluateTimer, this,
				&UDailyRewardComponent::HandleDeferredShow, RetryIn, false);
		}
		return;
	}
	bAwaitingElderDialog = false;
	EvaluateDailyReward();
}

void UDailyRewardComponent::EvaluateDailyReward()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !Player->GetStats())
	{
		return;
	}

	UContrarySaveGame* Save = Player->LoadOrCreateSaveObject();
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("DailyReward: save object unavailable, skip"));
		return;
	}

	const DailyReward::FComputeResult Result = DailyReward::Compute(FDateTime::Now(),
		Save->LastDailyRewardDate, Save->DailyStreakDays, BaseReward, RewardStepPerDay, MaxReward);

	if (!Result.bGrant)
	{
		UE_LOG(LogTemp, Log, TEXT("DailyReward: same calendar day (streak %d), no reward"),
			Save->DailyStreakDays);
		return;
	}

	// Контроллер и окно готовим ДО начисления и записи даты: показ не состоялся — день в
	// сейве НЕ помечен выданным, награда не сгорает молча (защита по ADR-051 п.2).
	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(Player->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("DailyReward: no player controller, day NOT consumed"));
		return;
	}

	// ТЗ Рината 08-07: слот WBP-класса на HUD (архитектура ADR-048). Назначен — окно живёт
	// на дереве владельца из дизайнера; пуст — прежний кодовый вид (BuildCodeTree).
	UClass* WindowClass = UDailyRewardWidget::StaticClass();
	if (const AContrarySurvivorHUD* Hud = Cast<AContrarySurvivorHUD>(PC->GetHUD()))
	{
		if (Hud->DailyRewardWidgetClass)
		{
			WindowClass = Hud->DailyRewardWidgetClass;
		}
	}
	ActiveWindow = CreateWidget<UDailyRewardWidget>(PC, WindowClass);
	if (!ActiveWindow)
	{
		UE_LOG(LogTemp, Warning, TEXT("DailyReward: window creation failed, day NOT consumed"));
		return;
	}

	// Build 1.2 (ТЗ №3 раздел 2): «первый день» = самый первый вход профиля после
	// установки (входов ещё не было вовсе) — в этот день кнопку удвоения не показываем.
	// Снимок ДО записи новой даты в сейв.
	const bool bFirstEverDay = (Save->LastDailyRewardDate.GetTicks() == 0 && Save->DailyStreakDays <= 0);

	// Начисляем в живые статы и зеркалим в сейв: загрузка сейва при смерти перетирает деньги
	// значением из слота — без зеркала награда терялась бы при первой же смерти.
	Player->GetStats()->AddMoney(Result.Reward);
	if (Save->bHasData)
	{
		Save->Money += Result.Reward;
	}
	Save->LastDailyRewardDate = FDateTime::Now().GetDate();
	Save->DailyStreakDays = Result.NewStreak;
	Player->WriteSaveObject(Save);

	UE_LOG(LogTemp, Log, TEXT("DailyReward: day %d of streak -> +%.0f coins (persisted)"),
		Result.NewStreak, Result.Reward);

	UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this);
	if (Analytics)
	{
		// F3 (ADR-038): «ежедневный вход» (value = день серии). Без ключей — no-op.
		Analytics->RecordDailyLogin(Result.NewStreak);
		// Build 1.2 (ТЗ №3 п.6): daily_reward_claimed для ВСЕХ получений — по нему издатель
		// считает возврат на 2/3/7 день.
		Analytics->RecordDailyRewardClaimed(Result.NewStreak);
	}

	// Окно «Ежедневная награда». Курсор в игре и так виден; режим GameAndUI — чтобы кнопка
	// ловила клик (паттерн модалок контроллера: диалог/магазин).
	ActiveWindow->ApplyStyle(WindowStyle); // стиль с компонента (EditAnywhere) поверх дефолтов
	ActiveWindow->SetupContent(Result.NewStreak, Result.Reward);
	ActiveWindow->OnClosed.AddUObject(this, &UDailyRewardComponent::HandleWindowClosed);

	// --- Build 1.2: условия показа золотой кнопки (ТЗ №3 п.4): не первый день профиля,
	// 15 минут суммарного игрового времени, ролик готов. Награда дня «ещё не забрана» —
	// база начислена только что, удвоение доступно, пока баннер открыт. Не выполнено —
	// кнопки просто нет (обычная «Забрать» — всегда).
	GrantedReward = Result.Reward;
	GrantedStreak = Result.NewStreak;
	bDoubleAdInProgress = false;
	bDoubleClaimed = false;

	IAdService* Ads = AdService::Get(this);
	FString DenyReason;
	if (bFirstEverDay)
	{
		DenyReason = TEXT("first_day");
	}
	// Порог теперь EditAnywhere на игроке (Build 1.2.1 В1: 360 с вместо константы 15 мин).
	else if (!AdGating::IsPlaytimeGatePassed(Player->GetTotalPlayTimeSeconds(), Player->GetAdMinPlaytimeSeconds()))
	{
		DenyReason = TEXT("under_15min");
	}
	else if (!Ads || !Ads->IsRewardedReady())
	{
		DenyReason = TEXT("no_ad");
	}
	const bool bShowDouble = DenyReason.IsEmpty();
	ActiveWindow->SetupDoubleOffer(Result.Reward, bShowDouble);
	ActiveWindow->OnDoubleRequested.AddUObject(this, &UDailyRewardComponent::HandleDoubleRequested);
	if (Analytics)
	{
		if (bShowDouble)
		{
			// value = день серии; размер базовой награды — в лог QA (GA несёт одно число).
			Analytics->RecordAdStage(TEXT("daily"), TEXT("button_shown"),
				static_cast<float>(Result.NewStreak), /*bWithValue=*/true);
		}
		else
		{
			Analytics->RecordAdNotShown(TEXT("daily"), DenyReason);
		}
	}
	UE_LOG(LogQA, Display, TEXT("QA: DAILY x2 button %s%s%s (day %d, base %.0f)"),
		bShowDouble ? TEXT("SHOWN") : TEXT("hidden"),
		bShowDouble ? TEXT("") : TEXT(" reason="), *DenyReason,
		Result.NewStreak, Result.Reward);

	ActiveWindow->AddToViewport(/*ZOrder=*/50);

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
}

void UDailyRewardComponent::HandleWindowClosed()
{
	ActiveWindow = nullptr;

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	AContrarySurvivorPlayerController* PC = Player
		? Cast<AContrarySurvivorPlayerController>(Player->GetController()) : nullptr;
	if (PC && !PC->IsAnyModalUIOpen())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = true; // курсор нужен и в игре (клик-таргетинг)
	}
}

void UDailyRewardComponent::HandleDoubleRequested()
{
	if (bDoubleAdInProgress || bDoubleClaimed || !ActiveWindow)
	{
		return;
	}

	IAdService* Ads = AdService::Get(this);
	if (!Ads || !Ads->IsRewardedReady())
	{
		// Ролик разгрузился между показом кнопки и кликом — честно прячем кнопку.
		ActiveWindow->SetupDoubleOffer(GrantedReward, /*bVisible=*/false);
		return;
	}

	UE_LOG(LogQA, Display, TEXT("QA: daily x2 button clicked (base %.0f)"), GrantedReward);
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordAdStage(TEXT("daily"), TEXT("button_clicked"));
		Analytics->RecordAdStage(TEXT("daily"), TEXT("started"));
	}

	bDoubleAdInProgress = true;
	Ads->ShowRewarded(AdPlacements::DailyDouble,
		FSimpleDelegate::CreateUObject(this, &UDailyRewardComponent::HandleDoubleAdSuccess),
		FSimpleDelegate::CreateUObject(this, &UDailyRewardComponent::HandleDoubleAdFail));
}

void UDailyRewardComponent::HandleDoubleAdSuccess()
{
	bDoubleAdInProgress = false;
	if (bDoubleClaimed)
	{
		return;
	}

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !Player->GetStats())
	{
		return;
	}

	// Начисление ДО закрытия баннера, с проверкой по факту (ТЗ №3 п.5: ежедневка — самая
	// обидная точка для потери награды). Доначисляем вторую дневную сумму (итог = двойная)
	// и зеркалим в сейв тем же путём, что базовую (иначе смерть перетёрла бы деньги слотом).
	bDoubleClaimed = true;
	Player->GetStats()->AddMoney(GrantedReward);
	if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		if (Save->bHasData)
		{
			Save->Money += GrantedReward;
			Player->WriteSaveObject(Save);
		}
	}

	const float Total = GrantedReward * 2.0f;
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		// value = начисленный итог; базовая сумма — в лог QA (GA несёт одно число).
		Analytics->RecordAdStage(TEXT("daily"), TEXT("completed"), Total, /*bWithValue=*/true);
	}
	UE_LOG(LogQA, Display, TEXT("QA: daily x2 completed - base %.0f, total %.0f (day %d)"),
		GrantedReward, Total, GrantedStreak);

	if (ActiveWindow)
	{
		ActiveWindow->ShowDoubledResult(Total);
	}
}

void UDailyRewardComponent::HandleDoubleAdFail()
{
	bDoubleAdInProgress = false;

	// Закрыт досрочно: баннер в исходном состоянии, обычная награда доступна, повторная
	// попытка разрешена (ТЗ №3 п.5) — только спокойная строка про полный просмотр.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordAdStage(TEXT("daily"), TEXT("dismissed"));
	}
	if (ActiveWindow)
	{
		ActiveWindow->ShowAdNotFinished();
	}
}
