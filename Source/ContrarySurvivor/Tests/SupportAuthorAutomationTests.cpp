// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты окна «Поддержать автора» (задание издателя, решение Рината
// 11.08.2026). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.SupportAuthor; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается (всё это видно без живой игры):
//   * пункт «Поддержать автора» стоит в главном меню строго после «Настройки» и перед
//     «Сообщество» и виден ВСЕГДА, без порогов и условий;
//   * при неготовом ролике кнопка просмотра прячется ЦЕЛИКОМ (не серая и не с надписью);
//   * порог по игровому времени к этой точке не применяется: он не пройден, а кнопка есть;
//   * после просмотра ролика игроку НИЧЕГО не начисляется — проверяем на живом персонаже,
//     сверяя деньги, здоровье, голод, жажду и содержимое рюкзака до и после;
//   * область нажатия не мельче 48 точек (требование Б8);
//   * имена событий замера совпадают с именами из задания издателя.
//
// НЕ покрывается headless: вид окна, реальные нажатия и сам показ ролика — это живая
// проверка на устройстве.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Ads/AdService.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "ContrarySurvivor/UI/SupportAuthorWidget.h"
#include "UInventoryComponent.h"
#include "Blueprint/UserWidget.h"    // CreateWidget: окно из живого ассета дизайнера
#include "Components/Widget.h"       // перебор полей-кубиков по типу
#include "Components/Border.h"       // снимок цвета подложки
#include "Components/Button.h"       // снимок заливки кнопки
#include "Components/CanvasPanelSlot.h" // снимок положения и размера в холсте
#include "Components/TextBlock.h"    // снимок цвета, кегля и текста подписей
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"      // FObjectPropertyBase: проверка привязки с обеих сторон

static constexpr EAutomationTestFlags SupportAuthorTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (тот же приём «свой мир на файл теста», что в остальных тестах).
namespace SupportAuthorTestWorld
{
	static UWorld* Create()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			return nullptr;
		}
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(World);

		const FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			WorldSettings->NotifyBeginPlay(); // мир без режима игры сам begun-play не ставит
		}
		return World;
	}

	static void Destroy(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}
}

// --- 1. Место пункта в главном меню ---------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportMenuRowOrderTest,
	"ContrarySurvivor.SupportAuthor.MenuRowStandsBetweenSettingsAndCommunity", SupportAuthorTestFlags)

bool FSupportMenuRowOrderTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Order = UStartScreenWidget::GetMenuRowOrder();

	const int32 SettingsIndex = Order.IndexOfByKey(FName(TEXT("SettingsButton")));
	const int32 SupportIndex = Order.IndexOfByKey(FName(TEXT("SupportButton")));
	const int32 CommunityIndex = Order.IndexOfByKey(FName(TEXT("CommunityButton")));

	if (!TestTrue(TEXT("Пункт «Настройки» в меню есть"), SettingsIndex != INDEX_NONE)
		|| !TestTrue(TEXT("Пункт «Поддержать автора» в меню есть"), SupportIndex != INDEX_NONE)
		|| !TestTrue(TEXT("Пункт «Сообщество» в меню есть"), CommunityIndex != INDEX_NONE))
	{
		return false;
	}

	// Дословно из задания издателя: строго после «Настройки» и перед «Сообщество».
	TestTrue(TEXT("«Поддержать автора» стоит ПОСЛЕ «Настройки»"), SupportIndex > SettingsIndex);
	TestTrue(TEXT("«Поддержать автора» стоит ПЕРЕД «Сообщество»"), SupportIndex < CommunityIndex);
	TestEqual(TEXT("«Поддержать автора» стоит сразу за «Настройки», без чужих пунктов между ними"),
		SupportIndex, SettingsIndex + 1);
	TestEqual(TEXT("Сразу за ним идёт «Сообщество»"), CommunityIndex, SupportIndex + 1);

	// Пункт заведён ровно один раз.
	int32 Repeats = 0;
	for (const FName& Row : Order)
	{
		if (Row == FName(TEXT("SupportButton")))
		{
			++Repeats;
		}
	}
	TestEqual(TEXT("Пункт заведён ровно один раз"), Repeats, 1);
	return true;
}

