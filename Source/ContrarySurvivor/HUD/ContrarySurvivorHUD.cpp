// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorHUD.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"    // FCanvasTextItem (текст с тенью/обводкой, #18)
#include "Engine/Engine.h" // GEngine->GetMediumFont
#include "EngineUtils.h" // TActorIterator
#include "GameFramework/Pawn.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Debug/QADebug.h"     // QA-оверлей (буфер сообщений + видимость)
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // журнал квестов (диалог/трекер)
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "AArmor.h"               // EArmorSlot, AArmor
#include "AMasterInventoryItem.h" // EItemCategory, ItemName
#include "AMasterWeapon.h"        // GetCurrentWeapon display
#include "ARangedWeapon.h"        // патроны экипированного дальнобоя в HUD (#5)
#include "UInventoryComponent.h"  // рюкзак
#include "ContrarySurvivor/Actors/TraderNPC.h" // каталог/цены магазина
#include "ContrarySurvivor/Actors/ElderNPC.h"  // староста (предлагаемый квест)
#include "ContrarySurvivor/Actors/InteractableNPCInterface.h" // маркеры интерактивных NPC

void AContrarySurvivorHUD::DrawHUD()
{
	Super::DrawHUD();

	UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}

	// Залоченная цель игрока (приоритет показа). Геттер контроллера: GetCurrentTarget() -> AActor*.
	AActor* LockedTarget = nullptr;
	APlayerController* PC = GetOwningPlayerController();
	if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
	{
		LockedTarget = CSPC->GetCurrentTarget();
	}

	// Точка отсчёта для дистанции — пешка игрока (если есть), иначе позиция камеры/контроллера.
	FVector PlayerLocation = FVector::ZeroVector;
	bool bHavePlayerLocation = false;
	if (PC)
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			PlayerLocation = PlayerPawn->GetActorLocation();
			bHavePlayerLocation = true;
		}
	}

	const float RadiusSq = HealthBarShowRadius * HealthBarShowRadius;

	// Текущая пешка игрока — её хелсбар над головой не рисуем (у игрока свой HUD-стек).
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

	// ТИП-АГНОСТИЧНО: проходим по всем Pawn'ам с UStatsComponent (бандит, волк, …),
	// определяя «врага» по наличию компонента, а не по конкретному классу.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Enemy = *It;
		if (!IsValid(Enemy) || Enemy == PlayerPawn)
		{
			continue;
		}

		UStatsComponent* Stats = Enemy->FindComponentByClass<UStatsComponent>();
		if (!Stats || Stats->IsDead())
		{
			continue; // не-враги и мёртвых не показываем
		}

		// Условие показа: залочена (текущая цель любого типа) ИЛИ в радиусе от игрока.
		const bool bIsLocked = (LockedTarget == Enemy);
		bool bInRadius = false;
		if (!bIsLocked && bHavePlayerLocation)
		{
			bInRadius = FVector::DistSquared(PlayerLocation, Enemy->GetActorLocation()) <= RadiusSq;
		}

		if (bIsLocked || bInRadius)
		{
			DrawTargetHealthBar(Enemy, Stats, bIsLocked);
		}

		// ФИКС1: над текущей залоченной целью — заметный маркер-ретикл,
		// чтобы игрок (тач-управление) видел, на КОМ сейчас лок.
		if (bIsLocked)
		{
			DrawTargetMarker(Enemy);
		}
	}

	// --- Маркеры интерактивных NPC (находимость): торговец и т.п. ---
	// Рисуем до модальных экранов; внутри функция сама пропускает при открытых меню.
	DrawInteractiveNPCMarkers();

	// --- Статы игрока (GDD §7.7) ---
	if (PC)
	{
		if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PC->GetPawn()))
		{
			DrawPlayerStats(PlayerChar);

			// --- Трекер активного квеста («Волков: X/5») — GDD §7.7 ---
			// Только вне модальных экранов (на них квест виден в самом диалоге).
			if (!bInventoryOpen && !bShopOpen && !bDialogOpen && !bDeathScreen)
			{
				DrawQuestTracker(PlayerChar->GetQuests());
			}

			// --- Экран инвентаря поверх HUD (модальный, GDD §7.4) ---
			if (bInventoryOpen)
			{
				DrawInventory(PlayerChar);
			}

			// --- Экран магазина поверх HUD (модальный, GDD §7.6) ---
			if (bShopOpen)
			{
				DrawShop(PlayerChar);
			}

			// --- Экран диалога со старостой (модальный, GDD §7.7) ---
			if (bDialogOpen)
			{
				DrawDialog(PlayerChar);
			}

			// --- Экран смерти (#26): поверх всего, респаун по кнопке/клавише ---
			if (bDeathScreen)
			{
				DrawDeathScreen(PlayerChar);
			}
		}
	}

	// --- Контекстная подсказка взаимодействия (E) — пикап/торговец/староста (BUG3) ---
	// Только когда модальные экраны закрыты (иначе перекрывает панель).
	if (!bInventoryOpen && !bShopOpen && !bDialogOpen && !bDeathScreen)
	{
		if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
		{
			if (CSPC->HasInteractPrompt())
			{
				DrawInteractPrompt(CSPC->GetInteractPromptText());
			}
		}
	}

	// --- QA-оверлей (debug под автотестера) — рисуем ПОСЛЕДНИМ, поверх всех экранов ---
	DrawQADebugOverlay();
}

void AContrarySurvivorHUD::DrawQADebugOverlay()
{
	if (!Canvas || !FQADebug::bOverlayVisible)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const TArray<FString>& Messages = FQADebug::GetMessages();

	const float SX = static_cast<float>(Canvas->SizeX);
	const float SY = static_cast<float>(Canvas->SizeY);
	const float Margin = 16.0f;
	const float Scale = FMath::Max(0.5f, QAOverlayTextScale);

	// Высота строки по фактическому шрифту (с масштабом) + небольшой межстрочный зазор.
	float ProbeW = 0.0f, ProbeH = 0.0f;
	GetTextSize(TEXT("Ag"), ProbeW, ProbeH, Font);
	const float LineH = (ProbeH > 0.0f ? ProbeH : 14.0f) * Scale + 3.0f;

	// Заголовок-метка + строки. Низ-право: стек растёт снизу вверх.
	const FString Header = TEXT("== QA ==");
	const int32 TotalLines = Messages.Num() + 1; // +заголовок
	float Y = SY - Margin - LineH * TotalLines;

	// Заголовок (правый край).
	{
		float TW = 0.0f, TH = 0.0f;
		GetTextSize(Header, TW, TH, Font);
		const float X = SX - TW * Scale - Margin;
		DrawShadowedText(Header, QAOverlayColor, X, Y, Font, Scale);
		Y += LineH;
	}

	// Сообщения (старые сверху, свежие снизу — естественная лента).
	for (const FString& Line : Messages)
	{
		float TW = 0.0f, TH = 0.0f;
		GetTextSize(Line, TW, TH, Font);
		const float X = SX - TW * Scale - Margin;
		DrawShadowedText(Line, QAOverlayColor, X, Y, Font, Scale);
		Y += LineH;
	}
}

