// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест целостности АДРЕСОВ ИЗ НАСТРОЕК. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.ConfigUrls; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// ЗАЧЕМ. 08-09 Ринат живьём поймал: кнопка «Сообщество» открывала браузер с адресом
// «https:» — всё после двойного слэша пропадало. Причина не в нашем коде: движок считает
// незакавыченный «//» в файле настроек началом комментария и режет строку
// (Core/Private/Misc/ConfigCacheIni.cpp:1776-1780, дословно «it contains unquoted '//'
// (interpreted as a comment when importing)»). Лечится кавычками вокруг адреса.
//
// Тест берёт значения ИМЕННО ТАК, КАК ИХ ЧИТАЕТ ИГРА — через те же методы настроек, а не
// подставляет строку в память: иначе он проверял бы себя, а не файл настроек. Пустой адрес
// — законная настройка («пункт спрятан»), такие пропускаем. Непустой обязан быть целым:
// начинаться с https:// и иметь имя узла с точкой. Обрезанный «https:» этого не проходит.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Analytics/DataConsentSettings.h"
#include "ContrarySurvivor/UI/EndOfStoryWidget.h"
#include "ContrarySurvivor/UI/StartScreenWidget.h"

static constexpr EAutomationTestFlags ConfigUrlTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace ConfigUrlTest
{
	// Целый ли адрес: протокол на месте, после него есть узел с точкой и без пробелов.
	// Именно это и ломалось: у обрезанного «https:» нет ни двойного слэша, ни узла.
	static bool IsWholeUrl(const FString& Url, FString& OutReason)
	{
		if (!Url.StartsWith(TEXT("https://")))
		{
			OutReason = TEXT("нет начала «https://» — похоже, адрес обрезан на двойном слэше (нужны кавычки в файле настроек)");
			return false;
		}
		const FString Host = Url.RightChop(FString(TEXT("https://")).Len());
		if (Host.IsEmpty())
		{
			OutReason = TEXT("после «https://» пусто — адреса нет");
			return false;
		}
		if (!Host.Contains(TEXT(".")))
		{
			OutReason = TEXT("в имени узла нет точки — это не адрес сайта");
			return false;
		}
		if (Host.Contains(TEXT(" ")))
		{
			OutReason = TEXT("в адресе пробел — строка собрана неверно");
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigUrlsAreWholeTest,
	"ContrarySurvivor.ConfigUrls.AddressesSurviveConfigReading", ConfigUrlTestFlags)

bool FConfigUrlsAreWholeTest::RunTest(const FString& Parameters)
{
	// Все адреса, которые игра читает из настроек, — теми же методами, что и в бою.
	struct FUrlEntry
	{
		const TCHAR* Human; // как называть в сообщении об ошибке
		FString Value;
	};
	const UDataConsentSettings* Consent = UDataConsentSettings::Get();
	const TArray<FUrlEntry> Urls =
	{
		{ TEXT("«Сообщество» в главном меню и в паузе"), UMainMenuSettings::GetCommunityUrl() },
		{ TEXT("«Сообщить об ошибке» на экране настроек"), UMainMenuSettings::GetBugReportUrl() },
		{ TEXT("«Другие способы поддержать» в окне «Поддержать автора»"), UMainMenuSettings::GetSupportPostUrl() },
		{ TEXT("«Политика конфиденциальности»"), Consent ? Consent->PrivacyPolicyUrl.TrimStartAndEnd() : FString() },
		{ TEXT("канал на карточке конца сюжета"), UEndOfStorySettings::GetChannelUrl() },
	};

	int32 FilledCount = 0;
	for (const FUrlEntry& Entry : Urls)
	{
		if (Entry.Value.IsEmpty())
		{
			// Пустой адрес — законная настройка: пункт просто спрятан. Это не поломка.
			AddInfo(FString::Printf(TEXT("Адрес не задан (пункт спрятан): %s"), Entry.Human));
			continue;
		}
		++FilledCount;
		FString Reason;
		const bool bWhole = ConfigUrlTest::IsWholeUrl(Entry.Value, Reason);
		TestTrue(FString::Printf(TEXT("Адрес цел — %s: «%s»%s%s"),
			Entry.Human, *Entry.Value,
			bWhole ? TEXT("") : TEXT(", беда: "), bWhole ? TEXT("") : *Reason), bWhole);
		if (bWhole)
		{
			AddInfo(FString::Printf(TEXT("Адрес цел: %s — «%s»"), Entry.Human, *Entry.Value));
		}
	}

	AddInfo(FString::Printf(TEXT("Заполненных адресов в настройках: %d из %d"),
		FilledCount, Urls.Num()));

	// Отдельно ловим ровно тот обрубок, который приехал к Ринату: «https:» без всего
	// остального. Если однажды кавычки в файле настроек снимут, тест назовёт беду словами.
	FString ReasonForCut;
	TestFalse(TEXT("Обрубок «https:» проверку не проходит (защита самого теста)"),
		ConfigUrlTest::IsWholeUrl(TEXT("https:"), ReasonForCut));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
