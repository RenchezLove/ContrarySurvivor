// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // FQuest — снимок журнала квестов (Б3)
#include "AConsumableItem.h" // EConsumableType — расходник различается им, не только классом (Б3)
#include "ContrarySaveGame.generated.h"

/**
 * Одна запись предмета рюкзака (Б3: полноценное восстановление инвентаря при «Продолжить»).
 * Класс один на несколько предметов (AConsumableItem — еда/вода/аптечка, AQuestItem — шкура/
 * ноутбук), поэтому одного пути класса недостаточно — служебный ключ (ItemName) и переводимая
 * подпись (ItemDisplayText) заполняются НА ЭКЗЕМПЛЯРЕ при создании предмета (лут/покупка/выдача)
 * и сохраняются вместе с классом, иначе восстановленный предмет потерял бы тип/название.
 * bEquipped — тем же списком идёт и надетая броня (единый источник, дублирующих полей на слот
 * Head/Torso/Legs больше нет): при восстановлении предмет с bEquipped=true надевается, а не
 * просто кладётся в рюкзак.
 */
USTRUCT(BlueprintType)
struct FSavedInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	FString ClassPath;

	// Служебный ключ предмета (ADR-050, НЕ переводится) — по нему сходится логика квестов
	// и для AConsumableItem/AQuestItem различаются предметы одного класса.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	FString ItemName;

	// Строка DT_Items, из которой собран предмет (AMasterInventoryItem::SourceItemRow).
	// БЕЗ неё восстановление получало от класса только пустую заготовку: одна BP_ArmorBase
	// обслуживает все девять броней, и слот/защита/меш экипировки живут ИСКЛЮЧИТЕЛЬНО в
	// строке (баг 24.08 — надетая броня торса и штанов возвращалась тряпкой Т0 с защитой 0
	// и обе садились в слот торса). Пусто в СТАРЫХ сейвах (записаны до этого поля) — там
	// строку ищут по служебному ключу ItemName, см. APlayerCharacter::RestoreInventoryAndArmor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	FName ItemRow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	FText ItemDisplayText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	int32 StackCount = 1;

	// Экипирована ли броня (слот Head/Torso/Legs — берётся из класса при восстановлении).
	// Не-броневые предметы всегда false.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	bool bEquipped = false;

	// Тип расходника (для AConsumableItem — один класс обслуживает еду/воду/аптечку, тип
	// выставляется НА ЭКЗЕМПЛЯРЕ и классом не определяется). У прочих предметов не используется.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	EConsumableType ConsumableType = EConsumableType::Food;
};

/**
 * Память одной базы противника (ТЗ издателя 22.08.2026 «возрождение базы и её уровни»):
 * текущая ступень и момент последней ПОЛНОЙ зачистки по РЕАЛЬНЫМ часам (UTC). Живёт в
 * сейве — переживает выход из игры; «Новая игра» честно сбрасывает базы на начальную
 * ступень (слот стирается целиком). Пишет AMasterEnemyBase retention-паттерном
 * (LoadGameFromSlot → правка ТОЛЬКО своей записи → SaveGameToSlot).
 */
USTRUCT(BlueprintType)
struct FSavedEnemyBaseState
{
	GENERATED_BODY()

	// Идентификатор базы (настраиваемый BaseSaveId актора; пусто = имя размещённого актора).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|World")
	FName BaseId;

	// Текущая ступень базы (1..5).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|World")
	int32 Tier = 1;

	// Какая ступень была зачищена последней — по НЕЙ считается пауза возрождения (из новой
	// ступени зачищенную не вывести: на третью попадают и со второй, и с пятой). 0 = не было.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|World")
	int32 LastClearedTier = 0;

	// Момент последней полной зачистки, РЕАЛЬНОЕ время UTC. Ticks == 0 — зачисток не было.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|World")
	FDateTime LastClearUtc;
};

/**
 * Сейв игрока (GDD §7.8). Хранит статы выживания + позицию игрока (точка респауна).
 * Запись/чтение через UGameplayStatics::SaveGameToSlot / LoadGameFromSlot.
 * Костёр (ACampfire) делает автосейв при входе в безопасную зону.
 */
