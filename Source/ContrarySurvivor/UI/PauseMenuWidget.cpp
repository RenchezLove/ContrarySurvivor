// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/PauseMenuWidget.h"
#include "ContrarySurvivor/Analytics/DataConsentSettings.h"   // Б6: подписи строк паузы
#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h"  // Б6: согласие, политика, версия
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA: предупреждения о недостающих кубиках
#include "ContrarySurvivor/UI/StartScreenWidget.h" // UMainMenuSettings: адрес сообщества — один на игру
#include "HAL/PlatformProcess.h" // FPlatformProcess::LaunchURL (пункт «Сообщество»)
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
		// Переключатель согласия из паузы УБРАН (ТЗ Рината 08-08): согласие ставится только
		// на экране согласия. 08-09 его кубики удалены отовсюду — из кода, из кодового
		// дерева, из контрактов и из живого ассета (режим генератора -dropdead), поэтому
		// в списке ожидаемых их нет и быть не должно.
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ TitleText, TEXT("TitleText") },
			{ ResumeButton, TEXT("ResumeButton") }, { ResumeText, TEXT("ResumeText") },
			{ MainMenuButton, TEXT("MainMenuButton") }, { MainMenuText, TEXT("MainMenuText") },
			{ SettingsButton, TEXT("SettingsButton") }, { SettingsText, TEXT("SettingsText") },
			{ CommunityButton, TEXT("CommunityButton") }, { CommunityText, TEXT("CommunityText") },
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

	// Запоминаем подписи ДО того, как их тронет режим переспроса: в дизайнер-дереве это тексты
	// владельца, и вернуть после отмены надо именно их.
	OriginalTitleText = TitleText ? TitleText->GetText() : FText::GetEmpty();
	OriginalResumeText = ResumeText ? ResumeText->GetText() : FText::GetEmpty();

	// Клики — в обоих путях (в WBP кнопки пришли из дизайнера, обработчики всё равно наши).
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResumeClicked);
	}
	// ТЗ Рината 08-08: переключателя согласия в паузе нет. 08-09 кубики удалены и из кода,
	// и из живого ассета: невидимая кнопка продолжала лежать в панели ровно под новой
	// «В главное меню» и валила проверку раскладки. Отзыв согласия — на экране согласия.
	if (PolicyButton)
	{
		PolicyButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandlePolicyClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleQuitClicked);
	}
	if (MainMenuButton)
	{
		MainMenuButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleMainMenuClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleSettingsClicked);
	}
	if (CommunityButton)
	{
		CommunityButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleCommunityClicked);
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

	// Подход 3 волны меню: возврат в главное меню — сразу под «Продолжить», выше «Выхода»
	// (это возврат в игру, а не закрытие игры, и путать их пальцем не должно).
	MainMenuButton = MakeMenuButton(Column, Defaults.MainMenuText, TEXT("MainMenuButton"));
	if (MainMenuButton)
	{
		MainMenuText = Cast<UTextBlock>(MainMenuButton->GetContent());
	}

	// ТЗ Рината 08-08: переключатель согласия из паузы убран (согласие — только на стартовом
	// экране согласия), поэтому кодовое дерево его больше НЕ строит. Остаётся строка политики
	// (ADR-059 минимум для паузы) и мелкий номер версии сборки ниже.
	// Волна 08-09: «Настройки» и «Сообщество» прямо из паузы — открывают то же самое,
	// что одноимённые пункты главного меню. Стоят между возвратом в меню и политикой.
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
	// Стиль запоминаем: переспрос «выйти в меню» переключает подписи туда-обратно без
	// пересоздания дерева.
	CachedStyle = Style;

	// Дерево владельца из WBP_PauseMenu: цвета/шрифты — его (ТЗ Рината 08-07); из кода
	// живут только подписи согласия/политики/версии и подписи режима переспроса (это данные).
	if (bDesignerTree)
	{
		RefreshConsentAndVersion();
		bConfirmingMainMenu ? ApplyConfirmMainMenuLabels() : ApplyNormalLabels();
		return;
	}

	if (DimBorder)    { DimBorder->SetBrushColor(Style.DimColor); }
	if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }
	if (TitleText)
	{
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
		TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
	}

	auto StyleButtonLabel = [&Style](UTextBlock* Label)
	{
		if (Label)
		{
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
			Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
		}
	};
	StyleButtonLabel(ResumeText);
	StyleButtonLabel(QuitText);
	StyleButtonLabel(MainMenuText);

	// Сами тексты ставит режим (обычный вид либо переспрос) — иначе повторный ApplyStyle
	// сбросил бы открытый переспрос.
	bConfirmingMainMenu ? ApplyConfirmMainMenuLabels() : ApplyNormalLabels();

	// Подпись строки политики приходит не из стиля, а из настроек проекта (одно место правды
	// на весь текст согласия) — здесь только шрифт и цвет. Переключатель согласия из паузы
	// убран (ТЗ Рината 08-08), поэтому стилизуем только строку политики.
	if (PolicyText)
	{
		// Своим кеглем, мельче остальных подписей: «Политика конфиденциальности» — 27 знаков,
		// и общим кеглем она не помещалась в кнопку (Ринат увидел обрезанную надпись 08-09).
		// Плюс перенос по словам: если владелец сузит кнопку, надпись перенесётся, а не
		// обрежется.
		PolicyText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(6, Style.PolicyFontSize)));
		PolicyText->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
		PolicyText->SetAutoWrapText(true);
		PolicyText->SetJustification(ETextJustify::Center);
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

	// Незакрытый вопрос «выйти в меню?» не должен пережить закрытие паузы (игрок мог выйти
	// из неё клавишей): каждое открытие — обычный вид.
	if (bConfirmingMainMenu)
	{
		bConfirmingMainMenu = false;
	}
	ApplyNormalLabels();
}