void AContrarySurvivorHUD::DrawInteractPrompt(const FString& Text)
{
	if (Text.IsEmpty() || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const float SX = static_cast<float>(Canvas->SizeX);
	const float SY = static_cast<float>(Canvas->SizeY);

	// Размер текста для центрирования и фоновой плашки.
	float TextW = 0.0f, TextH = 0.0f;
	GetTextSize(Text, TextW, TextH, Font);

	const float PadX = 14.0f;
	const float PadY = 8.0f;
	const float BoxW = TextW + PadX * 2.0f;
	const float BoxH = TextH + PadY * 2.0f;

	// Низ-центр экрана (над тач-зоной/над краем).
	const float BoxX = (SX - BoxW) * 0.5f;
	const float BoxY = SY * 0.82f;

	DrawRect(InteractPromptBgColor, BoxX, BoxY, BoxW, BoxH);
	DrawText(Text, InteractPromptTextColor, BoxX + PadX, BoxY + PadY, Font);
}

// ===========================================================================
// Экран инвентаря (immediate-mode, без UMG/.uasset) — GDD §7.4
// ===========================================================================

void AContrarySurvivorHUD::SetInventoryOpen(bool bOpen)
{
	bInventoryOpen = bOpen;
	if (!bOpen)
	{
		InvHitRegions.Reset();
	}
}

void AContrarySurvivorHUD::ToggleInventory()
{
	SetInventoryOpen(!bInventoryOpen);
}

bool AContrarySurvivorHUD::PointInRegion(const FVector2D& P, const FInvHitRegion& R)
{
	return P.X >= R.Min.X && P.X <= R.Max.X && P.Y >= R.Min.Y && P.Y <= R.Max.Y;
}

bool AContrarySurvivorHUD::HandleInventoryClick(FVector2D ScreenPos)
{
	if (!bInventoryOpen)
	{
		return false;
	}

	APlayerController* PC = GetOwningPlayerController();
	APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		return false;
	}

	for (const FInvHitRegion& R : InvHitRegions)
	{
		if (!PointInRegion(ScreenPos, R))
		{
			continue;
		}

		switch (R.Action)
		{
			case EInvAction::UnequipSlot:
				Player->Inv_UnequipSlot(static_cast<EArmorSlot>(R.SlotIndex));
				return true;
			case EInvAction::UseItem:
				if (IsValid(R.Item)) { Player->Inv_UseBackpackItem(R.Item); }
				return true;
			case EInvAction::DropItem:
				if (IsValid(R.Item)) { Player->Inv_DropItem(R.Item); }
				return true;
			default:
				break;
		}
	}
	return false;
}

void AContrarySurvivorHUD::DrawInvBox(float X, float Y, float W, float H, const FLinearColor& BaseColor,
	const FVector2D& MousePos, const FString& Label, UFont* Font)
{
	const bool bHover = (MousePos.X >= X && MousePos.X <= X + W && MousePos.Y >= Y && MousePos.Y <= Y + H);
	DrawRect(bHover ? InvHoverColor : BaseColor, X, Y, W, H);
	if (Font && !Label.IsEmpty())
	{
		DrawText(Label, FLinearColor::White, X + 8.0f, Y + (H - 12.0f) * 0.5f, Font);
	}
}

