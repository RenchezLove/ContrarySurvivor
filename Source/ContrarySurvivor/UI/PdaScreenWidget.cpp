// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Actors/TalkerNPC.h"
#include "ContrarySurvivor/Actors/VillageZone.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/MapExplorationComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryItemRow.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/UI/CorpseLootWidget.h"   // PickMoneyWord — склонение слова «монета»
#include "ContrarySurvivor/UI/PdaMapMath.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "ContrarySurvivor/UI/QuestTargets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "Misc/App.h" // CanEverRender: в служебных запусках без отрисовки картинку черноты не заводим
#include "Rendering/DrawElements.h"
#include "TextureResource.h" // данные картинки-маски черноты
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
	const FLinearColor PdaTextColor(0.95f, 0.96f, 1.0f, 1.0f);
	const FLinearColor PdaDimTextColor(0.72f, 0.74f, 0.78f, 1.0f);
	const FLinearColor PdaGoldColor(1.0f, 0.85f, 0.3f, 1.0f);

	// Шрифт окна. Roboto движка берётся ОБЪЕКТОМ — так же, как в окнах из генератора: такой
	// шрифт сохраняется в файл окна. Объекта нет (сборка без него) — встроенный шрифт интерфейса.
	FSlateFontInfo PdaFont(int32 Size, const TCHAR* Typeface)
	{
		static TWeakObjectPtr<UObject> Roboto;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			Roboto = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		}
		if (UObject* Font = Roboto.Get())
		{
			return FSlateFontInfo(Font, Size, FName(Typeface));
		}
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}

	FSlateBrush RoundedBrush(const FLinearColor& Tint, float Radius,
		const FLinearColor& OutlineColor = FLinearColor::Transparent, float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	FSlateBrush CircleBrush(const FLinearColor& Tint,
		const FLinearColor& OutlineColor = FLinearColor::Transparent, float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.OutlineSettings.Color = FSlateColor(OutlineColor);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	FButtonStyle FlatButtonStyle(const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed)
	{
		FButtonStyle Style;
		Style.Normal = RoundedBrush(Normal, 4.0f);
		Style.Hovered = RoundedBrush(Hovered, 4.0f);
		Style.Pressed = RoundedBrush(Pressed, 4.0f);
		Style.Disabled = RoundedBrush(FLinearColor(Normal.R, Normal.G, Normal.B, Normal.A * 0.5f), 4.0f);
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		return Style;
	}

	FButtonStyle CircleButtonStyle(const FLinearColor& Color)
	{
		const FLinearColor Outline(0.0f, 0.0f, 0.0f, Color.A * 0.85f);
		FButtonStyle Style;
		Style.Normal = CircleBrush(Color, Outline, 2.0f);
		Style.Hovered = CircleBrush(Color * 1.2f, Outline, 2.0f);
		Style.Pressed = CircleBrush(Color * 0.8f, Outline, 2.0f);
		Style.Disabled = Style.Normal;
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		return Style;
	}

	UTextBlock* NewText(UWidgetTree* Tree, const FName& Name, const FText& Text,
		const FLinearColor& Color, int32 Size, const TCHAR* Typeface)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		Block->SetFont(PdaFont(Size, Typeface));
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->bIsVariable = true;
		return Block;
	}

	UButton* NewCaptionButton(UWidgetTree* Tree, const FName& ButtonName, const FName& TextName,
		const FText& Caption, const FLinearColor& Normal)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->SetStyle(FlatButtonStyle(Normal, Normal * 1.25f, Normal * 0.8f));
		Button->bIsVariable = true;
		UTextBlock* Label = NewText(Tree, TextName, Caption, PdaTextColor, 16, TEXT("Bold"));
		Label->SetJustification(ETextJustify::Center);
		Button->SetContent(Label);
		return Button;
	}

	void PlaceAt(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Pos, const FVector2D& Size)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetAnchors(FAnchors());
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetPosition(Pos);
			Slot->SetSize(Size);
		}
	}

	void PlaceAuto(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Pos)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetAnchors(FAnchors());
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetPosition(Pos);
			Slot->SetAutoSize(true);
		}
	}

	void PlaceStretch(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(Offsets);
		}
	}
}

// ===========================================================================
// Кнопка с заданием
// ===========================================================================

void UPdaQuestButton::BindPick()
{
	OnClicked.AddUniqueDynamic(this, &UPdaQuestButton::HandlePicked);
}

void UPdaQuestButton::HandlePicked()
{
	OnPicked.ExecuteIfBound(QuestId);
}

// ===========================================================================
// Раскладка окна — одна на готовый файл и на кодовую запаску
// ===========================================================================

