// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA: предупреждения о недостающих кубиках
#include "ContrarySurvivor/UI/OwnerTextGuard.h" // подписи владельца код не перезаписывает
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
#include "Components/Image.h"   // фон и логотип меню (переделка 08-09)
#include "Styling/SlateBrush.h" // кисть сплошной заливки фона
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"   // TSoftObjectPtr<UTexture2D> в полях стиля
#include "HAL/PlatformProcess.h" // FPlatformProcess::LaunchURL (пункт «Сообщество»)
#include "Styling/CoreStyle.h"

FString UMainMenuSettings::GetCommunityUrl()
{
	// Значение из Config/DefaultGame.ini (раздел [/Script/ContrarySurvivor.MainMenuSettings]).
	// GetDefault отдаёт объект-по-умолчанию класса, в который движок уже загрузил конфиг.
	const UMainMenuSettings* Settings = GetDefault<UMainMenuSettings>();
	return Settings ? Settings->CommunityUrl.TrimStartAndEnd() : FString();
}

FString UMainMenuSettings::GetBugReportUrl()
{
	const UMainMenuSettings* Settings = GetDefault<UMainMenuSettings>();
	return Settings ? Settings->BugReportUrl.TrimStartAndEnd() : FString();
}

FString UMainMenuSettings::GetSupportPostUrl()
{
	const UMainMenuSettings* Settings = GetDefault<UMainMenuSettings>();
	return Settings ? Settings->SupportPostUrl.TrimStartAndEnd() : FString();
}