void AContrarySurvivorHUD::DrawInventory(APlayerCharacter* Player)
{
	if (!Player || !Canvas)
	{
		return;
	}

	InvHitRegions.Reset();

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Позиция курсора (для подсветки зон под мышью). На тач — последняя точка тапа.
	FVector2D Mouse(-1.0f, -1.0f);
	if (APlayerController* PCc = GetOwningPlayerController())
	{
		float MX = 0.0f, MY = 0.0f;
		if (PCc->GetMousePosition(MX, MY))
		{
			Mouse = FVector2D(MX, MY);
		}
	}

	const float SX = static_cast<float>(Canvas->SizeX);
	const float SY = static_cast<float>(Canvas->SizeY);

	// Затемнение фона.
	DrawRect(InvDimColor, 0.0f, 0.0f, SX, SY);

	// Центрированная панель.
	const float PanelW = FMath::Min(960.0f, SX * 0.86f);
	const float PanelH = FMath::Min(600.0f, SY * 0.86f);
	const float PX = (SX - PanelW) * 0.5f;
	const float PY = (SY - PanelH) * 0.5f;
	DrawRect(InvPanelColor, PX, PY, PanelW, PanelH);

	const float Pad = 16.0f;
	const float HeaderY = PY + Pad;
	if (Font)
	{
		DrawText(TEXT("INVENTORY  (Tab / I to close)"), FLinearColor::White, PX + Pad, HeaderY, Font);
	}

	// Деньги / голод / жажда (GDD §7.7).
	if (UStatsComponent* St = Player->GetStats())
	{
		const FString StatStr = FString::Printf(TEXT("Money %.0f      Hunger %.0f / %.0f      Thirst %.0f / %.0f"),
			St->GetMoney(), St->GetHunger(), St->GetSurvivalMax(), St->GetThirst(), St->GetSurvivalMax());
		if (Font)
		{
			DrawText(StatStr, FLinearColor(1.0f, 0.9f, 0.4f, 1.0f), PX + Pad, HeaderY + 22.0f, Font);
		}
	}

	const float ContentY = HeaderY + 56.0f;

	// --- Левая колонка: paper-doll (слоты брони + оружие) ---
	const float LeftW = PanelW * 0.42f;
	const float LeftX = PX + Pad;
	const float ColW = LeftW - Pad;
	if (Font)
	{
		DrawText(TEXT("EQUIPMENT"), FLinearColor::White, LeftX, ContentY, Font);
	}

	float SlotY = ContentY + 24.0f;
	const float SlotH = 56.0f;
	const float SlotGap = 10.0f;

	auto DrawArmorSlot = [&](const TCHAR* Name, EArmorSlot Slot)
	{
		AArmor* Eq = Player->GetEquippedArmor(Slot);
		FString Worn = TEXT("(empty)");
		if (Eq)
		{
			Worn = Eq->ItemName.IsEmpty() ? Eq->GetName() : Eq->ItemName;
		}
		const FString Label = FString::Printf(TEXT("%s: %s"), Name, *Worn);
		DrawInvBox(LeftX, SlotY, ColW, SlotH, Eq ? InvSlotFilledColor : InvSlotColor, Mouse, Label, Font);

		if (Eq)
		{
			// Клик по занятому слоту -> снять броню.
			FInvHitRegion R;
			R.Min = FVector2D(LeftX, SlotY);
			R.Max = FVector2D(LeftX + ColW, SlotY + SlotH);
			R.Action = EInvAction::UnequipSlot;
			R.SlotIndex = static_cast<int32>(Slot);
			InvHitRegions.Add(R);
		}
		SlotY += SlotH + SlotGap;
	};

	DrawArmorSlot(TEXT("Head"), EArmorSlot::Head);
	DrawArmorSlot(TEXT("Torso"), EArmorSlot::Torso);
	DrawArmorSlot(TEXT("Legs"), EArmorSlot::Legs);

	// Слот оружия (только отображение CurrentWeapon).
	{
		AMasterWeapon* W = Player->GetCurrentWeapon();
		const FString Label = FString::Printf(TEXT("Weapon: %s"), W ? *W->GetName() : TEXT("(none)"));
		DrawInvBox(LeftX, SlotY, ColW, SlotH, InvSlotColor, Mouse, Label, Font);
		SlotY += SlotH + SlotGap;
	}

	if (Font)
	{
		DrawText(TEXT("(click an armor slot to unequip)"), FLinearColor(0.7f, 0.7f, 0.7f, 1.0f), LeftX, SlotY, Font);
	}

	// --- Правая колонка: рюкзак (неэкипированные предметы) ---
	const float RightX = LeftX + LeftW + Pad;
	const float RightW = (PX + PanelW - Pad) - RightX;
	if (Font)
	{
		DrawText(TEXT("BACKPACK"), FLinearColor::White, RightX, ContentY, Font);
	}

	float RowY = ContentY + 24.0f;
	const float RowH = 34.0f;
	const float RowGap = 6.0f;
	const float DropW = 30.0f;
	const float MaxRowY = PY + PanelH - Pad - RowH;

	if (UInventoryComponent* Inv = Player->GetInventory())
	{
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (!IsValid(Item) || Inv->IsItemEquipped(Item))
			{
				continue; // экипированные показаны в paper-doll
			}

			const float MainW = RightW - DropW - 6.0f;

			FString ActionHint;
			const EItemCategory Cat = Item->GetItemCategory();
			switch (Cat)
			{
				case EItemCategory::Consumable: ActionHint = TEXT("use");   break;
				case EItemCategory::Armor:      ActionHint = TEXT("equip"); break;
				default:                        ActionHint = TEXT("");      break;
			}

			const FString Name = Item->ItemName.IsEmpty() ? Item->GetName() : Item->ItemName;
			const FString Label = ActionHint.IsEmpty()
				? Name
				: FString::Printf(TEXT("%s  [%s]"), *Name, *ActionHint);

			DrawInvBox(RightX, RowY, MainW, RowH, InvSlotColor, Mouse, Label, Font);

			// Клик по строке -> использовать (надеть броню / съесть расходник).
			if (Cat == EItemCategory::Consumable || Cat == EItemCategory::Armor)
			{
				FInvHitRegion R;
				R.Min = FVector2D(RightX, RowY);
				R.Max = FVector2D(RightX + MainW, RowY + RowH);
				R.Action = EInvAction::UseItem;
				R.Item = Item;
				InvHitRegions.Add(R);
			}

			// Кнопка [X] -> выбросить.
			const float DropX = RightX + MainW + 6.0f;
			DrawInvBox(DropX, RowY, DropW, RowH, InvDropColor, Mouse, TEXT("X"), Font);
			{
				FInvHitRegion D;
				D.Min = FVector2D(DropX, RowY);
				D.Max = FVector2D(DropX + DropW, RowY + RowH);
				D.Action = EInvAction::DropItem;
				D.Item = Item;
				InvHitRegions.Add(D);
			}

			RowY += RowH + RowGap;
			if (RowY > MaxRowY)
			{
				break; // MVP: без прокрутки — не вылезаем за панель
			}
		}
	}
}

// ===========================================================================
// Экран магазина (immediate-mode, без UMG/.uasset) — GDD §7.6
// ===========================================================================

void AContrarySurvivorHUD::SetShopOpen(bool bOpen, ATraderNPC* Trader)
{
	bShopOpen = bOpen;
	ShopTrader = bOpen ? Trader : nullptr;
	if (!bOpen)
	{
		ShopHitRegions.Reset();
	}
}

bool AContrarySurvivorHUD::HandleShopClick(FVector2D ScreenPos)
{
	if (!bShopOpen || !ShopTrader)
	{
		return false;
	}

	APlayerController* PC = GetOwningPlayerController();
	APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		return false;
	}

	const TArray<FShopEntry>& Catalog = ShopTrader->GetCatalog();

	for (const FShopHitRegion& R : ShopHitRegions)
	{
		const bool bInside = ScreenPos.X >= R.Min.X && ScreenPos.X <= R.Max.X &&
			ScreenPos.Y >= R.Min.Y && ScreenPos.Y <= R.Max.Y;
		if (!bInside)
		{
			continue;
		}

		switch (R.Action)
		{
			case EShopAction::Buy:
				if (Catalog.IsValidIndex(R.EntryIndex))
				{
					Player->Shop_BuyEntry(Catalog[R.EntryIndex]);
				}
				return true;
			case EShopAction::Sell:
				if (IsValid(R.Item))
				{
					Player->Shop_SellItem(R.Item, ShopTrader->GetSellValue(R.Item));
				}
				return true;
			case EShopAction::Close:
				if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
				{
					CSPC->CloseShop();
				}
				return true;
			default:
				break;
		}
	}
	return false;
}

