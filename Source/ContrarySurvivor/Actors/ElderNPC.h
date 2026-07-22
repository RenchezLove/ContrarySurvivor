// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // база: модульный гуманоид (как AMasterTrader)
#include "InteractableNPCInterface.h"                            // HUD-маркер находимости
#include "ContrarySurvivor/Components/QuestComponent.h"          // FQuest (предлагаемый квест)
#include "AConsumableItem.h"                                     // EConsumableType (подарок первой встречи)
#include "ElderNPC.generated.h"

class USphereComponent;
class UQuestComponent;
class APlayerCharacter;

/**
 * Что делает нажатие кнопки под конкретной репликой интро (Build 1, скриптовый диалог первой
 * встречи). Эффект срабатывает по действию ИГРОКА — когда он жмёт кнопку этой реплики, — после
 * чего диалог переходит к следующей реплике.
 */
UENUM(BlueprintType)
enum class EElderIntroAction : uint8
{
	None       UMETA(DisplayName = "Просто продолжить"),   // никакого эффекта, просто дальше
	GiveGift   UMETA(DisplayName = "Выдать подарок (аптечку)"), // положить бинт/аптечку в рюкзак + подсказка
	StartQuest UMETA(DisplayName = "Начать первый квест")   // принять квест «Шкуры волков»
};

/**
 * Одна реплика скриптового интро-диалога старосты (Build 1). Реплики проигрываются ПО ОЧЕРЕДИ:
 * под каждой репликой ровно ОДНА кнопка-ответ игрока (ветвления нет — решение Рината). Тексты —
 * переводимые (ADR-050), редактируются на размещённом BP_Elder (директива Рината: игровые
 * параметры EditAnywhere).
 */
USTRUCT(BlueprintType)
struct FElderIntroLine
{
	GENERATED_BODY()

	// Реплика старосты (крупный текст в окне диалога).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Intro", meta = (MultiLine = "true"))
	FText NPCText;

	// Единственная кнопка-ответ игрока под этой репликой (квадратные скобки рисует WBP_Dialog).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Intro")
	FText ButtonLabel;

	// Что происходит при нажатии кнопки этой реплики (выдать аптечку / начать квест / просто дальше).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Intro")
	EElderIntroAction Action = EElderIntroAction::None;
};

