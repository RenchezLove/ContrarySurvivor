// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/PauseMenuWidget.h"
#include "ContrarySurvivor/Analytics/DataConsentSettings.h"   // Б6: подписи строк паузы
#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h"  // Б6: согласие, политика, версия
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

void UPauseMenuWidget::NativeOnInitialized()
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
			{ ResumeButton, TEXT("ResumeButton") }, { ResumeText, TEXT("ResumeText") },
			{ ConsentButton, TEXT("ConsentButton") }, { ConsentText, TEXT("ConsentText") },
			{ PolicyButton, TEXT("PolicyButton") }, { PolicyText, TEXT("PolicyText") },
			{ QuitButton, TEXT("QuitButton") }, { QuitText, TEXT("QuitText") },
			{ VersionText, TEXT("VersionText") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("PauseMenuWidget: кубик %s не найден в WBP_PauseMenu — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики — в обоих путях (в WBP кнопки пришли из дизайнера, обработчики всё равно наши).
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResumeClicked);
	}
	if (ConsentButton)
	{
		ConsentButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleConsentClicked);
	}
	if (PolicyButton)
	{
		PolicyButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandlePolicyClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleQuitClicked);
	}

	if (!bDesignerTree)
	{
		ApplyStyle(FPauseMenuStyle());
	}
	else
	{
		// Стили — владельца, но подписи согласия/политики/версии живут состоянием (Б6).
		RefreshConsentAndVersion();
	}
}

void UPauseMenuWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseRoot"));
	WidgetTree->RootWidget = Root;

	// Стиль (цвета/тексты/шрифты) — дефолты FPauseMenuStyle; фактический стиль перекрывает
	// ApplyStyle (EditAnywhere-настройка контроллера, директива Рината 07-18).
	const FPauseMenuStyle Defaults;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы клик мимо кнопок не ушёл в мир
	// (само событие гасится в NativeOnMouseButtonDown/NativeOnTouchStarted ниже).
	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimBorder))
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

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	ResumeButton = MakeMenuButton(Column, Defaults.ResumeText, TEXT("ResumeButton"));
	if (ResumeButton)
	{
		ResumeText = Cast<UTextBlock>(ResumeButton->GetContent());
	}

	// Б6 (ADR-059 + правило 5 источника истины): переключатель согласия и строка политики.
	// Подписи берутся из настроек проекта (UDataConsentSettings) при каждом открытии паузы.
	ConsentButton = MakeMenuButton(Column, FText::GetEmpty(), TEXT("ConsentButton"));
	if (ConsentButton)
	{
		ConsentText = Cast<UTextBlock>(ConsentButton->GetContent());
	}
	PolicyButton = MakeMenuButton(Column, FText::GetEmpty(), TEXT("PolicyButton"));
	if (PolicyButton)
	{
		PolicyText = Cast<UTextBlock>(PolicyButton->GetContent());
	}

	QuitButton = MakeMenuButton(Column, Defaults.QuitText, TEXT("QuitButton"));
	if (QuitButton)
	{
		QuitText = Cast<UTextBlock>(QuitButton->GetContent());
	}

	// Б6 (ADR-059): мелко номер версии сборки внизу панели.
	VersionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VersionText"));
	if (UVerticalBoxSlot* VersionSlot = Column->AddChildToVerticalBox(VersionText))
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
}

void UPauseMenuWidget::ApplyStyle(const FPauseMenuStyle& Style)
{
	// Дерево владельца из WBP_PauseMenu: цвета/шрифты — его (ТЗ Рината 08-07); из кода
	// живут только подписи согласия/политики/версии (RefreshConsentAndVersion ниже).
	if (bDesignerTree)
	{
		RefreshConsentAndVersion();
		return;
	}

	if (DimBorder)    { DimBorder->SetBrushColor(Style.DimColor); }
	if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }
	if (TitleText)
	{
		TitleText->SetText(Style.TitleText);
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
		TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
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
	StyleButtonLabel(ResumeText, Style.ResumeText);
	StyleButtonLabel(QuitText, Style.QuitText);

	// Б6: подписи переключателя согласия и политики приходят не из стиля, а из настроек
	// проекта (одно место правды на весь текст согласия) — здесь только шрифт и цвет.
	for (UTextBlock* Label : { ConsentText.Get(), PolicyText.Get() })
	{
		if (Label)
		{
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
			Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
		}
	}
	if (VersionText)
	{
		VersionText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(6, Style.VersionFontSize)));
		VersionText->SetColorAndOpacity(FSlateColor(Style.VersionColor));
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

	if (ConsentText && Settings)
	{
		// Пока игрок не отвечал, ничего не собирается — так и пишем «выключен».
		const bool bGranted = Consent && Consent->IsConsentGranted();
		ConsentText->SetText(bGranted ? Settings->PauseMenuConsentOnText : Settings->PauseMenuConsentOffText);
	}
	if (PolicyText && Settings)
	{
		PolicyText->SetText(Settings->PauseMenuPolicyText);
	}
	if (VersionText)
	{
		VersionText->SetText(Consent ? Consent->GetBuildVersionText() : FText::GetEmpty());
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
