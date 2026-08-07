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
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // GetCurrentWeapon (иконка оружия)
#include "ContrarySurvivor/Characters/PlayerCharacter.h" // GetRangedWeaponInstance (защитный гейт иконки)
#include "ContrarySurvivor/UI/WeaponUiSyncLog.h" // Warning рассинхрона — один раз при входе
#include "ARangedWeapon.h"   // пистолет/нож различаются классом оружия
#include "Engine/Texture2D.h"
#include "ContrarySurvivor/Utils/ContrarySurvivorStatics.h" // GetCurrentFPS (Блок E)
#include "ContrarySurvivor/Debug/QADebug.h" // CONTRARY_WITH_QA_CHEATS: экранной отладки нет в Shipping

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

	if (bDesignerTree)
	{
		// Дерево Рината: стиль/раскладка целиком в WBP, код кубики не перекрашивает.
		// Недостающие имена — предупреждение (элемент не работает, остальное живёт).
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ StickBase, TEXT("StickBase") }, { StickThumb, TEXT("StickThumb") },
			{ FireButton, TEXT("FireButton") }, { ReloadButton, TEXT("ReloadButton") },
			{ InteractButton, TEXT("InteractButton") }, { SprintButton, TEXT("SprintButton") },
			{ WeaponButton, TEXT("WeaponButton") }, { InventoryButton, TEXT("InventoryButton") },
			{ PauseButton, TEXT("PauseButton") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("TouchControlsWidget: кубик %s не найден в WBP_TouchControls — элемент отключён"),
					Entry.Name);
			}
		}
		// Цвет покоя переключателя БЕГ — тот, что выставил Ринат (не жёсткий белый).
		SprintIdleColor = SprintButton ? SprintButton->GetBackgroundColor() : FLinearColor::White;

		// Кубика иконки оружия может не быть в старом WBP (добавлен 07-19): fallback —
		// создаём кодом в корневую канву ассета. Двигать мышкой Ринат сможет после
		// добавления кубика в ассет (коммандлет -augment), логика работает уже сейчас.
		if (!WeaponIconImage)
		{
			CreateWeaponIconInCanvas(Cast<UCanvasPanel>(WidgetTree ? WidgetTree->RootWidget : nullptr));
			UE_LOG(LogQA, Warning,
				TEXT("TouchControlsWidget: кубик WeaponIconImage не найден в WBP — %s"),
				WeaponIconImage ? TEXT("создан кодом (позиция дефолтная)") : TEXT("корень не канва, иконка отключена"));
		}
		// Число кадров: нет кубика в WBP — создаём кодом (позиция дефолтная), логика работает.
		if (!FpsText)
		{
			CreateFpsTextInCanvas(Cast<UCanvasPanel>(WidgetTree ? WidgetTree->RootWidget : nullptr));
		}
		// Строка времён кадра — по тому же правилу.
		if (!FrameTimeText)
		{
			CreateFrameTimeTextInCanvas(Cast<UCanvasPanel>(WidgetTree ? WidgetTree->RootWidget : nullptr));
		}
	}
	else
	{
		// Кодовое дерево: применяем настройки контроллера к построенному в NativeOnInitialized
		// стику (там были дефолты конфига) и строим кнопки — конфиг уже известен.
		SprintIdleColor = FLinearColor::White;
		if (StickBase)
		{
			if (UCanvasPanelSlot* BaseSlot = Cast<UCanvasPanelSlot>(StickBase->Slot))
			{
				BaseSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
				BaseSlot->SetSize(FVector2D(Config.StickRadius * 2.0f, Config.StickRadius * 2.0f));
			}
			StickBase->SetBrush(MakeCircleBrush(Config.StickBaseColor));
			StickBase->SetRenderOpacity(Config.IdleOpacity);
		}
		if (StickThumb)
		{
			if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(StickThumb->Slot))
			{
				ThumbSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
				ThumbSlot->SetSize(FVector2D(Config.StickThumbRadius * 2.0f, Config.StickThumbRadius * 2.0f));
			}
			StickThumb->SetBrush(MakeCircleBrush(Config.StickThumbColor));
			StickThumb->SetRenderOpacity(Config.IdleOpacity);
		}
		BuildButtons();
	}

	BindButtonHandlers();
	CollectCombatGroup();
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

	// WBP-наследник уже пришёл с деревом Рината (создано из ассета ДО этого вызова) —
	// кубики привязаны BindWidgetOptional, строить ничего не нужно.
	bDesignerTree = (WidgetTree && WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		return;
	}

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

	// Число кадров рядом с ПАУЗА (Блок E): создаём кубик в кодовом дереве (позиция/стиль из полей).
	CreateFpsTextInCanvas(RootCanvas);
	// Под ним — строка времён кадра (замер «во что упираемся», задача лида 05-08).
	CreateFrameTimeTextInCanvas(RootCanvas);
}

