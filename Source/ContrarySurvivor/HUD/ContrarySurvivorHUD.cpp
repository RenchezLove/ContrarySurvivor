// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h" // GEngine->GetMediumFont
#include "EngineUtils.h" // TActorIterator
#include "GameFramework/Pawn.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "AArmor.h"               // EArmorSlot, AArmor
#include "AMasterInventoryItem.h" // EItemCategory, ItemName
#include "AMasterWeapon.h"        // GetCurrentWeapon display
#include "UInventoryComponent.h"  // рюкзак
#include "ContrarySurvivor/Actors/TraderNPC.h" // каталог/цены магазина

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

	// --- Статы игрока (GDD §7.7) ---
	if (PC)
	{
		if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PC->GetPawn()))
		{
			DrawPlayerStats(PlayerChar->GetStats());

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
		}
	}

	// --- Контекстная подсказка взаимодействия (E) — пикап/торговец (BUG3) ---
	// Только когда модальные экраны закрыты (иначе перекрывает панель).
	if (!bInventoryOpen && !bShopOpen)
	{
		if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
		{
			if (CSPC->HasInteractPrompt())
			{
				DrawInteractPrompt(CSPC->GetInteractPromptText());
			}
		}
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

void AContrarySurvivorHUD::DrawPlayerStats(UStatsComponent* Stats)
{
	if (!Stats || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Компактный левый стек: HP -> Hunger -> Thirst -> Money.
	// DEV-режим (решение game-lead): голод/жажда/деньги видны ВСЕГДА, не только в
	// критической зоне. Прятать-до-критич. (GDD §7.7) вернём в финальной UX-полировке.
	const float BarX = PlayerHudMarginX;
	float CurY = PlayerHudMarginY;
	const float SurvivalMax = FMath::Max(Stats->GetSurvivalMax(), 1.0f);
	const FLinearColor MoneyColor(1.0f, 0.85f, 0.2f, 1.0f);

	// --- HP-бар (слева вверху, GDD §7.7) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, PlayerHealthBarHeight);
	const float HpFillWidth = PlayerHealthBarWidth * FMath::Clamp(Stats->GetHealthPercent(), 0.0f, 1.0f);
	if (HpFillWidth > 0.0f)
	{
		DrawRect(PlayerHealthFillColor, BarX, CurY, HpFillWidth, PlayerHealthBarHeight);
	}
	if (Font)
	{
		DrawText(FString::Printf(TEXT("HP %.0f/%.0f"), Stats->GetHealth(), Stats->GetMaxHealth()),
			FLinearColor::White, BarX + 6.0f, CurY + 2.0f, Font);
	}
	CurY += PlayerHealthBarHeight + 6.0f;

	// Высота баров голода/жажды.
	const float SurvBarH = 16.0f;

	// --- Голод (всегда) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, SurvBarH);
	const float HungerFillW = PlayerHealthBarWidth * FMath::Clamp(Stats->GetHunger() / SurvivalMax, 0.0f, 1.0f);
	if (HungerFillW > 0.0f)
	{
		DrawRect(HungerColor, BarX, CurY, HungerFillW, SurvBarH);
	}
	if (Font)
	{
		DrawText(FString::Printf(TEXT("Hunger %.0f"), Stats->GetHunger()),
			FLinearColor::White, BarX + 6.0f, CurY + 1.0f, Font);
	}
	CurY += SurvBarH + 4.0f;

	// --- Жажда (всегда) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, SurvBarH);
	const float ThirstFillW = PlayerHealthBarWidth * FMath::Clamp(Stats->GetThirst() / SurvivalMax, 0.0f, 1.0f);
	if (ThirstFillW > 0.0f)
	{
		DrawRect(ThirstColor, BarX, CurY, ThirstFillW, SurvBarH);
	}
	if (Font)
	{
		DrawText(FString::Printf(TEXT("Thirst %.0f"), Stats->GetThirst()),
			FLinearColor::White, BarX + 6.0f, CurY + 1.0f, Font);
	}
	CurY += SurvBarH + 6.0f;

	// --- Деньги (всегда, читаемо в общем стеке) ---
	if (Font)
	{
		DrawText(FString::Printf(TEXT("Money %.0f"), Stats->GetMoney()),
			MoneyColor, BarX, CurY, Font);
	}
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
