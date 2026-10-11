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
		const FText& Caption, const FLinearColor& Normal, int32 FontSize = 16)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->SetStyle(FlatButtonStyle(Normal, Normal * 1.25f, Normal * 0.8f));
		Button->bIsVariable = true;
		UTextBlock* Label = NewText(Tree, TextName, Caption, PdaTextColor, FontSize, TEXT("Bold"));
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

// Кисть-картинка для подложки окна. UBorder::SetBrushFromTexture меняет у кисти только картинку
// (Border.cpp:261-263), а вид прежней кисти — скруглённая плашка с жёлтым контуром — остаётся:
// отсюда была жёлтая рамка вокруг смартфона (замечание Рината 11.10.2026).
static FSlateBrush MakePhonePictureBrush(UTexture2D* Texture)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.TintColor = FSlateColor(FLinearColor::White);
	const FIntPoint Size = PdaMap::GetPictureSize(Texture);
	Brush.ImageSize = FVector2D(FMath::Max(1, Size.X), FMath::Max(1, Size.Y));
	return Brush;
}

void UPdaScreenWidget::BuildDefaultTree(UWidgetTree* Tree)
{
	if (!Tree)
	{
		return;
	}
	const UPdaScreenWidget* Defaults = GetDefault<UPdaScreenWidget>();
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Tree->RootWidget = Root;

	// Затемнение на весь экран; Visible — нажатия в мир не проходят.
	UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Dim->SetVisibility(ESlateVisibility::Visible);
	Dim->bIsVariable = true;
	PlaceStretch(Root, Dim, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	// Подложка-смартфон по центру экрана. Размер здесь — для экрана 1920 на 1080 (он же виден
	// в редакторе окон); в игре размер, отступы экрана и картинку ставит ApplyPhoneLayout по
	// настройкам и настоящему размеру экрана. Пока картинки смартфона нет — тёмная плашка с
	// рамкой, как у сумки и магазина. Внутри свой холст: всё содержимое лежит в экране смартфона.
	const FVector2D DesignPhone = PdaMap::ComputePhoneSize(FVector2D(1920.0, 1080.0),
		Settings->PhoneHeightFraction, Settings->PhoneAspectRatio, Settings->PhoneSideMargin);
	UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
	Plate->SetBrush(RoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.97f), 28.0f,
		FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
	// Картинка смартфона задана в настройках — подложка в файле окна сразу рисуется ею,
	// чистой кистью: рамка и скругление запасной плашки на картинку не переходят.
	if (UTexture2D* DesignPhoneTexture = Settings->PhoneTexture.IsNull() ? nullptr : Settings->PhoneTexture.LoadSynchronous())
	{
		Plate->SetBrush(MakePhonePictureBrush(DesignPhoneTexture));
	}
	Plate->SetPadding(PdaMap::ComputePhoneScreenPadding(DesignPhone, Settings->PhoneScreenInsets));
	Plate->bIsVariable = true;
	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
	{
		PlateSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PlateSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PlateSlot->SetPosition(FVector2D::ZeroVector);
		PlateSlot->SetSize(DesignPhone);
	}
	UCanvasPanel* Panel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
	Plate->SetContent(Panel);

	// --- Левый столбец: заголовок, две вкладки, подсказка карты, «Закрыть». Столбец узкий,
	// чтобы карта справа получила всю высоту экрана смартфона и почти всю его ширину. ---
	const float SideW = 220.0f;      // ширина столбца
	const float ContentX = 236.0f;   // откуда начинается вкладка (столбец + просвет 16)
	const float SideButtonH = 96.0f; // высота кнопок столбца — под палец на телефоне (Ринат 11.10.2026)
	const float SideButtonGap = 12.0f;

	PlaceAuto(Panel, NewText(Tree, TEXT("TitleText"), Defaults->TitleLabel, PdaTextColor, 24, TEXT("Bold")),
		FVector2D(4.0f, 4.0f));

	const FLinearColor TabColor(0.18f, 0.20f, 0.24f, 1.0f);
	PlaceAt(Panel, NewCaptionButton(Tree, TEXT("QuestsTabButton"), TEXT("QuestsTabText"),
		Defaults->QuestsTabLabel, TabColor, 20), FVector2D(0.0f, 56.0f), FVector2D(SideW, SideButtonH));
	PlaceAt(Panel, NewCaptionButton(Tree, TEXT("MapTabButton"), TEXT("MapTabText"),
		Defaults->MapTabLabel, TabColor, 20), FVector2D(0.0f, 56.0f + SideButtonH + SideButtonGap), FVector2D(SideW, SideButtonH));

	// Подсказка вкладки карты — в столбце под вкладками (на самой карте она отнимала бы место).
	UTextBlock* Hint = NewText(Tree, TEXT("MapHintText"), Defaults->MapHintLabel, PdaDimTextColor, 14, TEXT("Bold"));
	Hint->SetAutoWrapText(true);
	PlaceAt(Panel, Hint, FVector2D(4.0f, 56.0f + 2.0f * (SideButtonH + SideButtonGap) + 4.0f), FVector2D(SideW - 8.0f, 220.0f));

	UButton* Close = NewCaptionButton(Tree, TEXT("CloseButton"), TEXT("CloseText"),
		Defaults->CloseLabel, FLinearColor(0.3f, 0.3f, 0.34f, 1.0f), 20);
	if (UCanvasPanelSlot* CloseSlot = Panel->AddChildToCanvas(Close))
	{
		CloseSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		CloseSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		CloseSlot->SetPosition(FVector2D::ZeroVector);
		CloseSlot->SetSize(FVector2D(SideW, SideButtonH));
	}

	// --- Вкладка «Задания»: слева список, справа сведения о выбранном ---
	UCanvasPanel* Quests = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestsPanel"));
	Quests->bIsVariable = true;
	PlaceStretch(Panel, Quests, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(ContentX, 0.0f, 0.0f, 0.0f));

	UScrollBox* List = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("QuestList"));
	List->bIsVariable = true;
	// Растянут по высоте панели, ширина 440 (при совпавших якорях правый отступ — это ширина).
	PlaceStretch(Quests, List, FAnchors(0.0f, 0.0f, 0.0f, 1.0f), FMargin(0.0f, 0.0f, 440.0f, 0.0f));

	PlaceAuto(Quests, NewText(Tree, TEXT("EmptyText"), Defaults->EmptyLabel, PdaDimTextColor, 16, TEXT("Bold")),
		FVector2D(8.0f, 8.0f));

	// Сведения о задании тянутся от списка до правого края экрана смартфона (якоря по ширине
	// растянуты: левый отступ — от списка, правый — ноль), поэтому занимают всё место при
	// любом размере окна.
	const float DetailX = 464.0f;
	auto PlaceDetail = [Quests, DetailX](UWidget* Widget, float Y, float Height)
	{
		if (UCanvasPanelSlot* DetailSlot = Quests->AddChildToCanvas(Widget))
		{
			DetailSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
			DetailSlot->SetOffsets(FMargin(DetailX, Y, 0.0f, Height));
		}
	};
	PlaceAuto(Quests, NewText(Tree, TEXT("DetailTitleText"), FText::GetEmpty(), PdaTextColor, 22, TEXT("Bold")),
		FVector2D(DetailX, 0.0f));
	PlaceAuto(Quests, NewText(Tree, TEXT("DetailGiverText"), FText::GetEmpty(), PdaDimTextColor, 16, TEXT("Bold")),
		FVector2D(DetailX, 40.0f));

	UTextBlock* Description = NewText(Tree, TEXT("DetailDescriptionText"), FText::GetEmpty(), PdaTextColor, 18, TEXT("Bold"));
	Description->SetAutoWrapText(true);
	PlaceDetail(Description, 100.0f, 236.0f);

	UTextBlock* Objectives = NewText(Tree, TEXT("DetailObjectivesText"), FText::GetEmpty(), PdaTextColor, 18, TEXT("Bold"));
	Objectives->SetAutoWrapText(true);
	PlaceDetail(Objectives, 346.0f, 130.0f);

	UTextBlock* Reward = NewText(Tree, TEXT("DetailRewardText"), FText::GetEmpty(), PdaGoldColor, 18, TEXT("Bold"));
	Reward->SetAutoWrapText(true);
	PlaceDetail(Reward, 486.0f, 94.0f);

	UButton* Track = NewCaptionButton(Tree, TEXT("TrackButton"), TEXT("TrackText"),
		Defaults->TrackLabel, FLinearColor(0.16f, 0.36f, 0.16f, 1.0f), 20);
	if (UCanvasPanelSlot* TrackSlot = Quests->AddChildToCanvas(Track))
	{
		TrackSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		TrackSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		TrackSlot->SetPosition(FVector2D(DetailX, 0.0f));
		TrackSlot->SetSize(FVector2D(320.0f, SideButtonH));
	}

	// --- Вкладка «Карта» (занимает то же место, что «Задания»; видимость вкладок ведёт код).
	// MapPanel — поле карты на всю высоту экрана смартфона; всё, что выходит за поле при
	// приближении, обрезается. ---
	UCanvasPanel* Map = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapPanel"));
	Map->SetClipping(EWidgetClipping::ClipToBounds);
	Map->bIsVariable = true;
	PlaceStretch(Panel, Map, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(ContentX, 0.0f, 0.0f, 0.0f));

	// Тёмный фон поля: виден по бокам карты, пока она показана целиком.
	UBorder* Background = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapBackground"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.04f, 1.0f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	Background->bIsVariable = true;
	PlaceStretch(Map, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	// Сама карта. В игре код ставит её в поле по приближению и сдвигу (ApplyMapView).
	UCanvasPanel* Area = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapArea"));
	Area->bIsVariable = true;
	PlaceStretch(Map, Area, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

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

	// Кнопки поля карты — столбиком в правом-нижнем углу, поверх карты: «+», «−», «найти героя».
	// 90 на 90 — нижняя граница удобного под палец размера из руководства по раскладке.
	const float MapButtonSize = 96.0f;
	const float MapButtonGap = 12.0f;
	const float MapButtonMargin = 14.0f;
	const FLinearColor MapButtonColor(0.10f, 0.12f, 0.15f, 0.88f);
	auto PlaceMapButton = [&](const TCHAR* ButtonName, const TCHAR* TextName, const FText& Caption, int32 FontSize, int32 FromBottom)
	{
		UButton* Button = NewCaptionButton(Tree, FName(ButtonName), FName(TextName), Caption, MapButtonColor, FontSize);
		if (UCanvasPanelSlot* ButtonSlot = Map->AddChildToCanvas(Button))
		{
			ButtonSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
			ButtonSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			ButtonSlot->SetPosition(FVector2D(-MapButtonMargin,
				-MapButtonMargin - FromBottom * (MapButtonSize + MapButtonGap)));
			ButtonSlot->SetSize(FVector2D(MapButtonSize, MapButtonSize));
			ButtonSlot->SetZOrder(5); // поверх карты и отметок
		}
	};
	// «Моя позиция» — в две строки кеглем 14: слово «позиция» уже кнопки с запасом.
	PlaceMapButton(TEXT("MapFindHeroButton"), TEXT("MapFindHeroText"), Defaults->FindHeroLabel, 14, 0);
	PlaceMapButton(TEXT("MapZoomOutButton"), TEXT("MapZoomOutText"), Defaults->ZoomOutLabel, 34, 1);
	PlaceMapButton(TEXT("MapZoomInButton"), TEXT("MapZoomInText"), Defaults->ZoomInLabel, 34, 2);

	// Шаги, общие с дополнением готового файла окна: меню вкладок, поле карты под саму карту,
	// круг района поиска.
	MoveMenuIntoSideMenu(Tree);
	FitMapPanelToMap(Tree);
	AddSearchAreaMark(Tree);
}

float UPdaScreenWidget::ComputeDesignMapPanelWidth()
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	double Aspect = Settings->PhoneAspectRatio;
	const FIntPoint Picture = PdaMap::GetPictureSize(Settings->PhoneTexture.IsNull() ? nullptr : Settings->PhoneTexture.LoadSynchronous());
	if (Picture.X > 0 && Picture.Y > 0)
	{
		Aspect = static_cast<double>(Picture.X) / Picture.Y;
	}
	const FVector2D Phone = PdaMap::ComputePhoneSize(FVector2D(1920.0, 1080.0),
		Settings->PhoneHeightFraction, Aspect, Settings->PhoneSideMargin);
	const FMargin Pad = PdaMap::ComputePhoneScreenPadding(Phone, Settings->PhoneScreenInsets);
	const double InnerHeight = FMath::Max(1.0, Phone.Y - Pad.Top - Pad.Bottom);
	// Места по ширине «сколько угодно» — карту ограничивает только высота.
	return FMath::RoundToFloat(static_cast<float>(PdaMap::FitMapSize(FVector2D(1.0e7, InnerHeight),
		Settings->MapWorldTopLeft, Settings->MapWorldBottomRight).X));
}

bool UPdaScreenWidget::MoveMenuIntoSideMenu(UWidgetTree* Tree)
{
	if (!Tree || Tree->FindWidget(TEXT("SideMenu")))
	{
		return false; // меню уже есть
	}
	UWidget* Quests = Tree->FindWidget(TEXT("QuestsPanel"));
	const UCanvasPanelSlot* QuestsSlot = Quests ? Cast<UCanvasPanelSlot>(Quests->Slot) : nullptr;
	UCanvasPanel* Panel = Quests ? Cast<UCanvasPanel>(Quests->GetParent()) : nullptr;
	if (!QuestsSlot || !Panel)
	{
		return false;
	}
	// В файле окна меню стоит как при вкладке «Задания»: от левого края до вкладки за вычетом
	// просвета. В игре ширину под открытую вкладку ставит UpdateSideMenuWidth.
	const float MenuWidth = FMath::Max(1.0f, QuestsSlot->GetOffsets().Left - GetDefault<UPdaScreenWidget>()->SideMenuGap);

	UCanvasPanel* Menu = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SideMenu"));
	Menu->bIsVariable = true;
	if (UCanvasPanelSlot* MenuSlot = Panel->AddChildToCanvas(Menu))
	{
		MenuSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 1.0f));
		MenuSlot->SetOffsets(FMargin(0.0f, 0.0f, MenuWidth, 0.0f)); // при совпавших якорях правый отступ — ширина
	}

	for (const TCHAR* WidgetName : { TEXT("TitleText"), TEXT("QuestsTabButton"), TEXT("MapTabButton"), TEXT("MapHintText"), TEXT("CloseButton") })
	{
		UWidget* Widget = Tree->FindWidget(WidgetName);
		const UCanvasPanelSlot* OldSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		if (!OldSlot || Widget->GetParent() != Panel)
		{
			continue;
		}
		FAnchorData Layout = OldSlot->GetLayout();
		const bool bAutoSize = OldSlot->GetAutoSize();
		const int32 ZOrder = OldSlot->GetZOrder();
		// Кнопки и подсказка тянутся на всю ширину меню с прежними отступами от его краёв;
		// элемент по размеру текста (заголовок) остаётся, где стоял.
		if (!bAutoSize && Layout.Anchors.Minimum.X == 0.0f && Layout.Anchors.Maximum.X == 0.0f)
		{
			const float Left = Layout.Offsets.Left - Layout.Offsets.Right * Layout.Alignment.X;
			const float Right = FMath::Max(0.0f, MenuWidth - (Left + Layout.Offsets.Right));
			Layout.Anchors.Maximum.X = 1.0f;
			Layout.Offsets.Left = Left;
			Layout.Offsets.Right = Right;
		}
		Panel->RemoveChild(Widget);
		if (UCanvasPanelSlot* NewSlot = Menu->AddChildToCanvas(Widget))
		{
			NewSlot->SetLayout(Layout);
			NewSlot->SetAutoSize(bAutoSize);
			NewSlot->SetZOrder(ZOrder);
		}
	}
	return true;
}