void AContrarySurvivorHUD::DrawShop(APlayerCharacter* Player)
{
	if (!Player || !Canvas || !ShopTrader)
	{
		return;
	}

	ShopHitRegions.Reset();

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Позиция курсора (подсветка зон).
	FVector2D Mouse(-1.0f, -1.0f);
	if (APlayerController* PCc = GetOwningPlayerController())
	{
		float MX = 0.0f, MY = 0.0f;
		if (PCc->GetMousePosition(MX, MY))
		{
			Mouse = FVector2D(MX, MY);
		}
	}

	const float SX = static_cast<float>(Canvas->SizeX);
	const float SY = static_cast<float>(Canvas->SizeY);

	// Затемнение фона + центр-панель (как в инвентаре).
	DrawRect(InvDimColor, 0.0f, 0.0f, SX, SY);
	const float PanelW = FMath::Min(960.0f, SX * 0.86f);
	const float PanelH = FMath::Min(600.0f, SY * 0.86f);
	const float PX = (SX - PanelW) * 0.5f;
	const float PY = (SY - PanelH) * 0.5f;
	DrawRect(InvPanelColor, PX, PY, PanelW, PanelH);

	const float Pad = 16.0f;
	const float HeaderY = PY + Pad;
	if (Font)
	{
		DrawText(TEXT("TRADER  (E to close)"), FLinearColor::White, PX + Pad, HeaderY, Font);
	}

	const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
	if (Font)
	{
		DrawText(FString::Printf(TEXT("Money %.0f"), Money), FLinearColor(1.0f, 0.9f, 0.4f, 1.0f),
			PX + Pad, HeaderY + 22.0f, Font);
	}

	// Кнопка Close (правый верх панели).
	{
		const float CloseW = 90.0f, CloseH = 28.0f;
		const float CX = PX + PanelW - Pad - CloseW;
		const float CY = HeaderY;
		DrawInvBox(CX, CY, CloseW, CloseH, InvDropColor, Mouse, TEXT("Close"), Font);
		FShopHitRegion R;
		R.Min = FVector2D(CX, CY);
		R.Max = FVector2D(CX + CloseW, CY + CloseH);
		R.Action = EShopAction::Close;
		ShopHitRegions.Add(R);
	}

	const float ContentY = HeaderY + 56.0f;
	const float RowH = 34.0f;
	const float RowGap = 6.0f;
	const float BtnW = 64.0f;
	const float MaxRowY = PY + PanelH - Pad - RowH;

	// --- Левая колонка: каталог на продажу (BUY) ---
	const float LeftW = PanelW * 0.52f;
	const float LeftX = PX + Pad;
	const float LeftColW = LeftW - Pad;
	if (Font)
	{
		DrawText(TEXT("FOR SALE"), FLinearColor::White, LeftX, ContentY, Font);
	}

	float RowY = ContentY + 24.0f;
	const TArray<FShopEntry>& Catalog = ShopTrader->GetCatalog();
	for (int32 i = 0; i < Catalog.Num(); ++i)
	{
		const FShopEntry& E = Catalog[i];
		const float MainW = LeftColW - BtnW - 6.0f;
		const FString Label = FString::Printf(TEXT("%s  -  %.0f"), *E.DisplayName, E.Price);
		DrawInvBox(LeftX, RowY, MainW, RowH, InvSlotColor, Mouse, Label, Font);

		// Кнопка [Buy] — зелёная, если хватает денег, иначе тускло-красная.
		const float BtnX = LeftX + MainW + 6.0f;
		const bool bAfford = (Money >= E.Price);
		DrawInvBox(BtnX, RowY, BtnW, RowH, bAfford ? InvSlotFilledColor : InvDropColor, Mouse, TEXT("Buy"), Font);
		if (bAfford)
		{
			FShopHitRegion R;
			R.Min = FVector2D(BtnX, RowY);
			R.Max = FVector2D(BtnX + BtnW, RowY + RowH);
			R.Action = EShopAction::Buy;
			R.EntryIndex = i;
			ShopHitRegions.Add(R);
		}

		RowY += RowH + RowGap;
		if (RowY > MaxRowY)
		{
			break; // MVP: без прокрутки
		}
	}

	// --- Правая колонка: рюкзак на продажу (SELL) ---
	const float RightX = LeftX + LeftW + Pad;
	const float RightW = (PX + PanelW - Pad) - RightX;
	if (Font)
	{
		DrawText(TEXT("SELL FROM BACKPACK"), FLinearColor::White, RightX, ContentY, Font);
	}

	float SellY = ContentY + 24.0f;
	if (UInventoryComponent* Inv = Player->GetInventory())
	{
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (!IsValid(Item) || Inv->IsItemEquipped(Item))
			{
				continue; // надетую броню не продаём из этого списка
			}

			const float MainW = RightW - BtnW - 6.0f;
			const float SellVal = ShopTrader->GetSellValue(Item);
			const FString Name = Item->ItemName.IsEmpty() ? Item->GetName() : Item->ItemName;
			const FString Label = FString::Printf(TEXT("%s  (+%.0f)"), *Name, SellVal);
			DrawInvBox(RightX, SellY, MainW, RowH, InvSlotColor, Mouse, Label, Font);

			const float BtnX = RightX + MainW + 6.0f;
			DrawInvBox(BtnX, SellY, BtnW, RowH, InvSlotFilledColor, Mouse, TEXT("Sell"), Font);
			{
				FShopHitRegion R;
				R.Min = FVector2D(BtnX, SellY);
				R.Max = FVector2D(BtnX + BtnW, SellY + RowH);
				R.Action = EShopAction::Sell;
				R.Item = Item;
				ShopHitRegions.Add(R);
			}

			SellY += RowH + RowGap;
			if (SellY > MaxRowY)
			{
				break;
			}
		}
	}
}

// ===========================================================================
// Экран диалога со старостой (immediate-mode, без UMG/.uasset) — GDD §7.7
// ===========================================================================

void AContrarySurvivorHUD::SetDialogOpen(bool bOpen, AElderNPC* Elder)
{
	bDialogOpen = bOpen;
	DialogElder = bOpen ? Elder : nullptr;
	if (!bOpen)
	{
		DialogHitRegions.Reset();
	}
}

bool AContrarySurvivorHUD::HandleDialogClick(FVector2D ScreenPos)
{
	if (!bDialogOpen || !DialogElder)
	{
		return false;
	}

	APlayerController* PC = GetOwningPlayerController();
	APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		return false;
	}

	UQuestComponent* Quests = Player->GetQuests();
	AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC);
	// Актуальный квест старосты по порядку (кв.1, затем кв.2 после сдачи кв.1).
	const FName QuestId = DialogElder->GetQuestForPlayer(Quests).QuestId;

	for (const FDialogHitRegion& R : DialogHitRegions)
	{
		const bool bInside = ScreenPos.X >= R.Min.X && ScreenPos.X <= R.Max.X &&
			ScreenPos.Y >= R.Min.Y && ScreenPos.Y <= R.Max.Y;
		if (!bInside)
		{
			continue;
		}

		switch (R.Action)
		{
			case EDialogAction::Accept:
				UE_LOG(LogQA, Display, TEXT("QA: dialog choice ACCEPT"));
				if (Quests) { Quests->AcceptQuest(QuestId); }
				return true;
			case EDialogAction::Decline:
				UE_LOG(LogQA, Display, TEXT("QA: dialog choice DECLINE"));
				if (CSPC) { CSPC->CloseDialog(); }
				return true;
			case EDialogAction::TurnIn:
				UE_LOG(LogQA, Display, TEXT("QA: dialog choice TURN IN"));
				if (Quests) { Quests->TurnInQuest(QuestId); }
				return true;
			case EDialogAction::Close:
				UE_LOG(LogQA, Display, TEXT("QA: dialog choice CLOSE"));
				if (CSPC) { CSPC->CloseDialog(); }
				return true;
			default:
				break;
		}
	}
	return false;
}