void UPauseMenuWidget::RefreshConsentAndVersion()
{
	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this);

	// ТЗ Рината 08-08: подпись переключателя согласия в паузе больше не обновляем — самого
	// переключателя в паузе нет. Освежаем только строку политики и номер версии сборки.
	if (PolicyText && Settings)
	{
		PolicyText->SetText(Settings->PauseMenuPolicyText);
	}
	if (VersionText)
	{
		VersionText->SetText(Consent ? Consent->GetBuildVersionText() : FText::GetEmpty());
	}
}

void UPauseMenuWidget::SetProgressUnsaved(bool bInUnsaved)
{
	bProgressUnsaved = bInUnsaved;
}

bool UPauseMenuWidget::ShouldConfirmMainMenu(bool bInProgressUnsaved)
{
	// Спека: «Возврат в меню из паузы — с подтверждением, если прогресс не сохранён».
	// Сохранено всё — уходим молча, лишний вопрос игрока только злит.
	return bInProgressUnsaved;
}

void UPauseMenuWidget::ApplyNormalLabels()
{
	// В дизайнер-дереве возвращаем подписи ВЛАДЕЛЬЦА (как он набрал их в WBP), в кодовом —
	// значения стиля с контроллера.
	if (TitleText)
	{
		TitleText->SetText(bDesignerTree ? OriginalTitleText : CachedStyle.TitleText);
	}
	if (ResumeText)
	{
		ResumeText->SetText(bDesignerTree ? OriginalResumeText : CachedStyle.ResumeText);
	}
	if (QuitText && !bDesignerTree)
	{
		QuitText->SetText(CachedStyle.QuitText);
	}
	// Подпись возврата в меню — всегда наша: у неё два состояния, и оба ведёт код.
	if (MainMenuText)
	{
		MainMenuText->SetText(CachedStyle.MainMenuText);
	}
	if (SettingsText && !bDesignerTree)
	{
		SettingsText->SetText(CachedStyle.SettingsText);
	}
	if (CommunityText && !bDesignerTree)
	{
		CommunityText->SetText(CachedStyle.CommunityText);
	}

	// «Настройки» появляются САМИ по факту привязки обработчика владельцем (тот же приём, что
	// в главном меню): не привязано — пункта нет, чтобы в панели не висела мёртвая кнопка.
	SetRowVisibility(SettingsButton, OnSettingsRequested.IsBound()
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// «Сообщество» видно только при непустом адресе в настройке проекта. Правило одно на всю
	// игру — берём его у главного меню, второго адреса и второго правила не заводим.
	SetRowVisibility(CommunityButton,
		UStartScreenWidget::CommunityVisibilityFor(UMainMenuSettings::GetCommunityUrl()));

	// Возвращаем ровно ту видимость, которая была ДО вопроса, и только если прятали её мы.
	// Иначе выход из переспроса «показал» бы пункты, которые владелец сам скрыл в дизайнере.
	if (bRowsHiddenByConfirm)
	{
		SetRowVisibility(QuitButton, SavedQuitVisibility);
		SetRowVisibility(PolicyButton, SavedPolicyVisibility);
		bRowsHiddenByConfirm = false;
	}
}

void UPauseMenuWidget::ApplyConfirmMainMenuLabels()
{
	// Те же две кнопки, другие подписи: «Продолжить» становится «Отмена», «В главное меню» —
	// «Да, выйти». Остальные пункты на время вопроса прячутся.
	if (TitleText)   { TitleText->SetText(CachedStyle.ConfirmMainMenuTitleText); }
	if (ResumeText)  { ResumeText->SetText(CachedStyle.ConfirmMainMenuCancelText); }
	if (MainMenuText) { MainMenuText->SetText(CachedStyle.ConfirmMainMenuYesText); }

	if (!bRowsHiddenByConfirm)
	{
		// Запоминаем видимость ДО вопроса — вернём именно её (владелец мог что-то скрыть сам).
		SavedQuitVisibility = QuitButton ? QuitButton->GetVisibility() : ESlateVisibility::Visible;
		SavedPolicyVisibility = PolicyButton ? PolicyButton->GetVisibility() : ESlateVisibility::Visible;
		bRowsHiddenByConfirm = true;
	}
	SetRowVisibility(QuitButton, ESlateVisibility::Collapsed);
	SetRowVisibility(PolicyButton, ESlateVisibility::Collapsed);
}

void UPauseMenuWidget::SetRowVisibility(UWidget* Widget, ESlateVisibility InVisibility)
{
	if (!Widget)
	{
		return;
	}
	UWidget* Row = Widget;
	if (USizeBox* Box = Cast<USizeBox>(Widget->GetParent()))
	{
		Row = Box;
	}
	Row->SetVisibility(InVisibility);
}

void UPauseMenuWidget::HandleResumeClicked()
{
	if (bConfirmingMainMenu)
	{
		// Кнопка сейчас подписана «Отмена»: возвращаем обычный вид паузы, из игры не выходим.
		bConfirmingMainMenu = false;
		ApplyNormalLabels();
		return;
	}
	OnResumeRequested.Broadcast();
}

void UPauseMenuWidget::HandleQuitClicked()
{
	OnQuitRequested.Broadcast();
}

void UPauseMenuWidget::HandleMainMenuClicked()
{
	if (!bConfirmingMainMenu)
	{
		if (!ShouldConfirmMainMenu(bProgressUnsaved))
		{
			OnMainMenuRequested.Broadcast();
			return;
		}
		// Первое нажатие ничего не решает — только спрашивает (спека).
		bConfirmingMainMenu = true;
		ApplyConfirmMainMenuLabels();
		return;
	}
	// Второе явное нажатие («Да, выйти»). Режим снимаем ДО сигнала владельцу: виджет паузы
	// переиспользуется, и в следующий раз он обязан открыться обычным видом (та же ловушка,
	// что у переспроса «Новая игра» в главном меню).
	bConfirmingMainMenu = false;
	ApplyNormalLabels();
	OnMainMenuRequested.Broadcast();
}

void UPauseMenuWidget::HandlePolicyClicked()
{
	if (const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this))
	{
		// Адреса нет — метод сам тихо ничего не делает, пустую страницу не показываем.
		Consent->OpenPrivacyPolicy();
	}
}

void UPauseMenuWidget::HandleSettingsClicked()
{
	// Экран настроек открывает владелец: виджет не знает ни про контроллер, ни про то, что
	// настройки лягут поверх паузы. Не привязано — пункт и не показывался (ApplyNormalLabels).
	UE_LOG(LogQA, Display, TEXT("QA: pause menu SETTINGS pressed"));
	OnSettingsRequested.Broadcast();
}

void UPauseMenuWidget::HandleCommunityClicked()
{
	// Тот же адрес и то же поведение, что у пункта «Сообщество» в главном меню.
	const FString Url = UMainMenuSettings::GetCommunityUrl();
	if (Url.IsEmpty())
	{
		// При пустом адресе пункт спрятан целиком, штатно сюда не попасть — строка в журнал.
		UE_LOG(LogQA, Display, TEXT("QA: pause menu COMMUNITY pressed (no url in config)"));
		return;
	}
	FString Error;
	FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
	UE_LOG(LogQA, Display, TEXT("QA: pause menu COMMUNITY pressed, url '%s'%s%s"),
		*Url, Error.IsEmpty() ? TEXT("") : TEXT(", error: "), *Error);
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
