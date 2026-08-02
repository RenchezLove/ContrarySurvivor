// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DeathScreenWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Ads/AdService.h"
#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AMasterInventoryItem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

void UDeathScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RespawnButton)
	{
		RespawnButton->OnClicked.AddDynamic(this, &UDeathScreenWidget::HandleRespawnClicked);
	}
	else
	{
		// Возрождение остаётся доступным клавишами Enter/Пробел (путь контроллера) — не тупик.
		UE_LOG(LogQA, Warning, TEXT("DeathScreenWidget: кубик RespawnButton не найден в WBP_Death"));
	}

	if (SaveBackpackButton)
	{
		SaveBackpackButton->OnClicked.AddDynamic(this, &UDeathScreenWidget::HandleSaveBackpackClicked);
	}
}

void UDeathScreenWidget::InitDeath(APlayerCharacter* InPlayer)
{
	Player = InPlayer;
	bAdInProgress = false;
	RefreshLossPreview();
}

void UDeathScreenWidget::RefreshLossPreview()
{
	if (!Player)
	{
		return;
	}

	// Пока экран открыт — ничего не списано (ТЗ №1 раздел 2 п.5): оба плана считаются от
	// снимка на момент смерти и показываются игроку; применяет их только выбор кнопки.
	const DeathLoss::FPlan NormalPlan = Player->ComputeDeathLossPlan(/*bBackpackRescued=*/false);
	const DeathLoss::FPlan RescuedPlan = Player->ComputeDeathLossPlan(/*bBackpackRescued=*/true);

	// --- Сетка «Будет потеряно»: первые LostItems кандидатов, сгруппированные по названию
	// (порядок инвентаря — тот же, что применит DropDeathLoss: превью не разойдётся с фактом).
	TArray<AMasterInventoryItem*> Candidates = Player->GetDeathLossCandidates();
	const int32 LostCount = FMath::Min(NormalPlan.LostItems, Candidates.Num());

	struct FLossEntry
	{
		FText Name;
		TSoftObjectPtr<UTexture2D> Icon;
		int32 Count = 0;
	};
	TArray<FLossEntry> Entries;
	for (int32 Index = 0; Index < LostCount; ++Index)
	{
		AMasterInventoryItem* Item = Candidates[Index];
		if (!IsValid(Item))
		{
			continue;
		}
		const FText Name = Item->GetItemDisplayText();
		FLossEntry* Found = Entries.FindByPredicate([&Name](const FLossEntry& E)
		{
			return E.Name.EqualTo(Name);
		});
		if (Found)
		{
			++Found->Count;
		}
		else
		{
			FLossEntry NewEntry;
			NewEntry.Name = Name;
			NewEntry.Icon = Item->GetItemIcon(); // единый геттер (вычислим у расходника/квест-предмета)
			NewEntry.Count = 1;
			Entries.Add(NewEntry);
		}
	}
	// «Самые ценные» позиции вперёд: цен у предметов нет — сортируем по количеству.
	Entries.Sort([](const FLossEntry& A, const FLossEntry& B) { return A.Count > B.Count; });

	if (LossGrid && WidgetTree)
	{
		LossGrid->ClearChildren(); // виджет переиспользуется между смертями

		const int32 ShownEntries = FMath::Min(Entries.Num(), FMath::Max(1, MaxLossPreviewEntries));
		for (int32 Index = 0; Index < ShownEntries; ++Index)
		{
			const FLossEntry& Entry = Entries[Index];

			UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

			// Иконка предмета; текстуры может не быть (мягкие ссылки, ADR-043) — тогда
			// вместо картинки название мелким шрифтом, блок обязан работать без текстур.
			UTexture2D* IconTexture = Entry.Icon.LoadSynchronous();
			if (IconTexture)
			{
				UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				FSlateBrush Brush;
				Brush.SetResourceObject(IconTexture);
				Brush.SetImageSize(FVector2D(LossIconSize, LossIconSize));
				Icon->SetBrush(Brush);
				if (UVerticalBoxSlot* IconSlot = Cell->AddChildToVerticalBox(Icon))
				{
					IconSlot->SetHorizontalAlignment(HAlign_Center);
				}
			}
			else
			{
				UTextBlock* NameBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				NameBlock->SetText(Entry.Name);
				NameBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13));
				NameBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)));
				if (UVerticalBoxSlot* NameSlot = Cell->AddChildToVerticalBox(NameBlock))
				{
					NameSlot->SetHorizontalAlignment(HAlign_Center);
				}
			}

			UTextBlock* CountBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			FFormatNamedArguments CountArgs;
			CountArgs.Add(TEXT("Count"), FText::AsNumber(Entry.Count));
			CountBlock->SetText(FText::Format(LossCountFormat, CountArgs));
			CountBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
			CountBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.8f, 0.35f, 1.0f)));
			if (UVerticalBoxSlot* CountSlot = Cell->AddChildToVerticalBox(CountBlock))
			{
				CountSlot->SetHorizontalAlignment(HAlign_Center);
			}

			// Зазор между позициями (в ассете сетка — HorizontalBox; иная панель — без отступов).
			if (UHorizontalBoxSlot* CellSlot = Cast<UHorizontalBoxSlot>(LossGrid->AddChild(Cell)))
			{
				CellSlot->SetPadding(FMargin(6.0f, 0.0f));
			}
		}

		if (LossMoreText)
		{
			const int32 HiddenEntries = Entries.Num() - ShownEntries;
			if (HiddenEntries > 0)
			{
				FFormatNamedArguments MoreArgs;
				MoreArgs.Add(TEXT("Count"), FText::AsNumber(HiddenEntries));
				LossMoreText->SetText(FText::Format(LossMoreFormat, MoreArgs));
				LossMoreText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
			else
			{
				LossMoreText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	const int32 LostMoneyInt = FMath::RoundToInt32(NormalPlan.LostMoney);
	if (LossMoneyText)
	{
		FFormatNamedArguments MoneyArgs;
		MoneyArgs.Add(TEXT("Amount"), FText::AsNumber(LostMoneyInt));
		LossMoneyText->SetText(FText::Format(LossMoneyFormat, MoneyArgs));
		LossMoneyText->SetVisibility(LostMoneyInt > 0
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Блок целиком прячется, когда терять нечего (ни предметов, ни денег).
	const bool bAnythingToLose = (LostCount > 0) || (LostMoneyInt > 0);
	if (LossPanel)
	{
		LossPanel->SetVisibility(bAnythingToLose
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Подстрока «Возродиться»: конкретные потери числами.
	if (RespawnSubText)
	{
		FFormatNamedArguments SubArgs;
		SubArgs.Add(TEXT("Items"), FText::AsNumber(LostCount));
		SubArgs.Add(TEXT("Money"), FText::AsNumber(LostMoneyInt));
		RespawnSubText->SetText(FText::Format(RespawnSubFormat, SubArgs));
	}

	// --- Условия показа «Спасти рюкзак» (ТЗ №1 п.3): ВСЕ обязаны выполниться, иначе
	// кнопка прячется целиком (никаких неактивных серых кнопок). ---
	const int32 SavedItems = FMath::Max(0, NormalPlan.LostItems - RescuedPlan.LostItems);
	const int32 SavedMoney = FMath::Max(0,
		FMath::RoundToInt32(NormalPlan.LostMoney - RescuedPlan.LostMoney));

	UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this);
	IAdService* Ads = AdService::Get(this);

	FString DenyReason;
	// Порог теперь EditAnywhere на игроке (Build 1.2.1 В1: 360 с вместо константы 15 мин).
	if (!AdGating::IsPlaytimeGatePassed(Player->GetTotalPlayTimeSeconds(), Player->GetAdMinPlaytimeSeconds()))
	{
		DenyReason = TEXT("under_15min");
	}
	else if (SavedItems <= 0 && SavedMoney <= 0)
	{
		DenyReason = TEXT("no_items"); // просмотр ничего не спасёт — выгоды нет
	}
	else if (!Ads || !Ads->IsRewardedReady())
	{
		DenyReason = TEXT("no_ad");
	}
	else if (Player->GetBackpackAdUsesToday() >= Player->GetBackpackAdDailyLimit())
	{
		DenyReason = TEXT("limit");
	}

	const bool bShowAdButton = DenyReason.IsEmpty();
	if (SaveBackpackButton)
	{
		SaveBackpackButton->SetVisibility(bShowAdButton
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SaveBackpackSubText && bShowAdButton)
	{
		// Конкретная выгода числами (ТЗ раздел 0 п.3), не «посмотреть рекламу».
		FFormatNamedArguments SubArgs;
		SubArgs.Add(TEXT("Items"), FText::AsNumber(SavedItems));
		SubArgs.Add(TEXT("Money"), FText::AsNumber(SavedMoney));
		SaveBackpackSubText->SetText(FText::Format(SaveBackpackSubFormat, SubArgs));
	}

	if (Analytics)
	{
		if (bShowAdButton)
		{
			// value = предметов под угрозой (GA несёт одно число; номер смерти — в лог QA).
			Analytics->RecordAdStage(TEXT("backpack"), TEXT("button_shown"),
				static_cast<float>(LostCount), /*bWithValue=*/true);
		}
		else
		{
			Analytics->RecordAdNotShown(TEXT("backpack"), DenyReason);
		}
	}
	UE_LOG(LogQA, Display,
		TEXT("QA: DEATH-LOSS preview - lose %d items / %d money (bag %d items / %.0f money); ad button %s%s%s (death #%d)"),
		LostCount, LostMoneyInt, NormalPlan.DroppedItems, NormalPlan.DroppedMoney,
		bShowAdButton ? TEXT("SHOWN") : TEXT("hidden"),
		bShowAdButton ? TEXT("") : TEXT(" reason="), *DenyReason,
		Player->GetDeathCountThisSession());
}

void UDeathScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Player)
	{
		return;
	}

	// Статистика последней жизни — те же источники, что Canvas DrawDeathScreen.
	if (LifetimeText)
	{
		// Минуты и секунды — с ведущим нулём, поэтому формат числа задаём явно
		// (по умолчанию FText::AsNumber ведущий ноль не рисует).
		FNumberFormattingOptions TwoDigits;
		TwoDigits.SetMinimumIntegralDigits(2);
		TwoDigits.SetUseGrouping(false);

		const float LifeSec = Player->GetLastLifeDuration();
		FFormatNamedArguments Args;
		Args.Add(TEXT("Minutes"), FText::AsNumber(FMath::FloorToInt32(LifeSec / 60.0f), &TwoDigits));
		Args.Add(TEXT("Seconds"), FText::AsNumber(FMath::FloorToInt32(LifeSec) % 60, &TwoDigits));
		LifetimeText->SetText(FText::Format(LifetimeFormat, Args));
	}
	if (KillerText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Name"), Player->GetLastDamagerName());
		KillerText->SetText(FText::Format(KillerFormat, Args));
	}
	if (MoneyText)
	{
		const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FText::AsNumber(FMath::RoundToInt32(Money)));
		MoneyText->SetText(FText::Format(MoneyFormat, Args));
	}
	if (QuestsText)
	{
		const int32 QuestsDone = Player->GetQuests() ? Player->GetQuests()->GetTurnedInQuestCount() : 0;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), FText::AsNumber(QuestsDone));
		QuestsText->SetText(FText::Format(QuestsFormat, Args));
	}
	if (KillsText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), FText::AsNumber(Player->GetEnemyKillCount()));
		KillsText->SetText(FText::Format(KillsFormat, Args));
	}
	if (MoneyLossText)
	{
		// Процент — живой из игрока (DeathMoneyLossFraction), текст не разойдётся с BP-настройкой.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Percent"),
			FText::AsNumber(FMath::RoundToInt32(Player->GetDeathMoneyLossFraction() * 100.0f)));
		MoneyLossText->SetText(FText::Format(MoneyLossFormat, Args));
	}
}

