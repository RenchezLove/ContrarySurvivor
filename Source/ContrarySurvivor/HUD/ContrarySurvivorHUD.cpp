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
#include "ContrarySurvivor/UI/ShopScreenWidget.h"      // ADR-048: UMG-путь магазина (слот ShopWidgetClass)
#include "ContrarySurvivor/UI/DialogScreenWidget.h"    // ADR-048: UMG-путь диалога старосты
#include "ContrarySurvivor/UI/InventoryScreenWidget.h" // ADR-048: UMG-путь инвентаря
#include "ContrarySurvivor/UI/DeathScreenWidget.h"     // ADR-048: UMG-путь экрана смерти
#include "ContrarySurvivor/UI/PlayerStatsWidget.h"     // ADR-048: постоянная панель статов
#include "ContrarySurvivor/UI/QuestTrackerWidget.h"    // ADR-048: постоянный трекер квеста
#include "ContrarySurvivor/UI/InteractPromptWidget.h"  // ADR-048: постоянная подсказка E
#include "ContrarySurvivor/UI/CorpseLootWidget.h"      // Build 1.2.1 (А1): окно обыска трупа
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // Build 1.2.1 (А1): контейнер лута трупа
#include "AArmor.h"               // EArmorSlot, AArmor
#include "AMasterInventoryItem.h" // EItemCategory, ItemName
#include "AMasterWeapon.h"        // GetCurrentWeapon display
#include "ARangedWeapon.h"        // патроны экипированного дальнобоя в HUD (#5)
#include "UInventoryComponent.h"  // рюкзак
#include "AAmmoItem.h"            // стак патронов (слайдер продажи)
#include "ContrarySurvivor/Actors/ShopTypes.h" // FShopEntry / EShopEntryKind (каталог/цены магазина, A2)
#include "ContrarySurvivor/Actors/ElderNPC.h"  // староста (предлагаемый квест)
#include "ContrarySurvivor/Actors/InteractableNPCInterface.h" // маркеры интерактивных NPC
#include "ContrarySurvivor/Actors/MasterEnemyBase.h" // Этап D: метка цели квеста (QuestMarkerTag базы)
#include "ContrarySurvivor/Controllers/EnemyAIController.h" // D6: стрелки на стрелков за кадром
#include "ContrarySurvivor/Save/ContrarySaveGame.h" // флаги сценки/сообщения конца сюжета (Build 1.2)
#include "ContrarySurvivor/UI/EndOfStoryWidget.h"   // плашка конца сюжета (Build 1.2)
#include "Engine/Texture2D.h" // иконки слотов брони (ADR-043)
#include "TimerManager.h"     // таймер задержки сообщения конца сюжета
#include "UObject/ConstructorHelpers.h" // Д2: мягкий FClassFinder WBP_EndOfStory

AContrarySurvivorHUD::AContrarySurvivorHUD()
{
	// Build 1.2.1 (Д2): плашка конца сюжета — WBP_EndOfStory, если ассет уже на диске
	// (генерируется cpp-2 отдельным шагом); нет — мягкий фолбэк на кодовый класс, плашка
	// работает как раньше. Образец — PickupBP в конструкторе APlayerCharacter.
	static ConstructorHelpers::FClassFinder<UEndOfStoryWidget> EndOfStoryBP(TEXT("/Game/UI/WBP_EndOfStory"));
	EndOfStoryWidgetClass = EndOfStoryBP.Succeeded()
		? TSubclassOf<UEndOfStoryWidget>(EndOfStoryBP.Class)
		: TSubclassOf<UEndOfStoryWidget>(UEndOfStoryWidget::StaticClass());
}

void AContrarySurvivorHUD::BeginPlay()
{
	Super::BeginPlay();

	// ADR-048: постоянные панели — один раз на старте, если слоты назначены. Виджеты
	// сами берут игрока/контроллер каждый кадр и сами прячутся на модалках — HUD их
	// больше не трогает. Z=5: под тач-слоем (10) и всеми окнами.
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}
	if (PlayerStatsWidgetClass && !PlayerStatsWidgetInstance)
	{
		PlayerStatsWidgetInstance = CreateWidget<UPlayerStatsWidget>(PC, PlayerStatsWidgetClass);
		if (PlayerStatsWidgetInstance)
		{
			PlayerStatsWidgetInstance->AddToViewport(/*ZOrder=*/5);
		}
	}
	if (QuestTrackerWidgetClass && !QuestTrackerWidgetInstance)
	{
		QuestTrackerWidgetInstance = CreateWidget<UQuestTrackerWidget>(PC, QuestTrackerWidgetClass);
		if (QuestTrackerWidgetInstance)
		{
			QuestTrackerWidgetInstance->AddToViewport(/*ZOrder=*/5);
		}
	}
	if (InteractPromptWidgetClass && !InteractPromptWidgetInstance)
	{
		InteractPromptWidgetInstance = CreateWidget<UInteractPromptWidget>(PC, InteractPromptWidgetClass);
		if (InteractPromptWidgetInstance)
		{
			InteractPromptWidgetInstance->AddToViewport(/*ZOrder=*/5);
		}
	}
}

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

	// D6 (ADR-035): красные краевые стрелки на врагов-стрелков за кадром.
	DrawOffscreenShooterArrows();

	// D5: всплывающие цифры урона по врагам (живут ~секунду, чистятся по возрасту).
	DrawDamageNumbers();

	// --- Статы игрока (GDD §7.7) ---
	if (PC)
	{
		if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PC->GetPawn()))
		{
			// Этап D: метка цели активного квеста (гаснет после сдачи).
			DrawQuestTargetMarker(PlayerChar);

			// Build 1 интро: задача сверху по центру + стрелка на деревню (активны только пока
			// контроллер их выставил во время интро; после — пусто, ничего не рисуется).
			DrawIntroObjective();
			DrawIntroDirectionMarker();

			// Build 1.2: страховка сообщения конца сюжета — разовая проверка флагов сейва
			// (внутри защита от повторного чтения слота).
			MaybeScheduleEndOfStoryFromSave(PlayerChar);

			// ADR-048: при назначенном PlayerStatsWidgetClass статы рисует UMG-панель.
			// Дефект с телефона 08-09: поверх главного меню оставался игровой интерфейс.
			// UMG-панель прячется сама (UPlayerStatsWidget::VisibilityForMainMenu), а здесь
			// тот же гейт для запасного Canvas-пути — иначе при пустом слоте полосы снова
			// оказались бы поверх меню.
			const AContrarySurvivorPlayerController* MenuPC =
				Cast<AContrarySurvivorPlayerController>(PC);
			const bool bMainMenuOnScreen = MenuPC && MenuPC->IsMainMenuOnScreen();
			if (!PlayerStatsWidgetInstance && !bMainMenuOnScreen)
			{
				DrawPlayerStats(PlayerChar);
			}

			// --- Трекер активного квеста («Волков: X/5») — GDD §7.7 ---
			// Только вне модальных экранов (на них квест виден в самом диалоге).
			// ADR-048: UMG-трекер прячется на модалках САМ (проверка внутри виджета).
			if (!QuestTrackerWidgetInstance
				&& !bInventoryOpen && !bShopOpen && !bDialogOpen && !bDeathScreen)
			{
				DrawQuestTracker(PlayerChar->GetQuests());
			}

			// --- Экран инвентаря поверх HUD (модальный, GDD §7.4) ---
			// ADR-048: при назначенном InventoryWidgetClass инвентарь рисует UMG-виджет.
			if (bInventoryOpen && !IsUmgInventoryActive())
			{
				DrawInventory(PlayerChar);
			}

			// --- Экран магазина поверх HUD (модальный, GDD §7.6) ---
			// ADR-048: при назначенном ShopWidgetClass магазин рисует UMG-виджет.
			if (bShopOpen && !IsUmgShopActive())
			{
				DrawShop(PlayerChar);
			}

			// --- Экран диалога со старостой (модальный, GDD §7.7) ---
			// ADR-048: при назначенном DialogWidgetClass диалог рисует UMG-виджет.
			if (bDialogOpen && !IsUmgDialogActive())
			{
				DrawDialog(PlayerChar);
			}

			// --- Экран смерти (#26): поверх всего, респаун по кнопке/клавише ---
			// ADR-048: при назначенном DeathWidgetClass экран смерти рисует UMG-виджет.
			if (bDeathScreen && !IsUmgDeathActive())
			{
				DrawDeathScreen(PlayerChar);
			}
		}
	}

	// --- Контекстная подсказка взаимодействия (E) — пикап/торговец/староста (BUG3) ---
	// Только когда модальные экраны закрыты (иначе перекрывает панель).
	// ADR-048: UMG-подсказка (InteractPromptWidgetInstance) прячется/показывается САМА.
	if (!InteractPromptWidgetInstance
		&& !bInventoryOpen && !bShopOpen && !bDialogOpen && !bDeathScreen)
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

bool AContrarySurvivorHUD::IsTouchLayerShown() const
{
	const AContrarySurvivorPlayerController* CSPC =
		Cast<AContrarySurvivorPlayerController>(GetOwningPlayerController());
	return CSPC && CSPC->HasTouchLayer();
}

