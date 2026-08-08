// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/InventoryScreenWidget.h"
#include "ContrarySurvivor/UI/ItemTileWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AArmor.h"               // EArmorSlot, надетая броня paper-doll
#include "AMasterInventoryItem.h" // EItemCategory, ItemIcon
#include "AMasterWeapon.h"        // слот оружия (отображение)
#include "UInventoryComponent.h"
#include "Blueprint/WidgetTree.h" // сетка плиток строится в дереве живого экрана
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"

void UInventoryScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Дефолт класса плитки — C++-плитка с кодовым деревом: экран работает без ассетов
	// (паттерн окна обыска); WBP_ItemTile назначает генератор/Ринат.
	if (!TileWidgetClass)
	{
		TileWidgetClass = UItemTileWidget::StaticClass();
	}

	if (HeadSlotButton)
	{
		HeadSlotButton->OnClicked.AddDynamic(this, &UInventoryScreenWidget::HandleHeadSlotClicked);
	}
	if (TorsoSlotButton)
	{
		TorsoSlotButton->OnClicked.AddDynamic(this, &UInventoryScreenWidget::HandleTorsoSlotClicked);
	}
	if (LegsSlotButton)
	{
		LegsSlotButton->OnClicked.AddDynamic(this, &UInventoryScreenWidget::HandleLegsSlotClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UInventoryScreenWidget::HandleCloseClicked);
	}
	// Кнопки закрытия может и не быть: инвентарь закрывается клавишей Tab/кнопкой СУМКА —
	// предупреждение не пишем, это законная раскладка.
}

void UInventoryScreenWidget::InitInventory(APlayerCharacter* InPlayer)
{
	Player = InPlayer;
	LastBackpackCount = -1; // форс-пересборка
	RefreshAll();
}

void UInventoryScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Player)
	{
		return;
	}

	// Значения статов — каждый кадр (голод/жажда утекают и при открытом инвентаре).
	// Три отдельных кубика вместо одной слепленной строки (ADR-050).
	if (UStatsComponent* St = Player->GetStats())
	{
		const FText MaxText = FText::AsNumber(FMath::RoundToInt32(St->GetSurvivalMax()));

		if (InvMoneyText)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Amount"), FText::AsNumber(FMath::RoundToInt32(St->GetMoney())));
			InvMoneyText->SetText(FText::Format(MoneyFormat, Args));
		}
		if (InvHungerText)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Current"), FText::AsNumber(FMath::RoundToInt32(St->GetHunger())));
			Args.Add(TEXT("Max"), MaxText);
			InvHungerText->SetText(FText::Format(HungerFormat, Args));
		}
		if (InvThirstText)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Current"), FText::AsNumber(FMath::RoundToInt32(St->GetThirst())));
			Args.Add(TEXT("Max"), MaxText);
			InvThirstText->SetText(FText::Format(ThirstFormat, Args));
		}
	}

	// Дешёвая сигнатура состава: изменилась — пересборка paper-doll и рюкзака.
	// Ловит и внешние изменения (подбор, QA-клавиши выдачи предметов F2/F3).
	// Build 1.2.1 (стаки): считаем ШТУКИ (Max(1, стак)) — съеденная из стака тушёнка
	// меняет сигнатуру и обновляет «x5» -> «x4», хотя число строк не изменилось.
	int32 BackpackCount = 0;
	if (UInventoryComponent* Inv = Player->GetInventory())
	{
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (IsValid(Item) && !Inv->IsItemEquipped(Item))
			{
				BackpackCount += FMath::Max(1, Item->GetStackCount());
			}
		}
	}
	const int32 ProtectionPct = FMath::RoundToInt(Player->GetEffectiveArmorFraction() * 100.0f);

	if (BackpackCount != LastBackpackCount || ProtectionPct != LastProtectionPct
		|| MakeWeaponSignature() != LastWeaponName)
	{
		RefreshAll();
	}
}

FString UInventoryScreenWidget::MakeWeaponSignature() const
{
	// Состав ОБОИХ слотов плюс пометка, какой сейчас в руках: смена оружия по кнопке
	// меняет только пометку — она обязана попасть в сигнатуру, иначе слоты не обновятся.
	if (!Player)
	{
		return FString();
	}
	const AMasterWeapon* Ranged = Player->GetRangedWeaponInstance();
	const AMasterWeapon* Melee = Player->GetMeleeWeaponInstance();
	const AMasterWeapon* InHands = Player->GetCurrentWeapon();
	return FString::Printf(TEXT("%s|%s|%s"),
		Ranged ? *Ranged->GetName() : TEXT("-"),
		Melee ? *Melee->GetName() : TEXT("-"),
		InHands ? *InHands->GetName() : TEXT("-"));
}