FString UMainMenuSettings::GetEffectiveBugReportUrl()
{
	// Решение game-lead 08-09: канал у нас один, поэтому отдельный адрес для отчётов об
	// ошибках необязателен — не заполнен, значит отчёты идут в чат сообщества.
	const FString OwnUrl = GetBugReportUrl();
	return OwnUrl.IsEmpty() ? GetCommunityUrl() : OwnUrl;
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
			{ BackgroundImage, TEXT("BackgroundImage") }, { LogoImage, TEXT("LogoImage") },
			{ TitleText, TEXT("TitleText") }, { SubtitleText, TEXT("SubtitleText") },
			{ ContinueButton, TEXT("ContinueButton") }, { ContinueText, TEXT("ContinueText") },
			{ NewGameButton, TEXT("NewGameButton") }, { NewGameText, TEXT("NewGameText") },
			{ SettingsButton, TEXT("SettingsButton") }, { SettingsText, TEXT("SettingsText") },
			{ SupportButton, TEXT("SupportButton") }, { SupportText, TEXT("SupportText") },
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
	if (SupportButton)
	{
		SupportButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleSupportClicked);
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
	// Отметка «дерево наше»: только по ней код имеет право ставить подписи поверх непустых
	// (в дереве владельца его текст не трогаем, см. UI/OwnerTextGuard.h).
	bCodeTreeBuilt = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StartRoot"));
	WidgetTree->RootWidget = Root;

	// --- Слой 1: фон на весь экран (картинка либо сплошная заливка). Кладём ПЕРВЫМ, чтобы
	// он оказался под всем остальным. Живое содержимое ставит ApplyStyle. ---
	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
	if (UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(BackgroundImage))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	// --- Слой 2: ступенчатое затемнение к низу, чтобы мелкие строки внизу читались на любой
	// картинке. Полосы кладём снизу вверх с убывающей непрозрачностью (высоту и силу задаёт
	// ApplyStyle: она зависит от полей стиля). ---
	constexpr int32 ShadeBandCount = 6;
	BottomShadeBands.Reset();
	for (int32 BandIndex = 0; BandIndex < ShadeBandCount; ++BandIndex)
	{
		UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			FName(*FString::Printf(TEXT("BottomShade%d"), BandIndex)));
		// Полосы — чистая декорация: касания сквозь них должны доходить до затемнения ниже.
		Band->SetVisibility(ESlateVisibility::HitTestInvisible);
		Root->AddChildToCanvas(Band);
		BottomShadeBands.Add(Band);
	}

	// --- Слой 3: прозрачный барьер на весь экран. Visible — ловит хит-тест, чтобы клик мимо
	// кнопок не ушёл в мир. Непрозрачность даёт фон под ним, а не этот слой. ---
	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimBorder))
	{
		DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimmerSlot->SetOffsets(FMargin(0.0f));
	}

	// --- Слой 4: логотип игры СЛЕВА, по центру высоты (просьба Рината 08-09). Размер и
	// отступ — из стиля; текстуры нет, значит кубик схлопнут и место не занимает. ---
	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogoImage"));
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToCanvas(LogoImage);

	// --- Слой 5: столбик кнопок СПРАВА, прижат к правому краю и центрирован по высоте.
	// Привязка именно к краю, а не фиксированная точка от центра: экраны у телефонов разной
	// формы, и раскладка обязана переживать другое соотношение сторон (требование лида). ---
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartFrame"));
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartPanel"));
	PanelBorder->SetPadding(FMargin(28.0f, 22.0f));
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

	// Пункты меню строятся СТРОГО по списку GetMenuRowOrder — он и есть источник правды о
	// порядке (задание издателя: «Поддержать автора» после «Настройки» и перед «Сообщество»).
	// Тот же список проверяет автотест, поэтому порядок нельзя поменять здесь незаметно.
	for (const FName& RowName : GetMenuRowOrder())
	{
		if (RowName == TEXT("ContinueButton"))
		{
			ContinueButton = MakeMenuButton(Column, Defaults.ContinueText, RowName);
			ContinueText = ContinueButton ? Cast<UTextBlock>(ContinueButton->GetContent()) : nullptr;
		}
		else if (RowName == TEXT("NewGameButton"))
		{
			NewGameButton = MakeMenuButton(Column, Defaults.NewGameText, RowName);
			NewGameText = NewGameButton ? Cast<UTextBlock>(NewGameButton->GetContent()) : nullptr;
		}
		else if (RowName == TEXT("SettingsButton"))
		{
			SettingsButton = MakeMenuButton(Column, Defaults.SettingsText, RowName);
			SettingsText = SettingsButton ? Cast<UTextBlock>(SettingsButton->GetContent()) : nullptr;
		}
		else if (RowName == TEXT("SupportButton"))
		{
			SupportButton = MakeMenuButton(Column, Defaults.SupportText, RowName);
			SupportText = SupportButton ? Cast<UTextBlock>(SupportButton->GetContent()) : nullptr;
		}
		else if (RowName == TEXT("CommunityButton"))
		{
			CommunityButton = MakeMenuButton(Column, Defaults.CommunityText, RowName);
			CommunityText = CommunityButton ? Cast<UTextBlock>(CommunityButton->GetContent()) : nullptr;
		}
		else if (RowName == TEXT("ExitButton"))
		{
			ExitButton = MakeMenuButton(Column, Defaults.ExitText, RowName);
			ExitText = ExitButton ? Cast<UTextBlock>(ExitButton->GetContent()) : nullptr;
		}
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

	// Колонка прижата к ПРАВОМУ краю (якорь 1 по X) и центрирована по высоте: на телефоне в
	// альбомной ориентации кнопки попадают под большой палец правой руки. Точный отступ от
	// края ставит ApplyStyle (поле стиля), размер — по содержимому.
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f, 1.0f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D(-64.0f, 0.0f));
	}
}

