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
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"     // Б9: безопасная зона экрана (вырез камеры на телефоне)
#include "Components/SafeZoneSlot.h" // её слот — там лежит запасной отступ
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/TextWidgetTypes.h" // UTextLayoutWidget: чтение AutoWrapText отражением (П.0 ADR-077)
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UObject/UnrealType.h" // FindFProperty/FBoolProperty (идемпотентность переноса подписей)
#include "Engine/Blueprint.h" // -hudslots: BP_ContrarySurvivorHUD — обычный Blueprint, не Widget
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
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // -hudslots: слоты UMG-классов HUD (ADR-048)
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h" // -hudslots 08-07: слоты окон контроллера
#include "ContrarySurvivor/UI/TouchControlsTypes.h"
#include "ContrarySurvivor/UI/IntroObjectiveWidget.h" // строка задачи вступления: вид берём у самого окна
#include "ContrarySurvivor/UI/InventoryScreenWidget.h"
#include "ContrarySurvivor/UI/SettingsScreenWidget.h" // родительский класс окна настроек
#include "ContrarySurvivor/UI/StartScreenWidget.h" // FStartScreenStyle: вид кнопок меню — одно место правды
#include "ContrarySurvivor/UI/SupportAuthorWidget.h" // FSupportAuthorStyle + вид кнопок окна поддержки
#include "ContrarySurvivor/UI/ConsentScreenWidget.h" // FConsentScreenStyle: формулировки согласия одним местом
#include "ContrarySurvivor/UI/PauseMenuWidget.h"    // FPauseMenuStyle: подпись о сохранении в паузе одним местом (ADR-074)
#include "ContrarySurvivor/UI/ShopScreenWidget.h"
#include "ContrarySurvivor/UI/CorpseLootWidget.h"   // TileWidgetClass окна обыска (Build 1.2.2)
#include "ContrarySurvivor/UI/ItemTileWidget.h"     // полный тип для TSubclassOf-присваивания

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

	// ------------------------------------------------------------------
	// WBP_BaseAnnounce — надпись при входе на базу противника (ТЗ 22.08 §5 + П.0 ADR-077:
	// раскладка и стиль в ассете, код окна ставит только тексты/видимость). Крупная
	// полупрозрачная ЦИФРА ступени кладётся в канву ПЕРВОЙ — рисуется ПОД строкой
	// («вторым планом, без скобок» — слова Рината дословно).
	// ------------------------------------------------------------------
	bool BuildBaseAnnounce(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AnnounceRoot"));
		Tree->RootWidget = Root;

		UTextBlock* Digit = MakeText(Tree, Roboto, TEXT("DigitText"), TEXT("3"),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.18f), 96, TEXT("Bold"));
		Digit->bIsVariable = true; // BindWidgetOptional кода
		if (UCanvasPanelSlot* DigitSlot = Root->AddChildToCanvas(Digit))
		{
			DigitSlot->SetAnchors(FAnchors(0.5f, 0.22f, 0.5f, 0.22f)); // верхняя треть, центр ширины
			DigitSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DigitSlot->SetAutoSize(true);
			DigitSlot->SetPosition(FVector2D(0.0f, 0.0f));
		}

		UTextBlock* Line = MakeText(Tree, Roboto, TEXT("LineText"),
			TEXT("Лагерь бандитов. Эти выглядят ещё более опытными"),
			FLinearColor(0.95f, 0.95f, 0.95f, 1.0f), 22, TEXT("Bold"));
		Line->bIsVariable = true;
		ApplyTextShadow(Line); // надпись висит поверх игрового мира
		if (UCanvasPanelSlot* LineSlot = Root->AddChildToCanvas(Line))
		{
			LineSlot->SetAnchors(FAnchors(0.5f, 0.22f, 0.5f, 0.22f));
			LineSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			LineSlot->SetAutoSize(true);
			LineSlot->SetPosition(FVector2D(0.0f, 0.0f));
		}
		return true;
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
	// ⛔ Два абзаца ниже — ИСТОРИЯ отменённой схемы замков (жила до П.0 ADR-077, 23.08);
	// действующее правило — в теле LockSubtreeInDesigner.
	// Поэтому контент кнопок статичной раскладки «замыкали» (bLockedInDesigner):
	// замкнутый виджет выпадает из сетки дизайнера, клик проваливается к самой
	// кнопке — у неё канвас-слот, ручки и перетаскивание работают. Замок НЕ
	// наследуется (IsLockedInDesigner читает только собственный флаг — Widget.h:474)
	// — ставим рекурсивно на всё поддерево контента. Флаг редакторный
	// (WITH_EDITORONLY_DATA, Widget.h:408-410): сериализуется в ассет, на рантайм не
	// влияет никак.
	//
	// ПОПРАВКА 08-07 (ТЗ Рината, п.3: «Размер текста в кнопках не получается поменять…
	// возможно я не нашел в WBP где это делается»): прежняя цена замка — «панель „Детали“
	// у замкнутого виджета заблокирована (SWidgetDetailsView.cpp:375-393)» — оказалась
	// неподъёмной: шрифт подписи кнопки было НЕГДЕ править, а снять замок значком в
	// «Иерархии» владелец не обязан догадываться. Поэтому ТЕКСТЫ внутри кнопок теперь
	// размыкаются обратно (UnlockTextBlocksInSubtree — та же волна, что ADR-056 для
	// начинки слотов брони). Контейнеры и иконки остаются замкнутыми: клик мимо текста
	// по-прежнему выделяет кнопку целиком, а сам текст берут кликом по нему или в
	// «Иерархии» — и правят шрифт в «Деталях».

	void LockSubtreeInDesigner(UWidget* Widget)
	{
		// ⛔ П.0 ADR-077 (23.08): ЗАМКИ ОТМЕНЕНЫ — Ринат требует выделять/двигать/менять ВСЁ
		// («НЕ МОГУ ДВИГАТЬ КНОПКИ… ПЛАШКИ, ТЕКСТ В НИХ» — пауза и плитка стояли на замках).
		// Функция намеренно ПУСТАЯ, а не удалена: её зовут все сборщики окон — одна точка
		// глушит замки везде, включая будущие -rebuild. -verify теперь валит ЛЮБОЙ замок,
		// на живых ассетах их снимает -unlockall. НЕ возвращать замки без решения Рината.
		(void)Widget;
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

	// Разомкнуть ТЕКСТЫ поддерева (подписи кнопок) — см. «ПОПРАВКА 08-07» выше. Правки
	// владельца переживают -rebuild штатно: Font — в белом списке TransferOwnerStyle,
	// а саму волну размыкания закрывает его правило «старый замкнут, новый свободен ->
	// вид из новой генерации». AssetName и bChanged нужны только журналу прогона
	// -unlockcaptions по существующим ассетам; из генерации зовётся с nullptr.
	void UnlockTextBlocksInSubtree(UWidget* Widget, const TCHAR* AssetName, bool* bChanged)
	{
		if (!Widget)
		{
			return;
		}
		if (Widget->IsA<UTextBlock>() && Widget->IsLockedInDesigner())
		{
			Widget->SetLockedInDesigner(false);
			if (bChanged)
			{
				*bChanged = true;
			}
			if (AssetName)
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("UNLOCK %s: подпись '%s' разомкнута — выделяется в дизайнере, шрифт правится в панели «Детали»."),
					AssetName, *Widget->GetName());
			}
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				UnlockTextBlocksInSubtree(Panel->GetChildAt(Index), AssetName, bChanged);
			}
		}
	}

	// Контент в кнопки статичной раскладки класть ТОЛЬКО этим хелпером: SetContent +
	// замок на всём поддереве, затем размыкание текстов (поправка 08-07). Звать ПОСЛЕ
	// сборки поддерева контента (замок не наследуется — поздним детям он бы не достался).
	void SetButtonContent(UButton* Button, UWidget* Content)
	{
		Button->SetContent(Content);
		LockSubtreeInDesigner(Content);
		UnlockTextBlocksInSubtree(Content, nullptr, nullptr);
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

	// ПЛИТКА ПРЕДМЕТА WBP_ItemTile (Build 1.2.2, решение Рината: «иконки в сетке, как в
	// сталкере или LDoE») — ОДНА на инвентарь/магазин/обыск, вместо прежних строковых
	// WBP_InventoryRow/WBP_ShopRow.
	//
	// П.0.5 ADR-077 (23.08, отчёт Рината п.7: «ТОЖЕ НЕЛЬЗЯ СВОБОДНО ПЕРЕТАСКИВАТЬ ПЛАШКИ,
	// ТЕКСТ В НИХ НАСТРАИВАТЬ»): прежнее дерево держало начинку в Overlay/Border/VerticalBox
	// — у таких детей нет ручек перетаскивания (см. «Канвас-первая раскладка» выше). Теперь
	// вся начинка — отдельные канвас-слоты в TileCanvas внутри корневого TileSizeBox: каждый
	// кубик выделяется и таскается мышкой. Имена кубиков — прежний контракт
	// BindWidgetOptional-полей UItemTileWidget; кодовый фолбэк BuildFallbackTree структурно
	// остался стопкой (ему дизайнер не нужен), контракт — только на ИМЕНА.
	//
	// Габариты плитки и иконки по-прежнему ставит код окна (SetTileSize из настроек экрана;
	// решение лида 23.08: размер плитки — настройка окна-хозяина, у разных окон он законно
	// разный) — здесь дефолты инвентаря. Чтобы начинка тянулась за габаритом: кнопка —
	// растяжка на всю плитку; иконка — к верхней кромке по центру ширины; тексты — растяжка
	// по ширине, пришвартованы к верху; корзинка выброса — к правому верхнему углу. Рост
	// плитки под длинное название (Б8) сохраняется: авторазмерный текст на верхнем якоре
	// прибавляет канве «отступ сверху + высота с переносами» (SConstraintCanvas.cpp:374-384),
	// а SizeBox держит MinDesiredHeight, не потолок.
	//
	// Подложки TilePlate в ассетном дереве НЕТ намеренно (решение cpp-dev 24.08, хвост
	// acf44c4): фон плитки даёт сама кнопка TileButton — растянута на всю плитку, стиль
	// (цвет/подсветки наведения и нажатия) правится в ассете; отдельная плашка поверх кнопки
	// закрывала бы её подсветки, а под кнопкой была бы не видна. Кодовый фолбэк без ассета
	// по-прежнему строит свой Border — контракт имён это не трогает.
	//
	// Клики: тексты и иконка теперь лежат ПОВЕРХ кнопки соседними слотами, а не внутри неё,
	// поэтому в ассете они SelfHitTestInvisible — касание проваливается сквозь них к кнопке.
	// Цифра количества по умолчанию стоит у правого нижнего угла иконки ДЕФОЛТНОГО размера
	// (86); при другом размере иконки её ставит на место владелец в дизайнере — код
	// геометрию не трогает (П.0).
	bool BuildItemTile(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		USizeBox* TileSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TileSizeBox"));
		TileSize->SetWidthOverride(110.0f);
		// Б8 (издатель 08-05): высота плитки — МИНИМУМ, а не жёсткий потолок, иначе длинное
		// название брони вылезает за границу и наезжает на плитку строкой ниже. Живые
		// габариты всё равно ставит код окна (UItemTileWidget::SetTileSize), здесь дефолт.
		TileSize->SetMinDesiredHeight(150.0f);
		Tree->RootWidget = TileSize;

		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TileCanvas"));
		TileSize->SetContent(Canvas);

		// Кнопка основного действия — вся плитка (использовать/надеть/купить/продать/
		// забрать — решает окно-владелец). Цвета — InvSlotColor с подсветками, как слоты
		// брони; недоступную покупку код гасит (Disabled-стиль полупрозрачный). Контента у
		// кнопки больше нет — начинка лежит соседними канвас-слотами поверх.
		UButton* Tile = MakeStyledButton(Tree, TEXT("TileButton"),
			FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
			FLinearColor(0.25f, 0.27f, 0.33f, 1.0f));
		if (UCanvasPanelSlot* TileSlot = CanvasStretch(Canvas, Tile, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f)))
		{
			TileSlot->SetZOrder(0);
		}

		// Квадрат иконки — к верхней кромке, центр ширины (при смене ширины плитки остаётся
		// по центру). Габарит перезапишет SetTileSize окна-владельца.
		USizeBox* IconBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TileIconBox"));
		IconBox->SetWidthOverride(86.0f);
		IconBox->SetHeightOverride(86.0f);
		IconBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* IconBoxSlot = CanvasAuto(Canvas, IconBox,
			FVector2D(0.0f, 6.0f), FAnchors(0.5f, 0.0f, 0.5f, 0.0f)))
		{
			IconBoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			IconBoxSlot->SetZOrder(1);
		}

		// Иконка предмета — текстуру ставит код окна (SetTileData), в ассете пустая.
		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TileIcon"));
		Icon->bIsVariable = true;
		Icon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		IconBox->SetContent(Icon);

		// Цифра количества — видимость ведёт код (только у стака >1); по умолчанию у правого
		// нижнего угла иконки, без тени пропадала бы на светлом арте.
		UTextBlock* Count = MakeText(Tree, Roboto, TEXT("TileCountText"), TEXT("x1"),
			FLinearColor(1.0f, 0.85f, 0.3f, 1.0f), 14, TEXT("Bold"));
		ApplyTextShadow(Count);
		Count->bIsVariable = true;
		Count->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* CountSlot = CanvasAuto(Canvas, Count,
			FVector2D(41.0f, 90.0f), FAnchors(0.5f, 0.0f, 0.5f, 0.0f)))
		{
			CountSlot->SetAlignment(FVector2D(1.0f, 1.0f)); // правым нижним углом к точке
			CountSlot->SetZOrder(2);
		}

		// Подпись-название ПОД иконкой — постоянная (решение Рината), с переносом строк.
		// Растяжка по ширине: перенос считается от живой ширины плитки.
		UTextBlock* Name = MakeText(Tree, Roboto, TEXT("TileNameText"), TEXT("Предмет"),
			FLinearColor(0.95f, 0.95f, 0.95f, 1.0f), 12, TEXT("Regular"));
		Name->SetJustification(ETextJustify::Center);
		Name->SetAutoWrapText(true);
		Name->bIsVariable = true;
		Name->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* NameSlot = CanvasStretch(Canvas, Name,
			FAnchors(0.0f, 0.0f, 1.0f, 0.0f), FMargin(6.0f, 96.0f, 6.0f, 0.0f)))
		{
			NameSlot->SetAutoSize(true); // высота — по содержимому (переносы растят плитку, Б8)
			NameSlot->SetZOrder(1);
		}

		// Цена под названием (UIMoneyColor) — вне магазина код держит её спрятанной.
		UTextBlock* Price = MakeText(Tree, Roboto, TEXT("TilePriceText"), TEXT("0"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 12, TEXT("Bold"));
		Price->SetJustification(ETextJustify::Center);
		Price->SetVisibility(ESlateVisibility::Collapsed);
		Price->bIsVariable = true;
		if (UCanvasPanelSlot* PriceSlot = CanvasStretch(Canvas, Price,
			FAnchors(0.0f, 0.0f, 1.0f, 0.0f), FMargin(6.0f, 128.0f, 6.0f, 0.0f)))
		{
			PriceSlot->SetAutoSize(true);
			PriceSlot->SetZOrder(1);
		}

		// «Не хватает монет» (ADR-049: одним потухшим цветом кнопки не обойтись) —
		// код показывает только когда нужно.
		UTextBlock* Status = MakeText(Tree, Roboto, TEXT("TileStatusText"), TEXT("Не хватает монет"),
			FLinearColor(0.85f, 0.35f, 0.3f, 1.0f), 10, TEXT("Regular"));
		Status->SetJustification(ETextJustify::Center);
		Status->SetAutoWrapText(true);
		Status->SetVisibility(ESlateVisibility::Collapsed);
		Status->bIsVariable = true;
		if (UCanvasPanelSlot* StatusSlot = CanvasStretch(Canvas, Status,
			FAnchors(0.0f, 0.0f, 1.0f, 0.0f), FMargin(6.0f, 148.0f, 6.0f, 0.0f)))
		{
			StatusSlot->SetAutoSize(true);
			StatusSlot->SetZOrder(1);
		}

		// Мини-кнопка выброса ПОВЕРХ плитки, к правому верхнему углу; в ассете спрятана:
		// показывает только рюкзак (SetDropVisible). Коробка прозрачна для попаданий, чтобы
		// со спрятанной кнопкой угол плитки оставался кликабельным.
		USizeBox* DropBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TileDropBox"));
		DropBox->SetWidthOverride(26.0f);
		DropBox->SetHeightOverride(26.0f);
		DropBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* DropBoxSlot = CanvasAuto(Canvas, DropBox,
			FVector2D(-2.0f, 2.0f), FAnchors(1.0f, 0.0f, 1.0f, 0.0f)))
		{
			DropBoxSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			DropBoxSlot->SetZOrder(3);
		}
		UButton* Drop = MakeStyledButton(Tree, TEXT("DropButton"),
			FLinearColor(0.45f, 0.15f, 0.12f, 1.0f), FLinearColor(0.58f, 0.2f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f));
		Drop->SetVisibility(ESlateVisibility::Collapsed);
		// Подпись выброса — статичная (код её не трогает, текст Рината).
		UTextBlock* DropCaption = MakeText(Tree, Roboto, TEXT("DropLabel"), TEXT("X"),
			FLinearColor::White, 11, TEXT("Bold"));
		DropCaption->SetJustification(ETextJustify::Center);
		SetButtonContent(Drop, DropCaption);
		DropBox->SetContent(Drop);
		return true;
	}

	// Размер иконки и кегль названия в слоте брони. Значения — только СТАРТОВЫЕ: с
	// Build 1.2.2 оба кубика разомкнуты в дизайнере и дальше ими владеет Ринат
	// (пересборка окна его значения сохраняет — TransferOwnerStyle переносит Brush и Font).
	constexpr float ArmorSlotIconSize = 48.0f;
	constexpr int32 ArmorSlotTextSize = 16;

	// Стартовый размер иконки в слоте оружия (дальше им владеет Ринат — кубик лежит в
	// своём канвас-слоте с ручками). Шаг между двумя слотами оружия считается от него.
	constexpr float WeaponSlotIconSize = 40.0f;

	// Слот брони paper-doll: кнопка (клик по занятому — снять) с подписью, иконкой и текстом
	// надетого. Габарит задаёт канвас-слот вызывающего (ADR-051: ручки мышкой).
	UButton* MakeArmorSlotButton(UWidgetTree* Tree, UObject* Roboto,
		const FString& StaticCaption, const FName& ButtonName, const FName& IconName, const FName& TextName)
	{
		UButton* SlotButton = MakeStyledButton(Tree, ButtonName,
			FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
			FLinearColor(0.25f, 0.27f, 0.33f, 1.0f)); // InvSlotColor + подсветки

		// Начинка слота — ВЛОЖЕННАЯ КАНВА (приёмка Рината: «пунктирная сетка не даёт менять
		// размер картинок и текста и двигать их в ячейках под броню»). Пунктир в дизайнере
		// рисовал прежний ряд-коробка: он сам расставляет детей, и ручек перетаскивания у
		// них нет — ручки даёт ТОЛЬКО канвас-слот (STransformHandle::CanResize,
		// STransformHandle.cpp:153-155), поэтому лечит не разлочка, а смена контейнера.
		// Имя кубика оставлено прежним («…Box»), чтобы не рвать контракт замков и привязки.
		UCanvasPanel* SlotBox = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			FName(*(ButtonName.ToString() + TEXT("Box"))));

		// Все трое — на канвас-слотах с якорем «левый край, середина по высоте»: слот
		// владелец растягивает как хочет, а начинка остаётся по центру и не разъезжается.
		auto PlaceInSlot = [&](UWidget* Widget, float X, bool bAutoSize, const FVector2D& Size)
		{
			if (UCanvasPanelSlot* CanvasSlot = SlotBox->AddChildToCanvas(Widget))
			{
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.5f, 0.0f, 0.5f));
				CanvasSlot->SetAlignment(FVector2D(0.0f, 0.5f));
				CanvasSlot->SetPosition(FVector2D(X, 0.0f));
				if (bAutoSize)
				{
					CanvasSlot->SetAutoSize(true);
				}
				else
				{
					CanvasSlot->SetSize(Size);
				}
			}
		};

		// Статичная подпись слота («Голова») — текст Рината, код не трогает.
		UTextBlock* Caption = MakeText(Tree, Roboto, FName(*(ButtonName.ToString() + TEXT("Caption"))),
			StaticCaption, FLinearColor(0.7f, 0.72f, 0.78f, 1.0f), 14, TEXT("Regular"));
		PlaceInSlot(Caption, 8.0f, /*bAutoSize=*/true, FVector2D::ZeroVector);

		// Иконка надетого: код прячет её на пустом слоте — в ассете сразу Collapsed.
		// Размер пишем В КИСТЬ (Brush.ImageSize): прежний SetDesiredSizeOverride трогает
		// только живой Slate-виджет и в ассет не сохраняется (Image.cpp:122-128) — из-за
		// этого иконка в слоте выходила натуральных 32 px и терялась рядом с подписью
		// (приёмка Рината). Код игры ставит текстуру с bMatchSize=false и этот размер не
		// перебивает, поэтому дальше им владеет Ринат (кубик разомкнут, см. ниже).
		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);
		FSlateBrush IconBrush;
		IconBrush.SetImageSize(FVector2D(ArmorSlotIconSize, ArmorSlotIconSize));
		Icon->SetBrush(IconBrush);
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		Icon->bIsVariable = true;
		PlaceInSlot(Icon, 88.0f, /*bAutoSize=*/false, FVector2D(ArmorSlotIconSize, ArmorSlotIconSize));

		// Что надето — ставит код («(пусто)» / имя брони).
		UTextBlock* Worn = MakeText(Tree, Roboto, TextName, TEXT("(пусто)"),
			FLinearColor::White, ArmorSlotTextSize, TEXT("Regular"));
		Worn->bIsVariable = true;
		PlaceInSlot(Worn, 88.0f + ArmorSlotIconSize + 8.0f, /*bAutoSize=*/true, FVector2D::ZeroVector);

		// Контент — в самом конце: SetButtonContent замыкает поддерево целиком,
		// включая только что добавленных детей (клик выделяет саму кнопку).
		SetButtonContent(SlotButton, SlotBox);

		// Канва сама по себе размера не имеет: у слота кнопки выравнивание по умолчанию
		// «по центру» (ButtonSlot.cpp:17-20) — с ним канва схлопнулась бы в точку и начинка
		// пропала. Растягиваем её на всю кнопку и убираем отступ 4x2, чтобы координаты
		// начинки считались от края слота.
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(SlotBox->Slot))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Fill);
			ContentSlot->SetPadding(FMargin(0.0f));
		}

		// ...и сразу СНИМАЕМ замок с трёх кубиков начинки (ADR-056, решение Рината на
		// приёмке: «не могу увеличить место под картинки в слотах брони, а также текст —
		// они залочены»). Замкнутому виджету дизайнер блокирует панель «Детали»
		// (SWidgetDetailsView.cpp:375-393) и не пускает его в сетку выделения
		// (SDesignerView.cpp:2060-2070) — именно это и мешало. Замок проверяется по
		// собственному флагу каждого виджета, обход детей от него не зависит
		// (SDesignerView.cpp:2078-2088), поэтому канва слота остаётся замкнутой, а её дети
		// выделяются. Цена: клик по иконке/тексту выделит их, а не кнопку целиком —
		// саму кнопку теперь берут за свободный край слота или из панели «Иерархия».
		Caption->SetLockedInDesigner(false);
		Icon->SetLockedInDesigner(false);
		Worn->SetLockedInDesigner(false);
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

		// Строка статов: три пары «иконка + значение». Build 1.2.1 (задача Рината «двигать
		// мышкой всё»): прежний ряд-коробка StatsRow убран — каждая иконка и каждое значение
		// в СВОЁМ канвас-слоте. Позиции повторяют раскладку ряда при образцовых текстах:
		// [монета 22][6]«0»[24][голод 22][6]«100 из 100»[24][жажда 22][6]«100 из 100»;
		// ширины образцов — оценка по метрике Roboto 16 (Slate-замер в коммандлете
		// недоступен), при пересборке фактическую геометрию владельца переносит
		// ConvertInventoryStatsRowToPairSlots. Числа в игре растут вправо; при 4+ значных
		// деньгах пара наедет на соседнюю иконку — теперь Ринат разводит их мышкой сам.
		const FLinearColor StatsColor(1.0f, 0.85f, 0.2f, 1.0f); // UIMoneyColor
		auto AddStatPair = [&](const TCHAR* IconName, const TCHAR* TexturePath,
			const TCHAR* ValueName, const TCHAR* ValueSample, float IconX, float ValueX)
		{
			// Иконка через MakeIcon: размер пишется в кисть (прежний SetDesiredSizeOverride
			// в ассет не сериализовался — иконки выходили натуральных 32 px).
			UImage* Icon = MakeIcon(Tree, TEXT("WBP_Inventory"), FName(IconName), TexturePath, 22.0f);
			if (UCanvasPanelSlot* IconSlot = CanvasAuto(PanelCanvas, Icon, FVector2D(IconX, 45.0f)))
			{
				IconSlot->SetAlignment(FVector2D(0.0f, 0.5f)); // вертикальный центр прежнего ряда
			}
			UTextBlock* Value = MakeText(Tree, Roboto, FName(ValueName), ValueSample,
				StatsColor, 16, TEXT("Regular"));
			Value->bIsVariable = true;
			if (UCanvasPanelSlot* ValueSlot = CanvasAuto(PanelCanvas, Value, FVector2D(ValueX, 45.0f)))
			{
				ValueSlot->SetAlignment(FVector2D(0.0f, 0.5f));
			}
		};
		AddStatPair(TEXT("InvMoneyIcon"), TEXT("/Game/UI/Icons/T_Icon_Money.T_Icon_Money"),
			TEXT("InvMoneyText"), TEXT("0"), 0.0f, 28.0f);
		AddStatPair(TEXT("InvHungerIcon"), TEXT("/Game/UI/Icons/T_Icon_Hunger.T_Icon_Hunger"),
			TEXT("InvHungerText"), TEXT("100 из 100"), 61.0f, 89.0f);
		AddStatPair(TEXT("InvThirstIcon"), TEXT("/Game/UI/Icons/T_Icon_Thirst.T_Icon_Thirst"),
			TEXT("InvThirstText"), TEXT("100 из 100"), 191.0f, 219.0f);

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

		// Подпись блока оружия — статичный текст Рината, код его не трогает.
		CanvasAuto(PanelCanvas, MakeText(Tree, Roboto, TEXT("WeaponLabel"), TEXT("Оружие"),
			FLinearColor::White, 15, TEXT("Regular")), FVector2D(0.0f, 319.0f));

		// ДВА слота оружия (Build 1.2.2, Ринат на приёмке: «должен быть один слот под
		// холодное оружие и один слот под огнестрельное»). Экземпляры у игрока живут оба
		// сразу, в руках один — тот помечается словами формата (WeaponInHandsFormat).
		// Каждый слот = иконка + название, оба кубика в своих канвас-слотах (ручки мышкой).
		// Текстуру и видимость иконки ставит код экрана, пустой слот — иконка спрятана,
		// в ассете сразу Collapsed. Размер иконки живёт в кисти и принадлежит Ринату.
		auto AddWeaponSlot = [&](const TCHAR* IconName, const TCHAR* TextName, float Y)
		{
			UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(IconName));
			FSlateBrush IconBrush;
			IconBrush.SetImageSize(FVector2D(WeaponSlotIconSize, WeaponSlotIconSize));
			Icon->SetBrush(IconBrush);
			Icon->SetVisibility(ESlateVisibility::Collapsed);
			Icon->bIsVariable = true;
			if (UCanvasPanelSlot* IconSlot = PanelCanvas->AddChildToCanvas(Icon))
			{
				IconSlot->SetAnchors(FAnchors());
				IconSlot->SetAlignment(FVector2D::ZeroVector);
				IconSlot->SetPosition(FVector2D(64.0f, Y - 3.0f));
				IconSlot->SetSize(FVector2D(WeaponSlotIconSize, WeaponSlotIconSize));
			}

			UTextBlock* Name = MakeText(Tree, Roboto, FName(TextName), TEXT("Пусто"),
				FLinearColor::White, 15, TEXT("Regular"));
			Name->bIsVariable = true;
			CanvasAuto(PanelCanvas, Name, FVector2D(64.0f + WeaponSlotIconSize + 8.0f, Y));
		};
		AddWeaponSlot(TEXT("RangedSlotIcon"), TEXT("RangedSlotText"), 319.0f);   // огнестрельное
		AddWeaponSlot(TEXT("MeleeSlotIcon"), TEXT("MeleeSlotText"),
			319.0f + WeaponSlotIconSize + 8.0f);                                 // холодное

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
	// затемнение + каждый элемент прежнего столбца (заголовок, строки итога, кнопки,
	// подсказка) в СВОЁМ канвас-слоте с ручками. Якорь всех элементов — точка чуть выше
	// середины экрана (0.5/0.45, где стоял центр столба); позиции Y повторяют раскладку,
	// которую столб считал сам (высота строки от кегля + прежние отступы). Build 1.2.1
	// (задача Рината «двигать мышкой всё»): ряды-коробки статистики и столб блока потерь
	// тоже разложены — каждая подпись и каждое значение в своём канвас-слоте (пары — по
	// разделителю у оси, см. AddStatPair), блок потерь — вложенный канвас LossPanel.
	// Значение пары растёт в игре вправо от оси и подпись не толкает. Внутри кнопок
	// начинка по-прежнему замкнута (кнопка двигается целиком).
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

		// Каждая строка статистики — пара «статичная подпись + значение»: подпись
		// принадлежит Ринату, код пишет только число (ADR-050). Build 1.2.1 (задача Рината
		// «двигать мышкой всё»): прежний ряд-коробка убран — каждый текст в СВОЁМ
		// канвас-слоте. Ширину текста headless не замерить (FSlateApplication при -run= не
		// создаётся), поэтому пара выравнивается по разделителю: подпись прижата правым
		// краем к точке −4 от оси, значение начинается в +4 — прежний зазор 8 и прежние Y
		// сохранены, колонка значений выходит ровной (раньше каждый ряд центрировался
		// отдельно и левые края строк гуляли).
		const FLinearColor StatColor(0.95f, 0.95f, 0.95f, 1.0f);
		auto AddStatPair = [&](const TCHAR* LabelName, const TCHAR* Caption,
			const TCHAR* ValueName, const TCHAR* ValueSample, float Y)
		{
			if (UCanvasPanelSlot* LabelSlot = CanvasAuto(Root,
				MakeText(Tree, Roboto, FName(LabelName), Caption, StatColor, 22, TEXT("Regular")),
				FVector2D(-4.0f, Y), FAnchors(0.5f, 0.45f, 0.5f, 0.45f)))
			{
				LabelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			}
			UTextBlock* Value = MakeText(Tree, Roboto, FName(ValueName), ValueSample,
				StatColor, 22, TEXT("Regular"));
			Value->bIsVariable = true;
			CanvasAuto(Root, Value, FVector2D(4.0f, Y), FAnchors(0.5f, 0.45f, 0.5f, 0.45f));
		};
		AddStatPair(TEXT("LifetimeLabel"), TEXT("Прожито"), TEXT("LifetimeText"), TEXT("00:00"), -164.0f);
		// ADR-059: KillerText — цельная фраза («Тебя убил волк»), а подпись KillerLabel экран
		// прячет кодом. Как «значение» пары фраза начиналась от центральной оси и выглядела
		// съехавшей вправо (кадр phone-dist-03-death.png) — поэтому центрируем её по оси, как
		// остальные цельные строки. Скрытую подпись оставляем на прежнем месте (ручка Рината).
		if (UCanvasPanelSlot* KillerLabelSlot = CanvasAuto(Root,
			MakeText(Tree, Roboto, TEXT("KillerLabel"), TEXT("Убийца"), StatColor, 22, TEXT("Regular")),
			FVector2D(-4.0f, -132.0f), FAnchors(0.5f, 0.45f, 0.5f, 0.45f)))
		{
			KillerLabelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		}
		UTextBlock* KillerPhrase = MakeText(Tree, Roboto, TEXT("KillerText"), TEXT("—"),
			StatColor, 22, TEXT("Regular"));
		KillerPhrase->bIsVariable = true;
		PlaceCentered(KillerPhrase, -132.0f);
		AddStatPair(TEXT("MoneyLabel"), TEXT("Монеты"), TEXT("MoneyText"), TEXT("0"), -100.0f);
		AddStatPair(TEXT("QuestsLabel"), TEXT("Квестов выполнено"), TEXT("QuestsText"), TEXT("0"), -68.0f);
		AddStatPair(TEXT("KillsLabel"), TEXT("Врагов убито"), TEXT("KillsText"), TEXT("0"), -36.0f);

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
		// Build 1.2.1 (задача Рината «двигать мышкой всё»): LossPanel — теперь ВЛОЖЕННЫЙ
		// КАНВАС в своём канвас-слоте. Код по-прежнему прячет блок целиком (LossPanel биндится
		// как UWidget — DeathScreenWidget.cpp:186), а заголовок/сетка/хвост/деньги внутри
		// лежат каждый в своём канвас-слоте с ручками. Y внутри блока повторяют прежний столб
		// (высоты строк от кегля + прежние отступы 6/4/4); цена канваса — при спрятанном
		// хвосте «и ещё N» строка денег вверх больше не подъезжает.
		const FLinearColor LossColor(0.95f, 0.55f, 0.15f, 1.0f);
		UCanvasPanel* LossPanel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LossPanel"));
		LossPanel->bIsVariable = true;
		if (UCanvasPanelSlot* LossSlot = Root->AddChildToCanvas(LossPanel))
		{
			LossSlot->SetAnchors(FAnchors(0.5f, 0.45f, 0.5f, 0.45f));
			LossSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			LossSlot->SetPosition(FVector2D(0.0f, 132.0f));
			LossSlot->SetSize(FVector2D(560.0f, 150.0f)); // вмещает сетку из 8 иконок 44 px
		}

		// Элемент блока: горизонтальный центр канваса LossPanel (как прежний HAlign_Center).
		auto PlaceLossCentered = [&](UWidget* Widget, float Y)
		{
			if (UCanvasPanelSlot* Slot = CanvasAuto(LossPanel, Widget,
				FVector2D(0.0f, Y), FAnchors(0.5f, 0.0f, 0.5f, 0.0f)))
			{
				Slot->SetAlignment(FVector2D(0.5f, 0.0f));
			}
		};

		PlaceLossCentered(MakeText(Tree, Roboto, TEXT("LossHeaderText"),
			TEXT("БУДЕТ ПОТЕРЯНО"), LossColor, 18, TEXT("Bold")), 0.0f);

		UHorizontalBox* LossGrid = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LossGrid"));
		LossGrid->bIsVariable = true;
		PlaceLossCentered(LossGrid, 30.0f); // заголовок ~24 + прежний отступ 6

		UTextBlock* LossMore = MakeText(Tree, Roboto, TEXT("LossMoreText"),
			TEXT("и ещё 3 предметов"), FLinearColor(0.8f, 0.8f, 0.8f, 1.0f), 14, TEXT("Regular"));
		LossMore->bIsVariable = true;
		LossMore->SetVisibility(ESlateVisibility::Collapsed); // видимость ведёт код
		PlaceLossCentered(LossMore, 97.0f); // сетка: иконка 44 + количество ~19 + отступ 4

		UTextBlock* LossMoney = MakeText(Tree, Roboto, TEXT("LossMoneyText"),
			TEXT("−0 монет"), LossColor, 16, TEXT("Bold"));
		LossMoney->bIsVariable = true;
		PlaceLossCentered(LossMoney, 120.0f); // хвост ~19 + отступ 4

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

	// ----------------------------------------------------------------------
	// WBP_IntroObjective — строка задачи вступления вверху по центру (переезд с холста
	// 08-09, просьба Рината «все окна на WBP»). Вид повторяет прежнюю отрисовку холста
	// (AContrarySurvivorHUD::DrawIntroObjective): плашка и цвет как у трекера задания.
	// Текст живой ставит код (UIntroObjectiveWidget), здесь образец.
	// ----------------------------------------------------------------------
	bool BuildIntroObjective(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Значения по умолчанию берём у самого окна: правка поля в C++ и пересборка ассета
		// дают один и тот же вид, расхождению взяться неоткуда.
		const UIntroObjectiveWidget* Defaults = GetDefault<UIntroObjectiveWidget>();
		const FLinearColor PlateColor = Defaults ? Defaults->PlateColor : FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);
		const FLinearColor TextColor = Defaults ? Defaults->ObjectiveColor : FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);
		const int32 FontSize = Defaults ? Defaults->ObjectiveFontSize : 34;
		const float TopMargin = Defaults ? Defaults->TopMargin : 64.0f;

		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ObjectivePlate"));
		Plate->SetBrushColor(PlateColor);
		Plate->SetPadding(FMargin(18.0f, 8.0f));
		Plate->SetHorizontalAlignment(HAlign_Center);
		Plate->SetVerticalAlignment(VAlign_Center);
		Plate->bIsVariable = true;

		UTextBlock* Objective = MakeText(Tree, Roboto, TEXT("ObjectiveText"),
			NSLOCTEXT("Intro", "ObjGoVillage", "Впереди деревня. Дойти до неё."),
			TextColor, FontSize, TEXT("Bold"));
		Objective->SetJustification(ETextJustify::Center);
		Objective->bIsVariable = true;
		Plate->SetContent(Objective);

		if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
		{
			PlateSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
			PlateSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			PlateSlot->SetPosition(FVector2D(0.0f, TopMargin));
			PlateSlot->SetAutoSize(true);
		}
		return true;
	}

	// ======================================================================
	// Магазин: WBP_Shop (геометрия и цвета — Canvas DrawShop/DrawShopSlider).
	// Строковый WBP_ShopRow УДАЛЁН (Build 1.2.2): списки заполняет общая плитка
	// WBP_ItemTile (BuildItemTile выше).
	// ======================================================================

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
			// Б8 п.4: высота была 28 — мельче предела под палец (48). Ширина заодно ровнее.
			CloseSlot->SetSize(FVector2D(96.0f, 48.0f));
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

		// Пересчёт пачек в патроны: показывается ТОЛЬКО при покупке патронов, в ассете
		// сразу Collapsed. Build 1.2.1 (задача Рината «двигать мышкой всё»): прежняя
		// обёртка SliderQtyAmmoRow убрана — в ряду жил ОДИН этот текст, отдельной подписи
		// нет, прятать контейнер незачем; при отсутствии ряда код переключает видимость
		// самого текста (ShopScreenWidget.cpp:389, ветка else if — проверено).
		UTextBlock* SliderQtyAmmo = MakeText(Tree, Roboto, TEXT("SliderQtyAmmoText"),
			TEXT("всего 30 патронов"), SliderQtyColor, 16, TEXT("Regular"));
		SliderQtyAmmo->SetVisibility(ESlateVisibility::Collapsed);
		SliderQtyAmmo->bIsVariable = true;
		CanvasAuto(SliderCanvas, SliderQtyAmmo, FVector2D(0.0f, 57.0f));

		// Ползунок: диапазон/шаг выставляет код при каждой транзакции — тут только кубик.
		// По ширине — растяжка (при ресайзе панели тянется), высота фиксированная.
		USlider* Qty = Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("QtySlider"));
		Qty->bIsVariable = true;
		CanvasStretch(SliderCanvas, Qty, FAnchors(0.0f, 0.0f, 1.0f, 0.0f),
			FMargin(0.0f, 86.0f, 0.0f, 16.0f));

		// [-] [+] и живой итог справа. Б8 п.4 (дословно «кнопки ±48 точек»): было 48x30 —
		// по высоте мельче предела под палец, теперь 56x56 с запасом; шаг по горизонтали
		// увеличен под новую ширину, иначе кнопки налезли бы друг на друга.
		auto AddSmallButton = [&](const TCHAR* ButtonName, const TCHAR* LabelName,
			const TCHAR* Caption, float X)
		{
			UButton* Small = MakeStyledButton(Tree, FName(ButtonName),
				FLinearColor(0.15f, 0.16f, 0.2f, 1.0f), FLinearColor(0.2f, 0.22f, 0.27f, 1.0f),
				FLinearColor(0.25f, 0.27f, 0.33f, 1.0f)); // InvSlotColor + подсветки
			SetButtonContent(Small, MakeText(Tree, Roboto, FName(LabelName), Caption,
				FLinearColor::White, 15, TEXT("Bold")));
			CanvasAt(SliderCanvas, Small, FVector2D(X, 114.0f), FVector2D(56.0f, 56.0f));
		};
		AddSmallButton(TEXT("QtyMinusButton"), TEXT("QtyMinusLabel"), TEXT("-"), 0.0f);
		AddSmallButton(TEXT("QtyPlusButton"), TEXT("QtyPlusLabel"), TEXT("+"), 64.0f);

		UTextBlock* SliderTotal = MakeText(Tree, Roboto, TEXT("SliderTotalText"), TEXT("Итого: 0"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 19, TEXT("Regular")); // UIMoneyColor
		SliderTotal->bIsVariable = true;
		CanvasAuto(SliderCanvas, SliderTotal, FVector2D(128.0f, 119.0f));

		// Подтверждение (Отмена красная, Подтвердить зелёная, 132x48 — Б8 п.4, высота была
		// 34 и не дотягивала до предела под палец) — якоря низ-право: при ресайзе панели
		// мышкой кнопки остаются в углу.
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
				BigSlot->SetSize(FVector2D(132.0f, 48.0f)); // SliderBigButtonWidth/Height
			}
		};
		AddBigButton(TEXT("SliderCancelButton"), TEXT("SliderCancelLabel"), TEXT("Отмена"),
			FLinearColor(0.5f, 0.12f, 0.12f, 1.0f), FLinearColor(0.62f, 0.17f, 0.16f, 1.0f),
			FLinearColor(0.7f, 0.25f, 0.2f, 1.0f), -142.0f);
		AddBigButton(TEXT("SliderConfirmButton"), TEXT("SliderConfirmLabel"), TEXT("Подтвердить"),
			FLinearColor(0.2f, 0.3f, 0.22f, 1.0f), FLinearColor(0.26f, 0.4f, 0.29f, 1.0f),
			FLinearColor(0.32f, 0.5f, 0.36f, 1.0f), 0.0f);

		// Золотая кнопка «Продать дороже» (Build 1.2, ТЗ №2). Раньше её добавлял ТОЛЬКО
		// -augment (AugmentShopSellAdButton) — пересборка -rebuild оставила бы магазин без
		// кнопки рекламы; с Build 1.2.1 она строится и здесь. Геометрия, стиль и тексты —
		// один в один с дополнением; в ассете сразу Collapsed (видимость решают условия ТЗ
		// в коде UShopScreenWidget), начинка замкнута — кнопка двигается целиком.
		UButton* SellAd = MakeStyledButton(Tree, TEXT("SellAdButton"),
			FLinearColor(0.85f, 0.62f, 0.14f, 1.0f), FLinearColor(0.95f, 0.72f, 0.2f, 1.0f),
			FLinearColor(1.0f, 0.8f, 0.3f, 1.0f)); // тёплое золото — единый цвет rewarded-кнопок
		SellAd->SetVisibility(ESlateVisibility::Collapsed);
		SellAd->bIsVariable = true;

		UHorizontalBox* SellAdRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("SellAdRow"));
		UImage* SellAdIcon = MakeIcon(Tree, TEXT("WBP_Shop"), TEXT("SellAdIcon"),
			TEXT("/Game/UI/Icons/T_Icon_AdVideo.T_Icon_AdVideo"), 24.0f);
		SellAdIcon->SetColorAndOpacity(FLinearColor(0.1f, 0.08f, 0.03f, 1.0f));
		if (UHorizontalBoxSlot* SellAdIconSlot = SellAdRow->AddChildToHorizontalBox(SellAdIcon))
		{
			SellAdIconSlot->SetVerticalAlignment(VAlign_Center);
			SellAdIconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}

		UVerticalBox* SellAdLabelBox = Tree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("SellAdLabelBox"));
		const FLinearColor GoldTextColor(0.1f, 0.08f, 0.03f, 1.0f);
		UTextBlock* SellAdPrice = MakeText(Tree, Roboto, TEXT("SellAdText"),
			NSLOCTEXT("Shop", "SellAdSample", "Продать за 225 вместо 150"),
			GoldTextColor, 15, TEXT("Bold"));
		SellAdPrice->bIsVariable = true;
		if (UVerticalBoxSlot* SellAdPriceSlot = SellAdLabelBox->AddChildToVerticalBox(SellAdPrice))
		{
			SellAdPriceSlot->SetHorizontalAlignment(HAlign_Center);
		}
		UTextBlock* SellAdSub = MakeText(Tree, Roboto, TEXT("SellAdSubText"),
			NSLOCTEXT("Shop", "SellAdSubSample", "на 50% больше за просмотр ролика"),
			GoldTextColor, 11, TEXT("Regular"));
		SellAdSub->bIsVariable = true;
		if (UVerticalBoxSlot* SellAdSubSlot = SellAdLabelBox->AddChildToVerticalBox(SellAdSub))
		{
			SellAdSubSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UHorizontalBoxSlot* SellAdLabelSlot = SellAdRow->AddChildToHorizontalBox(SellAdLabelBox))
		{
			SellAdLabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		SetButtonContent(SellAd, SellAdRow);
		if (UCanvasPanelSlot* SellAdSlot = SliderCanvas->AddChildToCanvas(SellAd))
		{
			// Низ-лево панели сделки (Отмена/Подтвердить живут в правом-нижнем углу).
			SellAdSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
			SellAdSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			SellAdSlot->SetPosition(FVector2D(0.0f, -24.0f));
			SellAdSlot->SetSize(FVector2D(280.0f, 50.0f));
		}
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

	// Б9 (издатель, вторая половина пункта): «отодвинуть полосы статов от выреза камеры».
	//
	// Штатный механизм движка — виджет безопасной зоны экрана (USafeZone). Он оборачивает
	// содержимое и сам отступает от краёв ровно настолько, сколько закрыто вырезом ИМЕННО
	// на этом устройстве; своё число подбирать не нужно, и на экране без выреза (ПК)
	// отступ равен нулю, то есть вид не меняется.
	//
	// Почему это работает на телефоне (сверено по исходникам UE 5.5, не по памяти):
	//   - GameActivity.java.template:3316-3341 — Android отдаёт движку реальные отступы
	//     выреза (DisplayCutout.getSafeInsetLeft/Top/Right/Bottom), вызывая
	//     nativeSetSafezoneInfo; это происходит, потому что мы разрешили рисовать в область
	//     выреза (bUseDisplayCutout=True в Config/DefaultEngine.ini);
	//   - AndroidWindow.cpp:183-186 — числа ложатся в GAndroidLandscapeSafezone;
	//   - AndroidApplication.cpp:186-210 — оттуда в TitleSafePaddingSize метрик экрана;
	//   - SlateApplicationBase.cpp:75-99 — оттуда в размер безопасной зоны;
	//   - SSafeZone.cpp:92-140, 186-206 — виджет берёт этот размер, делит на масштаб
	//     интерфейса (то есть отступ верен при любом DPI) и отступает от края.
	// Отступ пересчитывается на лету: Android шлёт событие смены безопасной зоны
	// (AndroidWindow.cpp:188-190), Slate по нему обновляет отступ.
	//
	// Ручку владельцу это НЕ отнимает: у слота безопасной зоны есть собственное поле
	// «Padding» в «Деталях» дизайнера — если конкретный телефон почему-то не сообщает
	// вырез, туда вписывается запасной отступ руками, а весь остальной вид не трогается.
	void WrapRootInSafeZone(UWidgetTree* Tree, const TCHAR* AssetName, bool& bChanged)
	{
		if (!Tree || !Tree->RootWidget)
		{
			return;
		}
		if (Cast<USafeZone>(Tree->RootWidget))
		{
			UE_LOG(LogGenerateWbp, Display,
				TEXT("SAFEZONE %s: корень уже безопасная зона — ничего не меняю."), AssetName);
			return; // идемпотентность: повторный прогон ничего не делает
		}

		UWidget* OldRoot = Tree->RootWidget;
		USafeZone* Zone = Tree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("SafeZone"));
		Tree->RootWidget = Zone;

		if (USafeZoneSlot* ZoneSlot = Cast<USafeZoneSlot>(Zone->AddChild(OldRoot)))
		{
			// Растяжка: прежний корень получает всю площадь за вычетом безопасной зоны,
			// поэтому вся расстановка владельца внутри него остаётся прежней — просто
			// съезжает с выреза целиком.
			ZoneSlot->SetHorizontalAlignment(HAlign_Fill);
			ZoneSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("SAFEZONE %s: прежний корень '%s' не встал в безопасную зону."),
				AssetName, *OldRoot->GetName());
			Tree->RootWidget = OldRoot; // откат: ассет остаётся прежним, а не полупустым
			return;
		}

		bChanged = true;
		UE_LOG(LogGenerateWbp, Display,
			TEXT("SAFEZONE %s: '%s' завёрнут в безопасную зону экрана (вырез камеры больше не перекрывает панель)."),
			AssetName, *OldRoot->GetName());
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

		// Б9: весь столб статов — внутри безопасной зоны экрана, иначе на телефоне с вырезом
		// под камеру полоски начинались бы прямо под глазком (мы разрешили рисовать в эту
		// область ради соотношения сторон). Делается последним: обёртка не должна мешать
		// расстановке выше.
		bool bWrapped = false;
		WrapRootInSafeZone(Tree, TEXT("WBP_PlayerStats"), bWrapped);
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
	// Окно обыска: WBP_SearchWindow, бывш. WBP_CorpseLoot (Build 1.2.1, ТЗ А1; переименован
	// оператором по ADR-077 п.8 — окно универсальное; код окна — UCorpseLootWidget)
	// ======================================================================

	// Канвас-схема «похоже на экран торговли» (Ринат): затемнение, центральная панель с
	// золотой рамкой, заголовок, крестик, список лута растяжкой, «Забрать всё» внизу.
	// Тексты кубиков ставит код окна (TitleLabel/TakeAllCaption/CloseCaption — Class
	// Defaults ассета); сетку плиток в списке создаёт код классом TileWidgetClass
	// (Build 1.2.2 — общая плитка WBP_ItemTile).
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
	// Волна 08-07 (ТЗ Рината: «все интерфейсы — редактируемыми WBP, всё двигается
	// мышкой, без дурацкой пунктирной сетки»): восемь окон, живших только кодовыми
	// деревьями, получают ассеты. Правила этой волны:
	//  - замков дизайнера НЕТ ВООБЩЕ (ни на подписях, ни на иконках, ни на коробках):
	//    клик по любому элементу выделяет именно его, панель «Детали» доступна везде;
	//    кнопку целиком берут кликом по её краю или в «Иерархии»;
	//  - контент кнопок кладётся ПРЯМЫМ SetContent (не SetButtonContent — тот ставит замки);
	//  - раскладка канвас-первая (ADR-051 п.1): плашка в канвас-слоте, внутри вложенный
	//    канвас, каждый элемент в своём слоте с ручками;
	//  - дефолтный вид повторяет кодовые деревья виджетов (те же тексты/цвета/шрифты).
	// ======================================================================

	// Затемнение под модалкой на весь экран (кубик DimBorder; Visible — ловит клик мимо кнопок).
	UBorder* MakeDimLayer(UWidgetTree* Tree, UCanvasPanel* Root, const FLinearColor& Color)
	{
		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(Color);
		Dim->SetVisibility(ESlateVisibility::Visible);
		Dim->bIsVariable = true;
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
		}
		return Dim;
	}

	// Плашка модалки (кубик PanelPlate): тёмная заливка + золотой кант, СРАЗУ в канвас-слоте
	// (ручки перетаскивания/ресайза), внутри вложенный канвас PanelCanvas для содержимого —
	// сдвиг плашки мышкой тянет всё содержимое следом (паттерн BuildDialog/BuildCorpseLoot).
	UCanvasPanel* MakeModalPlate(UWidgetTree* Tree, UCanvasPanel* Root,
		const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Pos,
		const FVector2D& Size, float PanelAlpha)
	{
		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
		Plate->SetBrush(MakeRoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, PanelAlpha), 6.0f,
			FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
		Plate->SetPadding(FMargin(0.0f));
		Plate->bIsVariable = true;
		if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
		{
			PlateSlot->SetAnchors(Anchors);
			PlateSlot->SetAlignment(Alignment);
			PlateSlot->SetPosition(Pos);
			PlateSlot->SetSize(Size);
		}
		UCanvasPanel* PanelCanvas = Tree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
		Plate->SetContent(PanelCanvas);
		return PanelCanvas;
	}

	// Текст, отцентрованный по горизонтали панели: якорь (0.5, AnchorY), авторазмер.
	UCanvasPanelSlot* CanvasCentered(UCanvasPanel* Canvas, UWidget* Widget,
		float AnchorY, const FVector2D& Alignment, const FVector2D& Pos)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(0.5f, AnchorY, 0.5f, AnchorY));
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Pos);
			Slot->SetAutoSize(true);
		}
		return Slot;
	}

	// Кнопка в канвас-слоте с явным прямоугольником, центрированная по X панели.
	void PlaceCenteredButton(UCanvasPanel* Canvas, UButton* Button,
		float AnchorY, const FVector2D& Alignment, const FVector2D& Pos, const FVector2D& Size)
	{
		if (UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(Button))
		{
			ButtonSlot->SetAnchors(FAnchors(0.5f, AnchorY, 0.5f, AnchorY));
			ButtonSlot->SetAlignment(Alignment);
			ButtonSlot->SetPosition(Pos);
			ButtonSlot->SetSize(Size);
		}
	}

	// Светло-серая кнопка (вид штатной UButton кодовых деревьев этих окон).
	UButton* MakeGreyButton(UWidgetTree* Tree, const FName& Name)
	{
		return MakeStyledButton(Tree, Name,
			FLinearColor(0.78f, 0.78f, 0.80f, 1.0f), FLinearColor(0.90f, 0.90f, 0.92f, 1.0f),
			FLinearColor(0.62f, 0.62f, 0.66f, 1.0f));
	}

	// Подпись кнопки БЕЗ замка (волна 08-07): прямой SetContent, текст свободен в дизайнере.
	UTextBlock* SetUnlockedCaption(UWidgetTree* Tree, UObject* Roboto, UButton* Button,
		const FName& Name, const FText& Caption, const FLinearColor& Color, int32 Size)
	{
		UTextBlock* Label = MakeText(Tree, Roboto, Name, Caption, Color, Size, TEXT("Bold"));
		Label->SetJustification(ETextJustify::Center);
		Label->bIsVariable = true;
		Button->SetContent(Label);
		return Label;
	}

	// ----------------------------------------------------------------------
	// WBP_DailyReward — окно «Ежедневная награда» (вид = UDailyRewardWidget::BuildCodeTree)
	// ----------------------------------------------------------------------
	bool BuildDailyReward(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Панель чуть выше середины (не спорит со статами), 440x320.
		UCanvasPanel* Panel = MakeModalPlate(Tree, Root, FAnchors(0.5f, 0.42f),
			FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(440.0f, 320.0f), 0.95f);

		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"),
			NSLOCTEXT("DailyRewardWidget", "TitleText", "ЕЖЕДНЕВНАЯ НАГРАДА"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 22, TEXT("Bold"));
		Title->bIsVariable = true;
		CanvasCentered(Panel, Title, 0.0f, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 18.0f));

		// Образцы значений — код переписывает их форматами стиля (SetupContent).
		UTextBlock* Streak = MakeText(Tree, Roboto, TEXT("StreakText"), TEXT("День серии: 1"),
			FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 17, TEXT("Regular"));
		Streak->bIsVariable = true;
		CanvasCentered(Panel, Streak, 0.0f, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 64.0f));

		UTextBlock* Reward = MakeText(Tree, Roboto, TEXT("RewardText"), TEXT("+25 монет"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 26, TEXT("Bold"));
		Reward->bIsVariable = true;
		CanvasCentered(Panel, Reward, 0.0f, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 96.0f));

		// «Забрать» — светлая кнопка, подпись свободна (без замка).
		UButton* Take = MakeGreyButton(Tree, TEXT("TakeButton"));
		SetUnlockedCaption(Tree, Roboto, Take, TEXT("TakeText"),
			NSLOCTEXT("DailyRewardWidget", "TakeButtonText", "Забрать"),
			FLinearColor(0.05f, 0.05f, 0.05f, 1.0f), 18);
		PlaceCenteredButton(Panel, Take, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -88.0f), FVector2D(200.0f, 46.0f));

		// Золотая кнопка удвоения (ТЗ №3): иконка видео + две строки. В ассете видима,
		// чтобы владельцу было что редактировать; на живом экране её ведёт SetupDoubleOffer.
		const FLinearColor Gold(0.85f, 0.62f, 0.14f, 1.0f);
		UButton* Double = MakeStyledButton(Tree, TEXT("DoubleButton"),
			Gold, Gold * 1.15f, Gold * 1.3f);

		UHorizontalBox* DoubleRow = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("DoubleRow"));
		UImage* DoubleIcon = MakeIcon(Tree, TEXT("WBP_DailyReward"), TEXT("DoubleIcon"),
			TEXT("/Game/UI/Icons/T_Icon_AdVideo.T_Icon_AdVideo"), 24.0f);
		if (UHorizontalBoxSlot* IconSlot = DoubleRow->AddChildToHorizontalBox(DoubleIcon))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(10.0f, 0.0f, 8.0f, 0.0f));
		}
		UVerticalBox* DoubleLabels = Tree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("DoubleLabels"));
		const FLinearColor GoldText(0.1f, 0.08f, 0.03f, 1.0f);
		UTextBlock* DoubleCaption = MakeText(Tree, Roboto, TEXT("DoubleText"),
			NSLOCTEXT("DailyRewardWidget", "DoubleButtonText", "Забрать вдвое больше"),
			GoldText, 17, TEXT("Bold"));
		DoubleCaption->bIsVariable = true;
		if (UVerticalBoxSlot* CaptionSlot = DoubleLabels->AddChildToVerticalBox(DoubleCaption))
		{
			CaptionSlot->SetHorizontalAlignment(HAlign_Center);
		}
		// Образец подстроки — код переписывает конкретными числами (SetupDoubleOffer).
		UTextBlock* DoubleSub = MakeText(Tree, Roboto, TEXT("DoubleSubText"),
			TEXT("50 монет вместо 25 за просмотр ролика"), GoldText, 11, TEXT("Regular"));
		DoubleSub->bIsVariable = true;
		if (UVerticalBoxSlot* SubSlot = DoubleLabels->AddChildToVerticalBox(DoubleSub))
		{
			SubSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UHorizontalBoxSlot* LabelsSlot = DoubleRow->AddChildToHorizontalBox(DoubleLabels))
		{
			LabelsSlot->SetVerticalAlignment(VAlign_Center);
			LabelsSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		}
		Double->SetContent(DoubleRow); // прямой SetContent — без замков (волна 08-07)
		PlaceCenteredButton(Panel, Double, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -16.0f), FVector2D(340.0f, 60.0f));
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_StartScreen — главное меню (вид = UStartScreenWidget::BuildCodeTree; ADR-062).
	// Пять пунктов сверху вниз + мелкие строки политики/версии внизу (спека
	// glavnoe-menu-spec.md). Видимость пунктов («Продолжить» по сейву, «Настройки» до
	// подхода 2, «Сообщество» по адресу в конфиге) ведёт код — в ассете все видимы.
	// ----------------------------------------------------------------------
	bool BuildStartScreen(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// --- Фон на весь экран. По умолчанию — сплошная заливка фирменным тёмным цветом
		// подложки иконки приложения (#271D14 в линейном виде). Картинку подставляет поле
		// настроек «Картинка фона» — код (UStartScreenWidget::ApplyStyle) кладёт её сюда сам.
		// Фон НЕПРОЗРАЧНЫЙ: до переделки его не было вовсе, и сквозь меню на телефоне была
		// видна живая игра, что расходится со спекой «фон меню статичный». ---
		const FLinearColor BrandDark(0.0203f, 0.0132f, 0.0070f, 1.0f);
		UImage* Background = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
		{
			FSlateBrush BackgroundBrush;
			BackgroundBrush.TintColor = FSlateColor(BrandDark);
			Background->SetBrush(BackgroundBrush);
		}
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		Background->bIsVariable = true;
		CanvasStretch(Root, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

		// --- Затемнение к низу, чтобы мелкие строки версии и политики читались на любой
		// картинке. Готового виджета-градиента в UMG нет, материал-ассет ради этого заводить
		// не хочется — кладём несколько полос с растущей вниз непрозрачностью. На тёмном фоне
		// ступени не видны, а работает на экране любой формы. ---
		constexpr int32 ShadeBandCount = 6;
		constexpr float ShadeHeightFraction = 0.35f;
		constexpr float ShadeMaxOpacity = 0.75f;
		for (int32 BandIndex = 0; BandIndex < ShadeBandCount; ++BandIndex)
		{
			const float BandHeight = ShadeHeightFraction / ShadeBandCount;
			const float BandTop = 1.0f - BandHeight * (BandIndex + 1);
			const float BandBottom = 1.0f - BandHeight * BandIndex;
			const float BandAlpha = ShadeMaxOpacity * (1.0f - static_cast<float>(BandIndex) / ShadeBandCount);

			UBorder* Band = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				FName(*FString::Printf(TEXT("BottomShade%d"), BandIndex)));
			Band->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, BandAlpha));
			Band->SetVisibility(ESlateVisibility::HitTestInvisible);
			Band->bIsVariable = true;
			CanvasStretch(Root, Band, FAnchors(0.0f, BandTop, 1.0f, BandBottom), FMargin(0.0f));
		}

		// Барьер касаний на весь экран: сам прозрачный (непрозрачность даёт фон выше), но
		// ловит тапы мимо кнопок, чтобы они не уходили в игру.
		MakeDimLayer(Tree, Root, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

		// --- Логотип игры СЛЕВА (просьба Рината 08-09). Текстуру подставляет поле настроек;
		// пока её нет, код прячет кубик целиком, а в ассете он остаётся видимым — владельцу
		// нужно за что-то браться мышкой. ---
		// Бледная заливка — это ЗАГЛУШКА пустого места (владельцу видно, за что браться
		// мышкой). Живую текстуру подставляет код и там же сбрасывает тинт в белый:
		// UImage::SetBrushFromTexture меняет только ресурс кисти и цвет не трогает, поэтому
		// без сброса логотип рисовался бы в четверть яркости.
		UImage* Logo = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LogoImage"));
		{
			FSlateBrush LogoBrush;
			LogoBrush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.25f));
			Logo->SetBrush(LogoBrush);
		}
		Logo->SetVisibility(ESlateVisibility::HitTestInvisible);
		Logo->bIsVariable = true;
		if (UCanvasPanelSlot* LogoSlot = Root->AddChildToCanvas(Logo))
		{
			LogoSlot->SetAnchors(FAnchors(0.0f, 0.5f, 0.0f, 0.5f));
			LogoSlot->SetAlignment(FVector2D(0.0f, 0.5f));
			LogoSlot->SetPosition(FVector2D(56.0f, 0.0f));
			// Пропорция ячейки = пропорции картинки, иначе надпись растянет. Живая текстура
			// «МАРЕВО» обрезана по надписи, 1024 на 360 (около 2.84 к 1). Ширина 700 выбрана
			// game-lead по макету Рината (название занимает около трети ширины экрана при
			// эталонных 1920), высота 246 — из той же пропорции. Квадрат 420x420 растягивал
			// надпись втрое, 420x148 держал форму, но выходил мелким.
			LogoSlot->SetSize(FVector2D(700.0f, 246.0f));
		}

		// --- Столбик кнопок СПРАВА: телефон лежит горизонтально, кнопки должны попадать под
		// большой палец правой руки. Привязка к ПРАВОМУ краю (якорь 1 по X) и к середине по
		// высоте — раскладка переживает экран другой формы, а не рассыпается.
		//
		// Переделка 08-09 по макету `concept-art/menu-mockups/menu-background-A-check.png`
		// (живой осмотр Рината): подложки под столбиком БОЛЬШЕ НЕТ — кнопки стоят прямо на
		// фоне, холодная синевато-серая рамка панели убрана. Размеры и цвета — из
		// FStartScreenStyle, то есть ровно те же, что у кодового дерева-запаски. ---
		const FStartScreenStyle MenuStyle;
		const FVector2D ButtonSize = MenuStyle.ButtonSize;
		const float ButtonStep = ButtonSize.Y + MenuStyle.ButtonSpacing;
		// Число пунктов берём из ОДНОГО источника правды о порядке — UStartScreenWidget::
		// GetMenuRowOrder. Тот же список строит кодовое дерево-запаску и проверяет автотест
		// ContrarySurvivor.SupportAuthor.MenuRowStandsBetweenSettingsAndCommunity, поэтому
		// добавить пункт в одном месте и забыть про другое уже нельзя.
		const TArray<FName> MenuRows = UStartScreenWidget::GetMenuRowOrder();
		const int32 MenuButtonCount = MenuRows.Num();
		// Столбик центрирован по высоте экрана (как на макете): верх первой кнопки — на
		// половину всей высоты столбика выше середины.
		const float ColumnTop = -0.5f * (MenuButtonCount * ButtonSize.Y
			+ (MenuButtonCount - 1) * MenuStyle.ButtonSpacing);

		// Заголовок и подпись живут НАД столбиком и в обычном меню спрятаны кодом
		// (UStartScreenWidget::ApplyChoiceLabels): на макете над кнопками пусто. В ассете
		// оставлены видимыми — они нужны переспросу «Начать заново?» и владельцу в дизайнере.
		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"),
			NSLOCTEXT("StartScreenWidget", "TitleText", "С ВОЗВРАЩЕНИЕМ"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 26, TEXT("Bold"));
		Title->SetJustification(ETextJustify::Center);
		Title->bIsVariable = true;
		if (UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(Title))
		{
			TitleSlot->SetAnchors(FAnchors(1.0f, 0.5f, 1.0f, 0.5f));
			TitleSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			TitleSlot->SetPosition(FVector2D(-MenuStyle.ButtonsRightMargin, ColumnTop - 56.0f));
			TitleSlot->SetSize(FVector2D(ButtonSize.X, 36.0f));
		}

		UTextBlock* Subtitle = MakeText(Tree, Roboto, TEXT("SubtitleText"),
			NSLOCTEXT("StartScreenWidget", "SubtitleText", "Найдено сохранение прошлой игры."),
			FLinearColor(0.85f, 0.85f, 0.85f, 1.0f), 18, TEXT("Regular"));
		Subtitle->SetAutoWrapText(true);
		Subtitle->SetJustification(ETextJustify::Center);
		Subtitle->bIsVariable = true;
		if (UCanvasPanelSlot* SubtitleSlot = Root->AddChildToCanvas(Subtitle))
		{
			SubtitleSlot->SetAnchors(FAnchors(1.0f, 0.5f, 1.0f, 0.5f));
			SubtitleSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			SubtitleSlot->SetPosition(FVector2D(-MenuStyle.ButtonsRightMargin, ColumnTop - 16.0f));
			SubtitleSlot->SetSize(FVector2D(ButtonSize.X, 34.0f));
		}

		// Подписи кнопок — образцы: живые ставит код (стиль контроллера; «Продолжить»/«Новая
		// игра» ещё и переключаются переспросом ApplyChoiceLabels/ApplyConfirmLabels).
		auto AddMenuButton = [&](const FName& ButtonName, const FName& TextName,
			const FText& Caption, int32 Index, bool bPrimary)
		{
			UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
			Button->SetStyle(UStartScreenWidget::MakeMenuButtonStyle(MenuStyle, bPrimary));
			Button->bIsVariable = true;
			SetUnlockedCaption(Tree, Roboto, Button, TextName, Caption,
				UStartScreenWidget::MenuButtonTextColor(MenuStyle, bPrimary), MenuStyle.ButtonFontSize);
			if (UCanvasPanelSlot* ButtonSlot = Root->AddChildToCanvas(Button))
			{
				ButtonSlot->SetAnchors(FAnchors(1.0f, 0.5f, 1.0f, 0.5f));
				ButtonSlot->SetAlignment(FVector2D(1.0f, 0.0f));
				ButtonSlot->SetPosition(FVector2D(-MenuStyle.ButtonsRightMargin, ColumnTop + Index * ButtonStep));
				ButtonSlot->SetSize(ButtonSize);
			}
		};
		// Пункты ставим СТРОГО в порядке GetMenuRowOrder: он один и тот же у ассета, у кодовой
		// запаски и у автотеста. Подписи берём из настроек по умолчанию — там ровно те же
		// строки, и второго места правды не заводится.
		//
		// ⛔ Выделен видом ТОЛЬКО «Продолжить». «Поддержать автора» идёт обычным пунктом, как
		// его соседи: дословно из задания издателя — «Не выделять цветом, не анимировать, не
		// делать крупнее соседей: это не главное действие в меню».
		for (int32 RowIndex = 0; RowIndex < MenuRows.Num(); ++RowIndex)
		{
			const FName ButtonName = MenuRows[RowIndex];
			// Имя кубика подписи = имя кнопки с «Button» на конце, заменённым на «Text»
			// (в проекте так у всех пунктов меню).
			FString CaptionCubeName = ButtonName.ToString();
			CaptionCubeName.RemoveFromEnd(TEXT("Button"));
			CaptionCubeName += TEXT("Text");
			const FName TextName(*CaptionCubeName);

			FText Caption;
			if (ButtonName == TEXT("ContinueButton"))      { Caption = MenuStyle.ContinueText; }
			else if (ButtonName == TEXT("NewGameButton"))  { Caption = MenuStyle.NewGameText; }
			else if (ButtonName == TEXT("SettingsButton")) { Caption = MenuStyle.SettingsText; }
			else if (ButtonName == TEXT("SupportButton"))  { Caption = MenuStyle.SupportText; }
			else if (ButtonName == TEXT("CommunityButton")){ Caption = MenuStyle.CommunityText; }
			else if (ButtonName == TEXT("ExitButton"))     { Caption = MenuStyle.ExitText; }
			else
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("BUILD WBP_StartScreen: для пункта '%s' нет подписи — пункт добавлен в GetMenuRowOrder, а сюда его не завели."),
					*ButtonName.ToString());
			}

			AddMenuButton(ButtonName, TextName, Caption, RowIndex,
				/*bPrimary=*/ButtonName == TEXT("ContinueButton"));
		}

		// Низ ЭКРАНА (спека: «мелким шрифтом, не кнопками»). Лежат не в панели, а прямо на
		// фоне — по центру нижнего края, на затемнении, чтобы читались при любой картинке.
		// Ссылка политики — прозрачная кнопка, видна только подпись (приём WBP_Consent).
		UButton* Policy = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PolicyButton"));
		Policy->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		Policy->bIsVariable = true;
		UTextBlock* PolicyCaption = MakeText(Tree, Roboto, TEXT("PolicyText"),
			NSLOCTEXT("DataConsentSettings", "PauseMenuPolicyText", "Политика конфиденциальности"),
			FLinearColor(0.55f, 0.75f, 1.0f, 1.0f), 13, TEXT("Regular"));
		PolicyCaption->SetJustification(ETextJustify::Center);
		PolicyCaption->bIsVariable = true;
		Policy->SetContent(PolicyCaption);
		CanvasCentered(Root, Policy, 1.0f, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -34.0f));

		UTextBlock* Version = MakeText(Tree, Roboto, TEXT("VersionText"), TEXT("0.0.0"),
			FLinearColor(0.6f, 0.6f, 0.6f, 1.0f), 12, TEXT("Regular"));
		Version->bIsVariable = true;
		CanvasCentered(Root, Version, 1.0f, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -12.0f));
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_Settings — экран настроек (вид = USettingsScreenWidget::BuildCodeTree; ADR-062,
	// подход 2 волны меню; спека glavnoe-menu-spec.md, раздел «Экран настроек»).
	// Раскладка в ДВА СТОЛБЦА: игра идёт в альбомной ориентации (DefaultEngine.ini,
	// Orientation=SensorLandscape), в один столбец список настроек на телефон не влезает.
	// Слева картинка, справа звук/управление/прочее. Все значения и подписи ставит код —
	// в ассете это образцы (владелец правит вид, а не тексты значений).
	// ----------------------------------------------------------------------
	bool BuildSettings(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		MakeDimLayer(Tree, Root, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));

		UCanvasPanel* Panel = MakeModalPlate(Tree, Root, FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(900.0f, 560.0f), 0.97f);

		const FLinearColor GoldColor(1.0f, 0.85f, 0.2f, 1.0f);
		const FLinearColor LabelColor(0.85f, 0.85f, 0.85f, 1.0f);
		const FLinearColor ValueColor(1.0f, 0.97f, 0.7f, 1.0f);
		const FLinearColor DarkCaption(0.05f, 0.05f, 0.05f, 1.0f);

		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"),
			NSLOCTEXT("SettingsScreenWidget", "Title", "НАСТРОЙКИ"), GoldColor, 24, TEXT("Bold"));
		Title->bIsVariable = true;
		CanvasCentered(Panel, Title, 0.0f, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 14.0f));

		// Столбцы содержимого: левый — картинка, правый — звук/управление/прочее.
		const float LeftX = 30.0f;
		const float RightX = 470.0f;
		const float TopY = 56.0f;

		auto AddHeader = [&](const TCHAR* Name, const FText& Caption, float X, float Y)
		{
			UTextBlock* Header = MakeText(Tree, Roboto, FName(Name), Caption, GoldColor, 17, TEXT("Bold"));
			Header->bIsVariable = true;
			CanvasAuto(Panel, Header, FVector2D(X, Y));
		};
		auto AddLabel = [&](const TCHAR* Name, const FText& Caption, float X, float Y)
		{
			UTextBlock* Label = MakeText(Tree, Roboto, FName(Name), Caption, LabelColor, 15, TEXT("Regular"));
			Label->bIsVariable = true;
			CanvasAuto(Panel, Label, FVector2D(X, Y));
		};
		// Образцы значений: живые строки всегда переписывает код экрана (RefreshLabels), поэтому
		// в ассете это просто текст-заполнитель — перевод ему не нужен.
		auto AddValue = [&](const TCHAR* Name, const FString& Sample, float X, float Y)
		{
			UTextBlock* Value = MakeText(Tree, Roboto, FName(Name), Sample, ValueColor, 15, TEXT("Regular"));
			Value->bIsVariable = true;
			CanvasAuto(Panel, Value, FVector2D(X, Y));
		};
		auto AddButton = [&](const TCHAR* ButtonName, const TCHAR* TextName, const FText& Caption,
			float X, float Y, const FVector2D& Size)
		{
			UButton* Button = MakeGreyButton(Tree, FName(ButtonName));
			SetUnlockedCaption(Tree, Roboto, Button, FName(TextName), Caption, DarkCaption, 16);
			CanvasAt(Panel, Button, FVector2D(X, Y), Size);
			return Button;
		};
		auto AddSlider = [&](const TCHAR* Name, float X, float Y, float Width)
		{
			USlider* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass(), FName(Name));
			Slider->bIsVariable = true;
			CanvasAt(Panel, Slider, FVector2D(X, Y), FVector2D(Width, 24.0f));
			return Slider;
		};

		// --- Левый столбец: картинка ---
		AddHeader(TEXT("GraphicsHeaderText"),
			NSLOCTEXT("SettingsScreenWidget", "GraphicsHeader", "Картинка"), LeftX, TopY);
		AddValue(TEXT("PresetValueText"), TEXT("Качество картинки: Авто (среднее)"),
			LeftX, TopY + 28.0f);

		const FVector2D PresetButtonSize(96.0f, 44.0f);
		AddButton(TEXT("PresetLowButton"), TEXT("PresetLowText"),
			NSLOCTEXT("ContrarySettings", "PresetLow", "Низкое"), LeftX, TopY + 56.0f, PresetButtonSize);
		AddButton(TEXT("PresetMediumButton"), TEXT("PresetMediumText"),
			NSLOCTEXT("ContrarySettings", "PresetMedium", "Среднее"), LeftX + 102.0f, TopY + 56.0f, PresetButtonSize);
		AddButton(TEXT("PresetHighButton"), TEXT("PresetHighText"),
			NSLOCTEXT("ContrarySettings", "PresetHigh", "Высокое"), LeftX + 204.0f, TopY + 56.0f, PresetButtonSize);
		AddButton(TEXT("PresetAutoButton"), TEXT("PresetAutoText"),
			NSLOCTEXT("ContrarySettings", "PresetAuto", "Авто"), LeftX + 306.0f, TopY + 56.0f, PresetButtonSize);

		AddLabel(TEXT("ResolutionLabelText"),
			NSLOCTEXT("SettingsScreenWidget", "ResolutionRow", "Масштаб разрешения"), LeftX, TopY + 112.0f);
		AddSlider(TEXT("ResolutionSlider"), LeftX, TopY + 138.0f, 280.0f);
		AddButton(TEXT("ResolutionMinusButton"), TEXT("ResolutionMinusText"),
			NSLOCTEXT("SettingsScreenWidget", "Minus", "−"), LeftX + 290.0f, TopY + 128.0f, FVector2D(52.0f, 44.0f));
		AddButton(TEXT("ResolutionPlusButton"), TEXT("ResolutionPlusText"),
			NSLOCTEXT("SettingsScreenWidget", "Plus", "+"), LeftX + 348.0f, TopY + 128.0f, FVector2D(52.0f, 44.0f));
		// Образец подписи: живую строку с пикселями этого экрана собирает код.
		AddValue(TEXT("ResolutionValueText"), TEXT("100% — это 720 на 1600"), LeftX, TopY + 168.0f);

		AddButton(TEXT("FrameLimitButton"), TEXT("FrameLimitText"),
			NSLOCTEXT("SettingsScreenWidget", "FrameLimitSample", "Ограничение кадров: 30 кадров"),
			LeftX, TopY + 198.0f, FVector2D(400.0f, 46.0f));
		AddButton(TEXT("FpsCounterButton"), TEXT("FpsCounterText"),
			NSLOCTEXT("SettingsScreenWidget", "FpsCounterSample", "Счётчик кадров: выключен"),
			LeftX, TopY + 250.0f, FVector2D(400.0f, 46.0f));

		// --- Правый столбец: звук, управление, прочее ---
		AddHeader(TEXT("SoundHeaderText"),
			NSLOCTEXT("SettingsScreenWidget", "SoundHeader", "Звук"), RightX, TopY);
		AddLabel(TEXT("MusicLabelText"),
			NSLOCTEXT("SettingsScreenWidget", "MusicRow", "Громкость музыки"), RightX, TopY + 28.0f);
		AddSlider(TEXT("MusicSlider"), RightX, TopY + 50.0f, 280.0f);
		AddValue(TEXT("MusicValueText"), TEXT("100%"), RightX + 292.0f, TopY + 48.0f);
		AddLabel(TEXT("EffectsLabelText"),
			NSLOCTEXT("SettingsScreenWidget", "EffectsRow", "Громкость эффектов"), RightX, TopY + 80.0f);
		AddSlider(TEXT("EffectsSlider"), RightX, TopY + 102.0f, 280.0f);
		AddValue(TEXT("EffectsValueText"), TEXT("100%"), RightX + 292.0f, TopY + 100.0f);

		AddHeader(TEXT("ControlsHeaderText"),
			NSLOCTEXT("SettingsScreenWidget", "ControlsHeader", "Управление"), RightX, TopY + 134.0f);
		AddLabel(TEXT("SensitivityLabelText"),
			NSLOCTEXT("SettingsScreenWidget", "SensitivityRow", "Чувствительность управления"), RightX, TopY + 162.0f);
		AddSlider(TEXT("SensitivitySlider"), RightX, TopY + 184.0f, 280.0f);
		AddValue(TEXT("SensitivityValueText"), TEXT("100%"), RightX + 292.0f, TopY + 182.0f);
		AddLabel(TEXT("OpacityLabelText"),
			NSLOCTEXT("SettingsScreenWidget", "OpacityRow", "Прозрачность экранных кнопок"), RightX, TopY + 214.0f);
		AddSlider(TEXT("OpacitySlider"), RightX, TopY + 236.0f, 280.0f);
		AddValue(TEXT("OpacityValueText"), TEXT("50%"), RightX + 292.0f, TopY + 234.0f);
		AddButton(TEXT("VibrationButton"), TEXT("VibrationText"),
			NSLOCTEXT("SettingsScreenWidget", "VibrationSample", "Вибрация: включена"),
			RightX, TopY + 264.0f, FVector2D(340.0f, 44.0f));

		AddHeader(TEXT("MiscHeaderText"),
			NSLOCTEXT("SettingsScreenWidget", "MiscHeader", "Прочее"), RightX, TopY + 320.0f);
		AddButton(TEXT("ReportBugButton"), TEXT("ReportBugText"),
			NSLOCTEXT("SettingsScreenWidget", "ReportBug", "Сообщить об ошибке"),
			RightX, TopY + 348.0f, FVector2D(340.0f, 44.0f));
		UTextBlock* BugHint = MakeText(Tree, Roboto, TEXT("ReportBugHintText"),
			TEXT("В сообщении укажите номер сборки: Версия 0.1.0 (сборка 1)"),
			LabelColor, 12, TEXT("Regular"));
		BugHint->SetAutoWrapText(true);
		BugHint->bIsVariable = true;
		CanvasAt(Panel, BugHint, FVector2D(RightX, TopY + 396.0f), FVector2D(340.0f, 34.0f));
		AddButton(TEXT("ResetProgressButton"), TEXT("ResetProgressText"),
			NSLOCTEXT("SettingsScreenWidget", "ResetProgress", "Сбросить прогресс"),
			RightX, TopY + 432.0f, FVector2D(340.0f, 44.0f));

		// «Назад» — по центру низа панели.
		UButton* Close = MakeGreyButton(Tree, TEXT("CloseButton"));
		SetUnlockedCaption(Tree, Roboto, Close, TEXT("CloseText"),
			NSLOCTEXT("SettingsScreenWidget", "Close", "Назад"), DarkCaption, 18);
		PlaceCenteredButton(Panel, Close, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(-230.0f, -14.0f), FVector2D(220.0f, 48.0f));

		// Панель двойного переспроса сброса. В ассете ВИДИМА (владельцу есть что править) —
		// на живом экране её прячет код при каждом показе (RefreshConfirmPanel).
		UBorder* Confirm = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmPanel"));
		Confirm->SetBrush(MakeRoundedBrush(FLinearColor(0.06f, 0.07f, 0.09f, 0.99f), 6.0f,
			FLinearColor(0.8f, 0.65f, 0.25f, 0.9f), 2.0f));
		Confirm->SetPadding(FMargin(0.0f));
		Confirm->bIsVariable = true;
		if (UCanvasPanelSlot* ConfirmSlot = Root->AddChildToCanvas(Confirm))
		{
			ConfirmSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			ConfirmSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ConfirmSlot->SetPosition(FVector2D::ZeroVector);
			ConfirmSlot->SetSize(FVector2D(460.0f, 220.0f));
		}
		UCanvasPanel* ConfirmCanvas = Tree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("ConfirmCanvas"));
		Confirm->SetContent(ConfirmCanvas);

		UTextBlock* ConfirmTitle = MakeText(Tree, Roboto, TEXT("ConfirmTitleText"),
			NSLOCTEXT("SettingsScreenWidget", "ResetAsk1", "Сбросить весь прогресс?"),
			GoldColor, 18, TEXT("Bold"));
		ConfirmTitle->SetAutoWrapText(true);
		ConfirmTitle->SetJustification(ETextJustify::Center);
		ConfirmTitle->bIsVariable = true;
		if (UCanvasPanelSlot* ConfirmTitleSlot = ConfirmCanvas->AddChildToCanvas(ConfirmTitle))
		{
			ConfirmTitleSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
			ConfirmTitleSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			ConfirmTitleSlot->SetPosition(FVector2D(0.0f, 26.0f));
			ConfirmTitleSlot->SetSize(FVector2D(400.0f, 56.0f));
		}
		UButton* ConfirmYes = MakeGreyButton(Tree, TEXT("ConfirmYesButton"));
		SetUnlockedCaption(Tree, Roboto, ConfirmYes, TEXT("ConfirmYesText"),
			NSLOCTEXT("SettingsScreenWidget", "ResetYes1", "Да, сбросить"), DarkCaption, 16);
		PlaceCenteredButton(ConfirmCanvas, ConfirmYes, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -74.0f), FVector2D(300.0f, 46.0f));
		UButton* ConfirmNo = MakeGreyButton(Tree, TEXT("ConfirmNoButton"));
		SetUnlockedCaption(Tree, Roboto, ConfirmNo, TEXT("ConfirmNoText"),
			NSLOCTEXT("SettingsScreenWidget", "ResetNo", "Отмена"), DarkCaption, 16);
		PlaceCenteredButton(ConfirmCanvas, ConfirmNo, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -18.0f), FVector2D(300.0f, 46.0f));
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_Consent — экран согласия Б6 (вид = UConsentScreenWidget::BuildCodeTree).
	// Тексты в ассете — образцы: живые всегда ставит код из настроек проекта (дословно из
	// источника истины soglasie-i-politika.md — правка ассета их не переопределяет).
	// ----------------------------------------------------------------------
	bool BuildConsent(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		// ⛔ ФОРМУЛИРОВКИ СОГЛАСИЯ БЕРЁМ ИЗ ОДНОГО МЕСТА — из настроек экрана
		// (FConsentScreenStyle), а не переписываем строки сюда второй раз. Раньше они лежали
		// здесь копией, и правка текста в коде до ассета не доезжала: именно так в ассете
		// пережила себя AppMetrica, убранная из игры требованием издателя 13.08.2026.
		const FConsentScreenStyle Texts;

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		MakeDimLayer(Tree, Root, FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));

		UCanvasPanel* Panel = MakeModalPlate(Tree, Root, FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(960.0f, 560.0f), 0.98f);

		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"), Texts.TitleText,
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 24, TEXT("Bold"));
		Title->bIsVariable = true;
		CanvasCentered(Panel, Title, 0.0f, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 24.0f));

		// Три абзаца с переносом слов в явных прямоугольниках (ручки ресайза в дизайнере).
		auto AddParagraph = [&](const TCHAR* Name, const FText& Sample, float Y, float Height)
		{
			UTextBlock* Paragraph = MakeText(Tree, Roboto, Name, Sample,
				FLinearColor(0.88f, 0.88f, 0.88f, 1.0f), 15, TEXT("Regular"));
			Paragraph->SetAutoWrapText(true);
			Paragraph->bIsVariable = true;
			if (UCanvasPanelSlot* ParagraphSlot = Panel->AddChildToCanvas(Paragraph))
			{
				ParagraphSlot->SetAnchors(FAnchors());
				ParagraphSlot->SetAlignment(FVector2D::ZeroVector);
				ParagraphSlot->SetPosition(FVector2D(30.0f, Y));
				ParagraphSlot->SetSize(FVector2D(900.0f, Height));
			}
		};
		AddParagraph(TEXT("Body1Text"), Texts.BodyText1, 76.0f, 110.0f);
		AddParagraph(TEXT("Body2Text"), Texts.BodyText2, 196.0f, 60.0f);
		AddParagraph(TEXT("Body3Text"), Texts.BodyText3, 266.0f, 50.0f);

		UButton* Accept = MakeGreyButton(Tree, TEXT("AcceptButton"));
		SetUnlockedCaption(Tree, Roboto, Accept, TEXT("AcceptText"),
			NSLOCTEXT("ConsentScreenWidget", "AcceptText", "Принимаю"),
			FLinearColor(0.05f, 0.05f, 0.05f, 1.0f), 19);
		PlaceCenteredButton(Panel, Accept, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -140.0f), FVector2D(300.0f, 58.0f));

		UButton* Decline = MakeGreyButton(Tree, TEXT("DeclineButton"));
		SetUnlockedCaption(Tree, Roboto, Decline, TEXT("DeclineText"),
			NSLOCTEXT("ConsentScreenWidget", "DeclineText", "Не сейчас"),
			FLinearColor(0.05f, 0.05f, 0.05f, 1.0f), 19);
		PlaceCenteredButton(Panel, Decline, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -72.0f), FVector2D(300.0f, 58.0f));

		// Ссылка на политику: прозрачная кнопка, видна только подпись.
		UButton* Policy = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PolicyButton"));
		Policy->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		Policy->bIsVariable = true;
		UTextBlock* PolicyCaption = MakeText(Tree, Roboto, TEXT("PolicyText"),
			NSLOCTEXT("ConsentScreenWidget", "PolicyLinkText", "Политика конфиденциальности"),
			FLinearColor(0.55f, 0.75f, 1.0f, 1.0f), 13, TEXT("Regular"));
		PolicyCaption->SetJustification(ETextJustify::Center);
		PolicyCaption->bIsVariable = true;
		Policy->SetContent(PolicyCaption);
		CanvasCentered(Panel, Policy, 1.0f, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -16.0f));
		return true;
	}

	// ADR-074: на сколько подпись о сохранении поднимается над строкой версии (обе привязаны
	// к низу панели, выравнивание по нижнему краю). Это высота строки версии плюс зазор:
	// высота строки Roboto ≈ 1.4 кегля (запас сверху, чтобы подписи не слиплись), зазор 6.
	// Одно место арифметики на BuildPauseMenu и AugmentPauseMenuSaveHint.
	float PauseSaveHintRiseAboveVersion(int32 VersionFontSize)
	{
		return FMath::CeilToFloat(FMath::Max(6, VersionFontSize) * 1.4f) + 6.0f;
	}

	// ----------------------------------------------------------------------
	// WBP_PauseMenu — меню паузы (вид = UPauseMenuWidget::BuildCodeTree). Подписи
	// согласия/политики/версии — образцы: живые ставит код из настроек и состояния (Б6).
	// ----------------------------------------------------------------------
	bool BuildPauseMenu(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		MakeDimLayer(Tree, Root, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));

		// Плашка выше прежней (было 440): подход 3 добавил пункт «В главное меню», и на
		// старой высоте нижняя кнопка налезала бы на строку версии.
		// Плашка выросла ещё раз: волна 08-09 добавила «Настройки» и «Сообщество».
		UCanvasPanel* Panel = MakeModalPlate(Tree, Root, FAnchors(0.5f, 0.45f),
			FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(380.0f, 650.0f), 0.95f);

		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"),
			NSLOCTEXT("PauseMenuWidget", "TitleText", "ПАУЗА"),
			FLinearColor(1.0f, 0.85f, 0.2f, 1.0f), 24, TEXT("Bold"));
		Title->bIsVariable = true;
		CanvasCentered(Panel, Title, 0.0f, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 22.0f));

		const FLinearColor DarkCaption(0.05f, 0.05f, 0.05f, 1.0f);
		auto AddMenuButton = [&](const TCHAR* ButtonName, const TCHAR* TextName,
			const FText& Caption, float Y) -> UButton*
		{
			UButton* Button = MakeGreyButton(Tree, FName(ButtonName));
			SetUnlockedCaption(Tree, Roboto, Button, FName(TextName), Caption, DarkCaption, 19);
			PlaceCenteredButton(Panel, Button, 0.0f, FVector2D(0.5f, 0.0f),
				FVector2D(0.0f, Y), FVector2D(280.0f, 58.0f));
			return Button;
		};
		AddMenuButton(TEXT("ResumeButton"), TEXT("ResumeText"),
			NSLOCTEXT("PauseMenuWidget", "ResumeText", "Продолжить"), 76.0f);
		// Подход 3 волны меню: возврат в главное меню — сразу под «Продолжить» и подальше от
		// «Выхода», чтобы палец не путал возврат в меню с закрытием игры.
		AddMenuButton(TEXT("MainMenuButton"), TEXT("MainMenuText"),
			NSLOCTEXT("PauseMenuWidget", "MainMenuText", "В главное меню"), 146.0f);
		// Переключателя согласия в паузе НЕТ (решение Рината 08-08) — и кодовое дерево его
		// больше не создаёт (08-09). Раньше он оставался невидимым кубиком и лежал ровно под
		// «В главное меню»; пересборка воскресила бы наложение, если оставить его здесь.
		// Волна 08-09 (просьба Рината): настройки и сообщество прямо из паузы.
		AddMenuButton(TEXT("SettingsButton"), TEXT("SettingsText"),
			NSLOCTEXT("PauseMenuWidget", "SettingsText", "Настройки"), 216.0f);
		AddMenuButton(TEXT("CommunityButton"), TEXT("CommunityText"),
			NSLOCTEXT("PauseMenuWidget", "CommunityText", "Сообщество"), 286.0f);
		// Подпись политики — своим кеглем: длинной надписью общий кегль 19 не помещался в
		// кнопку, и на телефоне она была обрезана (жалоба Рината 08-09).
		if (UButton* Policy = AddMenuButton(TEXT("PolicyButton"), TEXT("PolicyText"),
			NSLOCTEXT("PauseMenuWidget", "PolicySample", "Политика конфиденциальности"), 356.0f))
		{
			if (UTextBlock* PolicyCaption = Cast<UTextBlock>(Policy->GetContent()))
			{
				PolicyCaption->SetFont(FSlateFontInfo(Roboto, 14, TEXT("Bold")));
				PolicyCaption->SetAutoWrapText(true);
				PolicyCaption->SetJustification(ETextJustify::Center);
			}
		}
		AddMenuButton(TEXT("QuitButton"), TEXT("QuitText"),
			NSLOCTEXT("PauseMenuWidget", "QuitText", "Выход"), 426.0f);

		// Номер версии сборки — мелко внизу (живой текст ставит код).
		UTextBlock* Version = MakeText(Tree, Roboto, TEXT("VersionText"), TEXT("0.0.0"),
			FLinearColor(0.6f, 0.6f, 0.6f, 1.0f), 12, TEXT("Regular"));
		Version->bIsVariable = true;
		CanvasCentered(Panel, Version, 1.0f, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -14.0f));

		// ADR-074: постоянная подпись о сохранении — НАД строкой версии, с той же привязкой к
		// низу. Текст, кегль и цвет — из FPauseMenuStyle (одно место правды на код и ассет).
		// В живой ассет (окно отдано владельцу) она приезжает дополнением
		// AugmentPauseMenuSaveHint, здесь — вид «с нуля» для полноты кодового пути.
		const FPauseMenuStyle PauseStyle;
		UTextBlock* SaveHint = MakeText(Tree, Roboto, TEXT("SaveHintText"), PauseStyle.SaveHintText,
			PauseStyle.SaveHintColor, PauseStyle.SaveHintFontSize, TEXT("Regular"));
		SaveHint->SetJustification(ETextJustify::Center);
		SaveHint->bIsVariable = true;
		CanvasCentered(Panel, SaveHint, 1.0f, FVector2D(0.5f, 1.0f),
			FVector2D(0.0f, -14.0f - PauseSaveHintRiseAboveVersion(12)));
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_Intro — интро-экран (вид = UIntroScreenWidget::BuildCodeTree). Тексты и
	// прозрачности ведёт контроллер (это и есть анимация интро); владельцу — шрифты/позиции.
	// ----------------------------------------------------------------------
	bool BuildIntro(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Чёрный фон на весь экран (процедурная кисть — без текстуры).
		UImage* Background = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
		Background->SetBrush(MakeRoundedBrush(FLinearColor::Black, 0.0f));
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		Background->bIsVariable = true;
		if (UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Background))
		{
			BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BgSlot->SetOffsets(FMargin(0.0f));
		}

		// Крупная строка по центру: центральная полоса ~70% ширины, авто-перенос.
		UTextBlock* Line = MakeText(Tree, Roboto, TEXT("LineText"), TEXT("Строка интро."),
			FLinearColor::White, 42, TEXT("Regular"));
		Line->SetJustification(ETextJustify::Center);
		Line->SetAutoWrapText(true);
		Line->SetVisibility(ESlateVisibility::HitTestInvisible);
		Line->bIsVariable = true;
		CanvasStretch(Root, Line, FAnchors(0.15f, 0.42f, 0.85f, 0.58f), FMargin(0.0f));

		// Подсказка пропуска внизу. Collapsed — как в кодовом дереве: показывает её только
		// SetSkipHint при повторных заходах, иначе образец висел бы поверх интро.
		UTextBlock* SkipHint = MakeText(Tree, Roboto, TEXT("SkipHintText"),
			TEXT("Зажми, чтобы пропустить"), FLinearColor(0.8f, 0.8f, 0.82f, 1.0f), 22, TEXT("Regular"));
		SkipHint->SetJustification(ETextJustify::Center);
		SkipHint->SetVisibility(ESlateVisibility::Collapsed);
		SkipHint->bIsVariable = true;
		CanvasStretch(Root, SkipHint, FAnchors(0.2f, 0.88f, 0.8f, 0.96f), FMargin(0.0f));
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_OnboardingHint — тост-подсказка обучения (вид = UOnboardingHintWidget::BuildCodeTree).
	// Текст подсказки ставит код (шаг обучения); владельцу — плашка, шрифт, позиция.
	// ----------------------------------------------------------------------
	bool BuildOnboardingHint(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HintPlate"));
		Plate->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		Plate->SetPadding(FMargin(18.0f, 12.0f));
		Plate->bIsVariable = true;

		UTextBlock* Hint = MakeText(Tree, Roboto, TEXT("HintText"),
			TEXT("Здесь появляется подсказка обучения."),
			FLinearColor(1.0f, 0.95f, 0.5f, 1.0f), 18, TEXT("Bold"));
		Hint->SetAutoWrapText(true);
		Hint->SetJustification(ETextJustify::Center);
		Hint->bIsVariable = true;
		Plate->SetContent(Hint);

		// Верх-центр (0.5 / 0.10), фиксированная ширина — как FOnboardingHintStyle по умолчанию.
		if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
		{
			PlateSlot->SetAnchors(FAnchors(0.5f, 0.10f, 0.5f, 0.10f));
			PlateSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			PlateSlot->SetPosition(FVector2D::ZeroVector);
			PlateSlot->SetSize(FVector2D(820.0f, 96.0f));
		}
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_LimpIndicator — плашка «Ранен» (вид = ULimpIndicatorWidget::BuildCodeTree).
	// Текст ставит код; видимостью управляет NativeTick через базу USelfHidingWidget.
	// ----------------------------------------------------------------------
	bool BuildLimpIndicator(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		USizeBox* WidthBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WidthBox"));
		WidthBox->SetWidthOverride(344.0f);
		WidthBox->bIsVariable = true;

		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Plate"));
		Plate->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		Plate->SetPadding(FMargin(12.0f, 8.0f));
		Plate->bIsVariable = true;

		UTextBlock* Indicator = MakeText(Tree, Roboto, TEXT("IndicatorText"),
			TEXT("Ранен: скорость снижена"),
			FLinearColor(1.0f, 0.45f, 0.35f, 1.0f), 15, TEXT("Bold"));
		Indicator->SetAutoWrapText(true);
		Indicator->bIsVariable = true;

		Plate->SetContent(Indicator);
		WidthBox->SetContent(Plate);

		// Под стеком статов (Canvas-метрики: отступ 24, стек ~180 px) — дефолт стиля.
		if (UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(WidthBox))
		{
			BoxSlot->SetAnchors(FAnchors());
			BoxSlot->SetAlignment(FVector2D::ZeroVector);
			BoxSlot->SetPosition(FVector2D(24.0f, 210.0f));
			BoxSlot->SetAutoSize(true); // высота — по тексту; ширину держит SizeBox
		}
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_MockAd — экран-заглушка рекламного ролика (вид = UMockAdWidget::BuildCodeTree).
	// Отсчёт и появление кнопки закрытия ведёт код (NativeTick).
	// ----------------------------------------------------------------------
	bool BuildMockAd(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// Непрозрачный тёмный фон: «ролик» перекрывает игру целиком.
		MakeDimLayer(Tree, Root, FLinearColor(0.02f, 0.02f, 0.03f, 0.97f));

		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"),
			NSLOCTEXT("MockAd", "Title", "Здесь будет рекламный ролик"),
			FLinearColor(0.95f, 0.96f, 1.0f, 1.0f), 30, TEXT("Bold"));
		Title->bIsVariable = true;
		CanvasCentered(Root, Title, 0.5f, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -70.0f));

		// Образцы: placement и отсчёт переписывает код (SetPlacement / NativeTick).
		UTextBlock* Placement = MakeText(Tree, Roboto, TEXT("PlacementText"), TEXT("placement"),
			FLinearColor(0.55f, 0.57f, 0.62f, 1.0f), 12, TEXT("Regular"));
		Placement->bIsVariable = true;
		CanvasCentered(Root, Placement, 0.5f, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -44.0f));

		UTextBlock* Countdown = MakeText(Tree, Roboto, TEXT("CountdownText"), TEXT("Ролик идёт… 3"),
			FLinearColor(0.8f, 0.8f, 0.85f, 1.0f), 18, TEXT("Regular"));
		Countdown->bIsVariable = true;
		CanvasCentered(Root, Countdown, 0.5f, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 8.0f));

		// Кнопка в ассете видима (владельцу есть что редактировать); на живом экране её
		// прячет NativeOnInitialized и показывает конец отсчёта.
		UButton* Close = MakeGreyButton(Tree, TEXT("CloseButton"));
		SetUnlockedCaption(Tree, Roboto, Close, TEXT("CloseText"),
			NSLOCTEXT("MockAd", "Close", "Закрыть"),
			FLinearColor(0.05f, 0.05f, 0.05f, 1.0f), 18);
		PlaceCenteredButton(Root, Close, 0.5f, FVector2D(0.5f, 0.0f),
			FVector2D(0.0f, 48.0f), FVector2D(180.0f, 48.0f));
		return true;
	}

	// ----------------------------------------------------------------------
	// WBP_SupportAuthor — окно «Поддержать автора» (задание издателя; вид одобрен Ринатом
	// живьём 12.08.2026, повторяем его один в один).
	//
	// ⛔ РАСКЛАДКА ПЛОСКАЯ, БЕЗ ЕДИНОЙ ОБЁРТКИ — это прямое требование Рината: «чтобы там всё
	// легко мышкой и клавиатурой настраивалось и двигалось (без пунктирной таблицы-подложки,
	// которая всё блокирует)». Пунктирную сетку в дизайнере и запрет таскать элементы мышкой
	// даёт КОНТЕЙНЕР С АВТОРАСКЛАДКОЙ (VerticalBox/HorizontalBox/SizeBox): положение ребёнка
	// там считает родитель, поэтому мышке двигать нечего. Поэтому здесь:
	//   • каждый элемент — прямой ребёнок корневого холста со своим положением и размером;
	//   • ни VerticalBox, ни HorizontalBox, ни SizeBox;
	//   • затемнение и подложка окна — СОСЕДНИЕ элементы на дне холста, а не родители
	//     остальных (они лежат первыми, значит рисуются ниже и выделению не мешают).
	// Единственное вложение, которое остаётся, — подпись ВНУТРИ своей кнопки: иначе спрятанная
	// кнопка оставила бы подпись висеть в воздухе (ловушка скрытия, урок AmmoRow). Так устроены
	// подписи кнопок во всех окнах проекта.
	//
	// Кубики названы ровно так, как объявлены поля BindWidgetOptional в USupportAuthorWidget:
	// разойдись имена — привязка молча не сойдётся и окно приедет пустым.
	//
	// Все цвета, размеры, шрифты и тексты берутся из FSupportAuthorStyle по умолчанию и из
	// USupportAuthorWidget::MakeSupportButtonStyle — то есть из ТОГО ЖЕ места, что и одобренная
	// кодовая запаска. Своего вкуса здесь нет ни в одном значении.
	// ----------------------------------------------------------------------
	bool BuildSupportAuthor(UWidgetTree* Tree)
	{
		UObject* Roboto = LoadRobotoFont();
		const FSupportAuthorStyle Style; // одобренный вид: одно место правды на код и ассет

		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		// --- Дно холста: затемнение и подложка. Оба добавляются ПЕРВЫМИ, поэтому рисуются
		// ниже содержимого и в дизайнере не перехватывают щелчок по кнопкам. ---

		// Затемнение на весь экран. Visible (не HitTestInvisible): в игре оно ловит касания
		// мимо окна, чтобы они не уходили в мир.
		MakeDimLayer(Tree, Root, Style.DimColor);

		// Подложка окна: заливка + золотой кант одной кистью. Отдельной рамки-родителя нет —
		// кант рисует сама кисть, вкладывать ради него второй Border не нужно.
		const FVector2D WindowSize(680.0f, 420.0f);
		UBorder* Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelPlate"));
		Plate->SetBrush(MakeRoundedBrush(Style.PanelColor, 6.0f, Style.FrameColor, 2.0f));
		Plate->SetPadding(FMargin(0.0f));
		Plate->bIsVariable = true;
		if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
		{
			PlateSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			PlateSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PlateSlot->SetPosition(FVector2D::ZeroVector);
			PlateSlot->SetSize(WindowSize);
		}

		// --- Содержимое. Все элементы привязаны к СЕРЕДИНЕ экрана теми же якорями, что и
		// подложка: раскладка переживает экран другой формы, а не рассыпается. Координаты
		// отсчитываются от центра, поэтому левый край содержимого отрицательный. ---
		//
		// Поля внутри окна повторяют одобренную запаску: 28 точек по бокам и 24 сверху/снизу
		// (в запаске это были отступы Border'ов 2+26 и 2+22).
		constexpr float PadX = 28.0f;
		constexpr float PadY = 24.0f;
		const float ContentLeft = -0.5f * WindowSize.X + PadX;          // -312
		const float ContentRight = 0.5f * WindowSize.X - PadX;          //  312
		const float ContentWidth = ContentRight - ContentLeft;          //  624
		const float ContentTop = -0.5f * WindowSize.Y + PadY;           // -186

		// Кладёт элемент в холст по прямоугольнику, отсчитанному от центра экрана.
		auto PlaceCentered = [&](UWidget* Widget, const FVector2D& Pos, const FVector2D& Size)
		{
			if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget))
			{
				Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				Slot->SetAlignment(FVector2D::ZeroVector);
				Slot->SetPosition(Pos);
				Slot->SetSize(Size);
			}
		};

		// Крестик закрытия — первая строка, прижат к правому краю содержимого. Квадрат 48 на 48:
		// нижняя граница области нажатия под палец (требование Б8), её же держит код окна.
		const float CloseSize = USupportAuthorWidget::GetMinTouchSizePx();
		UButton* Close = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		Close->SetStyle(USupportAuthorWidget::MakeSupportButtonStyle(Style));
		Close->bIsVariable = true;
		SetUnlockedCaption(Tree, Roboto, Close, TEXT("CloseText"), Style.CloseText,
			Style.ButtonTextColor, Style.ButtonFontSize);
		PlaceCentered(Close, FVector2D(ContentRight - CloseSize, ContentTop),
			FVector2D(CloseSize, CloseSize));

		// Заголовок. Размер задан явно (а не по содержимому), чтобы владельцу было за что
		// взяться мышкой; по центру строку держит выключка, а не контейнер.
		const float TitleTop = ContentTop + CloseSize + 6.0f;
		constexpr float TitleHeight = 32.0f;
		UTextBlock* Title = MakeText(Tree, Roboto, TEXT("TitleText"), Style.TitleText,
			Style.TitleColor, Style.TitleFontSize, TEXT("Bold"));
		Title->SetJustification(ETextJustify::Center);
		Title->bIsVariable = true;
		PlaceCentered(Title, FVector2D(ContentLeft, TitleTop), FVector2D(ContentWidth, TitleHeight));

		// Пояснение. Переносится по словам: строка длинная, в одну не влезает.
		const float MessageTop = TitleTop + TitleHeight + 14.0f;
		constexpr float MessageHeight = 52.0f;
		UTextBlock* Message = MakeText(Tree, Roboto, TEXT("MessageText"), Style.MessageText,
			Style.MessageColor, Style.MessageFontSize, TEXT("Regular"));
		Message->SetJustification(ETextJustify::Center);
		Message->SetAutoWrapText(true);
		Message->bIsVariable = true;
		PlaceCentered(Message, FVector2D(ContentLeft, MessageTop), FVector2D(ContentWidth, MessageHeight));

		// ⛔ ДВЕ КНОПКИ ОКНА. Порядок менять нельзя (условие задания): сначала «Посмотреть
		// рекламу», под ней «Другие способы поддержать». Размер и стиль у них ОДИН И ТОТ ЖЕ —
		// оба берутся из одних и тех же полей, разойтись им нечем.
		const FVector2D ButtonSize(
			FMath::Max(USupportAuthorWidget::GetMinTouchSizePx(), Style.ButtonSize.X),
			FMath::Max(USupportAuthorWidget::GetMinTouchSizePx(), Style.ButtonSize.Y));
		const float ButtonLeft = -0.5f * ButtonSize.X; // обе кнопки по центру окна
		const float WatchTop = MessageTop + MessageHeight + 22.0f;
		const float LinkTop = WatchTop + ButtonSize.Y + 12.0f;

		auto AddWindowButton = [&](const FName& ButtonName, const FName& TextName,
			const FText& Caption, float Top)
		{
			UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
			Button->SetStyle(USupportAuthorWidget::MakeSupportButtonStyle(Style));
			Button->bIsVariable = true;
			SetUnlockedCaption(Tree, Roboto, Button, TextName, Caption,
				Style.ButtonTextColor, Style.ButtonFontSize);
			PlaceCentered(Button, FVector2D(ButtonLeft, Top), ButtonSize);
		};
		AddWindowButton(TEXT("WatchAdButton"), TEXT("WatchAdText"), Style.WatchAdText, WatchTop);
		AddWindowButton(TEXT("SupportLinkButton"), TEXT("SupportLinkText"), Style.SupportLinkText, LinkTop);

		// Строка благодарности после просмотра. В АССЕТЕ оставлена видимой, чтобы владельцу
		// было за что взяться мышкой (тот же приём, что у логотипа главного меню и кнопки
		// заглушки ролика); на живом экране её прячет код окна и показывает только после
		// досмотренного ролика.
		const float ThanksTop = LinkTop + ButtonSize.Y + 12.0f;
		UTextBlock* Thanks = MakeText(Tree, Roboto, TEXT("ThanksText"), Style.ThanksText,
			Style.TitleColor, Style.MessageFontSize, TEXT("Regular"));
		Thanks->SetJustification(ETextJustify::Center);
		Thanks->bIsVariable = true;
		PlaceCentered(Thanks, FVector2D(ContentLeft, ThanksTop), FVector2D(ContentWidth, 24.0f));

		// Самопроверка раскладки: содержимое обязано помещаться в подложку. Ловится здесь, а не
		// глазами на телефоне.
		const float ContentBottom = ThanksTop + 24.0f;
		if (ContentBottom > 0.5f * WindowSize.Y - PadY + KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("BUILD WBP_SupportAuthor: содержимое кончается на %.0f, а нижнее поле окна на %.0f — окно надо сделать выше."),
				ContentBottom, 0.5f * WindowSize.Y - PadY);
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

		// ⛔ ОКНО ОТДАНО ВЛАДЕЛЬЦУ (правило Рината 08-09: «я дальше сам настрою размер кнопок»).
		// Такое окно генератор больше НЕ трогает: не пересобирает (-rebuild), не перезаписывает
		// даже с -force. Единственное, что ему остаётся, — СОЗДАТЬ окно, если файла нет вовсе,
		// и дополнить недостающими кубиками через -augment (тот раскладку не переставляет).
		// Иначе выйдет так: Ринат двигает кнопки мышкой, а следующий наш прогон стирает работу.
		// Проверка -verify по такому окну смотрит только состав (все ли кубики на месте и
		// доступны ли они как переменные), но НЕ придирается к их положению и размеру.
		bool bOwnerOwned = false;
	};

	// Порядок важен: WBP_ItemTile ДО экранов с сетками (WBP_Inventory/WBP_Shop/
	// WBP_SearchWindow — им на CDO назначается класс плитки). Пустые заготовки Рината
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
		// Build 1.2.2: общая плитка предмета (инвентарь/магазин/обыск) вместо строковых
		// WBP_InventoryRow/WBP_ShopRow — кубики по BindWidgetOptional-полям UItemTileWidget.
		{ TEXT("/Game/UI/WBP_ItemTile"), TEXT("WBP_ItemTile"),
			TEXT("/Script/ContrarySurvivor.ItemTileWidget"), &BuildItemTile,
			{ TEXT("TileSizeBox"), TEXT("TileCanvas"), TEXT("TileButton"),
			  TEXT("TileIconBox"), TEXT("TileIcon"),
			  TEXT("TileCountText"), TEXT("TileNameText"), TEXT("TilePriceText"),
			  TEXT("TileStatusText"), TEXT("DropButton") } },
		{ TEXT("/Game/UI/WBP_Inventory"), TEXT("WBP_Inventory"),
			TEXT("/Script/ContrarySurvivor.InventoryScreenWidget"), &BuildInventory,
			{ TEXT("InvMoneyText"), TEXT("InvHungerText"), TEXT("InvThirstText"),
			  TEXT("HeadSlotButton"), TEXT("HeadSlotText"), TEXT("HeadSlotIcon"),
			  TEXT("TorsoSlotButton"), TEXT("TorsoSlotText"), TEXT("TorsoSlotIcon"),
			  TEXT("LegsSlotButton"), TEXT("LegsSlotText"), TEXT("LegsSlotIcon"),
			  TEXT("ProtectionText"),
			  TEXT("RangedSlotText"), TEXT("RangedSlotIcon"),
			  TEXT("MeleeSlotText"), TEXT("MeleeSlotIcon"),
			  TEXT("BackpackList"), TEXT("CloseButton") } },
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
		// Строка задачи вступления (08-09): последний игровой экран, живший только холстом.
		{ TEXT("/Game/UI/WBP_IntroObjective"), TEXT("WBP_IntroObjective"),
			TEXT("/Script/ContrarySurvivor.IntroObjectiveWidget"), &BuildIntroObjective,
			{ TEXT("ObjectivePlate"), TEXT("ObjectiveText") } },
		// Build 1.2.1: обёртки SliderQtyAmmoRow в контракте больше нет (текст пересчёта
		// лежит в своём канвас-слоте, код прячет его напрямую); золотая кнопка теперь
		// строится в BuildShop — её кубики вошли в контракт.
		{ TEXT("/Game/UI/WBP_Shop"), TEXT("WBP_Shop"),
			TEXT("/Script/ContrarySurvivor.ShopScreenWidget"), &BuildShop,
			{ TEXT("MoneyText"), TEXT("BuyList"), TEXT("SellList"), TEXT("CloseButton"),
			  TEXT("SliderPanel"), TEXT("SliderTitleText"), TEXT("SliderQtyText"),
			  TEXT("SliderQtyAmmoText"), TEXT("QtySlider"),
			  TEXT("QtyMinusButton"), TEXT("QtyPlusButton"), TEXT("SliderTotalText"),
			  TEXT("SliderConfirmButton"), TEXT("SliderCancelButton"),
			  TEXT("SellAdButton"), TEXT("SellAdText"), TEXT("SellAdSubText") } },
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
		// UCorpseLootWidget (сетку плиток создаёт код классом TileWidgetClass, Build 1.2.2).
		// ADR-077 п.8: ассет переименован оператором в WBP_SearchWindow (универсальное окно
		// обыска) БЕЗ редиректора — старый путь /Game/UI/WBP_CorpseLoot мёртв. Класс кода
		// остался UCorpseLootWidget (переименование класса не согласовано — только ассет).
		{ TEXT("/Game/UI/WBP_SearchWindow"), TEXT("WBP_SearchWindow"),
			TEXT("/Script/ContrarySurvivor.CorpseLootWidget"), &BuildCorpseLoot,
			{ TEXT("TitleText"), TEXT("LootList"),
			  TEXT("TakeAllButton"), TEXT("TakeAllText"),
			  TEXT("CloseButton"), TEXT("CloseText") } },
		// --- Волна 08-07 (ТЗ Рината): окна, жившие только кодовыми деревьями. Все ассеты
		// БЕЗ замков дизайнера вовсе — контракты в GLockContracts с пустым списком замков. ---
		{ TEXT("/Game/UI/WBP_DailyReward"), TEXT("WBP_DailyReward"),
			TEXT("/Script/ContrarySurvivor.DailyRewardWidget"), &BuildDailyReward,
			{ TEXT("PanelPlate"), TEXT("TitleText"), TEXT("StreakText"), TEXT("RewardText"),
			  TEXT("TakeButton"), TEXT("TakeText"),
			  TEXT("DoubleButton"), TEXT("DoubleText"), TEXT("DoubleSubText") } },
		// 08-09 (макет menu-background-A-check.png): подложки PanelPlate у меню больше нет —
		// кнопки стоят прямо на фоне.
		{ TEXT("/Game/UI/WBP_StartScreen"), TEXT("WBP_StartScreen"),
			TEXT("/Script/ContrarySurvivor.StartScreenWidget"), &BuildStartScreen,
			{ TEXT("BackgroundImage"), TEXT("LogoImage"),
			  TEXT("DimBorder"), TEXT("TitleText"), TEXT("SubtitleText"),
			  TEXT("ContinueButton"), TEXT("ContinueText"),
			  TEXT("NewGameButton"), TEXT("NewGameText"),
			  TEXT("SettingsButton"), TEXT("SettingsText"),
			  // Задание издателя 11.08.2026: пункт «Поддержать автора» стоит СТРОГО между
			  // «Настройки» и «Сообщество». Здесь это список состава (порядок в нём роли не
			  // играет), сам порядок задаётся в BuildStartScreen по GetMenuRowOrder.
			  TEXT("SupportButton"), TEXT("SupportText"),
			  TEXT("CommunityButton"), TEXT("CommunityText"),
			  TEXT("ExitButton"), TEXT("ExitText"),
			  TEXT("PolicyButton"), TEXT("PolicyText"), TEXT("VersionText") } },
		// ADR-062, подход 2: экран настроек. Кубики — по BindWidgetOptional-полям
		// USettingsScreenWidget (Source/ContrarySurvivor/UI/SettingsScreenWidget.h).
		{ TEXT("/Game/UI/WBP_Settings"), TEXT("WBP_Settings"),
			TEXT("/Script/ContrarySurvivor.SettingsScreenWidget"), &BuildSettings,
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"),
			  TEXT("GraphicsHeaderText"), TEXT("PresetValueText"),
			  TEXT("PresetLowButton"), TEXT("PresetLowText"),
			  TEXT("PresetMediumButton"), TEXT("PresetMediumText"),
			  TEXT("PresetHighButton"), TEXT("PresetHighText"),
			  TEXT("PresetAutoButton"), TEXT("PresetAutoText"),
			  TEXT("ResolutionLabelText"), TEXT("ResolutionSlider"), TEXT("ResolutionValueText"),
			  TEXT("ResolutionMinusButton"), TEXT("ResolutionMinusText"),
			  TEXT("ResolutionPlusButton"), TEXT("ResolutionPlusText"),
			  TEXT("FrameLimitButton"), TEXT("FrameLimitText"),
			  TEXT("FpsCounterButton"), TEXT("FpsCounterText"),
			  TEXT("SoundHeaderText"), TEXT("MusicLabelText"), TEXT("MusicSlider"), TEXT("MusicValueText"),
			  TEXT("EffectsLabelText"), TEXT("EffectsSlider"), TEXT("EffectsValueText"),
			  TEXT("ControlsHeaderText"),
			  TEXT("SensitivityLabelText"), TEXT("SensitivitySlider"), TEXT("SensitivityValueText"),
			  TEXT("OpacityLabelText"), TEXT("OpacitySlider"), TEXT("OpacityValueText"),
			  TEXT("VibrationButton"), TEXT("VibrationText"),
			  TEXT("MiscHeaderText"),
			  TEXT("ReportBugButton"), TEXT("ReportBugText"), TEXT("ReportBugHintText"),
			  TEXT("ResetProgressButton"), TEXT("ResetProgressText"),
			  TEXT("CloseButton"), TEXT("CloseText"),
			  TEXT("ConfirmPanel"), TEXT("ConfirmTitleText"),
			  TEXT("ConfirmYesButton"), TEXT("ConfirmYesText"),
			  TEXT("ConfirmNoButton"), TEXT("ConfirmNoText") },
			/*bOwnerOwned=*/true },
		{ TEXT("/Game/UI/WBP_Consent"), TEXT("WBP_Consent"),
			TEXT("/Script/ContrarySurvivor.ConsentScreenWidget"), &BuildConsent,
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"),
			  TEXT("Body1Text"), TEXT("Body2Text"), TEXT("Body3Text"),
			  TEXT("AcceptButton"), TEXT("AcceptText"), TEXT("DeclineButton"), TEXT("DeclineText"),
			  TEXT("PolicyButton"), TEXT("PolicyText") } },
		{ TEXT("/Game/UI/WBP_PauseMenu"), TEXT("WBP_PauseMenu"),
			TEXT("/Script/ContrarySurvivor.PauseMenuWidget"), &BuildPauseMenu,
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"),
			  TEXT("ResumeButton"), TEXT("ResumeText"),
			  TEXT("MainMenuButton"), TEXT("MainMenuText"),
			  TEXT("SettingsButton"), TEXT("SettingsText"),
			  TEXT("CommunityButton"), TEXT("CommunityText"),
			  // Задание издателя 11.08.2026: строка «Поддержать автора» рядом с «Сообщество» и
			  // «Политика конфиденциальности». Окно ОТДАНО ВЛАДЕЛЬЦУ — сюда строка приезжает
			  // только режимом -augment (AugmentPauseMenuSupport), пересборка запрещена.
			  TEXT("SupportButton"), TEXT("SupportText"),
			  TEXT("PolicyButton"), TEXT("PolicyText"), TEXT("QuitButton"), TEXT("QuitText"),
			  TEXT("VersionText"),
			  // ADR-074 (16.08.2026): постоянная подпись о сохранении над строкой версии;
			  // в живой ассет приезжает только -augment (AugmentPauseMenuSaveHint).
			  TEXT("SaveHintText") },
			/*bOwnerOwned=*/true },
		{ TEXT("/Game/UI/WBP_Intro"), TEXT("WBP_Intro"),
			TEXT("/Script/ContrarySurvivor.IntroScreenWidget"), &BuildIntro,
			{ TEXT("Background"), TEXT("LineText"), TEXT("SkipHintText") } },
		{ TEXT("/Game/UI/WBP_OnboardingHint"), TEXT("WBP_OnboardingHint"),
			TEXT("/Script/ContrarySurvivor.OnboardingHintWidget"), &BuildOnboardingHint,
			{ TEXT("HintPlate"), TEXT("HintText") } },
		{ TEXT("/Game/UI/WBP_LimpIndicator"), TEXT("WBP_LimpIndicator"),
			TEXT("/Script/ContrarySurvivor.LimpIndicatorWidget"), &BuildLimpIndicator,
			{ TEXT("WidthBox"), TEXT("Plate"), TEXT("IndicatorText") } },
		{ TEXT("/Game/UI/WBP_MockAd"), TEXT("WBP_MockAd"),
			TEXT("/Script/ContrarySurvivor.MockAdWidget"), &BuildMockAd,
			{ TEXT("DimBorder"), TEXT("TitleText"), TEXT("PlacementText"),
			  TEXT("CountdownText"), TEXT("CloseButton"), TEXT("CloseText") } },
		// Надпись при входе на базу противника (ТЗ 22.08 §5 + П.0 ADR-077: стиль в ассете;
		// класс окна назначается на базе слотом «Окно надписи»). Создание: -rebuild
		// -asset=WBP_BaseAnnounce; после создания стиль правит Ринат, пересборка не нужна.
		{ TEXT("/Game/UI/WBP_BaseAnnounce"), TEXT("WBP_BaseAnnounce"),
			TEXT("/Script/ContrarySurvivor.BaseEntryAnnounceWidget"), &BuildBaseAnnounce,
			{ TEXT("DigitText"), TEXT("LineText") } },
		// Окно «Поддержать автора» (задание издателя; вид одобрен Ринатом живьём 12.08.2026).
		// Раскладка плоская, каждый элемент — прямой ребёнок холста: требование Рината
		// «чтобы всё легко двигалось мышкой, без пунктирной таблицы-подложки». Подробности —
		// в шапке BuildSupportAuthor.
		//
		// ⛔ ОТДАНО ВЛАДЕЛЬЦУ (решение 12.08.2026): Ринат правит это окно мышкой, поэтому
		// генератор его больше НЕ пересобирает и НЕ перезаписывает даже с -force. Создать с
		// нуля он его может (это случай «файла нет вовсе»), дополнять — только -augment.
		// Проверка -verify по такому окну смотрит ровно то, что и есть критерий приёмки:
		// весь ли состав на месте и не замкнут ли какой-нибудь элемент в дизайнере (замок —
		// единственное, что реально мешает выделить элемент мышкой).
		{ TEXT("/Game/UI/WBP_SupportAuthor"), TEXT("WBP_SupportAuthor"),
			TEXT("/Script/ContrarySurvivor.SupportAuthorWidget"), &BuildSupportAuthor,
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"), TEXT("MessageText"),
			  TEXT("WatchAdButton"), TEXT("WatchAdText"),
			  TEXT("SupportLinkButton"), TEXT("SupportLinkText"),
			  TEXT("CloseButton"), TEXT("CloseText"), TEXT("ThanksText") },
			/*bOwnerOwned=*/true },
	};

	FString ObjectPathOf(const FWbpSpec& Spec)
	{
		return FString::Printf(TEXT("%s.%s"), Spec.PackageName, Spec.AssetName);
	}

	// ======================================================================
	// ГЕОМЕТРИЯ ОКОН (задача лида 08-09 после брака, доехавшего до телефона)
	// ======================================================================
	//
	// ЗАЧЕМ. Прежний -verify печатал про виджет только имя и класс, поэтому экран, у которого
	// подложка сжалась с 560 до 320, а кнопки уехали на позиции -100 и -30 и налезли друг на
	// друга, получал ровно такой же зелёный штамп, как здоровый. Брак прошёл гейт и уехал
	// Ринату на телефон. Ниже — печать геометрии и механические проверки, которые ловят
	// ровно этот класс поломок ДО сборки пакета.
	//
	// КАК СЧИТАЕМ ПРЯМОУГОЛЬНИК. В ассете лежит не готовая геометрия, а правило раскладки
	// (якоря, отступы, выравнивание), поэтому реальный прямоугольник зависит от размера
	// родителя. Считаем его ровно по механике UMG (SConstraintCanvas): по каждой оси,
	// если якоря совпадают — размер берём из отступов, а положение отсчитываем от якоря с
	// поправкой на выравнивание; если якоря разведены — края считаем от обоих якорей.
	// Размер КОРНЕВОГО холста берём эталонный — 1920x1080, в этих координатах владелец и
	// раскладывает окна в дизайнере UMG (и в них же считал Canvas-путь HUD). Телефон
	// получает ТО ЖЕ дерево через масштабирование интерфейса, поэтому мерить раскладку
	// «в точках телефона» нельзя: первый прогон с линейкой 1600x720 дал шесть ложных
	// тревог на здоровых окнах (кнопка возрождения «вылезала» за низ, плашки инвентаря и
	// магазина — за края), хотя на устройстве всё это масштабируется и помещается.
	const FVector2D GReferenceScreenSize(1920.0f, 1080.0f);

	// Насколько подложка должна быть больше лежащего на ней элемента, чтобы наложение
	// считалось задумкой, а не браком. Плашка окна больше своей надписи в разы; две кнопки
	// одного размера, оказавшиеся в одной точке, в этот признак не попадают — и правильно.
	constexpr float GBackdropAreaRatio = 1.3f;

	struct FWidgetRect
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		bool bAutoSize = false; // размер задаёт содержимое: в дизайн-тайме он неизвестен

		FVector2D GetSize() const { return Max - Min; }

		bool Intersects(const FWidgetRect& Other) const
		{
			// Касание краями пересечением не считаем: кнопки впритык — это норма.
			constexpr float Tolerance = 0.5f;
			return Min.X < Other.Max.X - Tolerance && Other.Min.X < Max.X - Tolerance
				&& Min.Y < Other.Max.Y - Tolerance && Other.Min.Y < Max.Y - Tolerance;
		}

		float GetArea() const
		{
			const FVector2D Size = GetSize();
			return FMath::Max(0.0f, Size.X) * FMath::Max(0.0f, Size.Y);
		}

		// Полностью ли этот прямоугольник накрывает другой (с запасом на округление).
		bool Contains(const FWidgetRect& Other) const
		{
			constexpr float Tolerance = 0.5f;
			return Min.X <= Other.Min.X + Tolerance && Min.Y <= Other.Min.Y + Tolerance
				&& Max.X >= Other.Max.X - Tolerance && Max.Y >= Other.Max.Y - Tolerance;
		}
	};

	// Прямоугольник канвас-слота в координатах родительского холста размера ParentSize.
	FWidgetRect ComputeCanvasRect(const UCanvasPanelSlot* Slot, const FVector2D& ParentSize)
	{
		FWidgetRect Rect;
		if (!Slot)
		{
			return Rect;
		}
		const FAnchorData Layout = Slot->GetLayout();
		const FAnchors& Anchors = Layout.Anchors;
		const FMargin& Offsets = Layout.Offsets;
		const FVector2D& Alignment = Layout.Alignment;
		Rect.bAutoSize = Slot->GetAutoSize();

		// Ось X.
		if (FMath::IsNearlyEqual(Anchors.Minimum.X, Anchors.Maximum.X))
		{
			const float Width = Offsets.Right; // при совпавших якорях правый отступ — это ширина
			const float Left = Anchors.Minimum.X * ParentSize.X + Offsets.Left - Alignment.X * Width;
			Rect.Min.X = Left;
			Rect.Max.X = Left + Width;
		}
		else
		{
			Rect.Min.X = Anchors.Minimum.X * ParentSize.X + Offsets.Left;
			Rect.Max.X = Anchors.Maximum.X * ParentSize.X - Offsets.Right;
		}

		// Ось Y.
		if (FMath::IsNearlyEqual(Anchors.Minimum.Y, Anchors.Maximum.Y))
		{
			const float Height = Offsets.Bottom; // при совпавших якорях нижний отступ — это высота
			const float Top = Anchors.Minimum.Y * ParentSize.Y + Offsets.Top - Alignment.Y * Height;
			Rect.Min.Y = Top;
			Rect.Max.Y = Top + Height;
		}
		else
		{
			Rect.Min.Y = Anchors.Minimum.Y * ParentSize.Y + Offsets.Top;
			Rect.Max.Y = Anchors.Maximum.Y * ParentSize.Y - Offsets.Bottom;
		}
		return Rect;
	}

	// Растянут ли слот на ВСЮ площадь родителя (якоря 0..1 по обеим осям). Это объективный
	// признак фонового слоя — затемнения, подложки, картинки фона: такой элемент по замыслу
	// лежит под всеми и пересекается со всем, ругаться на него бессмысленно. Признак
	// геометрический, а не «по имени»: имена у окон разные, а смысл один.
	bool IsFullAreaLayer(const UCanvasPanelSlot* Slot)
	{
		if (!Slot)
		{
			return false;
		}
		const FAnchors Anchors = Slot->GetAnchors();
		return FMath::IsNearlyZero(Anchors.Minimum.X) && FMath::IsNearlyZero(Anchors.Minimum.Y)
			&& FMath::IsNearlyEqual(Anchors.Maximum.X, 1.0f) && FMath::IsNearlyEqual(Anchors.Maximum.Y, 1.0f);
	}

	// Явные исключения на окно — там, где геометрия сама за себя не говорит.
	// Заполняется РУКАМИ и осознанно: проверка обязана молчать на здоровом экране, но не
	// имеет права молчать на больном, поэтому «на всякий случай» сюда ничего не вносим.
	struct FGeometryContract
	{
		const TCHAR* AssetName;
		// Дополнительные фоновые слои (не растянутые на всю площадь, но лежащие ПОД остальными
		// по замыслу): например полосы затемнения у нижнего края главного меню.
		std::initializer_list<const TCHAR*> BackgroundWidgets;
		// Намеренные наложения парами «A|B»: элементы, которым положено лежать друг на друге.
		std::initializer_list<const TCHAR*> AllowedOverlaps;
	};

	const FGeometryContract GGeometryContracts[] =
	{
		// Главное меню: полосы затемнения у низа — фон под строками версии и политики
		// (они и должны лежать НА затемнении, это и есть его смысл).
		{ TEXT("WBP_StartScreen"),
			{ TEXT("BottomShade0"), TEXT("BottomShade1"), TEXT("BottomShade2"),
			  TEXT("BottomShade3"), TEXT("BottomShade4"), TEXT("BottomShade5") },
			{ } },
	};

	// Фоновый ли это слой по контракту окна.
	bool IsDeclaredBackground(const TCHAR* AssetName, const FString& WidgetName)
	{
		for (const FGeometryContract& Contract : GGeometryContracts)
		{
			if (FCString::Strcmp(Contract.AssetName, AssetName) != 0)
			{
				continue;
			}
			for (const TCHAR* Background : Contract.BackgroundWidgets)
			{
				if (WidgetName == Background)
				{
					return true;
				}
			}
		}
		return false;
	}

	// Определена ниже (нужна проверке пересечений): разрешено ли паре лежать друг на друге.
	bool IsOverlapAllowed(const TCHAR* AssetName, const FString& FirstName, const FString& SecondName);

	// Проверка одного холста: печать геометрии детей + механические проверки. Возвращает
	// число НАЙДЕННЫХ ОШИБОК. Рекурсивно спускается во вложенные холсты (в том числе в те,
	// что лежат внутри подложек — типовая схема окон проекта: плашка, а в ней свой холст).
	int32 CheckCanvasGeometry(const TCHAR* AssetName, UCanvasPanel* Canvas,
		const FVector2D& CanvasSize, int32 Depth, bool bIsRootCanvas)
	{
		if (!Canvas)
		{
			return 0;
		}
		const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
		int32 ErrorCount = 0;

		struct FCheckedChild
		{
			UWidget* Widget = nullptr;
			FWidgetRect Rect;
			bool bParticipates = false; // участвует ли в проверке пересечений
		};
		TArray<FCheckedChild> Children;

		for (int32 Index = 0; Index < Canvas->GetChildrenCount(); ++Index)
		{
			UWidget* Child = Canvas->GetChildAt(Index);
			UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
			if (!Child || !Slot)
			{
				continue; // не канвас-слот: геометрию задаёт контейнер, проверять нечего
			}

			const FWidgetRect Rect = ComputeCanvasRect(Slot, CanvasSize);
			const FAnchorData Layout = Slot->GetLayout();
			const ESlateVisibility Visibility = Child->GetVisibility();
			const bool bHidden = (Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden);
			const bool bBackground = IsFullAreaLayer(Slot) || IsDeclaredBackground(AssetName, Child->GetName());

			// Печать. Формат стабильный: читается глазами и разбирается построчно.
			UE_LOG(LogGenerateWbp, Display,
				TEXT("VERIFY GEO %s: %s%s pos=(%.0f,%.0f) size=(%.0fx%.0f) anchors=(%.2f,%.2f..%.2f,%.2f) align=(%.2f,%.2f) rect=(%.0f,%.0f..%.0f,%.0f)%s%s%s"),
				AssetName, *Indent, *Child->GetName(),
				Layout.Offsets.Left, Layout.Offsets.Top,
				Rect.GetSize().X, Rect.GetSize().Y,
				Layout.Anchors.Minimum.X, Layout.Anchors.Minimum.Y,
				Layout.Anchors.Maximum.X, Layout.Anchors.Maximum.Y,
				Layout.Alignment.X, Layout.Alignment.Y,
				Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y,
				Rect.bAutoSize ? TEXT(" [размер по содержимому]") : TEXT(""),
				bBackground ? TEXT(" [фон]") : TEXT(""),
				bHidden ? TEXT(" [скрыт штатно]") : TEXT(""));

			// Штатно скрытые не проверяем вовсе: свёрнутый ряд патронов или спрятанный по
			// конфигу пункт меню — это норма, а не поломка.
			if (bHidden)
			{
				continue;
			}

			// 1. Нулевой или отрицательный размер. У элементов «по содержимому» размер в
			// ассете неизвестен — их пропускаем честно, а не придумываем число.
			if (!Rect.bAutoSize && (Rect.GetSize().X <= 0.0f || Rect.GetSize().Y <= 0.0f))
			{
				UE_LOG(LogGenerateWbp, Error,
					TEXT("VERIFY GEO FAIL: %s — у '%s' нулевой или отрицательный размер (%.0fx%.0f): на экране его не будет видно."),
					AssetName, *Child->GetName(), Rect.GetSize().X, Rect.GetSize().Y);
				++ErrorCount;
			}

			// 2. Вылезание за пределы родителя (сегодняшний случай: «Выход» вывалился за плашку).
			//    За ПОДЛОЖКУ — это ошибка: там всё детерминировано, размер родителя посчитан
			//    от его собственного прямоугольника. За КРАЙ ЭКРАНА — только предупреждение:
			//    настоящий размер холста зависит от устройства и от кривой масштабирования
			//    интерфейса, поэтому уверенно ругаться нельзя, но показать стоит.
			constexpr float Tolerance = 0.5f;
			const bool bOutside = !Rect.bAutoSize && !bBackground
				&& (Rect.Min.X < -Tolerance || Rect.Min.Y < -Tolerance
					|| Rect.Max.X > CanvasSize.X + Tolerance || Rect.Max.Y > CanvasSize.Y + Tolerance);
			if (bOutside && !bIsRootCanvas)
			{
				UE_LOG(LogGenerateWbp, Error,
					TEXT("VERIFY GEO FAIL: %s — '%s' вылезает за подложку: его прямоугольник (%.0f,%.0f..%.0f,%.0f), а место родителя (0,0..%.0f,%.0f)."),
					AssetName, *Child->GetName(),
					Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y, CanvasSize.X, CanvasSize.Y);
				++ErrorCount;
			}
			else if (bOutside)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("VERIFY GEO: %s — '%s' выходит за эталонный экран %.0fx%.0f: его прямоугольник (%.0f,%.0f..%.0f,%.0f). На устройстве это масштабируется, но проверьте на узком экране."),
					AssetName, *Child->GetName(), CanvasSize.X, CanvasSize.Y,
					Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);
			}

			FCheckedChild Entry;
			Entry.Widget = Child;
			Entry.Rect = Rect;
			// В проверке пересечений участвуют только элементы с известным размером и не фоны.
			Entry.bParticipates = !Rect.bAutoSize && !bBackground;
			Children.Add(Entry);
		}

		// 3. Пересечения (сегодняшний случай: «Новая игра» и «Настройки» налезли друг на друга).
		for (int32 First = 0; First < Children.Num(); ++First)
		{
			if (!Children[First].bParticipates)
			{
				continue;
			}
			for (int32 Second = First + 1; Second < Children.Num(); ++Second)
			{
				if (!Children[Second].bParticipates)
				{
					continue;
				}
				const FString FirstName = Children[First].Widget->GetName();
				const FString SecondName = Children[Second].Widget->GetName();
				if (!Children[First].Rect.Intersects(Children[Second].Rect))
				{
					continue;
				}
				if (IsOverlapAllowed(AssetName, FirstName, SecondName))
				{
					continue; // намеренное наложение, объявлено в контракте окна
				}

				// «Элемент лежит НА подложке» — это не брак, а обычная схема окна: плашка,
				// а на ней надпись или кнопка. Признак объективный: один прямоугольник
				// полностью накрывает другой и заметно больше его по площади. Две кнопки
				// одинакового размера, оказавшиеся в одной точке, под это НЕ подпадают —
				// именно такой случай и надо ловить.
				const FWidgetRect& FirstRect = Children[First].Rect;
				const FWidgetRect& SecondRect = Children[Second].Rect;
				const bool bFirstIsBackdrop = FirstRect.Contains(SecondRect)
					&& FirstRect.GetArea() >= SecondRect.GetArea() * GBackdropAreaRatio;
				const bool bSecondIsBackdrop = SecondRect.Contains(FirstRect)
					&& SecondRect.GetArea() >= FirstRect.GetArea() * GBackdropAreaRatio;
				if (bFirstIsBackdrop || bSecondIsBackdrop)
				{
					continue;
				}
				UE_LOG(LogGenerateWbp, Error,
					TEXT("VERIFY GEO FAIL: %s — '%s' (%.0f,%.0f..%.0f,%.0f) и '%s' (%.0f,%.0f..%.0f,%.0f) налезают друг на друга."),
					AssetName,
					*FirstName, Children[First].Rect.Min.X, Children[First].Rect.Min.Y,
					Children[First].Rect.Max.X, Children[First].Rect.Max.Y,
					*SecondName, Children[Second].Rect.Min.X, Children[Second].Rect.Min.Y,
					Children[Second].Rect.Max.X, Children[Second].Rect.Max.Y);
				++ErrorCount;
			}
		}

		// Рекурсия во вложенные холсты: напрямую и через подложку (плашка, а в ней холст —
		// типовая схема окон проекта, см. MakeModalPlate).
		for (const FCheckedChild& Entry : Children)
		{
			if (UCanvasPanel* Nested = Cast<UCanvasPanel>(Entry.Widget))
			{
				ErrorCount += CheckCanvasGeometry(AssetName, Nested, Entry.Rect.GetSize(), Depth + 1, /*bIsRootCanvas=*/false);
			}
			else if (UBorder* Plate = Cast<UBorder>(Entry.Widget))
			{
				if (UCanvasPanel* Inner = Cast<UCanvasPanel>(Plate->GetContent()))
				{
					// Внутренний холст меньше подложки на её внутренние отступы.
					const FMargin Padding = Plate->GetPadding();
					const FVector2D InnerSize(
						FMath::Max(0.0f, Entry.Rect.GetSize().X - Padding.Left - Padding.Right),
						FMath::Max(0.0f, Entry.Rect.GetSize().Y - Padding.Top - Padding.Bottom));
					ErrorCount += CheckCanvasGeometry(AssetName, Inner, InnerSize, Depth + 1, /*bIsRootCanvas=*/false);
				}
			}
		}
		return ErrorCount;
	}

	// Разрешено ли этой паре пересекаться (порядок имён неважен).
	bool IsOverlapAllowed(const TCHAR* AssetName, const FString& FirstName, const FString& SecondName)
	{
		for (const FGeometryContract& Contract : GGeometryContracts)
		{
			if (FCString::Strcmp(Contract.AssetName, AssetName) != 0)
			{
				continue;
			}
			for (const TCHAR* Pair : Contract.AllowedOverlaps)
			{
				FString Left, Right;
				if (!FString(Pair).Split(TEXT("|"), &Left, &Right))
				{
					continue;
				}
				if ((FirstName == Left && SecondName == Right) || (FirstName == Right && SecondName == Left))
				{
					return true;
				}
			}
		}
		return false;
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

	// ⛔ УСТАРЕЛО (П.0 ADR-077, 23.08): контракт замков ОТМЕНЁН — Ринат требует выделять и
	// двигать ВСЁ, -verify теперь проверяет обратное («ни одного замка нигде»), замки
	// снимает -unlockall. Таблица оставлена мёртвой историей (какая начинка замыкалась и
	// почему); в проверках больше не используется. LockSubtreeInDesigner в НОВОМ коде не звать.
	const FLockContract GLockContracts[] =
	{
		// Поправка 08-07 (ТЗ Рината п.3) во ВСЕХ контрактах ниже: ТЕКСТЫ внутри кнопок
		// разомкнуты (шрифт правится в «Деталях»), замкнутыми остаются только
		// контейнеры-коробки и иконки начинки — клик мимо текста выделяет кнопку.
		//
		// Build 1.2.1: шесть кубиков строки статов — в своих канвас-слотах, свободны.
		// Build 1.2.2 (ADR-056, решение Рината): начинка слотов брони — подпись, иконка и
		// название надетого — тоже СВОБОДНА (он крутит их размеры сам). Замкнутыми в слотах
		// остались только коробки-контейнеры: они не мешают, а клик по свободному краю
		// слота по-прежнему выделяет кнопку.
		{ TEXT("WBP_Inventory"),
			{ TEXT("HeadSlotButtonBox"), TEXT("TorsoSlotButtonBox"), TEXT("LegsSlotButtonBox") },
			{ TEXT("HeadSlotButton"), TEXT("TorsoSlotButton"), TEXT("LegsSlotButton"), TEXT("CloseButton"),
			  TEXT("CloseLabel"),
			  TEXT("HeadSlotButtonCaption"), TEXT("HeadSlotIcon"), TEXT("HeadSlotText"),
			  TEXT("TorsoSlotButtonCaption"), TEXT("TorsoSlotIcon"), TEXT("TorsoSlotText"),
			  TEXT("LegsSlotButtonCaption"), TEXT("LegsSlotIcon"), TEXT("LegsSlotText"),
			  TEXT("InvMoneyIcon"), TEXT("InvMoneyText"), TEXT("InvHungerIcon"), TEXT("InvHungerText"),
			  TEXT("InvThirstIcon"), TEXT("InvThirstText"),
			  TEXT("RangedSlotIcon"), TEXT("RangedSlotText"),
			  TEXT("MeleeSlotIcon"), TEXT("MeleeSlotText") } },
		// Build 1.2.1: из начинки золотой кнопки замкнута осталась только иконка видео;
		// текст пересчёта патронов — свободный канвас-слот.
		{ TEXT("WBP_Shop"),
			{ TEXT("SellAdIcon") },
			{ TEXT("CloseButton"), TEXT("QtyMinusButton"), TEXT("QtyPlusButton"),
			  TEXT("SliderCancelButton"), TEXT("SliderConfirmButton"),
			  TEXT("SellAdButton"), TEXT("SliderQtyAmmoText"),
			  TEXT("CloseLabel"), TEXT("QtyMinusLabel"), TEXT("QtyPlusLabel"),
			  TEXT("SliderCancelLabel"), TEXT("SliderConfirmLabel"),
			  TEXT("SellAdText"), TEXT("SellAdSubText") } },
		// Build 1.2.2: плитка предмета — контейнеры и иконка начинки замкнуты (клик по
		// ним выделяет кнопку плитки), тексты плитки, кнопки и коробка выброса свободны.
		{ TEXT("WBP_ItemTile"),
			{ TEXT("TilePlate"), TEXT("TileStack"), TEXT("TileIconZone"), TEXT("TileIconBox"),
			  TEXT("TileIcon") },
			{ TEXT("TileButton"), TEXT("TileDropBox"), TEXT("DropButton"),
			  TEXT("TileCountText"), TEXT("TileNameText"),
			  TEXT("TilePriceText"), TEXT("TileStatusText"), TEXT("DropLabel") } },
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
		// 08-07: замкнутой начинки в диалоге не осталось — все подписи кнопок свободны
		// (именно на WBP_Dialog Ринат и не смог поменять размер шрифта).
		{ TEXT("WBP_Dialog"),
			{ },
			{ TEXT("PanelPlate"), TEXT("NPCNameText"), TEXT("ReplicaText"),
			  TEXT("AcceptButton"), TEXT("DeclineButton"), TEXT("TurnInButton"), TEXT("CloseButton"),
			  TEXT("AcceptText"), TEXT("DeclineText"), TEXT("TurnInText"), TEXT("CloseText") } },
		// Build 1.2.1 (Д2): всё свободно (ручки у кнопок/подложки, шрифты у подписей).
		{ TEXT("WBP_EndOfStory"),
			{ },
			{ TEXT("Plate"), TEXT("MessageText"), TEXT("StatusText"),
			  TEXT("WriteButton"), TEXT("PlayButton"),
			  TEXT("WriteButtonText"), TEXT("PlayButtonText") } },
		// Build 1.2.1 (А1): окно обыска — всё свободно (вторая панель из ТЗ Рината 08-07).
		{ TEXT("WBP_CorpseLoot"),
			{ },
			{ TEXT("PanelPlate"), TEXT("TitleText"), TEXT("LootList"),
			  TEXT("CloseButton"), TEXT("TakeAllButton"),
			  TEXT("CloseText"), TEXT("TakeAllText") } },
		// Build 1.2.1 («двигать мышкой всё»): рядов-коробок статистики больше нет — каждая
		// подпись и каждое значение в своём канвас-слоте, свободны; блок потерь — вложенный
		// канвас, его тексты и сетка тоже свободны. Замкнута только иконка в кнопке рюкзака.
		{ TEXT("WBP_Death"),
			{ TEXT("SaveBackpackIcon") },
			{ TEXT("RespawnLabel"), TEXT("RespawnSubText"),
			  TEXT("SaveBackpackLabel"), TEXT("SaveBackpackSubText"),
			  TEXT("TitleText"),
			  TEXT("LifetimeLabel"), TEXT("LifetimeText"), TEXT("KillerLabel"), TEXT("KillerText"),
			  TEXT("MoneyLabel"), TEXT("MoneyText"), TEXT("QuestsLabel"), TEXT("QuestsText"),
			  TEXT("KillsLabel"), TEXT("KillsText"), TEXT("RespawnLineText"), TEXT("MoneyLossText"),
			  TEXT("ConsumablesLineText"), TEXT("SavedLineText"), TEXT("LossPanel"),
			  TEXT("LossHeaderText"), TEXT("LossGrid"), TEXT("LossMoreText"), TEXT("LossMoneyText"),
			  TEXT("SaveBackpackButton"), TEXT("RespawnButton"), TEXT("KeyHintText") } },
		// --- Волна 08-07: новые ассеты БЕЗ замков вовсе (прямое требование Рината: «всё
		// можно редактировать мышкой, без этой дурацкой пунктирной сетки»). Пустой список
		// замкнутых + полный список свободных = -verify докажет, что ни один элемент не
		// заперт и панель «Детали» доступна везде. ---
		{ TEXT("WBP_DailyReward"),
			{ },
			{ TEXT("PanelPlate"), TEXT("TitleText"), TEXT("StreakText"), TEXT("RewardText"),
			  TEXT("TakeButton"), TEXT("TakeText"),
			  TEXT("DoubleButton"), TEXT("DoubleRow"), TEXT("DoubleIcon"),
			  TEXT("DoubleLabels"), TEXT("DoubleText"), TEXT("DoubleSubText") } },
		{ TEXT("WBP_StartScreen"),
			{ },
			{ TEXT("BackgroundImage"), TEXT("LogoImage"),
			  TEXT("DimBorder"), TEXT("TitleText"), TEXT("SubtitleText"),
			  TEXT("ContinueButton"), TEXT("ContinueText"),
			  TEXT("NewGameButton"), TEXT("NewGameText"),
			  TEXT("SettingsButton"), TEXT("SettingsText"),
			  TEXT("SupportButton"), TEXT("SupportText"),
			  TEXT("CommunityButton"), TEXT("CommunityText"),
			  TEXT("ExitButton"), TEXT("ExitText"),
			  TEXT("PolicyButton"), TEXT("PolicyText"), TEXT("VersionText") } },
		// Экран настроек (ADR-062, подход 2): замков нет вовсе — всё правится мышкой,
		// включая подписи внутри кнопок (то же требование Рината, что для окон волны 08-07).
		{ TEXT("WBP_Settings"),
			{ },
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"),
			  TEXT("GraphicsHeaderText"), TEXT("PresetValueText"),
			  TEXT("PresetLowButton"), TEXT("PresetLowText"),
			  TEXT("PresetMediumButton"), TEXT("PresetMediumText"),
			  TEXT("PresetHighButton"), TEXT("PresetHighText"),
			  TEXT("PresetAutoButton"), TEXT("PresetAutoText"),
			  TEXT("ResolutionLabelText"), TEXT("ResolutionSlider"), TEXT("ResolutionValueText"),
			  TEXT("ResolutionMinusButton"), TEXT("ResolutionMinusText"),
			  TEXT("ResolutionPlusButton"), TEXT("ResolutionPlusText"),
			  TEXT("FrameLimitButton"), TEXT("FrameLimitText"),
			  TEXT("FpsCounterButton"), TEXT("FpsCounterText"),
			  TEXT("SoundHeaderText"), TEXT("MusicLabelText"), TEXT("MusicSlider"), TEXT("MusicValueText"),
			  TEXT("EffectsLabelText"), TEXT("EffectsSlider"), TEXT("EffectsValueText"),
			  TEXT("ControlsHeaderText"),
			  TEXT("SensitivityLabelText"), TEXT("SensitivitySlider"), TEXT("SensitivityValueText"),
			  TEXT("OpacityLabelText"), TEXT("OpacitySlider"), TEXT("OpacityValueText"),
			  TEXT("VibrationButton"), TEXT("VibrationText"),
			  TEXT("MiscHeaderText"),
			  TEXT("ReportBugButton"), TEXT("ReportBugText"), TEXT("ReportBugHintText"),
			  TEXT("ResetProgressButton"), TEXT("ResetProgressText"),
			  TEXT("CloseButton"), TEXT("CloseText"),
			  TEXT("ConfirmPanel"), TEXT("ConfirmTitleText"),
			  TEXT("ConfirmYesButton"), TEXT("ConfirmYesText"),
			  TEXT("ConfirmNoButton"), TEXT("ConfirmNoText") } },
		{ TEXT("WBP_Consent"),
			{ },
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"),
			  TEXT("Body1Text"), TEXT("Body2Text"), TEXT("Body3Text"),
			  TEXT("AcceptButton"), TEXT("AcceptText"), TEXT("DeclineButton"), TEXT("DeclineText"),
			  TEXT("PolicyButton"), TEXT("PolicyText") } },
		{ TEXT("WBP_PauseMenu"),
			{ },
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"),
			  TEXT("ResumeButton"), TEXT("ResumeText"),
			  TEXT("MainMenuButton"), TEXT("MainMenuText"),
			  TEXT("PolicyButton"), TEXT("PolicyText"), TEXT("QuitButton"), TEXT("QuitText"),
			  TEXT("VersionText"),
			  TEXT("SaveHintText") } }, // ADR-074: подпись о сохранении — свободна, как версия
		{ TEXT("WBP_Intro"),
			{ },
			{ TEXT("Background"), TEXT("LineText"), TEXT("SkipHintText") } },
		{ TEXT("WBP_OnboardingHint"),
			{ },
			{ TEXT("HintPlate"), TEXT("HintText") } },
		{ TEXT("WBP_LimpIndicator"),
			{ },
			{ TEXT("WidthBox"), TEXT("Plate"), TEXT("IndicatorText") } },
		// Окно «Поддержать автора»: замков нет вовсе — двигать и настраивать мышкой можно
		// каждый элемент, включая подписи внутри кнопок (прямое требование Рината 12.08.2026).
		// ⚠ Пока окно отдано владельцу, эта запись не работает: проверка таких окон уходит по
		// своей ветке раньше (там своя, более строгая проверка замков). Держим её записанной на
		// случай, если окно однажды заберут обратно — так же, как у паузы и настроек.
		{ TEXT("WBP_SupportAuthor"),
			{ },
			{ TEXT("DimBorder"), TEXT("PanelPlate"), TEXT("TitleText"), TEXT("MessageText"),
			  TEXT("WatchAdButton"), TEXT("WatchAdText"),
			  TEXT("SupportLinkButton"), TEXT("SupportLinkText"),
			  TEXT("CloseButton"), TEXT("CloseText"), TEXT("ThanksText") } },
		{ TEXT("WBP_MockAd"),
			{ },
			{ TEXT("DimBorder"), TEXT("TitleText"), TEXT("PlacementText"),
			  TEXT("CountdownText"), TEXT("CloseButton"), TEXT("CloseText") } },
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

	// WBP_PlayerStats, Б9 (вторая половина пункта издателя): полосы здоровья, голода и
	// жажды начинались вплотную к левому краю, а после разрешения рисовать в область выреза
	// (Config/DefaultEngine.ini, bUseDisplayCutout) окно приложения выросло с 1440x720 до
	// 1600x720 — левый край ушёл под глазок камеры.
	//
	// Правка отдельным дополнением, а НЕ внутри AugmentPlayerStats: то дополнение первым
	// делом выходит на живом ассете (там уже раскладка Д1 с полосками в канвас-слотах), и
	// до безопасной зоны дело бы не дошло. Здесь же меняется только КОРЕНЬ дерева —
	// расстановка владельца внутри не трогается ни на единицу.
	void AugmentPlayerStatsSafeZone(UWidgetTree* Tree, bool& bChanged)
	{
		WrapRootInSafeZone(Tree, TEXT("WBP_PlayerStats"), bChanged);
	}

	// WBP_Shop, пункт 6: строка пересчёта пачек в патроны. Показывается только при покупке
	// патронов, поэтому в ассете сразу спрятана — видимостью управляет код.
	void AugmentShopAmmoRow(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Shop");
		// Build 1.2.1: в новой раскладке обёртки нет — SliderQtyAmmoText лежит в своём
		// канвас-слоте, и заворачивать его обратно нельзя (пропали бы ручки). Строка уже
		// есть в ЛЮБОМ виде — дополнению делать нечего.
		if (Tree->FindWidget(TEXT("SliderQtyAmmoRow")) || Tree->FindWidget(TEXT("SliderQtyAmmoText")))
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

	// WBP_Shop, Б8 п.4 (издатель после ревизии 08-05, дословно «кнопки ±48 точек»): ни одна
	// нажимаемая кнопка магазина не должна быть мельче предела по любой стороне.
	//
	// Правило только ПОДНИМАЕТ размер и никогда не опускает: расстановка в ассете сделана
	// Ринатом мышкой, и уменьшать её код права не имеет. Поэтому кнопка, которую он уже
	// сделал крупной, остаётся как есть, а мелкая дорастает до предела. Прогон на текущем
	// ассете обязан ничего не менять — все шесть кнопок там уже крупнее (проверено срезом
	// геометрии от 08-05: закрытие 238x71, минус 98x76, плюс 104x76, отмена 192x66,
	// подтверждение 290x66, золотая 305x70). Правка нужна на будущее: свежая генерация
	// -rebuild ставит кнопки по дефолтам, и вот они как раз были мельче предела.
	//
	// То же правило работает и в самой игре при открытии окна (UShopScreenWidget,
	// параметр «Наименьший размер кнопки под палец») — здесь оно закрывает ассет, там живой
	// экран, случайно уменьшенная в дизайнере кнопка ловится в обоих местах.
	void AugmentShopTouchTargets(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Shop");
		constexpr float MinTouchSize = 48.0f;

		auto RaiseButton = [&](const TCHAR* WidgetName)
		{
			UWidget* Found = AugFind(Tree, Name, WidgetName);
			UCanvasPanelSlot* Slot = Found ? Cast<UCanvasPanelSlot>(Found->Slot) : nullptr;
			if (!Slot)
			{
				if (Found)
				{
					UE_LOG(LogGenerateWbp, Warning,
						TEXT("AUGMENT %s: слот '%s' не CanvasPanelSlot (%s) — размер не тронут."),
						Name, WidgetName, *Found->GetClass()->GetName());
				}
				return;
			}

			// У слота-растяжки поля Right/Bottom — это отступы, а не размер: их нельзя
			// править как размер (раскладка уехала бы).
			const FAnchorData Layout = Slot->GetLayout();
			if (!Layout.Anchors.Minimum.Equals(Layout.Anchors.Maximum))
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: '%s' стоит растяжкой — размер задаёт панель, предел не применяю."),
					Name, WidgetName);
				return;
			}

			const FVector2D OldSize = Slot->GetSize();
			const FVector2D NewSize(FMath::Max(OldSize.X, MinTouchSize), FMath::Max(OldSize.Y, MinTouchSize));
			if (NewSize.Equals(OldSize))
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: '%s' %.0fx%.0f — предел под палец %.0f соблюдён, не трогаю."),
					Name, WidgetName, OldSize.X, OldSize.Y, MinTouchSize);
				return; // идемпотентность: повторный прогон ничего не меняет
			}
			Slot->SetSize(NewSize);
			bChanged = true;
			UE_LOG(LogGenerateWbp, Display, TEXT("AUGMENT %s: '%s' %.0fx%.0f -> %.0fx%.0f (предел под палец)."),
				Name, WidgetName, OldSize.X, OldSize.Y, NewSize.X, NewSize.Y);
		};

		RaiseButton(TEXT("CloseButton"));
		RaiseButton(TEXT("QtyMinusButton"));
		RaiseButton(TEXT("QtyPlusButton"));
		RaiseButton(TEXT("SliderConfirmButton"));
		RaiseButton(TEXT("SliderCancelButton"));
		RaiseButton(TEXT("SellAdButton"));
	}

	// Правка AugmentShopRow УДАЛЕНА (Build 1.2.2): строкового WBP_ShopRow больше нет,
	// «Не хватает монет» живёт в плитке (TileStatusText, ставит код магазина).

	// WBP_QuestTracker, пункт 8: в ассете лежит текст-заглушка от генерации. Строку пишет
	// код, а пока квеста нет — кубик должен быть пустым, иначе заглушка мелькает на экране.
	void AugmentQuestTracker(UWidgetTree* Tree, bool& bChanged)
	{
		AugSetText(Tree, TEXT("WBP_QuestTracker"), TEXT("TrackerText"), FText::GetEmpty(), bChanged);
	}

	// WBP_PauseMenu (подход 3 волны меню): добавить пункт «В главное меню», если его ещё нет.
	// Именно точечное дополнение, а не пересборка: живая панель паузы уже стилизована
	// владельцем, и -rebuild пришлось бы гнать по всему окну. Новая кнопка встаёт под
	// «Продолжить» — куда бы владелец её ни передвинул, отсчёт идёт от её слота.
	void AugmentPauseMenu(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_PauseMenu");
		if (Tree->FindWidget(TEXT("MainMenuButton")))
		{
			return; // уже добавлено — режим идемпотентный
		}

		UWidget* Resume = Tree->FindWidget(TEXT("ResumeButton"));
		UCanvasPanelSlot* ResumeSlot = Resume ? Cast<UCanvasPanelSlot>(Resume->Slot) : nullptr;
		if (!ResumeSlot)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: кнопка ResumeButton не найдена в канвас-слоте — некуда пристроить «В главное меню»."),
				Name);
			return;
		}
		UCanvasPanel* Parent = Cast<UCanvasPanel>(Resume->GetParent());
		if (!Parent)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: родитель ResumeButton не CanvasPanel — пропускаю."), Name);
			return;
		}

		UObject* Roboto = LoadRobotoFont();
		UButton* MainMenu = MakeGreyButton(Tree, TEXT("MainMenuButton"));
		SetUnlockedCaption(Tree, Roboto, MainMenu, TEXT("MainMenuText"),
			NSLOCTEXT("PauseMenuWidget", "MainMenuText", "В главное меню"),
			FLinearColor(0.05f, 0.05f, 0.05f, 1.0f), 19);

		const FVector2D ResumeSize = ResumeSlot->GetSize();
		if (UCanvasPanelSlot* MainMenuSlot = Parent->AddChildToCanvas(MainMenu))
		{
			MainMenuSlot->SetAnchors(ResumeSlot->GetAnchors());
			MainMenuSlot->SetAlignment(ResumeSlot->GetAlignment());
			MainMenuSlot->SetSize(ResumeSize);
			// Под «Продолжить», с тем же зазором, что между остальными пунктами панели.
			MainMenuSlot->SetPosition(ResumeSlot->GetPosition() + FVector2D(0.0f, ResumeSize.Y + 12.0f));
		}
		bChanged = true;
	}

	// WBP_PauseMenu, волна 08-09 (просьба Рината): «Настройки» и «Сообщество» прямо из паузы
	// плюс подпись политики, которая не помещалась в кнопку. Делаем ДОПОЛНЕНИЕМ, а не
	// пересборкой: окно отдано владельцу, и пересборка стёрла бы его ручную настройку.
	// Идемпотентно: кнопки уже есть — выходим молча.
	void AugmentPauseMenuExtras(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_PauseMenu");

		// Подпись политики чиним всегда, когда она ещё общего кегля: это отдельная беда,
		// не связанная с новыми кнопками.
		if (UTextBlock* Policy = Cast<UTextBlock>(Tree->FindWidget(TEXT("PolicyText"))))
		{
			const FSlateFontInfo PolicyFont = Policy->GetFont();
			if (PolicyFont.Size > 14 || !Policy->GetAutoWrapText())
			{
				FSlateFontInfo Fixed = PolicyFont;
				Fixed.Size = 14;
				Policy->SetFont(Fixed);
				Policy->SetAutoWrapText(true);
				Policy->SetJustification(ETextJustify::Center);
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: подпись политики уменьшена до кегля 14 и переносится по словам — целиком не помещалась."),
					Name);
				bChanged = true;
			}
		}

		if (Tree->FindWidget(TEXT("SettingsButton")) && Tree->FindWidget(TEXT("CommunityButton")))
		{
			return; // кнопки уже добавлены — режим идемпотентный
		}

		UWidget* Anchor = Tree->FindWidget(TEXT("MainMenuButton"));
		UCanvasPanelSlot* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
		UCanvasPanel* Parent = Anchor ? Cast<UCanvasPanel>(Anchor->GetParent()) : nullptr;
		if (!AnchorSlot || !Parent)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: кнопка MainMenuButton не найдена в канвас-слоте — некуда пристроить новые пункты."),
				Name);
			return;
		}

		const FVector2D AnchorSize = AnchorSlot->GetSize();
		const FVector2D AnchorPos = AnchorSlot->GetPosition();
		const FAnchors AnchorAnchors = AnchorSlot->GetAnchors();
		constexpr float Gap = 12.0f;
		const float Step = AnchorSize.Y + Gap;

		// Пункты ниже «В главное меню» сдвигаем вниз, чтобы новые кнопки не легли на них.
		// Двигаем ТОЛЬКО те, что привязаны к тому же краю (сверху): строка версии привязана
		// к низу панели, её трогать нельзя — она уедет за край.
		const int32 InsertCount = 2;
		const float Shift = InsertCount * Step;
		for (int32 Index = 0; Index < Parent->GetChildrenCount(); ++Index)
		{
			UWidget* Child = Parent->GetChildAt(Index);
			UCanvasPanelSlot* ChildSlot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
			if (!ChildSlot || Child == Anchor)
			{
				continue;
			}
			if (!FMath::IsNearlyEqual(ChildSlot->GetAnchors().Minimum.Y, AnchorAnchors.Minimum.Y))
			{
				continue; // привязан к другому краю — не наш случай
			}
			if (ChildSlot->GetPosition().Y > AnchorPos.Y + KINDA_SMALL_NUMBER)
			{
				ChildSlot->SetPosition(ChildSlot->GetPosition() + FVector2D(0.0f, Shift));
			}
		}

		// Панель подросла на столько же, иначе нижние пункты вывалятся за её край.
		if (UWidget* Plate = Tree->FindWidget(TEXT("PanelPlate")))
		{
			if (UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(Plate->Slot))
			{
				PlateSlot->SetSize(PlateSlot->GetSize() + FVector2D(0.0f, Shift));
			}
		}

		UObject* Roboto = LoadRobotoFont();
		const FLinearColor DarkCaption(0.05f, 0.05f, 0.05f, 1.0f);
		auto AddAt = [&](const TCHAR* ButtonName, const TCHAR* TextName, const FText& Caption, int32 Index)
		{
			if (Tree->FindWidget(FName(ButtonName)))
			{
				return; // эта кнопка уже есть — вторую не заводим
			}
			UButton* Button = MakeGreyButton(Tree, FName(ButtonName));
			SetUnlockedCaption(Tree, Roboto, Button, FName(TextName), Caption, DarkCaption, 19);
			if (UCanvasPanelSlot* ButtonSlot = Parent->AddChildToCanvas(Button))
			{
				ButtonSlot->SetAnchors(AnchorAnchors);
				ButtonSlot->SetAlignment(AnchorSlot->GetAlignment());
				ButtonSlot->SetSize(AnchorSize);
				ButtonSlot->SetPosition(AnchorPos + FVector2D(0.0f, Step * Index));
			}
			bChanged = true;
		};
		AddAt(TEXT("SettingsButton"), TEXT("SettingsText"),
			NSLOCTEXT("PauseMenuWidget", "SettingsText", "Настройки"), 1);
		AddAt(TEXT("CommunityButton"), TEXT("CommunityText"),
			NSLOCTEXT("PauseMenuWidget", "CommunityText", "Сообщество"), 2);

		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: добавлены пункты «Настройки» и «Сообщество», нижние пункты сдвинуты на %.0f, панель подросла на столько же."),
			Name, Shift);
	}

	// WBP_PauseMenu, задание издателя 11.08.2026: строка «Поддержать автора» рядом с
	// «Сообщество» и «Политика конфиденциальности».
	//
	// ⛔ Окно ОТДАНО ВЛАДЕЛЬЦУ (bOwnerOwned в таблице генератора): пересобирать его нельзя даже
	// с -force, иначе сотрётся ручная настройка. Поэтому строку добавляем ДОПОЛНЕНИЕМ — тем же
	// приёмом, каким сюда приехали «Настройки» и «Сообщество».
	//
	// Идемпотентно: строка уже есть — выходим молча, второй кнопки повторный прогон не заводит.
	void AugmentPauseMenuSupport(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_PauseMenu");
		if (Tree->FindWidget(TEXT("SupportButton")))
		{
			return; // уже добавлено — режим идемпотентный
		}

		// Встаём сразу под «Сообщество» и берём у него размер, привязку и выравнивание: новый
		// пункт не должен отличаться от соседей ничем (условие задания — не выделять его).
		UWidget* Anchor = Tree->FindWidget(TEXT("CommunityButton"));
		UCanvasPanelSlot* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
		UCanvasPanel* Parent = Anchor ? Cast<UCanvasPanel>(Anchor->GetParent()) : nullptr;
		if (!AnchorSlot || !Parent)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: кнопка CommunityButton не найдена в канвас-слоте — некуда пристроить «Поддержать автора»."),
				Name);
			return;
		}

		const FVector2D AnchorSize = AnchorSlot->GetSize();
		const FVector2D AnchorPos = AnchorSlot->GetPosition();
		const FAnchors AnchorAnchors = AnchorSlot->GetAnchors();

		// Шаг между пунктами берём из ЖИВОГО ассета — расстояние от «Настройки» до «Сообщество».
		// Владелец мог развести пункты по-своему, и новая строка обязана встать с тем же
		// зазором, а не с зашитым в код. Не вышло измерить — запасной вариант как у соседей.
		float Step = AnchorSize.Y + 12.0f;
		if (UWidget* Above = Tree->FindWidget(TEXT("SettingsButton")))
		{
			if (UCanvasPanelSlot* AboveSlot = Cast<UCanvasPanelSlot>(Above->Slot))
			{
				const float LiveStep = AnchorPos.Y - AboveSlot->GetPosition().Y;
				if (LiveStep > KINDA_SMALL_NUMBER)
				{
					Step = LiveStep;
				}
			}
		}

		// Пункты ниже «Сообщество» сдвигаем на одну строку. Двигаем ТОЛЬКО привязанные к тому
		// же краю: строка версии привязана к низу панели, её трогать нельзя — уедет за край.
		for (int32 Index = 0; Index < Parent->GetChildrenCount(); ++Index)
		{
			UWidget* Child = Parent->GetChildAt(Index);
			UCanvasPanelSlot* ChildSlot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
			if (!ChildSlot || Child == Anchor)
			{
				continue;
			}
			if (!FMath::IsNearlyEqual(ChildSlot->GetAnchors().Minimum.Y, AnchorAnchors.Minimum.Y))
			{
				continue; // привязан к другому краю — не наш случай
			}
			if (ChildSlot->GetPosition().Y > AnchorPos.Y + KINDA_SMALL_NUMBER)
			{
				ChildSlot->SetPosition(ChildSlot->GetPosition() + FVector2D(0.0f, Step));
			}
		}

		// Панель подросла на ту же строку, иначе нижние пункты вывалятся за её край.
		if (UWidget* Plate = Tree->FindWidget(TEXT("PanelPlate")))
		{
			if (UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(Plate->Slot))
			{
				PlateSlot->SetSize(PlateSlot->GetSize() + FVector2D(0.0f, Step));
			}
		}

		// ⛔ Вид ровно как у соседей: та же серая кнопка, тот же кегль, тот же цвет подписи.
		// Дословно из задания издателя — пункт не выделяется и не делается крупнее соседей.
		UObject* Roboto = LoadRobotoFont();
		UButton* Support = MakeGreyButton(Tree, TEXT("SupportButton"));
		SetUnlockedCaption(Tree, Roboto, Support, TEXT("SupportText"),
			NSLOCTEXT("PauseMenuWidget", "SupportText", "Поддержать автора"),
			FLinearColor(0.05f, 0.05f, 0.05f, 1.0f), 19);
		if (UCanvasPanelSlot* SupportSlot = Parent->AddChildToCanvas(Support))
		{
			SupportSlot->SetAnchors(AnchorAnchors);
			SupportSlot->SetAlignment(AnchorSlot->GetAlignment());
			SupportSlot->SetSize(AnchorSize);
			SupportSlot->SetPosition(AnchorPos + FVector2D(0.0f, Step));
		}
		bChanged = true;

		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: добавлен пункт «Поддержать автора» под «Сообщество», нижние пункты сдвинуты на %.0f, панель подросла на столько же."),
			Name, Step);
	}

	// WBP_PauseMenu, ADR-074 (Ринат, 16.08.2026): постоянная подпись «Прогресс сохраняется у
	// костра в деревне» внизу панели, НАД строкой версии. Не кнопка — мелкий серый текст,
	// чуть заметнее версии. Текст, кегль и цвет — из FPauseMenuStyle: ОДНО место правды на
	// код и ассет (урок про копию текста в генераторе — копий формулировки не заводить).
	//
	// ⛔ Окно ОТДАНО ВЛАДЕЛЬЦУ (bOwnerOwned): пересобирать нельзя, ТОЛЬКО дополнением. Трогаем
	// ровно один новый кубик; соседей не двигаем и не убираем (слово Рината: «нигде не убирай
	// ничего»). Плашка подрастает ТОЛЬКО если иначе подпись легла бы на кнопку «Выход».
	//
	// Идемпотентно: кубик уже есть — выходим молча; текст владельца не переписываем (в игре
	// на дизайнер-дереве код его не ставит, значит хозяин текста в ассете — владелец).
	void AugmentPauseMenuSaveHint(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_PauseMenu");
		if (Tree->FindWidget(TEXT("SaveHintText")))
		{
			return; // уже добавлено — режим идемпотентный
		}

		// Якорь — строка версии: берём у неё привязку к низу и выравнивание, встаём над ней.
		UTextBlock* Version = Cast<UTextBlock>(Tree->FindWidget(TEXT("VersionText")));
		UCanvasPanelSlot* VersionSlot = Version ? Cast<UCanvasPanelSlot>(Version->Slot) : nullptr;
		UCanvasPanel* Parent = Version ? Cast<UCanvasPanel>(Version->GetParent()) : nullptr;
		if (!VersionSlot || !Parent)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: строка VersionText не найдена в канвас-слоте — некуда ставить подпись о сохранении."),
				Name);
			return;
		}

		const FPauseMenuStyle Style; // одно место правды на код и ассет
		UObject* Roboto = LoadRobotoFont();

		// Поднимаемся над версией на высоту её строки плюс зазор; кегль версии берём из
		// ЖИВОГО ассета — владелец мог его поменять.
		const float Rise = PauseSaveHintRiseAboveVersion(Version->GetFont().Size);
		const FVector2D HintPos = VersionSlot->GetPosition() - FVector2D(0.0f, Rise);
		const FAnchors HintAnchors = VersionSlot->GetAnchors();
		const FVector2D HintAlignment = VersionSlot->GetAlignment();
		// Ожидаемая высота самой подписи (та же оценка «1.4 кегля») — для проверки зазора до «Выхода».
		const float HintHeight = FMath::CeilToFloat(FMath::Max(6, Style.SaveHintFontSize) * 1.4f);

		UTextBlock* Hint = MakeText(Tree, Roboto, TEXT("SaveHintText"), Style.SaveHintText,
			Style.SaveHintColor, Style.SaveHintFontSize, TEXT("Regular"));
		Hint->SetJustification(ETextJustify::Center);
		Hint->bIsVariable = true;
		if (UCanvasPanelSlot* HintSlot = Parent->AddChildToCanvas(Hint))
		{
			HintSlot->SetAnchors(HintAnchors);
			HintSlot->SetAlignment(HintAlignment);
			HintSlot->SetAutoSize(true); // как у версии: размер по тексту
			HintSlot->SetPosition(HintPos);
		}
		bChanged = true;

		// Не налезает ли подпись на «Выход»? Подпись привязана к низу плашки, кнопка — к верху,
		// поэтому при нехватке места плашку растим на недостающее (и только тогда): версия и
		// подпись уедут вниз вместе с нижним краем, кнопки останутся на месте.
		float Grown = 0.0f;
		UWidget* Quit = Tree->FindWidget(TEXT("QuitButton"));
		UCanvasPanelSlot* QuitSlot = Quit ? Cast<UCanvasPanelSlot>(Quit->Slot) : nullptr;
		UWidget* Plate = Tree->FindWidget(TEXT("PanelPlate"));
		UCanvasPanelSlot* PlateSlot = Plate ? Cast<UCanvasPanelSlot>(Plate->Slot) : nullptr;
		if (QuitSlot && PlateSlot && Quit->GetParent() == Parent
			&& HintAnchors.Minimum.Y > QuitSlot->GetAnchors().Minimum.Y + KINDA_SMALL_NUMBER)
		{
			const float PlateHeight = PlateSlot->GetSize().Y;
			const float HintTop = PlateHeight * HintAnchors.Minimum.Y + HintPos.Y
				- HintAlignment.Y * HintHeight;
			const float QuitBottom = PlateHeight * QuitSlot->GetAnchors().Minimum.Y
				+ QuitSlot->GetPosition().Y
				+ (1.0f - QuitSlot->GetAlignment().Y) * QuitSlot->GetSize().Y;
			constexpr float MinGap = 8.0f;
			const float Missing = (QuitBottom + MinGap) - HintTop;
			if (Missing > KINDA_SMALL_NUMBER)
			{
				Grown = FMath::CeilToFloat(Missing);
				PlateSlot->SetSize(PlateSlot->GetSize() + FVector2D(0.0f, Grown));
			}
		}

		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: над строкой версии добавлена подпись «%s» (кегль %d, позиция %.0f от низа); плашка подросла на %.0f."),
			Name, *Style.SaveHintText.ToString(), Style.SaveHintFontSize, -HintPos.Y, Grown);
	}

	// ------------------------------------------------------------------
	// П.0 отчёта Рината 23.08 (ADR-077, системный разворот «всё настраивается в WBP»):
	// три процедуры ниже переносят В АССЕТЫ то, что раньше создавал код виджетов
	// (крестик диалога, компас на стике, строка перечня обыска). Код виджетов создание
	// удалил (коммит волны П.0) — без этих кубиков он пишет предупреждение и живёт дальше.
	// Все процедуры идемпотентны: кубик на месте — прогон ничего не меняет.
	// ------------------------------------------------------------------

	// WBP_Dialog: крестик закрытия в правом-верхнем углу (ADR-076 п.3) + перенос длинных
	// реплик на подписях кнопок (свойство переноса ставится В АССЕТЕ — Ринат его волен снять).
	void AugmentDialogCloseCrossAndWrap(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Dialog");

		// Часть 1: перенос текста подписей кнопок (кадр Рината: длинная реплика вылезала).
		// Читаем текущее значение через отражение (у свойства нет открытого геттера) — ради
		// честной идемпотентности: уже включено — не трогаем и не сохраняем.
		static const TCHAR* LabelNames[] = {
			TEXT("AcceptText"), TEXT("DeclineText"), TEXT("TurnInText"), TEXT("CloseText") };
		FBoolProperty* AutoWrapProp = FindFProperty<FBoolProperty>(
			UTextLayoutWidget::StaticClass(), TEXT("AutoWrapText"));
		for (const TCHAR* LabelName : LabelNames)
		{
			UTextBlock* Label = Cast<UTextBlock>(Tree->FindWidget(LabelName));
			if (!Label || !AutoWrapProp)
			{
				continue;
			}
			if (!AutoWrapProp->GetPropertyValue_InContainer(Label))
			{
				Label->SetAutoWrapText(true);
				bChanged = true;
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: у подписи %s включён перенос строк (реплика не вылезает за кнопку)."),
					Name, LabelName);
			}
		}

		// Часть 2: крестик закрытия — всегда в углу (ADR-076 п.3; стиль/позицию Ринат правит тут).
		if (Tree->FindWidget(TEXT("DialogCloseCrossButton")))
		{
			return; // уже добавлен
		}
		UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
		if (!Root)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: корень не канва — крестик закрытия не добавлен."), Name);
			return;
		}

		UObject* Roboto = LoadRobotoFont();
		UButton* Cross = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DialogCloseCrossButton"));
		Cross->bIsVariable = true; // BindWidgetOptional кода
		UTextBlock* CrossLabel = MakeText(Tree, Roboto, TEXT("DialogCloseCrossText"),
			TEXT("×") /* знак умножения «×» — читается крестиком */,
			FLinearColor::White, 30, TEXT("Bold"));
		CrossLabel->bIsVariable = true;
		Cross->SetContent(CrossLabel);
		if (UCanvasPanelSlot* CrossSlot = Root->AddChildToCanvas(Cross))
		{
			CrossSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f)); // правый-верхний угол
			CrossSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CrossSlot->SetPosition(FVector2D(-56.0f, 56.0f));
			CrossSlot->SetSize(FVector2D(72.0f, 72.0f)); // под палец
			CrossSlot->SetZOrder(10);
		}
		bChanged = true;
		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: добавлен крестик закрытия DialogCloseCrossButton (правый-верх, 72x72)."), Name);
	}

	// WBP_TouchControls: кубики компаса вокруг подложки стика (ADR-076 п.5). Позиции ставятся
	// ОДИН раз от живой геометрии StickBase — дальше их двигает Ринат; в игре весь компас
	// едет за стиком Render Translation'ом (код), слоты не трогаются.
	void AugmentTouchCompass(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_TouchControls");
		if (Tree->FindWidget(TEXT("CompassNText")))
		{
			return; // уже добавлен
		}

		UWidget* StickBase = Tree->FindWidget(TEXT("StickBase"));
		UCanvasPanelSlot* StickSlot = StickBase ? Cast<UCanvasPanelSlot>(StickBase->Slot) : nullptr;
		UCanvasPanel* Parent = StickBase ? Cast<UCanvasPanel>(StickBase->GetParent()) : nullptr;
		if (!StickSlot || !Parent)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: StickBase не найден в канвас-слоте — компас не добавлен."), Name);
			return;
		}

		const FVector2D StickPos = StickSlot->GetPosition();
		const FAnchors StickAnchors = StickSlot->GetAnchors();
		const float LetterRadius = StickSlot->GetSize().X * 0.5f + 26.0f; // край подложки + зазор

		UObject* Roboto = LoadRobotoFont();
		const FLinearColor LetterColor(1.0f, 1.0f, 1.0f, 0.55f); // полупрозрачно (слова Рината)

		auto MakeLetter = [&](const TCHAR* WidgetName, const TCHAR* Letter, const FVector2D& Offset)
		{
			UTextBlock* Text = MakeText(Tree, Roboto, WidgetName, Letter, LetterColor, 16, TEXT("Bold"));
			Text->bIsVariable = true; // BindWidgetOptional кода
			if (UCanvasPanelSlot* LetterSlot = Parent->AddChildToCanvas(Text))
			{
				LetterSlot->SetAnchors(StickAnchors); // как у стика — двигаются с ним при якорях
				LetterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				LetterSlot->SetAutoSize(true);
				LetterSlot->SetPosition(StickPos + Offset);
			}
		};
		MakeLetter(TEXT("CompassNText"), TEXT("N"), FVector2D(0.0f, -LetterRadius));
		MakeLetter(TEXT("CompassEText"), TEXT("E"), FVector2D(LetterRadius, 0.0f));
		MakeLetter(TEXT("CompassSText"), TEXT("S"), FVector2D(0.0f, LetterRadius));
		MakeLetter(TEXT("CompassWText"), TEXT("W"), FVector2D(-LetterRadius, 0.0f));

		// «Едва заметная тоненькая красная стрелка» из центра подложки к северу: узкая полоска,
		// смещена на полдлины вверх — читается указателем. Поворот на север ставит код.
		UImage* Arrow = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CompassArrowImage"));
		Arrow->bIsVariable = true;
		Arrow->SetColorAndOpacity(FLinearColor(1.0f, 0.12f, 0.08f, 0.55f));
		if (UCanvasPanelSlot* ArrowSlot = Parent->AddChildToCanvas(Arrow))
		{
			ArrowSlot->SetAnchors(StickAnchors);
			ArrowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ArrowSlot->SetPosition(StickPos + FVector2D(0.0f, -13.0f));
			ArrowSlot->SetSize(FVector2D(3.0f, 26.0f));
		}

		bChanged = true;
		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: добавлен компас (N/E/S/W на радиусе %.0f от центра стика + стрелка)."),
			Name, LetterRadius);
	}

	// Окно обыска: строка-перечень обыскиваемых («Труп волка; Труп волка; Мешок», ADR-076
	// п.2 / п.8 отчёта 23.08) + ВЫДЕЛЕННАЯ строка базы с уровнем («Логово волков — Ур. 4»,
	// Report1 п.14: «название базы выделялось жирным или цветом… рядом уровень»). Обе встают
	// НАД заголовком от его живой геометрии; текст в игре ставит код окна, стиль (жирность/
	// цвет выделения) и позицию правит Ринат здесь. Идемпотентно поблочно: каждый кубик со
	// своей проверкой — прогон по старому ассету добавит только недостающее.
	void AugmentCorpseSearchList(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_SearchWindow"); // бывш. WBP_CorpseLoot (ADR-077 п.8)
		UObject* Roboto = LoadRobotoFont();

		// Общая посадка строки над заголовком: OffsetY — насколько выше заголовка.
		auto PlaceAboveTitle = [&](UTextBlock* Line, float OffsetY) -> bool
		{
			UTextBlock* Title = Cast<UTextBlock>(Tree->FindWidget(TEXT("TitleText")));
			UCanvasPanelSlot* TitleSlot = Title ? Cast<UCanvasPanelSlot>(Title->Slot) : nullptr;
			UCanvasPanel* Parent = Title ? Cast<UCanvasPanel>(Title->GetParent()) : nullptr;
			if (TitleSlot && Parent)
			{
				if (UCanvasPanelSlot* LineSlot = Parent->AddChildToCanvas(Line))
				{
					LineSlot->SetAnchors(TitleSlot->GetAnchors());
					LineSlot->SetAlignment(TitleSlot->GetAlignment());
					LineSlot->SetAutoSize(true);
					LineSlot->SetPosition(TitleSlot->GetPosition() - FVector2D(0.0f, OffsetY));
				}
				return true;
			}
			if (UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget))
			{
				// Заголовка нет/не в канве — верх-центр окна («сверху в окне или над ним»).
				if (UCanvasPanelSlot* LineSlot = Root->AddChildToCanvas(Line))
				{
					LineSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
					LineSlot->SetAlignment(FVector2D(0.5f, 0.0f));
					LineSlot->SetAutoSize(true);
					LineSlot->SetPosition(FVector2D(0.0f, 122.0f - OffsetY));
				}
				return true;
			}
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: ни заголовка, ни корневой канвы — строка не добавлена."), Name);
			return false;
		};

		if (!Tree->FindWidget(TEXT("SearchObjectsListText")))
		{
			UTextBlock* Line = MakeText(Tree, Roboto, TEXT("SearchObjectsListText"),
				TEXT("Труп волка; Труп волка; Мешок") /* образец — в игре текст ставит код */,
				FLinearColor(0.8f, 0.8f, 0.8f, 1.0f), 14, TEXT("Regular"));
			Line->bIsVariable = true;
			Line->SetAutoWrapText(true);
			if (PlaceAboveTitle(Line, 26.0f))
			{
				bChanged = true;
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: добавлена строка перечня обыскиваемых SearchObjectsListText."), Name);
			}
		}

		// Report1 п.14: строка базы — ВЫШЕ перечня, стартовый стиль «выделено» (жирный,
		// тёплый золотой — как цифра количества плиток); дальше жирность/цвет крутит Ринат.
		if (!Tree->FindWidget(TEXT("SearchBaseNameText")))
		{
			UTextBlock* BaseLine = MakeText(Tree, Roboto, TEXT("SearchBaseNameText"),
				TEXT("Логово волков — Ур. 4") /* образец — в игре текст ставит код */,
				FLinearColor(1.0f, 0.85f, 0.3f, 1.0f), 16, TEXT("Bold"));
			BaseLine->bIsVariable = true;
			BaseLine->SetAutoWrapText(true);
			if (PlaceAboveTitle(BaseLine, 52.0f))
			{
				bChanged = true;
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: добавлена выделенная строка базы SearchBaseNameText (Report1 п.14)."), Name);
			}
		}
	}

	// WBP_SupportAuthor, решение Рината 13.08.2026: строка благодарности после ролика — это
	// одно слово «Спасибо», и сам кубик обязан быть в ассете. Ринат правил окно мышкой и
	// строку из него удалил (коммит 37dd9d8) — из-за этого код окна не находил свой кубик и
	// падали две проверки имён. Возвращаем кубик и приводим текст к новой правде.
	//
	// ⛔ Окно ОТДАНО ВЛАДЕЛЬЦУ (bOwnerOwned в таблице генератора): пересобирать его нельзя,
	// ТОЛЬКО дополнением. Прочих ручных правок не касаемся — трогаем ровно один кубик.
	//
	// Идемпотентно: кубик на месте и текст верный — прогон ничего не меняет и не сохраняет.
	void AugmentSupportAuthorThanks(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_SupportAuthor");
		const FSupportAuthorStyle Style; // одно место правды на код и ассет
		UObject* Roboto = LoadRobotoFont();

		// Кубик на месте — сверяем только текст (его в игре всё равно ставит код окна, но в
		// дизайнере владелец должен видеть то же слово).
		if (UTextBlock* Existing = Cast<UTextBlock>(Tree->FindWidget(TEXT("ThanksText"))))
		{
			if (!Existing->GetText().EqualTo(Style.ThanksText))
			{
				Existing->SetText(Style.ThanksText);
				bChanged = true;
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: строка благодарности приведена к «%s»."),
					Name, *Style.ThanksText.ToString());
			}
			return;
		}

		// Кубика нет — ставим его обратно под нижнюю кнопку окна, взяв геометрию у ЖИВОГО
		// ассета: владелец мог двигать кнопки, и строка обязана встать относительно них, а
		// не по зашитым в код числам.
		UWidget* Anchor = Tree->FindWidget(TEXT("SupportLinkButton"));
		UCanvasPanelSlot* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
		UCanvasPanel* Parent = Anchor ? Cast<UCanvasPanel>(Anchor->GetParent()) : nullptr;
		if (!AnchorSlot || !Parent)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: кнопка SupportLinkButton не найдена в канвас-слоте — некуда вернуть строку благодарности."),
				Name);
			return;
		}

		// Ширину строки берём у пояснения (оно тянется на всё содержимое окна), а если его
		// нет — у самой кнопки. Высота строки прежняя, 24 точки.
		FVector2D TextSize(AnchorSlot->GetSize().X, 24.0f);
		float TextLeft = AnchorSlot->GetPosition().X;
		if (UWidget* Message = Tree->FindWidget(TEXT("MessageText")))
		{
			if (UCanvasPanelSlot* MessageSlot = Cast<UCanvasPanelSlot>(Message->Slot))
			{
				TextSize.X = MessageSlot->GetSize().X;
				TextLeft = MessageSlot->GetPosition().X;
			}
		}

		UTextBlock* Thanks = MakeText(Tree, Roboto, TEXT("ThanksText"), Style.ThanksText,
			Style.TitleColor, Style.MessageFontSize, TEXT("Regular"));
		Thanks->SetJustification(ETextJustify::Center);
		Thanks->bIsVariable = true;

		// В АССЕТЕ строка видимая — владельцу нужно за что-то браться мышкой; в игре её
		// прячет код окна и показывает только после досмотренного ролика.
		if (UCanvasPanelSlot* ThanksSlot = Parent->AddChildToCanvas(Thanks))
		{
			ThanksSlot->SetAnchors(AnchorSlot->GetAnchors());
			ThanksSlot->SetAlignment(AnchorSlot->GetAlignment());
			ThanksSlot->SetPosition(FVector2D(TextLeft,
				AnchorSlot->GetPosition().Y + AnchorSlot->GetSize().Y + 12.0f));
			ThanksSlot->SetSize(TextSize);
		}
		bChanged = true;

		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: строка благодарности возвращена под нижнюю кнопку, текст «%s»."),
			Name, *Style.ThanksText.ToString());
	}

	// WBP_SupportAuthor, задача Рината 13.08.2026 (вторая): после ролика кнопка просмотра
	// исчезает (следующий ещё не загружен), и игрок не понимает, куда она делась. Под строкой
	// благодарности живёт подсказка «Новый ролик уже загружается — загляните в это окно ещё
	// раз.» В игре её показывает код окна ТОЛЬКО вместе с благодарностью и только
	// пока следующий ролик не готов (USupportAuthorWidget::ShouldShowNextAdHint); в ассете она
	// видимая — владельцу нужно за что-то браться мышкой (приём ThanksText).
	//
	// ⛔ Окно ОТДАНО ВЛАДЕЛЬЦУ: пересобирать нельзя, ТОЛЬКО дополнением, трогаем ровно один
	// кубик. Идемпотентно: кубик на месте и текст верный — прогон ничего не меняет.
	void AugmentSupportAuthorNextAdHint(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_SupportAuthor");
		const FSupportAuthorStyle Style; // одно место правды на код и ассет
		UObject* Roboto = LoadRobotoFont();

		// Кубик на месте — сверяем только текст (в игре его всё равно ставит код окна, но в
		// дизайнере владелец должен видеть ту же фразу).
		if (UTextBlock* Existing = Cast<UTextBlock>(Tree->FindWidget(TEXT("NextAdHintText"))))
		{
			if (!Existing->GetText().EqualTo(Style.NextAdHintText))
			{
				Existing->SetText(Style.NextAdHintText);
				bChanged = true;
				UE_LOG(LogGenerateWbp, Display,
					TEXT("AUGMENT %s: подсказка про следующий ролик приведена к «%s»."),
					Name, *Style.NextAdHintText.ToString());
			}
			return;
		}

		// Кубика нет — ставим под строку благодарности, взяв геометрию у ЖИВОГО ассета. Сама
		// строка к этому моменту есть: её гарантирует AugmentSupportAuthorThanks, который стоит
		// в таблице раньше и работает с тем же загруженным ассетом.
		UWidget* Anchor = Tree->FindWidget(TEXT("ThanksText"));
		UCanvasPanelSlot* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
		UCanvasPanel* Parent = Anchor ? Cast<UCanvasPanel>(Anchor->GetParent()) : nullptr;
		if (!AnchorSlot || !Parent)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("AUGMENT %s: строка ThanksText не найдена в канвас-слоте — некуда ставить подсказку про следующий ролик."),
				Name);
			return;
		}

		UTextBlock* Hint = MakeText(Tree, Roboto, TEXT("NextAdHintText"), Style.NextAdHintText,
			Style.TitleColor, Style.MessageFontSize, TEXT("Regular"));
		Hint->SetJustification(ETextJustify::Center);
		// Фраза длинная и на ширине окна переносится: перенос обязателен, высота — две строки.
		// SetAutoWrapText пишет сохраняемое свойство (TextWidgetTypes.cpp:56), в ассет попадёт.
		Hint->SetAutoWrapText(true);
		Hint->bIsVariable = true;

		if (UCanvasPanelSlot* HintSlot = Parent->AddChildToCanvas(Hint))
		{
			HintSlot->SetAnchors(AnchorSlot->GetAnchors());
			HintSlot->SetAlignment(AnchorSlot->GetAlignment());
			HintSlot->SetPosition(AnchorSlot->GetPosition()
				+ FVector2D(0.0f, AnchorSlot->GetSize().Y + 8.0f));
			HintSlot->SetSize(FVector2D(AnchorSlot->GetSize().X, 48.0f));
		}
		bChanged = true;

		UE_LOG(LogGenerateWbp, Display,
			TEXT("AUGMENT %s: под строкой благодарности добавлена подсказка про следующий ролик."),
			Name);
	}

	// WBP_Consent, требование издателя 13.08.2026: формулировки согласия в ассете обязаны
	// совпадать с опубликованной политикой (из текста убрана AppMetrica).
	//
	// Почему ДОПОЛНЕНИЕМ, а не пересборкой: окна согласия нет в списке разрешённых к
	// пересборке (RebuildAssets) — и правильно, целиком его переписывать ради одной строки
	// незачем. Трогаем РОВНО тексты четырёх подписей, раскладку и оформление не касаемся.
	//
	// На игрока это не влияет: UConsentScreenWidget::ApplyStyle ставит тексты абзацев всегда,
	// в обоих путях. Правка нужна, чтобы в дизайнере не висел устаревший абзац.
	//
	// Идемпотентно: тексты совпали — прогон ничего не меняет.
	void AugmentConsentTexts(UWidgetTree* Tree, bool& bChanged)
	{
		const TCHAR* Name = TEXT("WBP_Consent");
		const FConsentScreenStyle Texts; // одно место правды на код и ассет

		struct FConsentLabel
		{
			const TCHAR* CubeName;
			const FText& Text;
		};
		const FConsentLabel Labels[] =
		{
			{ TEXT("TitleText"), Texts.TitleText },
			{ TEXT("Body1Text"), Texts.BodyText1 },
			{ TEXT("Body2Text"), Texts.BodyText2 },
			{ TEXT("Body3Text"), Texts.BodyText3 },
		};

		for (const FConsentLabel& Label : Labels)
		{
			UTextBlock* Block = Cast<UTextBlock>(Tree->FindWidget(Label.CubeName));
			if (!Block)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("AUGMENT %s: подпись «%s» в ассете не найдена — текст обновить не на чем."),
					Name, Label.CubeName);
				continue;
			}
			if (Block->GetText().EqualTo(Label.Text))
			{
				continue; // уже совпадает
			}
			Block->SetText(Label.Text);
			bChanged = true;
			UE_LOG(LogGenerateWbp, Display,
				TEXT("AUGMENT %s: подпись «%s» приведена к тексту из настроек экрана."),
				Name, Label.CubeName);
		}
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

	// Экраны с сетками плиток (Build 1.2.2): класс плитки WBP_ItemTile_C на CDO окна.
	// Без него окно работает на кодовом дереве C++-плитки (дефолт в NativeOnInitialized),
	// но стилизация Рината из ассета плитки не подхватится — назначаем сами (guide требовал
	// ручного шага, генератор делает его за владельца — просьба лида). Зовётся ПОСЛЕ
	// компиляции (CDO свежий) и при генерации, и при пересборке (-rebuild).
	template <typename TScreenWidget>
	void AssignTileClass(const TCHAR* AssetName, UWidgetBlueprint* WBP)
	{
		UClass* TileClass = StaticLoadClass(UItemTileWidget::StaticClass(), nullptr,
			TEXT("/Game/UI/WBP_ItemTile.WBP_ItemTile_C"));
		TScreenWidget* CDO = WBP->GeneratedClass
			? Cast<TScreenWidget>(WBP->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (TileClass && CDO)
		{
			CDO->TileWidgetClass = TileClass;
			UE_LOG(LogGenerateWbp, Display, TEXT("%s: Tile Widget Class = %s."),
				AssetName, *TileClass->GetName());
		}
		else
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("%s: Tile Widget Class НЕ назначен (класс плитки=%d, CDO=%d) — окно живёт на кодовой C++-плитке."),
				AssetName, TileClass ? 1 : 0, CDO ? 1 : 0);
		}
	}

	// Проверка -verify: класс плитки на CDO окна назначен (пустой — окно молча живёт на
	// кодовой C++-плитке и стилизация Рината из WBP_ItemTile не подхватывается).
	template <typename TScreenWidget>
	bool VerifyTileClass(const TCHAR* AssetName, UWidgetBlueprint* WBP)
	{
		const TScreenWidget* CDO = WBP->GeneratedClass
			? Cast<TScreenWidget>(WBP->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (CDO && CDO->TileWidgetClass)
		{
			UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY %s: Tile Widget Class = %s."),
				AssetName, *CDO->TileWidgetClass->GetName());
			return true;
		}
		UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: %s — Tile Widget Class пуст."), AssetName);
		return false;
	}

	void ApplyTileClassFixup(const FWbpSpec& Spec, UWidgetBlueprint* WBP)
	{
		if (FCString::Strcmp(Spec.AssetName, TEXT("WBP_Inventory")) == 0)
		{
			AssignTileClass<UInventoryScreenWidget>(Spec.AssetName, WBP);
		}
		if (FCString::Strcmp(Spec.AssetName, TEXT("WBP_Shop")) == 0)
		{
			AssignTileClass<UShopScreenWidget>(Spec.AssetName, WBP);
		}
		if (FCString::Strcmp(Spec.AssetName, TEXT("WBP_SearchWindow")) == 0)
		{
			AssignTileClass<UCorpseLootWidget>(Spec.AssetName, WBP);
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

		ApplyTileClassFixup(Spec, WBP);

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

	// Build 1.2.1: переезд строки статов инвентаря «ряд-коробка StatsRow -> шесть
	// канвас-слотов». Расстановка владельца снимается со СТАРОГО дерева: позиция ряда — с
	// его канвас-слота, отступы — со старых hbox-слотов детей, ширины иконок — из их кистей
	// (Brush.ImageSize; в реальном ассете иконки без явного размера кисти рисуются 32 px —
	// прежний SetDesiredSizeOverride(22) в ассет не сериализовался). Ширины ТЕКСТОВ в
	// ассете не хранятся (это Slate-замер, в коммандлете недоступный) — берётся оценка
	// образцов по метрике Roboto 16. Ряда в старом дереве нет — пары остаются на штатных
	// позициях свежей генерации (громкая строка в лог, сверить глазами).
	void ConvertInventoryStatsRowToPairSlots(UWidgetTree* Tree, const TCHAR* AssetName,
		const TMap<FName, UWidget*>& OldWidgets)
	{
		UWidget* const* OldRow = OldWidgets.Find(FName(TEXT("StatsRow")));
		const UCanvasPanelSlot* OldRowSlot = OldRow ? Cast<UCanvasPanelSlot>((*OldRow)->Slot) : nullptr;
		if (!OldRowSlot)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("REBUILD %s: ряда 'StatsRow' в старом дереве нет (или он не в канвас-слоте) — пары статов остались на штатных позициях, сверить вид глазами."),
				AssetName);
			return;
		}
		const FVector2D RowPos = OldRowSlot->GetPosition();

		struct FPairSpec
		{
			const TCHAR* IconName;
			const TCHAR* ValueName;
			float SampleTextWidth; // оценка ширины образца значения (Roboto 16)
			float DefaultIconPad;  // отступ слева по прежней генерации
		};
		const FPairSpec Pairs[] =
		{
			{ TEXT("InvMoneyIcon"),  TEXT("InvMoneyText"),   9.0f,  0.0f }, // «0»
			{ TEXT("InvHungerIcon"), TEXT("InvHungerText"), 78.0f, 24.0f }, // «100 из 100»
			{ TEXT("InvThirstIcon"), TEXT("InvThirstText"), 78.0f, 24.0f },
		};

		// Отступ слева старого hbox-слота (нет слота — дефолт прежней генерации).
		auto OldLeftPad = [&OldWidgets](const TCHAR* WidgetName, float Default)
		{
			UWidget* const* Old = OldWidgets.Find(FName(WidgetName));
			const UHorizontalBoxSlot* HSlot = Old ? Cast<UHorizontalBoxSlot>((*Old)->Slot) : nullptr;
			return HSlot ? HSlot->GetPadding().Left : Default;
		};
		// Фактический размер старой иконки из её кисти.
		auto OldIconSize = [&OldWidgets](const TCHAR* WidgetName)
		{
			FVector2D Size(22.0f, 22.0f);
			UWidget* const* Old = OldWidgets.Find(FName(WidgetName));
			if (const UImage* Img = Old ? Cast<UImage>(*Old) : nullptr)
			{
				Size = Img->GetBrush().GetImageSize();
			}
			return Size;
		};

		// Высота прежнего ряда = самый высокий ребёнок (тексты 16pt ≈ 21 px ниже иконок);
		// дети центрировались по вертикали — центр переносится на канвас-позиции.
		float RowHeight = 21.0f;
		for (const FPairSpec& Pair : Pairs)
		{
			RowHeight = FMath::Max(RowHeight, static_cast<float>(OldIconSize(Pair.IconName).Y));
		}
		const float CenterY = RowPos.Y + RowHeight * 0.5f;

		float X = RowPos.X;
		for (const FPairSpec& Pair : Pairs)
		{
			const FVector2D IconSize = OldIconSize(Pair.IconName);
			X += OldLeftPad(Pair.IconName, Pair.DefaultIconPad);
			if (UWidget* NewIcon = Tree->FindWidget(FName(Pair.IconName)))
			{
				if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(NewIcon->Slot))
				{
					Slot->SetPosition(FVector2D(X, CenterY));
				}
			}
			X += IconSize.X;
			X += OldLeftPad(Pair.ValueName, 6.0f);
			if (UWidget* NewValue = Tree->FindWidget(FName(Pair.ValueName)))
			{
				if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(NewValue->Slot))
				{
					Slot->SetPosition(FVector2D(X, CenterY));
				}
			}
			UE_LOG(LogGenerateWbp, Display,
				TEXT("REBUILD %s: пара '%s'+'%s' разложена от X=%.0f (иконка %.0fx%.0f, центр Y=%.1f)."),
				AssetName, Pair.IconName, Pair.ValueName,
				X - OldLeftPad(Pair.ValueName, 6.0f) - IconSize.X, IconSize.X, IconSize.Y, CenterY);
			X += Pair.SampleTextWidth;
		}
	}

	// Build 1.2.2: переезд «один слот оружия -> два» (Ринат: «должен быть один слот под
	// холодное оружие и один слот под огнестрельное»). Слот огнестрела встаёт РОВНО на
	// место прежнего единственного слота — вид и кисть владельца переносятся на него
	// поимённо (WeaponSlotIcon -> RangedSlotIcon, WeaponText -> RangedSlotText); слот
	// холодного оружия — копия огнестрельного, сдвинутая НИЖЕ, а если ниже уже нет места
	// (владелец держит слот у самого низа панели) — ВПРАВО. Шаг по имени в белом списке
	// не пройдёт (там сверяются одинаковые имена), поэтому перенос делается здесь вручную.
	// Старого слота в дереве нет — новые остаются на штатных позициях свежей генерации
	// (громкая строка в лог, сверить глазами).
	void ConvertInventoryWeaponSlotToTwoSlots(UWidgetTree* Tree, const TCHAR* AssetName,
		const TMap<FName, UWidget*>& OldWidgets)
	{
		struct FMoveSpec
		{
			const TCHAR* OldName;
			const TCHAR* RangedName;
			const TCHAR* MeleeName;
		};
		const FMoveSpec Moves[] =
		{
			{ TEXT("WeaponSlotIcon"), TEXT("RangedSlotIcon"), TEXT("MeleeSlotIcon") },
			{ TEXT("WeaponText"),     TEXT("RangedSlotText"), TEXT("MeleeSlotText") },
		};

		// Габарит строки = самый крупный перенесённый кубик (обычно иконка; у текста
		// авторазмер, в ассете его размер не хранится). Ничего не нашлось — шаг по штатной
		// генерации. Заодно запоминаем самый НИЖНИЙ кубик блока оружия.
		float RowHeight = WeaponSlotIconSize;
		float RowWidth = WeaponSlotIconSize;
		float LowestTop = 0.0f;
		for (const FMoveSpec& Move : Moves)
		{
			UWidget* const* Old = OldWidgets.Find(FName(Move.OldName));
			if (const UCanvasPanelSlot* OldSlot = Old ? Cast<UCanvasPanelSlot>((*Old)->Slot) : nullptr)
			{
				LowestTop = FMath::Max(LowestTop, OldSlot->GetLayout().Offsets.Top);
				if (!OldSlot->GetAutoSize())
				{
					RowHeight = FMath::Max(RowHeight, static_cast<float>(OldSlot->GetSize().Y));
					RowWidth = FMath::Max(RowWidth, static_cast<float>(OldSlot->GetSize().X));
				}
			}
		}

		// Куда ставить второй слот. Вниз — если строка целиком влезает в панель владельца
		// (её высота за вычетом отступа рамки UIPanelPadding=16 с двух сторон). У Рината
		// после приёмки блок оружия стоит у самого низа — там вниз места нет, и слот уехал
		// бы за край окна; в этом случае ставим вправо, свободная ширина панели это позволяет.
		float PanelInnerHeight = 0.0f;
		if (UWidget* const* OldPanel = OldWidgets.Find(FName(TEXT("PanelPlate"))))
		{
			if (const UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>((*OldPanel)->Slot))
			{
				PanelInnerHeight = static_cast<float>(PanelSlot->GetSize().Y) - 2.0f * 16.0f;
			}
		}
		const float StepY = RowHeight + 8.0f;
		const float StepX = RowWidth + 8.0f;
		const bool bPlaceBelow = PanelInnerHeight <= 0.0f || (LowestTop + StepY <= PanelInnerHeight);
		const FVector2D Step = bPlaceBelow ? FVector2D(0.0f, StepY) : FVector2D(StepX, 0.0f);

		bool bMovedAny = false;
		for (const FMoveSpec& Move : Moves)
		{
			UWidget* const* Old = OldWidgets.Find(FName(Move.OldName));
			const UCanvasPanelSlot* OldSlot = Old ? Cast<UCanvasPanelSlot>((*Old)->Slot) : nullptr;
			if (!OldSlot)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("REBUILD %s: старого кубика '%s' в дереве нет (или он не в канвас-слоте) — слоты '%s'/'%s' остались на штатных позициях, сверить вид глазами."),
					AssetName, Move.OldName, Move.RangedName, Move.MeleeName);
				continue;
			}

			// Вид владельца (кисть с её размером / шрифт) — с одного старого кубика на оба новых.
			TArray<FName> PropNames;
			UWidget* NewRanged = Tree->FindWidget(FName(Move.RangedName));
			UWidget* NewMelee = Tree->FindWidget(FName(Move.MeleeName));
			if (NewRanged && NewRanged->GetClass() == (*Old)->GetClass())
			{
				CollectOwnerStyleProps(NewRanged, PropNames);
				for (const FName& PropName : PropNames)
				{
					FProperty* Prop = FindFProperty<FProperty>(NewRanged->GetClass(), PropName);
					if (!Prop)
					{
						continue;
					}
					const void* OldValue = Prop->ContainerPtrToValuePtr<const void>(*Old);
					// Текст названия каждый кадр переписывает код игры — переносить его
					// со старого кубика незачем (иначе в слоте холодного оружия в
					// дизайнере висело бы имя пистолета), всё остальное это вид владельца.
					if (PropName == FName(TEXT("Text")))
					{
						continue;
					}
					Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(NewRanged), OldValue);
					if (NewMelee && NewMelee->GetClass() == (*Old)->GetClass())
					{
						Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(NewMelee), OldValue);
					}
				}
			}

			// Расстановка: огнестрел — точно на место старого слота, холодное — строкой ниже.
			const FAnchorData OldLayout = OldSlot->GetLayout();
			if (UCanvasPanelSlot* RangedSlot = NewRanged ? Cast<UCanvasPanelSlot>(NewRanged->Slot) : nullptr)
			{
				RangedSlot->SetLayout(OldLayout);
				RangedSlot->SetAutoSize(OldSlot->GetAutoSize());
				RangedSlot->SetZOrder(OldSlot->GetZOrder());
			}
			if (UCanvasPanelSlot* MeleeSlot = NewMelee ? Cast<UCanvasPanelSlot>(NewMelee->Slot) : nullptr)
			{
				FAnchorData MeleeLayout = OldLayout;
				MeleeLayout.Offsets.Left += Step.X;
				MeleeLayout.Offsets.Top += Step.Y;
				MeleeSlot->SetLayout(MeleeLayout);
				MeleeSlot->SetAutoSize(OldSlot->GetAutoSize());
				MeleeSlot->SetZOrder(OldSlot->GetZOrder());
			}
			bMovedAny = true;
			UE_LOG(LogGenerateWbp, Display,
				TEXT("REBUILD %s: '%s' (%.0f,%.0f) -> '%s' на том же месте, '%s' со сдвигом (%.0f,%.0f)."),
				AssetName, Move.OldName, OldLayout.Offsets.Left, OldLayout.Offsets.Top,
				Move.RangedName, Move.MeleeName, Step.X, Step.Y);
		}

		if (bMovedAny)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("REBUILD %s: второй слот оружия (холодное) поставлен %s первого (сдвиг %.0f,%.0f; низ блока был на %.0f, внутренняя высота панели %.0f) — если он лёг на другой кубик, подвинуть мышкой в дизайнере."),
				AssetName, bPlaceBelow ? TEXT("ПОД") : TEXT("СПРАВА от"),
				Step.X, Step.Y, LowestTop, PanelInnerHeight);
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
				// Корнем панели статов теперь может быть безопасная зона экрана (Б9), а канва
				// лежать внутри неё — ищем канву и там тоже, иначе перенос сдвига молча пропал бы.
				UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
				if (!Root)
				{
					if (const UPanelWidget* Wrapper = Cast<UPanelWidget>(Tree->RootWidget))
					{
						Root = Wrapper->GetChildrenCount() > 0
							? Cast<UCanvasPanel>(Wrapper->GetChildAt(0)) : nullptr;
					}
				}
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

			// Кубик, ЗАМКНУТЫЙ в старом ассете, владелец править не мог: дизайнер не пускает
			// его ни в сетку выделения (SDesignerView.cpp:2060-2070), ни в панель «Детали»
			// (SWidgetDetailsView.cpp:375-393). Значит его вид в старом ассете — прежний
			// дефолт генератора, а не работа владельца. Поэтому при размыкании (волна
			// ADR-056: начинка слотов брони) берём вид и место из НОВОЙ генерации, иначе
			// перенос вернул бы ровно ту мелкую иконку, из-за которой замок и снимали.
			if (OldWidget->IsLockedInDesigner() && !NewWidget->IsLockedInDesigner())
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("REBUILD %s: '%s' в старом ассете был замкнут и теперь размыкается — вид взят из новой генерации (править его владелец не мог)."),
					AssetName, *NewWidget->GetName());
				continue;
			}

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

				// Одноразовая миграция (08-06, кадр phone-dist-03-death.png): KillerText экрана
				// смерти прежний генератор клал как «значение пары» — левым краем в +4 от оси
				// (после ADR-059 там цельная фраза «Тебя убил волк», и она выглядела съехавшей
				// вправо). Если в старом ассете кубик стоит РОВНО в этой генераторской точке —
				// владелец его не трогал, это не ручная расстановка, и легаси НЕ переносится:
				// остаётся новая центровка по оси. Любое другое положение — правка владельца,
				// переезжает как обычно ниже (принцип «ручная расстановка выигрывает» цел).
				bool bUntouchedLegacyKillerText = false;
				if (FCString::Strcmp(AssetName, TEXT("WBP_Death")) == 0
					&& NewWidget->GetFName() == FName(TEXT("KillerText"))
					&& OldCanvasSlot->GetAutoSize())
				{
					FAnchorData LegacyValueLayout;
					LegacyValueLayout.Anchors = FAnchors(0.5f, 0.45f, 0.5f, 0.45f);
					LegacyValueLayout.Offsets = FMargin(4.0f, -132.0f, 100.0f, 30.0f);
					LegacyValueLayout.Alignment = FVector2D::ZeroVector;
					bUntouchedLegacyKillerText = (OldLayout == LegacyValueLayout);
				}

				const bool bSame = OldLayout == NewCanvasSlot->GetLayout()
					&& OldCanvasSlot->GetAutoSize() == NewCanvasSlot->GetAutoSize()
					&& OldCanvasSlot->GetZOrder() == NewCanvasSlot->GetZOrder();
				if (bUntouchedLegacyKillerText)
				{
					if (!bSame)
					{
						UE_LOG(LogGenerateWbp, Display,
							TEXT("REBUILD %s: 'KillerText' стоял в нетронутой легаси-точке «значения пары» (офсет 4.0,-132.0, вырав 0,0) — легаси не перенесено, оставлена новая центровка по оси (миграция 08-06)."),
							AssetName);
						++MovedCount;
					}
				}
				else if (!bSame)
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
		// (заменён канвас-слотами ещё в ADR-051); с Build 1.2.1 (Д1) — ряды-коробки статов
		// с их SizeBox'ами (полоски переехали в свои канвас-слоты, геометрию перенёс
		// ConvertPlayerStatsRowsToBarSlots); с волны разлочки Build 1.2.1 — ряды-коробки
		// экрана смерти (пары в канвас-слотах), StatsRow инвентаря
		// (ConvertInventoryStatsRowToPairSlots), RowBox строк списков (канвас RowCanvas) и
		// обёртка SliderQtyAmmoRow магазина (текст прячется напрямую). Всё остальное —
		// громко: владелец мог создать кубик руками (например, AmmoBagText), автоматически
		// его не вернуть — только руками по этому логу.
		const FName IntentionallyDropped[] =
		{
			FName(TEXT("StatsBox")),
			FName(TEXT("HealthRow")), FName(TEXT("HungerRow")), FName(TEXT("ThirstRow")),
			FName(TEXT("HealthBarSize")), FName(TEXT("HungerBarSize")), FName(TEXT("ThirstBarSize")),
			FName(TEXT("LifetimeTextRow")), FName(TEXT("KillerTextRow")), FName(TEXT("MoneyTextRow")),
			FName(TEXT("QuestsTextRow")), FName(TEXT("KillsTextRow")),
			FName(TEXT("StatsRow")), FName(TEXT("RowBox")), FName(TEXT("SliderQtyAmmoRow")),
			// Build 1.2.2: единственный слот оружия разошёлся на два — вид и место старых
			// кубиков переносит ConvertInventoryWeaponSlotToTwoSlots.
			FName(TEXT("WeaponText")), FName(TEXT("WeaponSlotIcon")),
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
		// Тот же переезд для строки статов инвентаря (волна разлочки Build 1.2.1) и для
		// слота оружия, разошедшегося на два (Build 1.2.2).
		if (FCString::Strcmp(AssetName, TEXT("WBP_Inventory")) == 0)
		{
			ConvertInventoryStatsRowToPairSlots(Tree, AssetName, OldWidgets);
			ConvertInventoryWeaponSlotToTwoSlots(Tree, AssetName, OldWidgets);
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

		ApplyTileClassFixup(Spec, WBP);

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
	if (Switches.Contains(TEXT("unlockall")))
	{
		return UnlockAllDesignerLocks();
	}
	if (Switches.Contains(TEXT("unlockcaptions")))
	{
		return UnlockButtonCaptions();
	}
	if (Switches.Contains(TEXT("hudslots")))
	{
		return FixHudWidgetSlots();
	}
	if (Switches.Contains(TEXT("dropdead")))
	{
		return DropDeadWidgets();
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

	// Видимость и прозрачность (задача Г 08-08: якоря рядов совпадали, а невидимость
	// геометрический дамп не ловил — печатаем и её). «видим» — штатные значения; всё
	// остальное печатается явно, чтобы дифф рядов сразу показывал спрятанный кубик.
	FString VisDesc;
	const ESlateVisibility Vis = Widget->GetVisibility();
	if (Vis == ESlateVisibility::Collapsed || Vis == ESlateVisibility::Hidden)
	{
		VisDesc += (Vis == ESlateVisibility::Collapsed) ? TEXT(" СХЛОПНУТ") : TEXT(" СКРЫТ");
	}
	if (!FMath::IsNearlyEqual(Widget->GetRenderOpacity(), 1.0f))
	{
		VisDesc += FString::Printf(TEXT(" прозрачность=%.2f"), Widget->GetRenderOpacity());
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("SLOTS %s: %s%s : %s : %s%s"),
		AssetName, *FString::ChrN(Depth * 2, TEXT(' ')), *Widget->GetName(),
		*Widget->GetClass()->GetName(), *SlotDesc, *VisDesc);

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
	// 07-28: диалог и экран смерти; волна разлочки Build 1.2.1: экран смерти и строка
	// статов инвентаря без рядов-коробок; Build 1.2.2: вместо строк списков — общая
	// плитка WBP_ItemTile, она ПЕРВАЯ в списке: фиксап окон ищет её класс _C на диске;
	// окно обыска добавлено ради того же фиксапа). Перенос значений владельца
	// (TransferOwnerStyle) включён ВСЕМ ассетам списка: WBP_PlayerStats — коммит dfaffd0,
	// WBP_Dialog/WBP_Shop/WBP_Inventory — коммит 5c2b058, WBP_Inventory дополнительно —
	// ручная расстановка Рината 562c85d; остальным — на будущее (правок владельца
	// могло прибавиться, перенос на идентичном дереве безвреден). Остальные ассеты
	// пересборке не подлежат. Процессный предохранитель (проверяет лид перед запуском):
	// git status пересобираемых .uasset должен быть чист — иначе прогон затёр бы
	// несохранённые правки.
	struct FRebuildEntry
	{
		const TCHAR* AssetName;
		bool bTransferOwnerStyle;
	};
	static const FRebuildEntry RebuildAssets[] =
	{
		{ TEXT("WBP_ItemTile"), true },
		// Надпись входа на базу (П.0 ADR-077): раскладка из кода при СОЗДАНИИ; правок
		// владельца нет — перенос значений не нужен (как WBP_StartScreen).
		{ TEXT("WBP_BaseAnnounce"), false },
		{ TEXT("WBP_Shop"), true },
		{ TEXT("WBP_Inventory"), true },
		{ TEXT("WBP_SearchWindow"), true }, // бывш. WBP_CorpseLoot (ADR-077 п.8, переименован оператором)
		{ TEXT("WBP_PlayerStats"), true },
		{ TEXT("WBP_Dialog"), true },
		{ TEXT("WBP_Death"), true },
		// ADR-062 (волна меню): ввод главного меню в живой ассет — точечной пересборкой
		// `-rebuild -asset=WBP_StartScreen`.
		//
		// ⛔ ПЕРЕНОС ЗНАЧЕНИЙ ВЛАДЕЛЬЦА ЗДЕСЬ ВЫКЛЮЧЕН — и включать обратно нельзя.
		// Пересборка 08-09 с включённым переносом дала на телефоне разъехавшееся меню:
		// старая геометрия ПЕРВОЙ версии экрана (двухкнопочной, коммит ad93f73) перетёрла
		// новую раскладку из кода. Дословно из журнала `Saved/gl-startscreen-rebuild.log:214-216`:
		// `PanelPlate: (0,0,480,560) -> (0,0,480,320)`, `ContinueButton: (0,126,280,58) -> (0,-100,280,58)`,
		// `NewGameButton: (0,196,280,58) -> (0,-30,280,58)`. Новые пункты встали по коду, старые —
		// по мёртвым значениям, и кнопки налезли друг на друга.
		// Переносить в этом экране НЕЧЕГО: правок владельца в нём нет (единственный коммит —
		// сама генерация). Раскладка меню живёт в коде (BuildStartScreen), и пересборка обязана
		// класть её целиком. Остальные строки таблицы это НЕ затрагивает.
		{ TEXT("WBP_StartScreen"), false },
		// ADR-062 (подход 2 волны меню): экран настроек, точечная пересборка
		// `-rebuild -asset=WBP_Settings`.
		//
		// ⛔ ПЕРЕНОС ЗНАЧЕНИЙ ВЛАДЕЛЬЦА ВЫКЛЮЧЕН (08-09, разбор перед волной «под палец»).
		// Причина ровно та же, что у главного меню строкой выше, только пострадали бы не
		// координаты, а КЕГЛИ: перенос копирует у одноимённых подписей свойство Font
		// (CollectOwnerStyleProps), а в старом ассете лежат прежние мелкие 24/17/15/16 —
		// пересборка молча вернула бы их поверх новых 52/42/34/36, и на телефоне всё
		// осталось бы таким же мелким. Переносить здесь НЕЧЕГО: правок владельца в этом
		// окне нет (единственный коммит ассета — сама генерация, c4b6753), а вид и размеры
		// живут в коде (BuildSettings).
		{ TEXT("WBP_Settings"), false },
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
				// ⛔ Окно отдано владельцу — пересборка стёрла бы его ручную настройку.
				// Отказ ГРОМКИЙ, но это не ошибка прогона: так и задумано.
				if (Spec.bOwnerOwned)
				{
					UE_LOG(LogGenerateWbp, Warning,
						TEXT("REBUILD SKIP: %s отдан владельцу — раскладку он правит мышкой, пересборка её сотрёт. Недостающие кубики добавляй режимом -augment."),
						Spec.AssetName);
					break;
				}
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
		{ TEXT("/Game/UI/WBP_PlayerStats"),   TEXT("WBP_PlayerStats"),   &AugmentPlayerStatsSafeZone },
		{ TEXT("/Game/UI/WBP_Shop"),          TEXT("WBP_Shop"),          &AugmentShopAmmoRow },
		{ TEXT("/Game/UI/WBP_Shop"),          TEXT("WBP_Shop"),          &AugmentShopSellAdButton },
		{ TEXT("/Game/UI/WBP_Shop"),          TEXT("WBP_Shop"),          &AugmentShopTouchTargets },
		{ TEXT("/Game/UI/WBP_QuestTracker"),  TEXT("WBP_QuestTracker"),  &AugmentQuestTracker },
		{ TEXT("/Game/UI/WBP_TouchControls"), TEXT("WBP_TouchControls"), &AugmentTouchControls },
		// Подход 3 волны меню: пункт «В главное меню» в живую панель паузы.
		{ TEXT("/Game/UI/WBP_PauseMenu"),     TEXT("WBP_PauseMenu"),     &AugmentPauseMenu },
		// Волна 08-09: «Настройки» и «Сообщество» в паузе + подпись политики, которая не
		// помещалась. Отдельной записью со своей проверкой «уже сделано» — как у WBP_Shop.
		{ TEXT("/Game/UI/WBP_PauseMenu"),     TEXT("WBP_PauseMenu"),     &AugmentPauseMenuExtras },
		// Задание издателя 11.08.2026: строка «Поддержать автора». Отдельной записью со своей
		// проверкой «уже сделано» — иначе повторный прогон завёл бы вторую такую же строку.
		{ TEXT("/Game/UI/WBP_PauseMenu"),     TEXT("WBP_PauseMenu"),     &AugmentPauseMenuSupport },
		// ADR-074 (Ринат 16.08.2026): постоянная подпись «прогресс сохраняется у костра» над
		// строкой версии. Отдельной записью со своей проверкой «уже сделано».
		{ TEXT("/Game/UI/WBP_PauseMenu"),     TEXT("WBP_PauseMenu"),     &AugmentPauseMenuSaveHint },
		// Решение Рината 13.08.2026: вернуть строку благодарности в окно поддержки и оставить
		// в ней одно слово «Спасибо». Окно отдано владельцу — только дополнением.
		{ TEXT("/Game/UI/WBP_SupportAuthor"), TEXT("WBP_SupportAuthor"), &AugmentSupportAuthorThanks },
		// Задача Рината 13.08.2026 (вторая): подсказка «новый ролик уже загружается» под
		// строкой благодарности. Отдельной записью со своей проверкой «уже сделано»;
		// стоит ПОСЛЕ возврата самой строки — подсказка встаёт от её геометрии.
		{ TEXT("/Game/UI/WBP_SupportAuthor"), TEXT("WBP_SupportAuthor"), &AugmentSupportAuthorNextAdHint },
		// Требование издателя 13.08.2026: из текста согласия убрана AppMetrica. Пересборке
		// это окно не подлежит (его нет в списке RebuildAssets) — правим только подписи.
		{ TEXT("/Game/UI/WBP_Consent"),       TEXT("WBP_Consent"),       &AugmentConsentTexts },
		// П.0 отчёта Рината 23.08 (ADR-077): кодовые элементы переезжают В АССЕТЫ —
		// крестик диалога + перенос подписей, компас на стике, строка перечня обыска.
		{ TEXT("/Game/UI/WBP_Dialog"),        TEXT("WBP_Dialog"),        &AugmentDialogCloseCrossAndWrap },
		{ TEXT("/Game/UI/WBP_TouchControls"), TEXT("WBP_TouchControls"), &AugmentTouchCompass },
		{ TEXT("/Game/UI/WBP_SearchWindow"),  TEXT("WBP_SearchWindow"),  &AugmentCorpseSearchList },
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

// ======================================================================
// Поправка 08-07: тексты кнопок правятся владельцем + слот окна обыска на HUD
// ======================================================================

// Слоты UMG-классов на CDO блюпринтов (ADR-048; доступ через reflection — поля protected).
// Волна 08-07: прежний точечный фикс CorpseLootWidgetClass обобщён таблицей — «какой слот
// какого блюпринта каким классом WBP заполнять, если он пуст». Слоты окон контроллера
// (стартовый экран/интро/пауза) живут на BP контроллера: эти окна создаёт он сам.
static const TCHAR* GHudBlueprintPackage = TEXT("/Game/UI/BP_ContrarySurvivorHUD");
static const TCHAR* GHudBlueprintPath = TEXT("/Game/UI/BP_ContrarySurvivorHUD.BP_ContrarySurvivorHUD");
static const TCHAR* GPcBlueprintPackage = TEXT("/Game/System/BP_ContrarySurviorPlayerController");
static const TCHAR* GPcBlueprintPath =
	TEXT("/Game/System/BP_ContrarySurviorPlayerController.BP_ContrarySurviorPlayerController");

struct FCdoSlotSpec
{
	const TCHAR* BlueprintPackage; // пакет BP (для SavePackage)
	const TCHAR* BlueprintPath;    // объектный путь BP
	UClass* (*BaseClass)();        // нативный родитель BP (проверка «тот ли ассет»)
	const TCHAR* SlotName;         // имя FClassProperty на CDO
	const TCHAR* WidgetClassPath;  // класс WBP_..._C, которым заполняется пустой слот
};

// ВАЖНО: слоты, ещё не объявленные в C++ (перенос правки чужого файла на следующую волну),
// дают Warning и пропуск, а не ошибку: игра остаётся играбельной на кодовом фолбэке.
static const FCdoSlotSpec GCdoSlots[] =
{
	{ GHudBlueprintPackage, GHudBlueprintPath, &AContrarySurvivorHUD::StaticClass,
		TEXT("CorpseLootWidgetClass"), TEXT("/Game/UI/WBP_SearchWindow.WBP_SearchWindow_C") },
	{ GHudBlueprintPackage, GHudBlueprintPath, &AContrarySurvivorHUD::StaticClass,
		TEXT("DailyRewardWidgetClass"), TEXT("/Game/UI/WBP_DailyReward.WBP_DailyReward_C") },
	{ GHudBlueprintPackage, GHudBlueprintPath, &AContrarySurvivorHUD::StaticClass,
		TEXT("ConsentWidgetClass"), TEXT("/Game/UI/WBP_Consent.WBP_Consent_C") },
	{ GHudBlueprintPackage, GHudBlueprintPath, &AContrarySurvivorHUD::StaticClass,
		TEXT("OnboardingHintWidgetClass"), TEXT("/Game/UI/WBP_OnboardingHint.WBP_OnboardingHint_C") },
	{ GHudBlueprintPackage, GHudBlueprintPath, &AContrarySurvivorHUD::StaticClass,
		TEXT("LimpIndicatorWidgetClass"), TEXT("/Game/UI/WBP_LimpIndicator.WBP_LimpIndicator_C") },
	{ GHudBlueprintPackage, GHudBlueprintPath, &AContrarySurvivorHUD::StaticClass,
		TEXT("MockAdWidgetClass"), TEXT("/Game/UI/WBP_MockAd.WBP_MockAd_C") },
	{ GHudBlueprintPackage, GHudBlueprintPath, &AContrarySurvivorHUD::StaticClass,
		TEXT("IntroObjectiveWidgetClass"), TEXT("/Game/UI/WBP_IntroObjective.WBP_IntroObjective_C") },
	{ GPcBlueprintPackage, GPcBlueprintPath, &AContrarySurvivorPlayerController::StaticClass,
		TEXT("StartScreenWidgetClass"), TEXT("/Game/UI/WBP_StartScreen.WBP_StartScreen_C") },
	{ GPcBlueprintPackage, GPcBlueprintPath, &AContrarySurvivorPlayerController::StaticClass,
		TEXT("SettingsScreenWidgetClass"), TEXT("/Game/UI/WBP_Settings.WBP_Settings_C") },
	{ GPcBlueprintPackage, GPcBlueprintPath, &AContrarySurvivorPlayerController::StaticClass,
		TEXT("IntroScreenWidgetClass"), TEXT("/Game/UI/WBP_Intro.WBP_Intro_C") },
	{ GPcBlueprintPackage, GPcBlueprintPath, &AContrarySurvivorPlayerController::StaticClass,
		TEXT("PauseMenuWidgetClass"), TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C") },
	// Окно «Поддержать автора» (12.08.2026). Без заполненного слота окно молча живёт на
	// кодовом запасном дереве, и правка ассета мышкой в игре не видна — ровно та беда, ради
	// которой этот список и заведён.
	{ GPcBlueprintPackage, GPcBlueprintPath, &AContrarySurvivorPlayerController::StaticClass,
		TEXT("SupportWidgetClass"), TEXT("/Game/UI/WBP_SupportAuthor.WBP_SupportAuthor_C") },
};

// Печать-пруф: все слоты UMG-классов HUD (стабильный формат — срезы «до/после» сравниваются
// глазами). Расширен слотами волны 08-07.
static const TCHAR* GHudWidgetSlotNames[] =
{
	TEXT("ShopWidgetClass"), TEXT("DialogWidgetClass"), TEXT("InventoryWidgetClass"),
	TEXT("DeathWidgetClass"), TEXT("PlayerStatsWidgetClass"), TEXT("QuestTrackerWidgetClass"),
	TEXT("InteractPromptWidgetClass"), TEXT("CorpseLootWidgetClass"),
	TEXT("DailyRewardWidgetClass"), TEXT("ConsentWidgetClass"), TEXT("OnboardingHintWidgetClass"),
	TEXT("LimpIndicatorWidgetClass"), TEXT("MockAdWidgetClass"), TEXT("IntroObjectiveWidgetClass"),
};

// Загрузить BP по пути и вернуть CDO его generated-класса (nullptr при любой беде, лог пишется).
static UObject* LoadBpCdo(const TCHAR* BlueprintPath, UClass* ExpectedBase, UBlueprint*& OutBP)
{
	OutBP = LoadObject<UBlueprint>(nullptr, BlueprintPath);
	if (!OutBP || !OutBP->GeneratedClass)
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("HUDSLOT: %s не загрузился (или без generated-класса)."),
			BlueprintPath);
		return nullptr;
	}
	if (!OutBP->GeneratedClass->IsChildOf(ExpectedBase))
	{
		UE_LOG(LogGenerateWbp, Error, TEXT("HUDSLOT: %s — родитель не %s (%s)."),
			BlueprintPath, *ExpectedBase->GetName(),
			*OutBP->GeneratedClass->GetSuperClass()->GetPathName());
		return nullptr;
	}
	return OutBP->GeneratedClass->GetDefaultObject();
}

// Печать всех слотов UMG-классов HUD (стабильный формат — срезы «до/после» сравниваются глазами).
static void DumpHudWidgetSlots(const TCHAR* Context, UObject* HudCdo)
{
	for (const TCHAR* SlotName : GHudWidgetSlotNames)
	{
		const FClassProperty* Prop = FindFProperty<FClassProperty>(HudCdo->GetClass(), SlotName);
		if (!Prop)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("HUDSLOT %s: свойства '%s' в классе HUD нет — список слотов разошёлся с ContrarySurvivorHUD.h."),
				Context, SlotName);
			continue;
		}
		const UObject* Value = Prop->GetObjectPropertyValue_InContainer(HudCdo);
		UE_LOG(LogGenerateWbp, Display, TEXT("HUDSLOT %s: %s = %s"),
			Context, SlotName, Value ? *Value->GetPathName() : TEXT("(пусто)"));
	}
}

int32 UGenerateWbpCommandlet::UnlockButtonCaptions()
{
	// ТЗ Рината 08-07 п.3 («Размер текста в кнопках не получается поменять. Проверял в
	// WBP_Dialog и WBP_CorpseLoot… Проверь кнопки в других WBP. Сделай в них текст
	// редактируемым тоже»): в СУЩЕСТВУЮЩИХ ассетах подписи кнопок замкнуты замком
	// дизайнера, а замкнутому виджету редактор блокирует панель «Детали»
	// (SWidgetDetailsView.cpp:375-393) — шрифт было негде править. Проход по всем ассетам
	// таблицы: у каждого текста внутри кнопки снимается замок (см. UnlockTextBlocksInSubtree
	// и поправку 08-07 в шапке раздела замков). Идемпотентно: разомкнутое не трогается,
	// ассет сохраняется только при реальном изменении. Дерево и геометрия не меняются
	// вовсе — пруф равенства раскладки даёт дифф -dumpslots до/после.
	int32 FailCount = 0;
	int32 SavedCount = 0;
	for (const FWbpSpec& Spec : GAssets)
	{
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPathOf(Spec));
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("UNLOCK: %s не загрузился."), *ObjectPathOf(Spec));
			++FailCount;
			continue;
		}

		bool bChanged = false;
		TArray<UWidget*> AllWidgets;
		WBP->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (UButton* Button = Cast<UButton>(Widget))
			{
				for (int32 Index = 0; Index < Button->GetChildrenCount(); ++Index)
				{
					UnlockTextBlocksInSubtree(Button->GetChildAt(Index), Spec.AssetName, &bChanged);
				}
			}
		}

		if (!bChanged)
		{
			UE_LOG(LogGenerateWbp, Display, TEXT("UNLOCK SKIP: %s — замкнутых подписей кнопок нет."),
				Spec.AssetName);
			continue;
		}

		WBP->Modify();
		FKismetEditorUtilities::CompileBlueprint(WBP);
		if (WBP->Status == BS_Error)
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("UNLOCK: %s скомпилировался с ошибками — НЕ сохраняю (ассет на диске цел)."),
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
			UE_LOG(LogGenerateWbp, Error, TEXT("UNLOCK: SavePackage не сохранил %s."), *Filename);
			++FailCount;
			continue;
		}
		++SavedCount;
		UE_LOG(LogGenerateWbp, Display, TEXT("UNLOCK OK: %s сохранён."), Spec.AssetName);
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("UNLOCK ИТОГ: сохранено %d, ошибок %d, всего ассетов %d."),
		SavedCount, FailCount, static_cast<int32>(UE_ARRAY_COUNT(GAssets)));
	return FailCount > 0 ? 1 : 0;
}