void UInventoryScreenWidget::RefreshAll()
{
	if (!Player)
	{
		return;
	}

	// --- Paper-doll: три слота брони ---
	RefreshArmorSlot(EArmorSlot::Head, HeadSlotText, HeadSlotIcon);
	RefreshArmorSlot(EArmorSlot::Torso, TorsoSlotText, TorsoSlotIcon);
	RefreshArmorSlot(EArmorSlot::Legs, LegsSlotText, LegsSlotIcon);

	// --- «Защита: N%» и слот оружия ---
	const int32 ProtectionPct = FMath::RoundToInt(Player->GetEffectiveArmorFraction() * 100.0f);
	if (ProtectionText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Percent"), FText::AsNumber(ProtectionPct));
		ProtectionText->SetText(FText::Format(ProtectionFormat, Args));
	}
	// --- Два слота оружия (Build 1.2.2, Ринат: «один слот под холодное оружие и один под
	// огнестрельное»). Экземпляры живут оба сразу, в руках — один: он и помечается.
	// Это ТОЛЬКО отображение, переключение оружия по-прежнему делает SwitchWeapon.
	const AMasterWeapon* InHands = Player->GetCurrentWeapon();
	const AMasterWeapon* Ranged = Player->GetRangedWeaponInstance();
	const AMasterWeapon* Melee = Player->GetMeleeWeaponInstance();
	RefreshWeaponSlot(Ranged, Ranged && Ranged == InHands, RangedSlotText, RangedSlotIcon);
	RefreshWeaponSlot(Melee, Melee && Melee == InHands, MeleeSlotText, MeleeSlotIcon);

	// --- Рюкзак: СЕТКА ПЛИТОК (Build 1.2.2, Ринат: «иконки в сетке, как в сталкере или
	// LDoE») внутри прежнего ScrollBox-кубика. Плитки — динамика (как раньше строки):
	// каждый Refresh строит свежую UniformGrid-панель, старую забирает GC с ClearChildren.
	int32 BackpackCount = 0;
	if (BackpackList)
	{
		BackpackList->ClearChildren();

		UInventoryComponent* Inv = Player->GetInventory();
		if (Inv && TileWidgetClass && WidgetTree)
		{
			UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(
				UUniformGridPanel::StaticClass());
			// Зазоры между плитками (приёмка Рината: ряды слипались по вертикали). Отступ
			// кладётся на КАЖДУЮ ячейку со всех сторон, поэтому берём половину — между
			// соседними плитками складываются две половины и получается заданное число,
			// а по краю сетки остаётся половина. Сетку строит код, в дизайнере этих
			// отступов не поменять — они параметры окна (TileSpacingX/Y).
			Grid->SetSlotPadding(FMargin(TileSpacingX * 0.5f, TileSpacingY * 0.5f));
			BackpackList->AddChild(Grid);

			APlayerController* PC = GetOwningPlayer();
			const int32 Columns = FMath::Max(1, TileColumns);
			int32 TileIndex = 0;
			for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
			{
				if (!IsValid(Item) || Inv->IsItemEquipped(Item))
				{
					continue; // экипированное показано в paper-doll
				}
				// Сигнатура в ШТУКАХ (как в NativeTick) — «x5» -> «x4» тоже пересборка.
				BackpackCount += FMath::Max(1, Item->GetStackCount());

				UItemTileWidget* Tile = CreateWidget<UItemTileWidget>(PC, TileWidgetClass);
				if (!Tile)
				{
					continue;
				}
				Tile->Item = Item;

				// Название только через GetItemDisplayText, иконка только через GetItemIcon
				// (ADR-050: служебные имена и поля напрямую наружу не уходят).
				const TSoftObjectPtr<UTexture2D> SoftIcon = Item->GetItemIcon();
				Tile->SetTileData(SoftIcon.IsNull() ? nullptr : SoftIcon.LoadSynchronous(),
					Item->GetItemDisplayText(), Item->GetStackCount());
				Tile->SetTileSize(TileSize, TileIconSize);
				Tile->SetDropVisible(true); // выброс — мини-кнопка в углу плитки
				Tile->OnTileClicked.AddUObject(this, &UInventoryScreenWidget::HandleTileUse);
				Tile->OnDropClicked.AddUObject(this, &UInventoryScreenWidget::HandleTileDrop);

				if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(
					Tile, TileIndex / Columns, TileIndex % Columns))
				{
					GridSlot->SetHorizontalAlignment(HAlign_Center);
					GridSlot->SetVerticalAlignment(VAlign_Top);
				}
				++TileIndex;
			}
		}
		else if (!TileWidgetClass)
		{
			UE_LOG(LogQA, Warning,
				TEXT("InventoryScreenWidget: TileWidgetClass пуст — рюкзак пуст (дефолт ставится в NativeOnInitialized)"));
		}
	}
	else
	{
		UE_LOG(LogQA, Warning, TEXT("InventoryScreenWidget: кубик BackpackList не найден в WBP_Inventory"));
	}

	// Обновляем сигнатуру ПОСЛЕ пересборки (иначе тик пересоберёт повторно).
	LastBackpackCount = BackpackCount;
	LastProtectionPct = ProtectionPct;
	LastWeaponName = MakeWeaponSignature();
}

