// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/TouchControlsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"

namespace
{
	// Круглая кисть без текстур: RoundedBox со скруглением в полвысоты = круг
	// (HalfHeightRadius — дефолтный RoundingType кисти, SlateBrush.h:141).
	FSlateBrush MakeCircleBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}

	// Подсветка активного переключателя БЕГ.
	const FLinearColor SprintActiveTint(1.0f, 0.85f, 0.2f, 1.0f);
}

void UTouchControlsWidget::InitTouch(AContrarySurvivorPlayerController* InController,
	const UInputAction* InMoveAction, const UInputAction* InFireAction,
	const UInputAction* InReloadAction, const UInputAction* InSprintAction,
	const FTouchControlsConfig& InConfig)
{
	OwnerPC = InController;
	MoveActionRef = InMoveAction;
	FireActionRef = InFireAction;
	ReloadActionRef = InReloadAction;
	SprintActionRef = InSprintAction;
	Config = InConfig;

	// Применяем настройки к уже построенному стику (NativeOnInitialized отработал в CreateWidget).
	if (StickBase)
	{
		if (UCanvasPanelSlot* BaseSlot = Cast<UCanvasPanelSlot>(StickBase->Slot))
		{
			BaseSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
			BaseSlot->SetSize(FVector2D(Config.StickRadius * 2.0f, Config.StickRadius * 2.0f));
		}
		StickBase->SetRenderOpacity(Config.IdleOpacity);
	}
	if (StickThumb)
	{
		if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(StickThumb->Slot))
		{
			ThumbSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
			ThumbSlot->SetSize(FVector2D(Config.StickThumbRadius * 2.0f, Config.StickThumbRadius * 2.0f));
		}
		StickThumb->SetRenderOpacity(Config.IdleOpacity);
	}

	// Кнопки строятся здесь, а не в NativeOnInitialized: нужен конфиг (что включено, размеры).
	BuildButtons();
}

void UTouchControlsWidget::SetLayerEnabled(bool bEnabled)
{
	// SelfHitTestInvisible — штатная видимость UUserWidget (UserWidget.cpp:87): сам слой
	// хит-тест не ловит, хит-зоны — только подложка стика и кнопки внутри.
	SetVisibility(bEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bEnabled)
	{
		ResetStick();
		ResetHeldButtons();
	}
}

void UTouchControlsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Дерево целиком из C++ (паттерн окон этапа F): канва на весь экран, на ней стик и кнопки.
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TouchRoot"));
	WidgetTree->RootWidget = RootCanvas;
	// Канва прозрачна для кликов: хит-зоны слоя — стик и кнопки, тапы мимо них уходят
	// в мир (клик-выбор цели / ScreenTap, ADR-017).
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Подложка стика (низ-лево; anchors в угол, позиция = отступ центра, alignment по центру).
	StickBase = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StickBase"));
	StickBase->SetBrush(MakeCircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.25f)));
	StickBase->SetVisibility(ESlateVisibility::Visible); // хит-зона стика
	if (UCanvasPanelSlot* BaseSlot = RootCanvas->AddChildToCanvas(StickBase))
	{
		BaseSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		BaseSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BaseSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
		BaseSlot->SetSize(FVector2D(Config.StickRadius * 2.0f, Config.StickRadius * 2.0f));
	}

	// «Шляпка» стика — чистый визуал, кликов не ловит.
	StickThumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StickThumb"));
	StickThumb->SetBrush(MakeCircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.6f)));
	StickThumb->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* ThumbSlot = RootCanvas->AddChildToCanvas(StickThumb))
	{
		ThumbSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		ThumbSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ThumbSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
		ThumbSlot->SetSize(FVector2D(Config.StickThumbRadius * 2.0f, Config.StickThumbRadius * 2.0f));
	}

	StickBase->SetRenderOpacity(Config.IdleOpacity);
	StickThumb->SetRenderOpacity(Config.IdleOpacity);

	// Стик — часть боевой группы (прячется при модальных окнах, как и боевые кнопки).
	CombatGroupWidgets.Add(StickBase);
	CombatGroupWidgets.Add(StickThumb);
}