void AContrarySurvivorHUD::DrawQADebugOverlay()
{
	// Служебный слой поверх экрана в публикационной сборке не рисуется НИКОГДА: тела нет,
	// компилятор выкидывает функцию целиком (Б5 задания издателя — «убрать с экрана
	// счётчик кадров и подписи клавиш ПК», сюда же относится и эта отладочная лента).
#if !CONTRARY_WITH_QA_CHEATS
	return;
#else
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
#endif // CONTRARY_WITH_QA_CHEATS
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

	// ADR-048: назначен InventoryWidgetClass — инвентарь живёт UMG-виджетом,
	// Canvas-путь глушится проверками IsUmgInventoryActive. Слот пуст — как раньше.
	if (bOpen && InventoryWidgetClass)
	{
		APlayerController* PC = GetOwningPlayerController();
		APlayerCharacter* PlayerChar = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
		if (PC && PlayerChar)
		{
			if (!InventoryWidgetInstance)
			{
				InventoryWidgetInstance = CreateWidget<UInventoryScreenWidget>(PC, InventoryWidgetClass);
				if (InventoryWidgetInstance)
				{
					// Кнопка закрытия = тот же путь, что клавиша Tab (тумблер контроллера).
					if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
					{
						InventoryWidgetInstance->OnCloseRequested.AddUObject(
							CSPC, &AContrarySurvivorPlayerController::TouchToggleInventory);
					}
				}
			}
			if (InventoryWidgetInstance)
			{
				InventoryWidgetInstance->InitInventory(PlayerChar);
				if (!InventoryWidgetInstance->IsInViewport())
				{
					// Z=30: как магазин — над тач-слоем (10), под подсказками (40)/паузой (60).
					InventoryWidgetInstance->AddToViewport(/*ZOrder=*/30);
				}
			}
		}
	}
	else if (!bOpen && InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
	{
		InventoryWidgetInstance->RemoveFromParent();
	}
}

bool AContrarySurvivorHUD::IsUmgInventoryActive() const
{
	return InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport();
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
	if (!bInventoryOpen || IsUmgInventoryActive()) // UMG-путь: клики ловят кнопки виджета
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

UTexture2D* AContrarySurvivorHUD::ResolveIcon(const TSoftObjectPtr<UTexture2D>& SoftIcon)
{
	if (SoftIcon.IsNull())
	{
		return nullptr; // ссылка не задана (например, у старой брони _01 иконки нет)
	}

	const FString Key = SoftIcon.ToSoftObjectPath().ToString();
	if (const TObjectPtr<UTexture2D>* Cached = IconCache.Find(Key))
	{
		return Cached->Get(); // и успех, и неудача закэшированы — повторных загрузок нет
	}

	// Единственная попытка загрузки за жизнь HUD. Текстуры может ещё не быть в проекте
	// (рисует художник, импорт позже) — тогда кэшируем nullptr и живём на текстовом фолбэке.
	UTexture2D* Loaded = SoftIcon.LoadSynchronous();
	IconCache.Add(Key, Loaded);
	if (!Loaded)
	{
		UE_LOG(LogTemp, Log, TEXT("HUD: icon '%s' not found (expected until art import) - text fallback."), *Key);
	}
	return Loaded;
}

void AContrarySurvivorHUD::DrawInvBox(float X, float Y, float W, float H, const FLinearColor& BaseColor,
	const FVector2D& MousePos, const FString& Label, UFont* Font)
{
	const bool bHover = (MousePos.X >= X && MousePos.X <= X + W && MousePos.Y >= Y && MousePos.Y <= Y + H);
	DrawRect(bHover ? InvHoverColor : BaseColor, X, Y, W, H);
	if (Font && !Label.IsEmpty())
	{
		// #18: текст плитки/кнопки с тенью+обводкой — читается на любом фоне плитки.
		float TH = 0.0f, TW = 0.0f;
		GetTextSize(TEXT("Ag"), TW, TH, Font);
		const float TextH = (TH > 0.0f ? TH : 14.0f) * UIBoxLabelScale;
		DrawShadowedText(Label, FLinearColor::White, X + 8.0f, Y + (H - TextH) * 0.5f, Font, UIBoxLabelScale);
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
	const float PanelW = FMath::Min(UIPanelMaxWidth, SX * UIPanelScreenFrac);
	const float PanelH = FMath::Min(UIPanelMaxHeight, SY * UIPanelScreenFrac);
	const float PX = (SX - PanelW) * 0.5f;
	const float PY = (SY - PanelH) * 0.5f;
	DrawRect(InvPanelColor, PX, PY, PanelW, PanelH);
	// #18: рамка-обводка панели (золотой акцент) — отделяет от сцены.
	DrawRectOutline(PX, PY, PanelW, PanelH, UIPanelBorderColor, UIPanelBorderThickness);

	const float Pad = UIPanelPadding;
	const float HeaderY = PY + Pad;
	// #18: крупный заголовок с обводкой. Русские подписи — ADR-041/ADR-043.
	// На телефоне заголовок без приписки про клавиши Tab/I (Б5 задания издателя).
	DrawShadowedText(IsTouchLayerShown() ? InvHeaderTextTouch : InvHeaderText,
		UIHeaderColor, PX + Pad, HeaderY, Font, UIHeaderTextScale);

	// Деньги / голод / жажда (GDD §7.7) — крупно, золотой, на плашке (#18).
	// Литералы: подписи переехали в UInventoryScreenWidget (ADR-048).
	if (UStatsComponent* St = Player->GetStats())
	{
		const FString StatStr = FString::Printf(TEXT("Монеты %.0f      Голод %.0f / %.0f      Жажда %.0f / %.0f"),
			St->GetMoney(), St->GetHunger(), St->GetSurvivalMax(), St->GetThirst(), St->GetSurvivalMax());
		DrawLabelWithPlate(StatStr, UIMoneyColor, PX + Pad, HeaderY + 30.0f, Font, UIMoneyTextScale);
	}

	const float ContentY = HeaderY + 64.0f;

	// --- Левая колонка: paper-doll (слоты брони + оружие) ---
	const float LeftW = PanelW * InvLeftColumnFrac;
	const float LeftX = PX + Pad;
	const float ColW = LeftW - Pad;
	DrawShadowedText(InvEquipmentHeaderText, UIHeaderColor, LeftX, ContentY, Font, UISubHeaderTextScale);

	float SlotY = ContentY + 24.0f;
	const float SlotH = InvSlotHeight;
	const float SlotGap = InvSlotGap;

	// Слот брони (ADR-043): иконка (надетого предмета или пустого слота) + подпись.
	// Иконки — мягкие ссылки, текстур может ещё не быть: тогда чисто текстовый вид (фолбэк).
	auto DrawArmorSlot = [&](const FString& Name, EArmorSlot Slot, const TSoftObjectPtr<UTexture2D>& EmptyIcon)
	{
		AArmor* Eq = Player->GetEquippedArmor(Slot);
		UTexture2D* Icon = ResolveIcon(Eq ? Eq->GetItemIcon() : EmptyIcon); // единый геттер иконок

		const FString Worn = Eq
			? (Eq->ItemName.IsEmpty() ? Eq->GetName() : Eq->ItemName)
			: FString(TEXT("(пусто)"));

		if (Icon)
		{
			// Плитка без текста, затем иконка квадратом по высоте слота и подпись правее.
			DrawInvBox(LeftX, SlotY, ColW, SlotH, Eq ? InvSlotFilledColor : InvSlotColor, Mouse, FString(), Font);
			const float IconPad = 4.0f;
			const float IconSize = SlotH - IconPad * 2.0f;
			DrawTexture(Icon, LeftX + IconPad, SlotY + IconPad, IconSize, IconSize,
				0.0f, 0.0f, 1.0f, 1.0f);
			float TH = 0.0f, TW = 0.0f;
			GetTextSize(TEXT("Ag"), TW, TH, Font);
			const float TextH = (TH > 0.0f ? TH : 14.0f) * UIBoxLabelScale;
			DrawShadowedText(FString::Printf(TEXT("%s: %s"), *Name, *Worn), FLinearColor::White,
				LeftX + IconPad * 2.0f + IconSize, SlotY + (SlotH - TextH) * 0.5f, Font, UIBoxLabelScale);
		}
		else
		{
			// Текстур ещё нет — прежний текстовый вид слота.
			DrawInvBox(LeftX, SlotY, ColW, SlotH, Eq ? InvSlotFilledColor : InvSlotColor, Mouse,
				FString::Printf(TEXT("%s: %s"), *Name, *Worn), Font);
		}

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

	DrawArmorSlot(InvSlotNameHead, EArmorSlot::Head, EmptySlotIconHead);
	DrawArmorSlot(InvSlotNameTorso, EArmorSlot::Torso, EmptySlotIconTorso);
	DrawArmorSlot(InvSlotNameLegs, EArmorSlot::Legs, EmptySlotIconLegs);

	// «Защита: N%» (ADR-043) — ФАКТИЧЕСКОЕ снижение урона (сумма слотов с потолком-капом).
	// Canvas рисует каждый кадр -> при надевании/снятии брони цифра пересчитывается сама.
	{
		const int32 ProtPct = FMath::RoundToInt(Player->GetEffectiveArmorFraction() * 100.0f);
		DrawLabelWithPlate(FString::Printf(TEXT("Защита: %d%%"), ProtPct), UIMoneyColor,
			LeftX, SlotY, Font, UIMoneyTextScale);
		SlotY += 34.0f;
	}

	// Слот оружия (только отображение CurrentWeapon).
	{
		AMasterWeapon* W = Player->GetCurrentWeapon();
		const FString Label = FString(TEXT("Оружие: ")) + (W ? W->GetName() : TEXT("(нет)"));
		DrawInvBox(LeftX, SlotY, ColW, SlotH, InvSlotColor, Mouse, Label, Font);
		SlotY += SlotH + SlotGap;
	}

	DrawShadowedText(InvUnequipHintText, InvHintTextColor, LeftX, SlotY, Font);

	// --- Правая колонка: рюкзак (неэкипированные предметы) ---
	const float RightX = LeftX + LeftW + Pad;
	const float RightW = (PX + PanelW - Pad) - RightX;
	DrawShadowedText(InvBackpackHeaderText, UIHeaderColor, RightX, ContentY, Font, UISubHeaderTextScale);

	float RowY = ContentY + 24.0f;
	const float RowH = UIRowHeight;
	const float RowGap = UIRowGap;
	const float DropW = InvDropButtonWidth;
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
				case EItemCategory::Consumable: ActionHint = TEXT("использовать"); break;
				case EItemCategory::Armor:      ActionHint = TEXT("надеть");       break;
				default:                        ActionHint = TEXT("");             break;
			}

			// Build 1.2.1 (стаки): у стака >1 к названию добавляется количество («Тушёнка x5»),
			// как в UMG-пути (InventoryScreenWidget::StackNameFormat).
			FString Name = Item->ItemName.IsEmpty() ? Item->GetName() : Item->ItemName;
			if (Item->GetStackCount() > 1)
			{
				Name = FString::Printf(TEXT("%s x%d"), *Name, Item->GetStackCount());
			}
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
			DrawInvBox(DropX, RowY, DropW, RowH, InvDropColor, Mouse, InvDropButtonText, Font);
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

void AContrarySurvivorHUD::SetShopOpen(bool bOpen, TScriptInterface<IShopVendor> Trader)
{
	bShopOpen = bOpen;
	ShopTrader = bOpen ? Trader : TScriptInterface<IShopVendor>();
	CancelShopSlider(); // закрытие/открытие магазина сбрасывает активную транзакцию
	ShopListScrollOffset = 0; // каждый визит к торговцу — списки с начала
	ShopSellScrollOffset = 0;
	ShopScrollAccumBuy = 0.0f;
	ShopScrollAccumSell = 0.0f;
	if (!bOpen)
	{
		ShopHitRegions.Reset();
	}

	// ADR-048: слот ShopWidgetClass назначен — магазин живёт UMG-виджетом, Canvas-путь
	// (DrawShop/клики/жесты) глушится проверками IsUmgShopActive. Слот пуст — всё как раньше.
	if (bOpen && ShopWidgetClass)
	{
		APlayerController* PC = GetOwningPlayerController();
		APlayerCharacter* PlayerChar = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
		if (PC && PlayerChar)
		{
			if (!ShopWidgetInstance)
			{
				ShopWidgetInstance = CreateWidget<UShopScreenWidget>(PC, ShopWidgetClass);
				if (ShopWidgetInstance)
				{
					// Close — закрывает контроллер (мир/режим ввода — его зона ответственности).
					if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
					{
						ShopWidgetInstance->OnCloseRequested.AddUObject(
							CSPC, &AContrarySurvivorPlayerController::CloseShop);
					}
				}
			}
			if (ShopWidgetInstance)
			{
				ShopWidgetInstance->InitShop(Trader, PlayerChar);
				if (!ShopWidgetInstance->IsInViewport())
				{
					// Z=30: над тач-слоем (10), под подсказками (40)/ежедневкой (50)/паузой (60).
					ShopWidgetInstance->AddToViewport(/*ZOrder=*/30);
				}
			}
		}
	}
	else if (!bOpen && ShopWidgetInstance && ShopWidgetInstance->IsInViewport())
	{
		ShopWidgetInstance->RemoveFromParent(); // экземпляр переиспользуется при следующем визите
	}
}

bool AContrarySurvivorHUD::IsUmgShopActive() const
{
	return ShopWidgetInstance && ShopWidgetInstance->IsInViewport();
}

void AContrarySurvivorHUD::SetCorpseLootOpen(bool bOpen, UCorpseLootComponent* Corpse)
{
	// Build 1.2.1 (ТЗ А1): окно обыска трупа. Canvas-пути нет: пустой слот класса —
	// создаём прямо из C++-класса (кодовое дерево-фолбэк UCorpseLootWidget).
	if (bOpen && Corpse)
	{
		APlayerController* PC = GetOwningPlayerController();
		APlayerCharacter* PlayerChar = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
		if (!PC || !PlayerChar)
		{
			return;
		}
		if (!CorpseLootWidgetInstance)
		{
			CorpseLootWidgetInstance = CorpseLootWidgetClass
				? CreateWidget<UCorpseLootWidget>(PC, CorpseLootWidgetClass)
				: CreateWidget<UCorpseLootWidget>(PC, UCorpseLootWidget::StaticClass());
			if (CorpseLootWidgetInstance)
			{
				// Закрытие (крестик / труп исчез): мир и режим ввода возвращает контроллер
				// (CloseCorpseLoot — тот же путь, что Esc). Слабая лямбда: HUD переживает
				// контроллер при травел-переходах.
				CorpseLootWidgetInstance->OnCloseRequested.AddWeakLambda(this, [this]()
				{
					if (AContrarySurvivorPlayerController* CSPC =
						Cast<AContrarySurvivorPlayerController>(GetOwningPlayerController()))
					{
						CSPC->CloseCorpseLoot();
					}
					else
					{
						SetCorpseLootOpen(false, nullptr); // фолбэк без нашего контроллера
					}
				});
			}
		}
		if (CorpseLootWidgetInstance)
		{
			CorpseLootWidgetInstance->InitCorpseLoot(Corpse, PlayerChar);
			if (!CorpseLootWidgetInstance->IsInViewport())
			{
				// Z=30: модальные окна (магазин/диалог) — тот же слой.
				CorpseLootWidgetInstance->AddToViewport(/*ZOrder=*/30);
			}
		}
	}
	else if (!bOpen && CorpseLootWidgetInstance && CorpseLootWidgetInstance->IsInViewport())
	{
		CorpseLootWidgetInstance->RemoveFromParent(); // экземпляр переиспользуется
	}
}

bool AContrarySurvivorHUD::IsCorpseLootOpen() const
{
	return CorpseLootWidgetInstance && CorpseLootWidgetInstance->IsInViewport();
}

void AContrarySurvivorHUD::ScrollShopList(int32 DeltaRows)
{
	if (!bShopOpen || IsUmgShopActive()) // UMG-путь: прокрутку делает штатный ScrollBox
	{
		return;
	}

	// Колесо листает колонку ПОД КУРСОРОМ: над рюкзаком — правый список (G2: длинный рюкзак
	// был недоступен для продажи и на ПК), иначе — каталог (прежнее поведение).
	bool bSellColumn = false;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		float MX = 0.0f, MY = 0.0f;
		if (PC->GetMousePosition(MX, MY))
		{
			bSellColumn = MX >= ShopSellAreaMin.X && MX <= ShopSellAreaMax.X &&
				MY >= ShopSellAreaMin.Y && MY <= ShopSellAreaMax.Y;
		}
	}

	if (bSellColumn)
	{
		ShopSellScrollOffset = FMath::Clamp(ShopSellScrollOffset + DeltaRows, 0, ShopSellMaxScroll);
	}
	else
	{
		ShopListScrollOffset = FMath::Clamp(ShopListScrollOffset + DeltaRows, 0, ShopListMaxScroll);
	}
}

EShopDragZone AContrarySurvivorHUD::GetShopDragZone(FVector2D ScreenPos) const
{
	if (!bShopOpen || IsUmgShopActive()) // UMG-путь: жесты обрабатывает Slate (ScrollBox/Slider)
	{
		return EShopDragZone::None;
	}

	// При активном слайдере списки закрыты модально — жест имеет смысл только на треке.
	// Зону трека расширяем по вертикали под палец (сам трек тонкий, ~34px с запасом клика).
	if (bSliderActive)
	{
		const float FingerPad = 16.0f;
		const bool bOnTrack = ScreenPos.X >= SliderTrackMin.X && ScreenPos.X <= SliderTrackMax.X &&
			ScreenPos.Y >= SliderTrackMin.Y - FingerPad && ScreenPos.Y <= SliderTrackMax.Y + FingerPad;
		return bOnTrack ? EShopDragZone::SliderTrack : EShopDragZone::None;
	}

	if (ScreenPos.X >= ShopBuyAreaMin.X && ScreenPos.X <= ShopBuyAreaMax.X &&
		ScreenPos.Y >= ShopBuyAreaMin.Y && ScreenPos.Y <= ShopBuyAreaMax.Y)
	{
		return EShopDragZone::BuyList;
	}
	if (ScreenPos.X >= ShopSellAreaMin.X && ScreenPos.X <= ShopSellAreaMax.X &&
		ScreenPos.Y >= ShopSellAreaMin.Y && ScreenPos.Y <= ShopSellAreaMax.Y)
	{
		return EShopDragZone::SellList;
	}
	return EShopDragZone::None;
}

void AContrarySurvivorHUD::ScrollShopZonePixels(EShopDragZone Zone, float DeltaPixels)
{
	if (!bShopOpen || IsUmgShopActive()
		|| (Zone != EShopDragZone::BuyList && Zone != EShopDragZone::SellList))
	{
		return;
	}

	const bool bBuy = (Zone == EShopDragZone::BuyList);
	float& Accum = bBuy ? ShopScrollAccumBuy : ShopScrollAccumSell;
	Accum += DeltaPixels;

	// Полные строки из накопленных пикселей; остаток живёт до следующей дельты жеста.
	const float Step = FMath::Max(1.0f, ShopRowStep);
	const int32 Rows = static_cast<int32>(Accum / Step);
	if (Rows == 0)
	{
		return;
	}
	Accum -= static_cast<float>(Rows) * Step;

	if (bBuy)
	{
		ShopListScrollOffset = FMath::Clamp(ShopListScrollOffset + Rows, 0, ShopListMaxScroll);
	}
	else
	{
		ShopSellScrollOffset = FMath::Clamp(ShopSellScrollOffset + Rows, 0, ShopSellMaxScroll);
	}
}

void AContrarySurvivorHUD::SetShopSliderQtyFromX(float ScreenX)
{
	if (!bShopOpen || !bSliderActive || IsUmgShopActive())
	{
		return;
	}
	// Та же математика, что у клика по треку (HandleShopClick, case SliderTrack).
	const float TrackW = FMath::Max(1.0f, SliderTrackMax.X - SliderTrackMin.X);
	const float Frac = FMath::Clamp((ScreenX - SliderTrackMin.X) / TrackW, 0.0f, 1.0f);
	const int32 NewQty = FMath::RoundToInt(Frac * static_cast<float>(FMath::Max(1, SliderQtyMax)));
	SliderQty = FMath::Clamp(NewQty, 1, FMath::Max(1, SliderQtyMax));
}

void AContrarySurvivorHUD::CancelShopSlider()
{
	bSliderActive = false;
	bSliderIsBuy = false;
	SliderEntryIndex = -1;
	SliderItem = nullptr;
	SliderQty = 1;
	SliderQtyMax = 1;
	SliderUnitPrice = 0.0f;
	SliderUnitAmmo = 0;
	SliderTitle.Reset();
}

void AContrarySurvivorHUD::ArmBuySlider(APlayerCharacter* Player, int32 EntryIndex)
{
	if (!Player || !ShopTrader)
	{
		return;
	}
	const TArray<FShopEntry>& Catalog = ShopTrader->GetCatalog();
	if (!Catalog.IsValidIndex(EntryIndex))
	{
		return;
	}
	const FShopEntry& E = Catalog[EntryIndex];

	bSliderActive = true;
	bSliderIsBuy = true;
	SliderEntryIndex = EntryIndex;
	SliderItem = nullptr;
	SliderUnitPrice = E.Price;
	SliderUnitAmmo = (E.Kind == EShopEntryKind::Ammo) ? FMath::Max(0, E.AmmoAmount) : 0;
	SliderTitle = E.DisplayName;

	// Потолок по деньгам: сколько единиц по цене может позволить игрок (минимум 1).
	const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
	int32 ByMoney = 999;
	if (E.Price > 0.0f)
	{
		ByMoney = FMath::FloorToInt(Money / E.Price);
	}
	SliderQtyMax = FMath::Clamp(ByMoney, 1, 999);
	SliderQty = 1;
}

void AContrarySurvivorHUD::ArmSellSlider(APlayerCharacter* Player, AMasterInventoryItem* Item)
{
	if (!Player || !ShopTrader || !IsValid(Item))
	{
		return;
	}

	// Слайдер количества имеет смысл только для СТАКА (Build 1.2.1, ТЗ Г: раньше — только
	// патроны, теперь любой стакаемый предмет: шкуры/тушёнка/аптечка). Нестакаемое продаём
	// сразу. Цена единицы: у патронов — спец-тариф за штуку, у прочих — GetSellValue
	// (тариф категории и есть цена ОДНОЙ штуки).
	if (!Item->IsStackable() || Item->GetStackCount() <= 1)
	{
		// Стак из одной штуки тоже уходит мгновенно (прежний UX Canvas-пути: панель
		// количества ради «1 из 1» не открываем; UMG-путь ведёт себя иначе — там окно
		// подтверждения нужно любой продаже из-за золотой кнопки).
		Player->Shop_SellItemQty(Item, Cast<AAmmoItem>(Item)
			? ShopTrader->GetAmmoSellPerRound() : ShopTrader->GetSellValue(Item), 1);
		return;
	}

	bSliderActive = true;
	bSliderIsBuy = false;
	SliderEntryIndex = -1;
	SliderItem = Item;
	SliderUnitPrice = Cast<AAmmoItem>(Item)
		? ShopTrader->GetAmmoSellPerRound() : ShopTrader->GetSellValue(Item);
	SliderUnitAmmo = 0;
	SliderTitle = Item->ItemName.IsEmpty() ? Item->GetItemDisplayText().ToString() : Item->ItemName;
	SliderQtyMax = FMath::Max(1, Item->GetStackCount());
	SliderQty = SliderQtyMax; // по умолчанию продать всё (как в STALKER — потом крутишь вниз)
}

void AContrarySurvivorHUD::AdjustShopSliderQty(int32 Delta)
{
	if (!bSliderActive)
	{
		return;
	}
	SliderQty = FMath::Clamp(SliderQty + Delta, 1, FMath::Max(1, SliderQtyMax));
}

void AContrarySurvivorHUD::ConfirmShopSlider(APlayerCharacter* Player)
{
	if (!bSliderActive || !Player)
	{
		CancelShopSlider();
		return;
	}

	const int32 Qty = FMath::Clamp(SliderQty, 1, FMath::Max(1, SliderQtyMax));

	if (bSliderIsBuy)
	{
		if (ShopTrader)
		{
			const TArray<FShopEntry>& Catalog = ShopTrader->GetCatalog();
			if (Catalog.IsValidIndex(SliderEntryIndex))
			{
				Player->Shop_BuyEntryQty(Catalog[SliderEntryIndex], Qty);
			}
		}
	}
	else if (IsValid(SliderItem))
	{
		Player->Shop_SellItemQty(SliderItem, SliderUnitPrice, Qty);
	}

	CancelShopSlider();
}

bool AContrarySurvivorHUD::HandleShopClick(FVector2D ScreenPos)
{
	if (!bShopOpen || !ShopTrader || IsUmgShopActive()) // UMG-путь: клики ловят кнопки виджета
	{
		return false;
	}

	APlayerController* PC = GetOwningPlayerController();
	APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		return false;
	}

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
				// Клик «Buy» арміт слайдер количества (не покупает сразу) — STALKER 2-стиль.
				ArmBuySlider(Player, R.EntryIndex);
				return true;
			case EShopAction::Sell:
				// Стак патронов -> слайдер; прочее -> прямая продажа (внутри ArmSellSlider).
				ArmSellSlider(Player, R.Item);
				return true;
			case EShopAction::Close:
				if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
				{
					CSPC->CloseShop();
				}
				return true;
			case EShopAction::SliderTrack:
			{
				// qty = round(max * (mouseX - trackX) / trackW), кламп 1..max.
				const float TrackW = FMath::Max(1.0f, SliderTrackMax.X - SliderTrackMin.X);
				const float Frac = FMath::Clamp((ScreenPos.X - SliderTrackMin.X) / TrackW, 0.0f, 1.0f);
				const int32 NewQty = FMath::RoundToInt(Frac * static_cast<float>(FMath::Max(1, SliderQtyMax)));
				SliderQty = FMath::Clamp(NewQty, 1, FMath::Max(1, SliderQtyMax));
				return true;
			}
			case EShopAction::SliderDec:
				AdjustShopSliderQty(-1);
				return true;
			case EShopAction::SliderInc:
				AdjustShopSliderQty(+1);
				return true;
			case EShopAction::SliderConfirm:
				ConfirmShopSlider(Player);
				return true;
			case EShopAction::SliderCancel:
				CancelShopSlider();
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
	const float PanelW = FMath::Min(UIPanelMaxWidth, SX * UIPanelScreenFrac);
	const float PanelH = FMath::Min(UIPanelMaxHeight, SY * UIPanelScreenFrac);
	const float PX = (SX - PanelW) * 0.5f;
	const float PY = (SY - PanelH) * 0.5f;
	DrawRect(InvPanelColor, PX, PY, PanelW, PanelH);
	// #18: рамка-обводка панели магазина (золотой акцент).
	DrawRectOutline(PX, PY, PanelW, PanelH, UIPanelBorderColor, UIPanelBorderThickness);

	const float Pad = UIPanelPadding;
	const float HeaderY = PY + Pad;
	// #18: крупный заголовок с обводкой.
	DrawShadowedText(ShopHeaderText, UIHeaderColor, PX + Pad, HeaderY, Font, UIHeaderTextScale);

	const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
	// Деньги — крупно, золотой, на плашке (#18). Литерал: поле переехало в UShopScreenWidget (ADR-048).
	DrawLabelWithPlate(FString::Printf(TEXT("Монеты %.0f"), Money), UIMoneyColor,
		PX + Pad, HeaderY + 30.0f, Font, UIMoneyTextScale);

	// Кнопка Close (правый верх панели).
	{
		const float CloseW = ShopCloseButtonWidth, CloseH = ShopCloseButtonHeight;
		const float CX = PX + PanelW - Pad - CloseW;
		const float CY = HeaderY;
		DrawInvBox(CX, CY, CloseW, CloseH, InvDropColor, Mouse, ShopCloseButtonText, Font);
		if (!bSliderActive) // при активном слайдере списки/кнопки за ним не кликаются (модально)
		{
			FShopHitRegion R;
			R.Min = FVector2D(CX, CY);
			R.Max = FVector2D(CX + CloseW, CY + CloseH);
			R.Action = EShopAction::Close;
			ShopHitRegions.Add(R);
		}
	}

	const float ContentY = HeaderY + 64.0f;
	const float RowH = UIRowHeight;
	const float RowGap = UIRowGap;
	const float BtnW = ShopRowButtonWidth;
	const float MaxRowY = PY + PanelH - Pad - RowH;

	// Шаг строки для конвертации пикселей свайпа в строки (ScrollShopZonePixels, G2).
	ShopRowStep = RowH + RowGap;

	// Подсказка прокрутки: на таче — про свайп, на ПК — про колесо (по наличию тач-слоя).
	const AContrarySurvivorPlayerController* CSPC =
		Cast<AContrarySurvivorPlayerController>(GetOwningPlayerController());
	const FString& ScrollHintTail = (CSPC && CSPC->HasTouchLayer()) ? ShopScrollHintSwipe : ShopScrollHintWheel;

	// --- Левая колонка: каталог на продажу (BUY) ---
	const float LeftW = PanelW * ShopLeftColumnFrac;
	const float LeftX = PX + Pad;
	const float LeftColW = LeftW - Pad;
	DrawShadowedText(ShopBuyHeaderText, UIHeaderColor, LeftX, ContentY, Font, UISubHeaderTextScale);

	float RowY = ContentY + 24.0f;
	const TArray<FShopEntry>& Catalog = ShopTrader->GetCatalog();

	// Прокрутка каталога: строка рисуется, если её верх <= MaxRowY, отсюда вместимость
	// панели и потолок смещения. Кламп здесь же — размер окна/каталога мог измениться.
	const int32 VisibleRows = FMath::Max(1, FMath::FloorToInt((MaxRowY - RowY) / (RowH + RowGap)) + 1);
	ShopListMaxScroll = FMath::Max(0, Catalog.Num() - VisibleRows);
	ShopListScrollOffset = FMath::Clamp(ShopListScrollOffset, 0, ShopListMaxScroll);

	// Зона свайпа каталога (G2): вся левая колонка по высоте видимых строк.
	ShopBuyAreaMin = FVector2D(LeftX, RowY);
	ShopBuyAreaMax = FVector2D(LeftX + LeftColW, MaxRowY + RowH);

	// Когда список длиннее панели — счётчик «X-Y из N» + подсказка, справа от заголовка FOR SALE.
	if (ShopListMaxScroll > 0)
	{
		const int32 FirstShown = ShopListScrollOffset + 1;
		const int32 LastShown = FMath::Min(ShopListScrollOffset + VisibleRows, Catalog.Num());
		const FString ScrollHint = FString::Printf(TEXT("%d-%d из %d %s"),
			FirstShown, LastShown, Catalog.Num(), *ScrollHintTail);
		float HintW = 0.0f, HintH = 0.0f;
		GetTextSize(ScrollHint, HintW, HintH, Font);
		DrawShadowedText(ScrollHint, UIHeaderColor, LeftX + LeftColW - HintW, ContentY, Font);
	}

	for (int32 i = ShopListScrollOffset; i < Catalog.Num(); ++i)
	{
		const FShopEntry& E = Catalog[i];
		const float MainW = LeftColW - BtnW - 6.0f;

		// ADR-043: у позиций брони показываем прибавку защиты «+N%» ДО покупки — значение
		// из CDO класса предмета (там же живёт тюнингуемый ArmorProtection).
		FString Label;
		if (E.ItemClass && E.ItemClass->IsChildOf(AArmor::StaticClass()))
		{
			const AArmor* ArmorCDO = GetDefault<AArmor>(E.ItemClass);
			const int32 AddPct = FMath::RoundToInt(ArmorCDO->GetArmorProtection() * 100.0f);
			Label = FString::Printf(TEXT("%s  (+%d%% защиты)  -  %.0f"), *E.DisplayName, AddPct, E.Price);
		}
		else
		{
			Label = FString::Printf(TEXT("%s  -  %.0f"), *E.DisplayName, E.Price);
		}
		DrawInvBox(LeftX, RowY, MainW, RowH, InvSlotColor, Mouse, Label, Font);

		// Кнопка [Buy] — зелёная, если хватает денег, иначе тускло-красная.
		const float BtnX = LeftX + MainW + 6.0f;
		const bool bAfford = (Money >= E.Price);
		DrawInvBox(BtnX, RowY, BtnW, RowH, bAfford ? InvSlotFilledColor : InvDropColor, Mouse, TEXT("Buy"), Font);
		if (bAfford && !bSliderActive)
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
			break; // ниже панели не рисуем; остальное доступно колесом (ScrollShopList)
		}
	}

	// --- Правая колонка: рюкзак на продажу (SELL) ---
	const float RightX = LeftX + LeftW + Pad;
	const float RightW = (PX + PanelW - Pad) - RightX;
	DrawShadowedText(ShopSellHeaderText, UIHeaderColor, RightX, ContentY, Font, UISubHeaderTextScale);

	float SellY = ContentY + 24.0f;

	// G2: рюкзак тоже перерастает панель — прокрутка как у каталога. Продаваемые предметы
	// собираются заранее (фильтр прежний: валидный и не надет), чтобы посчитать потолок
	// смещения и рисовать окно списка со сдвига ShopSellScrollOffset.
	TArray<AMasterInventoryItem*> SellItems;
	if (UInventoryComponent* Inv = Player->GetInventory())
	{
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (IsValid(Item) && !Inv->IsItemEquipped(Item))
			{
				SellItems.Add(Item); // надетую броню не продаём из этого списка
			}
		}
	}

	const int32 SellVisibleRows = FMath::Max(1, FMath::FloorToInt((MaxRowY - SellY) / (RowH + RowGap)) + 1);
	ShopSellMaxScroll = FMath::Max(0, SellItems.Num() - SellVisibleRows);
	ShopSellScrollOffset = FMath::Clamp(ShopSellScrollOffset, 0, ShopSellMaxScroll);

	// Зона свайпа рюкзака (G2): вся правая колонка по высоте видимых строк.
	ShopSellAreaMin = FVector2D(RightX, SellY);
	ShopSellAreaMax = FVector2D(RightX + RightW, MaxRowY + RowH);

	// Счётчик «X-Y из N» над списком, когда рюкзак длиннее панели (подсказка прокрутки —
	// одна на панель, у каталога; здесь только диапазон, заголовок SELL длинный).
	if (ShopSellMaxScroll > 0)
	{
		const FString SellCounter = FString::Printf(TEXT("%d-%d из %d"),
			ShopSellScrollOffset + 1,
			FMath::Min(ShopSellScrollOffset + SellVisibleRows, SellItems.Num()),
			SellItems.Num());
		float CounterW = 0.0f, CounterH = 0.0f;
		GetTextSize(SellCounter, CounterW, CounterH, Font);
		DrawShadowedText(SellCounter, UIHeaderColor, RightX + RightW - CounterW, ContentY, Font);
	}

	for (int32 i = ShopSellScrollOffset; i < SellItems.Num(); ++i)
	{
		AMasterInventoryItem* Item = SellItems[i];

		const float MainW = RightW - BtnW - 6.0f;
		const float SellVal = ShopTrader->GetSellValue(Item);
		// Build 1.2.1 (стаки): у стака >1 показываем количество («Шкура волка x3»).
		FString Name = Item->ItemName.IsEmpty() ? Item->GetName() : Item->ItemName;
		if (Item->GetStackCount() > 1)
		{
			Name = FString::Printf(TEXT("%s x%d"), *Name, Item->GetStackCount());
		}
		const FString Label = FString::Printf(TEXT("%s  (+%.0f)"), *Name, SellVal);
		DrawInvBox(RightX, SellY, MainW, RowH, InvSlotColor, Mouse, Label, Font);

		const float BtnX = RightX + MainW + 6.0f;
		DrawInvBox(BtnX, SellY, BtnW, RowH, InvSlotFilledColor, Mouse, TEXT("Sell"), Font);
		if (!bSliderActive)
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
			break; // ниже панели не рисуем; остальное доступно прокруткой
		}
	}

	// Поверх списков — панель слайдера количества (если идёт транзакция купли/продажи стака).
	if (bSliderActive)
	{
		DrawShopSlider(Player, Mouse, Font, SX, SY);
	}
}