void UTouchControlsWidget::BuildButtons()
{
	// Правый-нижний веер под большой палец: ОГОНЬ в углу, ДЕЙСТВИЕ левее, ПЕРЕЗАРЯД выше,
	// БЕГ по диагонали, ОРУЖИЕ над перезарядкой. СУМКА — правый-верх, ПАУЗА — левый-верх.
	// Позиции/подписи/цвета Ринат тюнит EditAnywhere-полями контроллера (дефолты подписей —
	// его конструктор), виджет только строит по конфигу. Имена кубиков = именам в WBP-режиме.
	FireButton = MakeTouchButton(Config.FireButton, ETouchCorner::BottomRight,
		TEXT("FireButton"), FireText);
	ReloadButton = MakeTouchButton(Config.ReloadButton, ETouchCorner::BottomRight,
		TEXT("ReloadButton"), ReloadText);
	InteractButton = MakeTouchButton(Config.InteractButton, ETouchCorner::BottomRight,
		TEXT("InteractButton"), InteractText);
	SprintButton = MakeTouchButton(Config.SprintButton, ETouchCorner::BottomRight,
		TEXT("SprintButton"), SprintText);
	WeaponButton = MakeTouchButton(Config.WeaponButton, ETouchCorner::BottomRight,
		TEXT("WeaponButton"), WeaponText);
	InventoryButton = MakeTouchButton(Config.InventoryButton, ETouchCorner::TopRight,
		TEXT("InventoryButton"), InventoryText);
	PauseButton = MakeTouchButton(Config.PauseButton, ETouchCorner::TopLeft,
		TEXT("PauseButton"), PauseText);

	// Иконка текущего оружия — над кнопкой ОРУЖИЕ (кнопка МЕНЯЕТ оружие, иконка показывает,
	// что в руках СЕЙЧАС — рядом читается как пара).
	CreateWeaponIconInCanvas(RootCanvas);
}

void UTouchControlsWidget::CreateWeaponIconInCanvas(UCanvasPanel* Canvas)
{
	if (!Canvas || !WidgetTree || WeaponIconImage)
	{
		return;
	}
	WeaponIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WeaponIconImage"));
	WeaponIconImage->SetVisibility(ESlateVisibility::Collapsed); // покажет UpdateWeaponIcon
	WeaponIconImage->SetRenderOpacity(Config.IdleOpacity);
	if (UCanvasPanelSlot* IconSlot = Canvas->AddChildToCanvas(WeaponIconImage))
	{
		IconSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
		IconSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		IconSlot->SetPosition(FVector2D(-Config.WeaponButton.Margin.X,
			-Config.WeaponButton.Margin.Y - Config.WeaponButton.Radius - 34.0f));
		IconSlot->SetSize(FVector2D(48.0f, 48.0f));
	}
}