int32 UGenerateWbpCommandlet::UnlockAllDesignerLocks()
{
	// П.0 отчёта Рината 23.08 (ADR-077): снять ВСЕ замки дизайнера везде. Прежняя схема
	// намеренно замыкала начинку кнопок/рядов (клик проваливался к кнопке — её удобно
	// таскать), но ценой стала невозможность выделить и настроить начинку — ровно то, за
	// что Ринат разнёс паузу и плитку («НЕ МОГУ ДВИГАТЬ КНОПКИ… ПЛАШКИ, ТЕКСТ В НИХ»).
	// Новая правда: выделяется и правится ВСЁ; геометрия ассетов не меняется вовсе.
	int32 FailCount = 0;
	int32 SavedCount = 0;
	for (const FWbpSpec& Spec : GAssets)
	{
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPathOf(Spec));
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("UNLOCKALL: %s не загрузился."), *ObjectPathOf(Spec));
			++FailCount;
			continue;
		}

		int32 UnlockedCount = 0;
		TArray<UWidget*> AllWidgets;
		WBP->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->IsLockedInDesigner())
			{
				Widget->SetLockedInDesigner(false);
				++UnlockedCount;
			}
		}

		if (UnlockedCount == 0)
		{
			UE_LOG(LogGenerateWbp, Display, TEXT("UNLOCKALL SKIP: %s — замков нет."), Spec.AssetName);
			continue;
		}

		WBP->Modify();
		FKismetEditorUtilities::CompileBlueprint(WBP);
		if (WBP->Status == BS_Error)
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("UNLOCKALL: %s скомпилировался с ошибками — НЕ сохраняю (ассет на диске цел)."),
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
			UE_LOG(LogGenerateWbp, Error, TEXT("UNLOCKALL: SavePackage не сохранил %s."), *Filename);
			++FailCount;
			continue;
		}
		++SavedCount;
		UE_LOG(LogGenerateWbp, Display, TEXT("UNLOCKALL OK: %s — снято замков %d, сохранён."),
			Spec.AssetName, UnlockedCount);
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("UNLOCKALL ИТОГ: сохранено %d, ошибок %d, всего ассетов %d."),
		SavedCount, FailCount, static_cast<int32>(UE_ARRAY_COUNT(GAssets)));
	return FailCount > 0 ? 1 : 0;
}