void AContrarySurvivorHUD::DrawDialog(APlayerCharacter* Player)
{
	if (!Player || !Canvas || !DialogElder)
	{
		return;
	}

	DialogHitRegions.Reset();

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Позиция курсора (подсветка зон).
	FVector2D Mouse(-1.0f, -1.0f);
	if (APlayerController* PCc = GetOwningPlayerController())
	{
		float MX = 0.0f, MY = 0.0f;
		if (PCc->GetMousePosition(MX, MY))
		{
			Mouse = FVector2D(MX, MY);
		}
	}

	const float SX = static_cast<float>(Canvas->SizeX);
	const float SY = static_cast<float>(Canvas->SizeY);

	// Затемнение фона + нижняя панель диалога (как в визуальных новеллах).
	DrawRect(InvDimColor, 0.0f, 0.0f, SX, SY);

	const float PanelW = FMath::Min(900.0f, SX * 0.86f);
	const float PanelH = FMath::Min(280.0f, SY * 0.4f);
	const float PX = (SX - PanelW) * 0.5f;
	const float PY = SY - PanelH - 40.0f;
	DrawRect(InvPanelColor, PX, PY, PanelW, PanelH);

	const float Pad = 18.0f;

	// Определяем текст и кнопки по состоянию АКТУАЛЬНОГО квеста (по порядку) в журнале игрока.
	const FQuest& Offered = DialogElder->GetQuestForPlayer(Player->GetQuests());
	const FQuest* InLog = Player->GetQuests() ? Player->GetQuests()->FindQuest(Offered.QuestId) : nullptr;
	const EQuestState State = InLog ? InLog->State : EQuestState::NotStarted;
	// Источник данных квеста: журнал (если уже в нём) либо предложение старосты (ещё не принят).
	const FQuest& QData = InLog ? *InLog : Offered;

	// Заголовок — имя NPC.
	if (Font)
	{
		DrawText(TEXT("СТАРОСТА"), FLinearColor(0.6f, 0.85f, 1.0f, 1.0f), PX + Pad, PY + Pad, Font);
	}

	// Строка прогресса целей (kill и/или item), обобщённо по полям квеста.
	FString ObjStr;
	if (QData.TargetCount > 0)
	{
		ObjStr += FString::Printf(TEXT("%s: %d/%d"),
			*QData.KillTargetTag.ToString(), QData.Progress, QData.TargetCount);
	}
	if (QData.RequiredItemCount > 0)
	{
		if (!ObjStr.IsEmpty()) { ObjStr += TEXT(", "); }
		ObjStr += FString::Printf(TEXT("%s: %d/%d"),
			*QData.RequiredItemName, QData.ItemProgress, QData.RequiredItemCount);
	}

	// Реплика старосты (зависит от состояния).
	FString NPCText;
	switch (State)
	{
		case EQuestState::NotStarted:
			NPCText = Offered.Description; // полное описание задания
			break;
		case EQuestState::Active:
			NPCText = FString::Printf(TEXT("Ты ещё не закончил. %s — %s."), *QData.Title, *ObjStr);
			break;
		case EQuestState::Completed:
			NPCText = TEXT("Отлично! Задание выполнено. Вот твоя награда.");
			break;
		case EQuestState::TurnedIn:
			NPCText = TEXT("Спасибо тебе ещё раз. Деревня тебе благодарна.");
			break;
		default:
			break;
	}

	if (Font && !NPCText.IsEmpty())
	{
		DrawText(NPCText, FLinearColor::White, PX + Pad, PY + Pad + 30.0f, Font);
	}

	// Кнопки-ответы (внизу панели).
	const float BtnH = 40.0f;
	const float BtnY = PY + PanelH - Pad - BtnH;
	const float BtnGap = 14.0f;

	auto AddButton = [&](float X, float W, const FString& Label, EDialogAction Action, const FLinearColor& Color)
	{
		DrawInvBox(X, BtnY, W, BtnH, Color, Mouse, Label, Font);
		FDialogHitRegion R;
		R.Min = FVector2D(X, BtnY);
		R.Max = FVector2D(X + W, BtnY + BtnH);
		R.Action = Action;
		DialogHitRegions.Add(R);
	};

	const float BtnX = PX + Pad;

	switch (State)
	{
		case EQuestState::NotStarted:
		{
			const float BtnW = 200.0f;
			AddButton(BtnX, BtnW, TEXT("[ Принять ]"), EDialogAction::Accept, InvSlotFilledColor);
			AddButton(BtnX + BtnW + BtnGap, BtnW, TEXT("[ Отказаться ]"), EDialogAction::Decline, InvDropColor);
			break;
		}
		case EQuestState::Active:
		{
			AddButton(BtnX, 200.0f, TEXT("[ Закрыть ]"), EDialogAction::Close, InvSlotColor);
			break;
		}
		case EQuestState::Completed:
		{
			const FString TurnInLabel = FString::Printf(TEXT("[ Сдать (+%.0f) ]"), QData.RewardMoney);
			AddButton(BtnX, 240.0f, TurnInLabel, EDialogAction::TurnIn, InvSlotFilledColor);
			break;
		}
		case EQuestState::TurnedIn:
		{
			AddButton(BtnX, 200.0f, TEXT("[ Закрыть ]"), EDialogAction::Close, InvSlotColor);
			break;
		}
		default:
			break;
	}
}