// --- 2. Пункт виден всегда ------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportMenuRowAlwaysVisibleTest,
	"ContrarySurvivor.SupportAuthor.MenuRowIsAlwaysVisible", SupportAuthorTestFlags)

bool FSupportMenuRowAlwaysVisibleTest::RunTest(const FString& Parameters)
{
	// У этого пункта нет ни порога, ни условий в настройках: он есть всегда.
	TestEqual(TEXT("Пункт «Поддержать автора» виден"),
		UStartScreenWidget::SupportVisibilityFor(), ESlateVisibility::Visible);

	// Для сравнения: соседний пункт «Сообщество» при пустом адресе прячется. Это показывает,
	// что «виден всегда» — осознанное отличие, а не совпадение.
	TestEqual(TEXT("Соседний пункт «Сообщество» без адреса прячется"),
		UStartScreenWidget::CommunityVisibilityFor(FString()), ESlateVisibility::Collapsed);
	return true;
}

// --- 3. Кнопка просмотра при неготовом ролике -----------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportWatchAdHiddenWhenNotReadyTest,
	"ContrarySurvivor.SupportAuthor.WatchAdButtonDisappearsWhenAdNotReady", SupportAuthorTestFlags)

bool FSupportWatchAdHiddenWhenNotReadyTest::RunTest(const FString& Parameters)
{
	// Дословно из задания: «Не серая, не с надписью "недоступно" — её просто нет, окно
	// остаётся с одной кнопкой». Значит Collapsed: пункт не оставляет пустого места.
	TestEqual(TEXT("Ролик не готов — кнопки просмотра нет вовсе"),
		USupportAuthorWidget::WatchAdVisibilityFor(false), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Ролик готов — кнопка есть"),
		USupportAuthorWidget::WatchAdVisibilityFor(true), ESlateVisibility::Visible);

	TestFalse(TEXT("Правило показа: без готового ролика кнопку не показываем"),
		USupportAuthorWidget::ShouldShowWatchAdButton(false));
	TestTrue(TEXT("Правило показа: с готовым роликом показываем"),
		USupportAuthorWidget::ShouldShowWatchAdButton(true));
	return true;
}

// --- 4. Порог по игровому времени к этой точке не применяется -------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportIgnoresPlaytimeGateTest,
	"ContrarySurvivor.SupportAuthor.PlaytimeGateDoesNotApply", SupportAuthorTestFlags)

bool FSupportIgnoresPlaytimeGateTest::RunTest(const FString& Parameters)
{
	// Свежая игра: сыграно ноль секунд, первый квест не сдан — общий порог трёх остальных
	// точек показа НЕ пройден.
	const bool bCommonGatePassed = AdGating::IsAdGatePassed(/*TotalPlaySeconds=*/0.0,
		/*bFirstQuestDone=*/false);
	TestFalse(TEXT("Общий порог рекламы в свежей игре не пройден"), bCommonGatePassed);

	// ⛔ И тем не менее кнопка просмотра в этом окне ЕСТЬ: правило зависит только от готовности
	// ролика. Дословно из задания: «Кнопка доступна всегда».
	TestTrue(TEXT("Кнопка просмотра доступна даже при непройденном пороге"),
		USupportAuthorWidget::ShouldShowWatchAdButton(/*bAdReady=*/true));

	// И наоборот: пройденный порог сам по себе кнопку не заводит — без ролика её нет.
	TestTrue(TEXT("Порог со временем проходится"),
		AdGating::IsAdGatePassed(/*TotalPlaySeconds=*/100000.0, false));
	TestFalse(TEXT("Но без готового ролика кнопки всё равно нет"),
		USupportAuthorWidget::ShouldShowWatchAdButton(false));
	return true;
}

// --- 5. За просмотр игроку ничего не начисляется --------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportGivesNoRewardTest,
	"ContrarySurvivor.SupportAuthor.WatchingGrantsNothingToPlayer", SupportAuthorTestFlags)

