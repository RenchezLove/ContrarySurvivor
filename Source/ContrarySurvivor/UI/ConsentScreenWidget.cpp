// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/ConsentScreenWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA: предупреждения о недостающих кубиках
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

	// ТЗ Рината 08-07, детект как в EndOfStoryWidget.cpp: дерево владельца из WBP уже
	// построено и кубики привязаны — строить и стилизовать ничего не нужно.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ TitleText, TEXT("TitleText") },
			{ Body1Text, TEXT("Body1Text") }, { Body2Text, TEXT("Body2Text") },
			{ Body3Text, TEXT("Body3Text") },
			{ AcceptButton, TEXT("AcceptButton") }, { AcceptText, TEXT("AcceptText") },
			{ DeclineButton, TEXT("DeclineButton") }, { DeclineText, TEXT("DeclineText") },
			{ PolicyButton, TEXT("PolicyButton") }, { PolicyText, TEXT("PolicyText") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("ConsentScreenWidget: кубик %s не найден в WBP_Consent — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики — в обоих путях (в WBP кнопки пришли из дизайнера, обработчики всё равно наши).
	if (AcceptButton)
	{
		AcceptButton->OnClicked.AddDynamic(this, &UConsentScreenWidget::HandleAcceptClicked);
	}
	if (DeclineButton)
	{
		DeclineButton->OnClicked.AddDynamic(this, &UConsentScreenWidget::HandleDeclineClicked);
	}
	if (PolicyButton)
	{
		PolicyButton->OnClicked.AddDynamic(this, &UConsentScreenWidget::HandlePolicyClicked);
	}

	if (!bDesignerTree)
	{
		ApplyStyle(FConsentScreenStyle());
	}
}

void UConsentScreenWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ConsentRoot"));
	WidgetTree->RootWidget = Root;

	// Стиль и тексты — дефолты FConsentScreenStyle; фактические перекрывает ApplyStyle
	// (настройка проекта UDataConsentSettings).
	const FConsentScreenStyle Defaults;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы касание мимо кнопок не ушло в мир.
	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimBorder))
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

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	Body1Text = MakeParagraph(Column, Defaults.BodyText1, TEXT("Body1Text"));
	Body2Text = MakeParagraph(Column, Defaults.BodyText2, TEXT("Body2Text"));
	Body3Text = MakeParagraph(Column, Defaults.BodyText3, TEXT("Body3Text"));

	AcceptButton = MakeMenuButton(Column, Defaults.AcceptText, TEXT("AcceptButton"));
	if (AcceptButton)
	{
		AcceptText = Cast<UTextBlock>(AcceptButton->GetContent());
	}
	DeclineButton = MakeMenuButton(Column, Defaults.DeclineText, TEXT("DeclineButton"));
	if (DeclineButton)
	{
		DeclineText = Cast<UTextBlock>(DeclineButton->GetContent());
	}

	// Ссылка на политику — мелкой строкой под кнопками (по источнику истины, раздел 2).
	PolicyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PolicyButton"));
	PolicyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PolicyText"));
	PolicyText->SetText(Defaults.PolicyLinkText);
	PolicyButton->SetContent(PolicyText);
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
}

void UConsentScreenWidget::ApplyStyle(const FConsentScreenStyle& Style)
{
	// Дерево владельца из WBP_Consent: цвета/шрифты/размеры — его (ТЗ Рината 08-07).
	// ТЕКСТЫ ставятся в обоих путях: формулировки согласия — дословно из источника истины
	// (издатель проверяет), правка ассета их переопределять не должна.
	if (!bDesignerTree)
	{
		if (DimBorder)    { DimBorder->SetBrushColor(Style.DimColor); }
		if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
		if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }
		if (ColumnBox)    { ColumnBox->SetWidthOverride(FMath::Max(200.0f, Style.TextColumnWidth)); }
	}

	if (TitleText)
	{
		TitleText->SetText(Style.TitleText);
		if (!bDesignerTree)
		{
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
			TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
		}
	}

	UTextBlock* const Bodies[] = { Body1Text.Get(), Body2Text.Get(), Body3Text.Get() };
	const FText BodyTexts[] = { Style.BodyText1, Style.BodyText2, Style.BodyText3 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bodies); ++Index)
	{
		UTextBlock* Block = Bodies[Index];
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
		if (!bDesignerTree)
		{
			Block->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.BodyFontSize)));
			Block->SetColorAndOpacity(FSlateColor(Style.BodyColor));
		}
	}

	auto StyleButtonLabel = [this, &Style](UTextBlock* Label, const FText& Text)
	{
		if (Label)
		{
			Label->SetText(Text);
			if (!bDesignerTree)
			{
				Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
				Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
			}
		}
	};
	StyleButtonLabel(AcceptText, Style.AcceptText);
	StyleButtonLabel(DeclineText, Style.DeclineText);

	if (PolicyText)
	{
		PolicyText->SetText(Style.PolicyLinkText);
		if (!bDesignerTree)
		{
			PolicyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.LinkFontSize)));
			PolicyText->SetColorAndOpacity(FSlateColor(Style.LinkColor));
		}
	}

	if (!bDesignerTree)
	{
		for (USizeBox* Box : ButtonBoxes)
		{
			if (Box)
			{
				Box->SetWidthOverride(Style.ButtonSize.X);
				Box->SetHeightOverride(Style.ButtonSize.Y);
			}
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