void UDeathScreenWidget::HandleRespawnClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: respawn button clicked (UMG)"));
	if (Player && !bAdInProgress)
	{
		Player->Respawn(); // тот же вызов, что кнопка Canvas-экрана и клавиши Enter/Пробел
	}
}

void UDeathScreenWidget::HandleSaveBackpackClicked()
{
	if (!Player || bAdInProgress)
	{
		return;
	}

	IAdService* Ads = AdService::Get(this);
	if (!Ads || !Ads->IsRewardedReady())
	{
		// Ролик разгрузился между показом кнопки и кликом — честно прячем кнопку.
		if (SaveBackpackButton)
		{
			SaveBackpackButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	UE_LOG(LogQA, Display, TEXT("QA: save-backpack button clicked"));
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordAdStage(TEXT("backpack"), TEXT("button_clicked"));
		Analytics->RecordAdStage(TEXT("backpack"), TEXT("started"));
	}

	bAdInProgress = true;
	Ads->ShowRewarded(AdPlacements::DeathBackpack,
		FSimpleDelegate::CreateUObject(this, &UDeathScreenWidget::HandleAdSuccess),
		FSimpleDelegate::CreateUObject(this, &UDeathScreenWidget::HandleAdFail));
}

void UDeathScreenWidget::HandleAdSuccess()
{
	bAdInProgress = false;
	if (!Player)
	{
		return;
	}

	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordAdStage(TEXT("backpack"), TEXT("completed"));
	}
	Player->RegisterBackpackAdUse();

	// Возрождение со «спасённым» планом потерь (10%/10%); экран смерти скрывает Respawn.
	Player->Respawn(/*bBackpackRescued=*/true);
}

void UDeathScreenWidget::HandleAdFail()
{
	bAdInProgress = false;

	// Досрочное закрытие/ошибка показа: ничего не списываем, экран остаётся, кнопку не
	// блокируем (ТЗ №1 п.4) — только спокойная строка про полный просмотр.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordAdStage(TEXT("backpack"), TEXT("dismissed"));
	}
	if (SaveBackpackSubText)
	{
		SaveBackpackSubText->SetText(AdNotFinishedText);
	}
}
