// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты переезда запуска на пустой загрузочный уровень (ADR-067 п.5).
// Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.BootFlow; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается (всё это — правила, которые видно без живого мира):
//   * какое намерение получает игрок при этом запуске: самый первый после установки едет
//     сразу в мир новой игрой, любой следующий остаётся в главном меню;
//   * намерение читается ровно один раз и тут же сбрасывается в «нет»;
//   * наличие сохранения проверяется БЕЗ живого персонажа — иначе на загрузочном уровне
//     пункт «Продолжить» пропал бы у всех;
//   * стирание сохранения работает БЕЗ живого персонажа (кнопка «Новая игра» в меню);
//   * ожидание ответа про сбор данных КОНЕЧНО: оно сдаётся по счётчику шагов;
//   * узнавание загрузочного уровня по имени карты понимает и полный адрес, и короткое имя.
//
// НЕ покрывается headless: сам переезд между уровнями (OpenLevel требует живой игры) и вид
// меню на устройстве — это проверяет Ринат живьём.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/Subsystems/GameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"

static constexpr EAutomationTestFlags BootFlowTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace BootFlowTestLocal
{
	// Именованный тестовый слот — ЗАВЕДОМО не боевой 'ContrarySave': на машине Рината в боевом
	// слоте живой прогресс с телефона, трогать его тесты не имеют права.
	static FString TestSlot(const TCHAR* Tag)
	{
		return FString::Printf(TEXT("Test_BootFlow_%s"), Tag);
	}

	// Кладёт в слот объект сохранения. bWithRealData=false повторяет то, что пишет накопитель
	// игрового времени: файл слота есть, а реального прогресса в нём нет.
	static void WriteSlot(const FString& SlotName, bool bWithRealData)
	{
		UContrarySaveGame* Save = Cast<UContrarySaveGame>(
			UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
		if (!Save)
		{
			return;
		}
		Save->bHasData = bWithRealData;
		Save->TotalPlayTimeSeconds = 61.0f;
		UGameplayStatics::SaveGameToSlot(Save, SlotName, /*UserIndex=*/0);
	}
}

// --- 1. Какое намерение даёт запуск --------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootFlowLaunchIntentTest,
	"ContrarySurvivor.BootFlow.LaunchDecidesIntent", BootFlowTestFlags)

bool FBootFlowLaunchIntentTest::RunTest(const FString& Parameters)
{
	// Источник истины (ADR-067 п.5): «Первый запуск после установки — сразу в мир и играть;
	// любой следующий — главное меню, и мир грузится ТОЛЬКО после "Продолжить" или "Новая игра"».
	TestEqual(TEXT("Самый первый запуск после установки едет сразу в мир новой игрой"),
		AContrarySurvivorPlayerController::BootIntentForLaunch(/*bLaunchedBefore=*/false, /*bHasSave=*/false),
		EContraryWorldEntryIntent::NewGame);

	TestEqual(TEXT("Повторный запуск остаётся в меню — мир пока не грузим"),
		AContrarySurvivorPlayerController::BootIntentForLaunch(true, false),
		EContraryWorldEntryIntent::None);

	// Найденное сохранение само доказывает прошлый запуск (обновление со сборки, где отметки
	// о запусках ещё не было) — прежнее поведение сохраняется.
	TestEqual(TEXT("Сохранение без отметки о запусках тоже даёт меню"),
		AContrarySurvivorPlayerController::BootIntentForLaunch(false, true),
		EContraryWorldEntryIntent::None);

	TestEqual(TEXT("Повторный запуск с сохранением даёт меню"),
		AContrarySurvivorPlayerController::BootIntentForLaunch(true, true),
		EContraryWorldEntryIntent::None);
	return true;
}

// --- 2. Намерение читается один раз и сбрасывается ------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootFlowIntentConsumeTest,
	"ContrarySurvivor.BootFlow.IntentIsReadOnceAndReset", BootFlowTestFlags)

