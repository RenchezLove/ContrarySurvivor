// Fill out your copyright notice in the Description page of Project Settings.

#include "GenerateWbpCommandlet.h"

#include "Algo/Find.h" // список намеренно выброшенных кубиков при -rebuild (Build 1.2.1 Д1)
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
#include "Engine/StaticMesh.h" // Build 1.2.1 (-pickupfix): материал слотов мешей лута
#include "Engine/Texture2D.h" // LoadObject<UTexture2D> для иконок (в Image.h только объявление)
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h" // Build 1.2.1 (-pickupfix): параметры свечения в M_VColor
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "StaticMeshResources.h" // пруф вершинных цветов (ColorVertexBuffer)
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Styling/SlateTypes.h"
#include "UObject/PropertyPortFlags.h" // PPF_None для переноса стилизации владельца
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h" // FindFProperty/FProperty для переноса стилизации владельца
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

	// Иконка из Content/UI/Icons. Размер пишется В КИСТЬ (Brush.ImageSize): прежний путь
	// через SetDesiredSizeOverride менял только живой Slate-виджет и в ассет НЕ попадал
	// (UImage::SetDesiredSizeOverride пишет в MyImage, не в UPROPERTY — Image.cpp:122-128),
	// поэтому иконки выходили дефолтных 32x32. Текстуры нет — кубик всё равно создаётся
	// (пустая картинка заметна в редакторе), об этом предупреждаем.
	UImage* MakeIcon(UWidgetTree* Tree, const TCHAR* ContextName, const FName& IconName,
		const TCHAR* TexturePath, float IconSize)
	{
		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);
		FSlateBrush IconBrush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath))
		{
			IconBrush.SetResourceObject(Texture);
		}
		else
		{
			UE_LOG(LogGenerateWbp, Warning, TEXT("%s: текстура %s не загрузилась."),
				ContextName, TexturePath);
		}
		IconBrush.SetImageSize(FVector2D(IconSize, IconSize));
		Icon->SetBrush(IconBrush);
		return Icon;
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
	// «Замок» на контенте кнопок — фикс выделения в дизайнере (добро лида 07-27)
	// ======================================================================
	//
	// Клик в UMG-дизайнере выделяет САМЫЙ ГЛУБОКИЙ виджет блюпринта под курсором
	// (SDesignerView::FindWidgetUnderCursor — обход BubblePath с конца,
	// UMGEditor/Private/Designer/SDesignerView.cpp:1632-1678), поэтому по кнопке
	// выделялась её подпись/бокс, а они лежат в слоте КНОПКИ — ручки же
	// перетаскивания/ресайза дизайнер даёт только канвас-слоту
	// (STransformHandle::CanResize, Designer/STransformHandle.cpp:153-155).
	//
	// Спрятать контент от дизайнера через ESlateVisibility::HitTestInvisible НЕЛЬЗЯ:
	// дизайнер НЕ пользуется игровой hit-test-сеткой Slate, а строит СВОЮ, обходя
	// дерево БЕЗ учёта Visibility (PopulateWidgetGeometryCache_Loop,
	// SDesignerView.cpp:2034-2089, дети берутся фильтром EVisibility::All). Фильтр
	// попадания в сетку — только редакторные флаги IsVisibleInDesigner («глазик») и
	// IsLockedInDesigner («замок») при включённом Respect Locks (строки 2052-2077;
	// дефолт настройки true — WidgetDesignerSettings.cpp:16). Будущий код: НЕ пытаться
	// «чинить» выделение через HitTestInvisible — работает только замок/глазик.
	//
	// Поэтому контент кнопок статичной раскладки «замыкаем» (bLockedInDesigner):
	// замкнутый виджет выпадает из сетки дизайнера, клик проваливается к самой
	// кнопке — у неё канвас-слот, ручки и перетаскивание работают. Замок НЕ
	// наследуется (IsLockedInDesigner читает только собственный флаг — Widget.h:474)
	// — ставим рекурсивно на всё поддерево контента. Флаг редакторный
	// (WITH_EDITORONLY_DATA, Widget.h:408-410): сериализуется в ассет, на рантайм не
	// влияет никак. Цена для владельца: чтобы править текст подписи, надо один раз
	// снять замок значком на её строке в панели «Иерархия» (панель «Детали» у
	// замкнутого виджета заблокирована — SWidgetDetailsView.cpp:375-393).

	void LockSubtreeInDesigner(UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}
		Widget->SetLockedInDesigner(true);
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				LockSubtreeInDesigner(Panel->GetChildAt(Index));
			}
		}
	}

	// Замкнуть только ДЕТЕЙ панели: сама панель остаётся свободной (она лежит в
	// канвас-слоте, её выделяют и таскают мышкой), а вся начинка проваливает клик к
	// панели. Для рядов панели статов это аналог SetButtonContent, где роль «кнопки»
	// играет ряд. Звать ПОСЛЕ сборки поддерева (замок не наследуется).
	void LockPanelChildrenInDesigner(UPanelWidget* Panel)
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			LockSubtreeInDesigner(Panel->GetChildAt(Index));
		}
	}

	// Контент в кнопки статичной раскладки класть ТОЛЬКО этим хелпером: SetContent +
	// замок на всём поддереве. Звать ПОСЛЕ сборки поддерева контента (замок не
	// наследуется — поздним детям он бы не достался).
	void SetButtonContent(UButton* Button, UWidget* Content)
	{
		Button->SetContent(Content);
		LockSubtreeInDesigner(Content);
	}

	// ======================================================================
	// Канвас-первая раскладка экранов (ADR-051 п.1, добро лида 07-24)
	// ======================================================================
	//
	// Ручки перетаскивания/ресайза мышкой в дизайнере есть ТОЛЬКО у виджета в
	// канвас-слоте (STransformHandle::CanResize: Cast<UCanvasPanelSlot> != nullptr,
	// UMGEditor/Private/Designer/STransformHandle.cpp:153). Поэтому кнопки, подложки
	// и списки кладём каждый в СВОЙ канвас-слот, а контейнерные коробки
	// (VerticalBox/SizeBox) для статичной раскладки не используем. Тексты — авторазмером
	// (размер задаёт шрифт), кнопки/списки/панели — явным прямоугольником или растяжкой.

	// Виджет в канвас-слот с явным прямоугольником (якорь и выравнивание — верх-лево).
	UCanvasPanelSlot* CanvasAt(UCanvasPanel* Canvas, UWidget* Widget,
		const FVector2D& Pos, const FVector2D& Size)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors());
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetPosition(Pos);
			Slot->SetSize(Size);
		}
		return Slot;
	}

	// Виджет в канвас-слот авторазмером: позиция фиксирована, габарит — по содержимому.
	UCanvasPanelSlot* CanvasAuto(UCanvasPanel* Canvas, UWidget* Widget,
		const FVector2D& Pos, const FAnchors& Anchors = FAnchors())
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		if (Slot)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetPosition(Pos);
			Slot->SetAutoSize(true);
		}
		return Slot;
	}

	// Виджет-растяжка по долевым якорям с отступами (списки: при ресайзе панели мышкой
	// тянутся следом). При растянутой оси Offsets = отступы от якорей, не позиция/размер.
	UCanvasPanelSlot* CanvasStretch(UCanvasPanel* Canvas, UWidget* Widget,
		const FAnchors& Anchors, const FMargin& Offsets)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		if (Slot)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(Offsets);
		}
		return Slot;
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

	// Слот брони paper-doll: кнопка (клик по занятому — снять) с подписью, иконкой и текстом
	// надетого. Габарит задаёт канвас-слот вызывающего (ADR-051: ручки мышкой).
	UButton* MakeArmorSlotButton(UWidgetTree* Tree, UObject* Roboto,
		const FString& StaticCaption, const FName& ButtonName, const FName& IconName, const FName& TextName)
	{
		UButton* SlotButton = MakeStyledButton(Tree, ButtonName,
			FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
			FLinearColor(0.25f, 0.27f, 0.33f, 1.0f)); // InvSlotColor + подсветки

		UHorizontalBox* SlotBox = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			FName(*(ButtonName.ToString() + TEXT("Box"))));

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

		// Контент — в самом конце: SetButtonContent замыкает поддерево целиком,
		// включая только что добавленных детей (клик выделяет саму кнопку).
		SetButtonContent(SlotButton, SlotBox);
		return SlotButton;
	}

	// Экран инвентаря — КАНВАС-ПЕРВЫЙ (ADR-051 п.1, добро лида 07-24): каждая кнопка,
	// подложка и список — в своём канвас-слоте, чтобы владелец тянул их мышкой за край
	// в дизайнере (см. шапку раздела канвас-помощников). Числа позиций — арифметика от
	// той же геометрии, что у прежней контейнерной раскладки (панель 960x600, отступ 16
	// -> внутренняя область 928x568, колонки 0.42/0.58, слот брони 56 + зазор 10):
	// вид тот же, изменилась только механика раскладки. Строка статов — иконки+значения
	// (перенос augment-вида в базовую генерацию); подписи «Защита»/«Оружие» — отдельные
	// кубики (догон ADR-050: код пишет в ProtectionText/WeaponText только значение).
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

		// Центральная панель 960x600 (UIPanelMaxWidth/Height) с золотой рамкой (#18) —
		// канвас-слот, тянется мышкой. Внутри — собственный канвас: содержимое
		// позиционируется в области за вычетом отступа 16 (928x568).
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

		UCanvasPanel* PanelCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
		Panel->SetContent(PanelCanvas);

		// Заголовок (статичный текст Рината), высота строки ~26 при 22pt.
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("HeaderText"),
			TEXT("Инвентарь"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 22, TEXT("Bold")),
			FVector2D(0.0f, 0.0f));

		// Строка статов: три пары «иконка + значение» ОДНИМ рядом. Ряд остаётся HBox
		// сознательно: числа меняются в игре и толкают соседей — по отдельности на канвасе
		// пары наезжали бы друг на друга при росте числа. Ряд целиком двигается мышкой.
		const FLinearColor StatsColor(1.0f, 0.85f, 0.2f, 1.0f); // UIMoneyColor
		UHorizontalBox* StatsRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("StatsRow"));
		auto AddStatPair = [&](const TCHAR* IconName, const TCHAR* TexturePath,
			const TCHAR* ValueName, const TCHAR* ValueSample, float LeftPad)
		{
			UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(IconName));
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath))
			{
				Icon->SetBrushFromTexture(Texture, /*bMatchSize=*/false);
			}
			else
			{
				UE_LOG(LogGenerateWbp, Warning, TEXT("WBP_Inventory: текстура %s не загрузилась."), TexturePath);
			}
			Icon->SetDesiredSizeOverride(FVector2D(22.0f, 22.0f));
			if (UHorizontalBoxSlot* IconSlot = StatsRow->AddChildToHorizontalBox(Icon))
			{
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
			}
			UTextBlock* Value = MakeText(Tree, Roboto, FName(ValueName), ValueSample,
				StatsColor, 16, TEXT("Regular"));
			Value->bIsVariable = true;
			if (UHorizontalBoxSlot* ValueSlot = StatsRow->AddChildToHorizontalBox(Value))
			{
				ValueSlot->SetVerticalAlignment(VAlign_Center);
				ValueSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
			}
		};
		AddStatPair(TEXT("InvMoneyIcon"), TEXT("/Game/UI/Icons/T_Icon_Money.T_Icon_Money"),
			TEXT("InvMoneyText"), TEXT("0"), 0.0f);
		AddStatPair(TEXT("InvHungerIcon"), TEXT("/Game/UI/Icons/T_Icon_Hunger.T_Icon_Hunger"),
			TEXT("InvHungerText"), TEXT("100 из 100"), 24.0f);
		AddStatPair(TEXT("InvThirstIcon"), TEXT("/Game/UI/Icons/T_Icon_Thirst.T_Icon_Thirst"),
			TEXT("InvThirstText"), TEXT("100 из 100"), 24.0f);
		CanvasAuto(PanelCanvas, StatsRow, FVector2D(0.0f, 34.0f)); // под заголовком (+8)

		// Левая колонка: снаряжение. Ширина = прежняя доля 0.42 от 928 минус зазор 12 ≈ 378.
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("EquipHeaderText"),
			TEXT("СНАРЯЖЕНИЕ"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")),
			FVector2D(0.0f, 68.0f));
		CanvasAt(PanelCanvas, MakeArmorSlotButton(Tree, Roboto, TEXT("Голова"),
			TEXT("HeadSlotButton"), TEXT("HeadSlotIcon"), TEXT("HeadSlotText")),
			FVector2D(0.0f, 94.0f), FVector2D(378.0f, 56.0f));
		CanvasAt(PanelCanvas, MakeArmorSlotButton(Tree, Roboto, TEXT("Торс"),
			TEXT("TorsoSlotButton"), TEXT("TorsoSlotIcon"), TEXT("TorsoSlotText")),
			FVector2D(0.0f, 160.0f), FVector2D(378.0f, 56.0f));
		CanvasAt(PanelCanvas, MakeArmorSlotButton(Tree, Roboto, TEXT("Штаны"),
			TEXT("LegsSlotButton"), TEXT("LegsSlotIcon"), TEXT("LegsSlotText")),
			FVector2D(0.0f, 226.0f), FVector2D(378.0f, 56.0f));

		// Защита и оружие: подпись и значение — ОТДЕЛЬНЫЕ кубики (каждый двигается мышкой).
		// Цвет значения защиты — нейтральный: при нулевой защите зелёный читался бы как
		// «всё хорошо», хотя брони нет (ADR-049). Итоговый цвет всё равно за Ринатом.
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("ProtectionLabel"), TEXT("Защита"),
			FLinearColor::White, 15, TEXT("Regular")), FVector2D(0.0f, 297.0f));
		UTextBlock* Protection = MakeText(Tree, Roboto, TEXT("ProtectionText"), TEXT("0%"),
			FLinearColor(0.85f, 0.85f, 0.85f, 1.0f), 15, TEXT("Regular"));
		Protection->bIsVariable = true;
		CanvasAuto(PanelCanvas, Protection, FVector2D(64.0f, 297.0f));

		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("WeaponLabel"), TEXT("Оружие"),
			FLinearColor::White, 15, TEXT("Regular")), FVector2D(0.0f, 319.0f));
		UTextBlock* Weapon = MakeText(Tree, Roboto, TEXT("WeaponText"), TEXT("Пусто"),
			FLinearColor::White, 15, TEXT("Regular"));
		Weapon->bIsVariable = true;
		CanvasAuto(PanelCanvas, Weapon, FVector2D(64.0f, 319.0f));

		// Правая колонка: рюкзак. Заголовок — у прежней доли 0.42; список — якоря-РАСТЯЖКА
		// до краёв панели (низ — над кнопкой закрытия): при ресайзе панели тянется следом.
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("BackpackHeaderText"),
			TEXT("РЮКЗАК"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")),
			FVector2D(0.0f, 68.0f), FAnchors(0.42f, 0.0f, 0.42f, 0.0f));

		UScrollBox* Backpack = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BackpackList"));
		Backpack->bIsVariable = true;
		CanvasStretch(PanelCanvas, Backpack, FAnchors(0.42f, 0.0f, 1.0f, 1.0f),
			FMargin(0.0f, 95.0f, 0.0f, 38.0f));

		// Кнопка закрытия (низ-право; дублирует Tab). Явный габарит — ручки мышкой.
		UButton* Close = MakeStyledButton(Tree, TEXT("CloseButton"),
			FLinearColor(0.3f, 0.3f, 0.34f, 1.0f), FLinearColor(0.4f, 0.4f, 0.45f, 1.0f),
			FLinearColor(0.5f, 0.5f, 0.55f, 1.0f));
		SetButtonContent(Close, MakeText(Tree, Roboto, TEXT("CloseLabel"), TEXT("Закрыть (Tab)"),
			FLinearColor::White, 15, TEXT("Regular")));
		if (UCanvasPanelSlot* CloseSlot = PanelCanvas->AddChildToCanvas(Close))
		{
			CloseSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
			CloseSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			CloseSlot->SetPosition(FVector2D::ZeroVector);
			CloseSlot->SetSize(FVector2D(120.0f, 28.0f));
		}
		return true;
	}

	// Экран смерти — КАНВАС-ПЕРВЫЙ (ADR-051 п.1, волна «двигать мышкой все окна» 07-28):
	// затемнение + каждый элемент прежнего столбца (заголовок, ряды статистики, строки
	// итога, кнопка, подсказка) в СВОЁМ канвас-слоте с ручками. Прежний столб DeathBox
	// (VerticalBox) убран: его слоты ручек не дают. Якорь всех элементов — точка чуть выше
	// середины экрана (0.5/0.45, где стоял центр столба); позиции Y повторяют раскладку,
	// которую столб считал сам (высота строки от кегля + прежние отступы). Ряды статистики
	// остаются HorizontalBox сознательно: значение растёт в игре и толкает подпись — на
	// канвасе порознь они наезжали бы друг на друга; начинка рядов замкнута
	// (bLockedInDesigner), клик в дизайнере выделяет ряд целиком. Экран ничего не прячет
	// (и Canvas-путь не прятал) — пустот от скрытых элементов тут не бывает.
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

		// Элемент на вертикальной оси центра: авторазмер, горизонтальное центрирование.
		auto PlaceCentered = [&](UWidget* Widget, float Y)
		{
			if (UCanvasPanelSlot* Slot = CanvasAuto(Root, Widget,
				FVector2D(0.0f, Y), FAnchors(0.5f, 0.45f, 0.5f, 0.45f)))
			{
				Slot->SetAlignment(FVector2D(0.5f, 0.0f));
			}
		};

		// Статичные строки (заголовок/штраф/подсказка) — тексты Рината, формулировки ADR-044.
		PlaceCentered(MakeText(Tree, Roboto, TEXT("TitleText"), TEXT("ВЫ ПОГИБЛИ"),
			FLinearColor(0.9f, 0.12f, 0.1f, 1.0f), 42, TEXT("Bold")), -236.0f);

		// Каждая строка статистики — пара «статичная подпись + значение» в одном ряду:
		// подпись принадлежит Ринату, код пишет только число (ADR-050).
		const FLinearColor StatColor(0.95f, 0.95f, 0.95f, 1.0f);
		auto AddStatLine = [&](const TCHAR* LabelName, const TCHAR* Caption,
			const TCHAR* ValueName, const TCHAR* ValueSample, float Y)
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
			PlaceCentered(Row, Y);
			LockPanelChildrenInDesigner(Row);
		};
		AddStatLine(TEXT("LifetimeLabel"), TEXT("Прожито"), TEXT("LifetimeText"), TEXT("00:00"), -164.0f);
		AddStatLine(TEXT("KillerLabel"), TEXT("Убийца"), TEXT("KillerText"), TEXT("—"), -132.0f);
		AddStatLine(TEXT("MoneyLabel"), TEXT("Монеты"), TEXT("MoneyText"), TEXT("0"), -100.0f);
		AddStatLine(TEXT("QuestsLabel"), TEXT("Квестов выполнено"), TEXT("QuestsText"), TEXT("0"), -68.0f);
		AddStatLine(TEXT("KillsLabel"), TEXT("Врагов убито"), TEXT("KillsText"), TEXT("0"), -36.0f);

		PlaceCentered(MakeText(Tree, Roboto, TEXT("RespawnLineText"),
			TEXT("Возрождение у костра в деревне."), StatColor, 19, TEXT("Regular")), 10.0f);
		UTextBlock* MoneyLoss = MakeText(Tree, Roboto, TEXT("MoneyLossText"),
			TEXT("−50% монет — если возродиться без просмотра ролика."),
			FLinearColor(0.95f, 0.55f, 0.15f, 1.0f), 19, TEXT("Regular")); // DeathPenaltyColor
		MoneyLoss->bIsVariable = true;
		PlaceCentered(MoneyLoss, 40.0f);
		PlaceCentered(MakeText(Tree, Roboto, TEXT("ConsumablesLineText"),
			TEXT("Половина потерянного останется мешком на месте гибели."), StatColor, 19, TEXT("Regular")), 70.0f);
		PlaceCentered(MakeText(Tree, Roboto, TEXT("SavedLineText"),
			TEXT("Снаряжение, оружие и важные предметы сохранены."),
			FLinearColor(0.45f, 0.85f, 0.45f, 1.0f), 19, TEXT("Regular")), 100.0f); // DeathSavedColor

		// --- Блок «Будет потеряно» (Build 1.2, ТЗ №1 раздел 2): заголовок, сетка позиций
		// (наполняет код: до 8 иконок с количеством), хвост «и ещё N», строка денег.
		// Один столб в одном канвас-слоте: блок живёт/прячется как целое (LossPanel),
		// начинка замкнута — клик в дизайнере выделяет блок целиком.
		const FLinearColor LossColor(0.95f, 0.55f, 0.15f, 1.0f);
		UVerticalBox* LossPanel = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LossPanel"));
		LossPanel->bIsVariable = true;

		UTextBlock* LossHeader = MakeText(Tree, Roboto, TEXT("LossHeaderText"),
			TEXT("БУДЕТ ПОТЕРЯНО"), LossColor, 18, TEXT("Bold"));
		if (UVerticalBoxSlot* HeaderSlot = LossPanel->AddChildToVerticalBox(LossHeader))
		{
			HeaderSlot->SetHorizontalAlignment(HAlign_Center);
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		UHorizontalBox* LossGrid = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LossGrid"));
		LossGrid->bIsVariable = true;
		if (UVerticalBoxSlot* GridSlot = LossPanel->AddChildToVerticalBox(LossGrid))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		UTextBlock* LossMore = MakeText(Tree, Roboto, TEXT("LossMoreText"),
			TEXT("и ещё 3 предметов"), FLinearColor(0.8f, 0.8f, 0.8f, 1.0f), 14, TEXT("Regular"));
		LossMore->bIsVariable = true;
		LossMore->SetVisibility(ESlateVisibility::Collapsed); // видимость ведёт код
		if (UVerticalBoxSlot* MoreSlot = LossPanel->AddChildToVerticalBox(LossMore))
		{
			MoreSlot->SetHorizontalAlignment(HAlign_Center);
			MoreSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		UTextBlock* LossMoney = MakeText(Tree, Roboto, TEXT("LossMoneyText"),
			TEXT("−0 монет"), LossColor, 16, TEXT("Bold"));
		LossMoney->bIsVariable = true;
		if (UVerticalBoxSlot* LossMoneySlot = LossPanel->AddChildToVerticalBox(LossMoney))
		{
			LossMoneySlot->SetHorizontalAlignment(HAlign_Center);
		}

		PlaceCentered(LossPanel, 132.0f);
		LockPanelChildrenInDesigner(LossPanel);

		// --- Золотая кнопка «Спасти рюкзак» (Build 1.2, ТЗ №1): единый золотой цвет
		// rewarded-кнопок (раздел 0 п.9), иконка видео + заголовок + живая подстрока с
		// конкретной выгодой (подстроку пишет код). Начинка замкнута.
		UButton* SaveBackpack = MakeStyledButton(Tree, TEXT("SaveBackpackButton"),
			FLinearColor(0.85f, 0.62f, 0.14f, 1.0f), FLinearColor(0.95f, 0.72f, 0.2f, 1.0f),
			FLinearColor(1.0f, 0.8f, 0.3f, 1.0f)); // тёплое золото
		UHorizontalBox* SaveRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SaveBackpackRow"));

		// Иконка видео (треугольник в скруглённом квадрате, единая для всех rewarded-кнопок).
		// Текстура белая с альфой — на золотой кнопке тонируется тёмным для контраста.
		UImage* SaveIcon = MakeIcon(Tree, TEXT("WBP_Death"), TEXT("SaveBackpackIcon"),
			TEXT("/Game/UI/Icons/T_Icon_AdVideo.T_Icon_AdVideo"), 30.0f);
		SaveIcon->SetColorAndOpacity(FLinearColor(0.1f, 0.08f, 0.03f, 1.0f));
		if (UHorizontalBoxSlot* IconSlot = SaveRow->AddChildToHorizontalBox(SaveIcon))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		}

		UVerticalBox* SaveLabelBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SaveBackpackLabelBox"));
		const FLinearColor GoldTextColor(0.1f, 0.08f, 0.03f, 1.0f);
		if (UVerticalBoxSlot* CaptionSlot = SaveLabelBox->AddChildToVerticalBox(
			MakeText(Tree, Roboto, TEXT("SaveBackpackLabel"), TEXT("СПАСТИ РЮКЗАК"),
				GoldTextColor, 20, TEXT("Bold"))))
		{
			CaptionSlot->SetHorizontalAlignment(HAlign_Center);
		}
		UTextBlock* SaveSub = MakeText(Tree, Roboto, TEXT("SaveBackpackSubText"),
			TEXT("Сохранить 6 предм. и 90 монет — за просмотр ролика"),
			GoldTextColor, 12, TEXT("Regular"));
		SaveSub->bIsVariable = true;
		if (UVerticalBoxSlot* SubSlot = SaveLabelBox->AddChildToVerticalBox(SaveSub))
		{
			SubSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UHorizontalBoxSlot* LabelBoxSlot = SaveRow->AddChildToHorizontalBox(SaveLabelBox))
		{
			LabelBoxSlot->SetVerticalAlignment(VAlign_Center);
		}
		SetButtonContent(SaveBackpack, SaveRow);
		if (UCanvasPanelSlot* SaveSlot = Root->AddChildToCanvas(SaveBackpack))
		{
			SaveSlot->SetAnchors(FAnchors(0.5f, 0.45f, 0.5f, 0.45f));
			SaveSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			SaveSlot->SetPosition(FVector2D(0.0f, 252.0f));
			SaveSlot->SetSize(FVector2D(380.0f, 64.0f));
		}

		// Кнопка возрождения 380x64 — НЕЙТРАЛЬНАЯ СЕРАЯ того же размера прямо под золотой
		// (ТЗ №1 раздел 2 п.4: отказ не спрятан; прежняя зелёная уступила серой). Заголовок
		// статичный, подстрока живая (числа потерь пишет код). Начинка замкнута.
		UButton* Respawn = MakeStyledButton(Tree, TEXT("RespawnButton"),
			FLinearColor(0.3f, 0.3f, 0.34f, 1.0f), FLinearColor(0.4f, 0.4f, 0.45f, 1.0f),
			FLinearColor(0.5f, 0.5f, 0.55f, 1.0f));
		UVerticalBox* RespawnBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RespawnLabelBox"));
		if (UVerticalBoxSlot* RespawnCaptionSlot = RespawnBox->AddChildToVerticalBox(
			MakeText(Tree, Roboto, TEXT("RespawnLabel"), TEXT("ВОЗРОДИТЬСЯ"),
				FLinearColor::White, 20, TEXT("Bold"))))
		{
			RespawnCaptionSlot->SetHorizontalAlignment(HAlign_Center);
		}
		UTextBlock* RespawnSub = MakeText(Tree, Roboto, TEXT("RespawnSubText"),
			TEXT("Потеряешь 7 предм. и 100 монет"),
			FLinearColor(0.85f, 0.85f, 0.85f, 1.0f), 12, TEXT("Regular"));
		RespawnSub->bIsVariable = true;
		if (UVerticalBoxSlot* RespawnSubSlot = RespawnBox->AddChildToVerticalBox(RespawnSub))
		{
			RespawnSubSlot->SetHorizontalAlignment(HAlign_Center);
		}
		SetButtonContent(Respawn, RespawnBox);
		if (UCanvasPanelSlot* BtnSlot = Root->AddChildToCanvas(Respawn))
		{
			BtnSlot->SetAnchors(FAnchors(0.5f, 0.45f, 0.5f, 0.45f));
			BtnSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			BtnSlot->SetPosition(FVector2D(0.0f, 324.0f));
			BtnSlot->SetSize(FVector2D(380.0f, 64.0f));
		}

		PlaceCentered(MakeText(Tree, Roboto, TEXT("KeyHintText"), TEXT("Enter / Пробел — возродиться"),
			FLinearColor(0.8f, 0.8f, 0.8f, 1.0f), 14, TEXT("Regular")), 398.0f);
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

	// Экран магазина — КАНВАС-ПЕРВЫЙ (ADR-051 п.1; механика и арифметика — как BuildInventory:
	// панель 960x600, отступ 16 -> область 928x568, доли колонок 0.52/0.48). Подпись «Монеты»
	// и подпись «Количество» слайдера — отдельные кубики (догон ADR-050: код пишет в
	// MoneyText/SliderQtyText только значение). Панель количества — поверх, на время сделки.
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

		UCanvasPanel* PanelCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
		Panel->SetContent(PanelCanvas);

		// Шапка: заголовок слева (статичный текст Рината), закрытие 90x28 в правом-верхнем углу.
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("HeaderText"),
			TEXT("Торговец"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 22, TEXT("Bold")),
			FVector2D(0.0f, 0.0f));

		UButton* Close = MakeStyledButton(Tree, TEXT("CloseButton"),
			FLinearColor(0.5f, 0.12f, 0.12f, 1.0f), FLinearColor(0.62f, 0.17f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f)); // InvDropColor
		SetButtonContent(Close, MakeText(Tree, Roboto, TEXT("CloseLabel"), TEXT("Закрыть"),
			FLinearColor::White, 14, TEXT("Regular")));
		if (UCanvasPanelSlot* CloseSlot = PanelCanvas->AddChildToCanvas(Close))
		{
			CloseSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
			CloseSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			CloseSlot->SetPosition(FVector2D::ZeroVector);
			CloseSlot->SetSize(FVector2D(90.0f, 28.0f)); // ShopCloseButtonWidth/Height
		}

		// Деньги игрока: статичная подпись «Монеты» (код её НЕ трогает) + значение, которое
		// код обновляет каждый кадр (ADR-050) — отдельные кубики, двигаются порознь.
		const FLinearColor ShopMoneyColor(1.0f, 0.85f, 0.2f, 1.0f);
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("MoneyLabel"),
			TEXT("Монеты"), ShopMoneyColor, 16, TEXT("Regular")), FVector2D(0.0f, 34.0f));
		UTextBlock* Money = MakeText(Tree, Roboto, TEXT("MoneyText"), TEXT("0"),
			ShopMoneyColor, 16, TEXT("Regular"));
		Money->bIsVariable = true;
		CanvasAuto(PanelCanvas, Money, FVector2D(66.0f, 34.0f));

		// Колонки прежними долями (0.52 каталог / 0.48 рюкзак — ShopLeftColumnFrac);
		// спискам — якоря-РАСТЯЖКА до низа панели: при ресайзе панели тянутся следом.
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("BuyHeaderText"),
			TEXT("Товары торговца"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")),
			FVector2D(0.0f, 65.0f));
		UScrollBox* Buy = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BuyList"));
		Buy->bIsVariable = true;
		CanvasStretch(PanelCanvas, Buy, FAnchors(0.0f, 0.0f, 0.52f, 1.0f),
			FMargin(0.0f, 92.0f, 12.0f, 0.0f));

		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("SellHeaderText"),
			TEXT("Рюкзак"), FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 18, TEXT("Bold")),
			FVector2D(0.0f, 65.0f), FAnchors(0.52f, 0.0f, 0.52f, 0.0f));
		UScrollBox* Sell = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("SellList"));
		Sell->bIsVariable = true;
		CanvasStretch(PanelCanvas, Sell, FAnchors(0.52f, 0.0f, 1.0f, 1.0f),
			FMargin(0.0f, 92.0f, 0.0f, 0.0f));

		// Панель количества 600x260 (SliderPanelMaxWidth/Height) — последний ребёнок канвы,
		// рисуется поверх; в ассете сразу Collapsed (Visible/Collapsed переключает код).
		// Внутри — свой канвас (564x224 за вычетом отступа 18). Разметка — в варианте
		// «строка пересчёта патронов ВИДНА»: на канвасе скрытый ряд места не освобождает,
		// поэтому при покупке НЕ патронов между строкой количества и ползунком остаётся
		// пустой зазор (в контейнерном виде ползунок подъезжал вверх).
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

		UCanvasPanel* SliderCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SliderCanvas"));
		SliderPanel->SetContent(SliderCanvas);

		UTextBlock* SliderTitle = MakeText(Tree, Roboto, TEXT("SliderTitleText"), TEXT("Купить: —"),
			FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 20, TEXT("Bold"));
		SliderTitle->bIsVariable = true;
		CanvasAuto(SliderCanvas, SliderTitle, FVector2D(0.0f, 0.0f));

		// Количество: статичная подпись + значение разными кубиками (ADR-050).
		const FLinearColor SliderQtyColor(1.0f, 0.97f, 0.7f, 1.0f);
		CanvasAuto(SliderCanvas, MakeText(Tree, Roboto, TEXT("SliderQtyLabel"),
			TEXT("Количество"), SliderQtyColor, 18, TEXT("Regular")), FVector2D(0.0f, 32.0f));
		UTextBlock* SliderQty = MakeText(Tree, Roboto, TEXT("SliderQtyText"), TEXT("1 из 1"),
			SliderQtyColor, 18, TEXT("Regular"));
		SliderQty->bIsVariable = true;
		CanvasAuto(SliderCanvas, SliderQty, FVector2D(118.0f, 32.0f));

		// Пересчёт пачек в патроны: показывается ТОЛЬКО при покупке патронов. Прячется
		// ЦЕЛИКОМ контейнер SliderQtyAmmoRow — вместе с подписью, иначе она висела бы при
		// покупке аптечки (ловушка скрытия, ADR-050). В ассете сразу Collapsed.
		UHorizontalBox* AmmoRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("SliderQtyAmmoRow"));
		AmmoRow->SetVisibility(ESlateVisibility::Collapsed);
		AmmoRow->bIsVariable = true;
		UTextBlock* SliderQtyAmmo = MakeText(Tree, Roboto, TEXT("SliderQtyAmmoText"),
			TEXT("всего 30 патронов"), SliderQtyColor, 16, TEXT("Regular"));
		SliderQtyAmmo->bIsVariable = true;
		AmmoRow->AddChildToHorizontalBox(SliderQtyAmmo);
		CanvasAuto(SliderCanvas, AmmoRow, FVector2D(0.0f, 57.0f));

		// Ползунок: диапазон/шаг выставляет код при каждой транзакции — тут только кубик.
		// По ширине — растяжка (при ресайзе панели тянется), высота фиксированная.
		USlider* Qty = Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("QtySlider"));
		Qty->bIsVariable = true;
		CanvasStretch(SliderCanvas, Qty, FAnchors(0.0f, 0.0f, 1.0f, 0.0f),
			FMargin(0.0f, 86.0f, 0.0f, 16.0f));

		// [-] [+] (48x30 — SliderSmallButtonWidth/Height) и живой итог справа.
		auto AddSmallButton = [&](const TCHAR* ButtonName, const TCHAR* LabelName,
			const TCHAR* Caption, float X)
		{
			UButton* Small = MakeStyledButton(Tree, FName(ButtonName),
				FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
				FLinearColor(0.25f, 0.27f, 0.33f, 1.0f)); // InvSlotColor + подсветки
			SetButtonContent(Small, MakeText(Tree, Roboto, FName(LabelName), Caption,
				FLinearColor::White, 15, TEXT("Bold")));
			CanvasAt(SliderCanvas, Small, FVector2D(X, 114.0f), FVector2D(48.0f, 30.0f));
		};
		AddSmallButton(TEXT("QtyMinusButton"), TEXT("QtyMinusLabel"), TEXT("-"), 0.0f);
		AddSmallButton(TEXT("QtyPlusButton"), TEXT("QtyPlusLabel"), TEXT("+"), 56.0f);

		UTextBlock* SliderTotal = MakeText(Tree, Roboto, TEXT("SliderTotalText"), TEXT("Итого: 0"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 19, TEXT("Regular")); // UIMoneyColor
		SliderTotal->bIsVariable = true;
		CanvasAuto(SliderCanvas, SliderTotal, FVector2D(128.0f, 119.0f));

		// Подтверждение (Отмена красная, Подтвердить зелёная, 120x34) — якоря низ-право:
		// при ресайзе панели мышкой кнопки остаются в углу.
		auto AddBigButton = [&](const TCHAR* ButtonName, const TCHAR* LabelName, const TCHAR* Caption,
			const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed, float RightX)
		{
			UButton* Big = MakeStyledButton(Tree, FName(ButtonName), Normal, Hovered, Pressed);
			SetButtonContent(Big, MakeText(Tree, Roboto, FName(LabelName), Caption,
				FLinearColor::White, 15, TEXT("Regular")));
			if (UCanvasPanelSlot* BigSlot = SliderCanvas->AddChildToCanvas(Big))
			{
				BigSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
				BigSlot->SetAlignment(FVector2D(1.0f, 1.0f));
				BigSlot->SetPosition(FVector2D(RightX, -32.0f));
				BigSlot->SetSize(FVector2D(120.0f, 34.0f)); // SliderBigButtonWidth/Height
			}
		};
		AddBigButton(TEXT("SliderCancelButton"), TEXT("SliderCancelLabel"), TEXT("Отмена"),
			FLinearColor(0.5f, 0.12f, 0.12f, 1.0f), FLinearColor(0.62f, 0.17f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f), -130.0f);
		AddBigButton(TEXT("SliderConfirmButton"), TEXT("SliderConfirmLabel"), TEXT("Подтвердить"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f), 0.0f);
		return true;
	}

	// ======================================================================
	// Диалог старосты: WBP_Dialog (вид = Canvas DrawDialog — нижняя панель новеллы)
	// ======================================================================

	// КАНВАС-ПЕРВЫЙ (ADR-051 п.1, волна «двигать мышкой все окна» 07-28): панель, имя NPC,
	// реплика и каждая кнопка-ответ — в СВОЁМ канвас-слоте с ручками; прежние коробки
	// (PanelSize/DialogBox/ButtonsRow/SizeBox-обёртки кнопок) убраны — их слоты ручек не
	// дают. Цена канваса: (1) панель больше НЕ растёт от длинной реплики (раньше
	// SizeBox+AutoSize) — высота фиксированная 280, реплика переносит строки внутри
	// растяжки, не влезет — владелец растянет панель мышкой; (2) скрытая кодом кнопка
	// оставляет своё место пустым, но наборы кнопок по состояниям квеста не пересекаются
	// (вместе видны только Принять+Отказаться — они соседи), так что дыр между ВИДИМЫМИ
	// кнопками не бывает. Подписи кнопок — AcceptText/DeclineText/TurnInText/CloseText:
	// ТОЧНЫЕ имена привязок DialogScreenWidget.h, эта функция задаёт их сразу при постройке
	// (переименовывать подписи задним числом больше нечем и незачем).
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

		// Панель низ-центр 900x280 (DialogPanelMaxWidth / DialogMinPanelHeight), отступ от
		// низа 40 (DialogBottomMargin) — Border сразу в канвас-слоте, тянется мышкой.
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
		Panel->SetBrush(MakeRoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.95f), 6.0f,
			FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
		Panel->SetPadding(FMargin(18.0f)); // DialogPadding
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			PanelSlot->SetPosition(FVector2D(0.0f, -40.0f));
			PanelSlot->SetSize(FVector2D(900.0f, 280.0f));
		}

		UCanvasPanel* PanelCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
		Panel->SetContent(PanelCanvas);

		// Имя NPC — голубой DialogNameColor (текст ставит код с поля старосты).
		UTextBlock* NpcName = MakeText(Tree, Roboto, TEXT("NPCNameText"), TEXT("СТАРОСТА"),
			FLinearColor(0.65f, 0.88f, 1.0f, 1.0f), 22, TEXT("Bold"));
		NpcName->bIsVariable = true;
		CanvasAuto(PanelCanvas, NpcName, FVector2D(0.0f, 0.0f));

		// Реплика — растяжка между именем (высота строки ~26 при 22pt + зазор 10) и рядом
		// кнопок (40 + зазор 16): при ресайзе панели мышкой тянется следом. Перенос строк
		// обязателен (guide) — длинные квестовые описания многострочные.
		UTextBlock* Replica = MakeText(Tree, Roboto, TEXT("ReplicaText"), TEXT("Реплика старосты."),
			FLinearColor::White, 16, TEXT("Regular"));
		Replica->SetAutoWrapText(true);
		Replica->bIsVariable = true;
		CanvasStretch(PanelCanvas, Replica, FAnchors(0.0f, 0.0f, 1.0f, 1.0f),
			FMargin(0.0f, 36.0f, 0.0f, 56.0f));

		// Кнопка-ответ в своём канвас-слоте у НИЗА панели (якорь низ-лево — при ресайзе
		// панели кнопки остаются у нижней кромки). Высота 40 (DialogButtonHeight); ширины
		// 200/200/240/200 с зазором 8 — ровно внутренняя ширина панели 864. Лишние по
		// состоянию квеста кнопки код прячет сам (Collapsed). Подпись кладётся замком
		// (SetButtonContent): клик в дизайнере выделяет кнопку целиком, у неё ручки.
		auto AddAnswer = [&](UButton* Button, float X, float Width)
		{
			if (UCanvasPanelSlot* AnswerSlot = PanelCanvas->AddChildToCanvas(Button))
			{
				AnswerSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
				AnswerSlot->SetAlignment(FVector2D(0.0f, 1.0f));
				AnswerSlot->SetPosition(FVector2D(X, 0.0f));
				AnswerSlot->SetSize(FVector2D(Width, 40.0f));
			}
		};

		// [Принять] — зелёная (InvSlotFilledColor); подпись ставит КОД (реплика героя из
		// квеста / кнопка текущей реплики интро), образец — только для дизайнера.
		UButton* Accept = MakeStyledButton(Tree, TEXT("AcceptButton"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f));
		UTextBlock* AcceptCaption = MakeText(Tree, Roboto, TEXT("AcceptText"), TEXT("Взяться за дело"),
			FLinearColor::White, 15, TEXT("Regular"));
		AcceptCaption->bIsVariable = true;
		SetButtonContent(Accept, AcceptCaption);
		AddAnswer(Accept, 0.0f, 200.0f); // DialogButtonWidth

		// [Отказаться] — красная (InvDropColor); подпись ставит код (CloseReplyText квеста).
		UButton* Decline = MakeStyledButton(Tree, TEXT("DeclineButton"),
			FLinearColor(0.5f, 0.12f, 0.12f, 1.0f), FLinearColor(0.62f, 0.17f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f));
		UTextBlock* DeclineCaption = MakeText(Tree, Roboto, TEXT("DeclineText"), TEXT("Отказаться"),
			FLinearColor::White, 15, TEXT("Regular"));
		DeclineCaption->bIsVariable = true;
		SetButtonContent(Decline, DeclineCaption);
		AddAnswer(Decline, 208.0f, 200.0f);

		// [Сдать (+N)] — зелёная, ШИРЕ (240 — DialogTurnInButtonWidth); подпись ставит КОД
		// через кубик TurnInText (в ней сумма награды).
		UButton* TurnIn = MakeStyledButton(Tree, TEXT("TurnInButton"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f));
		UTextBlock* TurnInCaption = MakeText(Tree, Roboto, TEXT("TurnInText"), TEXT("Сдать (+0)"),
			FLinearColor::White, 15, TEXT("Regular"));
		TurnInCaption->bIsVariable = true;
		SetButtonContent(TurnIn, TurnInCaption);
		AddAnswer(TurnIn, 416.0f, 240.0f);

		// [Закрыть] — серая (InvSlotColor); подпись ставит код (CloseReplyText квеста).
		UButton* CloseBtn = MakeStyledButton(Tree, TEXT("CloseButton"),
			FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
			FLinearColor(0.25f, 0.27f, 0.33f, 1.0f));
		UTextBlock* CloseCaption = MakeText(Tree, Roboto, TEXT("CloseText"), TEXT("Закрыть"),
			FLinearColor::White, 15, TEXT("Regular"));
		CloseCaption->bIsVariable = true;
		SetButtonContent(CloseBtn, CloseCaption);
		AddAnswer(CloseBtn, 664.0f, 200.0f);
		return true;
	}

	// ======================================================================
	// Постоянная панель статов: WBP_PlayerStats (вид = Canvas DrawPlayerStats, верх-лево)
	// ======================================================================

	// Полоска стата: Overlay в СВОЁМ канвас-слоте -> ProgressBar растяжкой + текст поверх.
	// Build 1.2.1 (Д1, Ринат: «Не могу настроить размер полосок... Разлочь мне этот umg»):
	// прежний SizeBox с жёсткими Width/Height УБРАН — размер полоски задаёт КАНВАС-СЛОТ
	// Overlay'я (явный, не авто), бар заполняет Overlay растяжкой, и ручки слота в
	// дизайнере реально меняют размер полоски. Подпись — не переменная: код её не биндит
	// и не переписывает, Ринат правит текст и стиль сам (ADR-050). Значение — кубик кода.
	UOverlay* MakeStatBar(UWidgetTree* Tree, UObject* Roboto, const TCHAR* BarName,
		const TCHAR* ValueName, const FString& LabelCaption, const FString& ValueSample,
		const FLinearColor& FillColor)
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

		if (UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Bar))
		{
			// Растяжка на весь Overlay: его габарит диктует канвас-слот (ручки дизайнера).
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Fill);
		}

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
	// Build 1.2.1 (Д1, просьба Рината): полоски здоровья/еды/воды — КАЖДАЯ в СВОЁМ
	// канвас-слоте с ЯВНЫМ размером (ручки ресайза в дизайнере реально меняют размер
	// полоски), иконки — отдельные канвас-слоты рядом (авторазмер, таскаются мышкой).
	// Прежние ряды-коробки (HealthRow и родня, HorizontalBox) убраны: слот коробки ручек
	// полоске не давал, а SizeBox внутри жёстко держал габарит. Начинка полоски (бар,
	// подпись, значение) замкнута — клик в дизайнере выделяет полоску целиком.
	//
	// Геометрия прежнего столба: старт 24,24 (PlayerHudMarginX/Y), полоски 28/24/24 px,
	// Y: 24, 58, 86, 118 (патроны), 146 (деньги); иконка 24 px, зазор 6 -> полоска X=54.
	// Эти числа — для СВЕЖЕЙ генерации; при -rebuild геометрия снимается со старого
	// дерева владельца (ConvertPlayerStatsRowsToBarSlots — в реальном ассете иконки 32 px).
	bool BuildPlayerStats(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Панель живёт на экране всю игру: раньше тапы/клики в мир пропускал один зонтик
		// HitTestInvisible на столбе, теперь зонтик у КАЖДОГО верхнеуровневого элемента
		// (HitTestInvisible гасит хит-тест себе И всему поддереву — Visibility.h:22).
		auto PlaceTopLevel = [Root](UWidget* Widget, float Y)
		{
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			CanvasAuto(Root, Widget, FVector2D(24.0f, Y));
		};

		// Иконка стата: свой канвас-слот, авторазмер (габарит даёт кисть 24 px).
		auto PlaceStatIcon = [&](const FName& IconName, const TCHAR* TexturePath, const FVector2D& Pos)
		{
			UImage* Icon = MakeIcon(Tree, TEXT("WBP_PlayerStats"), IconName, TexturePath, 24.0f);
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			CanvasAuto(Root, Icon, Pos);
		};

		// Полоска стата: свой канвас-слот с явным размером (ручки ресайза), начинка замкнута.
		auto PlaceStatBar = [&](UOverlay* BarOverlay, const FVector2D& Pos, const FVector2D& Size)
		{
			BarOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
			CanvasAt(Root, BarOverlay, Pos, Size);
			LockPanelChildrenInDesigner(BarOverlay);
		};

		// Здоровье: полоска 320x28 (PlayerHealthBarWidth, PlayerHealthFillColor);
		// иконка 24 px центрируется по высоте полоски (+2).
		PlaceStatIcon(TEXT("HealthIcon"), TEXT("/Game/UI/Icons/T_Icon_Health.T_Icon_Health"),
			FVector2D(24.0f, 26.0f));
		PlaceStatBar(MakeStatBar(Tree, Roboto, TEXT("HealthBar"), TEXT("HealthText"),
				TEXT("Здоровье"), TEXT("100/100"), FLinearColor(0.85f, 0.1f, 0.1f, 0.95f)),
			FVector2D(54.0f, 24.0f), FVector2D(320.0f, 28.0f));

		// Голод и жажда: полоски 224x24 — 0.7 длины здоровья (решение владельца,
		// перенесено из доводки -augment).
		PlaceStatIcon(TEXT("HungerIcon"), TEXT("/Game/UI/Icons/T_Icon_Hunger.T_Icon_Hunger"),
			FVector2D(24.0f, 58.0f));
		PlaceStatBar(MakeStatBar(Tree, Roboto, TEXT("HungerBar"), TEXT("HungerText"),
				TEXT("Голод"), TEXT("100"), FLinearColor(0.85f, 0.55f, 0.1f, 0.95f)), // HungerColor
			FVector2D(54.0f, 58.0f), FVector2D(224.0f, 24.0f));

		PlaceStatIcon(TEXT("ThirstIcon"), TEXT("/Game/UI/Icons/T_Icon_Thirst.T_Icon_Thirst"),
			FVector2D(24.0f, 86.0f));
		PlaceStatBar(MakeStatBar(Tree, Roboto, TEXT("ThirstBar"), TEXT("ThirstText"),
				TEXT("Жажда"), TEXT("100"), FLinearColor(0.15f, 0.55f, 0.9f, 0.95f)), // ThirstColor
			FVector2D(54.0f, 86.0f), FVector2D(224.0f, 24.0f));

		// Патроны: код показывает строку только с огнестрелом в руках. Прячется ЦЕЛИКОМ
		// контейнер AmmoRow — вместе с подписью «Патроны», иначе подпись висела бы одна
		// при ноже в руках (ловушка скрытия, ADR-050). В ассете сразу Collapsed, поэтому
		// PlaceTopLevel сюда не годится — он перетёр бы Collapsed зонтиком. Показывая ряд,
		// код ставит ему SelfHitTestInvisible (PlayerStatsWidget.cpp:62-63): сам ряд хиты
		// не ловит, но детей это НЕ укрывает — детям HitTestInvisible прописан явно.
		const FLinearColor AmmoColor(0.95f, 0.95f, 0.95f, 1.0f);
		UHorizontalBox* AmmoRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("AmmoRow"));
		AmmoRow->SetVisibility(ESlateVisibility::Collapsed);
		AmmoRow->bIsVariable = true;

		UTextBlock* AmmoLabel = MakeText(Tree, Roboto, TEXT("AmmoLabel"), TEXT("Патроны"),
			AmmoColor, 14, TEXT("Regular"));
		ApplyTextShadow(AmmoLabel);
		AmmoLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		AmmoRow->AddChildToHorizontalBox(AmmoLabel);

		UTextBlock* Ammo = MakeText(Tree, Roboto, TEXT("AmmoText"), TEXT("7 / 51"),
			AmmoColor, 14, TEXT("Regular"));
		ApplyTextShadow(Ammo);
		Ammo->SetVisibility(ESlateVisibility::HitTestInvisible); // рантайм-видимость ставит код
		Ammo->bIsVariable = true;
		if (UHorizontalBoxSlot* AmmoValueSlot = AmmoRow->AddChildToHorizontalBox(Ammo))
		{
			AmmoValueSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		}
		CanvasAuto(Root, AmmoRow, FVector2D(24.0f, 118.0f));
		LockPanelChildrenInDesigner(AmmoRow);

		// Деньги — золотые на тёмной плашке (MoneyPlateColor): иконка + подпись + значение.
		const FLinearColor MoneyColor(1.0f, 0.85f, 0.2f, 1.0f); // PlayerMoneyColor
		UBorder* MoneyPlate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MoneyPlate"));
		MoneyPlate->SetBrush(MakeRoundedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), 3.0f));
		MoneyPlate->SetPadding(FMargin(8.0f, 3.0f));

		UHorizontalBox* MoneyRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("MoneyRow"));
		if (UHorizontalBoxSlot* MoneyIconSlot = MoneyRow->AddChildToHorizontalBox(
			MakeIcon(Tree, TEXT("WBP_PlayerStats"), TEXT("MoneyIcon"),
				TEXT("/Game/UI/Icons/T_Icon_Money.T_Icon_Money"), 22.0f)))
		{
			MoneyIconSlot->SetVerticalAlignment(VAlign_Center);
			MoneyIconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		}

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
		PlaceTopLevel(MoneyPlate, 146.0f);
		LockSubtreeInDesigner(MoneyRow); // вся начинка плашки; сама плашка свободна
		return true;
	}

	// ======================================================================
	// Плашка конца сюжета: WBP_EndOfStory (Build 1.2.1, ТЗ Д2)
	// ======================================================================

	// Канвас-первая: подложка, сообщение, строка-статус и обе кнопки — каждый в СВОЁМ
	// канвас-слоте с якорем верх-центр (у кнопок ручки; плашка компактная, экран не
	// закрывает — геометрия повторяет кодовый вид FEndOfStoryStyle: ширина 620, Y=110).
	// Имена кубиков = BindWidgetOptional-полям UEndOfStoryWidget; тексты в игре ставит
	// InitContent (дословный текст Рината живёт EditAnywhere на HUD) — здесь образцы.
	bool BuildEndOfStory(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Канвас-слот с якорем верх-центр экрана (доли 0.5/0.0), позиция = смещение от якоря.
		auto TopCenter = [Root](UWidget* Widget, const FVector2D& Pos, const FVector2D& Size,
			const FVector2D& Alignment)
		{
			if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget))
			{
				Slot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
				Slot->SetAlignment(Alignment);
				Slot->SetPosition(Pos);
				Slot->SetSize(Size);
			}
		};

		// Подложка (дефолты кодового стиля: чёрная 0.8, скругление 6).
		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Plate"));
		Plate->SetBrush(MakeRoundedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f), 6.0f));
		Plate->bIsVariable = true;
		TopCenter(Plate, FVector2D(0.0f, 110.0f), FVector2D(620.0f, 158.0f), FVector2D(0.5f, 0.0f));

		// Сообщение (образец; живой текст ставит код).
		UTextBlock* Message = MakeText(Tree, Roboto, TEXT("MessageText"),
			TEXT("Здесь заканчивается сюжет текущей версии игры."),
			FLinearColor(0.95f, 0.95f, 0.95f, 1.0f), 15, TEXT("Regular"));
		Message->SetAutoWrapText(true);
		Message->bIsVariable = true;
		TopCenter(Message, FVector2D(0.0f, 126.0f), FVector2D(588.0f, 56.0f), FVector2D(0.5f, 0.0f));

		// Строка-статус «Канал скоро появится»: скрыта, показывает код по [Написать мне]
		// при пустой ссылке (Collapsed в ассете — как AmmoRow панели статов).
		UTextBlock* Status = MakeText(Tree, Roboto, TEXT("StatusText"), TEXT("Канал скоро появится"),
			FLinearColor(1.0f, 0.85f, 0.3f, 1.0f), 14, TEXT("Bold"));
		Status->SetJustification(ETextJustify::Center);
		Status->SetVisibility(ESlateVisibility::Collapsed);
		Status->bIsVariable = true;
		TopCenter(Status, FVector2D(0.0f, 186.0f), FVector2D(588.0f, 22.0f), FVector2D(0.5f, 0.0f));

		// Кнопки: [Написать мне] слева от центра, [Играть дальше] справа; подписи — кубики
		// кода, замкнуты внутри кнопок (клик в дизайнере выделяет кнопку с ручками).
		const FLinearColor BtnNormal(0.25f, 0.28f, 0.33f, 1.0f); // ButtonColor кодового стиля
		const FLinearColor BtnHovered(0.32f, 0.36f, 0.42f, 1.0f);
		const FLinearColor BtnPressed(0.18f, 0.20f, 0.24f, 1.0f);
		const FLinearColor BtnText(0.95f, 0.96f, 1.0f, 1.0f);

		UButton* Write = MakeStyledButton(Tree, TEXT("WriteButton"), BtnNormal, BtnHovered, BtnPressed);
		UTextBlock* WriteLabel = MakeText(Tree, Roboto, TEXT("WriteButtonText"),
			TEXT("Написать мне"), BtnText, 14, TEXT("Bold"));
		WriteLabel->SetJustification(ETextJustify::Center);
		WriteLabel->bIsVariable = true;
		SetButtonContent(Write, WriteLabel);
		TopCenter(Write, FVector2D(-8.0f, 214.0f), FVector2D(220.0f, 40.0f), FVector2D(1.0f, 0.0f));

		UButton* Play = MakeStyledButton(Tree, TEXT("PlayButton"), BtnNormal, BtnHovered, BtnPressed);
		UTextBlock* PlayLabel = MakeText(Tree, Roboto, TEXT("PlayButtonText"),
			TEXT("Играть дальше"), BtnText, 14, TEXT("Bold"));
		PlayLabel->SetJustification(ETextJustify::Center);
		PlayLabel->bIsVariable = true;
		SetButtonContent(Play, PlayLabel);
		TopCenter(Play, FVector2D(8.0f, 214.0f), FVector2D(200.0f, 40.0f), FVector2D(0.0f, 0.0f));

		return true;
	}

	// ======================================================================
	// Окно обыска трупа: WBP_CorpseLoot (Build 1.2.1, ТЗ А1; код окна — UCorpseLootWidget)
	// ======================================================================

	// Канвас-схема «похоже на экран торговли» (Ринат): затемнение, центральная панель с
	// золотой рамкой, заголовок, крестик, список лута растяжкой, «Забрать всё» внизу.
	// Тексты кубиков ставит код окна (TitleLabel/TakeAllCaption/CloseCaption — Class
	// Defaults ассета); строки списка создаёт код классом RowWidgetClass (кодовое дерево).
	bool BuildCorpseLoot(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Затемнение на весь экран; Visible — клики в мир не проходят (модалка).
		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Dim->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
		}

		// Центральная панель 640x520 (уже инвентаря: один список) с золотой рамкой;
		// внутри собственный канвас, содержимое в области за вычетом отступа 16 (608x488).
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
		Panel->SetBrush(MakeRoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.95f), 6.0f,
			FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
		Panel->SetPadding(FMargin(16.0f));
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetPosition(FVector2D::ZeroVector);
			PanelSlot->SetSize(FVector2D(640.0f, 520.0f));
		}

		UCanvasPanel* PanelCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
		Panel->SetContent(PanelCanvas);

		// Заголовок (живой текст ставит код: TitleLabel из Class Defaults).
		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"), TEXT("Обыск трупа"),
			FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 20, TEXT("Bold"));
		Title->bIsVariable = true;
		CanvasAuto(PanelCanvas, Title, FVector2D(0.0f, 0.0f));

		// Крестик (верх-право; Esc ведёт контроллер). Подпись — кубик кода, замкнута.
		UButton* Close = MakeStyledButton(Tree, TEXT("CloseButton"),
			FLinearColor(0.3f, 0.3f, 0.34f, 1.0f), FLinearColor(0.4f, 0.4f, 0.45f, 1.0f),
			FLinearColor(0.22f, 0.22f, 0.26f, 1.0f));
		UTextBlock* CloseLabel = MakeText(Tree, Roboto, TEXT("CloseText"), TEXT("X"),
			FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 14, TEXT("Bold"));
		CloseLabel->SetJustification(ETextJustify::Center);
		CloseLabel->bIsVariable = true;
		SetButtonContent(Close, CloseLabel);
		if (UCanvasPanelSlot* CloseSlot = PanelCanvas->AddChildToCanvas(Close))
		{
			CloseSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
			CloseSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			CloseSlot->SetPosition(FVector2D(0.0f, 0.0f));
			CloseSlot->SetSize(FVector2D(36.0f, 32.0f));
		}

		// Список лута (деньги + предметы одним списком) — растяжка: при ресайзе панели
		// мышкой тянется следом; низ — над кнопкой «Забрать всё».
		UScrollBox* Loot = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LootList"));
		Loot->bIsVariable = true;
		CanvasStretch(PanelCanvas, Loot, FAnchors(0.0f, 0.0f, 1.0f, 1.0f),
			FMargin(0.0f, 44.0f, 0.0f, 60.0f));

		// «Забрать всё» (низ-право). Подпись — кубик кода (TakeAllCaption), замкнута.
		UButton* TakeAll = MakeStyledButton(Tree, TEXT("TakeAllButton"),
			FLinearColor(0.16f, 0.36f, 0.16f, 1.0f), FLinearColor(0.2f, 0.46f, 0.2f, 1.0f),
			FLinearColor(0.12f, 0.28f, 0.12f, 1.0f));
		UTextBlock* TakeAllLabel = MakeText(Tree, Roboto, TEXT("TakeAllText"), TEXT("Забрать всё"),
			FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 15, TEXT("Bold"));
		TakeAllLabel->SetJustification(ETextJustify::Center);
		TakeAllLabel->bIsVariable = true;
		SetButtonContent(TakeAll, TakeAllLabel);
		if (UCanvasPanelSlot* TakeAllSlot = PanelCanvas->AddChildToCanvas(TakeAll))
		{
			TakeAllSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
			TakeAllSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			TakeAllSlot->SetPosition(FVector2D(0.0f, 0.0f));
			TakeAllSlot->SetSize(FVector2D(220.0f, 44.0f));
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
			  TEXT("KillsText"), TEXT("MoneyLossText"), TEXT("RespawnButton"), TEXT("RespawnSubText"),
			  TEXT("LossPanel"), TEXT("LossGrid"), TEXT("LossMoreText"), TEXT("LossMoneyText"),
			  TEXT("SaveBackpackButton"), TEXT("SaveBackpackSubText") } },
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
			{ TEXT("NPCNameText"), TEXT("ReplicaText"),
			  TEXT("AcceptButton"), TEXT("AcceptText"), TEXT("DeclineButton"), TEXT("DeclineText"),
			  TEXT("TurnInButton"), TEXT("TurnInText"), TEXT("CloseButton"), TEXT("CloseText") } },
		{ TEXT("/Game/UI/WBP_PlayerStats"), TEXT("WBP_PlayerStats"),
			TEXT("/Script/ContrarySurvivor.PlayerStatsWidget"), &BuildPlayerStats,
			{ TEXT("HealthBar"), TEXT("HealthText"), TEXT("HungerBar"), TEXT("HungerText"),
			  TEXT("ThirstBar"), TEXT("ThirstText"), TEXT("AmmoRow"), TEXT("AmmoText"),
			  TEXT("MoneyText") } },
		// Build 1.2.1 (ТЗ Д2): плашка конца сюжета — кубики по BindWidgetOptional-полям
		// UEndOfStoryWidget (WidthBox в WBP нет: ширину даёт канвас-слот, поле остаётся null).
		{ TEXT("/Game/UI/WBP_EndOfStory"), TEXT("WBP_EndOfStory"),
			TEXT("/Script/ContrarySurvivor.EndOfStoryWidget"), &BuildEndOfStory,
			{ TEXT("Plate"), TEXT("MessageText"), TEXT("StatusText"),
			  TEXT("WriteButton"), TEXT("WriteButtonText"),
			  TEXT("PlayButton"), TEXT("PlayButtonText") } },
		// Build 1.2.1 (ТЗ А1): окно обыска трупа — кубики по BindWidgetOptional-полям
		// UCorpseLootWidget (строки списка создаёт код классом RowWidgetClass).
		{ TEXT("/Game/UI/WBP_CorpseLoot"), TEXT("WBP_CorpseLoot"),
			TEXT("/Script/ContrarySurvivor.CorpseLootWidget"), &BuildCorpseLoot,
			{ TEXT("TitleText"), TEXT("LootList"),
			  TEXT("TakeAllButton"), TEXT("TakeAllText"),
			  TEXT("CloseButton"), TEXT("CloseText") } },
	};

	FString ObjectPathOf(const FWbpSpec& Spec)
	{
		return FString::Printf(TEXT("%s.%s"), Spec.PackageName, Spec.AssetName);
	}

	// Контракт замков (см. SetButtonContent / LockPanelChildrenInDesigner): начинка
	// кнопок и рядов статичной раскладки обязана быть замкнута (клик в дизайнере
	// выделяет верхнеуровневый элемент целиком), сами верхнеуровневые элементы —
	// свободны (иначе их нельзя было бы выделить и тянуть). Проверяется в -verify:
	// это артефакт вместо ручного мышиного теста на каждый прогон.
	struct FLockContract
	{
		const TCHAR* AssetName;
		std::initializer_list<const TCHAR*> LockedContent;     // bLockedInDesigner == true
		std::initializer_list<const TCHAR*> SelectableWidgets; // bLockedInDesigner == false
	};

	const FLockContract GLockContracts[] =
	{
		{ TEXT("WBP_Inventory"),
			{ TEXT("HeadSlotButtonBox"), TEXT("HeadSlotButtonCaption"), TEXT("HeadSlotIcon"), TEXT("HeadSlotText"),
			  TEXT("TorsoSlotButtonBox"), TEXT("TorsoSlotButtonCaption"), TEXT("TorsoSlotIcon"), TEXT("TorsoSlotText"),
			  TEXT("LegsSlotButtonBox"), TEXT("LegsSlotButtonCaption"), TEXT("LegsSlotIcon"), TEXT("LegsSlotText"),
			  TEXT("CloseLabel") },
			{ TEXT("HeadSlotButton"), TEXT("TorsoSlotButton"), TEXT("LegsSlotButton"), TEXT("CloseButton") } },
		{ TEXT("WBP_Shop"),
			{ TEXT("CloseLabel"), TEXT("QtyMinusLabel"), TEXT("QtyPlusLabel"),
			  TEXT("SliderCancelLabel"), TEXT("SliderConfirmLabel") },
			{ TEXT("CloseButton"), TEXT("QtyMinusButton"), TEXT("QtyPlusButton"),
			  TEXT("SliderCancelButton"), TEXT("SliderConfirmButton") } },
		// Подписи-слова (HealthBarLabel и родня) в контракт НЕ входят: владелец удалил их
		// из своего ассета, перенос стилизации при -rebuild убирает их и из новой раскладки
		// (в свежесгенерированном ассете они есть и тоже замкнуты, но контракт проверяет
		// реальный ассет проекта).
		// Build 1.2.1 (Д1): полоски (Overlay) и иконки — СВОБОДНЫ (свои канвас-слоты с
		// ручками, Ринат их таскает и ресайзит); замкнута только начинка полосок
		// (бар/значение — клик выделяет полоску целиком). Рядов-коробок и SizeBox больше нет.
		{ TEXT("WBP_PlayerStats"),
			{ TEXT("HealthBar"), TEXT("HealthText"),
			  TEXT("HungerBar"), TEXT("HungerText"),
			  TEXT("ThirstBar"), TEXT("ThirstText"),
			  TEXT("AmmoText"), TEXT("MoneyRow"), TEXT("MoneyIcon"), TEXT("MoneyText") },
			{ TEXT("HealthIcon"), TEXT("HealthBarOverlay"),
			  TEXT("HungerIcon"), TEXT("HungerBarOverlay"),
			  TEXT("ThirstIcon"), TEXT("ThirstBarOverlay"),
			  TEXT("AmmoRow"), TEXT("MoneyPlate") } },
		{ TEXT("WBP_Dialog"),
			{ TEXT("AcceptText"), TEXT("DeclineText"), TEXT("TurnInText"), TEXT("CloseText") },
			{ TEXT("PanelPlate"), TEXT("NPCNameText"), TEXT("ReplicaText"),
			  TEXT("AcceptButton"), TEXT("DeclineButton"), TEXT("TurnInButton"), TEXT("CloseButton") } },
		// Build 1.2.1 (Д2): подписи кнопок замкнуты, кнопки/подложка/тексты свободны (ручки).
		{ TEXT("WBP_EndOfStory"),
			{ TEXT("WriteButtonText"), TEXT("PlayButtonText") },
			{ TEXT("Plate"), TEXT("MessageText"), TEXT("StatusText"),
			  TEXT("WriteButton"), TEXT("PlayButton") } },
		// Build 1.2.1 (А1): окно обыска — подписи кнопок замкнуты, остальное с ручками.
		{ TEXT("WBP_CorpseLoot"),
			{ TEXT("CloseText"), TEXT("TakeAllText") },
			{ TEXT("PanelPlate"), TEXT("TitleText"), TEXT("LootList"),
			  TEXT("CloseButton"), TEXT("TakeAllButton") } },
		{ TEXT("WBP_Death"),
			{ TEXT("LifetimeLabel"), TEXT("LifetimeText"), TEXT("KillerLabel"), TEXT("KillerText"),
			  TEXT("MoneyLabel"), TEXT("MoneyText"), TEXT("QuestsLabel"), TEXT("QuestsText"),
			  TEXT("KillsLabel"), TEXT("KillsText"), TEXT("RespawnLabel"), TEXT("RespawnSubText"),
			  TEXT("LossHeaderText"), TEXT("LossGrid"), TEXT("LossMoreText"), TEXT("LossMoneyText"),
			  TEXT("SaveBackpackIcon"), TEXT("SaveBackpackLabel"), TEXT("SaveBackpackSubText") },
			{ TEXT("TitleText"), TEXT("LifetimeTextRow"), TEXT("KillerTextRow"), TEXT("MoneyTextRow"),
			  TEXT("QuestsTextRow"), TEXT("KillsTextRow"), TEXT("RespawnLineText"), TEXT("MoneyLossText"),
			  TEXT("ConsumablesLineText"), TEXT("SavedLineText"), TEXT("LossPanel"),
			  TEXT("SaveBackpackButton"), TEXT("RespawnButton"), TEXT("KeyHintText") } },
	};

	// ======================================================================
	// РЕЖИМ ДОПОЛНЕНИЯ (-augment): точечная правка СУЩЕСТВУЮЩИХ ассетов
	// ======================================================================
	//
	// Зачем отдельный режим. В готовых панелях лежит ручная стилизация владельца (шрифты,
	// цвета, размеры, расположение): WBP_PlayerStats, WBP_TouchControls, а с коммита
	// 5c2b058 ещё WBP_Shop, WBP_Inventory и WBP_Dialog. Полная перегенерация (-force) её
	// уничтожает, поэтому для правки готовых панелей нужен путь, который дерево НЕ
	// пересобирает. (Пересборка -rebuild дерево меняет, но переносит значения владельца
	// на новое дерево — TransferOwnerStyle; это другой инструмент, см. RebuildWindows.)
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
		UImage* Icon = MakeIcon(Tree, AssetName, IconName, TexturePath, IconSize);
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
				MakeIcon(Tree, Name, FName(IconName), TexturePath, 22.0f)))
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

		// Build 1.2.1 (Д1): в новой раскладке полоски лежат в СВОИХ канвас-слотах, и все
		// правки этого дополнения уже входят в свежую генерацию (иконки, MoneyRow, AmmoRow,
		// длину полосок задаёт слот). Заворачивать полоску обратно в ряд-коробку НЕЛЬЗЯ —
		// пропали бы ручки ресайза Рината. Детект новой схемы — канвас-слот у полоски.
		if (UWidget* HealthOverlay = Tree->FindWidget(TEXT("HealthBarOverlay")))
		{
			if (Cast<UCanvasPanelSlot>(HealthOverlay->Slot))
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: раскладка Д1 (полоски в канвас-слотах) — дополнение уже входит в неё, пропуск."),
					Name);
				return;
			}
		}

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

	// WBP_Shop, Build 1.2 (ТЗ №2): золотая кнопка «Продать дороже» в панели сделки —
	// иконка видео + живая надпись «Продать за 225 вместо 150» + подстрока про ролик
	// (тексты пишет код UShopScreenWidget). Низ-лево панели, напротив Отмена/Подтвердить.
	// В ассете сразу Collapsed: видимость решают условия ТЗ в коде. Точечная правка —
	// стилизация Рината в остальном ассете не трогается (природа -augment).
	void AugmentShopSellAdButton(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Shop");
		if (Tree->FindWidget(TEXT("SellAdButton")))
		{
			return; // уже добавлена прошлым прогоном
		}
		UWidget* Found = AugFind(Tree, Name, TEXT("SliderCanvas"));
		UCanvasPanel* SliderCanvas = Cast<UCanvasPanel>(Found);
		if (!SliderCanvas)
		{
			if (Found)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("AUGMENT %s: 'SliderCanvas' не канвас (%s) — кнопка рекламы пропущена."),
					Name, *Found->GetClass()->GetName());
			}
			return;
		}

		UObject* Roboto = LoadRobotoFont();
		UButton* SellAd = MakeStyledButton(Tree, TEXT("SellAdButton"),
			FLinearColor(0.85f, 0.62f, 0.14f, 1.0f), FLinearColor(0.95f, 0.72f, 0.2f, 1.0f),
			FLinearColor(1.0f, 0.8f, 0.3f, 1.0f)); // тёплое золото — единый цвет rewarded-кнопок
		SellAd->SetVisibility(ESlateVisibility::Collapsed);
		SellAd->bIsVariable = true;

		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("SellAdRow"));
		UImage* Icon = MakeIcon(Tree, Name, TEXT("SellAdIcon"),
			TEXT("/Game/UI/Icons/T_Icon_AdVideo.T_Icon_AdVideo"), 24.0f);
		Icon->SetColorAndOpacity(FLinearColor(0.1f, 0.08f, 0.03f, 1.0f));
		if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(Icon))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}

		UVerticalBox* LabelBox = Tree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("SellAdLabelBox"));
		const FLinearColor GoldTextColor(0.1f, 0.08f, 0.03f, 1.0f);
		UTextBlock* Price = MakeText(Tree, Roboto, TEXT("SellAdText"),
			NSLOCTEXT("Shop", "SellAdSample", "Продать за 225 вместо 150"),
			GoldTextColor, 15, TEXT("Bold"));
		Price->bIsVariable = true;
		if (UVerticalBoxSlot* PriceSlot = LabelBox->AddChildToVerticalBox(Price))
		{
			PriceSlot->SetHorizontalAlignment(HAlign_Center);
		}
		UTextBlock* Sub = MakeText(Tree, Roboto, TEXT("SellAdSubText"),
			NSLOCTEXT("Shop", "SellAdSubSample", "на 50% больше за просмотр ролика"),
			GoldTextColor, 11, TEXT("Regular"));
		Sub->bIsVariable = true;
		if (UVerticalBoxSlot* SubSlot = LabelBox->AddChildToVerticalBox(Sub))
		{
			SubSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelBox))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		SetButtonContent(SellAd, Row);

		if (UCanvasPanelSlot* AdSlot = SliderCanvas->AddChildToCanvas(SellAd))
		{
			// Низ-лево панели сделки (Отмена/Подтвердить живут в правом-нижнем углу).
			AdSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
			AdSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			AdSlot->SetPosition(FVector2D(0.0f, -24.0f));
			AdSlot->SetSize(FVector2D(280.0f, 50.0f));
		}
		bChanged = true;
		UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT %s: добавлена золотая кнопка SellAdButton."), Name);
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

	// Экраны со списками: класс строки на CDO (без него списки пусты; guide требовал ручного
	// шага, генератор делает его сам — просьба лида). Зовётся ПОСЛЕ компиляции (CDO свежий)
	// и при генерации, и при пересборке (-rebuild).
	void ApplyRowClassFixup(const FWbpSpec& Spec, UWidgetBlueprint* WBP)
	{
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

		ApplyRowClassFixup(Spec, WBP);

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

	// ======================================================================
	// Перенос ручной стилизации владельца при -rebuild
	// ======================================================================
	//
	// В WBP_PlayerStats (коммит dfaffd0), а с 07-28 и в WBP_Dialog/WBP_Shop/WBP_Inventory
	// (коммит 5c2b058) лежит ручная стилизация Рината, которой нет в
	// коде генерации. Пересборка выселяет старое дерево, но старые виджеты остаются
	// живыми UObject'ами в transient-пакете — их свойства читаются и ПОСЛЕ выселения.
	// Порядок: до выселения снимается срез «имя -> старый виджет», после пересборки на
	// одноимённые новые виджеты того же класса переносится БЕЛЫЙ СПИСОК свойств стиля
	// через рефлексию. Именно белый список, а не «все свойства»: слепое копирование
	// утащило бы Slot, bIsVariable и Visibility и сломало бы контракты новой раскладки
	// (зонтики хит-теста, Collapsed у AmmoRow). Отдельно (шаг 3б) переезжает геометрия
	// КАНВАС-СЛОТА (расстановка ручками — позиция/размер/якоря): она живёт не на виджете,
	// и белый список её не покрывает. Каждое перенесённое значение и каждое расхождение
	// пишется в лог.

	// Свойства, которые владелец мог править в дизайнере. Имена сверены с заголовками
	// UE 5.5: Components/TextBlock.h, TextWidgetTypes.h (Justification), ProgressBar.h,
	// SizeBox.h, Image.h, Border.h, Button.h.
	void CollectOwnerStyleProps(const UWidget* Widget, TArray<FName>& OutProps)
	{
		if (Widget->IsA<UTextBlock>())
		{
			// Text переносится тоже: у подписей это текст владельца, а образцы значений
			// код игры всё равно переписывает каждый кадр.
			OutProps.Append({ TEXT("Text"), TEXT("Font"), TEXT("ColorAndOpacity"),
				TEXT("ShadowOffset"), TEXT("ShadowColorAndOpacity"),
				TEXT("Justification"), TEXT("MinDesiredWidth") });
		}
		else if (Widget->IsA<UProgressBar>())
		{
			OutProps.Append({ TEXT("WidgetStyle"), TEXT("FillColorAndOpacity") });
		}
		else if (Widget->IsA<UImage>())
		{
			OutProps.Append({ TEXT("Brush"), TEXT("ColorAndOpacity") });
		}
		else if (Widget->IsA<USizeBox>())
		{
			OutProps.Append({ TEXT("bOverride_WidthOverride"), TEXT("WidthOverride"),
				TEXT("bOverride_HeightOverride"), TEXT("HeightOverride") });
		}
		else if (Widget->IsA<UBorder>())
		{
			OutProps.Append({ TEXT("Background"), TEXT("BrushColor"), TEXT("Padding"),
				TEXT("ContentColorAndOpacity"),
				TEXT("HorizontalAlignment"), TEXT("VerticalAlignment") });
		}
		else if (Widget->IsA<UButton>())
		{
			// Кнопки окон (диалог/магазин/инвентарь) владелец перекрашивает; стиль всех
			// состояний — одна структура WidgetStyle (Button.h:38-49).
			OutProps.Append({ TEXT("WidgetStyle"), TEXT("ColorAndOpacity"), TEXT("BackgroundColor") });
		}
		// Контейнеры (канвас, ряды-коробки) — переносить нечего.
	}

	// Декоративные подписи-слова, которыми владеет Ринат: из WBP_PlayerStats он их УДАЛИЛ
	// (в таблице имён ассета этих имён нет — сверено поиском по бинарнику 07-27). Если
	// подписи не было в старом дереве, из новой раскладки она убирается тоже — иначе
	// пересборка вернула бы владельцу удалённые им слова. Проверка идёт ПО ИМЕНИ в каждом
	// пересобираемом ассете с переносом: у WBP_Shop есть свой MoneyLabel — пока владелец
	// его не удалял, он в старом дереве есть и остаётся; у WBP_Dialog/WBP_Inventory этих
	// имён нет вовсе. Кубики кода (значения, полоски, AmmoRow) сюда класть нельзя — только
	// декор, который владелец вправе выбросить.
	const TCHAR* GOwnerDeletedLabels[] =
	{
		TEXT("HealthBarLabel"), TEXT("HungerBarLabel"), TEXT("ThirstBarLabel"),
		TEXT("AmmoLabel"), TEXT("MoneyLabel"),
	};

	// Обрезка длинных значений для лога (стили-кисти разворачиваются в сотни символов).
	FString ClipForLog(FString Value)
	{
		constexpr int32 MaxLen = 160;
		if (Value.Len() > MaxLen)
		{
			Value = Value.Left(MaxLen) + TEXT("...");
		}
		return Value;
	}

	// Build 1.2.1 (Д1): переезд «ряд-коробка -> отдельные канвас-слоты иконки и полоски».
	// Расстановка Рината снимается со СТАРОГО дерева (оно канвас-первое ПО РЯДАМ с 07-28):
	// позиция ряда — с его канвас-слота, фактические размеры — с его иконки
	// (Brush.ImageSize; в реальном ассете 32 px) и его SizeBox (Width/HeightOverride —
	// значения владельца). Новая иконка встаёт на место старой (центр по высоте ряда),
	// новая полоска — правее иконки с прежним зазором 6 и ПРЕЖНИМ размером полоски.
	// Ряда в старом дереве нет / он не в канвас-слоте — полоска остаётся на штатной
	// позиции свежей генерации (громкая строка в лог, сверить глазами).
	void ConvertPlayerStatsRowsToBarSlots(UWidgetTree* Tree, const TCHAR* AssetName,
		const TMap<FName, UWidget*>& OldWidgets)
	{
		struct FRowSpec
		{
			const TCHAR* RowName;
			const TCHAR* IconName;
			const TCHAR* OverlayName;
			const TCHAR* SizeName;
			FVector2D DefaultBarSize;
		};
		const FRowSpec Rows[] =
		{
			{ TEXT("HealthRow"), TEXT("HealthIcon"), TEXT("HealthBarOverlay"), TEXT("HealthBarSize"), FVector2D(320.0f, 28.0f) },
			{ TEXT("HungerRow"), TEXT("HungerIcon"), TEXT("HungerBarOverlay"), TEXT("HungerBarSize"), FVector2D(224.0f, 24.0f) },
			{ TEXT("ThirstRow"), TEXT("ThirstIcon"), TEXT("ThirstBarOverlay"), TEXT("ThirstBarSize"), FVector2D(224.0f, 24.0f) },
		};

		for (const FRowSpec& Row : Rows)
		{
			UWidget* const* OldRow = OldWidgets.Find(FName(Row.RowName));
			const UCanvasPanelSlot* OldRowSlot = OldRow ? Cast<UCanvasPanelSlot>((*OldRow)->Slot) : nullptr;
			if (!OldRowSlot)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("REBUILD %s: ряда '%s' в старом дереве нет (или он не в канвас-слоте) — иконка и полоска остались на штатных позициях, сверить вид глазами."),
					AssetName, Row.RowName);
				continue;
			}
			const FVector2D RowPos = OldRowSlot->GetPosition();

			// Фактические размеры владельца из старого дерева.
			FVector2D IconSize(24.0f, 24.0f);
			if (UWidget* const* OldIcon = OldWidgets.Find(FName(Row.IconName)))
			{
				if (const UImage* Img = Cast<UImage>(*OldIcon))
				{
					IconSize = Img->GetBrush().GetImageSize();
				}
			}
			FVector2D BarSize = Row.DefaultBarSize;
			if (UWidget* const* OldSize = OldWidgets.Find(FName(Row.SizeName)))
			{
				if (const USizeBox* SB = Cast<USizeBox>(*OldSize))
				{
					BarSize = FVector2D(SB->GetWidthOverride(), SB->GetHeightOverride());
				}
			}

			// Раскладка прежнего ряда: иконка слева (центр по высоте), полоска через зазор 6.
			const float RowHeight = FMath::Max(IconSize.Y, BarSize.Y);
			if (UWidget* NewIcon = Tree->FindWidget(FName(Row.IconName)))
			{
				if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(NewIcon->Slot))
				{
					Slot->SetPosition(FVector2D(RowPos.X, RowPos.Y + (RowHeight - IconSize.Y) * 0.5f));
				}
			}
			if (UWidget* NewBar = Tree->FindWidget(FName(Row.OverlayName)))
			{
				if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(NewBar->Slot))
				{
					Slot->SetPosition(FVector2D(RowPos.X + IconSize.X + 6.0f,
						RowPos.Y + (RowHeight - BarSize.Y) * 0.5f));
					Slot->SetSize(BarSize);
				}
			}
			UE_LOG(LogGenerateWbp, Display,
				TEXT("REBUILD %s: ряд '%s' (поз. %s) разложен: иконка %.0fx%.0f, полоска %.0fx%.0f в своём канвас-слоте."),
				AssetName, Row.RowName, *RowPos.ToString(),
				IconSize.X, IconSize.Y, BarSize.X, BarSize.Y);
		}
	}

	void TransferOwnerStyle(UWidgetTree* Tree, const TCHAR* AssetName,
		const TMap<FName, UWidget*>& OldWidgets)
	{
		// 1. Подписи, удалённые владельцем, убрать из новой раскладки.
		for (const TCHAR* LabelName : GOwnerDeletedLabels)
		{
			UWidget* NewLabel = Tree->FindWidget(FName(LabelName));
			if (!NewLabel || OldWidgets.Contains(FName(LabelName)))
			{
				continue; // в новой раскладке подписи нет ИЛИ владелец её не удалял
			}
			if (UPanelWidget* Parent = NewLabel->GetParent())
			{
				Parent->RemoveChild(NewLabel);
			}
			Tree->RemoveWidget(NewLabel);
			UE_LOG(LogGenerateWbp, Display,
				TEXT("REBUILD %s: подпись '%s' убрана — владелец удалил её из ассета."),
				AssetName, LabelName);
		}

		// 2. Сдвиг всего столба: владелец мог передвинуть StatsBox — его позиция переносится
		// как смещение всех верхнеуровневых канвас-слотов от штатного старта 24,24.
		if (UWidget* const* OldColumn = OldWidgets.Find(FName(TEXT("StatsBox"))))
		{
			if (const UCanvasPanelSlot* OldSlot = Cast<UCanvasPanelSlot>((*OldColumn)->Slot))
			{
				const FVector2D Delta = OldSlot->GetPosition() - FVector2D(24.0f, 24.0f);
				UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
				if (!Delta.IsNearlyZero() && Root)
				{
					for (int32 Index = 0; Index < Root->GetChildrenCount(); ++Index)
					{
						if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Root->GetChildAt(Index)->Slot))
						{
							Slot->SetPosition(Slot->GetPosition() + Delta);
						}
					}
					UE_LOG(LogGenerateWbp, Display,
						TEXT("REBUILD %s: столб владельца стоял в %s — вся раскладка сдвинута на %s."),
						AssetName, *OldSlot->GetPosition().ToString(), *Delta.ToString());
				}
			}
		}

		// 3. Посвойственный перенос стиля на одноимённые виджеты того же класса.
		TArray<UWidget*> NewWidgets;
		Tree->GetAllWidgets(NewWidgets);
		TSet<FName> MatchedNames;
		int32 MovedCount = 0;
		for (UWidget* NewWidget : NewWidgets)
		{
			UWidget* const* OldPtr = OldWidgets.Find(NewWidget->GetFName());
			if (!OldPtr)
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("REBUILD %s: '%s' — новый кубик, в старом ассете его не было (переносить нечего)."),
					AssetName, *NewWidget->GetName());
				continue;
			}
			MatchedNames.Add(NewWidget->GetFName());
			UWidget* OldWidget = *OldPtr;
			if (OldWidget->GetClass() != NewWidget->GetClass())
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("REBUILD %s: у '%s' сменился класс (%s -> %s) — стиль НЕ перенесён, сверить вид глазами."),
					AssetName, *NewWidget->GetName(),
					*OldWidget->GetClass()->GetName(), *NewWidget->GetClass()->GetName());
				continue;
			}

			TArray<FName> PropNames;
			CollectOwnerStyleProps(NewWidget, PropNames);
			for (const FName& PropName : PropNames)
			{
				FProperty* Prop = FindFProperty<FProperty>(NewWidget->GetClass(), PropName);
				if (!Prop)
				{
					UE_LOG(LogGenerateWbp, Warning,
						TEXT("REBUILD %s: у класса %s нет свойства '%s' — белый список разошёлся с движком, свойство НЕ перенесено."),
						AssetName, *NewWidget->GetClass()->GetName(), *PropName.ToString());
					continue;
				}
				const void* OldValue = Prop->ContainerPtrToValuePtr<const void>(OldWidget);
				void* NewValue = Prop->ContainerPtrToValuePtr<void>(NewWidget);
				if (Prop->Identical(OldValue, NewValue, PPF_None))
				{
					continue; // значение и так совпадает — не шумим в логе
				}
				FString OldText, NewText;
				Prop->ExportTextItem_Direct(OldText, OldValue, nullptr, OldWidget, PPF_None);
				Prop->ExportTextItem_Direct(NewText, NewValue, nullptr, NewWidget, PPF_None);
				Prop->CopyCompleteValue(NewValue, OldValue);
				++MovedCount;
				UE_LOG(LogGenerateWbp, Display,
					TEXT("REBUILD %s: '%s'.%s: %s -> %s (значение владельца)."),
					AssetName, *NewWidget->GetName(), *PropName.ToString(),
					*ClipForLog(NewText), *ClipForLog(OldText));
			}

			// 3б. РАССТАНОВКА владельца: то, что он тянул ручками (позиция/размер/якоря/
			// выравнивание), живёт НЕ на виджете, а на его КАНВАС-СЛОТЕ (FAnchorData
			// LayoutData — CanvasPanelSlot.h:74-75), и белый список свойств виджета этого
			// не видит. Урок 07-28: пересборка вернула кнопки панели количества магазина
			// в дефолты генератора, потеряв утверждённую расстановку (срез -dumpslots:
			// SliderTotalText/SliderCancelButton/SliderConfirmButton). Оба виджета в
			// канвас-слотах — геометрия слота переезжает целиком.
			const UCanvasPanelSlot* OldCanvasSlot = Cast<UCanvasPanelSlot>(OldWidget->Slot);
			UCanvasPanelSlot* NewCanvasSlot = Cast<UCanvasPanelSlot>(NewWidget->Slot);
			if (OldCanvasSlot && NewCanvasSlot)
			{
				const FAnchorData OldLayout = OldCanvasSlot->GetLayout();
				const bool bSame = OldLayout == NewCanvasSlot->GetLayout()
					&& OldCanvasSlot->GetAutoSize() == NewCanvasSlot->GetAutoSize()
					&& OldCanvasSlot->GetZOrder() == NewCanvasSlot->GetZOrder();
				if (!bSame)
				{
					UE_LOG(LogGenerateWbp, Display,
						TEXT("REBUILD %s: канвас-слот '%s': офсеты (%.1f,%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f,%.1f) (расстановка владельца)."),
						AssetName, *NewWidget->GetName(),
						NewCanvasSlot->GetLayout().Offsets.Left, NewCanvasSlot->GetLayout().Offsets.Top,
						NewCanvasSlot->GetLayout().Offsets.Right, NewCanvasSlot->GetLayout().Offsets.Bottom,
						OldLayout.Offsets.Left, OldLayout.Offsets.Top,
						OldLayout.Offsets.Right, OldLayout.Offsets.Bottom);
					NewCanvasSlot->SetLayout(OldLayout);
					NewCanvasSlot->SetAutoSize(OldCanvasSlot->GetAutoSize());
					NewCanvasSlot->SetZOrder(OldCanvasSlot->GetZOrder());
					++MovedCount;
				}
			}
			else if (OldCanvasSlot && !NewCanvasSlot)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("REBUILD %s: '%s' был у владельца в канвас-слоте, в новой раскладке — нет: расстановка НЕ перенесена, сверить вид глазами."),
					AssetName, *NewWidget->GetName());
			}
			// Старый в коробке, новый на канвасе — штатный переезд бокс->канвас (позицию
			// даёт новая раскладка), отдельной строки в лог не нужно.
		}

		// 4. Старое, чему в новой раскладке пары не нашлось. Намеренно выброшены: StatsBox
		// (заменён канвас-слотами ещё в ADR-051) и, с Build 1.2.1 (Д1), ряды-коробки статов
		// с их SizeBox'ами (полоски переехали в свои канвас-слоты, геометрию перенёс
		// ConvertPlayerStatsRowsToBarSlots). Всё остальное — громко: владелец мог создать
		// кубик руками (например, AmmoBagText), автоматически его не вернуть — только
		// руками по этому логу.
		const FName IntentionallyDropped[] =
		{
			FName(TEXT("StatsBox")),
			FName(TEXT("HealthRow")), FName(TEXT("HungerRow")), FName(TEXT("ThirstRow")),
			FName(TEXT("HealthBarSize")), FName(TEXT("HungerBarSize")), FName(TEXT("ThirstBarSize")),
		};
		for (const TPair<FName, UWidget*>& Old : OldWidgets)
		{
			if (MatchedNames.Contains(Old.Key)
				|| Algo::Find(IntentionallyDropped, Old.Key) != nullptr)
			{
				continue;
			}
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("REBUILD %s: кубик '%s' (%s) из старого ассета в новую раскладку не попал — его вид не перенесён."),
				AssetName, *Old.Key.ToString(), *Old.Value->GetClass()->GetName());
		}

		// 5. Build 1.2.1 (Д1): панель статов — переезд «ряды-коробки -> отдельные
		// канвас-слоты иконок и полосок»: позиции/размеры снимаются со старых рядов
		// владельца (шаг 3б их не покрывает — рядов в новой раскладке больше нет).
		if (FCString::Strcmp(AssetName, TEXT("WBP_PlayerStats")) == 0)
		{
			ConvertPlayerStatsRowsToBarSlots(Tree, AssetName, OldWidgets);
		}

		UE_LOG(LogGenerateWbp, Display,
			TEXT("REBUILD %s: перенос стилизации владельца завершён, перенесено значений: %d."),
			AssetName, MovedCount);
	}

	// Пересборка дерева СУЩЕСТВУЮЩЕГО ассета текущей Build-функцией спеки (режим -rebuild,
	// ADR-051 п.1, добро лида 07-24). 0 — успех, 1 — ошибка.
	//
	// Почему пересборка, а не точечный конвертер живого дерева: замер Slate-геометрии в
	// коммандлете недоступен (FSlateApplication при -run= не создаётся —
	// LaunchEngineLoop.cpp:3228). Blueprint НЕ пересоздаётся — правится только
	// WidgetTree, поэтому ссылки на класс _C из слотов HUD остаются живыми. Старые виджеты
	// выселяются в transient-пакет, чтобы новые могли занять ТЕ ЖЕ имена (штатный приём
	// редактора — WidgetBlueprintEditorUtils.cpp:595 «so that it doesn't conflict with
	// future widgets sharing the same name»). Для ассетов с ручной стилизацией владельца
	// bTransferOwnerStyle включает её перенос на новое дерево (TransferOwnerStyle).
	int32 RebuildOne(const FWbpSpec& Spec, bool bTransferOwnerStyle)
	{
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPathOf(Spec));
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogGenerateWbp, Warning, TEXT("REBUILD: %s не найден/без дерева — генерирую с нуля."),
				Spec.AssetName);
			return GenerateOne(Spec);
		}

		WBP->Modify();

		TArray<UWidget*> OldWidgets;
		WBP->WidgetTree->GetAllWidgets(OldWidgets);

		// Срез старых виджетов по именам — ДО выселения (при коллизии имён в transient-
		// пакете Rename может дать объекту суффикс). Сами объекты живут дальше — из них
		// TransferOwnerStyle читает значения владельца уже после пересборки.
		TMap<FName, UWidget*> OldByName;
		if (bTransferOwnerStyle)
		{
			OldByName.Reserve(OldWidgets.Num());
			for (UWidget* Old : OldWidgets)
			{
				OldByName.Add(Old->GetFName(), Old);
			}
		}

		for (UWidget* Old : OldWidgets)
		{
			Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
		}
		WBP->WidgetTree->RootWidget = nullptr;
		UE_LOG(LogGenerateWbp, Display,
			TEXT("REBUILD %s: старое дерево (%d виджетов) выселено, строю канвас-первую раскладку."),
			Spec.AssetName, OldWidgets.Num());

		if (!Spec.Build(WBP->WidgetTree))
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("REBUILD: %s — Build-функция вернула ошибку, НЕ сохраняю."),
				Spec.AssetName);
			return 1;
		}

		if (bTransferOwnerStyle)
		{
			TransferOwnerStyle(WBP->WidgetTree, Spec.AssetName, OldByName);
		}

		// Контракт кубиков — ДО сохранения и ПОСЛЕ переноса стилизации (удаление подписей
		// не имеет права зацепить кубики кода): пропал хоть один — на диск не пишем.
		int32 MissingCount = 0;
		for (const TCHAR* Cube : Spec.ExpectedCubes)
		{
			if (!WBP->WidgetTree->FindWidget(FName(Cube)))
			{
				UE_LOG(LogGenerateWbp, Error, TEXT("REBUILD: %s — кубик %s пропал из новой раскладки."),
					Spec.AssetName, Cube);
				++MissingCount;
			}
		}
		if (MissingCount > 0)
		{
			return 1;
		}

		FKismetEditorUtilities::CompileBlueprint(WBP);
		if (WBP->Status == BS_Error)
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("REBUILD: %s скомпилировался с ошибками — НЕ сохраняю (ассет на диске цел)."),
				Spec.AssetName);
			return 1;
		}

		ApplyRowClassFixup(Spec, WBP);

		const FString Filename = FPackageName::LongPackageNameToFilename(
			Spec.PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(WBP->GetOutermost(), WBP, *Filename, SaveArgs))
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("REBUILD: SavePackage не сохранил %s."), *Filename);
			return 1;
		}

		UE_LOG(LogGenerateWbp, Display, TEXT("REBUILD OK: %s пересобран и сохранён (%s)."),
			Spec.AssetName, *Filename);
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
	if (Switches.Contains(TEXT("rebuild")))
	{
		// -asset=WBP_Death — пересобрать только один ассет из списка (Build 1.2).
		FString AssetFilter;
		FParse::Value(*Params, TEXT("asset="), AssetFilter);
		return RebuildWindows(AssetFilter);
	}
	if (Switches.Contains(TEXT("dumpslots")))
	{
		return DumpSlotsAll();
	}
	if (Switches.Contains(TEXT("adicon")))
	{
		return GenerateAdIcon();
	}
	if (Switches.Contains(TEXT("pickupfix")))
	{
		return FixPickupAssets();
	}
	return GenerateAll(Switches.Contains(TEXT("force")));
}