void UStartScreenWidget::ApplyStyle(const FStartScreenStyle& Style)
{
	CachedStyle = Style;

	// --- Фон и логотип применяются В ОБОИХ путях, включая дерево из дизайнера. Это не
	// оформление, а ДАННЫЕ: какую картинку показывать, задаёт поле настроек (картинку готовит
	// Ринат и подставляет туда же), а непрозрачность фона — требование спеки «фон меню
	// статичный», нарушение которого игрок уже увидел на телефоне. ---
	if (BackgroundImage)
	{
		if (ShouldFillBackgroundWithColor(Style.BackgroundTexture))
		{
			// Картинки нет — сплошная фирменная заливка. Кисть без текстуры красится
			// собственным цветом, поэтому фон остаётся полностью непрозрачным.
			FSlateBrush FillBrush;
			FillBrush.DrawAs = ESlateBrushDrawType::Image;
			FillBrush.TintColor = FSlateColor(Style.BackgroundFallbackColor);
			BackgroundImage->SetBrush(FillBrush);
		}
		else
		{
			BackgroundImage->SetBrushFromSoftTexture(Style.BackgroundTexture, /*bMatchSize=*/false);
			// Тинт сбрасываем в белый: иначе картинка ушла бы в цвет заливки.
			BackgroundImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
		}
		BackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (LogoImage)
	{
		const ESlateVisibility LogoVisibility = LogoVisibilityFor(Style.LogoTexture);
		if (LogoVisibility != ESlateVisibility::Collapsed)
		{
			LogoImage->SetBrushFromSoftTexture(Style.LogoTexture, /*bMatchSize=*/false);
			// Тинт сбрасываем в белый — как у фона. В ассете у пустого места логотипа стоит
			// бледная заливка (чтобы владелец видел, за что браться мышкой), а подстановка
			// текстуры цвет кисти НЕ трогает (UImage::SetBrushFromTexture меняет только
			// ресурс) — без этой строки живой логотип рисовался бы в четверть яркости.
			// Находку принёс unreal-operator 08-09, дефект был мой.
			LogoImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
		}
		LogoImage->SetVisibility(LogoVisibility);
	}

	// Размер ячейки логотипа — в обоих путях (см. обоснование у ApplyLogoCellSize): это
	// пропорция картинки, а не вкусовая раскладка. Благодаря этому Ринат крутит размер прямо
	// на ассете контроллера и видит результат без пересборки окна генератором.
	ApplyLogoCellSize(Style);

	// Дерево владельца из WBP_StartScreen: цвета/шрифты/размеры — его, код не перекрашивает
	// (ТЗ Рината 08-07). Тексты переключаются ниже — они зависят от режима переспроса.
	if (!bDesignerTree)
	{
		// Геометрию фоновых кубиков и колонки ставим только в кодовом дереве: в живом ассете
		// всё это двигает владелец мышкой, и код обязан держать руки при себе.
		ApplyCodeTreeLayout(Style);

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

		// Вид кнопок по макету 08-09: тёплая полупрозрачная заливка, тонкая рамка, скругление;
		// верхний пункт выделен оранжевым. Кисти собирает MakeMenuButtonStyle — тот же метод
		// зовёт генератор живого окна, поэтому запаска и WBP_StartScreen выглядят одинаково.
		// Верхний пункт остаётся выделенным и в переспросе: там он подписан «Отмена», и
		// подсвечивать безопасный выход правильнее, чем стирание прогресса.
		auto StyleMenuButton = [&Style](UButton* Button, bool bPrimary)
		{
			if (Button)
			{
				Button->SetStyle(MakeMenuButtonStyle(Style, bPrimary));
			}
		};
		StyleMenuButton(ContinueButton, /*bPrimary=*/true);
		StyleMenuButton(NewGameButton, false);
		StyleMenuButton(SettingsButton, false);
		// «Поддержать автора» красится ровно как соседние пункты: задание издателя прямо
		// запрещает выделять его цветом или делать крупнее соседей.
		StyleMenuButton(SupportButton, false);
		StyleMenuButton(CommunityButton, false);
		StyleMenuButton(ExitButton, false);

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

void UStartScreenWidget::CaptureOwnerCaptions()
{
	// Один раз и ДО первой подмены: дальше в этих кубиках может лежать текст переспроса.
	if (bOwnerCaptionsSaved)
	{
		return;
	}
	bOwnerCaptionsSaved = true;
	ContraryOwnerText::Remember(TitleText, OwnerTitleText);
	ContraryOwnerText::Remember(SubtitleText, OwnerSubtitleText);
	ContraryOwnerText::Remember(ContinueText, OwnerContinueText);
	ContraryOwnerText::Remember(NewGameText, OwnerNewGameText);
}

void UStartScreenWidget::ApplyChoiceLabels(const FStartScreenStyle& Style)
{
	// ⛔ ТЕКСТ МЕНЮ — ПРАВДА В АССЕТЕ (решение лида 24.08.2026, ADR-077 п.0). Ринат правил
	// подписи главного меню руками (ef8edc6), а прежняя версия этого метода ставила заголовок,
	// подзаголовок и все шесть подписей кнопок из стиля БЕЗУСЛОВНО — каждый запуск игры молча
	// возвращал его правки к значениям кода. Теперь код заполняет только пустой кубик и своё
	// кодовое дерево-запаску (UI/OwnerTextGuard.h). Оформление ниже — как было.
	CaptureOwnerCaptions();

	// Тексты ставим и в спрятанном состоянии (прячет их ApplyMenuRowVisibility в конце этого
	// метода): переспрос включается мгновенно, дописывать строки в тот момент негде.
	if (TitleText)
	{
		// Заголовок перебивает переспрос — здесь возвращаем авторский, а не значение стиля.
		ContraryOwnerText::Restore(TitleText, OwnerTitleText, Style.TitleText, bCodeTreeBuilt);
		if (!bDesignerTree)
		{
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
			TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
		}
	}
	if (SubtitleText)
	{
		// Подзаголовок тоже перебивает переспрос — возвращаем авторский.
		ContraryOwnerText::Restore(SubtitleText, OwnerSubtitleText, Style.SubtitleText, bCodeTreeBuilt);
		if (!bDesignerTree)
		{
			SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.SubtitleFontSize)));
			SubtitleText->SetColorAndOpacity(FSlateColor(Style.SubtitleColor));
		}
	}

	// Оформление подписи кнопки — только в своём дереве; текст ставится отдельно, по правилу
	// владельца (see UI/OwnerTextGuard.h).
	auto StyleButtonLook = [this, &Style](UTextBlock* Label, bool bPrimary)
	{
		if (Label && !bDesignerTree)
		{
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
			Label->SetColorAndOpacity(FSlateColor(MenuButtonTextColor(Style, bPrimary)));
		}
	};

	// «Продолжить» и «Новая игра» перебивает переспрос — им возврат авторской подписи;
	// остальным четырём переспрос слов не меняет, им хватает заполнения пустого кубика.
	ContraryOwnerText::Restore(ContinueText, OwnerContinueText, Style.ContinueText, bCodeTreeBuilt);
	ContraryOwnerText::Restore(NewGameText, OwnerNewGameText, Style.NewGameText, bCodeTreeBuilt);
	ContraryOwnerText::SetIfCodeOwns(SettingsText, Style.SettingsText, bCodeTreeBuilt);
	ContraryOwnerText::SetIfCodeOwns(SupportText, Style.SupportText, bCodeTreeBuilt);
	ContraryOwnerText::SetIfCodeOwns(CommunityText, Style.CommunityText, bCodeTreeBuilt);
	ContraryOwnerText::SetIfCodeOwns(ExitText, Style.ExitText, bCodeTreeBuilt);

	StyleButtonLook(ContinueText, /*bPrimary=*/true);
	StyleButtonLook(NewGameText, false);
	StyleButtonLook(SettingsText, false);
	StyleButtonLook(SupportText, false);
	StyleButtonLook(CommunityText, false);
	StyleButtonLook(ExitText, false);

	// Возврат из переспроса «Новая игра» обязан вернуть и спрятанные на его время пункты.
	ApplyMenuRowVisibility();
}