// ---------------------------------------------------------------------------
// Слайдер количества купли-продажи (immediate-mode, STALKER 2-стиль) — Фаза 5
// ---------------------------------------------------------------------------

void AContrarySurvivorHUD::DrawShopSlider(APlayerCharacter* Player, const FVector2D& Mouse,
	UFont* Font, float SX, float SY)
{
	// Компактная модальная панель по центру экрана.
	const float PW = FMath::Min(SliderPanelMaxWidth, SX * SliderPanelScreenFrac);
	const float PH = SliderPanelHeight;
	const float PXc = (SX - PW) * 0.5f;
	const float PYc = (SY - PH) * 0.5f;

	// Затемнение под панелью + сама панель + рамка-обводка (#18).
	DrawRect(InvDimColor, 0.0f, 0.0f, SX, SY);
	DrawRect(InvPanelColor, PXc, PYc, PW, PH);
	DrawRectOutline(PXc, PYc, PW, PH, UIPanelBorderColor, UIPanelBorderThickness);

	const float Pad = SliderPanelPadding;
	float Y = PYc + Pad;

	// Заголовок: что и в каком режиме (крупно, обводка). Литералы: поля переехали
	// в UShopScreenWidget (ADR-048), Canvas-путь доживает до выпила.
	{
		const FString Mode = bSliderIsBuy ? TEXT("КУПИТЬ") : TEXT("ПРОДАТЬ");
		DrawShadowedText(FString::Printf(TEXT("%s:  %s"), *Mode, *SliderTitle),
			UIHeaderColor, PXc + Pad, Y, Font, UISliderTitleScale);
	}
	Y += 38.0f;

	// Кламп qty на случай, если max изменился.
	SliderQty = FMath::Clamp(SliderQty, 1, FMath::Max(1, SliderQtyMax));

	// Строка количества + (для патронов) сколько это патронов — КРУПНОЕ число, на плашке (#18).
	{
		FString QtyLine = FString::Printf(TEXT("Кол-во: %d / %d"), SliderQty, SliderQtyMax);
		if (SliderUnitAmmo > 0)
		{
			QtyLine += FString::Printf(TEXT("   (= %d ammo)"), SliderQty * SliderUnitAmmo);
		}
		DrawLabelWithPlate(QtyLine, SliderQtyColor, PXc + Pad, Y, Font, UISliderQtyScale);
	}
	Y += 38.0f;

	// --- Трек слайдера ---
	const float TrackX = PXc + Pad;
	const float TrackW = PW - Pad * 2.0f;
	const float TrackY = Y + 10.0f;
	const float TrackH = SliderTrackHeight;
	SliderTrackMin = FVector2D(TrackX, TrackY - 8.0f);          // расширяем зону клика по вертикали
	SliderTrackMax = FVector2D(TrackX + TrackW, TrackY + TrackH + 8.0f);

	DrawRect(InvSlotColor, TrackX, TrackY, TrackW, TrackH); // фон трека
	const float Frac = (SliderQtyMax > 1)
		? static_cast<float>(SliderQty - 1) / static_cast<float>(SliderQtyMax - 1)
		: 1.0f;
	const float FillW = TrackW * Frac;
	DrawRect(InvSlotFilledColor, TrackX, TrackY, FillW, TrackH); // заполнение до ручки
	// Ручка.
	const float HandleW = 12.0f;
	const float HandleX = FMath::Clamp(TrackX + FillW - HandleW * 0.5f, TrackX, TrackX + TrackW - HandleW);
	DrawRect(FLinearColor::White, HandleX, TrackY - 6.0f, HandleW, TrackH + 12.0f);

	// Зона клика по треку.
	{
		FShopHitRegion R;
		R.Min = SliderTrackMin;
		R.Max = SliderTrackMax;
		R.Action = EShopAction::SliderTrack;
		ShopHitRegions.Add(R);
	}
	Y = TrackY + TrackH + 20.0f;

	// --- Кнопки [-] [+] ---
	const float SmallW = SliderSmallButtonWidth, SmallH = SliderSmallButtonHeight;
	DrawInvBox(TrackX, Y, SmallW, SmallH, InvSlotColor, Mouse, TEXT("-"), Font);
	{
		FShopHitRegion R; R.Min = FVector2D(TrackX, Y); R.Max = FVector2D(TrackX + SmallW, Y + SmallH);
		R.Action = EShopAction::SliderDec; ShopHitRegions.Add(R);
	}
	const float PlusX = TrackX + SmallW + 8.0f;
	DrawInvBox(PlusX, Y, SmallW, SmallH, InvSlotColor, Mouse, TEXT("+"), Font);
	{
		FShopHitRegion R; R.Min = FVector2D(PlusX, Y); R.Max = FVector2D(PlusX + SmallW, Y + SmallH);
		R.Action = EShopAction::SliderInc; ShopHitRegions.Add(R);
	}

	// --- Живая итоговая цена — крупно, золотой, на плашке (#18) ---
	const float Total = SliderUnitPrice * static_cast<float>(SliderQty);
	{
		const FString PriceStr = bSliderIsBuy
			? FString::Printf(TEXT("Итого: %.0f"), Total)
			: FString::Printf(TEXT("Выручка: +%.0f"), Total);
		DrawLabelWithPlate(PriceStr, UIMoneyColor, PlusX + SmallW + 24.0f, Y + 2.0f, Font, UISliderPriceScale);
	}

	// --- Кнопки [Confirm] [Cancel] (правый нижний угол панели) ---
	const float BtnW = SliderBigButtonWidth, BtnH = SliderBigButtonHeight;
	const float BtnY = PYc + PH - Pad - BtnH;
	const float ConfirmX = PXc + PW - Pad - BtnW;
	const float CancelX = ConfirmX - BtnW - 10.0f;

	DrawInvBox(CancelX, BtnY, BtnW, BtnH, InvDropColor, Mouse, SliderCancelText, Font);
	{
		FShopHitRegion R; R.Min = FVector2D(CancelX, BtnY); R.Max = FVector2D(CancelX + BtnW, BtnY + BtnH);
		R.Action = EShopAction::SliderCancel; ShopHitRegions.Add(R);
	}
	DrawInvBox(ConfirmX, BtnY, BtnW, BtnH, InvSlotFilledColor, Mouse, SliderConfirmText, Font);
	{
		FShopHitRegion R; R.Min = FVector2D(ConfirmX, BtnY); R.Max = FVector2D(ConfirmX + BtnW, BtnY + BtnH);
		R.Action = EShopAction::SliderConfirm; ShopHitRegions.Add(R);
	}

	// Подсказка по клавишам (стрелки/колесо ±1, Shift ±10). На телефоне клавиатуры нет:
	// берём тач-вариант, и если он пуст (так по умолчанию) — не рисуем строку вовсе
	// (Б5 задания издателя).
	const FString& KeysHint = IsTouchLayerShown() ? SliderKeysHintTextTouch : SliderKeysHintText;
	if (!KeysHint.IsEmpty())
	{
		DrawShadowedText(KeysHint, SliderKeysHintColor, PXc + Pad, BtnY + 8.0f, Font);
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

	// ADR-048: назначен DialogWidgetClass — диалог живёт UMG-виджетом, Canvas-путь
	// глушится проверками IsUmgDialogActive. Слот пуст — как раньше.
	if (bOpen && DialogWidgetClass && Elder)
	{
		APlayerController* PC = GetOwningPlayerController();
		APlayerCharacter* PlayerChar = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
		if (PC && PlayerChar)
		{
			if (!DialogWidgetInstance)
			{
				DialogWidgetInstance = CreateWidget<UDialogScreenWidget>(PC, DialogWidgetClass);
				if (DialogWidgetInstance)
				{
					// [Отказаться]/[Закрыть] — закрывает контроллер (мир/режим ввода — его зона).
					if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
					{
						DialogWidgetInstance->OnCloseRequested.AddUObject(
							CSPC, &AContrarySurvivorPlayerController::CloseDialog);
					}
				}
			}
			if (DialogWidgetInstance)
			{
				DialogWidgetInstance->InitDialog(Elder, PlayerChar);
				if (!DialogWidgetInstance->IsInViewport())
				{
					// Z=30: как магазин/инвентарь.
					DialogWidgetInstance->AddToViewport(/*ZOrder=*/30);
				}
			}
		}
	}
	else if (!bOpen && DialogWidgetInstance && DialogWidgetInstance->IsInViewport())
	{
		DialogWidgetInstance->RemoveFromParent();
	}
}

bool AContrarySurvivorHUD::IsUmgDialogActive() const
{
	return DialogWidgetInstance && DialogWidgetInstance->IsInViewport();
}

bool AContrarySurvivorHUD::HandleDialogClick(FVector2D ScreenPos)
{
	if (!bDialogOpen || !DialogElder || IsUmgDialogActive()) // UMG-путь: клики ловят кнопки виджета
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
	const FQuest& ActiveQuest = DialogElder->GetQuestForPlayer(Quests);
	const FName QuestId = ActiveQuest.QuestId;

	// Когда актуальный квест сменился ВНУТРИ той же сессии диалога (сдали кв.1 -> старостой
	// предлагается кв.2), новый квест ещё мог не попасть в журнал: OfferQuest вызывается только
	// при ОТКРЫТИИ диалога. Без записи в журнал AcceptQuest не найдёт квест (FindQuestMutable ==
	// nullptr) и [Принять] «не прожмётся» до переоткрытия диалога. Предлагаем актуальный квест в
	// журнал прямо здесь (идемпотентно — если он уже там, ничего не меняется).
	if (Quests)
	{
		Quests->OfferQuest(ActiveQuest);
	}

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

	// Определяем текст и кнопки по состоянию АКТУАЛЬНОГО квеста (по порядку) в журнале игрока.
	// Считается ДО геометрии панели: её высота зависит от длины реплики (см. ниже).
	const FQuest& Offered = DialogElder->GetQuestForPlayer(Player->GetQuests());
	// Синхронизируем журнал с актуальным квестом старосты КАЖДЫЙ кадр отрисовки: если внутри той же
	// сессии диалога квест сменился (сдали кв.1 -> предлагается кв.2), запись в журнал появится сразу,
	// и кнопка [Принять] сработает с первого клика без переоткрытия диалога. Идемпотентно.
	if (UQuestComponent* PlayerQuests = Player->GetQuests())
	{
		PlayerQuests->OfferQuest(Offered);
	}
	const FQuest* InLog = Player->GetQuests() ? Player->GetQuests()->FindQuest(Offered.QuestId) : nullptr;
	const EQuestState State = InLog ? InLog->State : EQuestState::NotStarted;
	// Источник данных квеста: журнал (если уже в нём) либо предложение старосты (ещё не принят).
	const FQuest& QData = InLog ? *InLog : Offered;

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

	// Реплика старосты (зависит от состояния). Тексты — EditAnywhere-поля AElderNPC
	// (директива Рината 07-18): у каждого размещённого старосты могут быть свои.
	FString NPCText;
	switch (State)
	{
		case EQuestState::NotStarted:
			NPCText = Offered.Description.ToString(); // полное описание задания
			break;
		case EQuestState::Active:
			// Сборка кодом (формат в редактор не отдаём): Prefix + название + « — » + прогресс + «.»
			NPCText = FString::Printf(TEXT("%s%s — %s."),
				*DialogElder->GetDialogueActivePrefix().ToString(), *QData.Title.ToString(), *ObjStr);
			break;
		case EQuestState::Completed:
			NPCText = DialogElder->GetDialogueCompletedText().ToString();
			break;
		case EQuestState::TurnedIn:
			NPCText = DialogElder->GetDialogueTurnedInText().ToString();
			break;
		default:
			break;
	}

	// Геометрия панели — ОТ СОДЕРЖИМОГО (фикс отрисовки, фидбек Рината 07-12: при малом окне
	// длинная реплика кв.3 налезала на кнопки и уходила под низ панели фиксированной высоты).
	// Реплика заранее разбивается на строки; высота панели = шапка + все строки + кнопки,
	// пол — DialogMinPanelHeight (короткие реплики), потолок — DialogMaxHeightFrac экрана.
	const float Pad = DialogPadding;
	const float PanelW = FMath::Min(DialogPanelMaxWidth, SX * DialogPanelScreenFrac);
	const float BtnH = DialogButtonHeight;
	const float TextTop = Pad + 36.0f;         // высота шапки с именем NPC и отступом
	const float TextToButtonsGap = 12.0f;

	TArray<FString> ReplicaLines;
	const float ReplicaLineStep = WrapTextIntoLines(NPCText, Font, PanelW - Pad * 2.0f,
		/*ScaleXY=*/1.0f, ReplicaLines);

	const float PanelHNeeded = TextTop + ReplicaLines.Num() * ReplicaLineStep
		+ TextToButtonsGap + BtnH + Pad;
	const float PanelH = FMath::Clamp(PanelHNeeded, FMath::Min(DialogMinPanelHeight, SY * 0.4f), SY * DialogMaxHeightFrac);
	const float PX = (SX - PanelW) * 0.5f;
	const float PY = SY - PanelH - DialogBottomMargin;

	// Затемнение фона + нижняя панель диалога (как в визуальных новеллах).
	DrawRect(InvDimColor, 0.0f, 0.0f, SX, SY);
	DrawRect(InvPanelColor, PX, PY, PanelW, PanelH);
	// #18: рамка-обводка панели диалога.
	DrawRectOutline(PX, PY, PanelW, PanelH, UIPanelBorderColor, UIPanelBorderThickness);

	// Заголовок — имя NPC (крупно, обводка; имя — поле старосты).
	DrawShadowedText(DialogElder->GetDialogueDisplayName().ToString(), DialogNameColor, PX + Pad, PY + Pad, Font, UIHeaderTextScale);

	// Кнопки-ответы (внизу панели); их верхняя граница — жёсткий предел отрисовки реплики.
	const float BtnY = PY + PanelH - Pad - BtnH;
	const float BtnGap = 14.0f;

	// Реплика старосты: те же строки, по которым считалась высота панели. Стоп у границы
	// кнопок — сработает, только если PanelHNeeded упёрся в потолок 55% экрана.
	float TextY = PY + TextTop;
	for (const FString& Line : ReplicaLines)
	{
		if (TextY + ReplicaLineStep > BtnY - 4.0f)
		{
			break;
		}
		DrawShadowedText(Line, DialogTextColor, PX + Pad, TextY, Font);
		TextY += ReplicaLineStep;
	}

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
			const float BtnW = DialogButtonWidth;
			AddButton(BtnX, BtnW, DialogAcceptText, EDialogAction::Accept, InvSlotFilledColor);
			AddButton(BtnX + BtnW + BtnGap, BtnW, DialogDeclineText, EDialogAction::Decline, InvDropColor);
			break;
		}
		case EQuestState::Active:
		{
			AddButton(BtnX, DialogButtonWidth, DialogCloseText, EDialogAction::Close, InvSlotColor);
			break;
		}
		case EQuestState::Completed:
		{
			// Литералы: префиксы кнопки сдачи переехали в UDialogScreenWidget (ADR-048).
			const FString TurnInLabel = FString::Printf(TEXT("[ Сдать (+%.0f) ]"), QData.RewardMoney);
			AddButton(BtnX, DialogTurnInButtonWidth, TurnInLabel, EDialogAction::TurnIn, InvSlotFilledColor);
			break;
		}
		case EQuestState::TurnedIn:
		{
			AddButton(BtnX, DialogButtonWidth, DialogCloseText, EDialogAction::Close, InvSlotColor);
			break;
		}
		default:
			break;
	}
}

