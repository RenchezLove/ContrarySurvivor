// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AnalyticsSubsystem.generated.h"

/**
 * Аналитика GameAnalytics (Этап F3, ADR-038). Обёртка над официальным плагином:
 * весь игровой код зовёт только этот сабсистем — про SDK он не знает.
 *
 * ⚠️ СЕКРЕТЫ (ADR-013: гейм-репо публичный): значения Game Key / Secret Key НЕ хранятся
 * ни в коде, ни в конфиге репозитория. Они читаются В РАНТАЙМЕ из локальных файлов
 * GameKey.txt / SecretKey.txt в папке KeysFolder (дефолт E:/game-dev-team/keys; папка в
 * .gitignore командного репо). Нет файлов / нет плагина (WITH_GAMEANALYTICS=0) —
 * аналитика ТИХО выключена: события no-op, ошибок в лог не спамим (одна строка при старте).
 *
 * Старт сессии SDK шлёт сам после Initialize (автоматический session handling).
 * Заметка для этапа G: при пакетовании Android ключи нужно будет внести в конфиг локально
 * перед сборкой пакета (на устройстве папки E:/ нет) — решается на этапе G.
 */
UCLASS(Config = Game)
class CONTRARYSURVIVOR_API UAnalyticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Хелпер доступа из игрового кода (null вне игры/без GameInstance).
	static UAnalyticsSubsystem* Get(const UObject* WorldContextObject);

	bool IsAnalyticsEnabled() const { return bEnabled; }

	// --- События (F3): все тихо no-op при выключенной аналитике ---

	// Смерть игрока.
	void RecordPlayerDeath();

	// Взятие квеста (QuestId — латинский стабильный id, напр. KillWolves).
	void RecordQuestAccepted(FName QuestId);

	// Сдача квеста.
	void RecordQuestTurnedIn(FName QuestId);

	// Покупка в магазине: имя позиции + итоговая цена (value события).
	void RecordPurchase(const FString& ItemName, float TotalPrice);

	// Убийство врага (тип: Wolf/Bandit).
	void RecordEnemyKill(const FString& EnemyType);

	// Ежедневный вход (value = день серии).
	void RecordDailyLogin(int32 StreakDay);

	// --- Build 1.2: rewarded-реклама (ТЗ издателя №1-№3, раздел «Аналитика») ---
	// GA design-события несут ОДНО число (value), словарей параметров у них нет, поэтому
	// имена ТЗ вида ad_backpack_button_shown ложатся в иерархию "ad:backpack:button_shown"
	// (двоеточие — родной пятиуровневый формат id событий GameAnalytics), а главный
	// параметр события едет числом value; причина непоказа — сегментом id.
	//
	// СООТВЕТСТВИЕ ИМЁН для сверки метрик издателем (имя ТЗ -> событие GA [value]):
	//   ad_backpack_button_shown   -> ad:backpack:button_shown   [предметов под угрозой]
	//   ad_backpack_button_clicked -> ad:backpack:button_clicked
	//   ad_backpack_started        -> ad:backpack:started
	//   ad_backpack_completed      -> ad:backpack:completed
	//   ad_backpack_dismissed      -> ad:backpack:dismissed
	//   ad_backpack_failed         -> ad:backpack:failed
	//   ad_backpack_not_shown      -> ad:backpack:not_shown:{under_15min|no_items|no_ad|limit}
	//   ad_shop_button_shown       -> ad:shop:button_shown       [сумма сделки]
	//   ad_shop_button_clicked     -> ad:shop:button_clicked
	//   ad_shop_started            -> ad:shop:started
	//   ad_shop_completed          -> ad:shop:completed          [начисленная сумма]
	//   ad_shop_dismissed          -> ad:shop:dismissed
	//   ad_shop_failed             -> ad:shop:failed
	//   ad_shop_not_shown          -> ad:shop:not_shown:{under_15min|below_min_total|no_ad|limit|cooldown}
	//   ad_daily_button_shown      -> ad:daily:button_shown      [день серии]
	//   ad_daily_button_clicked    -> ad:daily:button_clicked
	//   ad_daily_started           -> ad:daily:started
	//   ad_daily_completed         -> ad:daily:completed         [начисленный итог]
	//   ad_daily_dismissed         -> ad:daily:dismissed
	//   ad_daily_failed            -> ad:daily:failed
	//   ad_daily_not_shown         -> ad:daily:not_shown:{first_day|under_15min|no_ad}
	//   shop_sell_completed        -> shop:sell_completed        [сумма продажи]
	//   daily_reward_claimed       -> retention:daily_reward_claimed [день серии]
	// Параметры ТЗ сверх одного числа (номер смерти за сессию, базовая сумма при
	// удвоении и т.п.) в GA не влезают — они пишутся в QA-лог рядом с отправкой.

	// Этап точки рекламы: Point = backpack/shop/daily, Stage = button_shown/button_clicked/
	// started/completed/dismissed/failed. bWithValue — прицепить число (например, сумму).
	void RecordAdStage(const FString& Point, const FString& Stage,
		float Value = 0.0f, bool bWithValue = false);

	// Кнопка точки НЕ показана: причина сегментом (no_items/no_ad/limit/cooldown/
	// under_15min/below_min_total/first_day) — "ad:<point>:not_shown:<reason>".
	void RecordAdNotShown(const FString& Point, const FString& Reason);

	// Любая завершённая продажа у торговца, включая обычные (value = сумма сделки) —
	// издатель считает по ней долю денег экономики, приходящую через рекламу (ТЗ №2 п.6).
	void RecordShopSellCompleted(float Amount);

	// Любое получение ежедневной награды, включая обычное (value = день серии) — по нему
	// издатель считает возврат игроков на 2/3/7 день (ТЗ №3 п.6).
	void RecordDailyRewardClaimed(int32 StreakDay);

	// Папка с файлами ключей GameKey.txt / SecretKey.txt. Переопределяется в
	// Config/DefaultGame.ini: [/Script/ContrarySurvivor.AnalyticsSubsystem] KeysFolder=...
	UPROPERTY(Config)
	FString KeysFolder = TEXT("E:/game-dev-team/keys");

private:
	// Отправка design-события GA ("part1:part2[:part3]"). bWithValue — вариант с числом.
	void SendDesignEvent(const FString& EventId, float Value = 0.0f, bool bWithValue = false);

	// GA разрешает в id событий только латиницу/цифры/немного знаков — русские имена
	// предметов транслитерировать не пытаемся, просто заменяем недопустимое на '_'.
	static FString SanitizeEventPart(const FString& Raw);

	bool bEnabled = false;
};
