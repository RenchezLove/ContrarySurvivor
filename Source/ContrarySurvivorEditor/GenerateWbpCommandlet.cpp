// Fill out your copyright notice in the Description page of Project Settings.

#include "GenerateWbpCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Styling/SlateTypes.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

// Геймплей-модуль (включение по конвенции проекта, путь Source/ добавлен в Build.cs).
#include "ContrarySurvivor/UI/TouchControlsTypes.h"
#include "ContrarySurvivor/UI/InventoryScreenWidget.h"
#include "ContrarySurvivor/UI/InventoryRowWidget.h" // полный тип для TSubclassOf-присваивания

DEFINE_LOG_CATEGORY_STATIC(LogGenerateWbp, Log, All);

namespace
{
	// ======================================================================
	// Общие помощники стиля
	// ======================================================================

	// Круглая кисть без текстур (копия MakeCircleBrush из TouchControlsWidget.cpp —
	// сгенерированный тач-слой обязан выглядеть как кодовый).
	FSlateBrush MakeCircleBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}

	// Прямоугольник со скруглением углов (плашки панелей/кнопок экранов).
	FSlateBrush MakeRoundedBrush(const FLinearColor& Tint, float Radius,
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

	// Roboto движка — шрифт-UObject, гарантированно живущий в ассете (слейтовый шрифт кода
	// FCoreStyle — те же глифы Roboto, но как UObject в пакет не сериализуется).
	UObject* LoadRobotoFont()
	{
		return LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	}

	UTextBlock* MakeText(UWidgetTree* Tree, UObject* Roboto, const FName& Name,
		const FString& Text, const FLinearColor& Color, int32 Size, const TCHAR* Typeface)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(FText::FromString(Text));
		Block->SetFont(FSlateFontInfo(Roboto, Size, FName(Typeface)));
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}

	// Кнопка со сплошным стилем всех состояний (для экранов; Ринат перекрасит в дизайнере).
	UButton* MakeStyledButton(UWidgetTree* Tree, const FName& Name,
		const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style;
		Style.Normal   = MakeRoundedBrush(Normal, 4.0f);
		Style.Hovered  = MakeRoundedBrush(Hovered, 4.0f);
		Style.Pressed  = MakeRoundedBrush(Pressed, 4.0f);
		Style.Disabled = MakeRoundedBrush(FLinearColor(Normal.R, Normal.G, Normal.B, Normal.A * 0.5f), 4.0f);
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		Button->SetStyle(Style);
		Button->bIsVariable = true;
		return Button;
	}

	// ======================================================================
	// Дефолты тач-слоя — с CDO BP-контроллера (живые значения игры, включая
	// возможный тюнинг Рината в BP; поля protected — читаем через reflection)
	// ======================================================================

	struct FTouchLayerDefaults
	{
		float StickRadius = 110.0f;
		float StickThumbRadius = 45.0f;
		FVector2D StickMargin = FVector2D(160.0f, 160.0f);
		float IdleOpacity = 0.5f;
		FLinearColor StickBaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);
		FLinearColor StickThumbColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.6f);
		FTouchButtonSettings Fire, Reload, Interact, Sprint, Weapon, Inventory, Pause;
	};

	template <typename T>
	void ReadCdoProp(UObject* CDO, const TCHAR* PropName, T& OutValue)
	{
		if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(PropName))
		{
			OutValue = *Prop->ContainerPtrToValuePtr<T>(CDO);
		}
		else
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("Поле %s на контроллере не найдено — беру C++-дефолт."), PropName);
		}
	}

	bool ReadTouchDefaults(FTouchLayerDefaults& Out)
	{
		// Живой источник — BP контроллера (в нём тюнинг Рината); нет BP — нативный класс.
		UClass* PCClass = StaticLoadClass(APlayerController::StaticClass(), nullptr,
			TEXT("/Game/System/BP_ContrarySurviorPlayerController.BP_ContrarySurviorPlayerController_C"));
		if (!PCClass)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("BP_ContrarySurviorPlayerController не загрузился — дефолты с нативного класса."));
			PCClass = StaticLoadClass(APlayerController::StaticClass(), nullptr,
				TEXT("/Script/ContrarySurvivor.ContrarySurvivorPlayerController"));
		}
		if (!PCClass)
		{
			return false;
		}
		UObject* CDO = PCClass->GetDefaultObject();
		ReadCdoProp(CDO, TEXT("TouchStickRadius"), Out.StickRadius);
		ReadCdoProp(CDO, TEXT("TouchStickThumbRadius"), Out.StickThumbRadius);
		ReadCdoProp(CDO, TEXT("TouchStickMargin"), Out.StickMargin);
		ReadCdoProp(CDO, TEXT("TouchIdleOpacity"), Out.IdleOpacity);
		ReadCdoProp(CDO, TEXT("TouchStickBaseColor"), Out.StickBaseColor);
		ReadCdoProp(CDO, TEXT("TouchStickThumbColor"), Out.StickThumbColor);
		ReadCdoProp(CDO, TEXT("TouchFireButton"), Out.Fire);
		ReadCdoProp(CDO, TEXT("TouchReloadButton"), Out.Reload);
		ReadCdoProp(CDO, TEXT("TouchInteractButton"), Out.Interact);
		ReadCdoProp(CDO, TEXT("TouchSprintButton"), Out.Sprint);
		ReadCdoProp(CDO, TEXT("TouchWeaponButton"), Out.Weapon);
		ReadCdoProp(CDO, TEXT("TouchInventoryButton"), Out.Inventory);
		ReadCdoProp(CDO, TEXT("TouchPauseButton"), Out.Pause);
		return true;
	}

	// ======================================================================
	// Тач-слой: WBP_TouchControls (раскладка = MakeTouchButton кодового пути)
	// ======================================================================

	enum class ETouchCorner : uint8 { BottomRight, TopRight, TopLeft };

	void AddTouchButton(UWidgetTree* Tree, UCanvasPanel* Root, UObject* Roboto, float IdleOpacity,
		const FTouchButtonSettings& S, ETouchCorner Corner, const FName& ButtonName, const FName& TextName)
	{
		if (!S.bEnabled)
		{
			return;
		}

		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		// Тот же стиль состояний, что в TouchControlsWidget::MakeTouchButton.
		const FLinearColor& Tint = S.Color;
		FButtonStyle Style;
		Style.Normal   = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.30f));
		Style.Hovered  = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.40f));
		Style.Pressed  = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.55f));
		Style.Disabled = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.15f));
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		Button->SetStyle(Style);
		Button->SetRenderOpacity(IdleOpacity);
		Button->bIsVariable = true;

		const int32 FontSize = (S.FontSize > 0)
			? S.FontSize
			: FMath::Clamp<int32>(FMath::RoundToInt(S.Radius * 0.30f), 10, 22);
		UTextBlock* Label = MakeText(Tree, Roboto, TextName, S.Label, S.TextColor, FontSize, TEXT("Bold"));
		Label->bIsVariable = true;
		Button->SetContent(Label);

		if (UCanvasPanelSlot* BtnSlot = Root->AddChildToCanvas(Button))
		{
			FVector2D Pos = FVector2D::ZeroVector;
			switch (Corner)
			{
			case ETouchCorner::BottomRight:
				BtnSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
				Pos = FVector2D(-S.Margin.X, -S.Margin.Y);
				break;
			case ETouchCorner::TopRight:
				BtnSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
				Pos = FVector2D(-S.Margin.X, S.Margin.Y);
				break;
			case ETouchCorner::TopLeft:
				BtnSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				Pos = FVector2D(S.Margin.X, S.Margin.Y);
				break;
			}
			BtnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			BtnSlot->SetPosition(Pos);
			BtnSlot->SetSize(FVector2D(S.Radius * 2.0f, S.Radius * 2.0f));
		}
	}

	bool BuildTouchControls(UWidgetTree* Tree)
	{
		FTouchLayerDefaults D;
		if (!ReadTouchDefaults(D))
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("Класс контроллера не загрузился — дефолты недоступны."));
			return false;
		}
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root; // канва по умолчанию SelfHitTestInvisible — тапы мимо кнопок уходят в мир

		// Стик (низ-лево), геометрия и цвета — как в кодовом NativeOnInitialized+InitTouch.
		UImage* Base = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StickBase"));
		Base->SetBrush(MakeCircleBrush(D.StickBaseColor));
		Base->SetVisibility(ESlateVisibility::Visible); // хит-зона жеста стика
		Base->SetRenderOpacity(D.IdleOpacity);
		Base->bIsVariable = true;
		if (UCanvasPanelSlot* BaseSlot = Root->AddChildToCanvas(Base))
		{
			BaseSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
			BaseSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			BaseSlot->SetPosition(FVector2D(D.StickMargin.X, -D.StickMargin.Y));
			BaseSlot->SetSize(FVector2D(D.StickRadius * 2.0f, D.StickRadius * 2.0f));
		}

		UImage* Thumb = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StickThumb"));
		Thumb->SetBrush(MakeCircleBrush(D.StickThumbColor));
		Thumb->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // чистый визуал
		Thumb->SetRenderOpacity(D.IdleOpacity);
		Thumb->bIsVariable = true;
		if (UCanvasPanelSlot* ThumbSlot = Root->AddChildToCanvas(Thumb))
		{
			ThumbSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
			ThumbSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ThumbSlot->SetPosition(FVector2D(D.StickMargin.X, -D.StickMargin.Y));
			ThumbSlot->SetSize(FVector2D(D.StickThumbRadius * 2.0f, D.StickThumbRadius * 2.0f));
		}

		// Кнопки: углы и порядок — как BuildButtons кодового пути.
		AddTouchButton(Tree, Root, Roboto, D.IdleOpacity, D.Fire, ETouchCorner::BottomRight, TEXT("FireButton"), TEXT("FireText"));
		AddTouchButton(Tree, Root, Roboto, D.IdleOpacity, D.Reload, ETouchCorner::BottomRight, TEXT("ReloadButton"), TEXT("ReloadText"));
		AddTouchButton(Tree, Root, Roboto, D.IdleOpacity, D.Interact, ETouchCorner::BottomRight, TEXT("InteractButton"), TEXT("InteractText"));
		AddTouchButton(Tree, Root, Roboto, D.IdleOpacity, D.Sprint, ETouchCorner::BottomRight, TEXT("SprintButton"), TEXT("SprintText"));
		AddTouchButton(Tree, Root, Roboto, D.IdleOpacity, D.Weapon, ETouchCorner::BottomRight, TEXT("WeaponButton"), TEXT("WeaponText"));
		AddTouchButton(Tree, Root, Roboto, D.IdleOpacity, D.Inventory, ETouchCorner::TopRight, TEXT("InventoryButton"), TEXT("InventoryText"));
		AddTouchButton(Tree, Root, Roboto, D.IdleOpacity, D.Pause, ETouchCorner::TopLeft, TEXT("PauseButton"), TEXT("PauseText"));
		return true;
	}

	// ======================================================================
	// Экраны (геометрия и цвета — дефолты Canvas-пути ContrarySurvivorHUD.h)
	// ======================================================================

	// Одна строка рюкзака: плашка -> имя + «использовать» + выброс.
	bool BuildInventoryRow(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		USizeBox* RowSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RowSize"));
		RowSize->SetMinDesiredHeight(48.0f); // комфортная пальцу высота строки (guide: 40-60)
		Tree->RootWidget = RowSize;

		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RowPlate"));
		Plate->SetBrush(MakeRoundedBrush(FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), 4.0f)); // InvSlotColor
		Plate->SetPadding(FMargin(8.0f, 4.0f));
		RowSize->SetContent(Plate);

		UHorizontalBox* RowBox = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RowBox"));
		Plate->SetContent(RowBox);

		UTextBlock* Name = MakeText(Tree, Roboto, TEXT("NameText"), TEXT("Предмет x1"),
			FLinearColor::White, 15, TEXT("Regular"));
		Name->bIsVariable = true;
		if (UHorizontalBoxSlot* NameSlot = RowBox->AddChildToHorizontalBox(Name))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		UButton* Use = MakeStyledButton(Tree, TEXT("UseButton"),
			FLinearColor(0.2f, 0.35f, 0.5f, 1.0f), FLinearColor(0.25f, 0.45f, 0.62f, 1.0f),
			FLinearColor(0.3f, 0.5f, 0.7f, 1.0f));
		UTextBlock* UseCaption = MakeText(Tree, Roboto, TEXT("UseText"), TEXT("использовать"),
			FLinearColor::White, 13, TEXT("Regular"));
		UseCaption->bIsVariable = true;
		Use->SetContent(UseCaption);
		if (UHorizontalBoxSlot* UseSlot = RowBox->AddChildToHorizontalBox(Use))
		{
			UseSlot->SetVerticalAlignment(VAlign_Center);
			UseSlot->SetPadding(FMargin(6.0f, 2.0f));
		}

		UButton* Drop = MakeStyledButton(Tree, TEXT("DropButton"),
			FLinearColor(0.45f, 0.15f, 0.12f, 1.0f), FLinearColor(0.58f, 0.2f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f));
		// Подпись выброса — статичная (код её не трогает, текст Рината).
		Drop->SetContent(MakeText(Tree, Roboto, TEXT("DropLabel"), TEXT("X"),
			FLinearColor::White, 13, TEXT("Bold")));
		if (UHorizontalBoxSlot* DropSlot = RowBox->AddChildToHorizontalBox(Drop))
		{
			DropSlot->SetVerticalAlignment(VAlign_Center);
			DropSlot->SetPadding(FMargin(2.0f, 2.0f));
		}
		return true;
	}

	// Слот брони paper-doll: кнопка (клик по занятому — снять) с подписью, иконкой и текстом.
	void AddArmorSlotButton(UWidgetTree* Tree, UVerticalBox* Column, UObject* Roboto,
		const FString& StaticCaption, const FName& ButtonName, const FName& IconName, const FName& TextName)
	{
		UButton* SlotButton = MakeStyledButton(Tree, ButtonName,
			FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
			FLinearColor(0.25f, 0.27f, 0.33f, 1.0f)); // InvSlotColor + подсветки

		UHorizontalBox* SlotBox = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			FName(*(ButtonName.ToString() + TEXT("Box"))));
		SlotButton->SetContent(SlotBox);

		// Статичная подпись слота («Шлем») — текст Рината, код не трогает.
		UTextBlock* Caption = MakeText(Tree, Roboto, FName(*(ButtonName.ToString() + TEXT("Caption"))),
			StaticCaption, FLinearColor(0.7f, 0.72f, 0.78f, 1.0f), 14, TEXT("Regular"));
		if (UHorizontalBoxSlot* CapSlot = SlotBox->AddChildToHorizontalBox(Caption))
		{
			CapSlot->SetVerticalAlignment(VAlign_Center);
			CapSlot->SetPadding(FMargin(8.0f, 0.0f));
		}

		// Иконка надетого: код прячет её на пустом слоте — в ассете сразу Collapsed.
		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);
		Icon->SetDesiredSizeOverride(FVector2D(40.0f, 40.0f));
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		Icon->bIsVariable = true;
		if (UHorizontalBoxSlot* IconSlot = SlotBox->AddChildToHorizontalBox(Icon))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(4.0f, 4.0f));
		}

		// Что надето — ставит код («(пусто)» / имя брони).
		UTextBlock* Worn = MakeText(Tree, Roboto, TextName, TEXT("(пусто)"),
			FLinearColor::White, 15, TEXT("Regular"));
		Worn->bIsVariable = true;
		if (UHorizontalBoxSlot* WornSlot = SlotBox->AddChildToHorizontalBox(Worn))
		{
			WornSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			WornSlot->SetVerticalAlignment(VAlign_Center);
			WornSlot->SetPadding(FMargin(8.0f, 0.0f));
		}

		USizeBox* Height = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(ButtonName.ToString() + TEXT("Size"))));
		Height->SetHeightOverride(56.0f); // InvSlotHeight
		Height->SetContent(SlotButton);
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Height))
		{
			RowSlot->SetPadding(FMargin(0.0f, 5.0f)); // InvSlotGap/2
		}
	}

	// Экран инвентаря: затемнение -> центральная панель -> статы, две колонки, закрытие.
	bool BuildInventory(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Затемнение на весь экран (InvDimColor); Visible — клики в мир не проходят (модалка).
		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Dim->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
		}

		// Центральная панель 960x600 (UIPanelMaxWidth/Height) с золотой рамкой (#18).
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
		Panel->SetBrush(MakeRoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.95f), 6.0f,
			FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
		Panel->SetPadding(FMargin(16.0f)); // UIPanelPadding
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetPosition(FVector2D::ZeroVector);
			PanelSlot->SetSize(FVector2D(960.0f, 600.0f));
		}

		UVerticalBox* PanelBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelBox"));
		Panel->SetContent(PanelBox);

		// Заголовок (статичный текст Рината) + строка статов (ставит код).
		PanelBox->AddChildToVerticalBox(MakeText(Tree, Roboto, TEXT("HeaderText"),
			TEXT("ИНВЕНТАРЬ  (Tab / I — закрыть)"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 22, TEXT("Bold")));

		UTextBlock* Stats = MakeText(Tree, Roboto, TEXT("StatsText"),
			TEXT("Монеты 0      Голод 100 / 100      Жажда 100 / 100"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 16, TEXT("Regular")); // UIMoneyColor
		Stats->bIsVariable = true;
		if (UVerticalBoxSlot* StatsSlot = PanelBox->AddChildToVerticalBox(Stats))
		{
			StatsSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 12.0f));
		}

		// Две колонки: слева снаряжение (0.42 ширины — InvLeftColumnFrac), справа рюкзак.
		UHorizontalBox* Columns = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ColumnsBox"));
		if (UVerticalBoxSlot* ColumnsSlot = PanelBox->AddChildToVerticalBox(Columns))
		{
			ColumnsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UVerticalBox* EquipColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EquipBox"));
		if (UHorizontalBoxSlot* EquipSlot = Columns->AddChildToHorizontalBox(EquipColumn))
		{
			FSlateChildSize LeftSize(ESlateSizeRule::Fill);
			LeftSize.Value = 0.42f;
			EquipSlot->SetSize(LeftSize);
			EquipSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}
		EquipColumn->AddChildToVerticalBox(MakeText(Tree, Roboto, TEXT("EquipHeaderText"),
			TEXT("СНАРЯЖЕНИЕ"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")));

		AddArmorSlotButton(Tree, EquipColumn, Roboto, TEXT("Шлем"),
			TEXT("HeadSlotButton"), TEXT("HeadSlotIcon"), TEXT("HeadSlotText"));
		AddArmorSlotButton(Tree, EquipColumn, Roboto, TEXT("Торс"),
			TEXT("TorsoSlotButton"), TEXT("TorsoSlotIcon"), TEXT("TorsoSlotText"));
		AddArmorSlotButton(Tree, EquipColumn, Roboto, TEXT("Штаны"),
			TEXT("LegsSlotButton"), TEXT("LegsSlotIcon"), TEXT("LegsSlotText"));

		UTextBlock* Protection = MakeText(Tree, Roboto, TEXT("ProtectionText"), TEXT("Защита: 0%"),
			FLinearColor(0.6f, 0.9f, 0.6f, 1.0f), 15, TEXT("Regular"));
		Protection->bIsVariable = true;
		if (UVerticalBoxSlot* ProtSlot = EquipColumn->AddChildToVerticalBox(Protection))
		{
			ProtSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 2.0f));
		}
		UTextBlock* Weapon = MakeText(Tree, Roboto, TEXT("WeaponText"), TEXT("Оружие: (нет)"),
			FLinearColor::White, 15, TEXT("Regular"));
		Weapon->bIsVariable = true;
		EquipColumn->AddChildToVerticalBox(Weapon);

		UVerticalBox* BackpackColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BackpackBox"));
		if (UHorizontalBoxSlot* PackSlot = Columns->AddChildToHorizontalBox(BackpackColumn))
		{
			FSlateChildSize RightSize(ESlateSizeRule::Fill);
			RightSize.Value = 0.58f;
			PackSlot->SetSize(RightSize);
		}
		BackpackColumn->AddChildToVerticalBox(MakeText(Tree, Roboto, TEXT("BackpackHeaderText"),
			TEXT("РЮКЗАК"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")));

		UScrollBox* Backpack = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BackpackList"));
		Backpack->bIsVariable = true;
		if (UVerticalBoxSlot* ListSlot = BackpackColumn->AddChildToVerticalBox(Backpack))
		{
			ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ListSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		// Кнопка закрытия (низ-право; дублирует Tab — по guide необязательная, но кладём).
		UButton* Close = MakeStyledButton(Tree, TEXT("CloseButton"),
			FLinearColor(0.3f, 0.3f, 0.34f, 1.0f), FLinearColor(0.4f, 0.4f, 0.45f, 1.0f),
			FLinearColor(0.5f, 0.5f, 0.55f, 1.0f));
		UTextBlock* CloseCaption = MakeText(Tree, Roboto, TEXT("CloseLabel"), TEXT("Закрыть (Tab)"),
			FLinearColor::White, 15, TEXT("Regular"));
		Close->SetContent(CloseCaption);
		if (UVerticalBoxSlot* CloseSlot = PanelBox->AddChildToVerticalBox(Close))
		{
			CloseSlot->SetHorizontalAlignment(HAlign_Right);
			CloseSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		}
		return true;
	}

	// Экран смерти: затемнение -> центрированный столбец (заголовок, статистика, штраф, кнопка).
	bool BuildDeath(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f)); // DeathDimColor
		Dim->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
		}

		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DeathBox"));
		if (UCanvasPanelSlot* ColSlot = Root->AddChildToCanvas(Column))
		{
			// Столбец центрирован чуть выше середины — как Canvas (заголовок 0.18, статы 0.40).
			ColSlot->SetAnchors(FAnchors(0.5f, 0.45f, 0.5f, 0.45f));
			ColSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ColSlot->SetPosition(FVector2D::ZeroVector);
			ColSlot->SetAutoSize(true);
		}

		auto AddLine = [&](UTextBlock* Line, float TopPad = 3.0f, float BottomPad = 3.0f)
		{
			if (UVerticalBoxSlot* LineSlot = Column->AddChildToVerticalBox(Line))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Center);
				LineSlot->SetPadding(FMargin(0.0f, TopPad, 0.0f, BottomPad));
			}
		};

		// Статичные строки (заголовок/штраф/подсказка) — тексты Рината, формулировки ADR-044.
		AddLine(MakeText(Tree, Roboto, TEXT("TitleText"), TEXT("ВЫ ПОГИБЛИ"),
			FLinearColor(0.9f, 0.12f, 0.1f, 1.0f), 42, TEXT("Bold")), 0.0f, 22.0f);

		const FLinearColor StatColor(0.95f, 0.95f, 0.95f, 1.0f);
		UTextBlock* Lifetime = MakeText(Tree, Roboto, TEXT("LifetimeText"), TEXT("Прожито:  00:00"), StatColor, 22, TEXT("Regular"));
		UTextBlock* Killer = MakeText(Tree, Roboto, TEXT("KillerText"), TEXT("Убийца:  —"), StatColor, 22, TEXT("Regular"));
		UTextBlock* Money = MakeText(Tree, Roboto, TEXT("MoneyText"), TEXT("Монеты:  0"), StatColor, 22, TEXT("Regular"));
		UTextBlock* Quests = MakeText(Tree, Roboto, TEXT("QuestsText"), TEXT("Квестов выполнено:  0"), StatColor, 22, TEXT("Regular"));
		UTextBlock* Kills = MakeText(Tree, Roboto, TEXT("KillsText"), TEXT("Врагов убито:  0"), StatColor, 22, TEXT("Regular"));
		for (UTextBlock* Line : { Lifetime, Killer, Money, Quests, Kills })
		{
			Line->bIsVariable = true;
			AddLine(Line);
		}

		AddLine(MakeText(Tree, Roboto, TEXT("RespawnLineText"),
			TEXT("Возрождение у костра в деревне."), StatColor, 19, TEXT("Regular")), 16.0f);
		UTextBlock* MoneyLoss = MakeText(Tree, Roboto, TEXT("MoneyLossText"),
			TEXT("−40% монет — часть монет утрачена при гибели."),
			FLinearColor(0.95f, 0.55f, 0.15f, 1.0f), 19, TEXT("Regular")); // DeathPenaltyColor
		MoneyLoss->bIsVariable = true;
		AddLine(MoneyLoss);
		AddLine(MakeText(Tree, Roboto, TEXT("ConsumablesLineText"),
			TEXT("Расходники обронены мешком на месте гибели."), StatColor, 19, TEXT("Regular")));
		AddLine(MakeText(Tree, Roboto, TEXT("SavedLineText"),
			TEXT("Снаряжение, оружие и важные предметы сохранены."),
			FLinearColor(0.45f, 0.85f, 0.45f, 1.0f), 19, TEXT("Regular"))); // DeathSavedColor

		// Кнопка возрождения 360x56 (DeathButtonMaxWidth/Height, зелёная как Canvas).
		UButton* Respawn = MakeStyledButton(Tree, TEXT("RespawnButton"),
			FLinearColor(0.2f, 0.45f, 0.25f, 1.0f), FLinearColor(0.3f, 0.6f, 0.35f, 1.0f),
			FLinearColor(0.35f, 0.7f, 0.4f, 1.0f));
		Respawn->SetContent(MakeText(Tree, Roboto, TEXT("RespawnLabel"), TEXT("ВОЗРОДИТЬСЯ"),
			FLinearColor::White, 22, TEXT("Bold")));
		USizeBox* RespawnSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RespawnSize"));
		RespawnSize->SetWidthOverride(360.0f);
		RespawnSize->SetHeightOverride(56.0f);
		RespawnSize->SetContent(Respawn);
		if (UVerticalBoxSlot* BtnSlot = Column->AddChildToVerticalBox(RespawnSize))
		{
			BtnSlot->SetHorizontalAlignment(HAlign_Center);
			BtnSlot->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));
		}

		AddLine(MakeText(Tree, Roboto, TEXT("KeyHintText"), TEXT("Enter / Пробел — возродиться"),
			FLinearColor(0.8f, 0.8f, 0.8f, 1.0f), 14, TEXT("Regular")), 12.0f);
		return true;
	}

	// Трекер квеста: плашка в правом-верхнем углу со строкой (текст/цвет ставит код).
	bool BuildQuestTracker(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrackerPlate"));
		Plate->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f)); // QuestTrackerPlateColor
		Plate->SetPadding(FMargin(8.0f, 4.0f));
		if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
		{
			// Правый-верх, отступ 28/28 px и плашка по размеру текста — как Canvas.
			PlateSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
			PlateSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			PlateSlot->SetPosition(FVector2D(-28.0f, 28.0f));
			PlateSlot->SetAutoSize(true);
		}

		UTextBlock* Line = MakeText(Tree, Roboto, TEXT("TrackerText"),
			TEXT("Квест: Шкуры волков — Шкура волка 1/3"),
			FLinearColor(1.0f, 0.85f, 0.3f, 1.0f), 16, TEXT("Regular")); // QuestTrackerColor
		Line->bIsVariable = true;
		Plate->SetContent(Line);
		return true;
	}

	// Спайк-ассет: плашка «E — подобрать» (низ-центр). Оставлен для копий без ассета.
	bool BuildInteractPrompt(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptPlate"));
		Plate->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		Plate->SetPadding(FMargin(18.0f, 8.0f));
		Plate->SetHorizontalAlignment(HAlign_Center);
		Plate->SetVerticalAlignment(VAlign_Center);

		UTextBlock* Prompt = MakeText(Tree, Roboto, TEXT("PromptText"), TEXT("E — подобрать"),
			FLinearColor::White, 18, TEXT("Regular"));
		Prompt->bIsVariable = true;
		Plate->SetContent(Prompt);

		if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
		{
			PlateSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
			PlateSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			PlateSlot->SetPosition(FVector2D(0.0f, -140.0f));
			PlateSlot->SetAutoSize(true);
		}
		return true;
	}

	// ======================================================================
	// Таблица ассетов
	// ======================================================================

	struct FWbpSpec
	{
		const TCHAR* PackageName;      // /Game/UI/WBP_...
		const TCHAR* AssetName;        // WBP_...
		const TCHAR* ParentClassPath;  // /Script/ContrarySurvivor....
		bool (*Build)(UWidgetTree*);   // наполнение дерева
		std::initializer_list<const TCHAR*> ExpectedCubes; // контракт BindWidgetOptional
	};

	// Порядок важен: WBP_InventoryRow ДО WBP_Inventory (инвентарю назначается его класс).
	// Ассетов Рината (WBP_ShopScreenWiget/ShopRow/Dialog/PlayerStats) здесь НЕТ намеренно.
	const FWbpSpec GAssets[] =
	{
		{ TEXT("/Game/UI/WBP_TouchControls"), TEXT("WBP_TouchControls"),
			TEXT("/Script/ContrarySurvivor.TouchControlsWidget"), &BuildTouchControls,
			{ TEXT("StickBase"), TEXT("StickThumb"),
			  TEXT("FireButton"), TEXT("FireText"), TEXT("ReloadButton"), TEXT("ReloadText"),
			  TEXT("InteractButton"), TEXT("InteractText"), TEXT("SprintButton"), TEXT("SprintText"),
			  TEXT("WeaponButton"), TEXT("WeaponText"), TEXT("InventoryButton"), TEXT("InventoryText"),
			  TEXT("PauseButton"), TEXT("PauseText") } },
		{ TEXT("/Game/UI/WBP_InventoryRow"), TEXT("WBP_InventoryRow"),
			TEXT("/Script/ContrarySurvivor.InventoryRowWidget"), &BuildInventoryRow,
			{ TEXT("NameText"), TEXT("UseButton"), TEXT("UseText"), TEXT("DropButton") } },
		{ TEXT("/Game/UI/WBP_Inventory"), TEXT("WBP_Inventory"),
			TEXT("/Script/ContrarySurvivor.InventoryScreenWidget"), &BuildInventory,
			{ TEXT("StatsText"), TEXT("HeadSlotButton"), TEXT("HeadSlotText"), TEXT("HeadSlotIcon"),
			  TEXT("TorsoSlotButton"), TEXT("TorsoSlotText"), TEXT("TorsoSlotIcon"),
			  TEXT("LegsSlotButton"), TEXT("LegsSlotText"), TEXT("LegsSlotIcon"),
			  TEXT("ProtectionText"), TEXT("WeaponText"), TEXT("BackpackList"), TEXT("CloseButton") } },
		{ TEXT("/Game/UI/WBP_Death"), TEXT("WBP_Death"),
			TEXT("/Script/ContrarySurvivor.DeathScreenWidget"), &BuildDeath,
			{ TEXT("LifetimeText"), TEXT("KillerText"), TEXT("MoneyText"), TEXT("QuestsText"),
			  TEXT("KillsText"), TEXT("MoneyLossText"), TEXT("RespawnButton") } },
		{ TEXT("/Game/UI/WBP_QuestTracker"), TEXT("WBP_QuestTracker"),
			TEXT("/Script/ContrarySurvivor.QuestTrackerWidget"), &BuildQuestTracker,
			{ TEXT("TrackerText") } },
		{ TEXT("/Game/UI/WBP_InteractPrompt"), TEXT("WBP_InteractPrompt"),
			TEXT("/Script/ContrarySurvivor.InteractPromptWidget"), &BuildInteractPrompt,
			{ TEXT("PromptText") } },
	};

	FString ObjectPathOf(const FWbpSpec& Spec)
	{
		return FString::Printf(TEXT("%s.%s"), Spec.PackageName, Spec.AssetName);
	}

	// Генерация одного ассета по спеке. 0 — успех, 1 — ошибка.
	int32 GenerateOne(const FWbpSpec& Spec)
	{
		UClass* ParentClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, Spec.ParentClassPath);
		if (!ParentClass)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("%s: родительский класс %s не найден."),
				Spec.AssetName, Spec.ParentClassPath);
			return 1;
		}

		UPackage* Package = CreatePackage(Spec.PackageName);
		UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, Spec.AssetName, BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("%s: CreateBlueprint не создал Widget Blueprint."), Spec.AssetName);
			return 1;
		}

		if (!Spec.Build(WBP->WidgetTree))
		{
			return 1;
		}

		FKismetEditorUtilities::CompileBlueprint(WBP);
		if (WBP->Status == BS_Error)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("%s: Blueprint скомпилировался с ошибками — не сохраняю."), Spec.AssetName);
			return 1;
		}

		// WBP_Inventory: строкой рюкзака назначаем сгенерированный WBP_InventoryRow — без этого
		// список пуст (guide требовал ручного шага, генератор делает его сам; просьба лида).
		if (FCString::Strcmp(Spec.AssetName, TEXT("WBP_Inventory")) == 0)
		{
			UClass* RowClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr,
				TEXT("/Game/UI/WBP_InventoryRow.WBP_InventoryRow_C"));
			UInventoryScreenWidget* CDO = WBP->GeneratedClass
				? Cast<UInventoryScreenWidget>(WBP->GeneratedClass->GetDefaultObject())
				: nullptr;
			if (RowClass && CDO)
			{
				CDO->RowWidgetClass = RowClass;
				UE_LOG(LogGenerateWbp, Display, TEXT("WBP_Inventory: Row Widget Class = %s."), *RowClass->GetName());
			}
			else
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("WBP_Inventory: Row Widget Class НЕ назначен (класс строки=%d, CDO=%d) — назначить в редакторе."),
					RowClass ? 1 : 0, CDO ? 1 : 0);
			}
		}

		WBP->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(WBP);

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Spec.PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, WBP, *Filename, SaveArgs))
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("%s: SavePackage не сохранил %s."), Spec.AssetName, *Filename);
			return 1;
		}

		UE_LOG(LogGenerateWbp, Display, TEXT("OK: %s создан (родитель %s), файл %s."),
			Spec.PackageName, *ParentClass->GetPathName(), *Filename);
		return 0;
	}
}

