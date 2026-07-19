// Fill out your copyright notice in the Description page of Project Settings.

#include "GenerateWbpCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h" // LoadObject<UTexture2D> в режиме дополнения (в Image.h только объявление)
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
#include "ContrarySurvivor/UI/ShopScreenWidget.h"
#include "ContrarySurvivor/UI/ShopRowWidget.h"      // полный тип для TSubclassOf-присваивания

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

	// Переводимая надпись (ADR-050): текст кладётся в ассет как есть, без снятия культуры.
	// Перегрузка не спорит с FString-версией ниже: у FText нет неявного конструктора из
	// строкового литерала, поэтому TEXT("...") по-прежнему уходит в FString-вариант.
	UTextBlock* MakeText(UWidgetTree* Tree, UObject* Roboto, const FName& Name,
		const FText& Text, const FLinearColor& Color, int32 Size, const TCHAR* Typeface)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		Block->SetFont(FSlateFontInfo(Roboto, Size, FName(Typeface)));
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
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

	// Тень текста (аналог DrawShadowedText Canvas-пути) — для надписей ПОВЕРХ игрового
	// мира (постоянные панели), где фон произвольный и без тени текст пропадает.
	void ApplyTextShadow(UTextBlock* Block)
	{
		Block->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
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

		// Иконка текущего оружия — над кнопкой ОРУЖИЕ, 48x48, в ассете Collapsed
		// (текстуру и показ ставит код игры — UTouchControlsWidget::UpdateWeaponIcon).
		UImage* WeaponIcon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WeaponIconImage"));
		WeaponIcon->SetVisibility(ESlateVisibility::Collapsed);
		WeaponIcon->bIsVariable = true;
		if (UCanvasPanelSlot* IconSlot = Root->AddChildToCanvas(WeaponIcon))
		{
			IconSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
			IconSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			IconSlot->SetPosition(FVector2D(-D.Weapon.Margin.X,
				-D.Weapon.Margin.Y - D.Weapon.Radius - 34.0f));
			IconSlot->SetSize(FVector2D(48.0f, 48.0f));
		}
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
		UTextBlock* UseCaption = MakeText(Tree, Roboto, TEXT("UseText"), TEXT("Использовать"),
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

		// Статичная подпись слота («Голова») — текст Рината, код не трогает.
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

		// Заголовок (статичный текст Рината) + строка статов.
		PanelBox->AddChildToVerticalBox(MakeText(Tree, Roboto, TEXT("HeaderText"),
			TEXT("Инвентарь"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 22, TEXT("Bold")));

		// Раньше все три стата были слеплены в ОДИН кубик. Теперь у каждого своя пара
		// «статичная подпись + значение»: подписи не переменные, код их не трогает (ADR-050).
		const FLinearColor StatsColor(1.0f, 0.85f, 0.2f, 1.0f); // UIMoneyColor
		UHorizontalBox* StatsRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("StatsRow"));
		if (UVerticalBoxSlot* StatsSlot = PanelBox->AddChildToVerticalBox(StatsRow))
		{
			StatsSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 12.0f));
		}

		auto AddStat = [&](const TCHAR* LabelName, const TCHAR* Caption,
			const TCHAR* ValueName, const TCHAR* ValueSample, float LeftPad)
		{
			if (UHorizontalBoxSlot* LabelSlot = StatsRow->AddChildToHorizontalBox(
				MakeText(Tree, Roboto, FName(LabelName), Caption, StatsColor, 16, TEXT("Regular"))))
			{
				LabelSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
			}
			UTextBlock* Value = MakeText(Tree, Roboto, FName(ValueName), ValueSample,
				StatsColor, 16, TEXT("Regular"));
			Value->bIsVariable = true;
			if (UHorizontalBoxSlot* ValueSlot = StatsRow->AddChildToHorizontalBox(Value))
			{
				ValueSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
			}
		};
		AddStat(TEXT("InvMoneyLabel"), TEXT("Монеты"), TEXT("InvMoneyText"), TEXT("0"), 0.0f);
		AddStat(TEXT("InvHungerLabel"), TEXT("Голод"), TEXT("InvHungerText"), TEXT("100 из 100"), 28.0f);
		AddStat(TEXT("InvThirstLabel"), TEXT("Жажда"), TEXT("InvThirstText"), TEXT("100 из 100"), 28.0f);

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

		AddArmorSlotButton(Tree, EquipColumn, Roboto, TEXT("Голова"),
			TEXT("HeadSlotButton"), TEXT("HeadSlotIcon"), TEXT("HeadSlotText"));
		AddArmorSlotButton(Tree, EquipColumn, Roboto, TEXT("Торс"),
			TEXT("TorsoSlotButton"), TEXT("TorsoSlotIcon"), TEXT("TorsoSlotText"));
		AddArmorSlotButton(Tree, EquipColumn, Roboto, TEXT("Штаны"),
			TEXT("LegsSlotButton"), TEXT("LegsSlotIcon"), TEXT("LegsSlotText"));

		// Защита и оружие — тоже пары «статичная подпись + значение» (ADR-050).
		auto AddEquipLine = [&](const TCHAR* LabelName, const TCHAR* Caption,
			const TCHAR* ValueName, const TCHAR* ValueSample, const FLinearColor& ValueColor,
			float TopPad)
		{
			UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(), FName(*(FString(ValueName) + TEXT("Row"))));
			Row->AddChildToHorizontalBox(
				MakeText(Tree, Roboto, FName(LabelName), Caption, FLinearColor::White, 15, TEXT("Regular")));

			UTextBlock* Value = MakeText(Tree, Roboto, FName(ValueName), ValueSample,
				ValueColor, 15, TEXT("Regular"));
			Value->bIsVariable = true;
			if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value))
			{
				ValueSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
			}

			if (UVerticalBoxSlot* RowSlot = EquipColumn->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.0f, TopPad, 0.0f, 2.0f));
			}
		};
		// Цвет значения защиты — нейтральный: при нулевой защите зелёный читался бы как
		// «всё хорошо», хотя брони нет (ADR-049). Итоговый цвет всё равно за Ринатом.
		AddEquipLine(TEXT("ProtectionLabel"), TEXT("Защита"), TEXT("ProtectionText"), TEXT("0%"),
			FLinearColor(0.85f, 0.85f, 0.85f, 1.0f), 10.0f);
		AddEquipLine(TEXT("WeaponLabel"), TEXT("Оружие"), TEXT("WeaponText"), TEXT("Пусто"),
			FLinearColor::White, 2.0f);

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

		// Каждая строка статистики — пара «статичная подпись + значение» в одном ряду:
		// подпись принадлежит Ринату, код пишет только число (ADR-050).
		const FLinearColor StatColor(0.95f, 0.95f, 0.95f, 1.0f);
		auto AddStatLine = [&](const TCHAR* LabelName, const TCHAR* Caption,
			const TCHAR* ValueName, const TCHAR* ValueSample)
		{
			UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(), FName(*(FString(ValueName) + TEXT("Row"))));
			Row->AddChildToHorizontalBox(
				MakeText(Tree, Roboto, FName(LabelName), Caption, StatColor, 22, TEXT("Regular")));

			UTextBlock* Value = MakeText(Tree, Roboto, FName(ValueName), ValueSample,
				StatColor, 22, TEXT("Regular"));
			Value->bIsVariable = true;
			if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value))
			{
				ValueSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			}

			if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
			{
				RowSlot->SetHorizontalAlignment(HAlign_Center);
				RowSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 3.0f));
			}
		};
		AddStatLine(TEXT("LifetimeLabel"), TEXT("Прожито"), TEXT("LifetimeText"), TEXT("00:00"));
		AddStatLine(TEXT("KillerLabel"), TEXT("Убийца"), TEXT("KillerText"), TEXT("—"));
		AddStatLine(TEXT("MoneyLabel"), TEXT("Монеты"), TEXT("MoneyText"), TEXT("0"));
		AddStatLine(TEXT("QuestsLabel"), TEXT("Квестов выполнено"), TEXT("QuestsText"), TEXT("0"));
		AddStatLine(TEXT("KillsLabel"), TEXT("Врагов убито"), TEXT("KillsText"), TEXT("0"));

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
			TEXT("Квест: Собрать шкуры волков 1 из 3"),
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
	// Магазин: WBP_ShopRow + WBP_Shop (геометрия и цвета — Canvas DrawShop/DrawShopSlider)
	// ======================================================================

	// Одна строка списков магазина: плашка -> имя + цена + кнопка действия (Buy/Sell).
	bool BuildShopRow(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		USizeBox* RowSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RowSize"));
		RowSize->SetMinDesiredHeight(48.0f); // комфортная пальцу высота (guide 40-60; как WBP_InventoryRow)
		Tree->RootWidget = RowSize;

		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RowPlate"));
		Plate->SetBrush(MakeRoundedBrush(FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), 4.0f)); // InvSlotColor
		Plate->SetPadding(FMargin(8.0f, 4.0f));
		RowSize->SetContent(Plate);

		UHorizontalBox* RowBox = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RowBox"));
		Plate->SetContent(RowBox);

		UTextBlock* Name = MakeText(Tree, Roboto, TEXT("NameText"), TEXT("Товар"),
			FLinearColor::White, 15, TEXT("Regular"));
		Name->bIsVariable = true;
		if (UHorizontalBoxSlot* NameSlot = RowBox->AddChildToHorizontalBox(Name))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Цена — золотой акцент (UIMoneyColor); кубик необязательный, но кладём:
		// без него код дописывает цену к имени.
		UTextBlock* Price = MakeText(Tree, Roboto, TEXT("PriceText"), TEXT("0"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 15, TEXT("Regular"));
		Price->bIsVariable = true;
		if (UHorizontalBoxSlot* PriceSlot = RowBox->AddChildToHorizontalBox(Price))
		{
			PriceSlot->SetVerticalAlignment(VAlign_Center);
			PriceSlot->SetPadding(FMargin(8.0f, 0.0f));
		}

		// «Не хватает монет» — код показывает эту строку только в недоступных товарах,
		// в остальных прячет (ADR-049: одним цветом кнопки часть игроков не считывает).
		// В ассете сразу Collapsed — видимость переключает код.
		UTextBlock* NoMoney = MakeText(Tree, Roboto, TEXT("NoMoneyText"), TEXT("Не хватает монет"),
			FLinearColor(0.85f, 0.35f, 0.3f, 1.0f), 13, TEXT("Regular"));
		NoMoney->SetVisibility(ESlateVisibility::Collapsed);
		NoMoney->bIsVariable = true;
		if (UHorizontalBoxSlot* NoMoneySlot = RowBox->AddChildToHorizontalBox(NoMoney))
		{
			NoMoneySlot->SetVerticalAlignment(VAlign_Center);
			NoMoneySlot->SetPadding(FMargin(8.0f, 0.0f));
		}

		// Кнопка действия — зелёная, как доступные строки Canvas-пути (InvSlotFilledColor);
		// подпись ставит код («Купить»/«Продать»), недоступную кнопку код гасит сам.
		UButton* Action = MakeStyledButton(Tree, TEXT("ActionButton"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f));
		UTextBlock* ActionCaption = MakeText(Tree, Roboto, TEXT("ActionText"), TEXT("Купить"),
			FLinearColor::White, 13, TEXT("Regular"));
		ActionCaption->bIsVariable = true;
		Action->SetContent(ActionCaption);
		if (UHorizontalBoxSlot* ActionSlot = RowBox->AddChildToHorizontalBox(Action))
		{
			ActionSlot->SetVerticalAlignment(VAlign_Center);
			ActionSlot->SetPadding(FMargin(6.0f, 2.0f));
		}
		return true;
	}

	// Экран магазина: затемнение -> центральная панель (шапка, деньги, две колонки списков)
	// + панель количества ПОВЕРХ (код показывает её только на время транзакции).
	bool BuildShop(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Затемнение (InvDimColor); Visible — модалка, клики в мир не проходят.
		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Dim->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
		}

		// Центральная панель 960x600 с золотой рамкой — общая геометрия панелей (как инвентарь).
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

		// Шапка: заголовок (статичный текст Рината, литерал Canvas) + кнопка Close 90x28.
		UHorizontalBox* HeaderRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
		PanelBox->AddChildToVerticalBox(HeaderRow);

		UTextBlock* Header = MakeText(Tree, Roboto, TEXT("HeaderText"),
			TEXT("Торговец"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 22, TEXT("Bold"));
		if (UHorizontalBoxSlot* HeaderSlot = HeaderRow->AddChildToHorizontalBox(Header))
		{
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HeaderSlot->SetVerticalAlignment(VAlign_Center);
		}

		UButton* Close = MakeStyledButton(Tree, TEXT("CloseButton"),
			FLinearColor(0.5f, 0.12f, 0.12f, 1.0f), FLinearColor(0.62f, 0.17f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f)); // InvDropColor
		Close->SetContent(MakeText(Tree, Roboto, TEXT("CloseLabel"), TEXT("Закрыть"),
			FLinearColor::White, 14, TEXT("Regular")));
		USizeBox* CloseSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CloseSize"));
		CloseSize->SetWidthOverride(90.0f);  // ShopCloseButtonWidth
		CloseSize->SetHeightOverride(28.0f); // ShopCloseButtonHeight
		CloseSize->SetContent(Close);
		if (UHorizontalBoxSlot* CloseSlot = HeaderRow->AddChildToHorizontalBox(CloseSize))
		{
			CloseSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Деньги игрока: статичная подпись «Монеты» (код её НЕ трогает) + значение, которое
		// код обновляет каждый кадр (ADR-050 — подпись и значение разные кубики).
		const FLinearColor ShopMoneyColor(1.0f, 0.85f, 0.2f, 1.0f);
		UHorizontalBox* MoneyRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("MoneyRow"));
		if (UVerticalBoxSlot* MoneySlot = PanelBox->AddChildToVerticalBox(MoneyRow))
		{
			MoneySlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 12.0f));
		}

		MoneyRow->AddChildToHorizontalBox(MakeText(Tree, Roboto, TEXT("MoneyLabel"),
			TEXT("Монеты"), ShopMoneyColor, 16, TEXT("Regular")));

		UTextBlock* Money = MakeText(Tree, Roboto, TEXT("MoneyText"), TEXT("0"),
			ShopMoneyColor, 16, TEXT("Regular"));
		Money->bIsVariable = true;
		if (UHorizontalBoxSlot* MoneyValueSlot = MoneyRow->AddChildToHorizontalBox(Money))
		{
			MoneyValueSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		}

		// Колонки: слева каталог (0.52 — ShopLeftColumnFrac), справа рюкзак на продажу.
		UHorizontalBox* Columns = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ColumnsBox"));
		if (UVerticalBoxSlot* ColumnsSlot = PanelBox->AddChildToVerticalBox(Columns))
		{
			ColumnsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UVerticalBox* BuyColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuyBox"));
		if (UHorizontalBoxSlot* BuySlot = Columns->AddChildToHorizontalBox(BuyColumn))
		{
			FSlateChildSize LeftSize(ESlateSizeRule::Fill);
			LeftSize.Value = 0.52f;
			BuySlot->SetSize(LeftSize);
			BuySlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}
		BuyColumn->AddChildToVerticalBox(MakeText(Tree, Roboto, TEXT("BuyHeaderText"),
			TEXT("Товары торговца"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")));
		UScrollBox* Buy = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BuyList"));
		Buy->bIsVariable = true;
		if (UVerticalBoxSlot* BuyListSlot = BuyColumn->AddChildToVerticalBox(Buy))
		{
			BuyListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BuyListSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		UVerticalBox* SellColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SellBox"));
		if (UHorizontalBoxSlot* SellSlot = Columns->AddChildToHorizontalBox(SellColumn))
		{
			FSlateChildSize RightSize(ESlateSizeRule::Fill);
			RightSize.Value = 0.48f;
			SellSlot->SetSize(RightSize);
		}
		SellColumn->AddChildToVerticalBox(MakeText(Tree, Roboto, TEXT("SellHeaderText"),
			TEXT("Рюкзак"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")));
		UScrollBox* Sell = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SellList"));
		Sell->bIsVariable = true;
		if (UVerticalBoxSlot* SellListSlot = SellColumn->AddChildToVerticalBox(Sell))
		{
			SellListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SellListSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		// Панель количества 600x260 (SliderPanelMaxWidth/Height) — последний ребёнок канвы,
		// рисуется поверх; в ассете сразу Collapsed (Visible/Collapsed переключает код).
		UBorder* SliderPanel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SliderPanel"));
		SliderPanel->SetBrush(MakeRoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.95f), 6.0f,
			FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
		SliderPanel->SetPadding(FMargin(18.0f)); // SliderPanelPadding
		SliderPanel->SetVisibility(ESlateVisibility::Collapsed);
		SliderPanel->bIsVariable = true;
		if (UCanvasPanelSlot* SliderPanelSlot = Root->AddChildToCanvas(SliderPanel))
		{
			SliderPanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			SliderPanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			SliderPanelSlot->SetPosition(FVector2D::ZeroVector);
			SliderPanelSlot->SetSize(FVector2D(600.0f, 260.0f));
		}

		UVerticalBox* SliderBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SliderBox"));
		SliderPanel->SetContent(SliderBox);

		UTextBlock* SliderTitle = MakeText(Tree, Roboto, TEXT("SliderTitleText"), TEXT("Купить: —"),
			FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 20, TEXT("Bold"));
		SliderTitle->bIsVariable = true;
		SliderBox->AddChildToVerticalBox(SliderTitle);

		// Количество: статичная подпись + значение разными кубиками (ADR-050).
		const FLinearColor SliderQtyColor(1.0f, 0.97f, 0.7f, 1.0f);
		UHorizontalBox* QtyTextRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("QtyTextRow"));
		if (UVerticalBoxSlot* QtyTextSlot = SliderBox->AddChildToVerticalBox(QtyTextRow))
		{
			QtyTextSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		QtyTextRow->AddChildToHorizontalBox(MakeText(Tree, Roboto, TEXT("SliderQtyLabel"),
			TEXT("Количество"), SliderQtyColor, 18, TEXT("Regular")));

		UTextBlock* SliderQty = MakeText(Tree, Roboto, TEXT("SliderQtyText"), TEXT("1 из 1"),
			SliderQtyColor, 18, TEXT("Regular"));
		SliderQty->bIsVariable = true;
		if (UHorizontalBoxSlot* QtyValueSlot = QtyTextRow->AddChildToHorizontalBox(SliderQty))
		{
			QtyValueSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		}

		// Пересчёт пачек в патроны: показывается ТОЛЬКО при покупке патронов. Прячется
		// ЦЕЛИКОМ контейнер SliderQtyAmmoRow — вместе с подписью, иначе она висела бы при
		// покупке аптечки (ловушка скрытия, ADR-050). В ассете сразу Collapsed.
		UHorizontalBox* AmmoRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("SliderQtyAmmoRow"));
		AmmoRow->SetVisibility(ESlateVisibility::Collapsed);
		AmmoRow->bIsVariable = true;
		if (UVerticalBoxSlot* AmmoRowSlot = SliderBox->AddChildToVerticalBox(AmmoRow))
		{
			AmmoRowSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}

		UTextBlock* SliderQtyAmmo = MakeText(Tree, Roboto, TEXT("SliderQtyAmmoText"),
			TEXT("всего 30 патронов"), SliderQtyColor, 16, TEXT("Regular"));
		SliderQtyAmmo->bIsVariable = true;
		AmmoRow->AddChildToHorizontalBox(SliderQtyAmmo);

		// Ползунок: диапазон/шаг выставляет код при каждой транзакции — тут только кубик.
		USlider* Qty = Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("QtySlider"));
		Qty->bIsVariable = true;
		if (UVerticalBoxSlot* QtySliderSlot = SliderBox->AddChildToVerticalBox(Qty))
		{
			QtySliderSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		}

		// Ряд [-] [+] и живой итог справа.
		UHorizontalBox* QtyRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QtyRow"));
		if (UVerticalBoxSlot* QtyRowSlot = SliderBox->AddChildToVerticalBox(QtyRow))
		{
			QtyRowSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
		}

		auto AddSmallButton = [&](const TCHAR* ButtonName, const TCHAR* LabelName,
			const TCHAR* Caption, float LeftPad)
		{
			UButton* Small = MakeStyledButton(Tree, FName(ButtonName),
				FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
				FLinearColor(0.25f, 0.27f, 0.33f, 1.0f)); // InvSlotColor + подсветки
			Small->SetContent(MakeText(Tree, Roboto, FName(LabelName), Caption,
				FLinearColor::White, 15, TEXT("Bold")));
			USizeBox* SmallSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
				FName(*(FString(ButtonName) + TEXT("Size"))));
			SmallSize->SetWidthOverride(48.0f);  // SliderSmallButtonWidth
			SmallSize->SetHeightOverride(30.0f); // SliderSmallButtonHeight
			SmallSize->SetContent(Small);
			if (UHorizontalBoxSlot* SmallSlot = QtyRow->AddChildToHorizontalBox(SmallSize))
			{
				SmallSlot->SetVerticalAlignment(VAlign_Center);
				SmallSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
			}
		};
		AddSmallButton(TEXT("QtyMinusButton"), TEXT("QtyMinusLabel"), TEXT("-"), 0.0f);
		AddSmallButton(TEXT("QtyPlusButton"), TEXT("QtyPlusLabel"), TEXT("+"), 8.0f);

		UTextBlock* SliderTotal = MakeText(Tree, Roboto, TEXT("SliderTotalText"), TEXT("Итого: 0"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 19, TEXT("Regular")); // UIMoneyColor
		SliderTotal->bIsVariable = true;
		if (UHorizontalBoxSlot* TotalSlot = QtyRow->AddChildToHorizontalBox(SliderTotal))
		{
			TotalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TotalSlot->SetVerticalAlignment(VAlign_Center);
			TotalSlot->SetPadding(FMargin(24.0f, 0.0f, 0.0f, 0.0f));
		}

		// Ряд подтверждения (Cancel красная, Confirm зелёная) — правый низ панели.
		UHorizontalBox* ActionRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionRow"));
		if (UVerticalBoxSlot* ActionRowSlot = SliderBox->AddChildToVerticalBox(ActionRow))
		{
			ActionRowSlot->SetHorizontalAlignment(HAlign_Right);
			ActionRowSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
		}

		auto AddBigButton = [&](const TCHAR* ButtonName, const TCHAR* LabelName, const TCHAR* Caption,
			const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed, float LeftPad)
		{
			UButton* Big = MakeStyledButton(Tree, FName(ButtonName), Normal, Hovered, Pressed);
			Big->SetContent(MakeText(Tree, Roboto, FName(LabelName), Caption,
				FLinearColor::White, 15, TEXT("Regular")));
			USizeBox* BigSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
				FName(*(FString(ButtonName) + TEXT("Size"))));
			BigSize->SetWidthOverride(120.0f);  // SliderBigButtonWidth
			BigSize->SetHeightOverride(34.0f);  // SliderBigButtonHeight
			BigSize->SetContent(Big);
			if (UHorizontalBoxSlot* BigSlot = ActionRow->AddChildToHorizontalBox(BigSize))
			{
				BigSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
			}
		};
		AddBigButton(TEXT("SliderCancelButton"), TEXT("SliderCancelLabel"), TEXT("Отмена"),
			FLinearColor(0.5f, 0.12f, 0.12f, 1.0f), FLinearColor(0.62f, 0.17f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f), 0.0f);
		AddBigButton(TEXT("SliderConfirmButton"), TEXT("SliderConfirmLabel"), TEXT("Подтвердить"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f), 10.0f);
		return true;
	}

	// ======================================================================
	// Диалог старосты: WBP_Dialog (вид = Canvas DrawDialog — нижняя панель новеллы)
	// ======================================================================

	bool BuildDialog(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f)); // InvDimColor
		Dim->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
		}

		// Панель низ-центр: ширина 900 (DialogPanelMaxWidth), пол высоты 280 (DialogMinPanelHeight),
		// отступ от низа 40 (DialogBottomMargin); высота растёт от длины реплики (AutoSize + wrap).
		USizeBox* PanelSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
		PanelSize->SetWidthOverride(900.0f);
		PanelSize->SetMinDesiredHeight(280.0f);
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelSize))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			PanelSlot->SetPosition(FVector2D(0.0f, -40.0f));
			PanelSlot->SetAutoSize(true);
		}

		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
		Panel->SetBrush(MakeRoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.95f), 6.0f,
			FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
		Panel->SetPadding(FMargin(18.0f)); // DialogPadding
		PanelSize->SetContent(Panel);

		// Имя C++-переменной НЕ DialogBox: это макрос winuser.h (DialogBox -> DialogBoxA),
		// утечка windows.h в editor-модуле дала бы загадочную ошибку. Имя виджета — прежнее.
		UVerticalBox* DialogColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogBox"));
		Panel->SetContent(DialogColumn);

		// Имя NPC — голубой DialogNameColor (текст ставит код с поля старосты).
		UTextBlock* NpcName = MakeText(Tree, Roboto, TEXT("NPCNameText"), TEXT("СТАРОСТА"),
			FLinearColor(0.65f, 0.88f, 1.0f, 1.0f), 22, TEXT("Bold"));
		NpcName->bIsVariable = true;
		DialogColumn->AddChildToVerticalBox(NpcName);

		// Реплика: перенос строк обязателен (guide) — длинные квестовые описания многострочные.
		UTextBlock* Replica = MakeText(Tree, Roboto, TEXT("ReplicaText"), TEXT("Реплика старосты."),
			FLinearColor::White, 16, TEXT("Regular"));
		Replica->SetAutoWrapText(true);
		Replica->bIsVariable = true;
		if (UVerticalBoxSlot* ReplicaSlot = DialogColumn->AddChildToVerticalBox(Replica))
		{
			ReplicaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ReplicaSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 12.0f));
		}

		// Ряд ответов: все четыре кнопки рядом — лишние по состоянию квеста код прячет сам.
		UHorizontalBox* ButtonsRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonsRow"));
		DialogColumn->AddChildToVerticalBox(ButtonsRow);

		auto AddAnswer = [&](UButton* Button, float Width, float LeftPad)
		{
			USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
				FName(*(Button->GetName() + TEXT("Size"))));
			Size->SetWidthOverride(Width);
			Size->SetHeightOverride(40.0f); // DialogButtonHeight
			Size->SetContent(Button);
			if (UHorizontalBoxSlot* AnswerSlot = ButtonsRow->AddChildToHorizontalBox(Size))
			{
				AnswerSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
			}
		};

		// [Принять] — зелёная (InvSlotFilledColor), подпись статичная (текст Рината).
		UButton* Accept = MakeStyledButton(Tree, TEXT("AcceptButton"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f));
		Accept->SetContent(MakeText(Tree, Roboto, TEXT("AcceptLabel"), TEXT("Взяться за дело"),
			FLinearColor::White, 15, TEXT("Regular")));
		AddAnswer(Accept, 200.0f, 0.0f); // DialogButtonWidth

		// [Отказаться] — красная (InvDropColor).
		UButton* Decline = MakeStyledButton(Tree, TEXT("DeclineButton"),
			FLinearColor(0.5f, 0.12f, 0.12f, 1.0f), FLinearColor(0.62f, 0.17f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f));
		Decline->SetContent(MakeText(Tree, Roboto, TEXT("DeclineLabel"), TEXT("Отказаться"),
			FLinearColor::White, 15, TEXT("Regular")));
		AddAnswer(Decline, 200.0f, 14.0f);

		// [Сдать (+N)] — зелёная, ШИРЕ (240 — DialogTurnInButtonWidth); подпись ставит КОД
		// через кубик TurnInText (в ней сумма награды).
		UButton* TurnIn = MakeStyledButton(Tree, TEXT("TurnInButton"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f));
		UTextBlock* TurnInCaption = MakeText(Tree, Roboto, TEXT("TurnInText"), TEXT("Сдать (+0)"),
			FLinearColor::White, 15, TEXT("Regular"));
		TurnInCaption->bIsVariable = true;
		TurnIn->SetContent(TurnInCaption);
		AddAnswer(TurnIn, 240.0f, 14.0f);

		// [Закрыть] — серая (InvSlotColor).
		UButton* CloseBtn = MakeStyledButton(Tree, TEXT("CloseButton"),
			FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
			FLinearColor(0.25f, 0.27f, 0.33f, 1.0f));
		CloseBtn->SetContent(MakeText(Tree, Roboto, TEXT("CloseLabel"), TEXT("Закрыть"),
			FLinearColor::White, 15, TEXT("Regular")));
		AddAnswer(CloseBtn, 200.0f, 14.0f);
		return true;
	}

	// ======================================================================
	// Постоянная панель статов: WBP_PlayerStats (вид = Canvas DrawPlayerStats, верх-лево)
	// ======================================================================

	// Полоска стата: SizeBox-габарит -> ProgressBar + текст поверх слева (Overlay).
	// Строка шкалы: полоска, поверх неё СТАТИЧНАЯ подпись слева («Здоровье») и ЗНАЧЕНИЕ
	// справа («80/100»). Подпись — не переменная: код её не биндит и не переписывает,
	// Ринат правит текст и стиль сам (ADR-050). Значение — кубик с точным именем.
	UOverlay* MakeStatBar(UWidgetTree* Tree, UObject* Roboto, const TCHAR* BarName,
		const TCHAR* ValueName, const FString& LabelCaption, const FString& ValueSample,
		const FLinearColor& FillColor, float Height)
	{
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),
			FName(*(FString(BarName) + TEXT("Overlay"))));

		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(BarName));
		FProgressBarStyle BarStyle;
		BarStyle.BackgroundImage = FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f)); // BackgroundColor HUD
		BarStyle.FillImage = FSlateColorBrush(FLinearColor::White); // итоговый цвет даёт FillColorAndOpacity
		BarStyle.EnableFillAnimation = false;
		Bar->SetWidgetStyle(BarStyle);
		Bar->SetFillColorAndOpacity(FillColor);
		Bar->SetPercent(1.0f); // заполнение ставит код каждый кадр
		Bar->bIsVariable = true;

		USizeBox* BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(FString(BarName) + TEXT("Size"))));
		BarSize->SetWidthOverride(320.0f); // PlayerHealthBarWidth (все бары одной ширины)
		BarSize->SetHeightOverride(Height);
		BarSize->SetContent(Bar);
		Overlay->AddChildToOverlay(BarSize);

		// Подпись — обычный Text, код его НЕ трогает (bIsVariable=false).
		UTextBlock* Label = MakeText(Tree, Roboto, FName(*(FString(BarName) + TEXT("Label"))),
			LabelCaption, FLinearColor::White, 14, TEXT("Regular"));
		ApplyTextShadow(Label);
		if (UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Left);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(8.0f, 0.0f));
		}

		// Значение — кубик кода (имя точное, см. umg-layout-guide.md).
		UTextBlock* Value = MakeText(Tree, Roboto, FName(ValueName), ValueSample,
			FLinearColor::White, 14, TEXT("Regular"));
		ApplyTextShadow(Value);
		Value->bIsVariable = true;
		if (UOverlaySlot* ValueSlot = Overlay->AddChildToOverlay(Value))
		{
			ValueSlot->SetHorizontalAlignment(HAlign_Right);
			ValueSlot->SetVerticalAlignment(VAlign_Center);
			ValueSlot->SetPadding(FMargin(8.0f, 0.0f));
		}
		return Overlay;
	}

	// Панель статов: столбец верх-лево (HP -> голод -> жажда -> патроны -> деньги).
	bool BuildPlayerStats(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatsBox"));
		// Панель живёт на экране всю игру: HitTestInvisible на столбе — тапы/клики сквозь
		// ВСЁ поддерево уходят в мир (иначе бары и плашка денег глотали бы касания).
		Column->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* ColSlot = Root->AddChildToCanvas(Column))
		{
			ColSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			ColSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			ColSlot->SetPosition(FVector2D(24.0f, 24.0f)); // PlayerHudMarginX/Y
			ColSlot->SetAutoSize(true);
		}

		Column->AddChildToVerticalBox(MakeStatBar(Tree, Roboto, TEXT("HealthBar"), TEXT("HealthText"),
			TEXT("Здоровье"), TEXT("100/100"),
			FLinearColor(0.85f, 0.1f, 0.1f, 0.95f), 28.0f)); // PlayerHealthFillColor, 320x28

		UOverlay* Hunger = MakeStatBar(Tree, Roboto, TEXT("HungerBar"), TEXT("HungerText"),
			TEXT("Голод"), TEXT("100"),
			FLinearColor(0.85f, 0.55f, 0.1f, 0.95f), 24.0f); // HungerColor, высота 24
		if (UVerticalBoxSlot* HungerSlot = Column->AddChildToVerticalBox(Hunger))
		{
			HungerSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		UOverlay* Thirst = MakeStatBar(Tree, Roboto, TEXT("ThirstBar"), TEXT("ThirstText"),
			TEXT("Жажда"), TEXT("100"),
			FLinearColor(0.15f, 0.55f, 0.9f, 0.95f), 24.0f); // ThirstColor
		if (UVerticalBoxSlot* ThirstSlot = Column->AddChildToVerticalBox(Thirst))
		{
			ThirstSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}

		// Патроны: код показывает строку только с огнестрелом в руках. Прячется ЦЕЛИКОМ
		// контейнер AmmoRow — вместе с подписью «Патроны», иначе подпись висела бы одна
		// при ноже в руках (ловушка скрытия, ADR-050). В ассете сразу Collapsed.
		const FLinearColor AmmoColor(0.95f, 0.95f, 0.95f, 1.0f);
		UHorizontalBox* AmmoRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("AmmoRow"));
		AmmoRow->SetVisibility(ESlateVisibility::Collapsed);
		AmmoRow->bIsVariable = true;

		UTextBlock* AmmoLabel = MakeText(Tree, Roboto, TEXT("AmmoLabel"), TEXT("Патроны"),
			AmmoColor, 14, TEXT("Regular"));
		ApplyTextShadow(AmmoLabel);
		AmmoRow->AddChildToHorizontalBox(AmmoLabel);

		UTextBlock* Ammo = MakeText(Tree, Roboto, TEXT("AmmoText"), TEXT("7 / 51"),
			AmmoColor, 14, TEXT("Regular"));
		ApplyTextShadow(Ammo);
		Ammo->bIsVariable = true;
		if (UHorizontalBoxSlot* AmmoValueSlot = AmmoRow->AddChildToHorizontalBox(Ammo))
		{
			AmmoValueSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		}

		if (UVerticalBoxSlot* AmmoSlot = Column->AddChildToVerticalBox(AmmoRow))
		{
			AmmoSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		// Деньги — золотые на тёмной плашке (MoneyPlateColor): подпись + значение.
		const FLinearColor MoneyColor(1.0f, 0.85f, 0.2f, 1.0f); // PlayerMoneyColor
		UBorder* MoneyPlate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MoneyPlate"));
		MoneyPlate->SetBrush(MakeRoundedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), 3.0f));
		MoneyPlate->SetPadding(FMargin(8.0f, 3.0f));

		UHorizontalBox* MoneyRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("MoneyRow"));

		UTextBlock* MoneyLabel = MakeText(Tree, Roboto, TEXT("MoneyLabel"), TEXT("Монеты"),
			MoneyColor, 15, TEXT("Regular"));
		ApplyTextShadow(MoneyLabel);
		MoneyRow->AddChildToHorizontalBox(MoneyLabel);

		UTextBlock* Money = MakeText(Tree, Roboto, TEXT("MoneyText"), TEXT("0"),
			MoneyColor, 15, TEXT("Regular"));
		ApplyTextShadow(Money);
		Money->bIsVariable = true;
		if (UHorizontalBoxSlot* MoneyValueSlot = MoneyRow->AddChildToHorizontalBox(Money))
		{
			MoneyValueSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		}
		MoneyPlate->SetContent(MoneyRow);
		if (UVerticalBoxSlot* MoneySlot = Column->AddChildToVerticalBox(MoneyPlate))
		{
			MoneySlot->SetHorizontalAlignment(HAlign_Left);
			MoneySlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
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

	// Порядок важен: WBP_InventoryRow ДО WBP_Inventory и WBP_ShopRow ДО WBP_Shop
	// (экрану назначается класс его строки). Пустые заготовки Рината
	// (WBP_ShopScreenWiget/ShopRow/Dialog/PlayerStats) он удалил сам 07-19 («ничего
	// не создал по итогу», коммит 2754045) — эти панели теперь тоже генерируем.
	const FWbpSpec GAssets[] =
	{
		{ TEXT("/Game/UI/WBP_TouchControls"), TEXT("WBP_TouchControls"),
			TEXT("/Script/ContrarySurvivor.TouchControlsWidget"), &BuildTouchControls,
			{ TEXT("StickBase"), TEXT("StickThumb"),
			  TEXT("FireButton"), TEXT("FireText"), TEXT("ReloadButton"), TEXT("ReloadText"),
			  TEXT("InteractButton"), TEXT("InteractText"), TEXT("SprintButton"), TEXT("SprintText"),
			  TEXT("WeaponButton"), TEXT("WeaponText"), TEXT("InventoryButton"), TEXT("InventoryText"),
			  TEXT("PauseButton"), TEXT("PauseText"), TEXT("WeaponIconImage") } },
		{ TEXT("/Game/UI/WBP_InventoryRow"), TEXT("WBP_InventoryRow"),
			TEXT("/Script/ContrarySurvivor.InventoryRowWidget"), &BuildInventoryRow,
			{ TEXT("NameText"), TEXT("UseButton"), TEXT("UseText"), TEXT("DropButton") } },
		{ TEXT("/Game/UI/WBP_Inventory"), TEXT("WBP_Inventory"),
			TEXT("/Script/ContrarySurvivor.InventoryScreenWidget"), &BuildInventory,
			{ TEXT("InvMoneyText"), TEXT("InvHungerText"), TEXT("InvThirstText"),
			  TEXT("HeadSlotButton"), TEXT("HeadSlotText"), TEXT("HeadSlotIcon"),
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
		{ TEXT("/Game/UI/WBP_ShopRow"), TEXT("WBP_ShopRow"),
			TEXT("/Script/ContrarySurvivor.ShopRowWidget"), &BuildShopRow,
			{ TEXT("NameText"), TEXT("PriceText"), TEXT("ActionButton"), TEXT("ActionText"),
			  TEXT("NoMoneyText") } },
		{ TEXT("/Game/UI/WBP_Shop"), TEXT("WBP_Shop"),
			TEXT("/Script/ContrarySurvivor.ShopScreenWidget"), &BuildShop,
			{ TEXT("MoneyText"), TEXT("BuyList"), TEXT("SellList"), TEXT("CloseButton"),
			  TEXT("SliderPanel"), TEXT("SliderTitleText"), TEXT("SliderQtyText"),
			  TEXT("SliderQtyAmmoRow"), TEXT("SliderQtyAmmoText"), TEXT("QtySlider"),
			  TEXT("QtyMinusButton"), TEXT("QtyPlusButton"), TEXT("SliderTotalText"),
			  TEXT("SliderConfirmButton"), TEXT("SliderCancelButton") } },
		{ TEXT("/Game/UI/WBP_Dialog"), TEXT("WBP_Dialog"),
			TEXT("/Script/ContrarySurvivor.DialogScreenWidget"), &BuildDialog,
			{ TEXT("NPCNameText"), TEXT("ReplicaText"), TEXT("AcceptButton"), TEXT("DeclineButton"),
			  TEXT("TurnInButton"), TEXT("TurnInText"), TEXT("CloseButton") } },
		{ TEXT("/Game/UI/WBP_PlayerStats"), TEXT("WBP_PlayerStats"),
			TEXT("/Script/ContrarySurvivor.PlayerStatsWidget"), &BuildPlayerStats,
			{ TEXT("HealthBar"), TEXT("HealthText"), TEXT("HungerBar"), TEXT("HungerText"),
			  TEXT("ThirstBar"), TEXT("ThirstText"), TEXT("AmmoRow"), TEXT("AmmoText"),
			  TEXT("MoneyText") } },
	};

	FString ObjectPathOf(const FWbpSpec& Spec)
	{
		return FString::Printf(TEXT("%s.%s"), Spec.PackageName, Spec.AssetName);
	}

	// ======================================================================
	// РЕЖИМ ДОПОЛНЕНИЯ (-augment): точечная правка СУЩЕСТВУЮЩИХ ассетов
	// ======================================================================
	//
	// Зачем отдельный режим. В WBP_PlayerStats, WBP_Inventory и WBP_TouchControls лежит
	// ручная стилизация владельца (шрифты, цвета, размеры, расположение), которой НЕТ в
	// гите. Полная перегенерация (-force) её уничтожает, поэтому для правки готовых
	// панелей нужен путь, который дерево НЕ пересобирает.
	//
	// Сохранность обеспечивается САМОЙ ПРИРОДОЙ правки, а не аккуратностью исполнителя:
	//  1. Каждая операция адресует кубик по ТОЧНОМУ имени и трогает только его.
	//  2. Не нашли кубик — предупреждение и пропуск; дерево не меняется вообще.
	//  3. Ни одна операция не создаёт и не заменяет корень дерева.
	//  4. Все операции идемпотентны: повторный прогон ничего не портит и не дублирует.
	//  5. Ассет сохраняется ТОЛЬКО если что-то реально изменилось (флаг bChanged).
	// Стиль существующих кубиков (шрифт, цвет, размер) не читается и не переписывается —
	// у текстовых правок меняется исключительно свойство Text.

	// Найти кубик по имени. Нет — предупреждение и nullptr (правка пропускается).
	UWidget* AugFind(UWidgetTree* Tree, const TCHAR* AssetName, const TCHAR* WidgetName)
	{
		UWidget* Found = Tree->FindWidget(FName(WidgetName));
		if (!Found)
		{
			UE_LOG(LogGenerateWbp, Warning, TEXT("AUGMENT %s: кубик '%s' не найден — правка пропущена."),
				AssetName, WidgetName);
		}
		return Found;
	}

	// Заменить текст существующего текстового кубика. Стиль не трогаем — только Text.
	void AugSetText(UWidgetTree* Tree, const TCHAR* AssetName, const TCHAR* WidgetName,
		const FText& NewText, bool& bChanged)
	{
		UWidget* Found = AugFind(Tree, AssetName, WidgetName);
		UTextBlock* Block = Cast<UTextBlock>(Found);
		if (!Block)
		{
			if (Found)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("AUGMENT %s: кубик '%s' не текстовый (%s) — правка пропущена."),
					AssetName, WidgetName, *Found->GetClass()->GetName());
			}
			return;
		}
		if (Block->GetText().EqualTo(NewText))
		{
			return; // уже стоит нужный текст — идемпотентность
		}
		Block->SetText(NewText);
		bChanged = true;
		UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT %s: '%s' -> «%s»."),
			AssetName, WidgetName, *NewText.ToString());
	}

	// Иконка из Content/UI/Icons. Текстуры нет — кубик всё равно создаётся (пустая картинка
	// заметна в редакторе), об этом предупреждаем.
	UImage* AugMakeIcon(UWidgetTree* Tree, const TCHAR* AssetName, const FName& IconName,
		const TCHAR* TexturePath, float IconSize)
	{
		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath))
		{
			Icon->SetBrushFromTexture(Texture, /*bMatchSize=*/false);
		}
		else
		{
			UE_LOG(LogGenerateWbp, Warning, TEXT("AUGMENT %s: текстура %s не загрузилась."),
				AssetName, TexturePath);
		}
		Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		return Icon;
	}

	// Обернуть существующий кубик в горизонтальный ряд с заданным именем, поставив ряд на
	// то же место в родителе. Отступы слота-оригинала переносятся на ряд, чтобы раскладка
	// владельца не поехала. Ряд уже есть — ничего не делаем (идемпотентность).
	UHorizontalBox* AugWrapInRow(UWidgetTree* Tree, const TCHAR* AssetName,
		const TCHAR* TargetName, const FName& RowName, bool& bChanged)
	{
		if (UHorizontalBox* Existing = Cast<UHorizontalBox>(Tree->FindWidget(RowName)))
		{
			return Existing; // уже обёрнут прошлым прогоном
		}
		UWidget* Target = AugFind(Tree, AssetName, TargetName);
		if (!Target)
		{
			return nullptr;
		}
		UPanelWidget* Parent = Target->GetParent();
		if (!Parent)
		{
			UE_LOG(LogGenerateWbp, Warning, TEXT("AUGMENT %s: у кубика '%s' нет родителя — обёртка пропущена."),
				AssetName, TargetName);
			return nullptr;
		}

		// Свойства слота владельца снимаем ДО переноса: после него слот будет уже другого
		// типа. Переносим не только отступы, но и выравнивание с размером — иначе строка
		// съедет, даже если отступы совпадут.
		const int32 Index = Parent->GetChildIndex(Target);
		FMargin OldPadding(0.0f);
		EHorizontalAlignment OldHAlign = HAlign_Fill;
		FSlateChildSize OldSize;
		if (const UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Target->Slot))
		{
			OldPadding = VSlot->GetPadding();
			OldHAlign = VSlot->GetHorizontalAlignment();
			OldSize = VSlot->GetSize();
		}
		else if (const UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Target->Slot))
		{
			OldPadding = HSlot->GetPadding();
		}

		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName);
		Parent->RemoveChild(Target);
		if (UHorizontalBoxSlot* BodySlot = Row->AddChildToHorizontalBox(Target))
		{
			BodySlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UPanelSlot* RowSlot = Parent->InsertChildAt(Index, Row))
		{
			if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(RowSlot))
			{
				VSlot->SetPadding(OldPadding);
				VSlot->SetHorizontalAlignment(OldHAlign);
				VSlot->SetSize(OldSize);
			}
			else if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(RowSlot))
			{
				HSlot->SetPadding(OldPadding);
			}
		}
		bChanged = true;
		UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT %s: '%s' обёрнут в ряд '%s'."),
			AssetName, TargetName, *RowName.ToString());
		return Row;
	}

	// Поставить иконку первой в уже созданном ряду. Иконка уже есть — ничего не делаем.
	void AugPrependIcon(UWidgetTree* Tree, const TCHAR* AssetName, UHorizontalBox* Row,
		const FName& IconName, const TCHAR* TexturePath, float IconSize, bool& bChanged)
	{
		if (!Row || Tree->FindWidget(IconName))
		{
			return;
		}
		UImage* Icon = AugMakeIcon(Tree, AssetName, IconName, TexturePath, IconSize);
		if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(Row->InsertChildAt(0, Icon)))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		}
		bChanged = true;
	}

	// Новый текстовый кубик В СТИЛЕ уже лежащего в ассете (шрифт, цвет, тень). Так добавленные
	// значения выглядят как соседние надписи владельца, а не как чужеродная вставка, — и нам
	// не приходится задавать стиль самим, то есть навязывать своё оформление.
	UTextBlock* AugMakeTextLike(UWidgetTree* Tree, const UTextBlock* Sample,
		const FName& Name, const FText& Text)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		if (Sample)
		{
			Block->SetFont(Sample->GetFont());
			Block->SetColorAndOpacity(Sample->GetColorAndOpacity());
			Block->SetShadowOffset(Sample->GetShadowOffset());
			Block->SetShadowColorAndOpacity(Sample->GetShadowColorAndOpacity());
		}
		Block->bIsVariable = true;
		return Block;
	}

	// --- Правки по ассетам (каждая функция работает с уже загруженным деревом) ---

	// WBP_Shop, пункт 1: английские надписи в ассете. Это статичные подписи, код их не
	// пишет — поэтому починить их можно только здесь.
	void AugmentShop(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Shop");
		AugSetText(Tree, Name, TEXT("HeaderText"), NSLOCTEXT("Shop", "HeaderText", "Торговец"), bChanged);
		AugSetText(Tree, Name, TEXT("BuyHeaderText"), NSLOCTEXT("Shop", "BuyHeaderText", "Товары торговца"), bChanged);
		AugSetText(Tree, Name, TEXT("SellHeaderText"), NSLOCTEXT("Shop", "SellHeaderText", "Рюкзак"), bChanged);
		AugSetText(Tree, Name, TEXT("CloseLabel"), NSLOCTEXT("Shop", "CloseLabel", "Закрыть"), bChanged);
		AugSetText(Tree, Name, TEXT("SliderConfirmLabel"), NSLOCTEXT("Shop", "ConfirmLabel", "Подтвердить"), bChanged);
		AugSetText(Tree, Name, TEXT("SliderCancelLabel"), NSLOCTEXT("Shop", "CancelLabel", "Отмена"), bChanged);
	}

	// WBP_Inventory, пункт 2: числа статов замерли. В ассете висит старый слипшийся кубик
	// StatsText («Монеты 0   Голод 100 / 100   Жажда 100 / 100»), в который код больше не
	// пишет — вместо него теперь три отдельных кубика значений. Ставим ряд
	// [иконка][значение] × 3 на место старого кубика и старый удаляем.
	void AugmentInventory(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Inventory");
		if (Tree->FindWidget(TEXT("InvMoneyText")))
		{
			return; // ряд уже поставлен прошлым прогоном
		}

		UWidget* OldStats = AugFind(Tree, Name, TEXT("StatsText"));
		if (!OldStats)
		{
			return;
		}
		UPanelWidget* Parent = OldStats->GetParent();
		if (!Parent)
		{
			UE_LOG(LogGenerateWbp, Warning, TEXT("AUGMENT %s: у StatsText нет родителя — правка пропущена."), Name);
			return;
		}

		const int32 Index = Parent->GetChildIndex(OldStats);
		const FMargin OldPadding = [OldStats]()
		{
			const UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(OldStats->Slot);
			return VSlot ? VSlot->GetPadding() : FMargin(0.0f, 8.0f, 0.0f, 12.0f);
		}();

		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StatsRow"));

		// Значения пишет код, поэтому образец текста тут — только чтобы кубик было видно
		// в редакторе. А вот оформление НАСЛЕДУЕМ с удаляемого кубика: «не задавать стиль»
		// здесь означало бы не свободу владельцу, а молча стёртое оформление и работу
		// перекрашивать три кубика руками.
		auto AddPair = [&](const TCHAR* IconName, const TCHAR* TexturePath,
			const TCHAR* ValueName, const TCHAR* Sample, float LeftPad)
		{
			if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(
				AugMakeIcon(Tree, Name, FName(IconName), TexturePath, 22.0f)))
			{
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
			}
			// Стиль берём с УДАЛЯЕМОГО кубика: новые числа получают шрифт и цвет, которые
			// владелец настроил для этой строки, а не наши собственные.
			UTextBlock* Value = AugMakeTextLike(Tree, Cast<UTextBlock>(OldStats),
				FName(ValueName), FText::FromString(Sample));
			if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value))
			{
				ValueSlot->SetVerticalAlignment(VAlign_Center);
				ValueSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
			}
		};
		AddPair(TEXT("InvMoneyIcon"), TEXT("/Game/UI/Icons/T_Icon_Money.T_Icon_Money"),
			TEXT("InvMoneyText"), TEXT("0"), 0.0f);
		AddPair(TEXT("InvHungerIcon"), TEXT("/Game/UI/Icons/T_Icon_Hunger.T_Icon_Hunger"),
			TEXT("InvHungerText"), TEXT("100 из 100"), 24.0f);
		AddPair(TEXT("InvThirstIcon"), TEXT("/Game/UI/Icons/T_Icon_Thirst.T_Icon_Thirst"),
			TEXT("InvThirstText"), TEXT("100 из 100"), 24.0f);

		Parent->RemoveChild(OldStats);
		if (UPanelSlot* RowSlot = Parent->InsertChildAt(Index, Row))
		{
			if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(RowSlot))
			{
				VSlot->SetPadding(OldPadding);
			}
		}
		Tree->RemoveWidget(OldStats);

		bChanged = true;
		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: слипшийся StatsText заменён рядом из трёх пар «иконка + значение»."), Name);
	}

	// Поставить готовый кубик СРАЗУ ПОСЛЕ существующего, в того же родителя. Нужно, когда
	// новую строку надо вписать в определённое место столбца, а не в конец.
	void AugInsertAfter(UWidgetTree* Tree, const TCHAR* AssetName, const TCHAR* AfterName,
		UWidget* NewWidget, bool& bChanged)
	{
		UWidget* After = AugFind(Tree, AssetName, AfterName);
		if (!After)
		{
			return;
		}
		UPanelWidget* Parent = After->GetParent();
		if (!Parent)
		{
			UE_LOG(LogGenerateWbp, Warning, TEXT("AUGMENT %s: у кубика '%s' нет родителя — вставка пропущена."),
				AssetName, AfterName);
			return;
		}
		Parent->InsertChildAt(Parent->GetChildIndex(After) + 1, NewWidget);
		bChanged = true;
	}

	// WBP_PlayerStats, пункты 3-5. Слова-подписи владельцу не нужны — вместо них иконки
	// слева от полосок; полоски голода и жажды короче полоски здоровья; строка патронов
	// оборачивается в контейнер AmmoRow, который код прячет целиком.
	void AugmentPlayerStats(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_PlayerStats");

		struct FStatRow
		{
			const TCHAR* OverlayName;
			const TCHAR* RowName;
			const TCHAR* IconName;
			const TCHAR* TexturePath;
		};
		static const FStatRow Rows[] =
		{
			{ TEXT("HealthBarOverlay"), TEXT("HealthRow"), TEXT("HealthIcon"), TEXT("/Game/UI/Icons/T_Icon_Health.T_Icon_Health") },
			{ TEXT("HungerBarOverlay"), TEXT("HungerRow"), TEXT("HungerIcon"), TEXT("/Game/UI/Icons/T_Icon_Hunger.T_Icon_Hunger") },
			{ TEXT("ThirstBarOverlay"), TEXT("ThirstRow"), TEXT("ThirstIcon"), TEXT("/Game/UI/Icons/T_Icon_Thirst.T_Icon_Thirst") },
		};
		for (const FStatRow& Row : Rows)
		{
			UHorizontalBox* IconRow = AugWrapInRow(Tree, Name, Row.OverlayName, Row.RowName, bChanged);
			AugPrependIcon(Tree, Name, IconRow, Row.IconName, Row.TexturePath, 24.0f, bChanged);
		}

		// Деньги: иконка монеты перед числом (плашку MoneyPlate не трогаем).
		UHorizontalBox* MoneyRow = AugWrapInRow(Tree, Name, TEXT("MoneyText"), TEXT("MoneyRow"), bChanged);
		AugPrependIcon(Tree, Name, MoneyRow, TEXT("MoneyIcon"),
			TEXT("/Game/UI/Icons/T_Icon_Money.T_Icon_Money"), 22.0f, bChanged);

		// Полоски голода и жажды — доля от полоски здоровья (решение владельца). Ширину
		// берём с САМОЙ полоски здоровья, а не из числа в коде: владелец мог её менять,
		// и тогда пропорция всё равно сохранится.
		const float ShortBarRatio = 0.7f;
		USizeBox* HealthSize = Cast<USizeBox>(Tree->FindWidget(TEXT("HealthBarSize")));
		if (!HealthSize)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: кубик 'HealthBarSize' не найден — длину полосок не меняю."), Name);
		}
		else
		{
			const float TargetWidth = HealthSize->GetWidthOverride() * ShortBarRatio;
			for (const TCHAR* BarName : { TEXT("HungerBarSize"), TEXT("ThirstBarSize") })
			{
				USizeBox* Bar = Cast<USizeBox>(Tree->FindWidget(FName(BarName)));
				if (!Bar)
				{
					UE_LOG(LogGenerateWbp, Warning, TEXT("AUGMENT %s: кубик '%s' не найден — длина не изменена."),
						Name, BarName);
					continue;
				}
				if (FMath::IsNearlyEqual(Bar->GetWidthOverride(), TargetWidth))
				{
					continue; // уже укорочена прошлым прогоном
				}
				UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT %s: '%s' ширина %.0f -> %.0f."),
					Name, BarName, Bar->GetWidthOverride(), TargetWidth);
				Bar->SetWidthOverride(TargetWidth);
				bChanged = true;
			}
		}

		// Строка патронов: контейнер, который код прячет целиком. Иконки патронов не
		// нарисовано — ряд без иконки (решение game-lead). В ассете сразу спрятан, потому
		// что с ножом в руках строки быть не должно; показывает её код.
		if (!Tree->FindWidget(TEXT("AmmoRow")))
		{
			if (UHorizontalBox* AmmoRow = AugWrapInRow(Tree, Name, TEXT("AmmoText"), TEXT("AmmoRow"), bChanged))
			{
				AmmoRow->SetVisibility(ESlateVisibility::Collapsed);
				AmmoRow->bIsVariable = true;
			}
		}
	}

	// WBP_Shop, пункт 6: строка пересчёта пачек в патроны. Показывается только при покупке
	// патронов, поэтому в ассете сразу спрятана — видимостью управляет код.
	void AugmentShopAmmoRow(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Shop");
		if (Tree->FindWidget(TEXT("SliderQtyAmmoRow")))
		{
			return;
		}
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("SliderQtyAmmoRow"));
		Row->SetVisibility(ESlateVisibility::Collapsed);
		Row->bIsVariable = true;

		// Стиль — с соседней строки количества, чтобы пересчёт выглядел её продолжением.
		Row->AddChildToHorizontalBox(AugMakeTextLike(Tree,
			Cast<UTextBlock>(Tree->FindWidget(TEXT("SliderQtyText"))),
			TEXT("SliderQtyAmmoText"), FText::FromString(TEXT("всего 30 патронов"))));

		AugInsertAfter(Tree, Name, TEXT("SliderQtyText"), Row, bChanged);
	}

	// WBP_ShopRow, пункт 7: строка «Не хватает монет» рядом с ценой. В ассете спрятана —
	// код показывает её только в тех товарах, на которые не хватает денег.
	void AugmentShopRow(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_ShopRow");
		if (Tree->FindWidget(TEXT("NoMoneyText")))
		{
			return;
		}
		UTextBlock* NoMoney = AugMakeTextLike(Tree,
			Cast<UTextBlock>(Tree->FindWidget(TEXT("PriceText"))),
			TEXT("NoMoneyText"), NSLOCTEXT("Shop", "NotEnoughMoney", "Не хватает монет"));
		NoMoney->SetVisibility(ESlateVisibility::Collapsed);

		AugInsertAfter(Tree, Name, TEXT("PriceText"), NoMoney, bChanged);
	}

	// WBP_QuestTracker, пункт 8: в ассете лежит текст-заглушка от генерации. Строку пишет
	// код, а пока квеста нет — кубик должен быть пустым, иначе заглушка мелькает на экране.
	void AugmentQuestTracker(UWidgetTree* Tree, bool& bChanged)
	{
		AugSetText(Tree, TEXT("WBP_QuestTracker"), TEXT("TrackerText"), FText::GetEmpty(), bChanged);
	}

	// WBP_TouchControls: прежняя заплатка, перенесённая в общий вид без изменения поведения —
	// добавить кубик WeaponIconImage, если его ещё нет. Позиция берётся со слота кнопки
	// ОРУЖИЕ: куда владелец её передвинул, туда встанет и иконка.
	void AugmentTouchControls(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_TouchControls");
		if (Tree->FindWidget(TEXT("WeaponIconImage")))
		{
			return;
		}
		UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
		if (!Root)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: корень не CanvasPanel (%s) — некуда класть иконку."),
				Name, Tree->RootWidget ? *Tree->RootWidget->GetClass()->GetName() : TEXT("null"));
			return;
		}

		FAnchors IconAnchors(1.0f, 1.0f, 1.0f, 1.0f);
		FVector2D IconPos(-320.0f, -300.0f);
		if (UWidget* WeaponBtn = Tree->FindWidget(TEXT("WeaponButton")))
		{
			if (UCanvasPanelSlot* BtnSlot = Cast<UCanvasPanelSlot>(WeaponBtn->Slot))
			{
				IconAnchors = BtnSlot->GetAnchors();
				IconPos = BtnSlot->GetPosition() - FVector2D(0.0f, BtnSlot->GetSize().Y * 0.5f + 34.0f);
			}
		}

		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WeaponIconImage"));
		Icon->SetVisibility(ESlateVisibility::Collapsed); // текстуру и показ ставит код игры
		Icon->bIsVariable = true;
		if (UCanvasPanelSlot* IconSlot = Root->AddChildToCanvas(Icon))
		{
			IconSlot->SetAnchors(IconAnchors);
			IconSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			IconSlot->SetPosition(IconPos);
			IconSlot->SetSize(FVector2D(48.0f, 48.0f));
		}
		bChanged = true;
		UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT %s: добавлен WeaponIconImage (позиция %s)."),
			Name, *IconPos.ToString());
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

		// WBP_Shop: строкой списков назначаем сгенерированный WBP_ShopRow (тот же приём,
		// что у инвентаря выше) — без этого оба списка магазина пусты.
		if (FCString::Strcmp(Spec.AssetName, TEXT("WBP_Shop")) == 0)
		{
			UClass* RowClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr,
				TEXT("/Game/UI/WBP_ShopRow.WBP_ShopRow_C"));
			UShopScreenWidget* CDO = WBP->GeneratedClass
				? Cast<UShopScreenWidget>(WBP->GeneratedClass->GetDefaultObject())
				: nullptr;
			if (RowClass && CDO)
			{
				CDO->RowWidgetClass = RowClass;
				UE_LOG(LogGenerateWbp, Display, TEXT("WBP_Shop: Row Widget Class = %s."), *RowClass->GetName());
			}
			else
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("WBP_Shop: Row Widget Class НЕ назначен (класс строки=%d, CDO=%d) — назначить в редакторе."),
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
	if (Switches.Contains(TEXT("augment")))
	{
		return AugmentAll();
	}
	return GenerateAll(Switches.Contains(TEXT("force")));
}