void UStartScreenWidget::ApplyConfirmLabels(const FStartScreenStyle& Style)
{
	// Переспрос «Точно начать заново?» (решение лида 08-05): те же две кнопки, другие подписи —
	// «Продолжить» временно становится «Отмена», «Новая игра» — «Да, начать заново».
	// Вопрос и пояснение показываются ТОЛЬКО здесь: в обычном меню их над кнопками нет
	// (макет 08-09), но спрашивать о стирании прогресса молча нельзя.
	//
	// ⛔ ЗДЕСЬ слова ставит КОД, и это законно: переспрос — временное состояние окна, отдельных
	// кубиков под него в ассете нет. Условие правила владельца выполняется на выходе: авторские
	// подписи запомнены до первой подмены и возвращаются в ApplyChoiceLabels при отмене.
	CaptureOwnerCaptions();

	if (TitleText)
	{
		TitleText->SetText(Style.ConfirmTitleText);
		TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!bDesignerTree)
		{
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
			TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
		}
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Style.ConfirmSubtitleText);
		SubtitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!bDesignerTree)
		{
			SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.SubtitleFontSize)));
			SubtitleText->SetColorAndOpacity(FSlateColor(Style.SubtitleColor));
		}
	}

	auto StyleButtonLabel = [this, &Style](UTextBlock* Label, const FText& Text, bool bPrimary)
	{
		if (Label)
		{
			Label->SetText(Text);
			if (!bDesignerTree)
			{
				Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
				Label->SetColorAndOpacity(FSlateColor(MenuButtonTextColor(Style, bPrimary)));
			}
		}
	};
	StyleButtonLabel(ContinueText, Style.ConfirmCancelText, /*bPrimary=*/true);
	StyleButtonLabel(NewGameText, Style.ConfirmYesText, false);

	// На время переспроса остаются ровно две кнопки — «Отмена» и «Да, начать заново»;
	// остальные пункты меню прячутся, чтобы случайный тап рядом не увёл с вопроса о стирании.
	SetRowVisibility(ContinueButton, ESlateVisibility::Visible); // переспрос идёт только поверх сейва
	SetRowVisibility(SettingsButton, ESlateVisibility::Collapsed);
	SetRowVisibility(SupportButton, ESlateVisibility::Collapsed);
	SetRowVisibility(CommunityButton, ESlateVisibility::Collapsed);
	SetRowVisibility(ExitButton, ESlateVisibility::Collapsed);
}

