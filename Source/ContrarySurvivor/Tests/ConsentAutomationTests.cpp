// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты пункта Б6 задания издателя (ADR-059): экран согласия на
// обработку данных и ссылка на политику. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Consent; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Что здесь доказывается:
//   * согласие НЕ проставлено за игрока: сохранённое состояние по умолчанию «ещё не
//     спрашивали», а прежней строки bUserConsent=True в конфиге рекламы больше нет;
//   * в тексте экрана названы ВСЕ сборщики данных, названные и в опубликованной политике
//     (рекламная сеть Яндекса и GameAnalytics) — прямое условие издателя по РИ-30, — и НЕ
//     названа AppMetrica: политика говорит, что она не используется, и текст в игре обязан
//     совпадать с опубликованным документом (требование издателя 13.08.2026);
//   * адрес политики читается из конфига, а не зашит в коде;
//   * версия сборки для строки в меню паузы берётся из настроек магазина.
//
// НЕ покрывается headless: сам вид экрана и нажатия кнопок (Slate без живого запуска не
// кликается, как и весь остальной UI проекта).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h"
#include "ContrarySurvivor/Analytics/DataConsentSettings.h"
#include "ContrarySurvivor/UI/ConsentScreenWidget.h"
#include "Misc/ConfigCacheIni.h"

static constexpr EAutomationTestFlags ConsentTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// --- 1. Согласие не ставится за игрока --------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsentNotAssumedTest,
	"ContrarySurvivor.Consent.NotAssumedForPlayer", ConsentTestFlags)

bool FConsentNotAssumedTest::RunTest(const FString& Parameters)
{
	// Свежая память на установку игры: игрока ещё не спрашивали, значит статистика молчит.
	UAnalyticsProfileSave* Save = NewObject<UAnalyticsProfileSave>();
	if (!TestNotNull(TEXT("Объект памяти на установку создан"), Save))
	{
		return false;
	}
	TestTrue(TEXT("По умолчанию согласия нет — состояние «ещё не спрашивали»"),
		Save->ConsentState == EDataConsentState::Unknown);

	// Прежняя строка bUserConsent=True в разделе рекламы была именно «согласием за игрока».
	// Издатель потребовал её снять — проверяем, что её в конфиге больше нет.
	bool bLegacyConsent = false;
	const bool bLegacyKeyPresent = GConfig->GetBool(TEXT("/Script/ContrarySurvivor.YandexAdService"),
		TEXT("bUserConsent"), bLegacyConsent, GEngineIni);
	TestFalse(TEXT("В конфиге рекламы больше нет строки согласия за игрока (bUserConsent)"),
		bLegacyKeyPresent);

	// Экран согласия при первом запуске обязан спрашивать.
	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	if (TestNotNull(TEXT("Настройки согласия читаются"), Settings))
	{
		TestTrue(TEXT("Согласие при первом запуске спрашивается"), Settings->bAskConsentOnFirstLaunch);
	}
	return true;
}

// --- 2. В тексте названы все сборщики данных --------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsentTextNamesCollectorsTest,
	"ContrarySurvivor.Consent.TextNamesAllDataCollectors", ConsentTestFlags)

bool FConsentTextNamesCollectorsTest::RunTest(const FString& Parameters)
{
	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	if (!TestNotNull(TEXT("Настройки согласия читаются"), Settings))
	{
		return false;
	}

	const FConsentScreenStyle& Screen = Settings->ConsentScreenStyle;
	const FString WholeText = Screen.TitleText.ToString() + TEXT(" ")
		+ Screen.BodyText1.ToString() + TEXT(" ")
		+ Screen.BodyText2.ToString() + TEXT(" ")
		+ Screen.BodyText3.ToString();

	// Условие издателя по РИ-30: назвать сборщиков данных поимённо.
	TestTrue(TEXT("Названа рекламная сеть Яндекса"), WholeText.Contains(TEXT("Яндекс")));
	TestTrue(TEXT("Названа GameAnalytics"), WholeText.Contains(TEXT("GameAnalytics")));

	// ⛔ И ОБРАТНОЕ УТВЕРЖДЕНИЕ (требование издателя 13.08.2026): AppMetrica в тексте
	// НАЗЫВАТЬСЯ НЕ ДОЛЖНА. Опубликованная политика прямо говорит, что она не используется,
	// а экран согласия утверждал обратное — документ и игра обязаны совпадать. Это смена
	// утверждённой правды, а не ослабление проверки: раньше здесь требовалось её наличие.
	TestFalse(TEXT("AppMetrica в тексте согласия не упоминается"),
		WholeText.Contains(TEXT("AppMetrica")));

	// Обещание, что отказ не ломает игру, тоже должно остаться в тексте.
	TestTrue(TEXT("Сказано, что при отказе играть можно так же"),
		WholeText.Contains(TEXT("играть можно")));

	// Обе кнопки подписаны, и подписи не совпадают.
	TestFalse(TEXT("Кнопка согласия подписана"), Screen.AcceptText.IsEmpty());
	TestFalse(TEXT("Кнопка отказа подписана"), Screen.DeclineText.IsEmpty());
	TestNotEqual(TEXT("Подписи кнопок различаются"),
		Screen.AcceptText.ToString(), Screen.DeclineText.ToString());
	TestFalse(TEXT("Ссылка на политику подписана"), Screen.PolicyLinkText.IsEmpty());
	return true;
}

