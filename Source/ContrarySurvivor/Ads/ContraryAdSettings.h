// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ContraryAdSettings.generated.h"

/**
 * Настройки показа рекламы (решение Рината 04.10.2026, дословно: «хорошо будет, если это
 * будет BP или другой способ чтобы я мог быстро менять в редакторе»).
 *
 * Страница в редакторе: Project Settings → Game → «Реклама». Значения пишутся в
 * Config/DefaultGame.ini, раздел [/Script/ContrarySurvivor.ContraryAdSettings]
 * (config=Game, defaultconfig) и едут в сборку для телефона вместе с этим файлом.
 * Образец — UContraryDataSettings (Data/ContraryDataSettings.h).
 *
 * Здесь лежит всё про ДВА НОВЫХ формата: полноэкранный показ без награды после задания про
 * бандитов и маленький баннер. Четыре прежние точки ролика с наградой и их номера блоков
 * остались, где были: [/Script/ContrarySurvivor.YandexAdService] в Config/DefaultEngine.ini.
 *
 * Читают эти настройки и боевая реклама (UYandexAdService), и заглушка редактора
 * (UMockAdService), и точки показа в контроллере игрока.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Реклама"))
class CONTRARYSURVIVOR_API UContraryAdSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Раздел Project Settings, в котором лежит страница (штатная категория Game).
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	// Доступ по месту использования (объект настроек по умолчанию; всегда есть).
	static const UContraryAdSettings& Get() { return *GetDefault<UContraryAdSettings>(); }

	// === Полноэкранный показ после задания про бандитов ===

	UPROPERTY(config, EditAnywhere, Category = "Полноэкранный показ после задания про бандитов", meta = (
		DisplayName = "Показ включён",
		ToolTip = "Полноэкранная реклама без награды один раз за сохранение — когда игрок сдал старосте задание «Зачистить базу бандитов» и диалог закрылся. Выключено — показа нет."))
	bool bQuestInterstitialEnabled = true;

	UPROPERTY(config, EditAnywhere, Category = "Полноэкранный показ после задания про бандитов", meta = (
		DisplayName = "Пауза после ролика с наградой (сек)", ClampMin = "0.0",
		ToolTip = "Если игрок смотрел ролик с наградой меньше стольких секунд назад, полноэкранный показ пропускается."))
	float QuestInterstitialRewardedCooldownSeconds = 180.0f;

	UPROPERTY(config, EditAnywhere, Category = "Полноэкранный показ после задания про бандитов", meta = (
		DisplayName = "Номер рекламного блока",
		ToolTip = "Номер блока из рекламного кабинета Яндекса (вида R-M-XXXXXX-Y), формат «межстраничная реклама». Пусто — показа нет."))
	FString AdUnitIdQuestInterstitial;

	// === Баннер в окне торговли ===

	UPROPERTY(config, EditAnywhere, Category = "Баннер в окне торговли", meta = (
		DisplayName = "Баннер включён",
		ToolTip = "Маленький баннер у нижнего края экрана, пока открыто окно торговли. Игра на это время сжимается и освобождает под него место."))
	bool bBannerShopEnabled = true;

	UPROPERTY(config, EditAnywhere, Category = "Баннер в окне торговли", meta = (
		DisplayName = "Сколько первых открытий торговли без баннера", ClampMin = "0",
		ToolTip = "Счёт идёт с самого первого запуска игры на устройстве; новая игра и смерть его не сбрасывают. 3 — баннер возможен начиная с четвёртого открытия."))
	int32 BannerShopSkipFirstOpens = 3;

	UPROPERTY(config, EditAnywhere, Category = "Баннер в окне торговли", meta = (
		DisplayName = "Шанс показа баннера", ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "От 0 до 1. 0.6 — баннер появляется примерно в шести открытиях из десяти. Решается один раз при открытии окна."))
	float BannerShopShowChance = 0.6f;

	UPROPERTY(config, EditAnywhere, Category = "Баннер в окне торговли", meta = (
		DisplayName = "Номер рекламного блока",
		ToolTip = "Номер блока из рекламного кабинета Яндекса, формат «баннер». Пусто — баннера нет."))
	FString AdUnitIdBannerShop;

	// === Баннер на экране смерти ===

	UPROPERTY(config, EditAnywhere, Category = "Баннер на экране смерти", meta = (
		DisplayName = "Баннер включён",
		ToolTip = "Маленький баннер у нижнего края экрана, пока открыт экран смерти. Игра на это время сжимается и освобождает под него место."))
	bool bBannerDeathEnabled = true;

	UPROPERTY(config, EditAnywhere, Category = "Баннер на экране смерти", meta = (
		DisplayName = "Сколько первых смертей без баннера", ClampMin = "0",
		ToolTip = "Счёт идёт с самого первого запуска игры на устройстве; новая игра его не сбрасывает. 1 — баннер возможен начиная со второй смерти."))
	int32 BannerDeathSkipFirstOpens = 1;

	UPROPERTY(config, EditAnywhere, Category = "Баннер на экране смерти", meta = (
		DisplayName = "Шанс показа баннера", ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "От 0 до 1. 0.6 — баннер появляется примерно в шести смертях из десяти. Решается один раз при открытии экрана."))
	float BannerDeathShowChance = 0.6f;

	UPROPERTY(config, EditAnywhere, Category = "Баннер на экране смерти", meta = (
		DisplayName = "Номер рекламного блока",
		ToolTip = "Номер блока из рекламного кабинета Яндекса, формат «баннер». Пусто — баннера нет."))
	FString AdUnitIdBannerDeath;

	// === Общее для баннеров ===

	UPROPERTY(config, EditAnywhere, Category = "Баннер: общее", meta = (
		DisplayName = "Предельная высота баннера (доля экрана)", ClampMin = "0.05", ClampMax = "0.25",
		ToolTip = "От 0.05 до 0.25. Потолок высоты баннера; на телефоне игра сжимается ровно на столько, сколько баннер занял на деле (не меньше 50 точек экрана). В редакторе это высота серой полосы-заглушки."))
	float BannerMaxHeightFraction = 0.15f;
};
