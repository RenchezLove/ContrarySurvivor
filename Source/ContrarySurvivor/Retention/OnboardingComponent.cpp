// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Retention/OnboardingComponent.h"
#include "ContrarySurvivor/UI/GameScreenLayer.h" // окна — в область игры (сжимается под баннер)
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h" // Б4: события шагов обучения
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // слот OnboardingHintWidgetClass (ТЗ 08-07)
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
			bShown[static_cast<int32>(EOnboardingHint::LeaveVillage)] = Save->bHintLeaveVillageShown;
			bShown[static_cast<int32>(EOnboardingHint::Attack)]       = Save->bHintAttackShown;
			bShown[static_cast<int32>(EOnboardingHint::WeaponSwitch)] = Save->bHintWeaponSwitchShown;
			bShown[static_cast<int32>(EOnboardingHint::Armor)]        = Save->bHintArmorShown;
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
	// Пустой текст (подсказка старосты, первое открытие сумки — решения Рината 07.10.2026)
	// на экран не выводится, но признак и событие шага ниже уходят в тот же момент, что раньше.
	ShowWidget(GetHintText(Hint, IsTouchLayerActive()));

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
		case EOnboardingHint::LeaveVillage: return TEXT("leave_village");
		case EOnboardingHint::Attack:       return TEXT("attack");
		case EOnboardingHint::WeaponSwitch: return TEXT("weapon_switch");
		case EOnboardingHint::Armor:        return TEXT("armor");
		default:                         return TEXT("unknown");
	}
}

bool UOnboardingComponent::CountsTowardTutorialCompletion(EOnboardingHint Hint)
{
	// Все реальные шаги, кроме подсказки смерти: завершение обучения не должно требовать
	// первой гибели игрока (иначе метрика издателя меряет смертность, а не обучение).
	// ADR-074: подсказка выхода из деревни тоже не входит — условие завершения обязано
	// остаться теми же четырьмя шагами, что в выпущенной 0.1.0 (1), иначе показатель
	// издателя перестанет быть сравнимым между версиями.
	// 07.10.2026: перечислены явно, а не «все, кроме»: подсказки про удар, смену оружия и
	// броню, добавленные в конец перечисления, в условие НЕ входят — шагов по-прежнему четыре.
	return Hint == EOnboardingHint::Movement
		|| Hint == EOnboardingHint::Pickup
		|| Hint == EOnboardingHint::Elder
		|| Hint == EOnboardingHint::Inventory;
}

bool UOnboardingComponent::IsHintShown(EOnboardingHint Hint) const
{
	const int32 Index = static_cast<int32>(Hint);
	return Index >= 0 && Index < static_cast<int32>(EOnboardingHint::Count) && bShown[Index];
}

bool UOnboardingComponent::ShouldShowAttackHint(float DistanceToEnemy, float TriggerDistance)
{
	return DistanceToEnemy >= 0.0f && DistanceToEnemy <= TriggerDistance;
}