void AContrarySurvivorHUD::DrawQuestTracker(UQuestComponent* QuestComp)
{
	if (!QuestComp || !Canvas)
	{
		return;
	}

	const FQuest* Tracked = QuestComp->GetTrackedQuest();
	if (!Tracked)
	{
		return; // нет активного/выполненного квеста — трекер не рисуем
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const float SX = static_cast<float>(Canvas->SizeX);

	// Обобщённая строка прогресса целей (kill и/или item).
	FString ObjStr;
	if (Tracked->TargetCount > 0)
	{
		ObjStr += FString::Printf(TEXT("%s %d/%d"),
			*Tracked->KillTargetTag.ToString(), Tracked->Progress, Tracked->TargetCount);
	}
	if (Tracked->RequiredItemCount > 0)
	{
		if (!ObjStr.IsEmpty()) { ObjStr += TEXT(", "); }
		ObjStr += FString::Printf(TEXT("%s %d/%d"),
			*Tracked->RequiredItemName, Tracked->ItemProgress, Tracked->RequiredItemCount);
	}

	const bool bDone = (Tracked->State == EQuestState::Completed);
	FString Text;
	if (bDone)
	{
		Text = FString::Printf(TEXT("Квест выполнен: %s (%s) - вернись к старосте"),
			*Tracked->Title, *ObjStr);
	}
	else
	{
		Text = FString::Printf(TEXT("Квест: %s — %s"), *Tracked->Title, *ObjStr);
	}

	float TextW = 0.0f, TextH = 0.0f;
	GetTextSize(Text, TextW, TextH, Font);

	// Правый верхний угол под отступом.
	const float X = SX - TextW - 28.0f;
	const float Y = 28.0f;

	// Фоновая плашка для читаемости.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), X - 8.0f, Y - 4.0f, TextW + 16.0f, TextH + 8.0f);
	DrawText(Text, bDone ? QuestTrackerDoneColor : QuestTrackerColor, X, Y, Font);
}

// ===========================================================================
// Экран смерти (#26) — immediate-mode, без UMG
// ===========================================================================

void AContrarySurvivorHUD::SetDeathScreenOpen(bool bOpen)
{
	bDeathScreen = bOpen;
	if (!bOpen)
	{
		DeathRespawnBtnMin = FVector2D::ZeroVector;
		DeathRespawnBtnMax = FVector2D::ZeroVector;
	}
}

bool AContrarySurvivorHUD::HandleDeathScreenClick(FVector2D ScreenPos)
{
	if (!bDeathScreen)
	{
		return false;
	}
	const bool bInside =
		ScreenPos.X >= DeathRespawnBtnMin.X && ScreenPos.X <= DeathRespawnBtnMax.X &&
		ScreenPos.Y >= DeathRespawnBtnMin.Y && ScreenPos.Y <= DeathRespawnBtnMax.Y;
	if (bInside)
	{
		UE_LOG(LogQA, Display, TEXT("QA: death screen RESPAWN button clicked"));
	}
	return bInside;
}

void AContrarySurvivorHUD::DrawDeathScreen(APlayerCharacter* Player)
{
	if (!Player || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Позиция курсора (подсветка кнопки).
	FVector2D Mouse(-1.0f, -1.0f);
	if (APlayerController* PCc = GetOwningPlayerController())
	{
		float MX = 0.0f, MY = 0.0f;
		if (PCc->GetMousePosition(MX, MY))
		{
			Mouse = FVector2D(MX, MY);
		}
	}

	const float SX = static_cast<float>(Canvas->SizeX);
	const float SY = static_cast<float>(Canvas->SizeY);

	// Затемнение всего экрана.
	DrawRect(DeathDimColor, 0.0f, 0.0f, SX, SY);

	// Заголовок «Вы погибли» по центру сверху панели (крупно через Scale).
	const FString Title = TEXT("ВЫ ПОГИБЛИ");
	float TitleW = 0.0f, TitleH = 0.0f;
	if (Font)
	{
		GetTextSize(Title, TitleW, TitleH, Font);
	}
	const float TitleScale = 2.4f;
	const float TitleX = (SX - TitleW * TitleScale) * 0.5f;
	const float TitleY = SY * 0.18f;
	DrawShadowedText(Title, DeathTitleColor, TitleX, TitleY, Font, TitleScale);

	// --- Статистика последней жизни ---
	const float LifeSec = Player->GetLastLifeDuration();
	const int32 Minutes = FMath::FloorToInt(LifeSec / 60.0f);
	const int32 Seconds = FMath::FloorToInt(LifeSec) % 60;
	const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
	const int32 QuestsDone = Player->GetQuests() ? Player->GetQuests()->GetTurnedInQuestCount() : 0;
	const int32 Kills = Player->GetEnemyKillCount();

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Прожито:  %02d:%02d"), Minutes, Seconds));
	Lines.Add(FString::Printf(TEXT("Убийца:  %s"), *Player->GetLastDamagerName()));
	Lines.Add(FString::Printf(TEXT("Деньги:  %.0f"), Money));
	Lines.Add(FString::Printf(TEXT("Квестов выполнено:  %d"), QuestsDone));
	Lines.Add(FString::Printf(TEXT("Врагов убито:  %d"), Kills));

	const float StatScale = 1.3f;
	float LineH = 26.0f;
	if (Font)
	{
		float PW = 0.0f, PH = 0.0f;
		GetTextSize(TEXT("Ag"), PW, PH, Font);
		LineH = (PH > 0.0f ? PH : 18.0f) * StatScale + 10.0f;
	}

	float StatY = SY * 0.40f;
	for (const FString& Line : Lines)
	{
		float LW = 0.0f, LH = 0.0f;
		if (Font)
		{
			GetTextSize(Line, LW, LH, Font);
		}
		const float LX = (SX - LW * StatScale) * 0.5f;
		DrawShadowedText(Line, DeathStatColor, LX, StatY, Font, StatScale);
		StatY += LineH;
	}

	// --- Кнопка «Возродиться» (рисованный прямоугольник + hit-test) ---
	const float BtnW = FMath::Min(360.0f, SX * 0.5f);
	const float BtnH = 56.0f;
	const float BtnX = (SX - BtnW) * 0.5f;
	const float BtnY = StatY + 24.0f;

	const bool bHover =
		Mouse.X >= BtnX && Mouse.X <= BtnX + BtnW && Mouse.Y >= BtnY && Mouse.Y <= BtnY + BtnH;
	DrawRect(bHover ? FLinearColor(0.3f, 0.6f, 0.35f, 1.0f) : DeathButtonColor, BtnX, BtnY, BtnW, BtnH);

	const FString BtnLabel = TEXT("ВОЗРОДИТЬСЯ");
	float BLW = 0.0f, BLH = 0.0f;
	if (Font)
	{
		GetTextSize(BtnLabel, BLW, BLH, Font);
	}
	const float BtnScale = 1.4f;
	DrawShadowedText(BtnLabel, FLinearColor::White,
		BtnX + (BtnW - BLW * BtnScale) * 0.5f, BtnY + (BtnH - BLH * BtnScale) * 0.5f, Font, BtnScale);

	// Запоминаем прямоугольник кнопки для hit-теста (CU-мышь по HUD).
	DeathRespawnBtnMin = FVector2D(BtnX, BtnY);
	DeathRespawnBtnMax = FVector2D(BtnX + BtnW, BtnY + BtnH);

	// Подсказка-клавиша (дублирование, т.к. клик мышью по HUD ненадёжен).
	const FString Hint = TEXT("Enter / Пробел — возродиться");
	float HW = 0.0f, HH = 0.0f;
	if (Font)
	{
		GetTextSize(Hint, HW, HH, Font);
	}
	DrawShadowedText(Hint, FLinearColor(0.8f, 0.8f, 0.8f, 1.0f),
		(SX - HW) * 0.5f, BtnY + BtnH + 16.0f, Font, 1.0f);
}

void AContrarySurvivorHUD::DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y,
	UFont* Font, float ScaleXY)
{
	if (!Canvas || !Font)
	{
		return;
	}

	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	// Тень + обводка — текст читается на любом фоне (#18). Подтверждено по UE 5.5
	// CanvasItem.h: EnableShadow(InColor, InOffset), bOutlined/OutlineColor, Scale.
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f), FVector2D(1.5f, 1.5f));
	Item.bOutlined = true;
	Item.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);
	Item.Scale = FVector2D(ScaleXY, ScaleXY);
	Canvas->DrawItem(Item);
}

