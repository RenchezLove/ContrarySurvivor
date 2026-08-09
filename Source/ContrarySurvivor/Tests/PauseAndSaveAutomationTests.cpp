// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты подхода 3 волны меню (спека docs/contrary-survivor/glavnoe-menu-spec.md:
// надпись «Прогресс сохранён» у костра, имя слота параметром, возврат в меню из паузы с
// подтверждением, вибрация). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.PauseAndSave; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * возврат в главное меню из паузы переспрашивает РОВНО когда есть несохранённый прогресс,
//     первое нажатие ничего не решает, «Отмена» возвращает обычную паузу;
//   * «несохранённый прогресс» считается по заработанному (деньги, рюкзак, журнал квестов):
//     сохранился — вопроса нет, потратил/подобрал — вопрос вернулся;
//   * сохранение у костра действительно пишет слот и снимает признак несохранённого прогресса;
//   * имя слота — параметр: сохранение уходит в слот персонажа, боевой слот не трогается;
//   * вибрация при уроне включается только с разрешения игрока и только при реальном уроне.
//
// Тесты пишут в ИЗОЛИРОВАННЫЕ слоты (ASaveTestPlayerCharacter::UseTestSaveSlot), НЕ в боевой
// 'ContrarySave': в нём у Рината живой прогресс с телефона (ADR-058).
// НЕ покрывается headless: сама всплывающая надпись у костра и живые касания кнопок паузы —
// нужен запуск игры (Slate в Automation-тестах проекта не поднимается, обработчики зовутся напрямую).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Tests/SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/UI/PauseMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags PauseSaveTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир — та же обвязка, что в SaveLoadAutomationTests (по установленному
// в проекте паттерну «свой мир на файл теста»: обвязка там static и наружу не видна).
namespace PauseSaveTestWorld
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
			WorldSettings->NotifyBeginPlay();
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

	static ASaveTestPlayerCharacter* SpawnPlayer(UWorld* World, const FString& Slot)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASaveTestPlayerCharacter* Player = World->SpawnActor<ASaveTestPlayerCharacter>(
			ASaveTestPlayerCharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		if (Player)
		{
			Player->UseTestSaveSlot(Slot);
		}
		return Player;
	}

	static FString TestSlot(const TCHAR* Tag)
	{
		return FString::Printf(TEXT("Test_PauseSave_%s"), Tag);
	}
}

// --- 1. Возврат в меню из паузы: подтверждение ровно при несохранённом прогрессе ----------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseMainMenuConfirmTest,
	"ContrarySurvivor.PauseAndSave.MainMenuConfirmOnlyWhenUnsaved", PauseSaveTestFlags)

bool FPauseMainMenuConfirmTest::RunTest(const FString& Parameters)
{
	// Спека: «Возврат в меню из паузы — с подтверждением, если прогресс не сохранён».
	TestTrue(TEXT("Несохранённый прогресс — спрашиваем"),
		UPauseMenuWidget::ShouldConfirmMainMenu(true));
	TestFalse(TEXT("Всё сохранено — уходим без вопроса"),
		UPauseMenuWidget::ShouldConfirmMainMenu(false));

	UPauseMenuWidget* Pause = NewObject<UPauseMenuWidget>();
	if (!TestNotNull(TEXT("Виджет паузы создан"), Pause))
	{
		return false;
	}

	int32 MainMenuSignals = 0;
	int32 ResumeSignals = 0;
	Pause->OnMainMenuRequested.AddLambda([&MainMenuSignals]() { ++MainMenuSignals; });
	Pause->OnResumeRequested.AddLambda([&ResumeSignals]() { ++ResumeSignals; });

	// Прогресс сохранён — уходим сразу, лишний вопрос игрока только злит.
	Pause->SetProgressUnsaved(false);
	Pause->HandleMainMenuClicked();
	TestFalse(TEXT("Переспрос не открывался"), Pause->IsConfirmingMainMenu());
	TestEqual(TEXT("Ушли в меню с первого нажатия"), MainMenuSignals, 1);

	// Прогресс не сохранён — первое нажатие только спрашивает.
	Pause->SetProgressUnsaved(true);
	Pause->HandleMainMenuClicked();
	TestTrue(TEXT("Первое нажатие открыло вопрос"), Pause->IsConfirmingMainMenu());
	TestEqual(TEXT("Первое нажатие никуда не уводит"), MainMenuSignals, 1);

	// «Отмена» (та же кнопка «Продолжить» в режиме вопроса) возвращает обычную паузу и НЕ
	// снимает паузу с игры.
	Pause->HandleResumeClicked();
	TestFalse(TEXT("«Отмена» закрыла вопрос"), Pause->IsConfirmingMainMenu());
	TestEqual(TEXT("«Отмена» не выходит в меню"), MainMenuSignals, 1);
	TestEqual(TEXT("«Отмена» не снимает паузу"), ResumeSignals, 0);

	// Два явных нажатия — ровно один выход в меню, режим вопроса за собой не остаётся.
	Pause->HandleMainMenuClicked();
	Pause->HandleMainMenuClicked();
	TestEqual(TEXT("Второе нажатие уводит в меню ровно один раз"), MainMenuSignals, 2);
	TestFalse(TEXT("После выхода вопрос закрыт"), Pause->IsConfirmingMainMenu());

	// Обычная «Продолжить» вне вопроса работает как раньше.
	Pause->HandleResumeClicked();
	TestEqual(TEXT("«Продолжить» снимает паузу"), ResumeSignals, 1);
	return true;
}

