// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/IntroObjectiveWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA: предупреждение о недостающем кубике
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h" // открыто ли главное меню
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // текст задачи вступления
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

bool UIntroObjectiveWidget::ShouldShowObjective(const FText& Objective, bool bMainMenuOnScreen)
{
	// Задачи нет — строки нет вовсе (пустая плашка вверху экрана хуже, чем ничего).
	// Меню на экране — игровой интерфейс молчит: поверх меню ему не место (урок 08-09).
	return !Objective.IsEmpty() && !bMainMenuOnScreen;
}

void UIntroObjectiveWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	// Детект пути — как у остальных окон волны: дерево владельца уже построено, значит
	// строить и красить нечего.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		if (!ObjectiveText)
		{
			UE_LOG(LogQA, Warning,
				TEXT("IntroObjectiveWidget: кубик ObjectiveText не найден в WBP_IntroObjective — строка задачи не появится"));
		}
	}
	else
	{
		BuildCodeTree();
	}
}

void UIntroObjectiveWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("IntroObjectiveRoot"));
	WidgetTree->RootWidget = Root;

	// Плашка по центру ВЕРХНЕГО края: тот же угол и тот же вид, что рисовал холст
	// (DrawIntroObjective — плашка и цвет трекера задания).
	ObjectivePlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ObjectivePlate"));
	ObjectivePlate->SetBrushColor(PlateColor);
	ObjectivePlate->SetPadding(FMargin(18.0f, 8.0f));
	ObjectivePlate->SetHorizontalAlignment(HAlign_Center);
	ObjectivePlate->SetVerticalAlignment(VAlign_Center);

	ObjectiveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ObjectiveText"));
	ObjectiveText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, ObjectiveFontSize)));
	ObjectiveText->SetColorAndOpacity(FSlateColor(ObjectiveColor));
	ObjectiveText->SetJustification(ETextJustify::Center);
	ObjectivePlate->SetContent(ObjectiveText);

	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(ObjectivePlate))
	{
		PlateSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		PlateSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		PlateSlot->SetPosition(FVector2D(0.0f, FMath::Max(0.0f, TopMargin)));
		PlateSlot->SetAutoSize(true);
	}
}

void UIntroObjectiveWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	APlayerController* PC = GetOwningPlayer();
	const AContrarySurvivorHUD* HUD = PC ? Cast<AContrarySurvivorHUD>(PC->GetHUD()) : nullptr;
	if (!HUD)
	{
		SetContentVisible(false);
		return;
	}

	const AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC);
	const FText Objective = HUD->GetIntroObjective();
	if (!ShouldShowObjective(Objective, CSPC && CSPC->IsMainMenuOnScreen()))
	{
		// Прячем СОДЕРЖИМОЕ, а не себя: свернувшее себя окно потеряет тик навсегда.
		SetContentVisible(false);
		return;
	}
	SetContentVisible(true);

	if (ObjectiveText)
	{
		// Склейка через FText::Format с ИМЕНОВАННОЙ подстановкой (ADR-050): порядок слов
		// в языках разный, FString-склейка обнулила бы перевод.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Objective"), Objective);
		ObjectiveText->SetText(FText::Format(ObjectiveFormat, Args));
	}
}