void UTouchControlsWidget::UpdateWeaponIcon(bool bForceHide)
{
	if (!WeaponIconImage)
	{
		return;
	}

	// Текущее состояние: пешки нет/оружия нет/модалка -> иконки нет; дальнобой -> пистолет,
	// иначе нож (других типов оружия в игре нет; появятся — расширить состоянием на класс).
	EWeaponIconState NewState = EWeaponIconState::NoWeapon;
	if (!bForceHide && OwnerPC)
	{
		if (const AMasterHumanoidCharacter* Humanoid = Cast<AMasterHumanoidCharacter>(OwnerPC->GetPawn()))
		{
			if (AMasterWeapon* Weapon = Humanoid->GetCurrentWeapon())
			{
				ARangedWeapon* Ranged = Cast<ARangedWeapon>(Weapon);

				// Находка лида 08-05: на устройстве иконка держала «пистолет» при пустом слоте
				// огнестрела в рюкзаке. Корень найден 07-08: легаси-граф BP_PlayerCharacter
				// экипировал пистолет мимо слота (лечится в APlayerCharacter::
				// ReconcileOutOfSlotRangedWeapon). Гейт остаётся защитой в глубину: «в руках»
				// обязано быть ИМЕННО отслеживаемым стволом слота, иначе показываем нож
				// (Weapon в этой ветке точно не null). Warning — один раз при ВХОДЕ в
				// рассинхрон (раньше писался каждый вызов из NativeTick и заспамливал лог:
				// гейт «состояние не изменилось» стоит НИЖЕ этой проверки).
				if (const APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(Humanoid))
				{
					const bool bDesync = (Ranged && Ranged != PlayerChar->GetRangedWeaponInstance());
					if (WeaponUiSyncLog::ShouldLogDesyncOnce(bDesync, bWeaponDesyncLogged))
					{
						UE_LOG(LogQA, Warning,
							TEXT("TouchControlsWidget: CurrentWeapon '%s' is ARangedWeapon, but != RangedWeaponInstance ('%s') — showing knife defensively"),
							*Ranged->GetName(),
							PlayerChar->GetRangedWeaponInstance() ? *PlayerChar->GetRangedWeaponInstance()->GetName() : TEXT("null"));
					}
					if (bDesync)
					{
						Ranged = nullptr;
					}
				}
				NewState = Ranged ? EWeaponIconState::Pistol : EWeaponIconState::Knife;
			}
		}
	}
	if (NewState == WeaponIconState)
	{
		return;
	}
	WeaponIconState = NewState;

	UTexture2D* Icon = nullptr;
	if (NewState == EWeaponIconState::Pistol)
	{
		if (!ResolvedPistolIcon && !PistolIconTexture.IsNull())
		{
			ResolvedPistolIcon = PistolIconTexture.LoadSynchronous(); // один раз, дальше кэш
		}
		Icon = ResolvedPistolIcon;
	}
	else if (NewState == EWeaponIconState::Knife)
	{
		if (!ResolvedKnifeIcon && !KnifeIconTexture.IsNull())
		{
			ResolvedKnifeIcon = KnifeIconTexture.LoadSynchronous();
		}
		Icon = ResolvedKnifeIcon;
	}

	if (Icon)
	{
		WeaponIconImage->SetBrushFromTexture(Icon);
		WeaponIconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // чистый визуал, тапы сквозь
	}
	else
	{
		// Нет оружия ИЛИ текстура не загрузилась (нет ассета) — прячем, не рисуем пустую кисть.
		WeaponIconImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTouchControlsWidget::BindButtonHandlers()
{
	if (FireButton)
	{
		FireButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleFirePressed);
		FireButton->OnReleased.AddDynamic(this, &UTouchControlsWidget::HandleFireReleased);
	}
	if (ReloadButton)
	{
		ReloadButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleReloadPressed);
	}
	if (InteractButton)
	{
		InteractButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleInteractPressed);
	}
	if (SprintButton)
	{
		SprintButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleSprintPressed);
		SprintButton->OnReleased.AddDynamic(this, &UTouchControlsWidget::HandleSprintReleased);
	}
	if (WeaponButton)
	{
		WeaponButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleWeaponPressed);
	}
	if (InventoryButton)
	{
		InventoryButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandleInventoryPressed);
	}
	if (PauseButton)
	{
		PauseButton->OnPressed.AddDynamic(this, &UTouchControlsWidget::HandlePausePressed);
	}
}