bool FSupportGivesNoRewardTest::RunTest(const FString& Parameters)
{
	// Само правило, записанное словами.
	TestFalse(TEXT("Правило: за просмотр ничего не начисляется"),
		USupportAuthorWidget::GrantsRewardForWatching());

	// А теперь то же самое на ЖИВОМ персонаже: если кто-нибудь однажды допишет сюда награду,
	// этот тест её поймает.
	UWorld* World = SupportAuthorTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(
			APlayerCharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		AContrarySurvivorPlayerController* PC = World->SpawnActor<AContrarySurvivorPlayerController>(
			AContrarySurvivorPlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

		if (TestNotNull(TEXT("Персонаж создан"), Player)
			&& TestNotNull(TEXT("Контроллер создан"), PC)
			&& Player->GetStats())
		{
			PC->Possess(Player);

			UStatsComponent* Stats = Player->GetStats();
			const float MoneyBefore = Stats->GetMoney();
			const float HealthBefore = Stats->GetHealth();
			const float HungerBefore = Stats->GetHunger();
			const float ThirstBefore = Stats->GetThirst();
			const int32 ItemsBefore = Player->GetInventory()
				? Player->GetInventory()->GetInventoryItems().Num() : 0;

			// Ролик досмотрен до конца.
			PC->HandleSupportAdSuccess();

			TestEqual(TEXT("Денег после просмотра столько же"), Stats->GetMoney(), MoneyBefore);
			TestEqual(TEXT("Здоровья после просмотра столько же"), Stats->GetHealth(), HealthBefore);
			TestEqual(TEXT("Голод после просмотра не изменился"), Stats->GetHunger(), HungerBefore);
			TestEqual(TEXT("Жажда после просмотра не изменилась"), Stats->GetThirst(), ThirstBefore);
			TestEqual(TEXT("В рюкзаке столько же предметов"),
				Player->GetInventory() ? Player->GetInventory()->GetInventoryItems().Num() : 0, ItemsBefore);

			// Ролик оборвался с ошибкой — тем более ничего не начисляем.
			PC->HandleSupportAdFail();
			TestEqual(TEXT("После оборвавшегося ролика денег столько же"), Stats->GetMoney(), MoneyBefore);
			TestEqual(TEXT("После оборвавшегося ролика в рюкзаке столько же предметов"),
				Player->GetInventory() ? Player->GetInventory()->GetInventoryItems().Num() : 0, ItemsBefore);
		}
	}

	SupportAuthorTestWorld::Destroy(World);
	return true;
}

// --- 6. Область нажатия под палец -----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportTouchSizeTest,
	"ContrarySurvivor.SupportAuthor.TouchAreaIsBigEnough", SupportAuthorTestFlags)

bool FSupportTouchSizeTest::RunTest(const FString& Parameters)
{
	// Действующее требование Б8: область нажатия не меньше 48 точек по высоте.
	TestTrue(TEXT("Нижняя граница области нажатия не меньше 48 точек"),
		USupportAuthorWidget::GetMinTouchSizePx() >= 48.0f);

	// Размер кнопки в настройках по умолчанию тоже не мельче этой границы.
	const FSupportAuthorStyle Defaults;
	TestTrue(TEXT("Высота кнопки по умолчанию не меньше границы"),
		Defaults.ButtonSize.Y >= USupportAuthorWidget::GetMinTouchSizePx());
	return true;
}

// --- 7. Имена событий замера ----------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportAnalyticsNamesTest,
	"ContrarySurvivor.SupportAuthor.AnalyticsEventNamesMatchTheBrief", SupportAuthorTestFlags)

bool FSupportAnalyticsNamesTest::RunTest(const FString& Parameters)
{
	// Источник открытия окна едет СЕГМЕНТОМ имени — так же, как причина непоказа у остальных
	// рекламных точек. Оба источника из задания: главное меню и пауза.
	TestEqual(TEXT("Окно открыто из главного меню"),
		UAnalyticsSubsystem::MakeSupportWindowOpenedEventId(UAnalyticsSubsystem::SupportSourceMainMenu()),
		FString(TEXT("support:window_opened:main_menu")));
	TestEqual(TEXT("Окно открыто из паузы"),
		UAnalyticsSubsystem::MakeSupportWindowOpenedEventId(UAnalyticsSubsystem::SupportSourcePause()),
		FString(TEXT("support:window_opened:pause")));

	TestEqual(TEXT("Нажата «Посмотреть рекламу»"),
		UAnalyticsSubsystem::MakeSupportAdStartedEventId(), FString(TEXT("support:ad_started")));
	TestEqual(TEXT("Ролик досмотрен"),
		UAnalyticsSubsystem::MakeSupportAdCompletedEventId(), FString(TEXT("support:ad_completed")));
	TestEqual(TEXT("Нажата «Другие способы поддержать»"),
		UAnalyticsSubsystem::MakeSupportLinkOpenedEventId(), FString(TEXT("support:link_opened")));

	// Имя точки показа согласовано с рекламным кабинетом — менять нельзя.
	TestEqual(TEXT("Имя четвёртой точки показа"),
		AdPlacements::SupportAuthor.ToString(), FString(TEXT("support_author")));
	return true;
}