bool FBootFlowIntentConsumeTest::RunTest(const FString& Parameters)
{
	// Хранителю намерения нужен «объект игры» в хозяевах: движок это требует у всех подсистем
	// такого рода (ClassWithin у UGameInstanceSubsystem) и ругается, если хозяин другой.
	UGameInstance* OwnerGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	if (!TestNotNull(TEXT("Объект игры для теста создан"), OwnerGameInstance))
	{
		return false;
	}

	UGameFlowSubsystem* Flow = NewObject<UGameFlowSubsystem>(OwnerGameInstance);
	if (!TestNotNull(TEXT("Хранитель намерения создан"), Flow))
	{
		return false;
	}

	TestEqual(TEXT("По умолчанию намерения нет"),
		Flow->GetWorldEntryIntent(), EContraryWorldEntryIntent::None);

	Flow->SetWorldEntryIntent(EContraryWorldEntryIntent::Continue);
	TestEqual(TEXT("Намерение записалось"),
		Flow->GetWorldEntryIntent(), EContraryWorldEntryIntent::Continue);

	TestEqual(TEXT("Чтение отдаёт записанное намерение"),
		Flow->ConsumeWorldEntryIntent(), EContraryWorldEntryIntent::Continue);
	TestEqual(TEXT("После чтения намерение сброшено в «нет»"),
		Flow->GetWorldEntryIntent(), EContraryWorldEntryIntent::None);
	TestEqual(TEXT("Повторное чтение даёт «нет» — намерение одноразовое"),
		Flow->ConsumeWorldEntryIntent(), EContraryWorldEntryIntent::None);

	// «Новая игра» ходит тем же путём.
	Flow->SetWorldEntryIntent(EContraryWorldEntryIntent::NewGame);
	TestEqual(TEXT("Намерение «новая игра» доезжает"),
		Flow->ConsumeWorldEntryIntent(), EContraryWorldEntryIntent::NewGame);
	return true;
}

// --- 3. Наличие сохранения без живого персонажа ---------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootFlowSaveCheckWithoutPawnTest,
	"ContrarySurvivor.BootFlow.SaveCheckWithoutPawn", BootFlowTestFlags)

bool FBootFlowSaveCheckWithoutPawnTest::RunTest(const FString& Parameters)
{
	const FString Slot = BootFlowTestLocal::TestSlot(TEXT("Check"));
	APlayerCharacter::DeleteSaveInSlot(Slot, 0); // чистый старт

	TestFalse(TEXT("Пустой слот: продолжать нечего"),
		APlayerCharacter::HasSaveGameInSlot(Slot, 0));

	// Файл слота есть, но реального прогресса в нём нет (так пишет накопитель игрового времени).
	BootFlowTestLocal::WriteSlot(Slot, /*bWithRealData=*/false);
	TestTrue(TEXT("Файл слота на диске появился"),
		UGameplayStatics::DoesSaveGameExist(Slot, 0));
	TestFalse(TEXT("Но продолжать всё ещё нечего — реального прогресса нет"),
		APlayerCharacter::HasSaveGameInSlot(Slot, 0));

	// Настоящее сохранение.
	BootFlowTestLocal::WriteSlot(Slot, /*bWithRealData=*/true);
	TestTrue(TEXT("Настоящее сохранение видно без живого персонажа"),
		APlayerCharacter::HasSaveGameInSlot(Slot, 0));

	// Стирание тоже работает без персонажа — это и есть кнопка «Новая игра» в меню.
	APlayerCharacter::DeleteSaveInSlot(Slot, 0);
	TestFalse(TEXT("После стирания слот пуст"),
		APlayerCharacter::HasSaveGameInSlot(Slot, 0));
	TestFalse(TEXT("Файла слота на диске больше нет"),
		UGameplayStatics::DoesSaveGameExist(Slot, 0));

	// Слот по умолчанию берётся из умолчаний класса, а не пишется строкой второй раз. Боевой
	// слот тест не читает и не трогает — сверяем только имя.
	TestEqual(TEXT("Слот по умолчанию — тот же, что у персонажа"),
		APlayerCharacter::GetDefaultSaveSlotName(), FString(TEXT("ContrarySave")));
	TestEqual(TEXT("Номер слота по умолчанию — нулевой"),
		APlayerCharacter::GetDefaultSaveUserIndex(), 0);
	return true;
}

// --- 4. Ожидание ответа про сбор данных конечно ---------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootFlowConsentWaitIsFiniteTest,
	"ContrarySurvivor.BootFlow.ConsentWaitIsFinite", BootFlowTestFlags)

