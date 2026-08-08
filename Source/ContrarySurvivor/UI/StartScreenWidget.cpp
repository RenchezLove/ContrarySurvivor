// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA: предупреждения о недостающих кубиках
#include "ContrarySurvivor/Analytics/DataConsentSettings.h"  // подпись строки политики
#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h" // версия сборки + открытие политики
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "HAL/PlatformProcess.h" // FPlatformProcess::LaunchURL (пункт «Сообщество»)
#include "Styling/CoreStyle.h"

FString UMainMenuSettings::GetCommunityUrl()
{
	// Значение из Config/DefaultGame.ini (раздел [/Script/ContrarySurvivor.MainMenuSettings]).
	// GetDefault отдаёт объект-по-умолчанию класса, в который движок уже загрузил конфиг.
	const UMainMenuSettings* Settings = GetDefault<UMainMenuSettings>();
	return Settings ? Settings->CommunityUrl.TrimStartAndEnd() : FString();
}

void UStartScreenWidget::NativeOnInitialized()
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
			{ TitleText, TEXT("TitleText") }, { SubtitleText, TEXT("SubtitleText") },
			{ ContinueButton, TEXT("ContinueButton") }, { ContinueText, TEXT("ContinueText") },
			{ NewGameButton, TEXT("NewGameButton") }, { NewGameText, TEXT("NewGameText") },
			{ SettingsButton, TEXT("SettingsButton") }, { SettingsText, TEXT("SettingsText") },
			{ CommunityButton, TEXT("CommunityButton") }, { CommunityText, TEXT("CommunityText") },
			{ ExitButton, TEXT("ExitButton") }, { ExitText, TEXT("ExitText") },
			{ PolicyButton, TEXT("PolicyButton") }, { PolicyText, TEXT("PolicyText") },
			{ VersionText, TEXT("VersionText") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("StartScreenWidget: кубик %s не найден в WBP_StartScreen — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики — в обоих путях (в WBP кнопки пришли из дизайнера, обработчики всё равно наши).
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleContinueClicked);
	}
	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleNewGameClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleSettingsClicked);
	}
	if (CommunityButton)
	{
		CommunityButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleCommunityClicked);
	}
	if (ExitButton)
	{
		ExitButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleExitClicked);
	}
	if (PolicyButton)
	{
		PolicyButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandlePolicyClicked);
	}

	if (!bDesignerTree)
	{
		ApplyStyle(CachedStyle);
	}
}

void UStartScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Меню может показываться не один раз за сессию — данные-строки (версия сборки, подпись
	// политики) и видимость пунктов освежаются при каждом появлении на экране (паттерн
	// UPauseMenuWidget::NativeConstruct).
	RefreshMenuExtras();
}

void UStartScreenWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StartRoot"));
	WidgetTree->RootWidget = Root;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы клик мимо кнопок не ушёл в мир.
	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimBorder))
	{
		DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimmerSlot->SetOffsets(FMargin(0.0f));
	}

	// Панель по центру — двойная рамка в палитре HUD (как меню паузы/окно ежедневной награды).
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartFrame"));
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartPanel"));
	PanelBorder->SetPadding(FMargin(36.0f, 26.0f));
	FrameBorder->SetContent(PanelBorder);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StartColumn"));
	PanelBorder->SetContent(Column);

	const FStartScreenStyle Defaults;

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SubtitleText"));
	SubtitleText->SetAutoWrapText(true);
	SubtitleText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* SubtitleSlot = Column->AddChildToVerticalBox(SubtitleText))
	{
		SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
		SubtitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}

	ContinueButton = MakeMenuButton(Column, Defaults.ContinueText, TEXT("ContinueButton"));
	if (ContinueButton)
	{
		ContinueText = Cast<UTextBlock>(ContinueButton->GetContent());
	}
	NewGameButton = MakeMenuButton(Column, Defaults.NewGameText, TEXT("NewGameButton"));
	if (NewGameButton)
	{
		NewGameText = Cast<UTextBlock>(NewGameButton->GetContent());
	}
	SettingsButton = MakeMenuButton(Column, Defaults.SettingsText, TEXT("SettingsButton"));
	if (SettingsButton)
	{
		SettingsText = Cast<UTextBlock>(SettingsButton->GetContent());
	}
	CommunityButton = MakeMenuButton(Column, Defaults.CommunityText, TEXT("CommunityButton"));
	if (CommunityButton)
	{
		CommunityText = Cast<UTextBlock>(CommunityButton->GetContent());
	}
	ExitButton = MakeMenuButton(Column, Defaults.ExitText, TEXT("ExitButton"));
	if (ExitButton)
	{
		ExitText = Cast<UTextBlock>(ExitButton->GetContent());
	}

	// Низ панели (спека: «мелким шрифтом, не кнопками»): ссылка политики — прозрачная кнопка,
	// видна только подпись (приём экрана согласия); ниже — строка версии сборки (просто текст).
	// Живые тексты обеих строк ставит RefreshMenuExtras из настроек/подсистемы согласия.
	PolicyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PolicyButton"));
	PolicyButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	PolicyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PolicyText"));
	PolicyText->SetJustification(ETextJustify::Center);
	PolicyButton->SetContent(PolicyText);
	if (UVerticalBoxSlot* PolicySlot = Column->AddChildToVerticalBox(PolicyButton))
	{
		PolicySlot->SetHorizontalAlignment(HAlign_Center);
		PolicySlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	}

	VersionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VersionText"));
	if (UVerticalBoxSlot* VersionSlot = Column->AddChildToVerticalBox(VersionText))
	{
		VersionSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.45f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}
}

void UStartScreenWidget::ApplyStyle(const FStartScreenStyle& Style)
{
	CachedStyle = Style;

	// Дерево владельца из WBP_StartScreen: цвета/шрифты/размеры — его, код не перекрашивает
	// (ТЗ Рината 08-07). Тексты переключаются ниже — они зависят от режима переспроса.
	if (!bDesignerTree)
	{
		if (DimBorder)    { DimBorder->SetBrushColor(Style.DimColor); }
		if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
		if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }

		for (USizeBox* Box : ButtonBoxes)
		{
			if (Box)
			{
				Box->SetWidthOverride(Style.ButtonSize.X);
				Box->SetHeightOverride(Style.ButtonSize.Y);
			}
		}

		// Мелкие строки низа — один шрифт/цвет на обе (поле стиля «версия и политика»).
		if (PolicyText)
		{
			PolicyText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(6, Style.VersionFontSize)));
			PolicyText->SetColorAndOpacity(FSlateColor(Style.VersionColor));
		}
		if (VersionText)
		{
			VersionText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(6, Style.VersionFontSize)));
			VersionText->SetColorAndOpacity(FSlateColor(Style.VersionColor));
		}
	}

	// Новый стиль применяется в ТЕКУЩЕМ режиме (обычный выбор либо переспрос «Новая игра»):
	// иначе повторный ApplyStyle (например, из редактора) сбросил бы открытый переспрос.
	bConfirmingNewGame ? ApplyConfirmLabels(Style) : ApplyChoiceLabels(Style);
}

void UStartScreenWidget::ApplyChoiceLabels(const FStartScreenStyle& Style)
{
	if (TitleText)
	{
		TitleText->SetText(Style.TitleText);
		if (!bDesignerTree)
		{
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
			TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
		}
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Style.SubtitleText);
		if (!bDesignerTree)
		{
			SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.SubtitleFontSize)));
			SubtitleText->SetColorAndOpacity(FSlateColor(Style.SubtitleColor));
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
	StyleButtonLabel(ContinueText, Style.ContinueText);
	StyleButtonLabel(NewGameText, Style.NewGameText);
	StyleButtonLabel(SettingsText, Style.SettingsText);
	StyleButtonLabel(CommunityText, Style.CommunityText);
	StyleButtonLabel(ExitText, Style.ExitText);

	// Возврат из переспроса «Новая игра» обязан вернуть и спрятанные на его время пункты.
	ApplyMenuRowVisibility();
}

