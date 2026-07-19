// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/InventoryScreenWidget.h"
#include "ContrarySurvivor/UI/InventoryRowWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AArmor.h"               // EArmorSlot, надетая броня paper-doll
#include "AMasterInventoryItem.h" // EItemCategory, ItemIcon
#include "AMasterWeapon.h"        // слот оружия (отображение)
#include "UInventoryComponent.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Engine/Texture2D.h"

void UInventoryScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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
	int32 BackpackCount = 0;
	if (UInventoryComponent* Inv = Player->GetInventory())
	{
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (IsValid(Item) && !Inv->IsItemEquipped(Item))
			{
				++BackpackCount;
			}
		}
	}
	const int32 ProtectionPct = FMath::RoundToInt(Player->GetEffectiveArmorFraction() * 100.0f);
	const AMasterWeapon* Weapon = Player->GetCurrentWeapon();
	const FString WeaponName = Weapon ? Weapon->GetName() : FString();

	if (BackpackCount != LastBackpackCount || ProtectionPct != LastProtectionPct
		|| WeaponName != LastWeaponName)
	{
		RefreshAll();
	}
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
	const AMasterWeapon* Weapon = Player->GetCurrentWeapon();
	if (WeaponText)
	{
		// Здесь была ГЛАВНАЯ протечка служебных имён: стоял Weapon->GetName() и игрок читал
		// «BP_Pistol_C_1» (ADR-049, ревью издателя). Название берётся как у любого предмета,
		// с пустыми руками — то же слово, что у пустого слота брони.
		FFormatNamedArguments Args;
		Args.Add(TEXT("ItemName"), Weapon ? Weapon->GetItemDisplayText() : EmptySlotText);
		WeaponText->SetText(FText::Format(WeaponFormat, Args));
	}

	// --- Рюкзак ---
	int32 BackpackCount = 0;
	if (BackpackList)
	{
		BackpackList->ClearChildren();

		UInventoryComponent* Inv = Player->GetInventory();
		if (Inv && RowWidgetClass)
		{
			APlayerController* PC = GetOwningPlayer();
			for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
			{
				if (!IsValid(Item) || Inv->IsItemEquipped(Item))
				{
					continue; // экипированное показано в paper-doll
				}
				++BackpackCount;

				FText UseCaption;
				switch (Item->GetItemCategory())
				{
					case EItemCategory::Consumable: UseCaption = UseHintConsumable; break;
					case EItemCategory::Armor:      UseCaption = UseHintArmor;      break;
					default: break; // пусто — кнопка применения прячется в SetupRow
				}

				// Название только через GetItemDisplayText: служебное имя актора наружу
				// не уходит (ADR-050, порция 0).
				if (UInventoryRowWidget* Row = CreateWidget<UInventoryRowWidget>(PC, RowWidgetClass))
				{
					Row->Item = Item;
					Row->SetupRow(Item->GetItemDisplayText(), UseCaption);
					Row->OnUseClicked.AddUObject(this, &UInventoryScreenWidget::HandleRowUse);
					Row->OnDropClicked.AddUObject(this, &UInventoryScreenWidget::HandleRowDrop);
					BackpackList->AddChild(Row);
				}
			}
		}
		else if (!RowWidgetClass)
		{
			UE_LOG(LogQA, Warning,
				TEXT("InventoryScreenWidget: RowWidgetClass пуст (назначь WBP_InventoryRow в Class Defaults WBP_Inventory) — рюкзак пуст"));
		}
	}
	else
	{
		UE_LOG(LogQA, Warning, TEXT("InventoryScreenWidget: кубик BackpackList не найден в WBP_Inventory"));
	}

	// Обновляем сигнатуру ПОСЛЕ пересборки (иначе тик пересоберёт повторно).
	LastBackpackCount = BackpackCount;
	LastProtectionPct = ProtectionPct;
	LastWeaponName = Weapon ? Weapon->GetName() : FString();
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
		UTexture2D* Icon = nullptr;
		if (Eq && !Eq->ItemIcon.IsNull())
		{
			Icon = Eq->ItemIcon.LoadSynchronous();
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

void UInventoryScreenWidget::HandleRowUse(UInventoryRowWidget* Row)
{
	AMasterInventoryItem* Item = Row ? Row->Item.Get() : nullptr;
	if (Player && IsValid(Item))
	{
		Player->Inv_UseBackpackItem(Item); // тот же вызов, что клик Canvas-строки
		RefreshAll();
	}
}

void UInventoryScreenWidget::HandleRowDrop(UInventoryRowWidget* Row)
{
	AMasterInventoryItem* Item = Row ? Row->Item.Get() : nullptr;
	if (Player && IsValid(Item))
	{
		Player->Inv_DropItem(Item); // тот же вызов, что кнопка [X] Canvas-строки
		RefreshAll();
	}
}