// --- 8. Готовое окно из дизайнера реально привязывается к коду -------------------------------
//
// Самая дорогая ошибка этой волны: имена кубиков в ассете разошлись бы с именами полей
// BindWidgetOptional — привязка молча не сходится, и окно приезжает игроку ПУСТЫМ, без единой
// жалобы в журнале. Поэтому проверяем с ОБЕИХ сторон разом: поднимаем живой ассет
// /Game/UI/WBP_SupportAuthor и требуем, чтобы каждое поле-кубик оказалось не пустым.
//
// Проверка идёт перебором самих полей класса, а не списком имён в тесте: заведут новый кубик —
// он попадёт под проверку сам, и о нём нельзя будет забыть.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportDesignerAssetBindsToCodeTest,
	"ContrarySurvivor.SupportAuthor.DesignerAssetBindsToCode", SupportAuthorTestFlags)

bool FSupportDesignerAssetBindsToCodeTest::RunTest(const FString& Parameters)
{
	UClass* AssetClass = StaticLoadClass(USupportAuthorWidget::StaticClass(), nullptr,
		TEXT("/Game/UI/WBP_SupportAuthor.WBP_SupportAuthor_C"));
	if (!TestNotNull(TEXT("Ассет окна /Game/UI/WBP_SupportAuthor загрузился"), AssetClass))
	{
		return false;
	}

	UWorld* World = SupportAuthorTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	USupportAuthorWidget* Widget = CreateWidget<USupportAuthorWidget>(World, AssetClass);
	if (TestNotNull(TEXT("Окно из готового ассета создалось"), Widget))
	{
		int32 CheckedCubes = 0;
		for (TFieldIterator<FObjectPropertyBase> It(USupportAuthorWidget::StaticClass()); It; ++It)
		{
			FObjectPropertyBase* Prop = *It;
			if (!Prop->PropertyClass || !Prop->PropertyClass->IsChildOf(UWidget::StaticClass()))
			{
				continue;
			}
#if WITH_EDITOR
			// Кубики привязки помечены в заголовке meta = (BindWidgetOptional). Поля без этой
			// пометки (рамка и подложка кодовой запаски) в готовом окне пустые по замыслу.
			if (!Prop->HasMetaData(TEXT("BindWidgetOptional")))
			{
				continue;
			}
#endif
			++CheckedCubes;
			TestNotNull(*FString::Printf(
				TEXT("Кубик «%s» нашёлся в готовом окне (иначе имена в ассете и в коде разошлись)"),
				*Prop->GetName()),
				Prop->GetObjectPropertyValue_InContainer(Widget));
		}
		// Если перебор вдруг не нашёл ни одного поля, тест обязан упасть, а не молча зазеленеть.
		TestTrue(TEXT("Кубики для проверки нашлись"), CheckedCubes > 0);
		AddInfo(FString::Printf(TEXT("Проверено кубиков привязки: %d"), CheckedCubes));
	}

	SupportAuthorTestWorld::Destroy(World);
	return true;
}

// --- 9. Правки владельца мышкой переживают запуск игры ---------------------------------------
//
// Окно отдано владельцу: Ринат двигает и красит его элементы мышкой. Значит на дереве из
// дизайнера код НЕ имеет права трогать ни геометрию, ни цвета, ни шрифты — иначе первый же
// запуск игры молча вернёт всё к своим значениям, и вся ручная настройка пропадёт. Тексты —
// наоборот, обязаны приезжать из настроек: это данные задания издателя, а не оформление.
//
// Проверяем самым честным способом: снимаем состояние живого ассета, зовём ApplyStyle со
// стилем, у которого ВСЁ другое, и сверяем снимки.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportOwnerEditsSurviveApplyStyleTest,
	"ContrarySurvivor.SupportAuthor.OwnerEditsSurviveApplyStyle", SupportAuthorTestFlags)