void AContrarySurvivorHUD::DrawIntroObjective()
{
	if (IntroObjectiveText.IsEmpty() || !Canvas)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const FString Text = IntroObjectiveText.ToString();
	float TextW = 0.0f, TextH = 0.0f;
	GetTextSize(Text, TextW, TextH, Font);

	// Вверху по центру (ТЗ: задача вверху по центру). Плашка+цвет — как у трекера квеста.
	const float X = static_cast<float>(Canvas->SizeX) * 0.5f - TextW * 0.5f;
	const float Y = static_cast<float>(Canvas->SizeY) * 0.06f;

	DrawRect(QuestTrackerPlateColor, X - 12.0f, Y - 6.0f, TextW + 24.0f, TextH + 12.0f);
	DrawText(Text, QuestTrackerColor, X, Y, Font);
}

void AContrarySurvivorHUD::DrawIntroDirectionMarker()
{
	AActor* Target = IntroDirectionTarget.Get();
	if (!Target || !Canvas)
	{
		return;
	}
	// Build 1.2: цель-NPC (староста на шаге «Найти старосту») уже получает свой ИМЕННОЙ маркер
	// в DrawInteractiveNPCMarkers — безымянный ромб поверх был бы дублем.
	if (bNPCMarkersOnlyWhenNeeded && Target->Implements<UInteractableNPCInterface>())
	{
		return;
	}
	// Реюз системы маркеров NPC: маркер над целью + краевая стрелка, если цель за кадром.
	// Д3: тот же зелёный тип NPC — и картинка его же (пуста — прежний ромб).
	DrawNPCMarker(Target->GetActorLocation() + FVector(0.0f, 0.0f, QuestTargetMarkerZOffset),
		FString(), NPCMarkerColor, /*bEdgeArrowOnly=*/false, ResolveIcon(NPCMarkerTexture));
}