bool UPdaScreenWidget::FitMapPanelToMap(UWidgetTree* Tree)
{
	UWidget* Map = Tree ? Tree->FindWidget(TEXT("MapPanel")) : nullptr;
	UCanvasPanelSlot* MapSlot = Map ? Cast<UCanvasPanelSlot>(Map->Slot) : nullptr;
	if (!MapSlot || MapSlot->GetAnchors().Minimum.X == MapSlot->GetAnchors().Maximum.X)
	{
		return false; // поля нет либо оно уже стоит своей шириной
	}
	const FAnchors OldAnchors = MapSlot->GetAnchors();
	const FMargin OldOffsets = MapSlot->GetOffsets();
	// К правому краю экрана смартфона; по высоте растянуто, как было. При одной точке привязки
	// по ширине левый отступ — место, правый — ширина.
	MapSlot->SetAnchors(FAnchors(1.0f, OldAnchors.Minimum.Y, 1.0f, OldAnchors.Maximum.Y));
	MapSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	MapSlot->SetOffsets(FMargin(-OldOffsets.Right, OldOffsets.Top, ComputeDesignMapPanelWidth(), OldOffsets.Bottom));
	return true;
}

bool UPdaScreenWidget::AddSearchAreaMark(UWidgetTree* Tree)
{
	if (!Tree || Tree->FindWidget(TEXT("MapAreaMark")))
	{
		return false; // уже добавлен
	}
	UWidget* Markers = Tree->FindWidget(TEXT("MapMarkers"));
	UCanvasPanel* Area = Markers ? Cast<UCanvasPanel>(Markers->GetParent()) : nullptr;
	if (!Area)
	{
		return false;
	}
	const UCanvasPanelSlot* MarkersSlot = Cast<UCanvasPanelSlot>(Markers->Slot);
	const int32 ZOrder = MarkersSlot ? MarkersSlot->GetZOrder() : 0;
	int32 InsertAt = Area->GetChildIndex(Markers); // под отметками: нажатия по отметкам заданий остаются

	// Зелёный полупрозрачный круг с контуром. В файле стоит в середине карты; в игре код ставит
	// его на место района и растягивает вместе с приближением карты.
	const float MarkSize = 150.0f;
	UBorder* Mark = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapAreaMark"));
	Mark->SetBrush(CircleBrush(FLinearColor(0.2f, 0.85f, 0.3f, 0.22f), FLinearColor(0.3f, 1.0f, 0.4f, 0.95f), 3.0f));
	Mark->SetVisibility(ESlateVisibility::Collapsed);
	Mark->bIsVariable = true;
	if (UCanvasPanelSlot* MarkSlot = Cast<UCanvasPanelSlot>(Area->InsertChildAt(InsertAt++, Mark)))
	{
		MarkSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		MarkSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MarkSlot->SetPosition(FVector2D::ZeroVector);
		MarkSlot->SetSize(FVector2D(MarkSize, MarkSize));
		MarkSlot->SetZOrder(ZOrder);
	}

	// Подпись под кругом: в своём блоке с переносом строк.
	UTextBlock* Caption = NewText(Tree, TEXT("MapAreaMarkText"), GetDefault<UPdaScreenWidget>()->SearchAreaLabel,
		FLinearColor(0.6f, 1.0f, 0.65f, 1.0f), 14, TEXT("Bold"));
	Caption->SetAutoWrapText(true);
	Caption->SetJustification(ETextJustify::Center);
	Caption->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Caption->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	Caption->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* CaptionSlot = Cast<UCanvasPanelSlot>(Area->InsertChildAt(InsertAt, Caption)))
	{
		CaptionSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CaptionSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		CaptionSlot->SetPosition(FVector2D(0.0f, MarkSize * 0.5f + 6.0f));
		CaptionSlot->SetSize(FVector2D(300.0f, 130.0f));
		CaptionSlot->SetZOrder(ZOrder);
	}
	return true;
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
	if (MapZoomInButton)
	{
		MapZoomInButton->OnClicked.AddUniqueDynamic(this, &UPdaScreenWidget::HandleZoomInClicked);
	}
	if (MapZoomOutButton)
	{
		MapZoomOutButton->OnClicked.AddUniqueDynamic(this, &UPdaScreenWidget::HandleZoomOutClicked);
	}
	if (MapFindHeroButton)
	{
		MapFindHeroButton->OnClicked.AddUniqueDynamic(this, &UPdaScreenWidget::HandleFindHeroClicked);
	}
	// Приближенная карта больше своего поля — лишнее обрезается по его краям.
	if (MapPanel)
	{
		MapPanel->SetClipping(EWidgetClipping::ClipToBounds);
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
	PDA_BIND(PanelPlate, UBorder);
	PDA_BIND(MapArea, UCanvasPanel);
	PDA_BIND(MapZoomInButton, UButton);
	PDA_BIND(MapZoomOutButton, UButton);
	PDA_BIND(MapFindHeroButton, UButton);
	PDA_BIND(MapBackground, UBorder);
	PDA_BIND(MapImage, UImage);
	PDA_BIND(MapFogImage, UImage);
	PDA_BIND(MapMarkers, UCanvasPanel);
	PDA_BIND(MapHintText, UTextBlock);
	PDA_BIND(SideMenu, UCanvasPanel);
	PDA_BIND(MapAreaMark, UBorder);
	PDA_BIND(MapAreaMarkText, UTextBlock);
#undef PDA_BIND
}