void UPdaScreenWidget::BuildDefaultTree(UWidgetTree* Tree)
{
	if (!Tree)
	{
		return;
	}
	const UPdaScreenWidget* Defaults = GetDefault<UPdaScreenWidget>();

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Tree->RootWidget = Root;

	// Затемнение на весь экран; Visible — нажатия в мир не проходят.
	UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Dim->SetVisibility(ESlateVisibility::Visible);
	Dim->bIsVariable = true;
	PlaceStretch(Root, Dim, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	// Панель 1200x720 по центру — тёмная с золотой рамкой, как сумка и магазин; внутри свой
	// холст, содержимое в области за вычетом отступа 16 (1168x688).
	UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
	Plate->SetBrush(RoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.95f), 6.0f,
		FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
	Plate->SetPadding(FMargin(16.0f));
	Plate->bIsVariable = true;
	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
	{
		PlateSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PlateSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PlateSlot->SetPosition(FVector2D::ZeroVector);
		PlateSlot->SetSize(FVector2D(1200.0f, 720.0f));
	}
	UCanvasPanel* Panel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
	Plate->SetContent(Panel);

	// --- Верхняя строка: заголовок, две вкладки, «Закрыть» ---
	PlaceAuto(Panel, NewText(Tree, TEXT("TitleText"), Defaults->TitleLabel, PdaTextColor, 22, TEXT("Bold")),
		FVector2D(0.0f, 4.0f));

	const FLinearColor TabColor(0.18f, 0.20f, 0.24f, 1.0f);
	PlaceAt(Panel, NewCaptionButton(Tree, TEXT("QuestsTabButton"), TEXT("QuestsTabText"),
		Defaults->QuestsTabLabel, TabColor), FVector2D(120.0f, 0.0f), FVector2D(180.0f, 40.0f));
	PlaceAt(Panel, NewCaptionButton(Tree, TEXT("MapTabButton"), TEXT("MapTabText"),
		Defaults->MapTabLabel, TabColor), FVector2D(310.0f, 0.0f), FVector2D(180.0f, 40.0f));

	UButton* Close = NewCaptionButton(Tree, TEXT("CloseButton"), TEXT("CloseText"),
		Defaults->CloseLabel, FLinearColor(0.3f, 0.3f, 0.34f, 1.0f));
	if (UCanvasPanelSlot* CloseSlot = Panel->AddChildToCanvas(Close))
	{
		CloseSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		CloseSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		CloseSlot->SetPosition(FVector2D::ZeroVector);
		CloseSlot->SetSize(FVector2D(150.0f, 40.0f));
	}

	// --- Вкладка «Задания»: слева список, справа сведения о выбранном ---
	UCanvasPanel* Quests = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestsPanel"));
	Quests->bIsVariable = true;
	PlaceStretch(Panel, Quests, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f, 56.0f, 0.0f, 0.0f));

	UScrollBox* List = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("QuestList"));
	List->bIsVariable = true;
	// Растянут по высоте панели, ширина 440 (при совпавших якорях правый отступ — это ширина).
	PlaceStretch(Quests, List, FAnchors(0.0f, 0.0f, 0.0f, 1.0f), FMargin(0.0f, 0.0f, 440.0f, 0.0f));

	PlaceAuto(Quests, NewText(Tree, TEXT("EmptyText"), Defaults->EmptyLabel, PdaDimTextColor, 16, TEXT("Regular")),
		FVector2D(8.0f, 8.0f));

	const float DetailX = 464.0f;
	const float DetailW = 704.0f;
	PlaceAuto(Quests, NewText(Tree, TEXT("DetailTitleText"), FText::GetEmpty(), PdaTextColor, 20, TEXT("Bold")),
		FVector2D(DetailX, 0.0f));
	PlaceAuto(Quests, NewText(Tree, TEXT("DetailGiverText"), FText::GetEmpty(), PdaDimTextColor, 15, TEXT("Regular")),
		FVector2D(DetailX, 36.0f));

	UTextBlock* Description = NewText(Tree, TEXT("DetailDescriptionText"), FText::GetEmpty(), PdaTextColor, 16, TEXT("Regular"));
	Description->SetAutoWrapText(true);
	PlaceAt(Quests, Description, FVector2D(DetailX, 92.0f), FVector2D(DetailW, 160.0f));

	UTextBlock* Objectives = NewText(Tree, TEXT("DetailObjectivesText"), FText::GetEmpty(), PdaTextColor, 16, TEXT("Regular"));
	Objectives->SetAutoWrapText(true);
	PlaceAt(Quests, Objectives, FVector2D(DetailX, 262.0f), FVector2D(DetailW, 120.0f));

	UTextBlock* Reward = NewText(Tree, TEXT("DetailRewardText"), FText::GetEmpty(), PdaGoldColor, 16, TEXT("Regular"));
	Reward->SetAutoWrapText(true);
	PlaceAt(Quests, Reward, FVector2D(DetailX, 392.0f), FVector2D(DetailW, 90.0f));

	UButton* Track = NewCaptionButton(Tree, TEXT("TrackButton"), TEXT("TrackText"),
		Defaults->TrackLabel, FLinearColor(0.16f, 0.36f, 0.16f, 1.0f));
	if (UCanvasPanelSlot* TrackSlot = Quests->AddChildToCanvas(Track))
	{
		TrackSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		TrackSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		TrackSlot->SetPosition(FVector2D(DetailX, 0.0f));
		TrackSlot->SetSize(FVector2D(280.0f, 46.0f));
	}

	// --- Вкладка «Карта» (занимает то же место, что «Задания»; видимость вкладок ведёт код) ---
	UCanvasPanel* Map = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapPanel"));
	Map->bIsVariable = true;
	PlaceStretch(Panel, Map, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f, 56.0f, 0.0f, 0.0f));

	// Поле карты. В игре код вписывает его в панель с пропорциями прямоугольника мира.
	UCanvasPanel* Area = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapArea"));
	Area->bIsVariable = true;
	PlaceStretch(Map, Area, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	UBorder* Background = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapBackground"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.04f, 1.0f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	Background->bIsVariable = true;
	PlaceStretch(Area, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	// Картинка карты: скрыта, пока картинка не задана в настройках (пустая рисовалась бы белым).
	UImage* Picture = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MapImage"));
	Picture->SetVisibility(ESlateVisibility::Collapsed);
	Picture->bIsVariable = true;
	PlaceStretch(Area, Picture, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	// Чернота неизведанного: поверх картинки, под отметками. Картинку-маску ставит код игры;
	// до первой отрисовки элемент спрятан (без маски он закрасил бы карту целиком).
	UImage* Fog = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MapFogImage"));
	Fog->SetVisibility(ESlateVisibility::Collapsed);
	Fog->bIsVariable = true;
	PlaceStretch(Area, Fog, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	UCanvasPanel* Markers = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapMarkers"));
	Markers->bIsVariable = true;
	PlaceStretch(Area, Markers, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	UTextBlock* Hint = NewText(Tree, TEXT("MapHintText"), Defaults->MapHintLabel, PdaDimTextColor, 13, TEXT("Regular"));
	if (UCanvasPanelSlot* HintSlot = Map->AddChildToCanvas(Hint))
	{
		HintSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		HintSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		HintSlot->SetPosition(FVector2D(4.0f, 0.0f));
		HintSlot->SetAutoSize(true);
	}
}

// ===========================================================================
// Запуск и привязки
// ===========================================================================

void UPdaScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureSetup();
}

void UPdaScreenWidget::EnsureSetup()
{
	if (bSetupDone)
	{
		return;
	}
	bSetupDone = true;

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		// Готового файла окна нет — то же дерево строим кодом.
		BuildDefaultTree(WidgetTree);
		BindCodeTree();
	}

	if (QuestsTabButton)
	{
		QuestsTabButton->OnClicked.AddUniqueDynamic(this, &UPdaScreenWidget::HandleQuestsTabClicked);
	}
	if (MapTabButton)
	{
		MapTabButton->OnClicked.AddUniqueDynamic(this, &UPdaScreenWidget::HandleMapTabClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UPdaScreenWidget::HandleCloseClicked);
	}
	if (TrackButton)
	{
		TrackButton->OnClicked.AddUniqueDynamic(this, &UPdaScreenWidget::HandleTrackClicked);
	}
}

void UPdaScreenWidget::BindCodeTree()
{
#define PDA_BIND(Field, Type) Field = Cast<Type>(WidgetTree->FindWidget(FName(TEXT(#Field))))
	PDA_BIND(TitleText, UTextBlock);
	PDA_BIND(QuestsTabButton, UButton);
	PDA_BIND(QuestsTabText, UTextBlock);
	PDA_BIND(MapTabButton, UButton);
	PDA_BIND(MapTabText, UTextBlock);
	PDA_BIND(CloseButton, UButton);
	PDA_BIND(CloseText, UTextBlock);
	PDA_BIND(QuestsPanel, UCanvasPanel);
	PDA_BIND(QuestList, UScrollBox);
	PDA_BIND(EmptyText, UTextBlock);
	PDA_BIND(DetailTitleText, UTextBlock);
	PDA_BIND(DetailGiverText, UTextBlock);
	PDA_BIND(DetailDescriptionText, UTextBlock);
	PDA_BIND(DetailObjectivesText, UTextBlock);
	PDA_BIND(DetailRewardText, UTextBlock);
	PDA_BIND(TrackButton, UButton);
	PDA_BIND(TrackText, UTextBlock);
	PDA_BIND(MapPanel, UCanvasPanel);
	PDA_BIND(MapArea, UCanvasPanel);
	PDA_BIND(MapBackground, UBorder);
	PDA_BIND(MapImage, UImage);
	PDA_BIND(MapFogImage, UImage);
	PDA_BIND(MapMarkers, UCanvasPanel);
	PDA_BIND(MapHintText, UTextBlock);
#undef PDA_BIND
}

void UPdaScreenWidget::InitPda(APlayerCharacter* InPlayer)
{
	EnsureSetup();
	Player = InPlayer;
	LastFittedPanelSize = FVector2D::ZeroVector; // поле карты подгоняется заново (размер окна мог смениться)

	RefreshNow();
	RefreshTabLook();
}

void UPdaScreenWidget::RefreshNow()
{
	bRefreshPending = false;
	LastJournalStamp = ComputeJournalStamp();
	RebuildQuestList();
	RefreshDetails();
	RebuildMap();
}

void UPdaScreenWidget::ShowTab(bool bMap)
{
	bMapTab = bMap;
	RefreshTabLook();
}

void UPdaScreenWidget::RefreshTabLook()
{
	if (QuestsPanel)
	{
		QuestsPanel->SetVisibility(bMapTab ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (MapPanel)
	{
		MapPanel->SetVisibility(bMapTab ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// Открытая вкладка подписана золотым, вторая — приглушённым.
	if (QuestsTabText)
	{
		QuestsTabText->SetColorAndOpacity(FSlateColor(bMapTab ? PdaDimTextColor : PdaGoldColor));
	}
	if (MapTabText)
	{
		MapTabText->SetColorAndOpacity(FSlateColor(bMapTab ? PdaGoldColor : PdaDimTextColor));
	}
}

void UPdaScreenWidget::HandleQuestsTabClicked()
{
	ShowTab(false);
}

void UPdaScreenWidget::HandleMapTabClicked()
{
	ShowTab(true);
}

void UPdaScreenWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

void UPdaScreenWidget::HandleTrackClicked()
{
	TrackQuest(SelectedQuestId);
}

// ===========================================================================
// Вкладка «Задания»
// ===========================================================================

void UPdaScreenWidget::BuildJournalView(const TArray<FQuest>& Journal, FName TrackedQuestId, FPdaJournalView& OutView)
{
	OutView = FPdaJournalView();
	for (const FQuest& Quest : Journal)
	{
		if (Quest.State == EQuestState::TurnedIn)
		{
			OutView.Done.Add(Quest.QuestId);
		}
		else if (UQuestComponent::IsInActivePhase(Quest))
		{
			(Quest.Kind == EQuestKind::Side ? OutView.Side : OutView.Main).Add(Quest.QuestId);
			if (Quest.QuestId == TrackedQuestId)
			{
				OutView.Tracked = TrackedQuestId;
			}
		}
		// Предложенное, но не взятое задание в журнал КПК не попадает.
	}
}

uint32 UPdaScreenWidget::ComputeJournalStamp() const
{
	const UQuestComponent* Quests = Player.IsValid() ? Player->GetQuests() : nullptr;
	if (!Quests)
	{
		return 0;
	}
	uint32 Stamp = GetTypeHash(Quests->GetQuests().Num());
	if (const FQuest* Tracked = Quests->GetTrackedQuest())
	{
		Stamp = HashCombine(Stamp, GetTypeHash(Tracked->QuestId));
	}
	for (const FQuest& Quest : Quests->GetQuests())
	{
		Stamp = HashCombine(Stamp, GetTypeHash(Quest.QuestId));
		Stamp = HashCombine(Stamp, GetTypeHash(static_cast<uint8>(Quest.State)));
		Stamp = HashCombine(Stamp, GetTypeHash(static_cast<uint8>(Quest.Kind)));
		Stamp = HashCombine(Stamp, GetTypeHash(Quest.Progress));
		Stamp = HashCombine(Stamp, GetTypeHash(Quest.ItemProgress));
	}
	return Stamp;
}

void UPdaScreenWidget::AddSectionHeader(const FText& Text)
{
	UTextBlock* Header = NewText(WidgetTree, NAME_None, Text, PdaDimTextColor, 14, TEXT("Bold"));
	QuestList->AddChild(Header);
	if (UScrollBoxSlot* HeaderSlot = Cast<UScrollBoxSlot>(Header->Slot))
	{
		HeaderSlot->SetPadding(FMargin(6.0f, 10.0f, 6.0f, 4.0f));
	}
}

void UPdaScreenWidget::AddQuestRow(const FQuest& Quest, bool bTracked)
{
	const bool bSelected = (Quest.QuestId == SelectedQuestId);

	UPdaQuestButton* Row = WidgetTree->ConstructWidget<UPdaQuestButton>(UPdaQuestButton::StaticClass());
	Row->QuestId = Quest.QuestId;
	Row->BindPick();
	Row->OnPicked.BindUObject(this, &UPdaScreenWidget::SelectQuest);
	const FLinearColor RowColor = bSelected
		? FLinearColor(0.22f, 0.24f, 0.29f, 1.0f) : FLinearColor(0.11f, 0.12f, 0.15f, 1.0f);
	Row->SetStyle(FlatButtonStyle(RowColor, RowColor * 1.25f, RowColor * 0.8f));

	FFormatNamedArguments Args;
	Args.Add(TEXT("Title"), Quest.Title);
	UTextBlock* Title = NewText(WidgetTree, NAME_None,
		bTracked ? FText::Format(TrackedRowFormat, Args) : Quest.Title,
		UContraryPdaSettings::Get()->GetQuestKindColor(Quest.Kind), 17, bTracked ? TEXT("Bold") : TEXT("Regular"));
	Row->SetContent(Title);
	if (UButtonSlot* TitleSlot = Cast<UButtonSlot>(Title->Slot))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Left);
		TitleSlot->SetPadding(FMargin(12.0f, 10.0f));
	}

	QuestList->AddChild(Row);
	if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
	{
		RowSlot->SetPadding(FMargin(0.0f, 2.0f, 8.0f, 2.0f));
	}
	++QuestRowCount;
}

void UPdaScreenWidget::RebuildQuestList()
{
	QuestRowCount = 0;
	const UQuestComponent* Quests = Player.IsValid() ? Player->GetQuests() : nullptr;

	FPdaJournalView View;
	if (Quests)
	{
		const FQuest* Tracked = Quests->GetTrackedQuest();
		BuildJournalView(Quests->GetQuests(), Tracked ? Tracked->QuestId : NAME_None, View);
	}

	// Выбранное задание обязано идти; иначе выбираем отслеживаемое либо первое в списке.
	if (!View.Main.Contains(SelectedQuestId) && !View.Side.Contains(SelectedQuestId))
	{
		SelectedQuestId = !View.Tracked.IsNone() ? View.Tracked
			: (View.Main.Num() > 0 ? View.Main[0] : (View.Side.Num() > 0 ? View.Side[0] : NAME_None));
	}

	if (EmptyText)
	{
		EmptyText->SetVisibility(View.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!QuestList || !Quests)
	{
		return;
	}
	QuestList->ClearChildren();

	auto AddSection = [this, Quests, &View](const FText& Header, const TArray<FName>& Ids)
	{
		if (Ids.Num() == 0)
		{
			return;
		}
		AddSectionHeader(Header);
		for (const FName& Id : Ids)
		{
			if (const FQuest* Quest = Quests->FindQuest(Id))
			{
				AddQuestRow(*Quest, Id == View.Tracked);
			}
		}
	};
	AddSection(MainHeader, View.Main);
	AddSection(SideHeader, View.Side);

	// «Выполненные»: свёрнутый раздел, по нажатию — только названия.
	if (View.Done.Num() > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), FText::AsNumber(View.Done.Num()));

		UPdaQuestButton* Toggle = WidgetTree->ConstructWidget<UPdaQuestButton>(UPdaQuestButton::StaticClass());
		Toggle->BindPick();
		Toggle->OnPicked.BindWeakLambda(this, [this](FName) { ToggleDoneSection(); });
		const FLinearColor ToggleColor(0.09f, 0.10f, 0.12f, 1.0f);
		Toggle->SetStyle(FlatButtonStyle(ToggleColor, ToggleColor * 1.3f, ToggleColor * 0.8f));
		UTextBlock* ToggleText = NewText(WidgetTree, NAME_None,
			FText::Format(bDoneExpanded ? DoneHeaderExpanded : DoneHeaderCollapsed, Args),
			PdaDimTextColor, 14, TEXT("Bold"));
		Toggle->SetContent(ToggleText);
		if (UButtonSlot* ToggleSlot = Cast<UButtonSlot>(ToggleText->Slot))
		{
			ToggleSlot->SetHorizontalAlignment(HAlign_Left);
			ToggleSlot->SetPadding(FMargin(6.0f, 8.0f));
		}
		QuestList->AddChild(Toggle);
		if (UScrollBoxSlot* ToggleRowSlot = Cast<UScrollBoxSlot>(Toggle->Slot))
		{
			ToggleRowSlot->SetPadding(FMargin(0.0f, 12.0f, 8.0f, 2.0f));
		}

		if (bDoneExpanded)
		{
			for (const FName& Id : View.Done)
			{
				if (const FQuest* Quest = Quests->FindQuest(Id))
				{
					UTextBlock* DoneTitle = NewText(WidgetTree, NAME_None, Quest->Title, PdaDimTextColor, 15, TEXT("Regular"));
					QuestList->AddChild(DoneTitle);
					if (UScrollBoxSlot* DoneSlot = Cast<UScrollBoxSlot>(DoneTitle->Slot))
					{
						DoneSlot->SetPadding(FMargin(12.0f, 4.0f, 8.0f, 4.0f));
					}
				}
			}
		}
	}
}

void UPdaScreenWidget::SelectQuest(FName QuestId)
{
	SelectedQuestId = QuestId;
	RefreshDetails();
	// Список перестраиваем на следующем кадре: сейчас мы внутри нажатия на его же строку.
	bRefreshPending = true;
}

void UPdaScreenWidget::ToggleDoneSection()
{
	bDoneExpanded = !bDoneExpanded;
	bRefreshPending = true;
}

bool UPdaScreenWidget::TrackQuest(FName QuestId)
{
	UQuestComponent* Quests = Player.IsValid() ? Player->GetQuests() : nullptr;
	if (!Quests || !Quests->SetTrackedQuest(QuestId))
	{
		return false;
	}
	SelectedQuestId = QuestId;
	RefreshDetails();
	bRefreshPending = true; // строки списка и отметки карты перекрасятся на следующем кадре
	return true;
}

FText UPdaScreenWidget::BuildRewardText(const FQuest& Quest) const
{
	TArray<FText> Parts;

	const int32 Money = FMath::RoundToInt32(Quest.RewardMoney);
	if (Money > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FText::AsNumber(Money));
		Args.Add(TEXT("Word"), UCorpseLootWidget::PickMoneyWord(Money, MoneyWordOne, MoneyWordFew, MoneyWordMany));
		Parts.Add(FText::Format(RewardMoneyFormat, Args));
	}
	for (const FQuestItemGrant& Grant : Quest.RewardItems)
	{
		// Название — из таблицы предметов; строки нет — её имя (лучше, чем пропустить награду).
		const FContraryItemRow* ItemRow = ContraryItems::FindRow(Grant.ItemRow);
		FFormatNamedArguments Args;
		Args.Add(TEXT("Item"), (ItemRow && !ItemRow->DisplayText.IsEmpty())
			? ItemRow->DisplayText : FText::FromName(Grant.ItemRow));
		Args.Add(TEXT("Count"), FText::AsNumber(Grant.Count));
		Parts.Add(FText::Format(RewardItemFormat, Args));
	}
	if (Parts.Num() == 0)
	{
		return FText::GetEmpty();
	}
	FFormatNamedArguments Args;
	Args.Add(TEXT("Reward"), FText::Join(FText::FromString(TEXT(", ")), Parts));
	return FText::Format(RewardFormat, Args);
}

void UPdaScreenWidget::RefreshDetails()
{
	const UQuestComponent* Quests = Player.IsValid() ? Player->GetQuests() : nullptr;
	const FQuest* Quest = (Quests && !SelectedQuestId.IsNone()) ? Quests->FindQuest(SelectedQuestId) : nullptr;

	auto SetOrClear = [](UTextBlock* Block, const FText& Text)
	{
		if (Block)
		{
			Block->SetText(Text);
		}
	};

	if (!Quest)
	{
		SetOrClear(DetailTitleText, FText::GetEmpty());
		SetOrClear(DetailGiverText, FText::GetEmpty());
		SetOrClear(DetailDescriptionText, FText::GetEmpty());
		SetOrClear(DetailObjectivesText, FText::GetEmpty());
		SetOrClear(DetailRewardText, FText::GetEmpty());
		if (TrackButton)
		{
			TrackButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	SetOrClear(DetailTitleText, Quest->Title);
	if (DetailTitleText)
	{
		DetailTitleText->SetColorAndOpacity(FSlateColor(UContraryPdaSettings::Get()->GetQuestKindColor(Quest->Kind)));
	}

	// Кто выдал и кому сдавать — именами персонажей с уровня.
	{
		UWorld* World = Player.IsValid() ? Player->GetWorld() : nullptr;
		TArray<FText> Lines;
		const FText Giver = QuestTargets::ResolveGiverName(World, *Quest);
		if (!Giver.IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Giver"), Giver);
			Lines.Add(FText::Format(GiverFormat, Args));
		}
		const FText Receiver = QuestTargets::ResolveReceiverName(World, *Quest);
		if (!Receiver.IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Receiver"), Receiver);
			Lines.Add(FText::Format(ReceiverFormat, Args));
		}
		SetOrClear(DetailGiverText, FText::Join(FText::FromString(TEXT("\n")), Lines));
	}

	SetOrClear(DetailDescriptionText, Quest->Description);

	// Цели с прогрессом — тем же сборщиком, что строка задания на экране.
	{
		TArray<FText> Lines;
		const FText Objectives = QuestObjectiveText::BuildObjectives(*Quest, ObjectiveFormat, FText::FromString(TEXT("\n")));
		if (!Objectives.IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Objectives"), Objectives);
			Lines.Add(FText::Format(ObjectivesFormat, Args));
		}
		if (Quest->State == EQuestState::Completed)
		{
			Lines.Add(ReadyText);
		}
		else if (Quest->State == EQuestState::TurnedIn)
		{
			Lines.Add(TurnedInText);
		}
		SetOrClear(DetailObjectivesText, FText::Join(FText::FromString(TEXT("\n")), Lines));
	}

	SetOrClear(DetailRewardText, BuildRewardText(*Quest));

	if (TrackButton)
	{
		const bool bCanTrack = UQuestComponent::IsInActivePhase(*Quest);
		const bool bTracked = Quests->GetTrackedQuest() == Quest;
		TrackButton->SetVisibility(bCanTrack ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		TrackButton->SetIsEnabled(!bTracked);
		SetOrClear(TrackText, bTracked ? TrackedLabel : TrackLabel);
	}
}

// ===========================================================================
// Вкладка «Карта»
// ===========================================================================

void UPdaScreenWidget::AddGridLines()
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FVector2D TopLeft = Settings->MapWorldTopLeft;
	const FVector2D BottomRight = Settings->MapWorldBottomRight;
	const double Step = FMath::Max(200.0, static_cast<double>(GridStepWorld));

	auto AddLine = [this](const FAnchors& Anchors, const FMargin& Offsets)
	{
		UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Line->SetBrushColor(GridColor);
		Line->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceStretch(MapMarkers, Line, Anchors, Offsets);
	};

	// Линии по круглым значениям мира; не больше сотни на ось (защита от слишком мелкого шага).
	const double MinX = FMath::Min(TopLeft.X, BottomRight.X), MaxX = FMath::Max(TopLeft.X, BottomRight.X);
	int32 Count = 0;
	for (double X = FMath::CeilToDouble(MinX / Step) * Step; X < MaxX && Count < 100; X += Step, ++Count)
	{
		const float U = static_cast<float>(PdaMap::WorldToMapUV(FVector2D(X, 0.0), TopLeft, BottomRight).X);
		AddLine(FAnchors(U, 0.0f, U, 1.0f), FMargin(0.0f, 0.0f, 1.0f, 0.0f));
	}
	const double MinY = FMath::Min(TopLeft.Y, BottomRight.Y), MaxY = FMath::Max(TopLeft.Y, BottomRight.Y);
	Count = 0;
	for (double Y = FMath::CeilToDouble(MinY / Step) * Step; Y < MaxY && Count < 100; Y += Step, ++Count)
	{
		const float V = static_cast<float>(PdaMap::WorldToMapUV(FVector2D(0.0, Y), TopLeft, BottomRight).Y);
		AddLine(FAnchors(0.0f, V, 1.0f, V), FMargin(0.0f, 0.0f, 0.0f, 1.0f));
	}
}

void UPdaScreenWidget::AddMapDot(const FVector& WorldLocation, float Size, const FLinearColor& Color, const FText& Label)
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FVector2D UV = PdaMap::WorldToMapUV(FVector2D(WorldLocation.X, WorldLocation.Y),
		Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	const FAnchors Anchors(static_cast<float>(UV.X), static_cast<float>(UV.Y));

	UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dot->SetBrush(CircleBrush(Color, FLinearColor(0.0f, 0.0f, 0.0f, 0.8f), 1.5f));
	Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* DotSlot = MapMarkers->AddChildToCanvas(Dot))
	{
		DotSlot->SetAnchors(Anchors);
		DotSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		DotSlot->SetPosition(FVector2D::ZeroVector);
		DotSlot->SetSize(FVector2D(Size, Size));
	}

	if (!Label.IsEmpty())
	{
		// Небольшая надпись НАД местом (Ринат 10.10.2026: «над логовом надпись небольшая появляется»).
		UTextBlock* Text = NewText(WidgetTree, NAME_None, Label, Color, 13, TEXT("Bold"));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* TextSlot = MapMarkers->AddChildToCanvas(Text))
		{
			TextSlot->SetAnchors(Anchors);
			TextSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			TextSlot->SetPosition(FVector2D(0.0f, -Size * 0.5f - 2.0f));
			TextSlot->SetAutoSize(true);
		}
		ShownPlaceLabels.Add(Label.ToString());
	}
	++PlaceMapMarkerCount;
}

const UMapExplorationComponent* UPdaScreenWidget::GetExploration() const
{
	return Player.IsValid() ? Player->GetMapExploration() : nullptr;
}

bool UPdaScreenWidget::IsPlaceRevealed(const FVector& WorldLocation) const
{
	const UMapExplorationComponent* Exploration = GetExploration();
	return !Exploration || Exploration->IsWorldLocationRevealed(FVector2D(WorldLocation.X, WorldLocation.Y));
}

void UPdaScreenWidget::UpdateFogMask()
{
	const UMapExplorationComponent* Exploration = GetExploration();
	if (!Exploration || !Exploration->GetFog().IsValid())
	{
		// Памяти о карте нет — карта открыта целиком, как раньше.
		FogAlpha.Reset();
		LastFogRevision = INDEX_NONE;
		if (MapFogImage)
		{
			MapFogImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	if (Exploration->GetRevision() == LastFogRevision && FogAlpha.Num() > 0)
	{
		return; // открытое не менялось — маска та же (каждый кадр её не считаем)
	}
	LastFogRevision = Exploration->GetRevision();
	++FogRebuildCount;

	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FPdaMapFog& Fog = Exploration->GetFog();
	Fog.BuildAlpha(FogAlpha, Settings->MapFogSoftness);
	if (!MapFogImage)
	{
		return;
	}
	if (!FApp::CanEverRender())
	{
		MapFogImage->SetVisibility(ESlateVisibility::Collapsed); // рисовать нечем (служебный запуск)
		return;
	}

	// Картинка-маска: белая точка на клетку, в прозрачности — чернота. Цвет даёт сам элемент.
	const int32 Width = Fog.GetWidth();
	const int32 Height = Fog.GetHeight();
	const int32 PixelCount = Width * Height;
	const bool bNewTexture = !FogTexture || FogTexture->GetSizeX() != Width || FogTexture->GetSizeY() != Height;
	if (bNewTexture)
	{
		FogTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!FogTexture || !FogTexture->GetPlatformData() || FogTexture->GetPlatformData()->Mips.Num() == 0)
		{
			FogTexture = nullptr;
			MapFogImage->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		FogTexture->SRGB = false;
		FogTexture->Filter = TF_Bilinear;      // сглаживание при растяжении — мягкий край
		FogTexture->AddressX = TA_Clamp;
		FogTexture->AddressY = TA_Clamp;
		FogTexture->NeverStream = true;

		FTexture2DMipMap& Mip = FogTexture->GetPlatformData()->Mips[0];
		uint8* Pixels = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Index = 0; Index < PixelCount; ++Index)
		{
			Pixels[Index * 4 + 0] = 255;
			Pixels[Index * 4 + 1] = 255;
			Pixels[Index * 4 + 2] = 255;
			Pixels[Index * 4 + 3] = FogAlpha[Index];
		}
		Mip.BulkData.Unlock();
		FogTexture->UpdateResource();
		MapFogImage->SetBrushFromTexture(FogTexture);
	}
	else
	{
		// Картинка уже есть — досылаем новые точки, не пересоздавая её.
		uint8* Pixels = static_cast<uint8*>(FMemory::Malloc(PixelCount * 4));
		for (int32 Index = 0; Index < PixelCount; ++Index)
		{
			Pixels[Index * 4 + 0] = 255;
			Pixels[Index * 4 + 1] = 255;
			Pixels[Index * 4 + 2] = 255;
			Pixels[Index * 4 + 3] = FogAlpha[Index];
		}
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
		FogTexture->UpdateTextureRegions(0, 1, Region, Width * 4, 4, Pixels,
			[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(SrcData);
				delete Regions;
			});
	}
	MapFogImage->SetColorAndOpacity(Settings->MapFogColor);
	MapFogImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

FVector2D UPdaScreenWidget::ClampArrowCenter(const FVector2D& Center, const FVector2D& AreaMin, const FVector2D& AreaMax, double Inset)
{
	auto ClampAxis = [Inset](double Value, double Min, double Max)
	{
		return (Max - Min <= Inset * 2.0) ? (Min + Max) * 0.5 : FMath::Clamp(Value, Min + Inset, Max - Inset);
	};
	return FVector2D(ClampAxis(Center.X, AreaMin.X, AreaMax.X), ClampAxis(Center.Y, AreaMin.Y, AreaMax.Y));
}

void UPdaScreenWidget::RebuildMap()
{
	QuestMapMarkerCount = 0;
	PlaceMapMarkerCount = 0;
	ShownPlaceLabels.Reset();
	if (!MapMarkers)
	{
		return;
	}
	MapMarkers->ClearChildren();

	// Чернота неизведанного (пересчитывается, только если открытое изменилось).
	UpdateFogMask();

	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();

	// Картинка местности под отметками; не задана — тёмное поле с сеткой.
	LoadedMapTexture = Settings->MapTexture.IsNull() ? nullptr : Settings->MapTexture.LoadSynchronous();
	if (MapImage)
	{
		if (LoadedMapTexture)
		{
			MapImage->SetBrushFromTexture(LoadedMapTexture);
		}
		MapImage->SetVisibility(LoadedMapTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!LoadedMapTexture)
	{
		AddGridLines();
	}

	UWorld* World = Player.IsValid() ? Player->GetWorld() : GetWorld();
	if (!World)
	{
		return;
	}

	// --- Известные места: видны только там, где герой уже побывал ---
	for (TActorIterator<ACampfire> It(World); It; ++It)
	{
		if (IsPlaceRevealed(It->GetActorLocation()))
		{
			AddMapDot(It->GetActorLocation(), 9.0f, CampfireColor, FText::GetEmpty());
		}
	}

	// Места с подписями. Сначала — объекты уровня (их положение точное), потом список из
	// настроек: точка списка рядом с объектом — то же место (подпись и вид берутся из списка,
	// положение остаётся с объекта), точка без объекта рядом — место само по себе.
	struct FPlace
	{
		FVector Location;
		FText Name;
		FLinearColor Color;
		bool bFromSettings = false;
	};
	TArray<FPlace> Places;
	for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
	{
		if (!It->GetBaseDisplayName().IsEmpty())
		{
			Places.Add({ It->GetActorLocation(), It->GetBaseDisplayName(), EnemyPlaceColor });
		}
	}
	for (TActorIterator<AVillageZone> It(World); It; ++It)
	{
		Places.Add({ It->GetActorLocation(), VillageLabel, FriendlyPlaceColor });
	}
	const int32 ActorPlaceCount = Places.Num();
	const double MergeRadiusSq = FMath::Square(static_cast<double>(Settings->MapPlaceMergeRadius));
	for (const FPdaMapPlace& Listed : Settings->MapPlaces)
	{
		const FLinearColor Color = Listed.Kind == EPdaMapPlaceKind::Enemy ? EnemyPlaceColor
			: Listed.Kind == EPdaMapPlaceKind::Friendly ? FriendlyPlaceColor : NeutralPlaceColor;
		int32 Nearest = INDEX_NONE;
		double NearestSq = MergeRadiusSq;
		for (int32 Index = 0; Index < ActorPlaceCount; ++Index)
		{
			const double DistSq = FVector2D::DistSquared(Listed.WorldLocation,
				FVector2D(Places[Index].Location.X, Places[Index].Location.Y));
			if (!Places[Index].bFromSettings && DistSq <= NearestSq)
			{
				Nearest = Index;
				NearestSq = DistSq;
			}
		}
		if (Nearest != INDEX_NONE)
		{
			Places[Nearest].bFromSettings = true; // объект занят этой строкой списка
			Places[Nearest].Color = Color;
			if (!Listed.Name.IsEmpty())
			{
				Places[Nearest].Name = Listed.Name;
			}
		}
		else
		{
			Places.Add({ FVector(Listed.WorldLocation.X, Listed.WorldLocation.Y, 0.0), Listed.Name, Color, true });
		}
	}
	for (const FPlace& Place : Places)
	{
		if (IsPlaceRevealed(Place.Location))
		{
			AddMapDot(Place.Location, 12.0f, Place.Color, Place.Name);
		}
	}

	// --- Цели идущих заданий: отслеживаемое крупно и с подписью, остальные мельче и тусклее ---
	const UQuestComponent* Quests = Player.IsValid() ? Player->GetQuests() : nullptr;
	if (!Quests)
	{
		return;
	}
	const FQuest* Tracked = Quests->GetTrackedQuest();
	for (const FQuest& Quest : Quests->GetQuests())
	{
		if (!UQuestComponent::IsInActivePhase(Quest))
		{
			continue;
		}
		// Тот же источник, что у метки-указателя в мире.
		const AActor* Target = QuestTargets::FindTargetActor(World, Quest);
		if (!Target)
		{
			continue;
		}
		const bool bTracked = (&Quest == Tracked);
		// На неизведанном видна только цель отслеживаемого задания (и то по галочке настроек).
		if (!IsPlaceRevealed(Target->GetActorLocation())
			&& !(bTracked && Settings->bShowTrackedQuestOnUnexplored))
		{
			continue;
		}
		FLinearColor Color = Settings->GetQuestKindColor(Quest.Kind);
		if (!bTracked)
		{
			Color.A *= OtherQuestMarkerOpacity;
		}
		const float Size = bTracked ? 34.0f : 24.0f;
		const FVector Location = Target->GetActorLocation();
		const FVector2D UV = PdaMap::WorldToMapUV(FVector2D(Location.X, Location.Y),
			Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
		const FAnchors Anchors(static_cast<float>(UV.X), static_cast<float>(UV.Y));

		UPdaQuestButton* Marker = WidgetTree->ConstructWidget<UPdaQuestButton>(UPdaQuestButton::StaticClass());
		Marker->QuestId = Quest.QuestId;
		Marker->BindPick();
		Marker->OnPicked.BindWeakLambda(this, [this](FName Id) { TrackQuest(Id); });
		Marker->SetStyle(CircleButtonStyle(Color));
		if (UCanvasPanelSlot* MarkerSlot = MapMarkers->AddChildToCanvas(Marker))
		{
			MarkerSlot->SetAnchors(Anchors);
			MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			MarkerSlot->SetPosition(FVector2D::ZeroVector);
			MarkerSlot->SetSize(FVector2D(Size, Size));
			MarkerSlot->SetZOrder(bTracked ? 2 : 1); // поверх отметок мест
		}

		if (bTracked)
		{
			UTextBlock* Title = NewText(WidgetTree, NAME_None, Quest.Title, Color, 14, TEXT("Bold"));
			Title->SetShadowOffset(FVector2D(1.0f, 1.0f));
			Title->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
			Title->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* TitleSlot = MapMarkers->AddChildToCanvas(Title))
			{
				TitleSlot->SetAnchors(Anchors);
				TitleSlot->SetAlignment(FVector2D(0.5f, 1.0f));
				TitleSlot->SetPosition(FVector2D(0.0f, -Size * 0.5f - 2.0f));
				TitleSlot->SetAutoSize(true);
				TitleSlot->SetZOrder(2);
			}
		}
		++QuestMapMarkerCount;
	}
}

void UPdaScreenWidget::FitMapArea()
{
	UCanvasPanelSlot* AreaSlot = MapArea ? Cast<UCanvasPanelSlot>(MapArea->Slot) : nullptr;
	if (!AreaSlot || !MapPanel)
	{
		return;
	}
	const FVector2D PanelSize = MapPanel->GetCachedGeometry().GetLocalSize();
	if (PanelSize.X < 1.0 || PanelSize.Y < 1.0 || PanelSize.Equals(LastFittedPanelSize, 0.5))
	{
		return; // раскладка ещё не посчитана либо размер не менялся
	}
	LastFittedPanelSize = PanelSize;

	// Снизу оставляем место строке-подсказке.
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FVector2D Available(PanelSize.X, FMath::Max(1.0, PanelSize.Y - 26.0));
	const FVector2D Fitted = PdaMap::FitMapSize(Available, Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	AreaSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
	AreaSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	AreaSlot->SetPosition(FVector2D::ZeroVector);
	AreaSlot->SetSize(Fitted);
}

void UPdaScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Журнал изменился, пока окно открыто (или игрок что-то нажал в окне) — перерисовываем.
	if (bRefreshPending || ComputeJournalStamp() != LastJournalStamp)
	{
		RefreshNow();
	}
	if (bMapTab)
	{
		FitMapArea();
		// Герой открыл новое, пока карта на экране, — перерисовываем черноту и подписи мест.
		const UMapExplorationComponent* Exploration = GetExploration();
		if (Exploration && Exploration->GetRevision() != LastFogRevision)
		{
			RebuildMap();
		}
	}
}

int32 UPdaScreenWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	// Стрелка героя — поверх всего на карте: остриё смотрит туда же, куда герой.
	if (!bMapTab || !MapArea || !Player.IsValid())
	{
		return MaxLayer;
	}
	const FGeometry& AreaGeometry = MapArea->GetCachedGeometry();
	const FVector2D AreaLocalSize = AreaGeometry.GetLocalSize();
	const FVector2D AreaMin = AllottedGeometry.AbsoluteToLocal(AreaGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D AreaMax = AllottedGeometry.AbsoluteToLocal(AreaGeometry.LocalToAbsolute(AreaLocalSize));
	const FVector2D AreaSize = AreaMax - AreaMin;
	if (AreaSize.X < 4.0 || AreaSize.Y < 4.0)
	{
		return MaxLayer;
	}

	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FVector Location = Player->GetActorLocation();
	const FVector2D UV = PdaMap::WorldToMapUV(FVector2D(Location.X, Location.Y),
		Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	// Герой у края карты или за ним: стрелка прижимается к краю поля целиком, а не теряется.
	const FVector2D Center = ClampArrowCenter(AreaMin + UV * AreaSize, AreaMin, AreaMax, 14.0);

	const FVector Forward = Player->GetActorForwardVector();
	FVector2D Dir = PdaMap::WorldDirToMapDir(FVector2D(Forward.X, Forward.Y),
		Settings->MapWorldTopLeft, Settings->MapWorldBottomRight) * AreaSize;
	if (!Dir.Normalize())
	{
		Dir = FVector2D(0.0, -1.0);
	}
	const FVector2D Side(-Dir.Y, Dir.X);

	// Наконечник стрелы: остриё, два задних угла и выемка между ними.
	TArray<FVector2D> Points;
	Points.Add(Center + Dir * 14.0);
	Points.Add(Center - Dir * 9.0 + Side * 9.0);
	Points.Add(Center - Dir * 3.0);
	Points.Add(Center - Dir * 9.0 - Side * 9.0);
	Points.Add(Center + Dir * 14.0);

	const int32 ArrowLayer = MaxLayer + 1;
	FSlateDrawElement::MakeLines(OutDrawElements, ArrowLayer, AllottedGeometry.ToPaintGeometry(), Points,
		ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.9f), /*bAntialias=*/true, 6.0f);
	FSlateDrawElement::MakeLines(OutDrawElements, ArrowLayer + 1, AllottedGeometry.ToPaintGeometry(), Points,
		ESlateDrawEffect::None, PlayerArrowColor, /*bAntialias=*/true, 3.0f);
	return ArrowLayer + 1;
}

// ===========================================================================
// Кнопка «КПК» на экране игры (телефон)
// ===========================================================================

void UPdaTouchButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PdaButtonRoot"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	// Вид — как у экранных кнопок: полупрозрачный белый круг с подписью.
	PdaButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PdaButton"));
	FButtonStyle Style;
	Style.Normal = CircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.30f));
	Style.Hovered = CircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.40f));
	Style.Pressed = CircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.55f));
	Style.Disabled = Style.Normal;
	Style.NormalPadding = FMargin(0.0f);
	Style.PressedPadding = FMargin(0.0f);
	PdaButton->SetStyle(Style);
	PdaButton->OnClicked.AddUniqueDynamic(this, &UPdaTouchButtonWidget::HandleClicked);

	PdaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PdaText"));
	PdaText->SetFont(PdaFont(15, TEXT("Bold")));
	PdaText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f)));
	PdaText->SetJustification(ETextJustify::Center);
	PdaButton->SetContent(PdaText);

	Root->AddChildToCanvas(PdaButton);
	Setup(Margin, Radius, Label);
}