bool FSupportOwnerEditsSurviveApplyStyleTest::RunTest(const FString& Parameters)
{
	UClass* AssetClass = StaticLoadClass(USupportAuthorWidget::StaticClass(), nullptr,
		TEXT("/Game/UI/WBP_SupportAuthor.WBP_SupportAuthor_C"));
	if (!TestNotNull(TEXT("Ассет окна загрузился"), AssetClass))
	{
		return false;
	}
	UWorld* World = SupportAuthorTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	USupportAuthorWidget* Widget = CreateWidget<USupportAuthorWidget>(World, AssetClass);
	if (TestNotNull(TEXT("Окно из готового ассета создалось"), Widget))
	{
		// Снимок «до»: положение и размер каждого элемента в холсте.
		struct FGeometrySnapshot
		{
			FName Name;
			FVector2D Position = FVector2D::ZeroVector;
			FVector2D Size = FVector2D::ZeroVector;
		};
		const TCHAR* CanvasCubes[] =
		{
			TEXT("PanelPlate"), TEXT("TitleText"), TEXT("MessageText"),
			TEXT("WatchAdButton"), TEXT("SupportLinkButton"), TEXT("CloseButton"), TEXT("ThanksText"),
		};
		TArray<FGeometrySnapshot> Before;
		for (const TCHAR* CubeName : CanvasCubes)
		{
			UWidget* Cube = Widget->GetWidgetFromName(FName(CubeName));
			if (!TestNotNull(*FString::Printf(TEXT("Кубик «%s» есть в окне"), CubeName), Cube))
			{
				continue;
			}
			UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Cube->Slot);
			if (!TestNotNull(*FString::Printf(
				TEXT("Кубик «%s» лежит прямо в холсте — иначе мышкой его не подвинуть"), CubeName),
				CanvasSlot))
			{
				continue;
			}
			Before.Add({ FName(CubeName), CanvasSlot->GetPosition(), CanvasSlot->GetSize() });
		}
		TestEqual(TEXT("Сняли геометрию всех элементов холста"),
			Before.Num(), static_cast<int32>(UE_ARRAY_COUNT(CanvasCubes)));

		// Снимок «до»: оформление. Берём по одному представителю каждого вида.
		UTextBlock* Title = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("TitleText")));
		UBorder* Plate = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("PanelPlate")));
		UButton* WatchAd = Cast<UButton>(Widget->GetWidgetFromName(TEXT("WatchAdButton")));
		if (!Title || !Plate || !WatchAd)
		{
			AddError(TEXT("Не нашлись элементы для снимка оформления"));
			SupportAuthorTestWorld::Destroy(World);
			return false;
		}
		const FLinearColor TitleColorBefore = Title->GetColorAndOpacity().GetSpecifiedColor();
		// ⚠ Кегль шрифта в движке 5.5 — ДРОБНОЕ число (FSlateFontInfo::Size, float), а не целое.
		const float TitleFontSizeBefore = Title->GetFont().Size;
		const FLinearColor PlateColorBefore = Plate->GetBrushColor();
		const FLinearColor WatchFillBefore = WatchAd->GetStyle().Normal.TintColor.GetSpecifiedColor();

		// Стиль, у которого НЕ СОВПАДАЕТ НИЧЕГО: и цвета, и размеры, и шрифты, и тексты.
		FSupportAuthorStyle Alien;
		Alien.TitleColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
		Alien.MessageColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
		Alien.PanelColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		Alien.FrameColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
		Alien.ButtonFillColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);
		Alien.ButtonTextColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
		Alien.TitleFontSize = 99;
		Alien.MessageFontSize = 98;
		Alien.ButtonFontSize = 97;
		Alien.ButtonSize = FVector2D(123.0f, 456.0f);
		Alien.WindowWidth = 1234.0f;
		Alien.TitleText = FText::FromString(TEXT("ЗАГОЛОВОК ИЗ НАСТРОЕК"));
		Alien.WatchAdText = FText::FromString(TEXT("ПОДПИСЬ ИЗ НАСТРОЕК"));

		Widget->ApplyStyle(Alien);

		// Геометрия обязана остаться ровно той же — до последней точки.
		for (const FGeometrySnapshot& Snapshot : Before)
		{
			UWidget* Cube = Widget->GetWidgetFromName(Snapshot.Name);
			UCanvasPanelSlot* CanvasSlot = Cube ? Cast<UCanvasPanelSlot>(Cube->Slot) : nullptr;
			if (!CanvasSlot)
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("«%s» остался на своём месте"), *Snapshot.Name.ToString()),
				CanvasSlot->GetPosition(), Snapshot.Position);
			TestEqual(*FString::Printf(TEXT("«%s» остался прежнего размера"), *Snapshot.Name.ToString()),
				CanvasSlot->GetSize(), Snapshot.Size);
		}

		// Оформление тоже: код не перекрашивает окно владельца и не меняет кегли.
		TestEqual(TEXT("Цвет заголовка не тронут"),
			Title->GetColorAndOpacity().GetSpecifiedColor(), TitleColorBefore);
		TestEqual(TEXT("Кегль заголовка не тронут"), Title->GetFont().Size, TitleFontSizeBefore);
		TestEqual(TEXT("Цвет подложки не тронут"), Plate->GetBrushColor(), PlateColorBefore);
		TestEqual(TEXT("Заливка кнопки не тронута"),
			WatchAd->GetStyle().Normal.TintColor.GetSpecifiedColor(), WatchFillBefore);

		// А вот тексты приехать ОБЯЗАНЫ: это данные задания издателя, а не оформление.
		if (UTextBlock* WatchCaption = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("WatchAdText"))))
		{
			TestEqual(TEXT("Подпись кнопки приехала из настроек"),
				WatchCaption->GetText().ToString(), Alien.WatchAdText.ToString());
		}
		TestEqual(TEXT("Заголовок приехал из настроек"),
			Title->GetText().ToString(), Alien.TitleText.ToString());
	}

	SupportAuthorTestWorld::Destroy(World);
	return true;
}