void UStartScreenWidget::ApplyConfirmLabels(const FStartScreenStyle& Style)
{
	// Переспрос «Точно начать заново?» (решение лида 08-05): те же две кнопки, другие подписи —
	// «Продолжить» временно становится «Отмена», «Новая игра» — «Да, начать заново».
	if (TitleText)
	{
		TitleText->SetText(Style.ConfirmTitleText);
		if (!bDesignerTree)
		{
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
			TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
		}
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Style.ConfirmSubtitleText);
		if (!bDesignerTree)
		{
			SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.SubtitleFontSize)));
			SubtitleText->SetColorAndOpacity(FSlateColor(Style.SubtitleColor));
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
	StyleButtonLabel(ContinueText, Style.ConfirmCancelText);
	StyleButtonLabel(NewGameText, Style.ConfirmYesText);

	// На время переспроса остаются ровно две кнопки — «Отмена» и «Да, начать заново»;
	// остальные пункты меню прячутся, чтобы случайный тап рядом не увёл с вопроса о стирании.
	SetRowVisibility(ContinueButton, ESlateVisibility::Visible); // переспрос идёт только поверх сейва
	SetRowVisibility(SettingsButton, ESlateVisibility::Collapsed);
	SetRowVisibility(CommunityButton, ESlateVisibility::Collapsed);
	SetRowVisibility(ExitButton, ESlateVisibility::Collapsed);
}

