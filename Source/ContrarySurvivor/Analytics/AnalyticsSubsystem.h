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
	// имена ТЗ вида ad_backpack_button_shown ложатся в иерархию "ad:backpack:button_shown",
	// а главный параметр события едет числом value; причина непоказа — сегментом id.

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
