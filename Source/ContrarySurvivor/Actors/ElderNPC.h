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

	// Ранний сюжетный крючок (ADR-049 п.2, решение Рината «ранний намёк одной фразой»):
	// дописывается к реплике ПЕРВОГО квеста, когда игрок его ещё не взял. Отдельным полем,
	// а не внутри описания квеста, — чтобы Ринат заменил фразу целиком без программиста.
	// Пусто — ничего не дописывается, диалог работает как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "5", MultiLine = "true"))
	// Текст ИЗДАТЕЛЯ, вставлен дословно (раздел 3, «посев загадки»): та же мысль, что и в
	// моей прежней временной фразе, но сформулирована автором и подхватывается дальше
	// ноутбуком на базе бандитов. Ремарка «машет рукой» опущена — это указание актёру,
	// а не слова старосты.
	FText DialogueEarlyHookText = NSLOCTEXT("Dialog", "ElderEarlyHook",
		"И вот что странно… За последние недели чужаки всё идут и идут к нам. Будто гонит их что-то. Или кто-то. Не моего ума дело. Ступай.");

	// --- Подарок первой встречи (см. TryGiveFirstMeetingGift) ---

	// Что кладём в рюкзак. Medkit — это наш «Бинт» (название предмета берётся из
	// AConsumableItem, здесь оно не дублируется).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Подарок", meta = (DisplayPriority = "1"))
	EConsumableType FirstGiftConsumableType = EConsumableType::Medkit;

	// Сколько штук. 0 — подарок выключен, диалог работает как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Подарок", meta = (DisplayPriority = "2", ClampMin = "0"))
	int32 FirstGiftCount = 1;

	// Всплывающая подсказка о полученном предмете: {Item} — название предмета,
	// {Count} — сколько штук. Пусто — подсказка не показывается.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Подарок", meta = (DisplayPriority = "3"))
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