// --- 10. «Кнопки нет вовсе» работает и в окне из дизайнера ------------------------------------
//
// Побочное следствие плоского холста: положения там фиксированные, поэтому спрятанная кнопка
// просмотра САМА не подтянет вторую вверх — на её месте осталась бы дыра, а условие задания
// требует, чтобы окно осталось с одной кнопкой, без пустого места. Поэтому вторая кнопка
// переезжает на место первой и возвращается назад, когда ролик снова готов.
//
// ⚠ Родное место кнопки снимается ОДИН раз, до первого переезда: возврат обязан класть её
// туда, куда её поставил владелец мышкой, а не туда, где она оказалась после переезда.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportNoGapWhenWatchAdHiddenTest,
	"ContrarySurvivor.SupportAuthor.NoEmptyGapWhenWatchAdHidden", SupportAuthorTestFlags)

bool FSupportNoGapWhenWatchAdHiddenTest::RunTest(const FString& Parameters)
{
	UClass* AssetClass = StaticLoadClass(USupportAuthorWidget::StaticClass(), nullptr,
		TEXT("/Game/UI/WBP_SupportAuthor.WBP_SupportAuthor_C"));
	if (!TestNotNull(TEXT("Ассет окна загрузился"), AssetClass))
	{
		return false;
	}
	UWorld* World = SupportAuthorTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	USupportAuthorWidget* Widget = CreateWidget<USupportAuthorWidget>(World, AssetClass);
	if (TestNotNull(TEXT("Окно из готового ассета создалось"), Widget))
	{
		UWidget* WatchAd = Widget->GetWidgetFromName(TEXT("WatchAdButton"));
		UWidget* Link = Widget->GetWidgetFromName(TEXT("SupportLinkButton"));
		UCanvasPanelSlot* WatchSlot = WatchAd ? Cast<UCanvasPanelSlot>(WatchAd->Slot) : nullptr;
		UCanvasPanelSlot* LinkSlot = Link ? Cast<UCanvasPanelSlot>(Link->Slot) : nullptr;

		if (TestNotNull(TEXT("Кнопка просмотра лежит в холсте"), WatchSlot)
			&& TestNotNull(TEXT("Кнопка ссылки лежит в холсте"), LinkSlot))
		{
			const FVector2D WatchHome = WatchSlot->GetPosition();
			const FVector2D LinkHome = LinkSlot->GetPosition();

			// Порядок из задания: просмотр рекламы стоит ВЫШЕ второй кнопки.
			TestTrue(TEXT("«Посмотреть рекламу» стоит выше «Другие способы поддержать»"),
				WatchHome.Y < LinkHome.Y);

			// Ролик не готов: кнопки нет вовсе, а вторая занимает её место — дыры не остаётся.
			Widget->SetAdAvailable(false);
			TestEqual(TEXT("Кнопки просмотра нет вовсе"),
				WatchAd->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("Вторая кнопка заняла освободившееся место — пустого места нет"),
				LinkSlot->GetPosition(), WatchHome);

			// Ролик снова готов: обе кнопки на своих родных местах.
			Widget->SetAdAvailable(true);
			TestEqual(TEXT("Кнопка просмотра вернулась"),
				WatchAd->GetVisibility(), ESlateVisibility::Visible);
			TestEqual(TEXT("Вторая кнопка вернулась на своё родное место"),
				LinkSlot->GetPosition(), LinkHome);
			TestEqual(TEXT("Кнопка просмотра со своего места не двигалась"),
				WatchSlot->GetPosition(), WatchHome);

			// Повторный круг: место не «уползает» от того, что его пересчитали дважды.
			Widget->SetAdAvailable(false);
			Widget->SetAdAvailable(true);
			TestEqual(TEXT("После второго круга вторая кнопка всё там же"),
				LinkSlot->GetPosition(), LinkHome);
		}
	}

	SupportAuthorTestWorld::Destroy(World);
	return true;
}

