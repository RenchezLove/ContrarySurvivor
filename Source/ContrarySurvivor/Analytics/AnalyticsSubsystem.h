// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h" // EDataConsentState (Б6)
#include "AnalyticsSubsystem.generated.h"

/**
 * Аналитика GameAnalytics (Этап F3, ADR-038). Обёртка над официальным плагином:
 * весь игровой код зовёт только этот сабсистем — про SDK он не знает.
 *
 * ⚠️ СЕКРЕТЫ (ADR-013: гейм-репо публичный): значения Game Key / Secret Key НЕ хранятся
 * ни в коде, ни в конфиге репозитория.
 *
 * ОТКУДА БЕРУТСЯ КЛЮЧИ (Б4, задание издателя ADR-059) — два источника по порядку:
 *   1) вшитые в двоичный файл на этапе компиляции. ContrarySurvivor.Build.cs читает файлы
 *      GameKey.txt / SecretKey.txt из локальной папки вне репозитория (по умолчанию
 *      E:/game-dev-team/keys, переопределяется переменной окружения
 *      CONTRARY_ANALYTICS_KEYS_DIR) и передаёт значения определениями компилятора. Именно
 *      этот путь работает в собранной игре на телефоне, где никакой папки с ключами нет;
 *   2) если ключи не вшивались (сборка на машине без папки ключей) — прежний поиск тех же
 *      файлов на диске в рантайме, по пути KeysFolder. Нужен только разработчику локально.
 * Нет ни того, ни другого / нет плагина (WITH_GAMEANALYTICS=0) — аналитика ТИХО выключена:
 * события no-op, ошибок в лог не спамим (одна понятная строка при старте, без значений ключей).
 *
 * Старт сессии SDK шлёт сам после Initialize (автоматический session handling).
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
	// --- Б4 (задание издателя ADR-059): первый запуск и обучение ---
	//   first_launch               -> app:first_launch           [номер шага не нужен]
	//   tutorial_step              -> tutorial:step:{movement|pickup|elder|inventory|death} [номер шага]
	//   tutorial_completed         -> tutorial:completed         [сколько шагов всего]
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

	// --- Б4: обучение. Шаги — контекстные подсказки UOnboardingComponent (по факту их пять:
	// движение, подбор, староста, инвентарь, смерть); каждая показывается один раз за профиль.
	// Событие на шаг уходит не более одного раза ЗА УСТАНОВКУ игры (память —
	// UAnalyticsProfileSave в отдельном слоте), поэтому «Новая игра» воронку не задваивает.
	// StepId — латинское имя шага, StepIndex — его номер начиная с 1 (едет в value). ---
	void RecordTutorialStep(const FString& StepId, int32 StepIndex);

	// Обучение пройдено целиком (показан последний из шагов). Тоже не более одного раза
	// за установку. TotalSteps — сколько шагов в обучении всего (едет в value).
	void RecordTutorialCompleted(int32 TotalSteps);

	// --- Имена событий одним местом: тем же кодом строит отправка и проверяют автотесты. ---
	static FString MakeFirstLaunchEventId();
	static FString MakeTutorialStepEventId(const FString& StepId);
	static FString MakeTutorialCompletedEventId();

	// GA разрешает в id событий только латиницу/цифры/немного знаков — русские имена
	// предметов транслитерировать не пытаемся, просто заменяем недопустимое на '_'.
	static FString SanitizeEventPart(const FString& Raw);

	// Вшиты ли ключи в этот двоичный файл на этапе компиляции (Б4). Для автотеста-доказательства
	// и диагностики; сами ключи наружу не отдаются никогда.
	static bool AreKeysCompiledIn();
	static int32 GetCompiledGameKeyLength();
	static int32 GetCompiledSecretKeyLength();

	// --- Б6: согласие игрока на обработку данных (задание издателя ADR-059). Пока игрок не
	// ответил, статистика МОЛЧИТ и SDK не поднимается: согласие за игрока не ставится
	// (условие издателя по РИ-30). Решение хранится в той же памяти на установку игры,
	// поэтому переживает перезапуск и не сбрасывается кнопкой «Новая игра». Экран согласия
	// показывает UDataConsentSubsystem, он же зовёт SetDataConsent. ---

	EDataConsentState GetStoredConsentState();

	// Записать решение игрока и применить его к статистике: согласие поднимает SDK и
	// включает отправку (в том числе отложенное событие первого запуска), отказ выключает.
	void SetDataConsent(bool bGranted);

	// --- Волна «Главное меню» (ADR-062): признак «игра уже запускалась на этом устройстве».
	// Помечает текущий запуск в памяти на установку и отвечает, был ли запуск ДО него
	// (false = самый первый запуск после установки: меню не показывается, игрок идёт сразу
	// во вступление — спека главного меню). Живёт здесь, потому что владелец слота памяти
	// на установку — эта подсистема (кэш ProfileSave; два снимка затирали бы друг друга). ---
	bool MarkLaunchAndCheckWasLaunchedBefore();

	// Служебная память на установку игры (первый запуск, шаги обучения, ответ по согласию).
	// Единственный на игру снимок слота — им пользуется и подсистема согласия, чтобы два
	// снимка не затирали друг друга.
	UAnalyticsProfileSave* LoadOrCreateProfileSave();
	void WriteProfileSave(UAnalyticsProfileSave* Save);

	// Папка с файлами ключей GameKey.txt / SecretKey.txt — ЗАПАСНОЙ путь для разработчика,
	// когда ключи в сборку не вшиты (см. комментарий к классу). Переопределяется в
	// Config/DefaultGame.ini: [/Script/ContrarySurvivor.AnalyticsSubsystem] KeysFolder=...
	UPROPERTY(Config)
	FString KeysFolder = TEXT("E:/game-dev-team/keys");

private:
	// Отправка design-события GA ("part1:part2[:part3]"). bWithValue — вариант с числом.
	void SendDesignEvent(const FString& EventId, float Value = 0.0f, bool bWithValue = false);

	// Событие самого первого запуска игры на устройстве: ровно один раз за установку.
	// Зовётся, когда аналитика реально включена (то есть уже после согласия).
	void RecordFirstLaunchIfNeeded();

	// Достать ключи (сначала вшитые, потом файлы с диска) и написать в журнал одну понятную
	// строку про источник и длины. Возвращает false, если ключей нет вовсе.
	bool ResolveKeys();

	// Поднять SDK на найденных ключах и включить отправку. Зовётся только при согласии игрока.
	void StartSdkAndEnable();

	// Ключи, найденные при старте. Держим до решения игрока: согласие может прийти позже
	// (экран согласия), и тогда SDK поднимается тем же значением, без повторного поиска.
	FString ResolvedGameKey;
	FString ResolvedSecretKey;

	// Кэш служебной памяти на время сессии (шаги обучения дёргаются из игрового кода).
	UPROPERTY()
	TObjectPtr<UAnalyticsProfileSave> ProfileSave;

	bool bEnabled = false;
};
