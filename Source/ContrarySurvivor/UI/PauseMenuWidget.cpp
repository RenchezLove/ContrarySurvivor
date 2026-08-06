// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/PauseMenuWidget.h"
#include "ContrarySurvivor/Analytics/DataConsentSettings.h"   // Б6: подписи строк паузы
#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h"  // Б6: согласие, политика, версия
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

void UPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseRoot"));
	WidgetTree->RootWidget = Root;

	// Стиль (цвета/тексты/шрифты) — дефолты FPauseMenuStyle; фактический стиль перекрывает
	// ApplyStyle (EditAnywhere-настройка контроллера, директива Рината 07-18).
	const FPauseMenuStyle Defaults;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы клик мимо кнопок не ушёл в мир
	// (само событие гасится в NativeOnMouseButtonDown/NativeOnTouchStarted ниже).
	DimmerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseDimmer"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimmerBorder))
	{
		DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimmerSlot->SetOffsets(FMargin(0.0f));
	}

	// Панель по центру — двойная рамка в палитре HUD (как окно ежедневной награды).
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseFrame"));
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PausePanel"));
	PanelBorder->SetPadding(FMargin(36.0f, 26.0f));
	FrameBorder->SetContent(PanelBorder);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseColumn"));
	PanelBorder->SetContent(Column);

	TitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseTitle"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleBlock))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	if (UButton* ResumeButton = MakeMenuButton(Column, Defaults.ResumeText, TEXT("PauseResume")))
	{
		ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResumeClicked);
		ResumeLabel = Cast<UTextBlock>(ResumeButton->GetContent());
	}

	// Б6 (ADR-059 + правило 5 источника истины): переключатель согласия и строка политики.
	// Подписи берутся из настроек проекта (UDataConsentSettings) при каждом открытии паузы.
	if (UButton* ConsentButton = MakeMenuButton(Column, FText::GetEmpty(), TEXT("PauseConsent")))
	{
		ConsentButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleConsentClicked);
		ConsentLabel = Cast<UTextBlock>(ConsentButton->GetContent());
	}
	if (UButton* PolicyButton = MakeMenuButton(Column, FText::GetEmpty(), TEXT("PausePolicy")))
	{
		PolicyButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandlePolicyClicked);
		PolicyLabel = Cast<UTextBlock>(PolicyButton->GetContent());
	}

	if (UButton* QuitButton = MakeMenuButton(Column, Defaults.QuitText, TEXT("PauseQuit")))
	{
		QuitButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleQuitClicked);
		QuitLabel = Cast<UTextBlock>(QuitButton->GetContent());
	}

	// Б6 (ADR-059): мелко номер версии сборки внизу панели.
	VersionBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseVersion"));
	if (UVerticalBoxSlot* VersionSlot = Column->AddChildToVerticalBox(VersionBlock))
	{
		VersionSlot->SetHorizontalAlignment(HAlign_Center);
		VersionSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.45f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	ApplyStyle(Defaults);
}

void UPauseMenuWidget::ApplyStyle(const FPauseMenuStyle& Style)
{
	if (DimmerBorder) { DimmerBorder->SetBrushColor(Style.DimColor); }
	if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }
	if (TitleBlock)
	{
		TitleBlock->SetText(Style.TitleText);
		TitleBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
		TitleBlock->SetColorAndOpacity(FSlateColor(Style.TitleColor));
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
	StyleButtonLabel(ResumeLabel, Style.ResumeText);
	StyleButtonLabel(QuitLabel, Style.QuitText);

	// Б6: подписи переключателя согласия и политики приходят не из стиля, а из настроек
	// проекта (одно место правды на весь текст согласия) — здесь только шрифт и цвет.
	for (UTextBlock* Label : { ConsentLabel.Get(), PolicyLabel.Get() })
	{
		if (Label)
		{
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
			Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
		}
	}
	if (VersionBlock)
	{
		VersionBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(6, Style.VersionFontSize)));
		VersionBlock->SetColorAndOpacity(FSlateColor(Style.VersionColor));
	}
	RefreshConsentAndVersion();

	for (USizeBox* Box : ButtonBoxes)
	{
		if (Box)
		{
			Box->SetWidthOverride(Style.ButtonSize.X);
			Box->SetHeightOverride(Style.ButtonSize.Y);
		}
	}
}

UButton* UPauseMenuWidget::MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName)
{
	// SizeBox задаёт тач-габарит кнопки (у UButton 5.5 нет SetPadding): палец должен попадать.
	// Размер/шрифт/цвет ставит ApplyStyle (боксы и подписи запоминаются членами).
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
		BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	return Button;
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Пауза открывается много раз за игру, а решение по согласию могло смениться — подпись
	// переключателя и строку версии освежаем при каждом появлении меню на экране.
	RefreshConsentAndVersion();
}

void UPauseMenuWidget::RefreshConsentAndVersion()
{
	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this);

	if (ConsentLabel && Settings)
	{
		// Пока игрок не отвечал, ничего не собирается — так и пишем «выключен».
		const bool bGranted = Consent && Consent->IsConsentGranted();
		ConsentLabel->SetText(bGranted ? Settings->PauseMenuConsentOnText : Settings->PauseMenuConsentOffText);
	}
	if (PolicyLabel && Settings)
	{
		PolicyLabel->SetText(Settings->PauseMenuPolicyText);
	}
	if (VersionBlock)
	{
		VersionBlock->SetText(Consent ? Consent->GetBuildVersionText() : FText::GetEmpty());
	}
}

void UPauseMenuWidget::HandleResumeClicked()
{
	OnResumeRequested.Broadcast();
}

void UPauseMenuWidget::HandleQuitClicked()
{
	OnQuitRequested.Broadcast();
}

void UPauseMenuWidget::HandleConsentClicked()
{
	if (UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this))
	{
		Consent->SetConsent(!Consent->IsConsentGranted());
	}
	RefreshConsentAndVersion();
}

void UPauseMenuWidget::HandlePolicyClicked()
{
	if (const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this))
	{
		// Адреса нет — метод сам тихо ничего не делает, пустую страницу не показываем.
		Consent->OpenPrivacyPolicy();
	}
}

// --- Модальный барьер: события мимо кнопок не идут дальше в мир ---

FReply UPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UPauseMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UPauseMenuWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply UPauseMenuWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	return FReply::Handled();
}
