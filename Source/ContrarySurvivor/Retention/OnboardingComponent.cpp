// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Retention/OnboardingComponent.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h" // Б4: события шагов обучения
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/OnboardingHintWidget.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h" // HasTouchLayer: клавиши или экранные кнопки в тексте подсказки
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

UOnboardingComponent::UOnboardingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOnboardingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Кэшируем флаги «показано» из сейва один раз (дальше — только память).
	if (const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			bShown[static_cast<int32>(EOnboardingHint::Movement)]  = Save->bHintMovementShown;
			bShown[static_cast<int32>(EOnboardingHint::Pickup)]    = Save->bHintPickupShown;
			bShown[static_cast<int32>(EOnboardingHint::Elder)]     = Save->bHintElderShown;
			bShown[static_cast<int32>(EOnboardingHint::Inventory)] = Save->bHintInventoryShown;
			bShown[static_cast<int32>(EOnboardingHint::Death)]     = Save->bHintDeathShown;
		}
	}

	// Стартовая подсказка движения — с небольшой задержкой после начала игры.
	if (!bShown[static_cast<int32>(EOnboardingHint::Movement)])
	{
		if (UWorld* World = GetWorld())
		{
			FTimerDelegate ShowMovement = FTimerDelegate::CreateUObject(
				this, &UOnboardingComponent::TryShowHint, EOnboardingHint::Movement);
			World->GetTimerManager().SetTimer(MovementHintTimer, ShowMovement,
				FMath::Max(MovementHintDelay, 0.01f), false);
		}
	}
}

void UOnboardingComponent::TryShowHint(EOnboardingHint Hint)
{
	const int32 Index = static_cast<int32>(Hint);
	if (Index < 0 || Index >= static_cast<int32>(EOnboardingHint::Count) || bShown[Index])
	{
		return;
	}

	bShown[Index] = true;
	PersistShownFlag(Hint);
	ShowWidget(GetHintText(Hint));

	// Б4 (задание издателя ADR-059): событие на каждый показанный шаг обучения и отдельное —
	// когда показан последний из них. Защита от повторов «раз за установку игры» и запись
	// служебной памяти живут внутри UAnalyticsSubsystem, здесь только факт показа.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordTutorialStep(GetHintAnalyticsId(Hint), Index + 1);
		// «Обучение пройдено» — когда показаны все ЗАЧИТЫВАЕМЫЕ шаги (без подсказки смерти,
		// см. CountsTowardTutorialCompletion). Сторож по текущему шагу нужен, чтобы поздняя
		// подсказка смерти не дёргала событие повторно (в UAnalyticsSubsystem есть своя
		// защита от повторов, но полагаться на один рубеж не будем).
		if (CountsTowardTutorialCompletion(Hint) && AreTutorialCompletionHintsShown())
		{
			Analytics->RecordTutorialCompleted(GetTutorialCompletionStepCount());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Onboarding: hint %d shown (once per profile)"), Index);
}

const TCHAR* UOnboardingComponent::GetHintAnalyticsId(EOnboardingHint Hint)
{
	switch (Hint)
	{
		case EOnboardingHint::Movement:  return TEXT("movement");
		case EOnboardingHint::Pickup:    return TEXT("pickup");
		case EOnboardingHint::Elder:     return TEXT("elder");
		case EOnboardingHint::Inventory: return TEXT("inventory");
		case EOnboardingHint::Death:     return TEXT("death");
		default:                         return TEXT("unknown");
	}
}

bool UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint Hint)
{
	// Все реальные шаги, кроме подсказки смерти: завершение обучения не должно требовать
	// первой гибели игрока (иначе метрика издателя меряет смертность, а не обучение).
	return Hint < EOnboardingHint::Count && Hint != EOnboardingHint::Death;
}

int32 UOnboardingComponent::GetTutorialCompletionStepCount()
{
	int32 StepCount = 0;
	for (int32 i = 0; i < static_cast<int32>(EOnboardingHint::Count); ++i)
	{
		if (CountsTowardTutorialCompletion(static_cast<EOnboardingHint>(i)))
		{
			++StepCount;
		}
	}
	return StepCount;
}