void UStartScreenWidget::ApplyMenuRowVisibility()
{
	// «Продолжить» без сейва не показывается вовсе — Collapsed, места в колонке не занимает
	// (спека: «не гаснет серым»).
	SetRowVisibility(ContinueButton, ContinueVisibilityFor(bHasSave));

	// Заголовок «С ВОЗВРАЩЕНИЕМ» и подпись «Найдено сохранение прошлой игры» в обычном меню
	// спрятаны всегда (макет 08-09 не показывает над кнопками ничего). Их единственное живое
	// место — переспрос «Начать заново?»: там ApplyConfirmLabels показывает обе строки, потому
	// что спрашивать о стирании прогресса молча нельзя. Решение принимается ЗДЕСЬ, в одном
	// месте на всю видимость пунктов меню.
	if (TitleText)
	{
		TitleText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (SubtitleText)
	{
		SubtitleText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// «Настройки»: пока владелец не привязал открытие экрана настроек (подход 2), пункт
	// спрятан целиком; появится сам, как только привязка появится (каркас вызова готов).
	SetRowVisibility(SettingsButton, OnSettingsRequested.IsBound()
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// «Поддержать автора» виден ВСЕГДА — ни порога по времени, ни условий в настройках у него
	// нет (задание издателя). В отличие от «Сообщества», пустой адрес его не прячет: кнопка
	// показа рекламы работает и без адреса записи, а спрятать пункт целиком значило бы отнять
	// у игрока единственную добровольную точку поддержки.
	SetRowVisibility(SupportButton, SupportVisibilityFor());

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
			// Подпись — статичная, значит принадлежит владельцу ассета: настройка проекта
			// заполняет только пустой кубик (правило UI/OwnerTextGuard.h). Строка версии ниже —
			// наоборот, живые данные, её ставит код всегда.
			ContraryOwnerText::SetIfCodeOwns(PolicyText, Settings->PauseMenuPolicyText, bCodeTreeBuilt);
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

ESlateVisibility UStartScreenWidget::LogoVisibilityFor(const TSoftObjectPtr<UTexture2D>& LogoTexture)
{
	// Логотипа игры отдельной картинкой в проекте пока нет: пустое поле обязано означать
	// «просто не рисуем», а не пустой прямоугольник посреди меню.
	return LogoTexture.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
}

FButtonStyle UStartScreenWidget::MakeMenuButtonStyle(const FStartScreenStyle& Style, bool bPrimary)
{
	const FLinearColor Fill = bPrimary ? Style.PrimaryButtonFillColor : Style.ButtonFillColor;
	const FLinearColor Border = bPrimary ? Style.PrimaryButtonBorderColor : Style.ButtonBorderColor;
	const float Radius = FMath::Max(0.0f, Style.ButtonCornerRadius);
	const float BorderWidth = FMath::Max(0.0f, Style.ButtonBorderWidth);

	// Наведение и нажатие выводим из заливки: под пальцем кнопка светлеет, при нажатии темнеет.
	// Прозрачность у всех состояний одна — иначе кнопка на касании «мигала» бы фоном.
	auto Shade = [&Fill](float Scale, float TowardsWhite)
	{
		FLinearColor Result = Fill * Scale;
		Result = FMath::Lerp(Result, FLinearColor(1.0f, 1.0f, 1.0f, Fill.A), TowardsWhite);
		Result.A = Fill.A;
		return Result;
	};

	auto MakeBrush = [Radius, BorderWidth, &Border](const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Border);
		Brush.OutlineSettings.Width = BorderWidth;
		return Brush;
	};

	FButtonStyle Result;
	Result.Normal = MakeBrush(Fill);
	Result.Hovered = MakeBrush(Shade(1.0f, 0.10f));
	Result.Pressed = MakeBrush(Shade(0.75f, 0.0f));
	Result.Disabled = MakeBrush(FLinearColor(Fill.R, Fill.G, Fill.B, Fill.A * 0.5f));
	Result.NormalPadding = FMargin(0.0f);
	Result.PressedPadding = FMargin(0.0f);
	return Result;
}

ESlateVisibility UStartScreenWidget::SupportVisibilityFor()
{
	// Задание издателя: пункт доступен всегда, без порогов и условий.
	return ESlateVisibility::Visible;
}

TArray<FName> UStartScreenWidget::GetMenuRowOrder()
{
	// Порядок сверху вниз. ⛔ «Поддержать автора» стоит строго после «Настройки» и перед
	// «Сообщество» — это дословное требование задания издателя, менять нельзя.
	return TArray<FName>{
		TEXT("ContinueButton"),
		TEXT("NewGameButton"),
		TEXT("SettingsButton"),
		TEXT("SupportButton"),
		TEXT("CommunityButton"),
		TEXT("ExitButton"),
	};
}

bool UStartScreenWidget::ShouldFillBackgroundWithColor(const TSoftObjectPtr<UTexture2D>& BackgroundTexture)
{
	// Картинки нет — красим сплошным фирменным цветом. Прозрачным фон не остаётся НИКОГДА:
	// спека требует статичный фон, а игрок на телефоне увидел сквозь меню живую игру.
	return BackgroundTexture.IsNull();
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

void UStartScreenWidget::ApplyLogoCellSize(const FStartScreenStyle& Style)
{
	// Размер логотипа — единственная геометрия, которую код ставит и в дизайнерском дереве
	// (обоснование — у объявления метода: это пропорция картинки, а не раскладка). Нулевой
	// или отрицательный размер игнорируем: он спрятал бы логотип молча, а поле правит человек.
	if (!LogoImage || Style.LogoSize.X <= 0.0f || Style.LogoSize.Y <= 0.0f)
	{
		return;
	}
	if (UCanvasPanelSlot* LogoSlot = Cast<UCanvasPanelSlot>(LogoImage->Slot))
	{
		LogoSlot->SetSize(Style.LogoSize);
	}
}

void UStartScreenWidget::ApplyCodeTreeLayout(const FStartScreenStyle& Style)
{
	// Логотип: прижат к ЛЕВОМУ краю, центр по высоте. Размер ставит ApplyLogoCellSize —
	// он общий для обоих путей, здесь только привязка и отступ кодового дерева.
	if (LogoImage)
	{
		if (UCanvasPanelSlot* LogoSlot = Cast<UCanvasPanelSlot>(LogoImage->Slot))
		{
			LogoSlot->SetAnchors(FAnchors(0.0f, 0.5f, 0.0f, 0.5f));
			LogoSlot->SetAlignment(FVector2D(0.0f, 0.5f));
			LogoSlot->SetPosition(FVector2D(Style.LogoLeftMargin, 0.0f));
		}
	}

	// Колонка кнопок: прижата к ПРАВОМУ краю на заданный отступ.
	if (FrameBorder)
	{
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(FrameBorder->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f, 1.0f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
			PanelSlot->SetPosition(FVector2D(-FMath::Max(0.0f, Style.ButtonsRightMargin), 0.0f));
		}
	}

	// Зазор между пунктами колонки.
	for (USizeBox* Box : ButtonBoxes)
	{
		if (Box)
		{
			if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Box->Slot))
			{
				BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, FMath::Max(0.0f, Style.ButtonSpacing)));
			}
		}
	}

	// Затемнение к низу: полосы одинаковой высоты, непрозрачность растёт к нижнему краю.
	// Самая нижняя полоса — заданной силы, верхняя — почти прозрачная.
	const int32 BandCount = BottomShadeBands.Num();
	const float ShadeFraction = FMath::Clamp(Style.BottomShadeHeightFraction, 0.0f, 1.0f);
	for (int32 BandIndex = 0; BandIndex < BandCount; ++BandIndex)
	{
		UBorder* Band = BottomShadeBands[BandIndex];
		if (!Band)
		{
			continue;
		}
		// Доля высоты экрана: полосы идут снизу вверх, каждая занимает равную часть зоны.
		const float BandHeight = (BandCount > 0) ? ShadeFraction / BandCount : 0.0f;
		const float BandTop = 1.0f - BandHeight * (BandIndex + 1);
		const float BandBottom = 1.0f - BandHeight * BandIndex;

		// Сила: у самой нижней полосы (индекс 0) максимум, дальше линейно к нулю.
		const float BandAlpha = (BandCount > 0)
			? Style.BottomShadeOpacity * (1.0f - static_cast<float>(BandIndex) / BandCount)
			: 0.0f;
		Band->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, FMath::Clamp(BandAlpha, 0.0f, 1.0f)));

		if (UCanvasPanelSlot* BandSlot = Cast<UCanvasPanelSlot>(Band->Slot))
		{
			BandSlot->SetAnchors(FAnchors(0.0f, BandTop, 1.0f, BandBottom));
			BandSlot->SetOffsets(FMargin(0.0f));
		}
		// Нулевая зона затемнения — полос не видно вовсе.
		Band->SetVisibility(ShadeFraction > 0.0f && Style.BottomShadeOpacity > 0.0f
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
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

void UStartScreenWidget::HandleSupportClicked()
{
	// Виджет только сообщает: окно «Поддержать автора» открывает владелец (контроллер).
	OnSupportRequested.Broadcast();
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
