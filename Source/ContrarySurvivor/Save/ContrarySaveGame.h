// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ContrarySaveGame.generated.h"

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

	// --- Задел: инвентарь (GDD §7.8) ---
	// Пути классов предметов рюкзака. ЗАДЕЛ: полноценная сериализация инвентаря —
	// после доработки UInventoryComponent (см. эскалацию по категориям предметов).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	TArray<FString> InventoryItemClassPaths;

	// --- Задел: экипированная броня по слотам (Фаза 4) ---
	// Пути классов экипированной брони (Head/Torso/Legs); пусто = слот свободен.
	// ЗАДЕЛ: запись выполняется в APlayerCharacter::SaveGame. Авто-восстановление при
	// загрузке — будущая волна (сейчас игрок экипирует дефолтную броню в BeginPlay).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Equipment")
	FString EquippedHeadArmorClassPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Equipment")
	FString EquippedTorsoArmorClassPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Equipment")
	FString EquippedLegsArmorClassPath;

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

	// Переносит поля удержания из From в To (для SaveGame(), который создаёт свежий объект).
	static void CopyRetentionData(const UContrarySaveGame* From, UContrarySaveGame* To);
};