bool UOnboardingComponent::AreTutorialCompletionHintsShown() const
{
	for (int32 i = 0; i < static_cast<int32>(EOnboardingHint::Count); ++i)
	{
		if (CountsTowardTutorialCompletion(static_cast<EOnboardingHint>(i)) && !bShown[i])
		{
			return false;
		}
	}
	return true;
}

void UOnboardingComponent::CancelPendingMovementHint()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementHintTimer);
	}
}

void UOnboardingComponent::DismissCurrentHint()
{
	HideActiveWidget();
}

void UOnboardingComponent::ShowTransientHint(const FText& Text)
{
	// Тот же тост и тот же таймер автоскрытия, что у подсказок онбординга; признак
	// «показано» здесь не ведётся (см. комментарий в заголовке).
	ShowWidget(Text);
}

bool UOnboardingComponent::IsTouchLayerActive() const
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	const AContrarySurvivorPlayerController* PC =
		Player ? Cast<AContrarySurvivorPlayerController>(Player->GetController()) : nullptr;
	return PC && PC->HasTouchLayer();
}

FText UOnboardingComponent::GetHintText(EOnboardingHint Hint) const
{
	// Тексты — EditAnywhere-поля компонента (директива Рината 07-18); дефолты в заголовке.
	// У движения и подбора вариант зависит от управления: клавиши на ПК, экранные кнопки
	// и палец на телефоне (см. комментарий к полям в заголовке).
	const bool bTouch = IsTouchLayerActive();
	switch (Hint)
	{
		case EOnboardingHint::Movement:  return bTouch ? HintTextMovementTouch : HintTextMovement;
		case EOnboardingHint::Pickup:    return bTouch ? HintTextPickupTouch : HintTextPickup;
		case EOnboardingHint::Elder:     return HintTextElder;
		case EOnboardingHint::Inventory: return HintTextInventory;
		case EOnboardingHint::Death:     return HintTextDeath; // СТРОГО ADR-044 п.3 (см. заголовок)
		default:                         return FText::GetEmpty();
	}
}

void UOnboardingComponent::PersistShownFlag(EOnboardingHint Hint)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}
	UContrarySaveGame* Save = Player->LoadOrCreateSaveObject();
	if (!Save)
	{
		return;
	}

	switch (Hint)
	{
		case EOnboardingHint::Movement:  Save->bHintMovementShown = true;  break;
		case EOnboardingHint::Pickup:    Save->bHintPickupShown = true;    break;
		case EOnboardingHint::Elder:     Save->bHintElderShown = true;     break;
		case EOnboardingHint::Inventory: Save->bHintInventoryShown = true; break;
		case EOnboardingHint::Death:     Save->bHintDeathShown = true;     break;
		default: return;
	}
	Player->WriteSaveObject(Save);
}

void UOnboardingComponent::ShowWidget(const FText& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}

	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	// Один переиспользуемый тост: следующая подсказка замещает предыдущую.
	if (!ActiveWidget)
	{
		ActiveWidget = CreateWidget<UOnboardingHintWidget>(PC, UOnboardingHintWidget::StaticClass());
		if (!ActiveWidget)
		{
			return;
		}
		ActiveWidget->ApplyStyle(HintStyle); // стиль с компонента (EditAnywhere) поверх дефолтов
	}
	ActiveWidget->SetHintText(Text);
	if (!ActiveWidget->IsInViewport())
	{
		ActiveWidget->AddToViewport(/*ZOrder=*/40);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimer, this,
			&UOnboardingComponent::HideActiveWidget, FMath::Max(HintDuration, 1.0f), false);
	}
}

void UOnboardingComponent::HideActiveWidget()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
	if (ActiveWidget && ActiveWidget->IsInViewport())
	{
		ActiveWidget->RemoveFromParent();
	}
}