// --- 3. Ссылка на политику живёт в конфиге, а не в коде ---------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsentPolicyUrlFromConfigTest,
	"ContrarySurvivor.Consent.PolicyUrlComesFromConfig", ConsentTestFlags)

bool FConsentPolicyUrlFromConfigTest::RunTest(const FString& Parameters)
{
	// Раздел настройки обязан существовать в Config/DefaultGame.ini: именно туда издатель
	// впишет адрес, когда страница появится, без пересборки игры.
	FString UrlFromIni;
	const bool bKeyPresent = GConfig->GetString(TEXT("/Script/ContrarySurvivor.DataConsentSettings"),
		TEXT("PrivacyPolicyUrl"), UrlFromIni, GGameIni);
	TestTrue(TEXT("Строка адреса политики есть в конфиге игры"), bKeyPresent);

	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	if (!TestNotNull(TEXT("Настройки согласия читаются"), Settings))
	{
		return false;
	}
	TestEqual(TEXT("Значение в настройках совпадает с конфигом"), Settings->PrivacyPolicyUrl, UrlFromIni);

	AddInfo(Settings->PrivacyPolicyUrl.IsEmpty()
		? TEXT("Адрес политики пока пуст — нажатие на строку ничего не открывает (так и задумано).")
		: FString::Printf(TEXT("Адрес политики задан: %s"), *Settings->PrivacyPolicyUrl));
	return true;
}

// --- 4. Номер версии сборки для строки в меню паузы -------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsentBuildVersionTest,
	"ContrarySurvivor.Consent.BuildVersionAvailableForPauseMenu", ConsentTestFlags)

bool FConsentBuildVersionTest::RunTest(const FString& Parameters)
{
	// Строку версии в паузе собираем из тех же настроек, что уходят в магазин.
	FString VersionName;
	FString StoreVersion;
	const bool bHasName = GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"),
		TEXT("VersionDisplayName"), VersionName, GEngineIni);
	const bool bHasStore = GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"),
		TEXT("StoreVersion"), StoreVersion, GEngineIni);

	TestTrue(TEXT("Отображаемая версия задана в конфиге"), bHasName && !VersionName.IsEmpty());
	TestTrue(TEXT("Номер сборки для магазина задан в конфиге"), bHasStore && !StoreVersion.IsEmpty());
	AddInfo(FString::Printf(TEXT("Версия сборки для меню паузы: %s (сборка %s)"), *VersionName, *StoreVersion));

	const UDataConsentSettings* Settings = UDataConsentSettings::Get();
	if (TestNotNull(TEXT("Настройки согласия читаются"), Settings))
	{
		const FString Format = Settings->PauseMenuVersionFormat.ToString();
		TestTrue(TEXT("В строке версии есть место под номер версии"), Format.Contains(TEXT("{Version}")));
		TestTrue(TEXT("В строке версии есть место под номер сборки"), Format.Contains(TEXT("{Build}")));
		TestFalse(TEXT("Строка политики в паузе подписана"), Settings->PauseMenuPolicyText.IsEmpty());
		TestNotEqual(TEXT("Подписи переключателя согласия различаются"),
			Settings->PauseMenuConsentOnText.ToString(), Settings->PauseMenuConsentOffText.ToString());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