// --- 11. Пропал ассет — окно всё равно собирается кодом --------------------------------------
//
// Кодовая запаска остаётся на своём месте: если ассет потеряется или слот окажется пуст,
// игрок обязан увидеть работающее окно, а не пустоту.
//
// ⚠ ЧТО ИМЕННО ЗДЕСЬ ПРОВЕРЯЕТСЯ, а что нет. Проверяется ВЫБОР класса — та часть, которая
// может сломаться молча (кто-нибудь однажды уберёт запасную ветку, и при пустом слоте окно
// просто не создастся). САМА сборка дерева кодом headless НЕ проверяется и проверена быть не
// может: движок 5.5 зовёт NativeOnInitialized только при живом игровом контексте
// (UserWidget.cpp:159-163), которого у тестового мира нет. Это к живой проверке на устройстве.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportCodeFallbackChosenTest,
	"ContrarySurvivor.SupportAuthor.CodeFallbackChosenWhenAssetMissing", SupportAuthorTestFlags)

bool FSupportCodeFallbackChosenTest::RunTest(const FString& Parameters)
{
	// Слот пуст (ассета нет или его не назначили) — берём C++-класс, а не ничего.
	TestEqual(TEXT("Слот пуст — окно собирается кодом"),
		AContrarySurvivorPlayerController::ResolveSupportWidgetClass(nullptr),
		USupportAuthorWidget::StaticClass());

	// Ассет на месте — берём его, запаска в это не вмешивается.
	UClass* AssetClass = StaticLoadClass(USupportAuthorWidget::StaticClass(), nullptr,
		TEXT("/Game/UI/WBP_SupportAuthor.WBP_SupportAuthor_C"));
	if (TestNotNull(TEXT("Ассет окна загрузился"), AssetClass))
	{
		TestEqual(TEXT("Ассет назначен — берём его"),
			AContrarySurvivorPlayerController::ResolveSupportWidgetClass(AssetClass), AssetClass);
	}
	return true;
}

// --- Текст благодарности: одно слово (решение Рината 13.08.2026) -----------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportThanksTextTest,
	"ContrarySurvivor.SupportAuthor.ThanksTextIsSingleWord", SupportAuthorTestFlags)

bool FSupportThanksTextTest::RunTest(const FString& Parameters)
{
	// Дословно из решения владельца: «Оставь просто "Спасибо", без "...это реально помогает"».
	// Одно место правды на код, кодовую запаску и ассет — поле стиля.
	const FSupportAuthorStyle Defaults;
	TestEqual(TEXT("После ролика окно говорит одно слово"),
		Defaults.ThanksText.ToString(), FString(TEXT("Спасибо")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