void AContrarySurvivorHUD::DrawPlayerStats(APlayerCharacter* Player)
{
	if (!Player || !Canvas)
	{
		return;
	}

	UStatsComponent* Stats = Player->GetStats();
	if (!Stats)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Компактный левый стек: HP -> Hunger -> Thirst -> Ammo -> Money.
	// DEV-режим (решение game-lead): голод/жажда/деньги видны ВСЕГДА, не только в
	// критической зоне. Прятать-до-критич. (GDD §7.7) вернём в финальной UX-полировке.
	const float BarX = PlayerHudMarginX;
	float CurY = PlayerHudMarginY;
	const float SurvivalMax = FMath::Max(Stats->GetSurvivalMax(), 1.0f);
	const FLinearColor MoneyColor(1.0f, 0.85f, 0.2f, 1.0f);

	// --- HP-бар (слева вверху, GDD §7.7; #18: крупнее + текст с обводкой) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, PlayerHealthBarHeight);
	const float HpFillWidth = PlayerHealthBarWidth * FMath::Clamp(Stats->GetHealthPercent(), 0.0f, 1.0f);
	if (HpFillWidth > 0.0f)
	{
		DrawRect(PlayerHealthFillColor, BarX, CurY, HpFillWidth, PlayerHealthBarHeight);
	}
	DrawShadowedText(FString::Printf(TEXT("HP %.0f/%.0f"), Stats->GetHealth(), Stats->GetMaxHealth()),
		FLinearColor::White, BarX + 8.0f, CurY + 4.0f, Font);
	CurY += PlayerHealthBarHeight + 6.0f;

	// Высота баров голода/жажды (#18: крупнее).
	const float SurvBarH = PlayerSurvivalBarHeight;

	// --- Голод (всегда) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, SurvBarH);
	const float HungerFillW = PlayerHealthBarWidth * FMath::Clamp(Stats->GetHunger() / SurvivalMax, 0.0f, 1.0f);
	if (HungerFillW > 0.0f)
	{
		DrawRect(HungerColor, BarX, CurY, HungerFillW, SurvBarH);
	}
	DrawShadowedText(FString::Printf(TEXT("Hunger %.0f"), Stats->GetHunger()),
		FLinearColor::White, BarX + 8.0f, CurY + 3.0f, Font);
	CurY += SurvBarH + 4.0f;

	// --- Жажда (всегда) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, SurvBarH);
	const float ThirstFillW = PlayerHealthBarWidth * FMath::Clamp(Stats->GetThirst() / SurvivalMax, 0.0f, 1.0f);
	if (ThirstFillW > 0.0f)
	{
		DrawRect(ThirstColor, BarX, CurY, ThirstFillW, SurvBarH);
	}
	DrawShadowedText(FString::Printf(TEXT("Thirst %.0f"), Stats->GetThirst()),
		FLinearColor::White, BarX + 8.0f, CurY + 3.0f, Font);
	CurY += SurvBarH + 8.0f;

	// --- Патроны экипированного оружия (#5) ---
	// Показываем «в магазине / резерв» ТОЛЬКО для дальнобоя (ARangedWeapon). Холодное
	// оружие (нож, AMeleeWeapon) и пустые руки — патроны не рисуем. Геттеры базы
	// AMasterWeapon: GetCurrentAmmoInClip()/GetCurrentAmmoReserve() (подтв. AMasterWeapon.h).
	if (ARangedWeapon* Ranged = Cast<ARangedWeapon>(Player->GetCurrentWeapon()))
	{
		const FString AmmoStr = FString::Printf(TEXT("Ammo %d / %d"),
			Ranged->GetCurrentAmmoInClip(), Ranged->GetCurrentAmmoReserve());
		// Плашка под патронами для читаемости.
		float AmmoW = 0.0f, AmmoH = 0.0f;
		if (Font)
		{
			GetTextSize(AmmoStr, AmmoW, AmmoH, Font);
		}
		DrawRect(MoneyPlateColor, BarX - 4.0f, CurY - 2.0f, AmmoW + 16.0f, AmmoH + 6.0f);
		DrawShadowedText(AmmoStr, AmmoColor, BarX + 4.0f, CurY, Font);
		CurY += (AmmoH > 0.0f ? AmmoH : 16.0f) + 8.0f;
	}

	// --- Деньги (всегда) + подложка-плашка под текстом (#18) ---
	const FString MoneyStr = FString::Printf(TEXT("Money %.0f"), Stats->GetMoney());
	float MoneyW = 0.0f, MoneyH = 0.0f;
	if (Font)
	{
		GetTextSize(MoneyStr, MoneyW, MoneyH, Font);
	}
	DrawRect(MoneyPlateColor, BarX - 4.0f, CurY - 2.0f, MoneyW + 16.0f, MoneyH + 6.0f);
	DrawShadowedText(MoneyStr, MoneyColor, BarX + 4.0f, CurY, Font);
}

void AContrarySurvivorHUD::DrawTargetMarker(AActor* TargetActor)
{
	if (!IsValid(TargetActor) || !Canvas)
	{
		return;
	}

	// Якорь — центр силуэта цели; проекция в экран.
	const FVector WorldAnchor = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, TargetMarkerWorldZOffset);
	const FVector ScreenPos = Project(WorldAnchor, false);
	if (ScreenPos.Z <= 0.0f)
	{
		return; // за камерой
	}

	const float CX = ScreenPos.X;
	const float CY = ScreenPos.Y;
	const float H = TargetMarkerHalfSize;
	const float L = TargetMarkerCornerLen;
	const float T = TargetMarkerThickness;
	const FLinearColor C = TargetMarkerColor;

	// Четыре угловые скобки рамки (вид «захвата цели»).
	// Верх-левый
	DrawLine(CX - H, CY - H, CX - H + L, CY - H, C, T);
	DrawLine(CX - H, CY - H, CX - H, CY - H + L, C, T);
	// Верх-правый
	DrawLine(CX + H, CY - H, CX + H - L, CY - H, C, T);
	DrawLine(CX + H, CY - H, CX + H, CY - H + L, C, T);
	// Низ-левый
	DrawLine(CX - H, CY + H, CX - H + L, CY + H, C, T);
	DrawLine(CX - H, CY + H, CX - H, CY + H - L, C, T);
	// Низ-правый
	DrawLine(CX + H, CY + H, CX + H - L, CY + H, C, T);
	DrawLine(CX + H, CY + H, CX + H, CY + H - L, C, T);

	// Указывающий вниз треугольник над рамкой (доп. заметность).
	const float TriBot = CY - H - TargetMarkerTriGap;             // вершина (кончик вниз)
	const float TriTop = TriBot - TargetMarkerTriHeight;          // основание (выше)
	const float TriHalfW = TargetMarkerTriHeight * 0.6f;
	DrawLine(CX - TriHalfW, TriTop, CX + TriHalfW, TriTop, C, T); // основание
	DrawLine(CX - TriHalfW, TriTop, CX, TriBot, C, T);            // левое ребро к кончику
	DrawLine(CX + TriHalfW, TriTop, CX, TriBot, C, T);            // правое ребро к кончику
}