void UTouchControlsWidget::BuildButtons()
{
	// Правый-нижний веер под большой палец: ОГОНЬ в углу, ДЕЙСТВИЕ левее, ПЕРЕЗАРЯД выше,
	// БЕГ по диагонали, ОРУЖИЕ над перезарядкой. СУМКА — правый-верх, ПАУЗА — левый-верх.
	// Точные позиции Ринат тюнит EditAnywhere-полями контроллера после живой пробы.
	FireButton = MakeTouchButton(Config.FireButton, ETouchCorner::BottomRight,
		TEXT("ОГОНЬ"), TEXT("TouchFire"), /*bCombatGroup=*/true);
	if (FireButton)
	{
		FireButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleFirePressed);
		FireButton->OnReleased.AddDynamic(this, &UTouchControlsWidget::HandleFireReleased);
	}

	if (UButton* ReloadBtn = MakeTouchButton(Config.ReloadButton, ETouchCorner::BottomRight,
		TEXT("ПЕРЕЗАРЯД"), TEXT("TouchReload"), /*bCombatGroup=*/true))
	{
		ReloadBtn->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleReloadPressed);
	}

	if (UButton* InteractBtn = MakeTouchButton(Config.InteractButton, ETouchCorner::BottomRight,
		TEXT("ДЕЙСТВИЕ"), TEXT("TouchInteract"), /*bCombatGroup=*/true))
	{
		InteractBtn->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleInteractPressed);
	}

	SprintButton = MakeTouchButton(Config.SprintButton, ETouchCorner::BottomRight,
		TEXT("БЕГ"), TEXT("TouchSprint"), /*bCombatGroup=*/true);
	if (SprintButton)
	{
		SprintButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleSprintPressed);
		SprintButton->OnReleased.AddDynamic(this, &UTouchControlsWidget::HandleSprintReleased);
	}

	if (UButton* WeaponBtn = MakeTouchButton(Config.WeaponButton, ETouchCorner::BottomRight,
		TEXT("ОРУЖИЕ"), TEXT("TouchWeapon"), /*bCombatGroup=*/true))
	{
		WeaponBtn->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleWeaponPressed);
	}

	if (UButton* InventoryBtn = MakeTouchButton(Config.InventoryButton, ETouchCorner::TopRight,
		TEXT("СУМКА"), TEXT("TouchInventory"), /*bCombatGroup=*/false))
	{
		InventoryBtn->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleInventoryPressed);
	}

	if (UButton* PauseBtn = MakeTouchButton(Config.PauseButton, ETouchCorner::TopLeft,
		TEXT("II"), TEXT("TouchPause"), /*bCombatGroup=*/false))
	{
		PauseBtn->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandlePausePressed);
	}
}

UButton* UTouchControlsWidget::MakeTouchButton(const FTouchButtonSettings& S, ETouchCorner Corner,
	const FString& Label, const FName& WidgetName, bool bCombatGroup)
{
	if (!S.bEnabled || !RootCanvas)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);

	// Круглый стиль без текстур (та же кисть, что у стика); состояния — прозрачностью тона.
	FButtonStyle Style;
	Style.Normal   = MakeCircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.30f));
	Style.Hovered  = MakeCircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.40f));
	Style.Pressed  = MakeCircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.55f));
	Style.Disabled = MakeCircleBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.15f));
	Style.NormalPadding = FMargin(0.0f);
	Style.PressedPadding = FMargin(0.0f);
	Button->SetStyle(Style);
	Button->SetRenderOpacity(Config.IdleOpacity);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(WidgetName.ToString() + TEXT("Label"))));
	Text->SetText(FText::FromString(Label));
	const int32 FontSize = FMath::Clamp<int32>(FMath::RoundToInt(S.Radius * 0.30f), 10, 22);
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f)));
	Button->SetContent(Text);

	if (UCanvasPanelSlot* BtnSlot = RootCanvas->AddChildToCanvas(Button))
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

	if (bCombatGroup)
	{
		CombatGroupWidgets.Add(Button);
	}
	return Button;
}