void UInventoryScreenWidget::RefreshWeaponSlot(const AMasterWeapon* Weapon, bool bInHands,
	UTextBlock* SlotText, UImage* SlotIcon)
{
	if (SlotText)
	{
		// Здесь была ГЛАВНАЯ протечка служебных имён: стоял Weapon->GetName() и игрок читал
		// «BP_Pistol_C_1» (ADR-049, ревью издателя). Название берётся как у любого предмета,
		// пустой слот — то же слово, что у пустого слота брони.
		FFormatNamedArguments Args;
		Args.Add(TEXT("ItemName"), Weapon ? Weapon->GetItemDisplayText() : EmptySlotText);
		SlotText->SetText(FText::Format(bInHands ? WeaponInHandsFormat : WeaponFormat, Args));
	}

	if (SlotIcon)
	{
		// Та же схема, что у слотов брони: иконка только при оружии в слоте, пусто — кубик
		// спрятан (под ним видна подложка из WBP). Размер иконки берётся из ассета
		// (Brush.ImageSize) — bMatchSize=false его не трогает, поэтому правки Рината
		// в дизайнере код не перебивает.
		UTexture2D* Icon = nullptr;
		if (Weapon)
		{
			const TSoftObjectPtr<UTexture2D> SoftIcon = Weapon->GetItemIcon();
			Icon = SoftIcon.IsNull() ? nullptr : SoftIcon.LoadSynchronous();
		}
		if (Icon)
		{
			SlotIcon->SetBrushFromTexture(Icon, /*bMatchSize=*/false);
			SlotIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UInventoryScreenWidget::RefreshArmorSlot(EArmorSlot ArmorSlot, UTextBlock* SlotText, UImage* SlotIcon)
{
	AArmor* Eq = Player ? Player->GetEquippedArmor(ArmorSlot) : nullptr;

	if (SlotText)
	{
		SlotText->SetText(Eq ? Eq->GetItemDisplayText() : EmptySlotText);
	}

	if (SlotIcon)
	{
		// Иконка ТОЛЬКО надетого предмета (мягкая ссылка; пересборки редкие — по действиям
		// игрока, синхронная загрузка не бьёт по кадру). Пустой слот — иконка прячется,
		// под ней видна статичная подложка Рината из WBP (арт пустого слота — его зона).
		// Build 1.2.2: через GetItemIcon() — единый геттер иконок (вычисляемые у наследников).
		UTexture2D* Icon = nullptr;
		if (Eq)
		{
			const TSoftObjectPtr<UTexture2D> SoftIcon = Eq->GetItemIcon();
			Icon = SoftIcon.IsNull() ? nullptr : SoftIcon.LoadSynchronous();
		}
		if (Icon)
		{
			SlotIcon->SetBrushFromTexture(Icon, /*bMatchSize=*/false);
			SlotIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UInventoryScreenWidget::UnequipSlot(EArmorSlot ArmorSlot)
{
	// Снимаем ТОЛЬКО с занятого слота — как Canvas-путь (кликабельная зона была только
	// у занятых); клик по пустому слоту — ничего.
	if (Player && Player->GetEquippedArmor(ArmorSlot))
	{
		Player->Inv_UnequipSlot(ArmorSlot);
		RefreshAll();
	}
}

void UInventoryScreenWidget::HandleHeadSlotClicked()  { UnequipSlot(EArmorSlot::Head); }
void UInventoryScreenWidget::HandleTorsoSlotClicked() { UnequipSlot(EArmorSlot::Torso); }
void UInventoryScreenWidget::HandleLegsSlotClicked()  { UnequipSlot(EArmorSlot::Legs); }

void UInventoryScreenWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast(); // закрывает контроллер (тот же путь, что клавиша Tab)
}

void UInventoryScreenWidget::HandleTileUse(UItemTileWidget* Tile)
{
	AMasterInventoryItem* Item = Tile ? Tile->Item.Get() : nullptr;
	if (!Player || !IsValid(Item))
	{
		return;
	}
	// Клик по плитке = применить. Действие есть у расходника (использовать), брони (надеть)
	// и огнестрела (занять слот оружия — ТЗ Рината 08-08); прочие предметы (квест/патроны)
	// по клику молчат, как раньше строка без кнопки «Использовать».
	const EItemCategory Category = Item->GetItemCategory();
	if (Category == EItemCategory::Consumable || Category == EItemCategory::Armor)
	{
		Player->Inv_UseBackpackItem(Item); // тот же вызов, что раньше кнопка строки
		RefreshAll();
	}
	else if (Category == EItemCategory::Weapon)
	{
		// STALKER-поток: тап по огнестрелу в рюкзаке переносит его в пустой слот оружия
		// (в кобуру). Занят слот или это не дальнобойное оружие — TryAdoptRangedWeapon
		// вернёт false, предмет просто остаётся в рюкзаке, окно не перерисовываем.
		if (Player->TryAdoptRangedWeapon(Item))
		{
			RefreshAll();
		}
	}
}

void UInventoryScreenWidget::HandleTileDrop(UItemTileWidget* Tile)
{
	AMasterInventoryItem* Item = Tile ? Tile->Item.Get() : nullptr;
	if (Player && IsValid(Item))
	{
		Player->Inv_DropItem(Item); // тот же вызов, что раньше кнопка [X] строки
		RefreshAll();
	}
}
