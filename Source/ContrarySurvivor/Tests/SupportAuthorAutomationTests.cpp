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
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
