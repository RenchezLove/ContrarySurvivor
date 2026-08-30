// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты боевой музыки (задача №5, ТЗ издателя 30.08). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.CombatMusic; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается (чистые правила, без живого мира):
//   * выбор трека нового боя: каждый новый бой берёт тот трек, которого не было в предыдущем
//     бою; самый первый бой — первый трек; один назначенный трек играет всегда, пустые оба —
//     музыки нет (это настройка, а не ошибка);
//   * критерий «рядом с игроком есть враг в бою»: боевое состояние И дистанция в радиусе —
//     ориентир то, что происходит рядом с игроком, а не где-то на карте;
//   * гейт тишины: пауза мира (включая рекламу-заглушку), главное меню и экран смерти глушат
//     музыку; обыск и торговец мир не останавливают — музыка играет;
//   * значения по умолчанию на контроллере согласованы с ТЗ (два разных трека, приглушение
//     фона на 30 %).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"

static constexpr EAutomationTestFlags CombatMusicTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Выбор трека нового боя ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMusicTrackChoiceTest,
	"ContrarySurvivor.CombatMusic.TrackChoiceAlternates", CombatMusicTestFlags)

bool FCombatMusicTrackChoiceTest::RunTest(const FString& Parameters)
{
	// Самый первый бой (прошлого трека нет) — первый трек.
	TestEqual(TEXT("Первый бой берёт первый трек"),
		AContrarySurvivorPlayerController::PickCombatTrackIndex(INDEX_NONE, true, true), 0);

	// Чередование: «при каждом новом бое выбирается тот трек, которого не было в предыдущем бое».
	TestEqual(TEXT("После первого трека — второй"),
		AContrarySurvivorPlayerController::PickCombatTrackIndex(0, true, true), 1);
	TestEqual(TEXT("После второго трека — снова первый"),
		AContrarySurvivorPlayerController::PickCombatTrackIndex(1, true, true), 0);

	// Назначен только один трек — играет он, чередовать нечего.
	TestEqual(TEXT("Только первый трек назначен — играет первый, даже после первого"),
		AContrarySurvivorPlayerController::PickCombatTrackIndex(0, true, false), 0);
	TestEqual(TEXT("Только второй трек назначен — играет второй, даже после второго"),
		AContrarySurvivorPlayerController::PickCombatTrackIndex(1, false, true), 1);

	// Оба поля пустые — боевой музыки нет (допустимая настройка, не ошибка).
	TestEqual(TEXT("Оба трека пустые — музыки нет"),
		AContrarySurvivorPlayerController::PickCombatTrackIndex(INDEX_NONE, false, false),
		(int32)INDEX_NONE);
	return true;
}

// --- 2. Критерий «рядом с игроком есть враг в бою» -------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMusicNearbyRuleTest,
	"ContrarySurvivor.CombatMusic.NearbyEngagingRule", CombatMusicTestFlags)

bool FCombatMusicNearbyRuleTest::RunTest(const FString& Parameters)
{
	const float RadiusSq = FMath::Square(2500.0f);

	// Враг в бою и в радиусе — музыка звучит.
	TestTrue(TEXT("Враг в бою рядом — считается"),
		AContrarySurvivorPlayerController::ShouldCountEnemyForCombatMusic(
			/*bEngaging=*/true, FMath::Square(1000.0f), RadiusSq));

	// Мирный враг рядом (просто стоит/идёт домой) — музыку не держит.
	TestFalse(TEXT("Враг НЕ в бою — не считается, даже вплотную"),
		AContrarySurvivorPlayerController::ShouldCountEnemyForCombatMusic(
			false, FMath::Square(100.0f), RadiusSq));

	// «Если игрок сбежал… но формально кто-то ещё гонится где-то далеко — музыку тоже гасим».
	TestFalse(TEXT("Враг в бою, но далеко — не считается"),
		AContrarySurvivorPlayerController::ShouldCountEnemyForCombatMusic(
			true, FMath::Square(4000.0f), RadiusSq));

	// Граница радиуса включительно — на самом краю музыка ещё держится (без мигания на границе).
	TestTrue(TEXT("Ровно на границе радиуса — ещё считается"),
		AContrarySurvivorPlayerController::ShouldCountEnemyForCombatMusic(
			true, RadiusSq, RadiusSq));
	return true;
}