void UTouchControlsWidget::CollectCombatGroup()
{
	CombatGroupWidgets.Reset();
	CombatGroupShownVisibility.Reset();
	// СУМКА/ПАУЗА в группу НЕ входят — остаются при модалках (см. класс-коммент).
	UWidget* GroupMembers[] = { StickBase.Get(), StickThumb.Get(), FireButton.Get(),
		ReloadButton.Get(), InteractButton.Get(), SprintButton.Get(), WeaponButton.Get() };
	for (UWidget* Member : GroupMembers)
	{
		if (Member)
		{
			CombatGroupWidgets.Add(Member);
			// Запоминаем «показанную» видимость: у WBP-кубиков — выставленную Ринатом
			// (например SelfHitTestInvisible у «шляпки»), не жёсткое Visible.
			CombatGroupShownVisibility.Add(Member->GetVisibility());
		}
	}
}

UButton* UTouchControlsWidget::MakeTouchButton(const FTouchButtonSettings& S, ETouchCorner Corner,
	const FName& WidgetName, TObjectPtr<UTextBlock>& OutLabel)
{
	if (!S.bEnabled || !RootCanvas)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);

	// Круглый стиль без текстур (та же кисть, что у стика); состояния — прозрачностью тона
	// S.Color (белый с альфой 1 = прежний вид; альфа настройки масштабирует все состояния).
	const FLinearColor& Tint = S.Color;
	FButtonStyle Style;
	Style.Normal   = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.30f));
	Style.Hovered  = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.40f));
	Style.Pressed  = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.55f));
	Style.Disabled = MakeCircleBrush(FLinearColor(Tint.R, Tint.G, Tint.B, Tint.A * 0.15f));
	Style.NormalPadding = FMargin(0.0f);
	Style.PressedPadding = FMargin(0.0f);
	Button->SetStyle(Style);
	Button->SetRenderOpacity(Config.IdleOpacity);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(WidgetName.ToString() + TEXT("Label"))));
	Text->SetText(S.Label);
	const int32 FontSize = (S.FontSize > 0)
		? S.FontSize
		: FMath::Clamp<int32>(FMath::RoundToInt(S.Radius * 0.30f), 10, 22);
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
	Text->SetColorAndOpacity(FSlateColor(S.TextColor));
	Button->SetContent(Text);
	OutLabel = Text;

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

	return Button;
}

void UTouchControlsWidget::SetCombatGroupVisible(bool bVisible)
{
	bCombatGroupVisible = bVisible;
	for (int32 Index = 0; Index < CombatGroupWidgets.Num(); ++Index)
	{
		if (UWidget* GroupWidget = CombatGroupWidgets[Index])
		{
			GroupWidget->SetVisibility(bVisible
				? CombatGroupShownVisibility[Index]
				: ESlateVisibility::Collapsed);
		}
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
	bSprintVisualActive = false; // подсветка бега снята — при новом беге UpdateSprintVisual начнёт заново
	SprintPulseTime = 0.0f;
	if (SprintButton)
	{
		SprintButton->SetBackgroundColor(GetSprintIdleColor());
		if (!bDesignerTree)
		{
			SprintButton->SetRenderOpacity(Config.IdleOpacity);
		}
	}
}

void UTouchControlsWidget::CreateFpsTextInCanvas(UCanvasPanel* Canvas)
{
	// В публикационной сборке счётчик кадров не только не показывается — он и не создаётся
	// (Б5 задания издателя). Прятать созданный кубик недостаточно: надёжнее, когда его нет.
	if (!CONTRARY_WITH_QA_CHEATS)
	{
		return;
	}
	if (!Canvas || !WidgetTree || FpsText)
	{
		return; // нет канвы / уже есть (в т.ч. кубик из WBP)
	}
	FpsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FpsText"));
	FpsText->SetVisibility(ESlateVisibility::HitTestInvisible); // только визуал, кликов не ловит
	FpsText->SetColorAndOpacity(FSlateColor(FpsTextColor));
	{
		FSlateFontInfo Font = FpsText->GetFont();
		Font.Size = FpsFontSize;
		FpsText->SetFont(Font);
	}
	if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(FpsText))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f)); // верх-лево, рядом с ПАУЗА
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetPosition(FpsMargin);
	}
}