// ===========================================================================
// Сообщение о конце сюжета (Build 1.2, задача Рината 07-31)
// ===========================================================================

void AContrarySurvivorHUD::NotifyStoryEpilogueFinished()
{
	// Сценку доиграли только что — «показано ли уже» берём из сейва (страховка от повторов
	// при пересоздании HUD в той же сессии).
	bool bShownInSave = false;
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(PC->GetPawn()))
		{
			if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
			{
				bShownInSave = Save->bEndOfStoryShown;
			}
		}
	}
	ScheduleEndOfStoryMessage(/*bEpilogueSeen=*/true, bShownInSave);
}

void AContrarySurvivorHUD::MaybeScheduleEndOfStoryFromSave(APlayerCharacter* Player)
{
	if (bEndOfStorySaveChecked || !Player || !bEndOfStoryMessageEnabled)
	{
		return;
	}
	bEndOfStorySaveChecked = true; // одно чтение слота за жизнь HUD (зовёмся из DrawHUD)

	if (const UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
	{
		ScheduleEndOfStoryMessage(Save->bElderNotebookHintShown, Save->bEndOfStoryShown);
	}
}

void AContrarySurvivorHUD::ScheduleEndOfStoryMessage(bool bEpilogueSeen, bool bShownInSave)
{
	if (!bEndOfStoryMessageEnabled)
	{
		return;
	}
	if (!EndOfStoryGate.ShouldSchedule(bEpilogueSeen, bShownInSave))
	{
		return;
	}
	GetWorldTimerManager().SetTimer(EndOfStoryTimer, this,
		&AContrarySurvivorHUD::ShowEndOfStoryMessage,
		FMath::Max(0.01f, EndOfStoryDelaySeconds), /*bLoop=*/false);
	UE_LOG(LogQA, Display, TEXT("QA: end-of-story message scheduled in %.0f s"), EndOfStoryDelaySeconds);
}

void AContrarySurvivorHUD::ShowEndOfStoryMessage()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	// Флаг «показано» — в сейв ПРИ показе: смерть/перезапуск сообщение не повторят
	// (задача: один раз за сохранение).
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(PC->GetPawn()))
	{
		if (UContrarySaveGame* Save = Player->LoadOrCreateSaveObject())
		{
			if (Save->bEndOfStoryShown)
			{
				return; // другой путь успел показать раньше
			}
			Save->bEndOfStoryShown = true;
			Player->WriteSaveObject(Save);
		}
	}

	if (!EndOfStoryWidgetInstance)
	{
		// Д2 (Build 1.2.1): класс из слота EndOfStoryWidgetClass — WBP_EndOfStory, когда
		// ассет сгенерирован (дефолт ставит конструктор); пустой слот в BP — кодовый класс.
		EndOfStoryWidgetInstance = CreateWidget<UEndOfStoryWidget>(PC,
			EndOfStoryWidgetClass ? *EndOfStoryWidgetClass : UEndOfStoryWidget::StaticClass());
	}
	if (!EndOfStoryWidgetInstance)
	{
		return;
	}
	EndOfStoryWidgetInstance->ApplyStyle(EndOfStoryStyle);
	// Адрес канала берём из конфига (UEndOfStorySettings), а не с HUD: его вписывают позже
	// текстовым редактором, без пересборки кода и без переделки контента.
	EndOfStoryWidgetInstance->InitContent(EndOfStoryMessageText, EndOfStoryWriteButtonText,
		EndOfStoryPlayButtonText, EndOfStoryChannelPendingText, UEndOfStorySettings::GetChannelUrl());
	if (!EndOfStoryWidgetInstance->IsInViewport())
	{
		// Над модальными окнами (30), под экраном смерти (35) и интро (50); при открытой
		// модалке плашка сама прячет содержимое (SelfHiding) и возвращается после.
		EndOfStoryWidgetInstance->AddToViewport(/*ZOrder=*/34);
	}

	// Кнопки должны кликаться: режим ввода Game+UI (как у диалога), мир НЕ на паузе (решение
	// Рината). Открыта модалка — у неё уже свой режим, не трогаем.
	if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
	{
		if (!CSPC->IsAnyModalUIOpen())
		{
			FInputModeGameAndUI Mode;
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false);
			CSPC->SetInputMode(Mode);
			CSPC->bShowMouseCursor = true;
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: end-of-story message shown"));
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
		// Литералы: тексты переехали в UQuestTrackerWidget (ADR-048).
		Text = FString::Printf(TEXT("Квест выполнен: %s (%s) - вернись к старосте"),
			*Tracked->Title.ToString(), *ObjStr);
	}
	else
	{
		Text = FString::Printf(TEXT("Квест: %s — %s"), *Tracked->Title.ToString(), *ObjStr);
	}

	float TextW = 0.0f, TextH = 0.0f;
	GetTextSize(Text, TextW, TextH, Font);

	// Правый верхний угол под отступом.
	const float X = SX - TextW - 28.0f;
	const float Y = 28.0f;

	// Фоновая плашка для читаемости.
	DrawRect(QuestTrackerPlateColor, X - 8.0f, Y - 4.0f, TextW + 16.0f, TextH + 8.0f);
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

	// ADR-048: назначен DeathWidgetClass — экран смерти живёт UMG-виджетом.
	if (bOpen && DeathWidgetClass)
	{
		APlayerController* PC = GetOwningPlayerController();
		APlayerCharacter* PlayerChar = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
		if (PC && PlayerChar)
		{
			if (!DeathWidgetInstance)
			{
				DeathWidgetInstance = CreateWidget<UDeathScreenWidget>(PC, DeathWidgetClass);
			}
			if (DeathWidgetInstance)
			{
				DeathWidgetInstance->InitDeath(PlayerChar);
				if (!DeathWidgetInstance->IsInViewport())
				{
					// Z=35: поверх модальных окон (30) — смерть замещает всё, под ежедневкой/паузой.
					DeathWidgetInstance->AddToViewport(/*ZOrder=*/35);
				}
			}
		}
	}
	else if (!bOpen && DeathWidgetInstance && DeathWidgetInstance->IsInViewport())
	{
		DeathWidgetInstance->RemoveFromParent();
	}
}