void UStartScreenWidget::ApplyMenuRowVisibility()
{
	// «Продолжить» без сейва не показывается вовсе — Collapsed, места в колонке не занимает
	// (спека: «не гаснет серым»). Подзаголовок «Найдено сохранение прошлой игры.» без сейва
	// был бы неправдой — прячется вместе с пунктом.
	const ESlateVisibility ContinueVisibility = ContinueVisibilityFor(bHasSave);
	SetRowVisibility(ContinueButton, ContinueVisibility);
	if (SubtitleText)
	{
		SubtitleText->SetVisibility(ContinueVisibility == ESlateVisibility::Visible
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// «Настройки»: пока владелец не привязал открытие экрана настроек (подход 2), пункт
	// спрятан целиком; появится сам, как только привязка появится (каркас вызова готов).
	SetRowVisibility(SettingsButton, OnSettingsRequested.IsBound()
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// «Сообщество» видно только при непустом адресе в конфиге (спека).
	SetRowVisibility(CommunityButton, CommunityVisibilityFor(UMainMenuSettings::GetCommunityUrl()));

	SetRowVisibility(ExitButton, ESlateVisibility::Visible);
}

void UStartScreenWidget::RefreshMenuExtras()
{
	// Подпись политики и строка версии — те же источники, что в меню паузы (одно место правды
	// на весь текст: UDataConsentSettings / UDataConsentSubsystem).
	if (PolicyText)
	{
		if (const UDataConsentSettings* Settings = UDataConsentSettings::Get())
		{
			PolicyText->SetText(Settings->PauseMenuPolicyText);
		}
	}
	if (VersionText)
	{
		const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this);
		VersionText->SetText(Consent ? Consent->GetBuildVersionText() : FText::GetEmpty());
	}

	// Видимость пунктов не трогаем посреди переспроса — там свой набор кнопок.
	if (!bConfirmingNewGame)
	{
		ApplyMenuRowVisibility();
	}
}

void UStartScreenWidget::SetHasSave(bool bInHasSave)
{
	bHasSave = bInHasSave;
	if (!bConfirmingNewGame)
	{
		ApplyMenuRowVisibility();
	}
}

ESlateVisibility UStartScreenWidget::ContinueVisibilityFor(bool bInHasSave)
{
	return bInHasSave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

bool UStartScreenWidget::ShouldConfirmNewGame(bool bInHasSave)
{
	// Переспрос защищает существующий прогресс; без сейва стирать нечего.
	return bInHasSave;
}

ESlateVisibility UStartScreenWidget::CommunityVisibilityFor(const FString& CommunityUrl)
{
	return CommunityUrl.TrimStartAndEnd().IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
}

void UStartScreenWidget::SetRowVisibility(UWidget* Widget, ESlateVisibility InVisibility)
{
	if (!Widget)
	{
		return;
	}
	// В кодовом дереве кнопка обёрнута в SizeBox тач-габарита — прятать надо обёртку, иначе
	// колонка сохранит пустое место под пунктом (ловушка скрытия, урок AmmoRow).
	UWidget* Row = Widget;
	if (USizeBox* Box = Cast<USizeBox>(Widget->GetParent()))
	{
		Row = Box;
	}
	Row->SetVisibility(InVisibility);
}

UButton* UStartScreenWidget::MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName)
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
		BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	return Button;
}

void UStartScreenWidget::HandleContinueClicked()
{
	if (bConfirmingNewGame)
	{
		// Кнопка сейчас подписана «Отмена» — переспрос закрыт, сейв цел, обычный выбор снова.
		bConfirmingNewGame = false;
		ApplyChoiceLabels(CachedStyle);
		return;
	}
	OnContinueRequested.Broadcast();
}

void UStartScreenWidget::HandleNewGameClicked()
{
	if (!bConfirmingNewGame)
	{
		if (!ShouldConfirmNewGame(bHasSave))
		{
			// Сейва нет — стирать нечего, новая игра стартует без переспроса (спека:
			// переспрос идёт только поверх существующего сохранения).
			OnNewGameRequested.Broadcast();
			return;
		}
		// Первый клик ничего не стирает — только переспрашивает (решение лида 08-05: случайное
		// касание на телефоне не должно уничтожать прогресс без возможности отмены).
		bConfirmingNewGame = true;
		ApplyConfirmLabels(CachedStyle);
		return;
	}
	// Кнопка сейчас подписана «Да, начать заново» — второе явное нажатие стирает по-настоящему.
	// Решение принято: переспрос закрывается и подписи возвращаются ДО сигнала владельцу —
	// иначе кешированный виджет при повторном показе меню открылся бы в режиме переспроса
	// (нашёл автотест NewGameConfirmFlow).
	bConfirmingNewGame = false;
	ApplyChoiceLabels(CachedStyle);
	OnNewGameRequested.Broadcast();
}

void UStartScreenWidget::HandleSettingsClicked()
{
	// Виджет только сообщает: что открывать — решает владелец (контроллер, подход 2).
	OnSettingsRequested.Broadcast();
}

void UStartScreenWidget::HandleCommunityClicked()
{
	const FString Url = UMainMenuSettings::GetCommunityUrl();
	if (Url.IsEmpty())
	{
		// Пункт при пустом адресе спрятан целиком, штатно сюда не попасть — строка в журнал.
		UE_LOG(LogQA, Display, TEXT("QA: main menu COMMUNITY clicked (no url in config)"));
		return;
	}
	FString Error;
	FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
	UE_LOG(LogQA, Display, TEXT("QA: main menu COMMUNITY clicked, url '%s'%s%s"),
		*Url, Error.IsEmpty() ? TEXT("") : TEXT(", error: "), *Error);
}

void UStartScreenWidget::HandleExitClicked()
{
	// Само закрытие игры делает владелец (контроллер) — как с прочими решениями меню.
	UE_LOG(LogQA, Display, TEXT("QA: main menu EXIT clicked"));
	OnExitRequested.Broadcast();
}

void UStartScreenWidget::HandlePolicyClicked()
{
	if (const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this))
	{
		// Адреса нет — метод сам тихо ничего не делает, пустую страницу не показываем.
		Consent->OpenPrivacyPolicy();
	}
}

// --- Модальный барьер: события мимо кнопок не идут дальше в мир ---

FReply UStartScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UStartScreenWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UStartScreenWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply UStartScreenWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	return FReply::Handled();
}