// Печать поддерева с геометрией слота каждого виджета. Формат строк стабильный и
// одинаковый между прогонами — лог двух прогонов сравнивается диффом (артефакт
// «расстановка владельца сохранилась» вместо осмотра мышкой).
static void DumpSlotSubtree(const TCHAR* AssetName, UWidget* Widget, int32 Depth)
{
	if (!Widget)
	{
		return;
	}

	FString SlotDesc = TEXT("(корень)");
	if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		const FAnchorData Layout = CanvasSlot->GetLayout();
		SlotDesc = FString::Printf(
			TEXT("канвас якоря=(%.3f,%.3f,%.3f,%.3f) офсеты=(%.1f,%.1f,%.1f,%.1f) вырав=(%.2f,%.2f) авто=%d z=%d"),
			Layout.Anchors.Minimum.X, Layout.Anchors.Minimum.Y,
			Layout.Anchors.Maximum.X, Layout.Anchors.Maximum.Y,
			Layout.Offsets.Left, Layout.Offsets.Top, Layout.Offsets.Right, Layout.Offsets.Bottom,
			Layout.Alignment.X, Layout.Alignment.Y,
			CanvasSlot->GetAutoSize() ? 1 : 0, CanvasSlot->GetZOrder());
	}
	else if (const UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		const FMargin Pad = HSlot->GetPadding();
		SlotDesc = FString::Printf(TEXT("hbox отступы=(%.1f,%.1f,%.1f,%.1f)"),
			Pad.Left, Pad.Top, Pad.Right, Pad.Bottom);
	}
	else if (const UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		const FMargin Pad = VSlot->GetPadding();
		SlotDesc = FString::Printf(TEXT("vbox отступы=(%.1f,%.1f,%.1f,%.1f)"),
			Pad.Left, Pad.Top, Pad.Right, Pad.Bottom);
	}
	else if (Widget->Slot)
	{
		SlotDesc = Widget->Slot->GetClass()->GetName();
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("SLOTS %s: %s%s : %s : %s"),
		AssetName, *FString::ChrN(Depth * 2, TEXT(' ')), *Widget->GetName(),
		*Widget->GetClass()->GetName(), *SlotDesc);

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			DumpSlotSubtree(AssetName, Panel->GetChildAt(Index), Depth + 1);
		}
	}
}