bool AContrarySurvivorHUD::IsUmgDeathActive() const
{
	return DeathWidgetInstance && DeathWidgetInstance->IsInViewport();
}

bool AContrarySurvivorHUD::HandleDeathScreenClick(FVector2D ScreenPos)
{
	if (!bDeathScreen || IsUmgDeathActive()) // UMG-путь: возрождение — кнопка виджета
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
	const FString& Title = DeathTitleText;
	float TitleW = 0.0f, TitleH = 0.0f;
	if (Font)
	{
		GetTextSize(Title, TitleW, TitleH, Font);
	}
	const float TitleScale = DeathTitleScale;
	const float TitleX = (SX - TitleW * TitleScale) * 0.5f;
	const float TitleY = SY * DeathTitleYFrac;
	DrawShadowedText(Title, DeathTitleColor, TitleX, TitleY, Font, TitleScale);

	// --- Статистика последней жизни ---
	const float LifeSec = Player->GetLastLifeDuration();
	const int32 Minutes = FMath::FloorToInt(LifeSec / 60.0f);
	const int32 Seconds = FMath::FloorToInt(LifeSec) % 60;
	const float Money = Player->GetStats() ? Player->GetStats()->GetMoney() : 0.0f;
	const int32 QuestsDone = Player->GetQuests() ? Player->GetQuests()->GetTurnedInQuestCount() : 0;
	const int32 Kills = Player->GetEnemyKillCount();

	// Литералы: префиксы переехали в UDeathScreenWidget (ADR-048).
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Прожито:  %02d:%02d"), Minutes, Seconds));
	Lines.Add(FString::Printf(TEXT("Убийца:  %s"), *Player->GetLastDamagerName().ToString()));
	Lines.Add(FString::Printf(TEXT("Монеты:  %.0f"), Money));
	Lines.Add(FString::Printf(TEXT("Квестов выполнено:  %d"), QuestsDone));
	Lines.Add(FString::Printf(TEXT("Врагов убито:  %d"), Kills));

	const float StatScale = DeathStatScale;
	float LineH = 26.0f;
	if (Font)
	{
		float PW = 0.0f, PH = 0.0f;
		GetTextSize(TEXT("Ag"), PW, PH, Font);
		LineH = (PH > 0.0f ? PH : 18.0f) * StatScale + 10.0f;
	}

	float StatY = SY * DeathStatsYFrac;
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

	// --- A4/ADR-027: штраф смерти (правка Рината: показываем ВСЕГДА — штраф безусловный) ---
	// Текст финальный, согласован с макетом docs/contrary-survivor/ui/death-penalty-popup.final.png.
	{
		StatY += 14.0f;
		const float PenScale = DeathPenaltyScale;

		auto DrawDeathPenaltyLine = [&](const FString& Text, const FLinearColor& Color)
		{
			float TW = 0.0f, TH = 0.0f;
			if (Font) { GetTextSize(Text, TW, TH, Font); }
			const float TX = (SX - TW * PenScale) * 0.5f;
			DrawShadowedText(Text, Color, TX, StatY, Font, PenScale);
			StatY += LineH;
		};

		// Процент монет берём из игрока (DeathMoneyLossFraction), чтобы текст не расходился с
		// фактическим штрафом при смене параметра в BP. Строка собирается кодом:
		// Prefix + процент + Suffix = «−40% монет — …» (формат не в редакторе).
		const int32 MoneyLossPct = FMath::RoundToInt(Player->GetDeathMoneyLossFraction() * 100.0f);
		const FString MoneyPenaltyLine = FString::Printf(
			TEXT("−%d%% монет — часть монет утрачена при гибели."), MoneyLossPct);

		DrawDeathPenaltyLine(DeathRespawnLine, DeathStatColor);
		DrawDeathPenaltyLine(MoneyPenaltyLine, DeathPenaltyColor);
		// ADR-044 п.3 (дополнение Рината): БЕЗ «их можно забрать» — портит атмосферу.
		DrawDeathPenaltyLine(DeathConsumablesLine, DeathStatColor);
		DrawDeathPenaltyLine(DeathSavedLine, DeathSavedColor);
	}

	// --- Кнопка «Возродиться» (рисованный прямоугольник + hit-test) ---
	const float BtnW = FMath::Min(DeathButtonMaxWidth, SX * 0.5f);
	const float BtnH = DeathButtonHeight;
	const float BtnX = (SX - BtnW) * 0.5f;
	const float BtnY = StatY + 24.0f;

	const bool bHover =
		Mouse.X >= BtnX && Mouse.X <= BtnX + BtnW && Mouse.Y >= BtnY && Mouse.Y <= BtnY + BtnH;
	DrawRect(bHover ? DeathButtonHoverColor : DeathButtonColor, BtnX, BtnY, BtnW, BtnH);

	const FString& BtnLabel = DeathRespawnButtonText;
	float BLW = 0.0f, BLH = 0.0f;
	if (Font)
	{
		GetTextSize(BtnLabel, BLW, BLH, Font);
	}
	const float BtnScale = DeathButtonTextScale;
	DrawShadowedText(BtnLabel, FLinearColor::White,
		BtnX + (BtnW - BLW * BtnScale) * 0.5f, BtnY + (BtnH - BLH * BtnScale) * 0.5f, Font, BtnScale);

	// Запоминаем прямоугольник кнопки для hit-теста (CU-мышь по HUD).
	DeathRespawnBtnMin = FVector2D(BtnX, BtnY);
	DeathRespawnBtnMax = FVector2D(BtnX + BtnW, BtnY + BtnH);

	// Подсказка-клавиша (дублирование, т.к. клик мышью по HUD ненадёжен). На телефоне
	// клавиш нет — тач-вариант по умолчанию пуст, и строка не рисуется (Б5 задания издателя).
	const FString& Hint = IsTouchLayerShown() ? DeathKeyHintTextTouch : DeathKeyHintText;
	if (!Hint.IsEmpty())
	{
		float HW = 0.0f, HH = 0.0f;
		if (Font)
		{
			GetTextSize(Hint, HW, HH, Font);
		}
		DrawShadowedText(Hint, DeathKeyHintColor,
			(SX - HW) * 0.5f, BtnY + BtnH + 16.0f, Font, 1.0f);
	}
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

void AContrarySurvivorHUD::DrawLabelWithPlate(const FString& Text, const FLinearColor& Color,
	float X, float Y, UFont* Font, float ScaleXY)
{
	if (!Canvas || !Font || Text.IsEmpty())
	{
		return;
	}
	// Плашка по фактическому размеру текста (с учётом масштаба) + паддинг — текст читается
	// поверх любой сцены/панели (#18).
	float TW = 0.0f, TH = 0.0f;
	GetTextSize(Text, TW, TH, Font);
	const float PadX = 7.0f;
	const float PadY = 3.0f;
	DrawRect(UITextPlateColor, X - PadX, Y - PadY, TW * ScaleXY + PadX * 2.0f, TH * ScaleXY + PadY * 2.0f);
	DrawShadowedText(Text, Color, X, Y, Font, ScaleXY);
}

float AContrarySurvivorHUD::WrapTextIntoLines(const FString& Text, UFont* Font, float MaxWidth,
	float ScaleXY, TArray<FString>& OutLines)
{
	OutLines.Reset();
	if (!Canvas || !Font || Text.IsEmpty() || MaxWidth <= 0.0f)
	{
		return 0.0f;
	}

	// Высота строки — по фактической метрике шрифта (не хардкод под кегль).
	float LineW = 0.0f, LineH = 0.0f;
	GetTextSize(TEXT("Ag"), LineW, LineH, Font);
	const float LineStep = LineH * ScaleXY + 4.0f;

	// Перенос по словам: копим строку, пока следующая влезает в MaxWidth; слово длиннее
	// строки идёт как есть (обрезки/дефисов не делаем — для реплик диалога не нужно).
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "), /*CullEmpty=*/true);

	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float CW = 0.0f, CH = 0.0f;
		GetTextSize(Candidate, CW, CH, Font);
		if (CW * ScaleXY > MaxWidth && !Line.IsEmpty())
		{
			OutLines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		OutLines.Add(Line);
	}
	return LineStep;
}

float AContrarySurvivorHUD::DrawWrappedText(const FString& Text, const FLinearColor& Color,
	float X, float Y, UFont* Font, float MaxWidth, float ScaleXY)
{
	TArray<FString> Lines;
	const float LineStep = WrapTextIntoLines(Text, Font, MaxWidth, ScaleXY, Lines);
	for (const FString& Line : Lines)
	{
		DrawShadowedText(Line, Color, X, Y, Font, ScaleXY);
		Y += LineStep;
	}
	return Y;
}

void AContrarySurvivorHUD::DrawRectOutline(float X, float Y, float W, float H,
	const FLinearColor& Color, float Thickness)
{
	if (!Canvas)
	{
		return;
	}
	DrawLine(X, Y, X + W, Y, Color, Thickness);            // верх
	DrawLine(X, Y + H, X + W, Y + H, Color, Thickness);    // низ
	DrawLine(X, Y, X, Y + H, Color, Thickness);            // лево
	DrawLine(X + W, Y, X + W, Y + H, Color, Thickness);    // право
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

	// --- HP-бар (слева вверху, GDD §7.7; #18: крупнее + текст с обводкой) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, PlayerHealthBarHeight);
	const float HpFillWidth = PlayerHealthBarWidth * FMath::Clamp(Stats->GetHealthPercent(), 0.0f, 1.0f);
	if (HpFillWidth > 0.0f)
	{
		DrawRect(PlayerHealthFillColor, BarX, CurY, HpFillWidth, PlayerHealthBarHeight);
	}
	// Литералы: префиксы переехали в UPlayerStatsWidget (ADR-048).
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
	ARangedWeapon* Ranged = Cast<ARangedWeapon>(Player->GetCurrentWeapon());
	// Тот же защитный гейт, что в PlayerStatsWidget/TouchControlsWidget (находка лида 08-05):
	// «в руках» обязано быть ИМЕННО отслеживаемым стволом слота, не любым ARangedWeapon.
	if (Ranged && Ranged != Player->GetRangedWeaponInstance())
	{
		Ranged = nullptr;
	}
	if (Ranged)
	{
		// Обойма / резерв оружия + (в рюкзаке) — патроны как стак-предмет (Фаза 5).
		const FString AmmoStr = FString::Printf(TEXT("Ammo %d / %d  (bag %d)"),
			Ranged->GetCurrentAmmoInClip(), Ranged->GetCurrentAmmoReserve(),
			Player->GetReserveAmmoInInventory());
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
	const FString MoneyStr = FString::Printf(TEXT("Монеты %.0f"), Stats->GetMoney());
	float MoneyW = 0.0f, MoneyH = 0.0f;
	if (Font)
	{
		GetTextSize(MoneyStr, MoneyW, MoneyH, Font);
	}
	DrawRect(MoneyPlateColor, BarX - 4.0f, CurY - 2.0f, MoneyW + 16.0f, MoneyH + 6.0f);
	DrawShadowedText(MoneyStr, PlayerMoneyColor, BarX + 4.0f, CurY, Font);
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

	// Build 1.2.1 (ТЗ Д3): своя картинка задана — рисуем ЕЙ (квадрат 2H x 2H по центру
	// цели, без подкраски) вместо скобок и треугольника; пуста/не загрузилась — прежние
	// фигуры ниже. ResolveIcon кэширует и успех, и неудачу (без загрузок каждый кадр).
	if (UTexture2D* MarkerTex = ResolveIcon(TargetMarkerTexture))
	{
		DrawTexture(MarkerTex, CX - H, CY - H, 2.0f * H, 2.0f * H,
			0.0f, 0.0f, 1.0f, 1.0f);
		return;
	}

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

	// Build 1.2 («маркеры по необходимости», задача Рината 07-31): в новом режиме маркер
	// рисуется ТОЛЬКО над NPC, к которому сейчас ведёт навигация первых шагов, — это цель
	// стрелки интро (староста на шаге «Найти старосту», синхронно с сообщением вверху экрана).
	// Навигация кончилась (игрок заговорил со старостой — OpenDialog очистил цель) — постоянных
	// зелёных маркеров больше нет; дальше работают только квестовые метки (DrawQuestTargetMarker).
	const AActor* RequiredNPC = bNPCMarkersOnlyWhenNeeded ? IntroDirectionTarget.Get() : nullptr;
	if (bNPCMarkersOnlyWhenNeeded && !RequiredNPC)
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
		if (bNPCMarkersOnlyWhenNeeded && Actor != RequiredNPC)
		{
			continue; // не цель текущего шага навигации — маркер не нужен
		}

		const IInteractableNPCInterface* NPC = Cast<IInteractableNPCInterface>(Actor);
		const float ZOff = NPC ? NPC->GetNPCMarkerZOffset() : 240.0f;
		// Старый путь рисования печатает строкой; сам текст уже переводимый (ADR-050).
		const FString Label = NPC ? NPC->GetNPCMarkerLabel().ToString() : FString();

		// Д3: своя картинка типа NPC (пуста — прежний ромб). Кэш загрузки — ResolveIcon.
		DrawNPCMarker(Actor->GetActorLocation() + FVector(0.0f, 0.0f, ZOff), Label, NPCMarkerColor,
			/*bEdgeArrowOnly=*/false, ResolveIcon(NPCMarkerTexture));
	}
}

void AContrarySurvivorHUD::DrawOffscreenShooterArrows()
{
	UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}

	// Поверх модальных экранов стрелки не нужны (как маркеры NPC).
	if (bInventoryOpen || bShopOpen || bDialogOpen || bDeathScreen)
	{
		return;
	}

	// D6 (ADR-035): стрелок, ведущий бой ЗА КАДРОМ, помечается красной краевой стрелкой —
	// игрок знает, откуда прилетит, а правило честности уже не даёт стрелять из-за кадра.
	for (const TWeakObjectPtr<AEnemyAIController>& Ptr : AEnemyAIController::GetActiveControllers())
	{
		const AEnemyAIController* Enemy = Ptr.Get();
		if (!Enemy || Enemy->GetWorld() != World || !Enemy->IsRangedThreat())
		{
			continue;
		}
		const APawn* EnemyPawn = Enemy->GetPawn();
		if (!EnemyPawn)
		{
			continue;
		}
		// bEdgeArrowOnly: в кадре у врага и так есть хелсбар — рисуем только за кадром.
		DrawNPCMarker(EnemyPawn->GetActorLocation(), FString(), EnemyShooterArrowColor,
			/*bEdgeArrowOnly=*/true);
	}
}