void UTouchControlsWidget::CreateFrameTimeTextInCanvas(UCanvasPanel* Canvas)
{
	// Как и счётчик кадров: в публикационной сборке строка времён не создаётся вовсе (Б5).
	if (!CONTRARY_WITH_QA_CHEATS)
	{
		return;
	}
	if (!Canvas || !WidgetTree || FrameTimeText)
	{
		return; // нет канвы / уже есть (в т.ч. кубик из WBP)
	}
	FrameTimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FrameTimeText"));
	FrameTimeText->SetVisibility(ESlateVisibility::Collapsed); // до первого замера показывать нечего
	FrameTimeText->SetColorAndOpacity(FSlateColor(FpsTextColor));
	{
		FSlateFontInfo Font = FrameTimeText->GetFont();
		Font.Size = FrameTimeFontSize;
		FrameTimeText->SetFont(Font);
	}
	if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(FrameTimeText))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f)); // тот же угол, что у числа кадров
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetAutoSize(true);
		// Строкой ниже числа кадров: отступ по высоте кегля числа с небольшим зазором.
		CanvasSlot->SetPosition(FpsMargin + FVector2D(0.0f, static_cast<float>(FpsFontSize) + 6.0f));
	}
}

void UTouchControlsWidget::UpdateFrameTimeText(float DeltaTime)
{
	if (!FrameTimeText)
	{
		return; // кубика нет — нечего обновлять
	}

	// Требование издателя: в публикационной сборке никакой отладочной телеметрии на экране.
	// Гейт компиляционный, а не по галочке: так строку нельзя включить в релизе даже по ошибке
	// в настройках ассета. Выключатель общий для всего экранного отладочного слоя —
	// CONTRARY_WITH_QA_CHEATS (Debug/QADebug.h), в режиме Shipping он ноль.
	// Страховка на случай, если кубик пришёл из ассета WBP, а не создан кодом.
#if !CONTRARY_WITH_QA_CHEATS
	FrameTimeText->SetVisibility(ESlateVisibility::Collapsed);
#else
	// Живём по тому же выключателю, что счётчик кадров (плюс собственная галочка).
	if (!bShowFps || !bShowFrameTimings)
	{
		FrameTimeText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Замер снимаем КАЖДЫЙ кадр: сглаживание в движке накопительное, пропуск кадров исказил бы
	// числа. Стоит это чтения трёх счётчиков и нескольких умножений.
	float FrameMs = 0.0f, GameMs = 0.0f, DrawMs = 0.0f, GpuMs = 0.0f;
	UContrarySurvivorStatics::GetFrameTimingsMs(FrameMs, GameMs, DrawMs, GpuMs);

	// А вот текст пересобираем редко: сборка строки каждый кадр — лишняя работа в том самом
	// кадре, который мы и меряем.
	FrameTimeAccumulator += DeltaTime;
	if (FrameTimeAccumulator < FMath::Max(0.05f, FrameTimeUpdateInterval)
		&& FrameTimeText->GetVisibility() != ESlateVisibility::Collapsed)
	{
		return;
	}
	FrameTimeAccumulator = 0.0f;

	FrameTimeText->SetVisibility(ESlateVisibility::HitTestInvisible);
	// Culture-invariant строка: это отладочные числа, перевода не требуют.
	FrameTimeText->SetText(FText::FromString(FString::Printf(
		TEXT("кадр %.1f  логика %.1f  отрисовка %.1f  видео %.1f мс"),
		FrameMs, GameMs, DrawMs, GpuMs)));
#endif
}

void UTouchControlsWidget::UpdateFpsText()
{
	if (!FpsText)
	{
		return; // кубика нет (WBP без него и не кодовое дерево) — нечего обновлять
	}
	// Счётчик кадров с экрана релиза убран по требованию издателя — тем же компиляционным
	// гейтом, что и строка времён под ним (CONTRARY_WITH_QA_CHEATS, Debug/QADebug.h).
	// Страховка на случай, если кубик пришёл из ассета WBP, а не создан кодом.
#if !CONTRARY_WITH_QA_CHEATS
	FpsText->SetVisibility(ESlateVisibility::Collapsed);
	return;
#else
	if (!bShowFps)
	{
		FpsText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	FpsText->SetVisibility(ESlateVisibility::HitTestInvisible);

	const int32 Fps = FMath::RoundToInt(UContrarySurvivorStatics::GetCurrentFPS());
	// Число + приписка. Живое число — culture-invariant строка (перевода не требует).
	FString Line = FString::FromInt(Fps);
	if (!FpsSuffix.IsEmpty())
	{
		Line += FpsSuffix.ToString();
	}
	FpsText->SetText(FText::FromString(Line));
#endif
}

void UTouchControlsWidget::UpdateSprintVisual(float DeltaTime)
{
	if (!SprintButton)
	{
		return;
	}
	// Источник истины «бег включён» — реальное состояние персонажа GetIsSprinting() (директива
	// game-lead), а не флаг тач-кнопки: подсветка отражает фактический буст скорости, а не намерение.
	bool bSprinting = false;
	if (const AMasterHumanoidCharacter* Char = OwnerPC ? Cast<AMasterHumanoidCharacter>(OwnerPC->GetPawn()) : nullptr)
	{
		bSprinting = Char->GetIsSprinting();
	}
	if (bSprinting)
	{
		// Синий + пульсация. Яркость ходит по синусу между (1 - глубина) и 1 от выбранного
		// цвета: сам цвет не превышается, поэтому синий не выбеливается на пике. Альфа своя.
		SprintPulseTime += DeltaTime;
		const float Period = FMath::Max(0.05f, SprintPulsePeriod);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(2.0f * UE_PI * SprintPulseTime / Period); // 0..1
		const float Brightness = 1.0f - SprintPulseDepth * (1.0f - Pulse);
		FLinearColor C = SprintActiveColor * Brightness;
		C.A = SprintActiveColor.A;
		SprintButton->SetBackgroundColor(C);
		if (!bDesignerTree)
		{
			SprintButton->SetRenderOpacity(Config.ActiveOpacity);
		}
		bSprintVisualActive = true;
	}
	else if (bSprintVisualActive)
	{
		// Бег выключен — один раз возвращаем кнопку к покою.
		SprintButton->SetBackgroundColor(GetSprintIdleColor());
		if (!bDesignerTree)
		{
			SprintButton->SetRenderOpacity(Config.IdleOpacity);
		}
		SprintPulseTime = 0.0f;
		bSprintVisualActive = false;
	}
}

FLinearColor UTouchControlsWidget::GetSprintIdleColor() const
{
	// Ринат задал цвет покоя полем — берём поле; иначе тот, что снят с кнопки при создании
	// виджета (в WBP-режиме это цвет из дизайнера, в кодовом — белый).
	return bUseCustomSprintIdleColor ? SprintIdleColorCustom : SprintIdleColor;
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

void UTouchControlsWidget::ApplyStickCornerLock(const FGeometry& MyGeometry)
{
	// Только WBP-дерево: в кодовом дереве позицию ведёт Config.StickMargin, а слот «шляпки»
	// двигается при жесте (UpdateStickFromPointer) — замок конфликтовал бы с ним. В WBP-режиме
	// «шляпка» ходит Render Translation'ом, слоты свободны.
	if (!bLockStickCorner || !bDesignerTree || !StickBase || bStickLockSlotWarned)
	{
		return;
	}

	const FVector2D LocalSize = MyGeometry.GetLocalSize(); // весь экран в слейт-единицах
	if (LocalSize.Y <= KINDA_SMALL_NUMBER || LocalSize.Equals(LastStickLockSize, 0.5f))
	{
		return; // раскладки ещё нет / размер не менялся — слоты уже стоят
	}

	if (!Cast<UCanvasPanelSlot>(StickBase->Slot))
	{
		// Стик переложили из корневой канвы в другой контейнер — замок неприменим,
		// позицию целиком ведёт дизайнер. Говорим об этом один раз, не каждый кадр.
		bStickLockSlotWarned = true;
		UE_LOG(LogQA, Warning,
			TEXT("TouchControlsWidget: StickBase лежит не в канвас-слоте — фиксированный отступ стика от угла отключён, позиция из дизайнера"));
		return;
	}
	LastStickLockSize = LocalSize;

	// Отступ задан в пикселях эталонного экрана высотой 1080; на фактическом холсте
	// пересчитывается от его высоты — доля экрана (то есть видимый отступ от угла)
	// одинакова при любом разрешении и масштабе DPI, включая зону клампа кривой (<480 px).
	const float UnitsPerRef = static_cast<float>(LocalSize.Y) / 1080.0f;
	const FVector2D Pos(StickCornerOffsetRef.X * UnitsPerRef, -StickCornerOffsetRef.Y * UnitsPerRef);

	auto PlaceSlot = [&Pos](UWidget* W)
	{
		if (UCanvasPanelSlot* CanvasSlot = W ? Cast<UCanvasPanelSlot>(W->Slot) : nullptr)
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f)); // левый-нижний угол
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));          // позиция = центр круга
			CanvasSlot->SetPosition(Pos);
		}
	};
	PlaceSlot(StickBase);
	PlaceSlot(StickThumb);

	UE_LOG(LogQA, Display,
		TEXT("QA: стик поставлен на фиксированный отступ (%.0f, %.0f) от левого-нижнего угла (холст %.0fx%.0f, эталон %.0f/1080)"),
		Pos.X, -Pos.Y, LocalSize.X, LocalSize.Y, StickCornerOffsetRef.X);
}

void UTouchControlsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!OwnerPC)
	{
		return;
	}

	// Позиция стика — до всех гейтов: отступ от угла держится и под модалками (стик там
	// просто скрыт, но при закрытии обязан оказаться на месте без кадра «прыжка»).
	ApplyStickCornerLock(MyGeometry);

	// Число кадров рядом с ПАУЗА (Блок E): обновляем ДО гейта модалки — кнопка ПАУЗА видна и
	// на модальных экранах, значит и счётчик рядом с ней должен продолжать тикать.
	UpdateFpsText();
	// Времена кадра — там же и по тем же правилам (замер идёт и на модальных экранах: нам как
	// раз интересно, дорого ли обходится открытый магазин или инвентарь).
	UpdateFrameTimeText(InDeltaTime);

	// Модальное окно открыто -> боевая группа прячется, инжекция глушится (см. класс-коммент).
	// СУМКА/ПАУЗА остаются: их обработчики модалкам не вредят (тогл инвентаря / гейт паузы).
	const bool bModal = OwnerPC->IsAnyModalUIOpen();
	if (bModal == bCombatGroupVisible)
	{
		SetCombatGroupVisible(!bModal);
	}
	// Иконка оружия живёт по тем же правилам, что боевая группа: модалка — прячется,
	// закрылась — на следующем кадре состояние пересчитается и иконка вернётся.
	UpdateWeaponIcon(bModal);
	if (bModal)
	{
		return;
	}

	// Подсветка+пульсация кнопки БЕГ при включённом беге (Блок D): вне модалок, кнопка видна.
	UpdateSprintVisual(InDeltaTime);

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
		// Немедленная реакция; пульсацию поверх ведёт UpdateSprintVisual каждый кадр (Блок D).
		SprintButton->SetBackgroundColor(bSprintOn ? SprintActiveColor : GetSprintIdleColor());
		if (!bDesignerTree)
		{
			SprintButton->SetRenderOpacity(bSprintOn ? Config.ActiveOpacity : Config.IdleOpacity);
		}
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
		SprintButton->SetBackgroundColor(GetSprintIdleColor());
		if (!bDesignerTree)
		{
			SprintButton->SetRenderOpacity(Config.IdleOpacity);
		}
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
	if (bDesignerTree && StickBase)
	{
		// WBP: стик стоит там, куда его поставил Ринат, — центр берём из отрисованной
		// геометрии подложки (валидна к моменту pointer-событий: раскладка уже прошла).
		const FGeometry& BaseGeo = StickBase->GetCachedGeometry();
		return Geo.AbsoluteToLocal(BaseGeo.LocalToAbsolute(BaseGeo.GetLocalSize() * 0.5f));
	}
	// Кодовое дерево: anchors (0,1) = левый-нижний угол; центр стика — на StickMargin от него.
	const FVector2D LocalSize = Geo.GetLocalSize();
	return FVector2D(Config.StickMargin.X, LocalSize.Y - Config.StickMargin.Y);
}