void UTouchControlsWidget::SetCombatGroupVisible(bool bVisible)
{
	bCombatGroupVisible = bVisible;
	for (UWidget* GroupWidget : CombatGroupWidgets)
	{
		if (GroupWidget)
		{
			GroupWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	// «Шляпка» стика — чистый визуал: вернуть ей не-хит-тест видимость после общего Visible.
	if (bVisible && StickThumb)
	{
		StickThumb->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (!bVisible)
	{
		ResetStick();
		ResetHeldButtons();
	}
}

void UTouchControlsWidget::ResetHeldButtons()
{
	bFireHeld = false;
	bReloadQueued = false;
	bSprintOn = false;
	if (SprintButton)
	{
		SprintButton->SetBackgroundColor(FLinearColor::White);
		SprintButton->SetRenderOpacity(Config.IdleOpacity);
	}
}

UEnhancedInputLocalPlayerSubsystem* UTouchControlsWidget::GetInputSubsystem() const
{
	if (!OwnerPC)
	{
		return nullptr;
	}
	if (ULocalPlayer* LocalPlayer = OwnerPC->GetLocalPlayer())
	{
		return LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
	return nullptr;
}

void UTouchControlsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!OwnerPC)
	{
		return;
	}

	// Модальное окно открыто -> боевая группа прячется, инжекция глушится (см. класс-коммент).
	// СУМКА/ПАУЗА остаются: их обработчики модалкам не вредят (тогл инвентаря / гейт паузы).
	const bool bModal = OwnerPC->IsAnyModalUIOpen();
	if (bModal == bCombatGroupVisible)
	{
		SetCombatGroupVisible(!bModal);
	}
	if (bModal)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetInputSubsystem();
	if (!InputSubsystem)
	{
		return;
	}

	// Инжекция действует один кадр обработки ввода — повторяем, пока элемент активен.
	// Путь тот же, что у физических клавиш (WASD/ЛКМ/Shift/R): явных триггеров в IA-ассетах
	// нет (проверено содержимым ассетов), значит Triggered летит каждый кадр ненулевого
	// значения, Completed — на обнулении.
	if (bStickActive && !StickVector.IsNearlyZero() && MoveActionRef)
	{
		InputSubsystem->InjectInputVectorForAction(MoveActionRef,
			FVector(StickVector.X, StickVector.Y, 0.0), /*Modifiers=*/{}, /*Triggers=*/{});
	}
	if (bFireHeld && FireActionRef)
	{
		// Удержание кнопки ОГОНЬ = автоогонь, как зажатая ЛКМ на ПК (кулдаун — в оружии).
		InputSubsystem->InjectInputForAction(FireActionRef, FInputActionValue(true),
			/*Modifiers=*/{}, /*Triggers=*/{});
	}
	if (bSprintOn && SprintActionRef)
	{
		InputSubsystem->InjectInputForAction(SprintActionRef, FInputActionValue(true),
			/*Modifiers=*/{}, /*Triggers=*/{});
	}
	if (bReloadQueued)
	{
		if (ReloadActionRef)
		{
			// Один кадр true -> один Triggered -> одна перезарядка.
			InputSubsystem->InjectInputForAction(ReloadActionRef, FInputActionValue(true),
				/*Modifiers=*/{}, /*Triggers=*/{});
		}
		bReloadQueued = false;
	}
}

// --- Обработчики кнопок ---

void UTouchControlsWidget::HandleFirePressed()
{
	bFireHeld = true;
}

void UTouchControlsWidget::HandleFireReleased()
{
	// Прекращаем инжекцию -> значение IA_Fire падает в 0 -> Completed -> OnFireReleased
	// контроллера сбрасывает edge-флаг UI-клика (тот же путь, что отпускание ЛКМ).
	bFireHeld = false;
}

void UTouchControlsWidget::HandleReloadPressed()
{
	bReloadQueued = true;
}

void UTouchControlsWidget::HandleSprintPressed()
{
	// Переключатель (дефолт) или удержание — решает bTouchSprintToggle контроллера.
	bSprintOn = Config.bSprintToggle ? !bSprintOn : true;
	if (SprintButton)
	{
		SprintButton->SetBackgroundColor(bSprintOn ? SprintActiveTint : FLinearColor::White);
		SprintButton->SetRenderOpacity(bSprintOn ? Config.ActiveOpacity : Config.IdleOpacity);
	}
}

void UTouchControlsWidget::HandleSprintReleased()
{
	if (Config.bSprintToggle)
	{
		return; // в режиме переключателя отпускание ничего не меняет
	}
	bSprintOn = false;
	if (SprintButton)
	{
		SprintButton->SetBackgroundColor(FLinearColor::White);
		SprintButton->SetRenderOpacity(Config.IdleOpacity);
	}
}

void UTouchControlsWidget::HandleInteractPressed()
{
	if (OwnerPC)
	{
		OwnerPC->TouchInteract();
	}
}

void UTouchControlsWidget::HandleWeaponPressed()
{
	if (OwnerPC)
	{
		OwnerPC->TouchSwitchWeapon();
	}
}

void UTouchControlsWidget::HandleInventoryPressed()
{
	if (OwnerPC)
	{
		OwnerPC->TouchToggleInventory();
	}
}

void UTouchControlsWidget::HandlePausePressed()
{
	if (OwnerPC)
	{
		OwnerPC->TouchTogglePauseMenu();
	}
}

// --- Стик ---

FVector2D UTouchControlsWidget::GetStickCenterLocal(const FGeometry& Geo) const
{
	// Anchors (0,1) = левый-нижний угол; центр стика — на StickMargin от него.
	const FVector2D LocalSize = Geo.GetLocalSize();
	return FVector2D(Config.StickMargin.X, LocalSize.Y - Config.StickMargin.Y);
}

void UTouchControlsWidget::UpdateStickFromPointer(const FGeometry& Geo, const FPointerEvent& Ev)
{
	const FVector2D LocalPos = Geo.AbsoluteToLocal(Ev.GetScreenSpacePosition());
	FVector2D Offset = LocalPos - GetStickCenterLocal(Geo);

	// Ограничиваем ход «шляпки» радиусом подложки.
	const float Len = Offset.Size();
	if (Len > Config.StickRadius && Len > KINDA_SMALL_NUMBER)
	{
		Offset *= Config.StickRadius / Len;
	}

	// Экранный Y растёт вниз, а ось Y MoveAction — «вперёд» (Move: MoveDir = Forward*Y + Right*X).
	FVector2D NewVector(Offset.X / Config.StickRadius, -Offset.Y / Config.StickRadius);
	if (NewVector.Size() < Config.StickDeadZone)
	{
		NewVector = FVector2D::ZeroVector;
	}
	StickVector = NewVector;

	if (StickThumb)
	{
		if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(StickThumb->Slot))
		{
			ThumbSlot->SetPosition(FVector2D(
				Config.StickMargin.X + Offset.X, -Config.StickMargin.Y + Offset.Y));
		}
	}
}

void UTouchControlsWidget::ResetStick()
{
	bStickActive = false;
	StickPointerIndex = INDEX_NONE;
	StickVector = FVector2D::ZeroVector;
	if (StickThumb)
	{
		if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(StickThumb->Slot))
		{
			ThumbSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
		}
		StickThumb->SetRenderOpacity(Config.IdleOpacity);
	}
	if (StickBase)
	{
		StickBase->SetRenderOpacity(Config.IdleOpacity);
	}
}