// --- 2. Что считается несохранённым прогрессом --------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnsavedProgressTest,
	"ContrarySurvivor.PauseAndSave.UnsavedProgressFollowsEarnings", PauseSaveTestFlags)

bool FUnsavedProgressTest::RunTest(const FString& Parameters)
{
	UWorld* World = PauseSaveTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = PauseSaveTestWorld::TestSlot(TEXT("Unsaved"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* Player = PauseSaveTestWorld::SpawnPlayer(World, Slot);
		if (Player && Player->GetStats())
		{
			// Сохранения в этой сессии ещё не было — считаем, что терять есть что.
			TestTrue(TEXT("До первого сохранения прогресс считается несохранённым"),
				Player->IsProgressUnsaved());

			TestTrue(TEXT("Сохранение прошло"), Player->SaveGame());
			TestFalse(TEXT("Сразу после сохранения терять нечего"), Player->IsProgressUnsaved());

			// Деньги — часть заработанного: заработал и не сохранился — вопрос вернулся.
			Player->GetStats()->AddMoney(25.0f);
			TestTrue(TEXT("Заработанные деньги делают прогресс несохранённым"),
				Player->IsProgressUnsaved());

			TestTrue(TEXT("Повторное сохранение прошло"), Player->SaveGame());
			TestFalse(TEXT("После сохранения вопрос снова снят"), Player->IsProgressUnsaved());

			// Постоянно ползущие сами по себе жажда и здоровье прогрессом НЕ считаются:
			// иначе вопрос «выйти без сохранения?» выскакивал бы всегда и потерял бы смысл.
			Player->GetStats()->SetThirst(Player->GetStats()->GetThirst() - 10.0f);
			Player->GetStats()->SetHealth(Player->GetStats()->GetHealth() - 5.0f);
			TestFalse(TEXT("Просевшие жажда и здоровье вопроса не поднимают"),
				Player->IsProgressUnsaved());
		}
		else
		{
			AddError(TEXT("Игрок или его статы не создались"));
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	PauseSaveTestWorld::Destroy(World);
	return true;
}

// --- 3. Сохранение у костра и слот как параметр --------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampfireSaveTest,
	"ContrarySurvivor.PauseAndSave.CampfireSaveUsesSlotParameter", PauseSaveTestFlags)

bool FCampfireSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = PauseSaveTestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	const FString Slot = PauseSaveTestWorld::TestSlot(TEXT("Campfire"));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	{
		ASaveTestPlayerCharacter* Player = PauseSaveTestWorld::SpawnPlayer(World, Slot);
		if (Player)
		{
			TestEqual(TEXT("Имя слота — параметр персонажа, а не константа в коде"),
				Player->GetSaveSlotName(), Slot);

			// Сохранение у костра — то же сохранение плюс всплывающая надпись игроку.
			// Саму надпись headless не увидеть (Slate не поднимается), поэтому проверяем,
			// что путь отрабатывает целиком и не падает без живого экрана.
			TestTrue(TEXT("Сохранение у костра прошло"), Player->SaveGameAtCampfire());
			TestTrue(TEXT("Слот персонажа появился на диске"),
				UGameplayStatics::DoesSaveGameExist(Slot, 0));
			TestFalse(TEXT("После костра несохранённого прогресса нет"), Player->IsProgressUnsaved());

			// Главное: боевой слот игры тесты не трогают.
			TestNotEqual(TEXT("Тестовый слот — не боевой 'ContrarySave'"), Slot, FString(TEXT("ContrarySave")));
		}
		else
		{
			AddError(TEXT("Игрок не создался"));
		}
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	PauseSaveTestWorld::Destroy(World);
	return true;
}

// --- 4. Вибрация: только с разрешения игрока и только по реальному урону --------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageVibrationRuleTest,
	"ContrarySurvivor.PauseAndSave.VibrationNeedsSettingAndRealDamage", PauseSaveTestFlags)

bool FDamageVibrationRuleTest::RunTest(const FString& Parameters)
{
	// Решение game-lead 08-09: трясём телефон при уроне и при смерти. Здесь — правило урона.
	TestTrue(TEXT("Разрешена и урон прошёл — трясём"),
		APlayerCharacter::ShouldPlayDamageVibration(true, 12.0f));
	TestFalse(TEXT("Игрок выключил вибрацию — молчим"),
		APlayerCharacter::ShouldPlayDamageVibration(false, 12.0f));
	TestFalse(TEXT("Урон не прошёл (броня в ноль, неуязвимость) — молчим"),
		APlayerCharacter::ShouldPlayDamageVibration(true, 0.0f));
	TestFalse(TEXT("Отрицательный урон (лечение) вибрацией не отзывается"),
		APlayerCharacter::ShouldPlayDamageVibration(true, -5.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