UCLASS()
class CONTRARYSURVIVOR_API UContrarySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Есть ли валидные данные (false у свежесозданного объекта без записи).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	bool bHasData = false;

	// --- Статы (UStatsComponent) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Health = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Hunger = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Thirst = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Money = 50.0f;

	// --- Позиция/точка респауна ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Transform")
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Transform")
	FRotator PlayerRotation = FRotator::ZeroRotator;

	// --- Рюкзак + экипированная броня (Б3, GDD §7.8) ---
	// Единый список: содержит и обычные предметы рюкзака, и надетую броню (bEquipped=true) —
	// у брони в рюкзаке (UInventoryComponent) и так один список на экипированное/неэкипированное
	// (Фаза 4), отдельные поля-слоты Head/Torso/Legs только дублировали бы его и рисковали
	// разойтись. Восстановление — APlayerCharacter::LoadGameForContinue.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	TArray<FSavedInventoryEntry> InventoryEntries;

	// --- Журнал квестов (Б3) ---
	// Полный снимок FQuest (не только id/состояние): OfferQuest добавляет квест в журнал, только
	// если его там ещё нет, поэтому частичное восстановление («только состояние») оставило бы
	// заголовок/описание/тексты кнопок пустыми до повторного разговора со старостой.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Quests")
	TArray<FQuest> Quests;

	// --- Этап F: удержание (ежедневная награда ADR-044 п.4 + одноразовые подсказки F1) ---
	// Эти поля заполняет НЕ SaveGame() игрока, а UDailyRewardComponent/UOnboardingComponent
	// (запись в тот же слот). SaveGame() переносит их из прежнего сейва через CopyRetentionData —
	// иначе каждый автосейв костра обнулял бы серию и подсказки.

	// Календарная дата последнего засчитанного ежедневного входа (локальная дата устройства,
	// FDateTime::Now().GetDate()). Ticks == 0 — входов ещё не было.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	FDateTime LastDailyRewardDate;

	// Серия дней подряд (1 = первый день). 0 — награда ещё ни разу не выдавалась.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	int32 DailyStreakDays = 0;

	// Флаги «подсказка онбординга уже показана» (F1) — каждая ОДИН раз за профиль.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bHintMovementShown = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bHintPickupShown = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bHintElderShown = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bHintInventoryShown = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bHintDeathShown = false;

	// ADR-074 (16.08.2026): подсказка «прогресс сохраняется только у костра» при первом выходе
	// из деревни уже показана. Новое поле в конце блока подсказок; старые сейвы читают его
	// как false (штатная сериализация UPROPERTY), поэтому подсказку увидят и старые профили —
	// один раз.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bHintLeaveVillageShown = false;

	// Староста уже отдал подарок первой встречи (бинт к реплике «Держи, затяни раны»).
	// Признак живёт в сейве, а не в памяти актора: иначе подарок выдавался бы заново
	// после смерти игрока и после перезапуска игры, и его можно было бы фармить.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bElderFirstGiftGiven = false;

	// Финальная реплика-крючок интро («кто-то гонит чужаков») уже показана в этом профиле.
	// Build 1: раньше крючок дописывался к каждому повторному разговору и потому повторялся
	// (баг издателя). Теперь он — последняя реплика интро и показывается один раз; признак в
	// сейве переживает смерть/перезапуск, поэтому при повторном разговоре крючок не повторяется.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bElderHookShown = false;

	// Сценка-намёк после сдачи кв.2 («на ноутбуке — данные о тех, кто за тобой охотится» +
	// «волки к югу, торговец платит за шкуры») уже показана в этом профиле. Build 1: вместо
	// формального квеста 3 староста один раз проговаривает намёк (NotebookHintLines старосты);
	// при повторных разговорах — короткое напоминание. Признак по образцу bElderHookShown.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bElderNotebookHintShown = false;

	// Build 1.2: сообщение о конце сюжета (плашка через ~30 с после сценки старосты про
	// шкуры) показывается ОДИН раз за сохранение. Читает/пишет ContrarySurvivorHUD.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Retention")
	bool bEndOfStoryShown = false;

	// --- Build 1.2: rewarded-реклама (ТЗ издателя №1-№3). Поля пишет APlayerCharacter
	// (накопитель времени) и точки рекламы (счётчики); SaveGame() переносит их через
	// CopyRetentionData, как и остальное удержание. ---

	// Суммарное игровое время профиля с установки (сек). Накопитель для глобального
	// запрета рекламы первые 15 минут (ТЗ раздел 0 п.2). Копится в Tick игрока и
	// сбрасывается в слот раз в PlaytimeFlushInterval.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Ads")
	float TotalPlayTimeSeconds = 0.0f;

	// Точка 1 «Спасти рюкзак»: календарная дата счётчика (локальное время устройства)
	// и число использований ЗА ЭТУ дату (лимит 3/сутки, ТЗ №1 п.3). Дата сменилась —
	// счётчик логически 0 (AdGating::UsesToday). Ticks == 0 — использований не было.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Ads")
	FDateTime BackpackAdCounterDate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Ads")
	int32 BackpackAdUsesOnDate = 0;

	// Точка 2 «Продать дороже»: суточный счётчик (лимит 4/сутки) + момент прошлого
	// просмотра (кулдаун 3 минуты, ТЗ №2 п.4).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Ads")
	FDateTime ShopAdCounterDate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Ads")
	int32 ShopAdUsesOnDate = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Ads")
	FDateTime LastShopAdTime;

	// --- Мир: память баз противника (ТЗ издателя 22.08.2026, возрождение баз) ---
	// Заполняет AMasterEnemyBase (retention-паттерн); SaveGame() игрока переносит через
	// CopyRetentionData — иначе автосейв костра стирал бы ступени и таймеры возрождения.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|World")
	TArray<FSavedEnemyBaseState> EnemyBaseStates;

	// Переносит поля удержания из From в To (для SaveGame(), который создаёт свежий объект).
	static void CopyRetentionData(const UContrarySaveGame* From, UContrarySaveGame* To);
};