void UPdaTouchButtonWidget::Setup(const FVector2D& InMargin, float InRadius, const FText& InLabel)
{
	Margin = InMargin;
	Radius = FMath::Max(15.0f, InRadius);
	Label = InLabel;

	if (PdaText)
	{
		PdaText->SetText(Label);
	}
	if (UCanvasPanelSlot* ButtonSlot = PdaButton ? Cast<UCanvasPanelSlot>(PdaButton->Slot) : nullptr)
	{
		ButtonSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		ButtonSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ButtonSlot->SetPosition(FVector2D(-Margin.X, Margin.Y));
		ButtonSlot->SetSize(FVector2D(Radius * 2.0f, Radius * 2.0f));
	}
}

void UPdaTouchButtonWidget::SetupAt(const FAnchors& InAnchors, const FVector2D& InCenter, float InRadius, const FText& InLabel)
{
	Setup(Margin, InRadius, InLabel);
	if (UCanvasPanelSlot* ButtonSlot = PdaButton ? Cast<UCanvasPanelSlot>(PdaButton->Slot) : nullptr)
	{
		ButtonSlot->SetAnchors(InAnchors);
		ButtonSlot->SetPosition(InCenter);
	}
}

FVector2D UPdaTouchButtonWidget::ComputeCenterBeside(const FVector2D& NeighborCenter, const FVector2D& NeighborSize,
	float NeighborScale, float InRadius, float Gap, bool bToTheLeft)
{
	const double Offset = NeighborSize.X * 0.5 * FMath::Max(1.0f, NeighborScale) + FMath::Max(0.0f, Gap) + InRadius;
	return FVector2D(NeighborCenter.X + (bToTheLeft ? -Offset : Offset), NeighborCenter.Y);
}

void UPdaTouchButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	const bool bShow = PC && PC->GetPawn() && !PC->IsAnyModalUIOpen() && !PC->IsIntroMoveInputLocked();
	SetContentVisible(bShow);
}

void UPdaTouchButtonWidget::HandleClicked()
{
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer()))
	{
		PC->TouchTogglePda();
	}
}