int32 UGenerateWbpCommandlet::AugmentAll()
{
	// Таблица точечных правок: ассет -> функция, которая его дополняет. Каждая функция
	// работает по ТОЧНЫМ именам кубиков и дерево не пересобирает (см. шапку раздела).
	// Ассет сохраняется, только если функция реально что-то изменила.
	struct FAugmentSpec
	{
		const TCHAR* PackageName;
		const TCHAR* AssetName;
		void (*Augment)(UWidgetTree*, bool&);
	};
	// Порядок = порядок важности из задания: первыми панели, которые владелец видит сейчас.
	// У WBP_Shop две записи: надписи и строка патронов — разные правки одного ассета,
	// каждая со своей проверкой «уже сделано», поэтому их удобнее держать раздельно.
	static const FAugmentSpec Specs[] =
	{
		{ TEXT("/Game/UI/WBP_Shop"),          TEXT("WBP_Shop"),          &AugmentShop },
		{ TEXT("/Game/UI/WBP_Inventory"),     TEXT("WBP_Inventory"),     &AugmentInventory },
		{ TEXT("/Game/UI/WBP_PlayerStats"),   TEXT("WBP_PlayerStats"),   &AugmentPlayerStats },
		{ TEXT("/Game/UI/WBP_Shop"),          TEXT("WBP_Shop"),          &AugmentShopAmmoRow },
		{ TEXT("/Game/UI/WBP_ShopRow"),       TEXT("WBP_ShopRow"),       &AugmentShopRow },
		{ TEXT("/Game/UI/WBP_QuestTracker"),  TEXT("WBP_QuestTracker"),  &AugmentQuestTracker },
		{ TEXT("/Game/UI/WBP_TouchControls"), TEXT("WBP_TouchControls"), &AugmentTouchControls },
	};

	int32 FailCount = 0;
	int32 SavedCount = 0;
	for (const FAugmentSpec& Spec : Specs)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), Spec.PackageName, Spec.AssetName);
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("AUGMENT: %s не загрузился."), Spec.AssetName);
			++FailCount;
			continue;
		}

		bool bChanged = false;
		WBP->Modify();
		Spec.Augment(WBP->WidgetTree, bChanged);

		if (!bChanged)
		{
			UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT SKIP: %s — менять нечего."), Spec.AssetName);
			continue;
		}

		FKismetEditorUtilities::CompileBlueprint(WBP);
		if (WBP->Status == BS_Error)
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("AUGMENT: %s скомпилировался с ошибками — НЕ сохраняю (ассет на диске цел)."),
				Spec.AssetName);
			++FailCount;
			continue;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Spec.PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(WBP->GetOutermost(), WBP, *Filename, SaveArgs))
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("AUGMENT: SavePackage не сохранил %s."), *Filename);
			++FailCount;
			continue;
		}
		++SavedCount;
		UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT OK: %s сохранён."), Spec.AssetName);
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT ИТОГ: сохранено %d, ошибок %d, всего ассетов %d."),
		SavedCount, FailCount, static_cast<int32>(UE_ARRAY_COUNT(Specs)));
	return FailCount > 0 ? 1 : 0;
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

		// Тот же контракт магазина: класс строки списков назначен.
		if (bOk && FCString::Strcmp(Spec.AssetName, TEXT("WBP_Shop")) == 0)
		{
			const UShopScreenWidget* CDO = WBP->GeneratedClass
				? Cast<UShopScreenWidget>(WBP->GeneratedClass->GetDefaultObject())
				: nullptr;
			if (CDO && CDO->RowWidgetClass)
			{
				UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY WBP_Shop: Row Widget Class = %s."),
					*CDO->RowWidgetClass->GetName());
			}
			else
			{
				UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: WBP_Shop — Row Widget Class пуст."));
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