// --- 3. Гейт тишины (пауза/меню/смерть; обыск и торговец не глушат) --------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMusicSilenceGateTest,
	"ContrarySurvivor.CombatMusic.SilenceGateRule", CombatMusicTestFlags)

bool FCombatMusicSilenceGateTest::RunTest(const FString& Parameters)
{
	// Бой идёт, никаких экранов — музыка звучит.
	TestFalse(TEXT("Без паузы, меню и смерти музыка не глушится"),
		AContrarySurvivorPlayerController::ShouldGateSilenceCombatMusic(false, false, false));

	// Пауза мира: и меню паузы, и реклама-заглушка (та ставит SetGamePaused) — тишина.
	TestTrue(TEXT("Пауза мира глушит музыку"),
		AContrarySurvivorPlayerController::ShouldGateSilenceCombatMusic(true, false, false));

	// Главное меню поверх живого мира — тишина.
	TestTrue(TEXT("Главное меню глушит музыку"),
		AContrarySurvivorPlayerController::ShouldGateSilenceCombatMusic(false, true, false));

	// Экран смерти — тишина (ошибка 3 ТЗ: не играет на экране смерти).
	TestTrue(TEXT("Экран смерти глушит музыку"),
		AContrarySurvivorPlayerController::ShouldGateSilenceCombatMusic(false, false, true));

	// Обыск трупа и торговец мир НЕ останавливают — ни один из трёх признаков не взводится,
	// музыка продолжает играть (ошибка 4 ТЗ). Это тот же случай, что первая проверка: гейт
	// смотрит только на паузу/меню/смерть и ничего не знает про окна поверх живого мира.
	return true;
}

// --- 4. Значения по умолчанию на контроллере согласованы с ТЗ --------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMusicDefaultsTest,
	"ContrarySurvivor.CombatMusic.ControllerDefaults", CombatMusicTestFlags)

bool FCombatMusicDefaultsTest::RunTest(const FString& Parameters)
{
	const AContrarySurvivorPlayerController* Defaults =
		GetDefault<AContrarySurvivorPlayerController>();

	// Оба трека назначены и это РАЗНЫЕ ассеты — иначе «чередование» выродится в один трек.
	TestFalse(TEXT("Боевой трек 1 назначен"), Defaults->CombatMusicTrackA.IsNull());
	TestFalse(TEXT("Боевой трек 2 назначен"), Defaults->CombatMusicTrackB.IsNull());
	TestNotEqual(TEXT("Боевые треки — разные ассеты"),
		Defaults->CombatMusicTrackA.ToString(), Defaults->CombatMusicTrackB.ToString());

	// «Приглушается на 30 %» = множитель фона 0.7.
	TestEqual(TEXT("Приглушение фона в бою — на 30 %"),
		Defaults->CombatAmbienceDuckFactor, 0.7f);

	// Затихание «примерно за две-три секунды».
	TestTrue(TEXT("Затихание в пределах двух-трёх секунд"),
		Defaults->CombatMusicFadeOutSeconds >= 2.0f && Defaults->CombatMusicFadeOutSeconds <= 3.0f);

	// Радиус «рядом» больше дистанции обнаружения врага (1500 см), иначе музыка мигала бы
	// на границе: враг видит игрока и входит в бой, а музыка его «рядом» не считает.
	TestTrue(TEXT("Радиус «рядом» больше дистанции обнаружения врага"),
		Defaults->CombatMusicNearbyRadius > 1500.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