int32 UGenerateWbpCommandlet::DumpSlotsAll()
{
	int32 FailCount = 0;
	for (const FWbpSpec& Spec : GAssets)
	{
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPathOf(Spec));
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("SLOTS FAIL: %s не загрузился."), *ObjectPathOf(Spec));
			++FailCount;
			continue;
		}
		DumpSlotSubtree(Spec.AssetName, WBP->WidgetTree->RootWidget, 1);
	}
	return FailCount == 0 ? 0 : 1;
}

int32 UGenerateWbpCommandlet::RebuildWindows(const FString& AssetFilter)
{
	// Пересборка канвас-первой раскладкой (ADR-051 п.1 + волна «двигать мышкой все окна»
	// 07-28: диалог и экран смерти). Перенос значений владельца (TransferOwnerStyle)
	// включён ВСЕМ ассетам с его ручной стилизацией: WBP_PlayerStats — коммит dfaffd0,
	// WBP_Dialog/WBP_Shop/WBP_Inventory — коммит 5c2b058 (раньше у магазина и инвентаря
	// перенос был выключен — тогда правок владельца в них не было; после 5c2b058 пересборка
	// без переноса стёрла бы стилизацию). У WBP_Death правок владельца нет (git-история —
	// только генерация cc919a8), его пересборка чистая. Остальные ассеты пересборке не
	// подлежат. Процессный предохранитель (проверяет лид перед запуском): git status
	// пересобираемых .uasset должен быть чист — иначе прогон затёр бы несохранённые правки.
	struct FRebuildEntry
	{
		const TCHAR* AssetName;
		bool bTransferOwnerStyle;
	};
	static const FRebuildEntry RebuildAssets[] =
	{
		{ TEXT("WBP_Shop"), true },
		{ TEXT("WBP_Inventory"), true },
		{ TEXT("WBP_PlayerStats"), true },
		{ TEXT("WBP_Dialog"), true },
		{ TEXT("WBP_Death"), false },
	};

	int32 FailCount = 0;
	int32 ProcessedCount = 0;
	for (const FRebuildEntry& Entry : RebuildAssets)
	{
		// Точечная пересборка (-asset=ИМЯ): остальные окна не трогаются вовсе.
		if (!AssetFilter.IsEmpty() && !AssetFilter.Equals(Entry.AssetName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		++ProcessedCount;
		bool bFound = false;
		for (const FWbpSpec& Spec : GAssets)
		{
			if (FCString::Strcmp(Spec.AssetName, Entry.AssetName) == 0)
			{
				bFound = true;
				if (RebuildOne(Spec, Entry.bTransferOwnerStyle) != 0)
				{
					++FailCount;
				}
				break;
			}
		}
		if (!bFound)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("REBUILD: %s не найден в таблице ассетов."), Entry.AssetName);
			++FailCount;
		}
	}

	if (!AssetFilter.IsEmpty() && ProcessedCount == 0)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("REBUILD: фильтр -asset=%s не совпал ни с одним ассетом списка."),
			*AssetFilter);
		return 1;
	}
	UE_LOG(LogGenerateWbp, Display, TEXT("REBUILD ИТОГ: ошибок %d из %d ассетов."),
		FailCount, ProcessedCount);
	return FailCount > 0 ? 1 : 0;
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
		{ TEXT("/Game/UI/WBP_Shop"),          TEXT("WBP_Shop"),          &AugmentShopSellAdButton },
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

		// Контракт замков: начинка кнопок/рядов замкнута, верхнеуровневые элементы
		// свободны (см. GLockContracts). До первого прогона -rebuild после этой правки
		// ассеты на диске замков не имеют — провал здесь тогда означает «перегенерация
		// ещё не выполнена», это ожидаемо.
		for (const FLockContract& Contract : GLockContracts)
		{
			if (FCString::Strcmp(Spec.AssetName, Contract.AssetName) != 0)
			{
				continue;
			}
			for (const TCHAR* WidgetName : Contract.LockedContent)
			{
				UWidget* Found = WBP->WidgetTree ? WBP->WidgetTree->FindWidget(FName(WidgetName)) : nullptr;
				if (!Found || !Found->IsLockedInDesigner())
				{
					UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: %s — начинка '%s' %s."),
						Spec.AssetName, WidgetName,
						Found ? TEXT("не замкнута (bLockedInDesigner=false) — клик в дизайнере выделит её, а не верхнеуровневый элемент")
						      : TEXT("не найдена"));
					bOk = false;
				}
			}
			for (const TCHAR* WidgetName : Contract.SelectableWidgets)
			{
				UWidget* Found = WBP->WidgetTree ? WBP->WidgetTree->FindWidget(FName(WidgetName)) : nullptr;
				if (!Found || Found->IsLockedInDesigner())
				{
					UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: %s — виджет '%s' %s."),
						Spec.AssetName, WidgetName,
						Found ? TEXT("замкнут — владелец не сможет выделить и тянуть его в дизайнере")
						      : TEXT("не найден"));
					bOk = false;
				}
			}
			if (bOk)
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("VERIFY %s: замки на месте — замкнутой начинки %d, свободных верхнеуровневых виджетов %d."),
					Spec.AssetName, static_cast<int32>(Contract.LockedContent.size()),
					static_cast<int32>(Contract.SelectableWidgets.size()));
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

