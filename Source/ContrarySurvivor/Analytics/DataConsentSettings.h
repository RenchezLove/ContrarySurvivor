// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ContrarySurvivor/UI/ConsentScreenWidget.h" // FConsentScreenStyle — тексты экрана
#include "DataConsentSettings.generated.h"

/**
 * Настройки согласия и политики конфиденциальности (Б6, задание издателя ADR-059).
 *
 * Живут в КОНФИГЕ, а не в коде: Config/DefaultGame.ini, раздел
 * [/Script/ContrarySurvivor.DataConsentSettings]. Адрес политики издатель просил держать
 * именно так — вписать его можно текстовым редактором, пересобирать код не нужно (источник
 * истины `docs/contrary-survivor/soglasie-i-politika.md`, раздел 2, правило 6).
 *
 * Тот же приём уже применён в проекте к адресу канала (UEndOfStorySettings) и к
 * идентификаторам рекламы: UCLASS(Config = ...) плюс UPROPERTY(Config).
 */
UCLASS(Config = Game, DefaultConfig)
class CONTRARYSURVIVOR_API UDataConsentSettings : public UObject
{
	GENERATED_BODY()

public:
	// Быстрый доступ (значения читаются из конфига при загрузке класса).
	static const UDataConsentSettings* Get();

	// Адрес страницы с политикой конфиденциальности. ПУСТО ПО УМОЛЧАНИЮ — пока ссылки нет,
	// кнопка «Политика конфиденциальности» ведёт себя тихо: ничего не открывает и не
	// показывает пустую страницу (прямое указание game-lead).
	UPROPERTY(Config, EditAnywhere, Category = "Политика",
		meta = (DisplayName = "Ссылка на политику конфиденциальности", DisplayPriority = "1"))
	FString PrivacyPolicyUrl;

	// Показывать ли экран согласия при первом запуске. Выключатель на случай проверок;
	// в боевой сборке обязан быть включён — согласие за игрока не ставится.
	UPROPERTY(Config, EditAnywhere, Category = "Экран согласия",
		meta = (DisplayName = "Спрашивать согласие при первом запуске", DisplayPriority = "2"))
	bool bAskConsentOnFirstLaunch = true;

	// Оформление и тексты экрана согласия. Тексты по умолчанию — дословно из источника истины.
	UPROPERTY(Config, EditAnywhere, Category = "Экран согласия",
		meta = (DisplayName = "Оформление и тексты экрана", DisplayPriority = "3"))
	FConsentScreenStyle ConsentScreenStyle;

	// Подпись строки политики в меню паузы.
	UPROPERTY(Config, EditAnywhere, Category = "Меню паузы",
		meta = (DisplayName = "Подпись строки политики в паузе", DisplayPriority = "4"))
	FText PauseMenuPolicyText = NSLOCTEXT("DataConsentSettings", "PauseMenuPolicyText",
		"Политика конфиденциальности");

	// Подписи переключателя согласия в меню паузы (пункт 5 источника истины: решение
	// игрока должно быть доступно к смене позже).
	UPROPERTY(Config, EditAnywhere, Category = "Меню паузы",
		meta = (DisplayName = "Переключатель согласия: сейчас согласие дано", DisplayPriority = "5"))
	FText PauseMenuConsentOnText = NSLOCTEXT("DataConsentSettings", "PauseMenuConsentOnText",
		"Сбор данных: включён");

	UPROPERTY(Config, EditAnywhere, Category = "Меню паузы",
		meta = (DisplayName = "Переключатель согласия: сейчас согласия нет", DisplayPriority = "6"))
	FText PauseMenuConsentOffText = NSLOCTEXT("DataConsentSettings", "PauseMenuConsentOffText",
		"Сбор данных: выключен");

	// Строка версии сборки в меню паузы (ADR-059: «мелко номер версии сборки»). {Version} —
	// отображаемая версия из настроек Android, {Build} — номер сборки для магазина.
	UPROPERTY(Config, EditAnywhere, Category = "Меню паузы",
		meta = (DisplayName = "Строка версии в паузе", DisplayPriority = "7"))
	FText PauseMenuVersionFormat = NSLOCTEXT("DataConsentSettings", "PauseMenuVersionFormat",
		"Версия {Version} (сборка {Build})");
};