bool FBootFlowConsentWaitIsFiniteTest::RunTest(const FString& Parameters)
{
	// Ждём ответа, пока окно на экране либо пока мы ещё ловим момент его показать.
	TestTrue(TEXT("Окно на экране — ждём ответа"),
		UDataConsentSubsystem::ShouldWaitForConsentAnswer(/*bScreenOnScreen=*/true, /*bWaitingForScreen=*/false));
	TestTrue(TEXT("Ещё ловим момент показать окно — тоже ждём"),
		UDataConsentSubsystem::ShouldWaitForConsentAnswer(false, true));
	TestFalse(TEXT("Ни окна, ни ожидания — не ждём никого"),
		UDataConsentSubsystem::ShouldWaitForConsentAnswer(false, false));

	const int32 MaxSteps = UDataConsentSubsystem::GetMaxScreenWaitSteps();
	TestTrue(TEXT("Предел шагов ожидания задан положительным числом"), MaxSteps > 0);

	// ⛔ Главное: ожидание КОНЕЧНО. Экрана нет — ждём, но не бесконечно.
	TestTrue(TEXT("Экрана нет, шаги ещё есть — продолжаем ждать"),
		UDataConsentSubsystem::ShouldKeepWaitingForScreen(/*bScreenReady=*/false, /*StepsDone=*/1, MaxSteps));
	TestFalse(TEXT("Шаги израсходованы — сдаёмся, вечного ожидания нет"),
		UDataConsentSubsystem::ShouldKeepWaitingForScreen(false, MaxSteps, MaxSteps));
	TestFalse(TEXT("Шагов израсходовано больше предела — тем более сдаёмся"),
		UDataConsentSubsystem::ShouldKeepWaitingForScreen(false, MaxSteps + 10, MaxSteps));
	TestFalse(TEXT("Экран готов — ждать больше нечего"),
		UDataConsentSubsystem::ShouldKeepWaitingForScreen(true, 1, MaxSteps));
	return true;
}

// --- 5. Узнавание загрузочного уровня по имени карты ----------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootFlowLevelNameMatchTest,
	"ContrarySurvivor.BootFlow.LevelNameMatching", BootFlowTestFlags)

bool FBootFlowLevelNameMatchTest::RunTest(const FString& Parameters)
{
	// Адрес уровня можно писать и полным, и коротким — узнавание работает в обоих случаях.
	TestTrue(TEXT("Полный адрес загрузочного уровня узнаётся"),
		AContrarySurvivorPlayerController::IsSameLevel(TEXT("L_Boot"), TEXT("/Game/Maps/L_Boot")));
	TestTrue(TEXT("Короткое имя загрузочного уровня узнаётся"),
		AContrarySurvivorPlayerController::IsSameLevel(TEXT("L_Boot"), TEXT("L_Boot")));

	// ⛔ Игровая карта загрузочным уровнем не считается — иначе мир никогда бы не запустился.
	TestFalse(TEXT("Игровая карта загрузочным уровнем не считается"),
		AContrarySurvivorPlayerController::IsSameLevel(TEXT("L_World_C"), TEXT("/Game/Maps/L_Boot")));
	TestFalse(TEXT("Пустой адрес уровня ничему не равен"),
		AContrarySurvivorPlayerController::IsSameLevel(TEXT("L_Boot"), NAME_None));
	TestFalse(TEXT("Пустое имя текущей карты ничему не равно"),
		AContrarySurvivorPlayerController::IsSameLevel(FString(), TEXT("/Game/Maps/L_Boot")));
	return true;
}

// --- 6. Настройки: игра стартует на загрузочном уровне и он получает свой режим -------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBootFlowSettingsTest,
	"ContrarySurvivor.BootFlow.BootLevelGetsItsGameMode", BootFlowTestFlags)

bool FBootFlowSettingsTest::RunTest(const FString& Parameters)
{
	// Строка настройки обязана реально доезжать до движка. Имя поля внутри скобок легко
	// перепутать, и тогда загрузочный уровень тихо получил бы обычный режим игры — вместе с
	// персонажем, лесом и звуками, то есть ровно то, от чего мы уходим. Ошибка была бы тихой:
	// ни сборка, ни редактор о ней не предупреждают.
	const FString ModeForBootLevel = UGameMapsSettings::GetGameModeForMapName(TEXT("L_Boot"));
	TestEqual(TEXT("Загрузочный уровень получает свой режим игры"),
		ModeForBootLevel, FString(TEXT("/Script/ContrarySurvivor.BootGameMode")));

	TestNotNull(TEXT("Этот режим игры существует и загружается"),
		LoadClass<AGameModeBase>(nullptr, *ModeForBootLevel));

	// ⛔ Игровая карта под это правило не подпадает — её режим остаётся своим.
	TestTrue(TEXT("Игровая карта своего режима не теряет"),
		UGameMapsSettings::GetGameModeForMapName(TEXT("L_World_C")).IsEmpty());

	// И сама игра стартует именно на загрузочном уровне (иначе телефон снова грузил бы мир
	// до главного меню).
	TestTrue(TEXT("Игра стартует на загрузочном уровне"),
		UGameMapsSettings::GetGameDefaultMap().Contains(TEXT("L_Boot")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