FReply UTouchControlsWidget::HandlePointerDown(const FGeometry& Geo, const FPointerEvent& Ev)
{
	// Событие пришло всплытием => хит-тест попал в подложку стика (кнопки свои события
	// обрабатывают сами и сюда не пропускают).
	if (bStickActive)
	{
		return FReply::Unhandled(); // стик уже держит другой палец
	}
	bStickActive = true;
	StickPointerIndex = Ev.GetPointerIndex();
	UpdateStickFromPointer(Geo, Ev);
	if (StickBase)  { StickBase->SetRenderOpacity(Config.ActiveOpacity); }
	if (StickThumb) { StickThumb->SetRenderOpacity(Config.ActiveOpacity); }
	// Захват указателя: движения приходят и за пределами подложки, пока палец/кнопка не отпущены.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UTouchControlsWidget::HandlePointerMove(const FGeometry& Geo, const FPointerEvent& Ev)
{
	if (!bStickActive || Ev.GetPointerIndex() != StickPointerIndex)
	{
		return FReply::Unhandled();
	}
	UpdateStickFromPointer(Geo, Ev);
	return FReply::Handled();
}

FReply UTouchControlsWidget::HandlePointerUp(const FGeometry& Geo, const FPointerEvent& Ev)
{
	if (!bStickActive || Ev.GetPointerIndex() != StickPointerIndex)
	{
		return FReply::Unhandled();
	}
	ResetStick();
	return FReply::Handled().ReleaseMouseCapture();
}

// --- Мышь и тач — единый путь (ADR-017: клик = имитация тапа) ---

FReply UTouchControlsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return HandlePointerDown(InGeometry, InMouseEvent);
}

FReply UTouchControlsWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return HandlePointerMove(InGeometry, InMouseEvent);
}

FReply UTouchControlsWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return HandlePointerUp(InGeometry, InMouseEvent);
}

FReply UTouchControlsWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return HandlePointerDown(InGeometry, InGestureEvent);
}

FReply UTouchControlsWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return HandlePointerMove(InGeometry, InGestureEvent);
}

FReply UTouchControlsWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	return HandlePointerUp(InGeometry, InGestureEvent);
}