int32 UGenerateWbpCommandlet::GenerateAdIcon()
{
	// Build 1.2: единая иконка видео rewarded-кнопок (ТЗ раздел 0 п.8 — «треугольник
	// воспроизведения в скруглённом квадрате», выбран из двух допустимых вариантов).
	// Пиксели считаются процедурно (SDF-контур + залитый треугольник со сглаживанием),
	// белым с альфой — тонирует кнопка. Повторный прогон перезаписывает на месте.
	const FString PackageName = TEXT("/Game/UI/Icons/T_Icon_AdVideo");
	const FString AssetName = TEXT("T_Icon_AdVideo");

	constexpr int32 Size = 64;
	TArray<uint8> Pixels;
	Pixels.SetNumZeroed(Size * Size * 4); // BGRA8

	const float Cx = 32.0f, Cy = 32.0f;
	const float HalfExtent = 26.0f;   // скруглённый квадрат 52x52 по центру
	const float CornerRadius = 12.0f;
	const float OutlineWidth = 5.0f;

	// Треугольник остриём вправо, обход по часовой (в экранных координатах, ось Y вниз).
	const FVector2D TriA(26.0f, 21.0f), TriB(46.0f, 32.0f), TriC(26.0f, 43.0f);
	auto EdgeDist = [](const FVector2D& P, const FVector2D& E0, const FVector2D& E1)
	{
		// Знаковое расстояние до ребра: положительно ВНУТРИ треугольника.
		const FVector2D Edge = E1 - E0;
		const FVector2D Normal = FVector2D(-Edge.Y, Edge.X).GetSafeNormal();
		return static_cast<float>(FVector2D::DotProduct(P - E0, Normal));
	};

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const FVector2D P(X + 0.5f, Y + 0.5f);

			// SDF скруглённого прямоугольника; кольцо контура = |sdf| < половины толщины.
			// FVector2D в UE5 — double: промежутки считаем во float явно (без сужений).
			const float QX = static_cast<float>(FMath::Abs(P.X - Cx)) - (HalfExtent - CornerRadius);
			const float QY = static_cast<float>(FMath::Abs(P.Y - Cy)) - (HalfExtent - CornerRadius);
			const float OutsideDist = FMath::Sqrt(
				FMath::Square(FMath::Max(QX, 0.0f)) + FMath::Square(FMath::Max(QY, 0.0f)));
			const float InsideDist = FMath::Min(FMath::Max(QX, QY), 0.0f);
			const float RectSdf = OutsideDist + InsideDist - CornerRadius;
			const float RingAlpha = FMath::Clamp(OutlineWidth * 0.5f - FMath::Abs(RectSdf) + 0.5f, 0.0f, 1.0f);

			const float TriDist = FMath::Min3(
				EdgeDist(P, TriA, TriB), EdgeDist(P, TriB, TriC), EdgeDist(P, TriC, TriA));
			const float TriAlpha = FMath::Clamp(TriDist + 0.5f, 0.0f, 1.0f);

			uint8* Px = &Pixels[(Y * Size + X) * 4];
			Px[0] = 255; Px[1] = 255; Px[2] = 255; // BGR — белый
			Px[3] = static_cast<uint8>(FMath::RoundToInt(255.0f * FMath::Max(RingAlpha, TriAlpha)));
		}
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("ADICON: пакет %s не создался."), *PackageName);
		return 1;
	}

	UTexture2D* Texture = FindObject<UTexture2D>(Package, *AssetName);
	const bool bExisted = Texture != nullptr;
	if (!Texture)
	{
		Texture = NewObject<UTexture2D>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}
	Texture->Source.Init(Size, Size, /*NumSlices=*/1, /*NumMips=*/1, TSF_BGRA8, Pixels.GetData());
	Texture->SRGB = true;
	Texture->CompressionSettings = TC_EditorIcon; // UserInterface2D: без блочного сжатия, честная альфа
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->LODGroup = TEXTUREGROUP_UI;
	Texture->NeverStream = true;
	Texture->PostEditChange();
	if (!bExisted)
	{
		FAssetRegistryModule::AssetCreated(Texture);
	}
	Package->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(
		PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Texture, *Filename, SaveArgs))
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("ADICON: SavePackage не сохранил %s."), *Filename);
		return 1;
	}
	UE_LOG(LogGenerateWbp, Display, TEXT("ADICON OK: %s (%dx%d, контур + треугольник) сохранена в %s."),
		*PackageName, Size, Size, *Filename);
	return 0;
}