// Мёртвые кубики: ассет -> имена, которых в нём быть больше не должно. Заполняется РУКАМИ,
// когда элемент выпилен из кода: пока он лежит в живом ассете, он мешает владельцу в
// дизайнере и ломает проверку раскладки, даже будучи невидимым.
struct FDeadWidgetSpec
{
	const TCHAR* PackageName;
	const TCHAR* AssetName;
	std::initializer_list<const TCHAR*> WidgetNames;
};

static const FDeadWidgetSpec GDeadWidgets[] =
{
	// Переключатель согласия убран из паузы решением Рината 08-08 (согласие спрашивается
	// только на экране согласия). Кубики остались в ассете невидимыми и легли ровно под
	// новой кнопкой «В главное меню» — проверка раскладки 08-09 это и поймала.
	{ TEXT("/Game/UI/WBP_PauseMenu"), TEXT("WBP_PauseMenu"),
		{ TEXT("ConsentButton"), TEXT("ConsentText") } },
};

int32 UGenerateWbpCommandlet::DropDeadWidgets()
{
	int32 FailCount = 0;
	int32 RemovedTotal = 0;
	int32 SavedCount = 0;

	for (const FDeadWidgetSpec& Spec : GDeadWidgets)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), Spec.PackageName, Spec.AssetName);
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
		if (!WBP || !WBP->WidgetTree)
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("DROPDEAD: %s не загрузился."), Spec.AssetName);
			++FailCount;
			continue;
		}

		bool bChanged = false;
		WBP->Modify();
		for (const TCHAR* WidgetName : Spec.WidgetNames)
		{
			UWidget* Dead = WBP->WidgetTree->FindWidget(FName(WidgetName));
			if (!Dead)
			{
				// Уже вычищено — режим идемпотентный, повторный прогон просто молчит.
				UE_LOG(LogGenerateWbp, Display, TEXT("DROPDEAD SKIP: %s — '%s' в ассете уже нет."),
					Spec.AssetName, WidgetName);
				continue;
			}
			// RemoveWidget убирает кубик из дерева вместе с его слотом; соседей и их
			// стилизацию не трогает.
			if (WBP->WidgetTree->RemoveWidget(Dead))
			{
				UE_LOG(LogGenerateWbp, Display, TEXT("DROPDEAD: %s — '%s' удалён."),
					Spec.AssetName, WidgetName);
				bChanged = true;
				++RemovedTotal;
			}
			else
			{
				UE_LOG(LogGenerateWbp, Error, TEXT("DROPDEAD: %s — '%s' не удалось удалить."),
					Spec.AssetName, WidgetName);
				++FailCount;
			}
		}

		if (!bChanged)
		{
			continue;
		}

		FKismetEditorUtilities::CompileBlueprint(WBP);
		if (WBP->Status == BS_Error)
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("DROPDEAD: %s скомпилировался с ошибками — НЕ сохраняю (ассет на диске цел)."),
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
			UE_LOG(LogGenerateWbp, Error, TEXT("DROPDEAD: %s не сохранился (%s)."), Spec.AssetName, *Filename);
			++FailCount;
			continue;
		}
		++SavedCount;
		UE_LOG(LogGenerateWbp, Display, TEXT("DROPDEAD OK: %s сохранён."), Spec.AssetName);
	}

	UE_LOG(LogGenerateWbp, Display,
		TEXT("DROPDEAD ИТОГ: удалено кубиков %d, сохранено ассетов %d, ошибок %d."),
		RemovedTotal, SavedCount, FailCount);
	return FailCount > 0 ? 1 : 0;
}