bool UOnboardingComponent::ShouldShowWeaponSwitchHint(bool bHasMelee, bool bHasRanged)
{
	return bHasMelee && bHasRanged;
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
	// Ввод гасит только надпись, которая уже провисела свой минимум: иначе на телефоне, где
	// палец почти всегда на экране, очередь подсказок пролистывалась бы мгновенно.
	if (!HintQueue.MayDismissByInput(GetQueueTime(), MinShowBeforeInputDismiss))
	{
		return;
	}
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

FText UOnboardingComponent::GetHintText(EOnboardingHint Hint, bool bTouch) const
{
	// Тексты — EditAnywhere-поля компонента (директива Рината 07-18); дефолты в заголовке.
	// У движения, подбора и смены оружия вариант зависит от управления: клавиши на ПК,
	// экранные кнопки и палец на телефоне (см. комментарий к полям в заголовке).
	switch (Hint)
	{
		case EOnboardingHint::Movement:  return bTouch ? HintTextMovementTouch : HintTextMovement;
		case EOnboardingHint::Pickup:    return bTouch ? HintTextPickupTouch : HintTextPickup;
		// Подсказку старосты повторяют верхняя строка задачи и надпись у кнопки — текста нет.
		case EOnboardingHint::Elder:     return bShowElderHint ? HintTextElder : FText::GetEmpty();
		// Первое открытие сумки — шаг без текста; текст про броню ушёл в Armor.
		case EOnboardingHint::Inventory: return FText::GetEmpty();
		case EOnboardingHint::Attack:    return HintTextAttack;
		case EOnboardingHint::WeaponSwitch: return bTouch ? HintTextWeaponSwitchTouch : HintTextWeaponSwitch;
		case EOnboardingHint::Armor:     return HintTextInventory;
		case EOnboardingHint::Death:     return HintTextDeath; // СТРОГО ADR-044 п.3 (см. заголовок)
		case EOnboardingHint::LeaveVillage: return HintTextLeaveVillage; // ADR-074, управления не называет
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
		case EOnboardingHint::LeaveVillage: Save->bHintLeaveVillageShown = true; break;
		case EOnboardingHint::Attack:       Save->bHintAttackShown = true;       break;
		case EOnboardingHint::WeaponSwitch: Save->bHintWeaponSwitchShown = true; break;
		case EOnboardingHint::Armor:        Save->bHintArmorShown = true;        break;
		default: return;
	}
	Player->WriteSaveObject(Save);
}

double UOnboardingComponent::GetQueueTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UOnboardingComponent::ShowWidget(const FText& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}

	// Решение Рината 07.10.2026: «Одна подсказка за раз. Следующая ждёт, пока предыдущая уйдёт,
	// и не накладывается». Раньше новая надпись молча затирала показанную.
	switch (HintQueue.Offer(Text, GetQueueTime()))
	{
		case EOnboardingHintOffer::ShowNow:
			if (!PresentHint(Text))
			{
				// Показать некому (нет контроллера/виджета) — как и раньше, надпись пропадает;
				// очередь не держим занятой, иначе она встала бы навсегда.
				HintQueue.Reset();
			}
			break;
		case EOnboardingHintOffer::RefreshCurrent:
			// Та же надпись уже на экране (повторный отказ торговца) — продлеваем показ.
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(HideTimer, this,
					&UOnboardingComponent::HideActiveWidget, FMath::Max(HintDuration, 1.0f), false);
			}
			break;
		case EOnboardingHintOffer::Queued:
		default:
			break; // дождётся своей очереди (HideActiveWidget -> пауза -> ShowNextQueuedHint)
	}
}

bool UOnboardingComponent::PresentHint(const FText& Text)
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!PC)
	{
		return false;
	}

	// Один переиспользуемый тост.
	if (!ActiveWidget)
	{
		// ТЗ Рината 08-07: слот WBP-класса на HUD (ADR-048). Назначен — тост живёт на
		// дереве владельца из дизайнера; пуст — прежний кодовый вид.
		UClass* HintClass = UOnboardingHintWidget::StaticClass();
		if (const AContrarySurvivorHUD* Hud = Cast<AContrarySurvivorHUD>(PC->GetHUD()))
		{
			if (Hud->OnboardingHintWidgetClass)
			{
				HintClass = Hud->OnboardingHintWidgetClass;
			}
		}
		ActiveWidget = CreateWidget<UOnboardingHintWidget>(PC, HintClass);
		if (!ActiveWidget)
		{
			return false;
		}
		ActiveWidget->ApplyStyle(HintStyle); // стиль с компонента (EditAnywhere) поверх дефолтов
	}
	ActiveWidget->SetHintText(Text);
	if (!ActiveWidget->IsInViewport())
	{
		ContraryUI::AddToGameScreen(ActiveWidget, /*ZOrder=*/40);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimer, this,
			&UOnboardingComponent::HideActiveWidget, FMath::Max(HintDuration, 1.0f), false);
	}
	return true;
}

void UOnboardingComponent::HideActiveWidget()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
	if (ActiveWidget && ActiveWidget->IsInViewport())
	{
		ActiveWidget->RemoveFromParent();
	}

	if (!HintQueue.IsShowing())
	{
		return; // прятать было нечего — очередь не трогаем
	}
	HintQueue.MarkHidden();

	// В очереди есть следующая — показываем её после короткой паузы, а не встык.
	if (HintQueue.IsWaitingGap())
	{
		if (World)
		{
			World->GetTimerManager().SetTimer(GapTimer, this,
				&UOnboardingComponent::ShowNextQueuedHint, FMath::Max(HintGapSeconds, 0.01f), false);
		}
		else
		{
			ShowNextQueuedHint();
		}
	}
}

void UOnboardingComponent::ShowNextQueuedHint()
{
	FText Next;
	if (!HintQueue.TakeNext(GetQueueTime(), Next))
	{
		return;
	}
	if (!PresentHint(Next))
	{
		HintQueue.Reset();
	}
}
