// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/ConsentScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"

void UConsentScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ConsentRoot"));
	WidgetTree->RootWidget = Root;

	// Стиль и тексты — дефолты FConsentScreenStyle; фактические перекрывает ApplyStyle
	// (настройка проекта UDataConsentSettings).
	const FConsentScreenStyle Defaults;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы касание мимо кнопок не ушло в мир.
	DimmerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConsentDimmer"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimmerBorder))
	{
		DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimmerSlot->SetOffsets(FMargin(0.0f));
	}

	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConsentFrame"));
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConsentPanel"));
	PanelBorder->SetPadding(FMargin(32.0f, 24.0f));
	FrameBorder->SetContent(PanelBorder);

	// Ширину колонки задаём боксом: без неё абзацы растянулись бы на весь экран и перенос
	// слов работал бы по краю экрана, а не по краю панели.
	ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ConsentColumnBox"));
	PanelBorder->SetContent(ColumnBox);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConsentColumn"));
	ColumnBox->SetContent(Column);

	TitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConsentTitle"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleBlock))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	BodyBlocks.Add(MakeParagraph(Column, Defaults.BodyText1, TEXT("ConsentBody1")));
	BodyBlocks.Add(MakeParagraph(Column, Defaults.BodyText2, TEXT("ConsentBody2")));
	BodyBlocks.Add(MakeParagraph(Column, Defaults.BodyText3, TEXT("ConsentBody3")));

	if (UButton* AcceptButton = MakeMenuButton(Column, Defaults.AcceptText, TEXT("ConsentAccept")))
	{
		AcceptButton->OnClicked.AddDynamic(this, &UConsentScreenWidget::HandleAcceptClicked);
		AcceptLabel = Cast<UTextBlock>(AcceptButton->GetContent());
	}
	if (UButton* DeclineButton = MakeMenuButton(Column, Defaults.DeclineText, TEXT("ConsentDecline")))
	{
		DeclineButton->OnClicked.AddDynamic(this, &UConsentScreenWidget::HandleDeclineClicked);
		DeclineLabel = Cast<UTextBlock>(DeclineButton->GetContent());
	}

	// Ссылка на политику — мелкой строкой под кнопками (по источнику истины, раздел 2).
	PolicyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConsentPolicy"));
	PolicyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConsentPolicyLabel"));
	PolicyLabel->SetText(Defaults.PolicyLinkText);
	PolicyButton->SetContent(PolicyLabel);
	PolicyButton->OnClicked.AddDynamic(this, &UConsentScreenWidget::HandlePolicyClicked);
	// Кнопка-ссылка без своей заливки: видна только подпись.
	PolicyButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	if (UVerticalBoxSlot* PolicySlot = Column->AddChildToVerticalBox(PolicyButton))
	{
		PolicySlot->SetHorizontalAlignment(HAlign_Center);
		PolicySlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	ApplyStyle(Defaults);
}

void UConsentScreenWidget::ApplyStyle(const FConsentScreenStyle& Style)
{
	if (DimmerBorder) { DimmerBorder->SetBrushColor(Style.DimColor); }
	if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }
	if (ColumnBox)    { ColumnBox->SetWidthOverride(FMath::Max(200.0f, Style.TextColumnWidth)); }

	if (TitleBlock)
	{
		TitleBlock->SetText(Style.TitleText);
		TitleBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
		TitleBlock->SetColorAndOpacity(FSlateColor(Style.TitleColor));
	}

	const FText BodyTexts[] = { Style.BodyText1, Style.BodyText2, Style.BodyText3 };
	for (int32 Index = 0; Index < BodyBlocks.Num(); ++Index)
	{
		UTextBlock* Block = BodyBlocks[Index];
		if (!Block)
		{
			continue;
		}
		if (BodyTexts[Index].IsEmpty())
		{
			// Пустой абзац не должен занимать место (издатель может сократить текст).
			Block->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Block->SetVisibility(ESlateVisibility::HitTestInvisible);
		Block->SetText(BodyTexts[Index]);
		Block->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.BodyFontSize)));
		Block->SetColorAndOpacity(FSlateColor(Style.BodyColor));
	}

	auto StyleButtonLabel = [&Style](UTextBlock* Label, const FText& Text)
	{
		if (Label)
		{
			Label->SetText(Text);
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
			Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
		}
	};
	StyleButtonLabel(AcceptLabel, Style.AcceptText);
	StyleButtonLabel(DeclineLabel, Style.DeclineText);

	if (PolicyLabel)
	{
		PolicyLabel->SetText(Style.PolicyLinkText);
		PolicyLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.LinkFontSize)));
		PolicyLabel->SetColorAndOpacity(FSlateColor(Style.LinkColor));
	}

	for (USizeBox* Box : ButtonBoxes)
	{
		if (Box)
		{
			Box->SetWidthOverride(Style.ButtonSize.X);
			Box->SetHeightOverride(Style.ButtonSize.Y);
		}
	}
}

UTextBlock* UConsentScreenWidget::MakeParagraph(UVerticalBox* Column, const FText& Text, const FName& Name)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Block->SetText(Text);
	Block->SetAutoWrapText(true); // абзац переносится по ширине колонки, а не уезжает за экран
	// Имя локальной переменной НЕ Slot: так называется член UWidget, а сборка идёт с флагом
	// «предупреждения как ошибки» (C4458).
	if (UVerticalBoxSlot* ParagraphSlot = Column->AddChildToVerticalBox(Block))
	{
		ParagraphSlot->SetHorizontalAlignment(HAlign_Fill);
		ParagraphSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	return Block;
}

UButton* UConsentScreenWidget::MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName)
{
	// SizeBox задаёт тач-габарит кнопки (у UButton 5.5 нет SetPadding): палец должен попадать.
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Box"))));
	ButtonBoxes.Add(Box);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), BaseName);
	Box->SetContent(Button);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Label"))));
	Text->SetText(Label);
	Button->SetContent(Text);

	if (UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
	}
	return Button;
}

void UConsentScreenWidget::HandleAcceptClicked()
{
	OnAccepted.Broadcast();
}

void UConsentScreenWidget::HandleDeclineClicked()
{
	OnDeclined.Broadcast();
}

void UConsentScreenWidget::HandlePolicyClicked()
{
	OnPolicyRequested.Broadcast();
}

// --- Модальный барьер: события мимо кнопок не идут дальше в мир ---

FReply UConsentScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UConsentScreenWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UConsentScreenWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply UConsentScreenWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	return FReply::Handled();
}