int32 UGenerateWbpCommandlet::FixHudWidgetSlots()
{
	// ТЗ Рината 08-07: заполнить ПУСТЫЕ слоты UMG-классов по таблице GCdoSlots (началось с
	// CorpseLootWidgetClass п.1 — пустой слот незаметен: окно молча живёт на кодовом
	// запасном дереве, и правки владельца в WBP не видны; теперь так же подключаются все
	// окна волны 08-07). Правка тем же путём, каким GenerateOne пишет класс плитки на CDO
	// окна (правка CDO + SavePackage). Идемпотентно: непустой слот НЕ трогается (выбор
	// владельца), правка только «пусто -> WBP_..._C». Слот, ещё не объявленный в C++
	// (перенос правки чужого файла на следующую волну), — Warning и пропуск, не ошибка.
	int32 Errors = 0;

	// Оба блюпринта грузим по одному разу, копим факт изменения — SavePackage один на ассет.
	struct FBpBatch
	{
		UBlueprint* BP = nullptr;
		UObject* Cdo = nullptr;
		bool bChanged = false;
		const TCHAR* Package = nullptr;
	};
	TMap<FString, FBpBatch> Batches;

	for (const FCdoSlotSpec& SlotSpec : GCdoSlots)
	{
		FBpBatch* Batch = Batches.Find(SlotSpec.BlueprintPath);
		if (!Batch)
		{
			FBpBatch NewBatch;
			NewBatch.Package = SlotSpec.BlueprintPackage;
			NewBatch.Cdo = LoadBpCdo(SlotSpec.BlueprintPath, SlotSpec.BaseClass(), NewBatch.BP);
			Batch = &Batches.Add(SlotSpec.BlueprintPath, NewBatch);
			// Печать-пруф всех слотов — только для HUD (список имён GHudWidgetSlotNames его);
			// у контроллера слоты печатаются поштучно по ходу цикла.
			if (Batch->Cdo && FCString::Strcmp(SlotSpec.BlueprintPath, GHudBlueprintPath) == 0)
			{
				DumpHudWidgetSlots(TEXT("до"), Batch->Cdo);
			}
		}
		if (!Batch->Cdo)
		{
			++Errors;
			continue;
		}

		FClassProperty* SlotProp = FindFProperty<FClassProperty>(Batch->Cdo->GetClass(),
			SlotSpec.SlotName);
		if (!SlotProp)
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("HUDSLOT SKIP: свойства %s в %s ещё нет (слот не объявлен в C++) — окно живёт на кодовом фолбэке."),
				SlotSpec.SlotName, SlotSpec.BlueprintPath);
			continue;
		}
		if (SlotProp->GetObjectPropertyValue_InContainer(Batch->Cdo))
		{
			UE_LOG(LogGenerateWbp, Display,
				TEXT("HUDSLOT SKIP: %s уже заполнен — не трогаю (выбор владельца)."),
				SlotSpec.SlotName);
			continue;
		}

		UClass* WidgetClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr,
			SlotSpec.WidgetClassPath);
		if (!WidgetClass)
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("HUDSLOT: класс %s не загрузился — слот %s не заполнен."),
				SlotSpec.WidgetClassPath, SlotSpec.SlotName);
			++Errors;
			continue;
		}
		if (SlotProp->MetaClass && !WidgetClass->IsChildOf(SlotProp->MetaClass))
		{
			UE_LOG(LogGenerateWbp, Error,
				TEXT("HUDSLOT: %s не наследует %s — слот %s не заполнен."),
				SlotSpec.WidgetClassPath, *SlotProp->MetaClass->GetPathName(), SlotSpec.SlotName);
			++Errors;
			continue;
		}

		Batch->BP->Modify();
		SlotProp->SetObjectPropertyValue_InContainer(Batch->Cdo, WidgetClass);
		Batch->bChanged = true;
		UE_LOG(LogGenerateWbp, Display, TEXT("HUDSLOT OK: %s = %s."),
			SlotSpec.SlotName, *WidgetClass->GetPathName());
	}

	for (TPair<FString, FBpBatch>& Pair : Batches)
	{
		FBpBatch& Batch = Pair.Value;
		if (!Batch.Cdo || !Batch.bChanged)
		{
			continue;
		}
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Batch.Package, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Batch.BP->GetOutermost(), Batch.BP, *Filename, SaveArgs))
		{
			UE_LOG(LogGenerateWbp, Error, TEXT("HUDSLOT: SavePackage не сохранил %s."), *Filename);
			++Errors;
			continue;
		}
		if (Pair.Key == GHudBlueprintPath)
		{
			DumpHudWidgetSlots(TEXT("после"), Batch.Cdo);
		}
		UE_LOG(LogGenerateWbp, Display, TEXT("HUDSLOT: ассет сохранён (%s)."), *Filename);
	}

	UE_LOG(LogGenerateWbp, Display, TEXT("HUDSLOT ИТОГ: ошибок %d из %d слотов таблицы."),
		Errors, static_cast<int32>(UE_ARRAY_COUNT(GCdoSlots)));
	return Errors > 0 ? 1 : 0;
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
		// ⛔ Окно отдано владельцу: его не перезаписывает даже -force. Создать с нуля можно —
		// это как раз случай «файла нет вовсе», ради него исключение и оставлено.
		if (bForce && Spec.bOwnerOwned && FPackageName::DoesPackageExist(Spec.PackageName))
		{
			UE_LOG(LogGenerateWbp, Warning,
				TEXT("SKIP: %s отдан владельцу — не перезаписываю даже с -force (ручная настройка Рината)."),
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
			UWidget* Found = WBP->WidgetTree ? WBP->WidgetTree->FindWidget(FName(Cube)) : nullptr;
			if (!Found)
			{
				UE_LOG(LogGenerateWbp, Error, TEXT("VERIFY FAIL: %s — кубик %s не найден. [%s: missing]"),
					Spec.AssetName, Cube, Cube);
				bOk = false;
				continue;
			}
		}

		// --- Окно отдано владельцу: он правит раскладку мышкой. Проверяем только СОСТАВ
		// (выше) и то, что реально мешает эту раскладку править. Положение и размеры не наше
		// дело — иначе прогон ругался бы на собственную настройку Рината.
		//
		// ⚠ ЧТО ИМЕННО МЕШАЕТ ВЫБРАТЬ ЭЛЕМЕНТ МЫШКОЙ (сверено с движком, а не по здравому
		// смыслу): ЗАМОК дизайнера и «глазик» — только они выкидывают виджет из сетки выделения
		// (наш же разбор SDesignerView выше в этом файле). Признак «заведён переменной»
		// (bIsVariable) к выделению отношения НЕ имеет: редактор берёт его лишь для того, чтобы
		// создать переменную-член для графа (WidgetBlueprintCompiler.cpp:598-620), и в коде
		// дизайнера не встречается вовсе. Поэтому замок — ошибка, а «не переменная» — всего лишь
		// замечание: настроить такой элемент мышкой можно, просто из графа к нему не обратиться.
		if (Spec.bOwnerOwned)
		{
			for (const TCHAR* Cube : Spec.ExpectedCubes)
			{
				UWidget* Found = WBP->WidgetTree ? WBP->WidgetTree->FindWidget(FName(Cube)) : nullptr;
				if (!Found)
				{
					continue; // о пропаже уже сказано выше
				}
				if (Found->IsLockedInDesigner())
				{
					UE_LOG(LogGenerateWbp, Error,
						TEXT("VERIFY FAIL: %s — кубик '%s' замкнут в дизайнере: владелец не сможет его выделить и настроить. [%s: locked]"),
						Spec.AssetName, Cube, Cube);
					bOk = false;
				}
				const UPanelWidget* Parent = Found->GetParent();
				if (Parent && !Parent->IsA<UCanvasPanel>())
				{
					UE_LOG(LogGenerateWbp, Warning,
						TEXT("VERIFY %s: '%s' лежит в контейнере %s с автоматической раскладкой — мышкой его не подвинуть. [%s: parent=%s]"),
						Spec.AssetName, Cube, *Parent->GetClass()->GetName(), Cube, *Parent->GetClass()->GetName());
				}
				if (!Found->bIsVariable)
				{
					UE_LOG(LogGenerateWbp, Warning,
						TEXT("VERIFY %s: '%s' не заведён переменной окна — настроить мышкой можно, но из графа к нему не обратиться. [%s: not a variable]"),
						Spec.AssetName, Cube, Cube);
				}
			}
			UE_LOG(LogGenerateWbp, Display,
				TEXT("VERIFY %s: окно отдано владельцу — проверен состав (%d кубиков) и что ни один не замкнут; к положению и размеру не придираемся."),
				Spec.AssetName, static_cast<int32>(Spec.ExpectedCubes.size()));
			FailCount += bOk ? 0 : 1;
			continue;
		}

		// ⛔ РАЗВОРОТ П.0 ADR-077 (23.08): прежний контракт «начинка замкнута» (GLockContracts)
		// ОТМЕНЁН — Ринат требует выделять, двигать и менять ВСЁ («НЕ МОГУ ДВИГАТЬ КНОПКИ…
		// ПЛАШКИ, ТЕКСТ В НИХ» — пауза и плитка не двигались именно из-за замков). Новая
		// проверка противоположная: замков быть не должно НИ НА ОДНОМ виджете ни одного
		// ассета (снимает прогон -unlockall). Таблица GLockContracts оставлена мёртвой
		// историей у своего определения.
		{
			TArray<UWidget*> AllWidgets;
			if (WBP->WidgetTree)
			{
				WBP->WidgetTree->GetAllWidgets(AllWidgets);
			}
			int32 LockedCount = 0;
			for (const UWidget* Widget : AllWidgets)
			{
				if (Widget && Widget->IsLockedInDesigner())
				{
					UE_LOG(LogGenerateWbp, Error,
						TEXT("VERIFY FAIL: %s — виджет '%s' замкнут (bLockedInDesigner) — владелец не сможет выделить и настроить его; прогоните -unlockall (П.0 ADR-077)."),
						Spec.AssetName, *Widget->GetName());
					++LockedCount;
					bOk = false;
				}
			}
			if (LockedCount == 0)
			{
				UE_LOG(LogGenerateWbp, Display,
					TEXT("VERIFY %s: замков дизайнера нет — всё выделяется и двигается (П.0 ADR-077)."),
					Spec.AssetName);
			}
		}

		// Дополнительный контракт окон с сетками плиток (Build 1.2.2): класс плитки
		// WBP_ItemTile_C назначен на CDO окна.
		if (bOk && FCString::Strcmp(Spec.AssetName, TEXT("WBP_Inventory")) == 0)
		{
			bOk = VerifyTileClass<UInventoryScreenWidget>(Spec.AssetName, WBP);
		}
		if (bOk && FCString::Strcmp(Spec.AssetName, TEXT("WBP_Shop")) == 0)
		{
			bOk = VerifyTileClass<UShopScreenWidget>(Spec.AssetName, WBP);
		}
		if (bOk && FCString::Strcmp(Spec.AssetName, TEXT("WBP_SearchWindow")) == 0)
		{
			bOk = VerifyTileClass<UCorpseLootWidget>(Spec.AssetName, WBP);
		}

		// ГЕОМЕТРИЯ (задача лида 08-09). Прежде проверка знала про виджет только имя и класс,
		// поэтому разъехавшийся экран получал такой же зелёный штамп, как здоровый, и брак
		// уехал на телефон. Теперь по каждому элементу в канвас-слоте печатается его
		// прямоугольник, и механически ловятся три беды: нулевой размер, вылезание за
		// подложку и наложение элементов друг на друга.
		if (WBP->WidgetTree)
		{
			if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WBP->WidgetTree->RootWidget))
			{
				const int32 GeometryErrors =
					CheckCanvasGeometry(Spec.AssetName, RootCanvas, GReferenceScreenSize, 1, /*bIsRootCanvas=*/true);
				if (GeometryErrors > 0)
				{
					UE_LOG(LogGenerateWbp, Error,
						TEXT("VERIFY GEO ИТОГ: %s — найдено проблем с раскладкой: %d."),
						Spec.AssetName, GeometryErrors);
					bOk = false;
				}
			}
			else
			{
				// Не канвас в корне — геометрию задаёт контейнер, проверять нечего.
				UE_LOG(LogGenerateWbp, Display,
					TEXT("VERIFY GEO: %s — корень не CanvasPanel, проверка раскладки пропущена."),
					Spec.AssetName);
			}
		}

		if (bOk)
		{
			UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY OK: %s — все %d кубиков на месте, раскладка без наложений."),
				Spec.AssetName, static_cast<int32>(Spec.ExpectedCubes.size()));
		}
		else
		{
			++FailCount;
		}
	}

	// Контракт слотов CDO (08-07, обобщён таблицей GCdoSlots): каждый заявленный слот HUD и
	// контроллера заполнен классом-наследником своего окна. Пустой слот НЕ безобиден: окно
	// молча живёт на кодовом запасном дереве, и правки владельца в WBP не видны (чинится
	// -hudslots). Слот, ещё не объявленный в C++ (перенос правки чужого файла на следующую
	// волну), — Warning, не провал: игра играбельна на кодовом фолбэке.
	{
		bool bHudDumped = false;
		for (const FCdoSlotSpec& SlotSpec : GCdoSlots)
		{
			UBlueprint* BP = nullptr;
			UObject* Cdo = LoadBpCdo(SlotSpec.BlueprintPath, SlotSpec.BaseClass(), BP);
			if (!Cdo)
			{
				++FailCount;
				continue;
			}
			if (!bHudDumped && FCString::Strcmp(SlotSpec.BlueprintPath, GHudBlueprintPath) == 0)
			{
				DumpHudWidgetSlots(TEXT("verify"), Cdo);
				bHudDumped = true;
			}

			const FClassProperty* SlotProp = FindFProperty<FClassProperty>(Cdo->GetClass(),
				SlotSpec.SlotName);
			if (!SlotProp)
			{
				UE_LOG(LogGenerateWbp, Warning,
					TEXT("VERIFY: свойства %s в %s ещё нет (слот не объявлен в C++) — окно живёт на кодовом фолбэке."),
					SlotSpec.SlotName, SlotSpec.BlueprintPath);
				continue;
			}
			const UClass* SlotValue =
				Cast<UClass>(SlotProp->GetObjectPropertyValue_InContainer(Cdo));
			if (!SlotValue)
			{
				UE_LOG(LogGenerateWbp, Error,
					TEXT("VERIFY FAIL: слот %s пуст — окно живёт на кодовом запасном дереве, %s не используется (чинится -hudslots)."),
					SlotSpec.SlotName, SlotSpec.WidgetClassPath);
				++FailCount;
			}
			else if (SlotProp->MetaClass && !SlotValue->IsChildOf(SlotProp->MetaClass))
			{
				UE_LOG(LogGenerateWbp, Error,
					TEXT("VERIFY FAIL: в слоте %s чужой класс %s."),
					SlotSpec.SlotName, *SlotValue->GetPathName());
				++FailCount;
			}
			else
			{
				UE_LOG(LogGenerateWbp, Display, TEXT("VERIFY OK: слот %s = %s."),
					SlotSpec.SlotName, *SlotValue->GetPathName());
			}
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