void UPdaScreenWidget::InitPda(APlayerCharacter* InPlayer)
{
	EnsureSetup();
	Player = InPlayer;
	LastScreenSize = FVector2D::ZeroVector; // смартфон подгоняется под экран заново

	// Приближение остаётся, каким игрок его оставил в этом запуске, а карта при каждом открытии
	// встаёт героем в середину поля (при карте целиком это ничего не меняет). Размер поля ещё
	// неизвестен (окно открывается впервые) — поставим, когда раскладка посчитается.
	bCenterOnHeroPending = true;
	if (MapViewportSize.X >= 1.0 && MapViewportSize.Y >= 1.0)
	{
		bCenterOnHeroPending = false;
		CenterMapOnHero();
	}
	bMouseDragging = false;
	MapTouches.Reset();

	// Смартфон открыт — кнопка на экране своё отмигала (вкладка «Карта» мигает дальше, пока
	// игрок на неё не перейдёт).
	if (UMapExplorationComponent* Exploration = GetExplorationMutable())
	{
		Exploration->NotePhoneOpened();
	}

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
	UpdateSearchAreaMark();
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
	// Подсказка карты стоит в левом столбце — видна только с картой.
	if (MapHintText)
	{
		MapHintText->SetVisibility(bMapTab ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
	UpdateSideMenuWidth();
	// Игрок на вкладке «Карта» — район поиска он увидел: мигания гаснут.
	if (bMapTab)
	{
		if (UMapExplorationComponent* Exploration = GetExplorationMutable())
		{
			Exploration->NoteMapTabShown();
		}
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
		UContraryPdaSettings::Get()->GetQuestKindColor(Quest.Kind), 17, TEXT("Bold"));
	Title->SetAutoWrapText(true); // длинное название переносится, а не обрезается
	Row->SetContent(Title);
	if (UButtonSlot* TitleSlot = Cast<UButtonSlot>(Title->Slot))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Left);
		TitleSlot->SetPadding(QuestRowPadding);
	}

	QuestList->AddChild(Row);
	if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
	{
		RowSlot->SetPadding(FMargin(0.0f, QuestRowGap * 0.5f, 8.0f, QuestRowGap * 0.5f));
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
			ToggleSlot->SetPadding(FMargin(6.0f, QuestRowPadding.Top, 6.0f, QuestRowPadding.Bottom));
		}
		QuestList->AddChild(Toggle);
		if (UScrollBoxSlot* ToggleRowSlot = Cast<UScrollBoxSlot>(Toggle->Slot))
		{
			ToggleRowSlot->SetPadding(FMargin(0.0f, QuestRowGap, 8.0f, QuestRowGap * 0.5f));
		}

		if (bDoneExpanded)
		{
			for (const FName& Id : View.Done)
			{
				if (const FQuest* Quest = Quests->FindQuest(Id))
				{
					UTextBlock* DoneTitle = NewText(WidgetTree, NAME_None, Quest->Title, PdaDimTextColor, 15, TEXT("Bold"));
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

	const int32 Money = FMath::RoundToInt32(Quest.GetPayoutMoney()); // за вычетом долга торговцу
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

// ===========================================================================
// Вид карты: приближение и сдвиг
// ===========================================================================

FVector2D UPdaScreenWidget::GetMapBaseSize() const
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	return PdaMap::FitMapSize(MapViewportSize, Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
}

FVector2D UPdaScreenWidget::WorldToMapViewport(const FVector& WorldLocation) const
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FVector2D UV = PdaMap::WorldToMapUV(FVector2D(WorldLocation.X, WorldLocation.Y),
		Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	return MapView.MapToViewport(UV, MapViewportSize, GetMapBaseSize());
}

void UPdaScreenWidget::SetMapViewportSize(const FVector2D& Size)
{
	if (Size.X < 1.0 || Size.Y < 1.0)
	{
		return; // раскладка ещё не посчитана
	}
	const bool bChanged = !Size.Equals(MapViewportSize, 0.5);
	MapViewportSize = Size;
	if (bCenterOnHeroPending)
	{
		bCenterOnHeroPending = false;
		CenterMapOnHero(); // сам ставит карту в поле
	}
	else if (bChanged)
	{
		ApplyMapView();
	}
}

void UPdaScreenWidget::ApplyMapView()
{
	if (MapViewportSize.X < 1.0 || MapViewportSize.Y < 1.0)
	{
		return;
	}
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FVector2D BaseSize = GetMapBaseSize();
	MapView.Zoom = PdaMap::FMapView::ClampZoom(MapView.Zoom, Settings->MapMaxZoom);
	MapView.Normalize(MapViewportSize, BaseSize);

	if (UCanvasPanelSlot* AreaSlot = MapArea ? Cast<UCanvasPanelSlot>(MapArea->Slot) : nullptr)
	{
		AreaSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		AreaSlot->SetAlignment(FVector2D::ZeroVector);
		AreaSlot->SetPosition(MapView.GetContentOffset(MapViewportSize, BaseSize));
		AreaSlot->SetSize(MapView.GetContentSize(BaseSize));
	}
	// Кнопки у пределов гаснут: дальше приближать или отдалять некуда.
	if (MapZoomInButton)
	{
		MapZoomInButton->SetIsEnabled(MapView.Zoom < FMath::Max(1.0f, Settings->MapMaxZoom) - KINDA_SMALL_NUMBER);
	}
	if (MapZoomOutButton)
	{
		MapZoomOutButton->SetIsEnabled(MapView.Zoom > 1.0 + KINDA_SMALL_NUMBER);
	}
	UpdateSearchAreaMark(); // круг района растёт вместе с картой
}

void UPdaScreenWidget::ZoomMapAt(const FVector2D& ViewportPoint, double NewZoom)
{
	MapView.ZoomAt(ViewportPoint, NewZoom, UContraryPdaSettings::Get()->MapMaxZoom, MapViewportSize, GetMapBaseSize());
	ApplyMapView();
}

void UPdaScreenWidget::ZoomMapBySteps(int32 Steps)
{
	const double Step = FMath::Max(1.05, static_cast<double>(UContraryPdaSettings::Get()->MapZoomStep));
	ZoomMapAt(MapViewportSize * 0.5, MapView.Zoom * FMath::Pow(Step, static_cast<double>(Steps)));
}

void UPdaScreenWidget::PanMapBy(const FVector2D& Delta)
{
	MapView.PanBy(Delta, MapViewportSize, GetMapBaseSize());
	ApplyMapView();
}

void UPdaScreenWidget::PinchMap(const FVector2D& OldA, const FVector2D& OldB, const FVector2D& NewA, const FVector2D& NewB)
{
	MapView.Pinch(OldA, OldB, NewA, NewB, UContraryPdaSettings::Get()->MapMaxZoom, MapViewportSize, GetMapBaseSize());
	ApplyMapView();
}

void UPdaScreenWidget::CenterMapOnHero()
{
	if (Player.IsValid())
	{
		const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
		const FVector Location = Player->GetActorLocation();
		MapView.CenterOn(PdaMap::WorldToMapUV(FVector2D(Location.X, Location.Y),
			Settings->MapWorldTopLeft, Settings->MapWorldBottomRight), MapViewportSize, GetMapBaseSize());
	}
	ApplyMapView();
}

void UPdaScreenWidget::HandleZoomInClicked()
{
	ZoomMapBySteps(1);
}

void UPdaScreenWidget::HandleZoomOutClicked()
{
	ZoomMapBySteps(-1);
}

void UPdaScreenWidget::HandleFindHeroClicked()
{
	CenterMapOnHero();
}

// ===========================================================================
// Окно-смартфон: размер, отступы экрана, картинка
// ===========================================================================

void UPdaScreenWidget::ApplyPhoneLayout(const FVector2D& ScreenSize)
{
	if (ScreenSize.X < 1.0 || ScreenSize.Y < 1.0)
	{
		return;
	}
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	UTexture2D* PhoneTexture = Settings->PhoneTexture.IsNull() ? nullptr : Settings->PhoneTexture.LoadSynchronous();

	// Соотношение сторон — с картинки, а без неё — из настроек.
	double Aspect = Settings->PhoneAspectRatio;
	const FIntPoint PictureSize = PdaMap::GetPictureSize(PhoneTexture);
	if (PictureSize.X > 0 && PictureSize.Y > 0)
	{
		Aspect = static_cast<double>(PictureSize.X) / PictureSize.Y;
	}
	PhoneSize = PdaMap::ComputePhoneSize(ScreenSize, Settings->PhoneHeightFraction, Aspect, Settings->PhoneSideMargin);
	PhonePadding = PdaMap::ComputePhoneScreenPadding(PhoneSize, Settings->PhoneScreenInsets);

	if (PanelPlate)
	{
		if (UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(PanelPlate->Slot))
		{
			PlateSlot->SetSize(PhoneSize);
		}
		PanelPlate->SetPadding(PhonePadding);
		// Картинка есть — подложка рисуется ею; нет — остаётся вид из файла окна (тёмная плашка).
		// Кисть ставится, только если в окне сейчас не эта картинка чистой кистью: вид,
		// заданный в файле окна, код не перетирает.
		const FSlateBrush& Current = PanelPlate->Background;
		if (PhoneTexture && (Current.GetResourceObject() != PhoneTexture || Current.DrawAs != ESlateBrushDrawType::Image))
		{
			PanelPlate->SetBrush(MakePhonePictureBrush(PhoneTexture));
			PanelPlate->SetBrushColor(FLinearColor::White);
		}
	}
	LoadedPhoneTexture = PhoneTexture;
	UpdateSideMenuWidth();

	// Одна строка на каждую подгонку (открытие окна, смена размера экрана): по ней видно, с
	// какими числами смартфон встал на экран.
	const UCanvasPanelSlot* LoggedSlot = PanelPlate ? Cast<UCanvasPanelSlot>(PanelPlate->Slot) : nullptr;
	const FVector2D SlotSize = LoggedSlot ? LoggedSlot->GetSize() : FVector2D::ZeroVector;
	UE_LOG(LogQA, Display,
		TEXT("QA: pda phone layout: screen=%.0fx%.0f, picture=%dx%d (ready now: %dx%d), aspect=%.3f, phone=%.1fx%.1f, plate slot=%s %.1fx%.1f"),
		ScreenSize.X, ScreenSize.Y, PictureSize.X, PictureSize.Y,
		PhoneTexture ? PhoneTexture->GetSizeX() : 0, PhoneTexture ? PhoneTexture->GetSizeY() : 0,
		Aspect, PhoneSize.X, PhoneSize.Y, LoggedSlot ? TEXT("canvas") : TEXT("NOT CANVAS"), SlotSize.X, SlotSize.Y);
}

// ===========================================================================
// Мышь и пальцы
// ===========================================================================

bool UPdaScreenWidget::ScreenToMapViewport(const FVector2D& ScreenPosition, FVector2D& OutPoint) const
{
	if (!bMapTab || !MapPanel)
	{
		return false;
	}
	const FGeometry& PanelGeometry = MapPanel->GetCachedGeometry();
	const FVector2D Size = PanelGeometry.GetLocalSize();
	OutPoint = PanelGeometry.AbsoluteToLocal(ScreenPosition);
	return Size.X >= 1.0 && Size.Y >= 1.0
		&& OutPoint.X >= 0.0 && OutPoint.Y >= 0.0 && OutPoint.X <= Size.X && OutPoint.Y <= Size.Y;
}

bool UPdaScreenWidget::IsOverMapButton(const FVector2D& ScreenPosition) const
{
	auto IsUnder = [&ScreenPosition](const UWidget* Widget)
	{
		return Widget && Widget->IsVisible() && Widget->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	};
	if (IsUnder(MapZoomInButton) || IsUnder(MapZoomOutButton) || IsUnder(MapFindHeroButton))
	{
		return true;
	}
	if (MapMarkers)
	{
		for (int32 Index = 0; Index < MapMarkers->GetChildrenCount(); ++Index)
		{
			const UWidget* Child = MapMarkers->GetChildAt(Index);
			if (Child && Child->IsA<UButton>() && IsUnder(Child))
			{
				return true;
			}
		}
	}
	return false;
}

bool UPdaScreenWidget::IsOutsidePhone(const FVector2D& ScreenPosition) const
{
	if (!PanelPlate)
	{
		return false;
	}
	const FGeometry& PlateGeometry = PanelPlate->GetCachedGeometry();
	return PlateGeometry.GetLocalSize().X >= 1.0 && !PlateGeometry.IsUnderLocation(ScreenPosition);
}

FReply UPdaScreenWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D Point;
	if (ScreenToMapViewport(InMouseEvent.GetScreenSpacePosition(), Point))
	{
		const double Step = FMath::Max(1.05, static_cast<double>(UContraryPdaSettings::Get()->MapZoomStep));
		ZoomMapAt(Point, MapView.Zoom * FMath::Pow(Step, static_cast<double>(InMouseEvent.GetWheelDelta())));
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

FReply UPdaScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Нажатие мимо смартфона закрывает окно.
		if (IsOutsidePhone(InMouseEvent.GetScreenSpacePosition()))
		{
			OnCloseRequested.Broadcast();
			return FReply::Handled();
		}
		FVector2D Point;
		if (ScreenToMapViewport(InMouseEvent.GetScreenSpacePosition(), Point))
		{
			bMouseDragging = true;
			LastMouseDragPoint = Point;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPdaScreenWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bMouseDragging && MapPanel)
	{
		const FVector2D Point = MapPanel->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		PanMapBy(Point - LastMouseDragPoint);
		LastMouseDragPoint = Point;
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPdaScreenWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bMouseDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bMouseDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UPdaScreenWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// Палец на кнопке поля (приближение, отметка задания) — событие не берём: тогда движок сам
	// превратит касание в нажатие этой кнопки. Мимо поля — тоже не берём (нажатие мимо
	// смартфона дойдёт до обработчика мыши и закроет окно).
	FVector2D Point;
	if (MapTouches.Num() < 2 && ScreenToMapViewport(InGestureEvent.GetScreenSpacePosition(), Point)
		&& !IsOverMapButton(InGestureEvent.GetScreenSpacePosition()))
	{
		MapTouches.Add(InGestureEvent.GetPointerIndex(), Point);
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

FReply UPdaScreenWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	FVector2D* Stored = MapTouches.Find(InGestureEvent.GetPointerIndex());
	if (Stored && MapPanel)
	{
		const FVector2D OldPoint = *Stored;
		const FVector2D NewPoint = MapPanel->GetCachedGeometry().AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
		*Stored = NewPoint;
		if (MapTouches.Num() >= 2)
		{
			// Второй палец стоит на месте в этом событии: его точка — и старая, и новая.
			FVector2D Other = NewPoint;
			for (const TPair<uint32, FVector2D>& Touch : MapTouches)
			{
				if (Touch.Key != InGestureEvent.GetPointerIndex())
				{
					Other = Touch.Value;
					break;
				}
			}
			PinchMap(OldPoint, Other, NewPoint, Other);
		}
		else
		{
			PanMapBy(NewPoint - OldPoint);
		}
		return FReply::Handled();
	}
	return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

FReply UPdaScreenWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (MapTouches.Remove(InGestureEvent.GetPointerIndex()) > 0)
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

void UPdaScreenWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
	bMouseDragging = false;
	MapTouches.Reset();
}

void UPdaScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Размер экрана сменился (или окно показано впервые) — подгоняем смартфон.
	const FVector2D ScreenSize = MyGeometry.GetLocalSize();
	if (!ScreenSize.Equals(LastScreenSize, 0.5))
	{
		LastScreenSize = ScreenSize;
		ApplyPhoneLayout(ScreenSize);
	}

	// Журнал изменился, пока окно открыто (или игрок что-то нажал в окне) — перерисовываем.
	if (bRefreshPending || ComputeJournalStamp() != LastJournalStamp)
	{
		RefreshNow();
	}
	if (bMapTab)
	{
		if (MapPanel)
		{
			SetMapViewportSize(MapPanel->GetCachedGeometry().GetLocalSize());
		}
		// Герой открыл новое, пока карта на экране, — перерисовываем черноту и подписи мест.
		const UMapExplorationComponent* Exploration = GetExploration();
		if (Exploration && Exploration->GetRevision() != LastFogRevision)
		{
			RebuildMap();
		}
	}
	// Район поиска мог появиться или пропасть (время вышло, герой пришёл), пока окно открыто.
	UpdateSearchAreaMark();
	UpdateMapTabAttention(InDeltaTime);
}

// ===========================================================================
// Меню вкладок, район поиска, мигание вкладки «Карта»
// ===========================================================================

UMapExplorationComponent* UPdaScreenWidget::GetExplorationMutable() const
{
	return Player.IsValid() ? Player->GetMapExploration() : nullptr;
}

void UPdaScreenWidget::UpdateSideMenuWidth()
{
	UCanvasPanelSlot* MenuSlot = SideMenu ? Cast<UCanvasPanelSlot>(SideMenu->Slot) : nullptr;
	const UWidget* ActivePanel = bMapTab ? static_cast<const UWidget*>(MapPanel) : static_cast<const UWidget*>(QuestsPanel);
	const UCanvasPanelSlot* ActiveSlot = ActivePanel ? Cast<UCanvasPanelSlot>(ActivePanel->Slot) : nullptr;
	const double InnerWidth = PhoneSize.X - PhonePadding.Left - PhonePadding.Right;
	if (!MenuSlot || !ActiveSlot || InnerWidth < 1.0)
	{
		return; // размер смартфона ещё не посчитан — меню стоит, как в файле окна
	}
	// Левый край открытой вкладки — из её места в файле окна.
	const FAnchors Anchors = ActiveSlot->GetAnchors();
	const FMargin Offsets = ActiveSlot->GetOffsets();
	const bool bStretched = Anchors.Minimum.X != Anchors.Maximum.X;
	const double PanelLeft = Anchors.Minimum.X * InnerWidth
		+ (bStretched ? Offsets.Left : Offsets.Left - Offsets.Right * ActiveSlot->GetAlignment().X);

	FAnchorData Layout = MenuSlot->GetLayout();
	SideMenuWidth = FMath::Max(0.0, PanelLeft - SideMenuGap - Layout.Offsets.Left);
	if (Layout.Anchors.Minimum.X != 0.0f || Layout.Anchors.Maximum.X != 0.0f
		|| !FMath::IsNearlyEqual(Layout.Offsets.Right, static_cast<float>(SideMenuWidth), 0.05f))
	{
		Layout.Anchors.Minimum.X = 0.0f;
		Layout.Anchors.Maximum.X = 0.0f;
		Layout.Alignment.X = 0.0f;
		Layout.Offsets.Right = static_cast<float>(SideMenuWidth); // при одной точке привязки — ширина
		MenuSlot->SetLayout(Layout);
	}
}

void UPdaScreenWidget::UpdateSearchAreaMark()
{
	const UMapExplorationComponent* Exploration = GetExploration();
	const bool bShow = Exploration && Exploration->IsSearchAreaMarked() && (MapAreaMark || MapAreaMarkText);
	if (!bShow)
	{
		bSearchAreaShown = false;
		if (MapAreaMark && MapAreaMark->GetVisibility() != ESlateVisibility::Collapsed)
		{
			MapAreaMark->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (MapAreaMarkText && MapAreaMarkText->GetVisibility() != ESlateVisibility::Collapsed)
		{
			MapAreaMarkText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FVector2D UV = PdaMap::WorldToMapUV(Exploration->GetSearchAreaCenter(),
		Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
	if (bSearchAreaShown && UV.Equals(ShownSearchAreaUV, 0.0001) && FMath::IsNearlyEqual(MapView.Zoom, ShownSearchAreaZoom, 0.0001))
	{
		return; // стоит, где надо
	}
	bSearchAreaShown = true;
	ShownSearchAreaUV = UV;
	ShownSearchAreaZoom = MapView.Zoom;

	// Место — долями карты (якоря), как у остальных отметок; размер, вид и сдвиг подписи — из
	// файла окна. Круг растёт вместе с приближением карты, подпись отъезжает на край круга.
	const FAnchors Anchors(static_cast<float>(UV.X), static_cast<float>(UV.Y));
	double MarkHalfHeight = 0.0;
	if (MapAreaMark)
	{
		if (UCanvasPanelSlot* MarkSlot = Cast<UCanvasPanelSlot>(MapAreaMark->Slot))
		{
			MarkSlot->SetAnchors(Anchors);
			MarkHalfHeight = MarkSlot->GetSize().Y * 0.5;
		}
		MapAreaMark->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		MapAreaMark->SetRenderScale(FVector2D(MapView.Zoom, MapView.Zoom));
		MapAreaMark->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (MapAreaMarkText)
	{
		if (UCanvasPanelSlot* CaptionSlot = Cast<UCanvasPanelSlot>(MapAreaMarkText->Slot))
		{
			CaptionSlot->SetAnchors(Anchors);
		}
		MapAreaMarkText->SetRenderTranslation(FVector2D(0.0, MarkHalfHeight * (MapView.Zoom - 1.0)));
		MapAreaMarkText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UPdaScreenWidget::UpdateMapTabAttention(float DeltaTime)
{
	UMapExplorationComponent* Exploration = GetExplorationMutable();
	if (Exploration && bMapTab)
	{
		Exploration->NoteMapTabShown();
	}
	const bool bBlink = Exploration && Exploration->IsMapTabAttentionOn() && MapTabButton;
	if (bBlink)
	{
		if (!bMapTabAttentionActive)
		{
			MapTabIdleColor = MapTabButton->GetBackgroundColor(); // цвет из файла окна, к нему вернёмся
			MapTabAttentionTime = 0.0f;
			bMapTabAttentionActive = true;
		}
		MapTabAttentionTime += DeltaTime;
		// Та же формула, что у мигания экранных кнопок: яркость ходит по синусу.
		const float Pulse = 0.5f + 0.5f * FMath::Sin(2.0f * UE_PI * MapTabAttentionTime / FMath::Max(0.05f, MapTabAttentionPeriod));
		FLinearColor Color = MapTabAttentionColor * (1.0f - FMath::Clamp(MapTabAttentionDepth, 0.0f, 1.0f) * (1.0f - Pulse));
		Color.A = MapTabAttentionColor.A;
		MapTabButton->SetBackgroundColor(Color);
	}
	else if (bMapTabAttentionActive)
	{
		if (MapTabButton)
		{
			MapTabButton->SetBackgroundColor(MapTabIdleColor);
		}
		bMapTabAttentionActive = false;
	}
}

int32 UPdaScreenWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	// Стрелка героя — поверх всего на карте: остриё смотрит туда же, куда герой.
	if (!bMapTab || !MapArea || !MapPanel || !Player.IsValid())
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
	// Герой у края карты или за ним: стрелка прижимается к краю карты целиком, а не теряется.
	const FVector2D Center = ClampArrowCenter(AreaMin + UV * AreaSize, AreaMin, AreaMax, 14.0);

	// Карта приближена, и герой остался за краем поля — стрелку не рисуем (она вылезла бы на
	// рамку окна); вернуть её в поле — кнопка «найти героя».
	const FGeometry& PanelGeometry = MapPanel->GetCachedGeometry();
	const FVector2D PanelMin = AllottedGeometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D PanelMax = AllottedGeometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize()));
	if (Center.X < PanelMin.X + 10.0 || Center.Y < PanelMin.Y + 10.0
		|| Center.X > PanelMax.X - 10.0 || Center.Y > PanelMax.Y - 10.0)
	{
		return MaxLayer;
	}

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