void AContrarySurvivorHUD::DrawQuestTargetMarker(APlayerCharacter* Player)
{
	UWorld* World = GetWorld();
	if (!World || !Canvas || !Player)
	{
		return;
	}

	// Поверх модальных экранов метка не нужна (как маркеры NPC).
	if (bInventoryOpen || bShopOpen || bDialogOpen || bDeathScreen)
	{
		return;
	}

	UQuestComponent* Quests = Player->GetQuests();
	const FQuest* Tracked = Quests ? Quests->GetTrackedQuest() : nullptr;
	if (!Tracked || Tracked->MapMarkerTag.IsNone())
	{
		return; // нет активного квеста с меткой (после сдачи GetTrackedQuest = null — метка гаснет)
	}

	// Фикс меты (фидбек Рината 07-05): пока цели квеста НЕ выполнены — метка на цель (базу);
	// квест ГОТОВ К СДАЧЕ (Completed: все убиты/собрано) — метка ведёт к квестодателю (старосте).
	const bool bToGiver = (Tracked->State == EQuestState::Completed);

	// Кэш цели (qa-фикс): мир сканируется только при инвалидации кэша (смена тега/фазы, гибель
	// актора), НЕ каждый кадр; повторный поиск ненайденной цели дросселируется по времени.
	AActor* Target = QuestMarkerTargetCache.Get();
	if (!Target || QuestMarkerCachedTag != Tracked->MapMarkerTag || bQuestMarkerCachedToGiver != bToGiver)
	{
		const float Now = World->GetTimeSeconds();
		const bool bSameSearch = (QuestMarkerCachedTag == Tracked->MapMarkerTag
			&& bQuestMarkerCachedToGiver == bToGiver);
		if (!Target && bSameSearch && Now < QuestMarkerNextSearchTime)
		{
			return; // недавно искали и не нашли (цель не размещена/тег не проставлен) — ждём
		}

		Target = nullptr;
		if (bToGiver)
		{
			// Квестодатель MVP — староста (единственный AElderNPC на карте).
			for (TActorIterator<AElderNPC> It(World); It; ++It)
			{
				Target = *It;
				break;
			}
		}
		else
		{
			// Цель: база врагов с совпадающим QuestMarkerTag (BP_WolfDen/BP_BanditBase)…
			for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
			{
				if (It->GetQuestMarkerTag() == Tracked->MapMarkerTag)
				{
					Target = *It;
					break;
				}
			}
			// …или ЛЮБОЙ актор со стандартным Actor Tag (гибкость для будущих квестов без правок кода).
			if (!Target)
			{
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					if (IsValid(*It) && It->ActorHasTag(Tracked->MapMarkerTag))
					{
						Target = *It;
						break;
					}
				}
			}
		}

		QuestMarkerTargetCache = Target;
		QuestMarkerCachedTag = Tracked->MapMarkerTag;
		bQuestMarkerCachedToGiver = bToGiver;
		QuestMarkerNextSearchTime = Now + 0.5f; // дроссель следующего поиска, если не нашли
	}

	if (!Target)
	{
		return; // цель не размещена/тег не проставлен — метки нет (лог не спамим: каждый кадр)
	}

	// К сдаче метка висит над старостой ВЫШЕ его зелёного NPC-ромба (чтобы не сливались),
	// подпись — «Сдать: <квест>»; на цели — обычный подъём и ТЕКУЩАЯ невыполненная цель.
	float ZOff = QuestTargetMarkerZOffset;
	FString Label = Tracked->Title.ToString(); // фолбэк: целей с подписью нет — название квеста
	if (bToGiver)
	{
		if (const IInteractableNPCInterface* NPC = Cast<IInteractableNPCInterface>(Target))
		{
			ZOff = NPC->GetNPCMarkerZOffset() + 120.0f;
		}
		Label = FString::Printf(TEXT("%s%s"), *QuestTurnInMarkerPrefix, *Tracked->Title.ToString());
	}
	else
	{
		// Текст метки = ПЕРВАЯ невыполненная цель квеста (фидбек Рината 07-06), меняется по
		// ходу: «Перебить бандитов (1/3)» -> «Забрать ноутбук». Порядок целей kill -> item —
		// тот же, что в трекере верхнего правого угла (DrawQuestTracker). Не хардкод под
		// конкретный квест: подписи берутся из данных квеста (FQuest::*ObjectiveLabel).
		if (Tracked->TargetCount > 0 && Tracked->Progress < Tracked->TargetCount)
		{
			const FString Obj = !Tracked->KillObjectiveLabel.IsEmpty()
				? Tracked->KillObjectiveLabel.ToString() : Tracked->KillTargetTag.ToString();
			Label = FString::Printf(TEXT("%s (%d/%d)"), *Obj, Tracked->Progress, Tracked->TargetCount);
		}
		else if (Tracked->RequiredItemCount > 0 && Tracked->ItemProgress < Tracked->RequiredItemCount)
		{
			const FString Obj = !Tracked->ItemObjectiveLabel.IsEmpty()
				? Tracked->ItemObjectiveLabel.ToString() : Tracked->RequiredItemName;
			// Счётчик (x/N) у единичной цели («Забрать ноутбук») — шум, не показываем.
			Label = (Tracked->RequiredItemCount > 1)
				? FString::Printf(TEXT("%s (%d/%d)"), *Obj, Tracked->ItemProgress, Tracked->RequiredItemCount)
				: Obj;
		}
	}

	// Д3: своя картинка типа Quest (пуста — прежний золотой ромб).
	DrawNPCMarker(Target->GetActorLocation() + FVector(0.0f, 0.0f, ZOff), Label, QuestTargetMarkerColor,
		/*bEdgeArrowOnly=*/false, ResolveIcon(QuestTargetMarkerTexture));
}