int32 UGenerateWbpCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Switches.Contains(TEXT("verify")))
	{
		return VerifyAll();
	}
	return GenerateAll(Switches.Contains(TEXT("force")));
}

int32 UGenerateWbpCommandlet::GenerateAll(bool bForce)
{
	int32 FailCount = 0;
	int32 CreatedCount = 0;
	for (const FWbpSpec& Spec : GAssets)
	{
		// Существующий ассет (в т.ч. правленный Ринатом) без -force не трогаем.
		if (!bForce && FPackageName::DoesPackageExist(Spec.PackageName))
		{
			UE_LOG(LogGenerateWbp, Display, TEXT("SKIP: %s уже существует (перезапись только с -force)."),
				Spec.PackageName);
			continue;
		}
		if (GenerateOne(Spec) == 0)
		{
			++CreatedCount;
		}
		else
		{
			++FailCount;
		}
	}
	UE_LOG(LogGenerateWbp, Display, TEXT("Итог генерации: создано %d, ошибок %d."), CreatedCount, FailCount);
	return FailCount == 0 ? 0 : 1;
}

int32 UGenerateWbpCommandlet::VerifyAll()
{
	int32 FailCount = 0;
	for (const FWbpSpec& Spec : GAssets)
	{
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPathOf(Spec));
		if (!WBP)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: %s не загрузился."), *ObjectPathOf(Spec));
			++FailCount;
			continue;
		}

		UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY %s: родитель = %s, generated = %s"),
			Spec.AssetName,
			WBP->ParentClass ? *WBP->ParentClass->GetPathName() : TEXT("(null)"),
			WBP->GeneratedClass ? *WBP->GeneratedClass->GetPathName() : TEXT("(null)"));
		if (WBP->WidgetTree)
		{
			DumpWidgetTree(WBP->WidgetTree->RootWidget, 1);
		}

		bool bOk = WBP->ParentClass && WBP->ParentClass->GetPathName() == Spec.ParentClassPath;
		if (!bOk)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: %s — родитель не совпал."), Spec.AssetName);
		}
		for (const TCHAR* Cube : Spec.ExpectedCubes)
		{
			if (!WBP->WidgetTree || !WBP->WidgetTree->FindWidget(FName(Cube)))
			{
				UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: %s — кубик %s не найден."), Spec.AssetName, Cube);
				bOk = false;
			}
		}

		// Дополнительный контракт инвентаря: класс строки рюкзака назначен.
		if (bOk && FCString::Strcmp(Spec.AssetName, TEXT("WBP_Inventory")) == 0)
		{
			const UInventoryScreenWidget* CDO = WBP->GeneratedClass
				? Cast<UInventoryScreenWidget>(WBP->GeneratedClass->GetDefaultObject())
				: nullptr;
			if (CDO && CDO->RowWidgetClass)
			{
				UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY WBP_Inventory: Row Widget Class = %s."),
					*CDO->RowWidgetClass->GetName());
			}
			else
			{
				UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: WBP_Inventory — Row Widget Class пуст."));
				bOk = false;
			}
		}

		if (bOk)
		{
			UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY OK: %s — все %d кубиков на месте."),
				Spec.AssetName, static_cast<int32>(Spec.ExpectedCubes.size()));
		}
		else
		{
			++FailCount;
		}
	}
	UE_LOG(LogGenerateWbp, Display, TEXT("Итог проверки: ошибок %d."), FailCount);
	return FailCount == 0 ? 0 : 1;
}

void UGenerateWbpCommandlet::DumpWidgetTree(UWidget* Widget, int32 Depth)
{
	if (!Widget)
	{
		UE_LOG(LogGenerateWbp, Warning, TEXT("VERIFY: дерево пустое (RootWidget = null)."));
		return;
	}
	UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY: %s%s : %s"),
		*FString::ChrN(Depth * 2, TEXT(' ')), *Widget->GetName(), *Widget->GetClass()->GetName());
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			DumpWidgetTree(Panel->GetChildAt(Index), Depth + 1);
		}
	}
}