float UTouchControlsWidget::GetStickRadiusPx() const
{
	if (bDesignerTree && StickBase)
	{
		// Полширины подложки, как её растянул Ринат (минимум 1 — защита от нулевой геометрии).
		return FMath::Max(1.0f, StickBase->GetCachedGeometry().GetLocalSize().X * 0.5f);
	}
	return Config.StickRadius;
}

void UTouchControlsWidget::UpdateStickFromPointer(const FGeometry& Geo, const FPointerEvent& Ev)
{
	const float StickRadius = GetStickRadiusPx();
	const FVector2D LocalPos = Geo.AbsoluteToLocal(Ev.GetScreenSpacePosition());
	FVector2D Offset = LocalPos - GetStickCenterLocal(Geo);

	// Ограничиваем ход «шляпки» радиусом подложки.
	const float Len = Offset.Size();
	if (Len > StickRadius && Len > KINDA_SMALL_NUMBER)
	{
		Offset *= StickRadius / Len;
	}

	// Экранный Y растёт вниз, а ось Y MoveAction — «вперёд» (Move: MoveDir = Forward*Y + Right*X).
	FVector2D NewVector(Offset.X / StickRadius, -Offset.Y / StickRadius);
	if (NewVector.Size() < Config.StickDeadZone)
	{
		NewVector = FVector2D::ZeroVector;
	}
	StickVector = NewVector;

	if (StickThumb)
	{
		if (bDesignerTree)
		{
			// WBP: двигаем «шляпку» Render Translation'ом относительно места, куда её
			// поставил Ринат (работает в любом контейнере, не только Canvas).
			StickThumb->SetRenderTranslation(Offset);
		}
		else if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(StickThumb->Slot))
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
		if (bDesignerTree)
		{
			StickThumb->SetRenderTranslation(FVector2D::ZeroVector);
		}
		else
		{
			if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(StickThumb->Slot))
			{
				ThumbSlot->SetPosition(FVector2D(Config.StickMargin.X, -Config.StickMargin.Y));
			}
			StickThumb->SetRenderOpacity(Config.IdleOpacity);
		}
	}
	if (StickBase && !bDesignerTree)
	{
		StickBase->SetRenderOpacity(Config.IdleOpacity);
	}
}

FReply UTouchControlsWidget::HandlePointerDown(const FGeometry& Geo, const FPointerEvent& Ev)
{
	// Жест стика начинается ТОЛЬКО в границах подложки. Кодовое дерево: сюда и так доходят
	// лишь события с подложки (кнопки съедают свои). WBP: до нас всплывает любой Visible-кубик
	// Рината — фильтруем по реальной геометрии StickBase.
	if (bStickActive || !StickBase
		|| !StickBase->GetCachedGeometry().IsUnderLocation(Ev.GetScreenSpacePosition()))
	{
		return FReply::Unhandled();
	}
	bStickActive = true;
	StickPointerIndex = Ev.GetPointerIndex();
	UpdateStickFromPointer(Geo, Ev);
	if (!bDesignerTree)
	{
		// Кодовый стиль: активная прозрачность под пальцем. WBP-кубики не трогаем.
		if (StickBase)  { StickBase->SetRenderOpacity(Config.ActiveOpacity); }
		if (StickThumb) { StickThumb->SetRenderOpacity(Config.ActiveOpacity); }
	}
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