void AContrarySurvivorHUD::DrawNPCMarker(const FVector& WorldAnchor, const FString& Label,
	const FLinearColor& Color, bool bEdgeArrowOnly, UTexture2D* IconTexture)
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
		if (!bEdgeArrowOnly)
		{
			DrawNPCIcon(P, Label, Color, IconTexture);
		}
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
	DrawNPCEdgeArrow(Edge, Dir.GetSafeNormal(), Color);
}

void AContrarySurvivorHUD::DrawNPCIcon(const FVector2D& ScreenPos, const FString& Label, const FLinearColor& Color,
	UTexture2D* IconTexture)
{
	const float CX = ScreenPos.X;
	const float CY = ScreenPos.Y;
	const float H = NPCMarkerHalfSize;
	const float T = NPCMarkerThickness;
	const FLinearColor C = Color;

	// Build 1.2.1 (ТЗ Д3): картинка задана — рисуем ЕЙ (квадрат 2H x 2H, без подкраски)
	// вместо ромба; подпись под маркером ниже — как раньше.
	if (IconTexture)
	{
		DrawTexture(IconTexture, CX - H, CY - H, 2.0f * H, 2.0f * H,
			0.0f, 0.0f, 1.0f, 1.0f);
	}
	else
	{
		// Ромб (повёрнутый квадрат) — форма, отличная от углового ретикла врага.
		DrawLine(CX, CY - H, CX + H, CY, C, T); // верх -> право
		DrawLine(CX + H, CY, CX, CY + H, C, T); // право -> низ
		DrawLine(CX, CY + H, CX - H, CY, C, T); // низ -> лево
		DrawLine(CX - H, CY, CX, CY - H, C, T); // лево -> верх
	}

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

void AContrarySurvivorHUD::DrawNPCEdgeArrow(const FVector2D& EdgePos, const FVector2D& Dir, const FLinearColor& Color)
{
	const FLinearColor C = Color;
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

// ===========================================================================
// Всплывающие цифры урона (D5) — только по врагам (решение Рината)
// ===========================================================================

void AContrarySurvivorHUD::AddDamageNumber(const FVector& WorldLocation, float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FDamageNumberEntry Entry;
	// Небольшой случайный сдвиг по XY, чтобы серия попаданий не сливалась в одну точку.
	Entry.WorldLocation = WorldLocation + FVector(
		FMath::FRandRange(-15.0f, 15.0f), FMath::FRandRange(-15.0f, 15.0f), 0.0f);
	Entry.Amount = Amount;
	Entry.SpawnTime = World->GetTimeSeconds();

	// Страховка от разрастания (спам-урон): старейшие выкидываем.
	if (DamageNumbers.Num() > 50)
	{
		DamageNumbers.RemoveAt(0);
	}
	DamageNumbers.Add(Entry);
}

void AContrarySurvivorHUD::DrawDamageNumbers()
{
	if (!Canvas || DamageNumbers.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!World || !Font)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float Lifetime = FMath::Max(0.1f, DamageNumberLifetime);

	// Старение: чистим отжившие (и «из будущего» после смены мира — защита от мусора).
	for (int32 i = DamageNumbers.Num() - 1; i >= 0; --i)
	{
		const float Age = Now - DamageNumbers[i].SpawnTime;
		if (Age > Lifetime || Age < 0.0f)
		{
			DamageNumbers.RemoveAtSwap(i);
		}
	}

	// Поверх модальных экранов цифры не рисуем (но возраст выше уже посчитан — истекут сами).
	if (bInventoryOpen || bShopOpen || bDialogOpen || bDeathScreen)
	{
		return;
	}

	for (const FDamageNumberEntry& Entry : DamageNumbers)
	{
		const float Age = Now - Entry.SpawnTime;
		const float LifeT = FMath::Clamp(Age / Lifetime, 0.0f, 1.0f);

		// Подъём вверх в мировых координатах + затухание к концу жизни.
		const FVector WorldPos = Entry.WorldLocation
			+ FVector(0.0f, 0.0f, DamageNumberZOffset + DamageNumberRiseSpeed * Age);
		const FVector Screen = Project(WorldPos, false);
		if (Screen.Z <= 0.0f)
		{
			continue; // за камерой
		}

		const float Alpha = 1.0f - LifeT;
		FLinearColor Color = DamageNumberColor;
		Color.A *= Alpha;

		const FString Text = FString::Printf(TEXT("%.0f"), Entry.Amount);
		float TW = 0.0f, TH = 0.0f;
		GetTextSize(Text, TW, TH, Font);

		// Не через DrawShadowedText: тень/обводка там с фиксированной альфой — при затухании
		// цифры оставался бы «призрак» обводки. Здесь альфа применяется ко всем слоям.
		FCanvasTextItem Item(
			FVector2D(Screen.X - TW * DamageNumberTextScale * 0.5f, Screen.Y),
			FText::FromString(Text), Font, Color);
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f * Alpha), FVector2D(1.5f, 1.5f));
		Item.bOutlined = true;
		Item.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f * Alpha);
		Item.Scale = FVector2D(DamageNumberTextScale, DamageNumberTextScale);
		Canvas->DrawItem(Item);
	}
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