int32 UGenerateWbpCommandlet::FixPickupAssets()
{
	// Build 1.2.1 (ТЗ А2/А4). Причина «серых» пикапов ДОКАЗАНА срезом ассетов: меши
	// SM_LootSack/SM_HidePickup импортированы БЕЗ материалов (bImportMaterials=false в
	// метаданных импорта), в слоте — движковый WorldGridMaterial (та самая серая шахматка).
	// Перекрытий материала в C++/BP НЕТ (гипотеза ТЗ не подтвердилась). Паспорта модельера:
	// красить вершинными цветами, «в UE вешать M_VColor». Здесь: (А2) M_VColor в слот 0
	// всех трёх мешей лута (+ запасной рюкзак) с пруфом числа вершин с цветом; (А4) в
	// M_VColor добавляется эмиссив-пара GlowColor(чёрный) x GlowIntensity(0) — нулевые
	// дефолты не меняют вид ни одного пользователя материала, живые значения ставит MID
	// пикапа (APickup::SetupGlow). Повторный прогон — no-op (идемпотентно).
	UMaterial* VColor = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_VColor.M_VColor"));
	if (!VColor)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("PICKUPFIX: /Game/Materials/M_VColor не загрузился — стоп."));
		return 1;
	}

	int32 Errors = 0;

	auto SaveAsset = [&Errors](UObject* Asset, const TCHAR* Context)
	{
		const FString PackageName = Asset->GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, SaveArgs))
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("PICKUPFIX: SavePackage не сохранил %s (%s)."),
				*Filename, Context);
			++Errors;
			return false;
		}
		UE_LOG(LogGenerateWbp, Display, TEXT("PICKUPFIX: %s сохранён (%s)."), *PackageName, Context);
		return true;
	};

	// --- А4: параметры свечения в M_VColor (идемпотентно: по имени GlowIntensity) ---
	bool bAlreadyHasGlow = false;
	for (UMaterialExpression* Expr : VColor->GetExpressionCollection().Expressions)
	{
		const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expr);
		if (Scalar && Scalar->ParameterName == FName(TEXT("GlowIntensity")))
		{
			bAlreadyHasGlow = true;
			break;
		}
	}
	if (bAlreadyHasGlow)
	{
		UE_LOG(LogGenerateWbp, Display,
			TEXT("PICKUPFIX: у M_VColor уже есть GlowIntensity — параметры свечения пропущены."));
	}
	else
	{
		UMaterialEditorOnlyData* EditorData = VColor->GetEditorOnlyData();
		if (!EditorData)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("PICKUPFIX: у M_VColor нет editor-данных — стоп."));
			return 1;
		}
		VColor->Modify();

		UMaterialExpressionVectorParameter* GlowColor =
			NewObject<UMaterialExpressionVectorParameter>(VColor);
		GlowColor->ParameterName = TEXT("GlowColor");
		GlowColor->DefaultValue = FLinearColor::Black; // чёрный x что угодно = эмиссив 0
		GlowColor->Material = VColor;
		GlowColor->MaterialExpressionEditorX = -700;
		GlowColor->MaterialExpressionEditorY = 320;

		UMaterialExpressionScalarParameter* GlowIntensity =
			NewObject<UMaterialExpressionScalarParameter>(VColor);
		GlowIntensity->ParameterName = TEXT("GlowIntensity");
		GlowIntensity->DefaultValue = 0.0f;
		GlowIntensity->Material = VColor;
		GlowIntensity->MaterialExpressionEditorX = -700;
		GlowIntensity->MaterialExpressionEditorY = 520;

		UMaterialExpressionMultiply* GlowMul = NewObject<UMaterialExpressionMultiply>(VColor);
		GlowMul->Material = VColor;
		GlowMul->MaterialExpressionEditorX = -420;
		GlowMul->MaterialExpressionEditorY = 400;
		GlowMul->A.Connect(0, GlowColor);
		GlowMul->B.Connect(0, GlowIntensity);

		FMaterialExpressionCollection& Collection = VColor->GetExpressionCollection();
		Collection.AddExpression(GlowColor);
		Collection.AddExpression(GlowIntensity);
		Collection.AddExpression(GlowMul);
		EditorData->EmissiveColor.Connect(0, GlowMul);

		VColor->PreEditChange(nullptr);
		VColor->PostEditChange();
		if (SaveAsset(VColor, TEXT("А4: GlowColor x GlowIntensity -> Emissive, дефолты нулевые")))
		{
			UE_LOG(LogGenerateWbp, Display,
				TEXT("PICKUPFIX: M_VColor получил параметры свечения (дефолт: эмиссив 0 — вид прочих пользователей не тронут)."));
		}
	}

	// --- А2: M_VColor в слоты мешей лута ---
	const TCHAR* MeshPaths[] =
	{
		TEXT("/Game/Environment/Props/SM_LootSack.SM_LootSack"),     // мешок (BP_Pickup + дефолт APickup)
		TEXT("/Game/Environment/Props/SM_HidePickup.SM_HidePickup"), // свёрток шкуры (BP_PickupWolf)
		TEXT("/Game/Environment/Props/SM_LootBackpack.SM_LootBackpack"), // запасной вариант — чиним заодно
	};
	for (const TCHAR* Path : MeshPaths)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Mesh)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("PICKUPFIX: меш %s не загрузился."), Path);
			++Errors;
			continue;
		}

		// Пруф вершинных цветов: M_VColor без них красить нечем (рендер белым).
		int32 ColorVerts = -1; // -1 = проверить не удалось (рендер-данных нет)
		if (const FStaticMeshRenderData* RenderData = Mesh->GetRenderData())
		{
			if (RenderData->LODResources.Num() > 0)
			{
				ColorVerts = static_cast<int32>(
					RenderData->LODResources[0].VertexBuffers.ColorVertexBuffer.GetNumVertices());
			}
		}
		if (ColorVerts == 0)
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("PICKUPFIX: у %s НЕТ вершинных цветов — M_VColor его не покрасит, нужен реимпорт FBX с цветами."),
				*Mesh->GetName());
			++Errors;
		}

		// Слоты: всё, что не M_VColor (у обоих мешей это WorldGridMaterial), заменяется.
		bool bChanged = false;
		TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			UMaterialInterface* Current = Slots[Index].MaterialInterface;
			if (Current == VColor)
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("PICKUPFIX: %s слот %d уже M_VColor — пропуск."), *Mesh->GetName(), Index);
				continue;
			}
			// SetMaterial (editor-путь) сам ведёт Pre/PostEditChange ассета.
			Mesh->SetMaterial(Index, VColor);
			bChanged = true;
			UE_LOG(LogGenerateWbp, Display, TEXT("PICKUPFIX: %s слот %d: %s -> M_VColor."),
				*Mesh->GetName(), Index, *GetNameSafe(Current));
		}
		if (Slots.Num() == 0)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("PICKUPFIX: у %s нет слотов материалов."), *Mesh->GetName());
			++Errors;
			continue;
		}

		if (bChanged && !SaveAsset(Mesh, TEXT("А2: материал слота -> M_VColor")))
		{
			continue;
		}

		// Финальный срез-пруф ассета: слоты, вершинные цвета, реальный габарит (для А3:
		// у свёртка ~45 см по длинной оси при масштабе 1 — паспорт модельера).
		const FVector Extent = Mesh->GetBoundingBox().GetExtent() * 2.0f;
		UE_LOG(LogGenerateWbp, Display,
			TEXT("PICKUPFIX СРЕЗ: %s — слот0=%s, вершин с цветом %s, габарит %.0fx%.0fx%.0f см."),
			*Mesh->GetName(), *GetNameSafe(Slots[0].MaterialInterface),
			ColorVerts < 0 ? TEXT("НЕ ПРОВЕРЕНО (нет рендер-данных)") : *FString::FromInt(ColorVerts),
			Extent.X, Extent.Y, Extent.Z);
	}

	return Errors == 0 ? 0 : 1;
}