/**
 * Староста деревни (Фаза 5, GDD §7.7 — квестодатель MVP).
 *
 * A3: переведён с болванки AActor на AMasterHumanoidCharacter (модульный гуманоид, как
 * AMasterTrader). Визуал (Head/Torso/Legs + AnimBP) назначается в BP_Elder оператором —
 * C++ больше НЕ хардкодит меш/ABP. Не несёт UStatsComponent и НЕ блокирует ECC_Visibility,
 * как и торговец, поэтому не попадает в авто-лок/хелсбары игрока и простреливается насквозь.
 *
 * Взаимодействие (E) открывает ДИАЛОГ (а не магазин). Диалог предлагает Collect-квест
 * «3 шкуры волков» (без kill-цели — гейт по собранным шкурам), после сдачи — «зачистить базу
 * бандитов». При входе игрока в радиус (overlap) староста регистрируется у
 * AContrarySurvivorPlayerController (NearbyElder).
 *
 * Риски модульного гуманоида (проверить в PIE): капсула Character'а ТВЁРДАЯ (может загородить
 * проход) — приемлемо для статичного NPC; авто-AIController подавлен AutoPossessAI=Disabled.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AElderNPC : public AMasterHumanoidCharacter, public IInteractableNPCInterface
{
	GENERATED_BODY()

public:
	AElderNPC();

	// Первый квест старосты (DRAFT: «Шкуры волков»). Заполняется в конструкторе.
	const FQuest& GetOfferedQuest() const { return OfferedQuest; }

	// Квест, актуальный для игрока СЕЙЧАС (выдача по порядку, Фаза 5):
	//  - пока кв.1 не сдан (TurnedIn) — возвращает кв.1;
	//  - после сдачи кв.1 — возвращает кв.2 («Зачистить базу бандитов»).
	// Возвращается ссылка на член (валидна, пока жив актор). PlayerQuests может быть null
	// (тогда возвращается кв.1).
	const FQuest& GetQuestForPlayer(const UQuestComponent* PlayerQuests) const;

	// Подарок первой встречи (решение владельца 2026-07-20). В первой реплике староста
	// говорит «Держи, затяни раны» — значит предмет должен реально появиться в рюкзаке,
	// иначе слова расходятся с делом. Кладём ОДИН раз за профиль: признак живёт в сейве
	// (UContrarySaveGame::bElderFirstGiftGiven), поэтому переживает смерть игрока и
	// перезапуск игры и подарок нельзя нафармить повторным открытием диалога.
	// Зовёт контроллер при открытии диалога. Возвращает true, если выдали именно сейчас.
	bool TryGiveFirstMeetingGift(APlayerCharacter* Player);

	// --- IInteractableNPCInterface (HUD-маркер находимости) ---
	virtual FText GetNPCMarkerLabel() const override { return NPCMarkerLabel; }
	virtual float GetNPCMarkerZOffset() const override { return NPCMarkerZOffset; }

	// --- Тексты диалога для HUD (директива Рината 07-18: настраиваются на РАЗМЕЩЁННОМ
	// экземпляре BP_Elder; реплика NotStarted = Description текущего квеста) ---

	const FText& GetDialogueDisplayName() const { return DialogueDisplayName; }
	const FText& GetDialogueActivePrefix() const { return DialogueActivePrefix; }
	const FText& GetDialogueCompletedText() const { return DialogueCompletedText; }
	const FText& GetDialogueTurnedInText() const { return DialogueTurnedInText; }
	const FText& GetDialogueEarlyHookText() const { return DialogueEarlyHookText; }

	// Скриптовое интро первой встречи (Build 1): реплики старосты по очереди, по одной кнопке-
	// ответу на реплику. Проигрывается ТОЛЬКО пока первый квест ещё не принят (первая встреча).
	const TArray<FElderIntroLine>& GetIntroLines() const { return IntroLines; }

protected:
	virtual void PostInitializeComponents() override;

	// Имя NPC в шапке окна диалога.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "1"))
	FText DialogueDisplayName = NSLOCTEXT("Dialog", "ElderName", "СТАРОСТА");

	// Начало реплики о незаконченном квесте; дальше диалог подставляет название квеста
	// и его цели (формат самой сборки — настройка панели диалога).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "2", MultiLine = "true"))
	FText DialogueActivePrefix = NSLOCTEXT("Dialog", "ElderActivePrefix", "Ты ещё не закончил. ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "3", MultiLine = "true"))
	FText DialogueCompletedText = NSLOCTEXT("Dialog", "ElderCompleted",
		"Отлично! Задание выполнено. Вот твоя награда.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "4", MultiLine = "true"))
	FText DialogueTurnedInText = NSLOCTEXT("Dialog", "ElderTurnedIn",
		"Спасибо тебе ещё раз. Деревня тебе благодарна.");

	// ЛЕГАСИ (Build 1): раньше эта фраза-крючок дописывалась к реплике активного квеста и потому
	// повторялась при КАЖДОМ повторном разговоре (баг издателя). Теперь крючок — это ПОСЛЕДНЯЯ
	// реплика скриптового интро (IntroLines) и показывается один раз (признак bElderHookShown в
	// сейве). Поле оставлено, чтобы не терять текст/перевод; в потоке диалога больше НЕ участвует.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "5", MultiLine = "true"))
	FText DialogueEarlyHookText = NSLOCTEXT("Dialog", "ElderEarlyHook",
		"И вот что странно… За последние недели чужаки всё идут и идут к нам. Будто гонит их что-то. Или кто-то. Не моего ума дело. Ступай.");

	// Скриптовое интро первой встречи (Build 1, ТЗ издателя раздел 3): реплики старосты по очереди,
	// под каждой — ровно одна кнопка-ответ игрока. Заполняется в конструкторе дословным текстом ТЗ;
	// Ринат правит реплики/подписи кнопок прямо на BP_Elder. Играется, пока первый квест не принят.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Intro", meta = (DisplayPriority = "1", TitleProperty = "ButtonLabel"))
	TArray<FElderIntroLine> IntroLines;

	// --- Подарок первой встречи (см. TryGiveFirstMeetingGift) ---

	// Что кладём в рюкзак. Medkit — это наш «Бинт» (название предмета берётся из
	// AConsumableItem, здесь оно не дублируется).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Gift", meta = (DisplayPriority = "1"))
	EConsumableType FirstGiftConsumableType = EConsumableType::Medkit;

	// Сколько штук. 0 — подарок выключен, диалог работает как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Gift", meta = (DisplayPriority = "2", ClampMin = "0"))
	int32 FirstGiftCount = 1;

	// Всплывающая подсказка о полученном предмете: {Item} — название предмета,
	// {Count} — сколько штук. Пусто — подсказка не показывается.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Gift", meta = (DisplayPriority = "3"))
	FText FirstGiftHintFormat = NSLOCTEXT("Dialog", "ElderFirstGiftHint", "Получено: {Item}");

	// Подпись и подъём HUD-маркера находимости (были зашиты в override интерфейса).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "6"))
	FText NPCMarkerLabel = NSLOCTEXT("NPC", "ElderMarker", "Староста");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "7"))
	float NPCMarkerZOffset = 320.0f;

	// Триггер диалоговой зоны: overlap по Pawn (игроку). По образцу AMasterTrader::InteractTrigger.
	// meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината), сразу после Transform.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elder", meta = (DisplayPriority = "1"))
	USphereComponent* InteractTrigger;

	// Радиус, в котором доступно взаимодействие (см). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elder")
	float InteractRadius = 220.0f;

	// Квест 1 (DRAFT: «Шкуры волков» — собрать 3 шкуры, награда 150). Тюнингуется в редакторе.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (DisplayPriority = "2"))
	FQuest OfferedQuest;

	// Квест 2 (DRAFT: «Зачистить базу бандитов» — убить 3 бандитов + принести Ноутбук, награда 250).
	// Выдаётся ПОСЛЕ сдачи кв.1 (GetQuestForPlayer). Тюнингуется в редакторе.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FQuest SecondQuest;

	// Квест 3 (Этап F, ADR-044 п.1-2: крючок «кто охотится за ГГ» + сторонняя активность на время
	// «расследования» старосты). Collect: 3 «Шкуры волка» для торговца, награда 100. Выдаётся
	// ПОСЛЕ сдачи кв.2. Метка цели — второе логово (BP_WolfDen с тегом WolfDen2 ставит game-lead;
	// пока актора с тегом нет на карте, HUD метку просто не рисует). Тюнингуется в редакторе.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FQuest ThirdQuest;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