// ===========================================================================
// Маркеры интерактивных NPC (находимость) — торговец, позже староста (Фаза 5)
// ===========================================================================

void AContrarySurvivorHUD::DrawInteractiveNPCMarkers()
{
	UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}

	// Поверх модальных экранов (инвентарь/магазин/диалог) маркеры не нужны.
	if (bInventoryOpen || bShopOpen || bDialogOpen)
	{
		return;
	}

	// DRAFT/perf: для MVP (1 торговец) перебор всех актёров приемлем. Позже — реестр/тег.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->Implements<UInteractableNPCInterface>())
		{
			continue;
		}

		const IInteractableNPCInterface* NPC = Cast<IInteractableNPCInterface>(Actor);
		const float ZOff = NPC ? NPC->GetNPCMarkerZOffset() : 240.0f;
		const FString Label = NPC ? NPC->GetNPCMarkerLabel() : FString();

		DrawNPCMarker(Actor->GetActorLocation() + FVector(0.0f, 0.0f, ZOff), Label);
	}
}

void AContrarySurvivorHUD::DrawNPCMarker(const FVector& WorldAnchor, const FString& Label)
{
	if (!Canvas)
	{
		return;
	}

	const float SX = static_cast<float>(Canvas->SizeX);
	const float SY = static_cast<float>(Canvas->SizeY);
	const FVector2D Center(SX * 0.5f, SY * 0.5f);

	// Project: Z>0 — перед камерой; X,Y — экранные координаты.
	const FVector Screen = Project(WorldAnchor, false);
	const bool bBehind = Screen.Z <= 0.0f;

	FVector2D P(Screen.X, Screen.Y);
	if (bBehind)
	{
		// За камерой проекция «вывернута» — зеркалим относительно центра, чтобы стрелка
		// указывала в верную сторону.
		P = Center * 2.0f - P;
	}

	const float Margin = NPCMarkerEdgeMargin;
	const bool bOnScreen = !bBehind &&
		P.X >= Margin && P.X <= SX - Margin &&
		P.Y >= Margin && P.Y <= SY - Margin;

	if (bOnScreen)
	{
		DrawNPCIcon(P, Label);
		return;
	}

	// За кадром: зажимаем точку к краю экрана по лучу из центра и рисуем стрелку.
	FVector2D Dir = P - Center;
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.0f, -1.0f);
	}

	const float HalfW = SX * 0.5f - Margin;
	const float HalfH = SY * 0.5f - Margin;
	const float ScaleX = (FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER) ? HalfW / FMath::Abs(Dir.X) : TNumericLimits<float>::Max();
	const float ScaleY = (FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER) ? HalfH / FMath::Abs(Dir.Y) : TNumericLimits<float>::Max();
	const float Scale = FMath::Min(ScaleX, ScaleY);

	const FVector2D Edge = Center + Dir * Scale;
	DrawNPCEdgeArrow(Edge, Dir.GetSafeNormal());
}

void AContrarySurvivorHUD::DrawNPCIcon(const FVector2D& ScreenPos, const FString& Label)
{
	const float CX = ScreenPos.X;
	const float CY = ScreenPos.Y;
	const float H = NPCMarkerHalfSize;
	const float T = NPCMarkerThickness;
	const FLinearColor C = NPCMarkerColor;

	// Ромб (повёрнутый квадрат) — форма, отличная от углового ретикла врага.
	DrawLine(CX, CY - H, CX + H, CY, C, T); // верх -> право
	DrawLine(CX + H, CY, CX, CY + H, C, T); // право -> низ
	DrawLine(CX, CY + H, CX - H, CY, C, T); // низ -> лево
	DrawLine(CX - H, CY, CX, CY - H, C, T); // лево -> верх

	// Подпись под ромбом по центру.
	if (!Label.IsEmpty())
	{
		if (UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr)
		{
			float TW = 0.0f, TH = 0.0f;
			GetTextSize(Label, TW, TH, Font);
			DrawText(Label, C, CX - TW * 0.5f, CY + H + 2.0f, Font);
		}
	}
}

void AContrarySurvivorHUD::DrawNPCEdgeArrow(const FVector2D& EdgePos, const FVector2D& Dir)
{
	const FLinearColor C = NPCMarkerColor;
	const float T = NPCMarkerThickness;
	const float Len = NPCMarkerArrowLen;

	// Стрелка-треугольник: кончик в EdgePos, основание — назад вдоль -Dir.
	const FVector2D Perp(-Dir.Y, Dir.X);
	const FVector2D Back = EdgePos - Dir * Len;
	const FVector2D B1 = Back + Perp * (Len * 0.6f);
	const FVector2D B2 = Back - Perp * (Len * 0.6f);

	DrawLine(EdgePos.X, EdgePos.Y, B1.X, B1.Y, C, T);
	DrawLine(EdgePos.X, EdgePos.Y, B2.X, B2.Y, C, T);
	DrawLine(B1.X, B1.Y, B2.X, B2.Y, C, T);
}

void AContrarySurvivorHUD::DrawTargetHealthBar(AActor* TargetActor, UStatsComponent* Stats, bool bIsCurrentTarget)
{
	if (!IsValid(TargetActor) || !Stats || !Canvas)
	{
		return;
	}

	// Мировая точка над головой цели.
	const FVector WorldAnchor = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, HealthBarWorldZOffset);

	// Project: X,Y — экранные координаты, Z — глубина (>0 если перед камерой). За камерой — не рисуем.
	const FVector ScreenPos = Project(WorldAnchor, false);
	if (ScreenPos.Z <= 0.0f)
	{
		return;
	}

	// Отбрасываем то, что заведомо за пределами экрана.
	if (ScreenPos.X < -HealthBarWidth || ScreenPos.X > Canvas->SizeX + HealthBarWidth ||
		ScreenPos.Y < -HealthBarHeight || ScreenPos.Y > Canvas->SizeY + HealthBarHeight)
	{
		return;
	}

	const float HealthPercent = FMath::Clamp(Stats->GetHealthPercent(), 0.0f, 1.0f);

	// Полоску центрируем по горизонтали над якорем.
	const float BarX = ScreenPos.X - HealthBarWidth * 0.5f;
	const float BarY = ScreenPos.Y - HealthBarHeight;

	// Фон.
	DrawRect(BackgroundColor, BarX, BarY, HealthBarWidth, HealthBarHeight);

	// Заполнение по проценту здоровья. Текущая залоченная цель — ярче (выделяем).
	const float FillWidth = HealthBarWidth * HealthPercent;
	if (FillWidth > 0.0f)
	{
		DrawRect(bIsCurrentTarget ? TargetFillColor : FillColor, BarX, BarY, FillWidth, HealthBarHeight);
	}
}
