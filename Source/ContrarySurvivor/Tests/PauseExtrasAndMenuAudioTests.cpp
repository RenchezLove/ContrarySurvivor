// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны 08-09 (живой осмотр Рината): новые пункты меню паузы и
// тишина игрового мира под главным меню. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.PauseExtras; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.MenuAudio; Quit"
//
// Что здесь доказывается:
//   * «Сообщество» в паузе живёт по ТОМУ ЖЕ правилу, что и в главном меню (адрес один на игру,
//     пустой — пункта нет вовсе), второго правила и второго адреса не заведено;
//   * подпись «Политика конфиденциальности» набирается своим, более мелким кеглем — она не
//     помещалась в кнопку общим (Ринат увидел её обрезанной);
//   * отбор звуков для заглушения на время меню: интерфейсные не трогаем, молчащие тоже, а
//     чужую паузу не перехватываем — иначе при закрытии меню мы «оживили» бы не своё.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/PauseMenuWidget.h"
#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags PauseExtrasTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Пункты «Сообщество» и «Настройки» в паузе -------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseExtrasRulesTest,
	"ContrarySurvivor.PauseExtras.CommunityAndPolicyRules", PauseExtrasTestFlags)

bool FPauseExtrasRulesTest::RunTest(const FString& Parameters)
{
	const FPauseMenuStyle Style;

	// Подпись политики — своим кеглем, и он МЕНЬШЕ общего: длинная надпись не помещалась
	// в кнопку и обрезалась на телефоне.
	TestTrue(FString::Printf(
		TEXT("Кегль строки политики (%d) меньше кегля остальных подписей (%d)"),
		Style.PolicyFontSize, Style.ButtonFontSize),
		Style.PolicyFontSize < Style.ButtonFontSize);
	TestTrue(TEXT("Кегль строки политики остаётся читаемым"), Style.PolicyFontSize >= 10);

	// Подписи новых пунктов не пустые — иначе в паузе появятся безымянные кнопки.
	TestFalse(TEXT("У пункта «Настройки» есть подпись"), Style.SettingsText.IsEmpty());
	TestFalse(TEXT("У пункта «Сообщество» есть подпись"), Style.CommunityText.IsEmpty());

	// Правило показа «Сообщества» — ОДНО на игру: пауза берёт его у главного меню.
	TestEqual(TEXT("Пустой адрес — пункта «Сообщество» нет вовсе"),
		UStartScreenWidget::CommunityVisibilityFor(FString()), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Пробелы вместо адреса — тоже нет"),
		UStartScreenWidget::CommunityVisibilityFor(TEXT("   ")), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Адрес есть — пункт показан"),
		UStartScreenWidget::CommunityVisibilityFor(TEXT("https://t.me/marevo")), ESlateVisibility::Visible);
	return true;
}

// --- 2. Отбор звуков мира, которые глушим под меню ------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuWorldAudioSelectionTest,
	"ContrarySurvivor.MenuAudio.SilenceOnlyLiveWorldSounds", PauseExtrasTestFlags)

bool FMenuWorldAudioSelectionTest::RunTest(const FString& Parameters)
{
	// Обычный живой звук мира (птицы, ветер) — глушим.
	TestTrue(TEXT("Звучащий звук мира глушим"),
		AContrarySurvivorPlayerController::ShouldSilenceWorldSound(
			/*bIsUISound=*/false, /*bIsPlaying=*/true, /*bAlreadyPaused=*/false));

	// Звук интерфейса — не трогаем никогда: иначе онемеют щелчки кнопок и музыка меню.
	TestFalse(TEXT("Звук интерфейса не глушим"),
		AContrarySurvivorPlayerController::ShouldSilenceWorldSound(true, true, false));

	// Молчащий звук глушить незачем.
	TestFalse(TEXT("Молчащий звук не трогаем"),
		AContrarySurvivorPlayerController::ShouldSilenceWorldSound(false, false, false));

	// Уже приостановленный кем-то другим — не наш: если бы мы его записали себе, то при
	// закрытии меню включили бы звук, который выключили не мы.
	TestFalse(TEXT("Чужую паузу не перехватываем"),
		AContrarySurvivorPlayerController::ShouldSilenceWorldSound(false, true, true));

	// Совсем безнадёжный случай — тоже мимо.
	TestFalse(TEXT("Интерфейсный и молчащий — мимо"),
		AContrarySurvivorPlayerController::ShouldSilenceWorldSound(true, false, true));
	return true;
}

// --- 3. Лесной фон молчит, пока на экране меню -----------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbienceSilencedUnderMenuTest,
	"ContrarySurvivor.MenuAudio.AmbienceSilencedUnderMenu", PauseExtrasTestFlags)

bool FAmbienceSilencedUnderMenuTest::RunTest(const FString& Parameters)
{
	// Живая сессия 08-09: птицы пели поверх меню, потому что лесной фон заведён через
	// SpawnSound2D и считается ЗВУКОМ ИНТЕРФЕЙСА — общий отбор звуков мира его намеренно
	// пропускает. Теперь фон глушится напрямую по ссылке, и это правило держит тест.
	if (!GEngine)
	{
		return false;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
	Ctx.SetCurrentWorld(World);
	const FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->NotifyBeginPlay();
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(
			APlayerCharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		if (TestNotNull(TEXT("Игрок в тестовом мире создан"), Player))
		{
			TestFalse(TEXT("По умолчанию лес звучит"), Player->IsAmbienceSilenced());

			Player->SetAmbienceSilenced(true);
			TestTrue(TEXT("Меню на экране — лес молчит"), Player->IsAmbienceSilenced());

			// Повторная просьба ничего не ломает: гейт зовётся по каждому событию.
			Player->SetAmbienceSilenced(true);
			TestTrue(TEXT("Повторная просьба состояние не портит"), Player->IsAmbienceSilenced());

			Player->SetAmbienceSilenced(false);
			TestFalse(TEXT("Меню закрылось — лес вернулся"), Player->IsAmbienceSilenced());
		}
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
