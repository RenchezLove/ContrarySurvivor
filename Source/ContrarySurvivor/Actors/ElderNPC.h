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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Intro",
		meta = (MultiLine = "true", DisplayName = "Реплика старосты", DisplayPriority = "1"))
	FText NPCText;

	// Единственная кнопка-ответ игрока под этой репликой (квадратные скобки рисует WBP_Dialog).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Intro",
		meta = (DisplayName = "Ответ игрока (кнопка)", DisplayPriority = "2"))
	FText ButtonLabel;

	// Что происходит при нажатии кнопки этой реплики (выдать аптечку / начать квест / просто дальше).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Intro",
		meta = (DisplayName = "Действие кнопки", DisplayPriority = "3"))
	EElderIntroAction Action = EElderIntroAction::None;
};

/**
 * Ожидание после сценки про ноутбук, как его видит список заданий на экране (10.10.2026).
 */
enum class EElderNotebookWaitPhase : uint8
{
	None,    // сценки ещё не было либо следующее задание уже взято — строки нет
	Waiting, // староста разбирается с ноутбуком, таймер идёт
	Ready    // время вышло — пора поговорить со старостой
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

	// Второй (терминальный) квест («Зачистить базу бандитов») — по нему панель диалога решает,
	// пора ли играть сценку-намёк после сдачи ноутбука.
	const FQuest& GetSecondQuest() const { return SecondQuest; }

	// Третье задание («Отнести ноутбук учёным», Ринат 10.10.2026). Выдаётся в разговоре после
	// сценки «дай время покопаться в ноутбуке», когда вышло время ожидания; сдаётся не старосте, а
	// квестовому учёному на базе учёных (FQuest::TurnInNpcTag).
	const FQuest& GetThirdQuest() const { return ThirdQuest; }

	// Открыть третье задание, если пора: второе сдано, сценка про ноутбук в этом профиле уже
	// показана и ожидание после неё кончилось (признаки героя, читаются из сохранения). Зовёт
	// контроллер при ОТКРЫТИИ разговора. Задание кладётся в журнал героя, и с этой минуты
	// GetQuestForPlayer отдаёт его. Старое сохранение (второе задание давно сдано, таймера в
	// записи нет) получает задание сразу. true — задание открыто именно сейчас.
	bool TryUnlockThirdQuest(APlayerCharacter* Player) const;

	// То же правило чистой функцией — для автотеста без мира и сохранения.
	// bNotebookWaitOver — ожидание после сценки кончилось (10.10.2026): пока староста
	// «разбирается с ноутбуком», третье задание не открывается.
	static bool ShouldUnlockThirdQuest(bool bSecondQuestTurnedIn, bool bNotebookHintShown,
		bool bHasNotebookHintLines, bool bThirdQuestConfigured, bool bNotebookWaitOver);

	// --- Ожидание после сценки про ноутбук (Ринат 10.10.2026) ---

	// Сколько секунд игрового времени длится ожидание (поле чертежа).
	float GetNotebookWaitSeconds() const { return NotebookWaitSeconds; }

	// Сколько секунд осталось ждать. WaitEndPlayTime — отметка из сохранения (значение
	// накопителя игрового времени, при котором ожидание кончается; меньше нуля — таймера нет,
	// ждать нечего), TotalPlayTime — накопитель сейчас.
	static float GetNotebookWaitRemaining(float WaitEndPlayTime, float TotalPlayTime);

	// Остаток для строки на экране: целые минуты с округлением вверх, не меньше одной.
	static int32 GetNotebookWaitMinutesLeft(float RemainingSeconds);

	// Что показывать в списке заданий на экране: ничего, «староста разбирается с ноутбуком»
	// или «разобрался, поговорите с ним». Вторая строка держится, пока следующее задание не
	// взято. Сохранение без таймера (сценка показана до этой правки) сразу даёт вторую строку.
	static EElderNotebookWaitPhase GetNotebookWaitPhase(bool bNotebookHintShown,
		bool bNextQuestTaken, float RemainingSeconds);

	// Состояние задания, как его видит разговор СО СТАРОСТОЙ. Готовое задание, которое
	// сдаётся другому собеседнику, для старосты остаётся «взято, не закончено»: кнопки сдачи
	// у него нет. Одно место правды на оба пути окна разговора.
	static EQuestState GetElderDialogState(const FQuest& Quest);

	// Реплика старосты, пока герой несёт ноутбук учёным, и после того, как отнёс.
	const FText& GetThirdQuestCarryText() const { return ThirdQuestCarryText; }
	const FText& GetThirdQuestDoneText() const { return ThirdQuestDoneText; }

	// Квест, актуальный для игрока СЕЙЧАС (выдача по порядку, Фаза 5):
	//  - пока кв.1 не сдан (TurnedIn) — возвращает кв.1;
	//  - после сдачи кв.1 — возвращает кв.2 («Зачистить базу бандитов»); кв.2 — последний
	//    (Build 1: формального кв.3 больше нет, вместо него сценка-намёк NotebookHintLines).
	//  - 10.10.2026: если третье задание уже открыто (лежит в журнале героя) — отдаётся оно.
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

	// Реплика в состоянии «задание выполнено, жду сдачи» ДЛЯ КОНКРЕТНОГО квеста (ADR-065).
	// У первого квеста она своя: вместе с наградой староста между делом советует заглянуть
	// к торговцу. Общий DialogueCompletedText для этого не годится — он звучал бы и при
	// сдаче второго квеста, где совет про первое оружие уже нелеп.
	// Пустая реплика первого квеста означает «веди себя как раньше» — вернётся общая.
	const FText& GetCompletedTextForQuest(const FName& QuestId) const;

	// Тот же выбор чистой функцией — чтобы правило гонялось автотестом без мира и диалога.
	static bool ShouldUseFirstQuestCompletedText(const FName& QuestId, const FName& FirstQuestId,
		bool bFirstQuestTextIsSet);
	const FText& GetDialogueTurnedInText() const { return DialogueTurnedInText; }
	const FText& GetDialogueEarlyHookText() const { return DialogueEarlyHookText; }

	// Скриптовое интро первой встречи (Build 1): реплики старосты по очереди, по одной кнопке-
	// ответу на реплику. Проигрывается ТОЛЬКО пока первый квест ещё не принят (первая встреча).
	const TArray<FElderIntroLine>& GetIntroLines() const { return IntroLines; }

	// Сценка-намёк после сдачи кв.2 (Build 1, замена формального кв.3): реплики по очереди,
	// как IntroLines. Показывается один раз (признак bElderNotebookHintShown в сейве).
	const TArray<FElderIntroLine>& GetNotebookHintLines() const { return NotebookHintLines; }

	// Короткая реплика при повторных разговорах после показанного намёка.
	const FText& GetNotebookHintRepeatText() const { return NotebookHintRepeatText; }

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

	// ADR-065 (замысел Рината: «первое оружие — решение игрока, а не подарок»). Реплика
	// показывается ТОЛЬКО при сдаче ПЕРВОГО квеста, вместе с наградой: староста не даёт
	// инструкцию и ничего не подсвечивает, а между делом советует зайти к торговцу — чтобы
	// игрок сам додумался купить оружие и припасы. При сдаче второго квеста показывается
	// обычная реплика выше. Очистить это поле — вернуть прежнее поведение для всех квестов.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "4", MultiLine = "true"))
	FText FirstQuestCompletedText = NSLOCTEXT("Dialog", "ElderFirstQuestCompleted",
		"Держи, заработал. И вот что: загляни к торговцу, он под навесом. С ножом на волка ходить — дело молодое, а с железом в руке и подальше зайти можно. Да про припасы не забудь: голодному дорога короткая.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "5", MultiLine = "true"))
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
	// 07.10.2026 (текст Рината дословно): надпись говорит, где искать подарок, и заодно
	// объясняет хромоту — развёрнутая подсказка о ранении на старте больше не показывается.
	// Текст назван под аптечку: при смене FirstGiftConsumableType поправить и его.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|Gift", meta = (DisplayPriority = "3"))
	FText FirstGiftHintFormat = NSLOCTEXT("Dialog", "ElderFirstGiftHint",
		"Получена аптечка — она в сумке. Подлечись, и хромота пройдёт");

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

	// ADR-075 (группа 6): квесты старосты берутся из таблицы квестов DT_Quests по именам
	// строк ниже. Строка нашлась и собралась (включая разрешение предмета цели через таблицу
	// предметов) — она ГЛАВНЕЕ конструкторных значений; нет — прежние значения из кода
	// (мягкая деградация, игра работает без таблиц). Сверка лида 22.08 (тройной дамп
	// elder_quests/elder_instance): правок Рината на BP_Elder нет, в таблицу переезжают
	// конструкторные значения как есть.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "1",
		DisplayName = "Первый квест (строка таблицы квестов)"))
	FName FirstQuestRow = FName(TEXT("KillWolves"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "2",
		DisplayName = "Второй квест (строка таблицы квестов)"))
	FName SecondQuestRow = FName(TEXT("ClearBanditBase"));

	// Квест 1 («Шкуры волков»: 3 шкуры, награда 200 — ADR-065). ADR-075: VisibleAnywhere
	// (было EditAnywhere) — источник теперь таблица квестов, вторая редактируемая копия
	// того же квеста была бы запрещённым дублем (п.5 ТЗ). Поле видно для отладки.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest", meta = (DisplayPriority = "3"))
	FQuest OfferedQuest;

	// Квест 2 («Зачистить базу бандитов»: 3 бандита + Ноутбук, награда 250). Выдаётся ПОСЛЕ
	// сдачи кв.1 (GetQuestForPlayer). ADR-075: VisibleAnywhere — источник = таблица (см. выше).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FQuest SecondQuest;

	// --- Третье задание: отнести ноутбук учёным (Ринат 10.10.2026; числа и тексты — черновик) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "4",
		DisplayName = "Третий квест (строка таблицы квестов)"))
	FName ThirdQuestRow = FName(TEXT("DeliverLaptopToScientists"));

	// Действует, пока строки с таким именем нет в таблице квестов; появится строка — главнее она.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "5",
		DisplayName = "Третий квест (если строки таблицы нет)"))
	FQuest ThirdQuest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "8", MultiLine = "true",
		DisplayName = "Реплика: герой ещё несёт ноутбук учёным"))
	FText ThirdQuestCarryText = NSLOCTEXT("Dialog", "ElderThirdQuestCarry",
		"Ноутбук у тебя. Неси его учёным на базу — дорогу покажет метка.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (DisplayPriority = "9", MultiLine = "true",
		DisplayName = "Реплика: ноутбук отнесён учёным"))
	FText ThirdQuestDoneText = NSLOCTEXT("Dialog", "ElderThirdQuestDone",
		"Отнёс ноутбук? Добро. Теперь дело за учёными: как разберутся — узнаем, кто за тобой охотится.");

	// Сценка после сдачи кв.2 (Build 1.2, формулировка Рината 07-31): реплика 1 — сюжетный
	// крючок ADR-044 («на ноутбуке — данные о тех, кто за тобой охотится», СОХРАНЁН); дальше
	// староста сообщает СПОСОБ ЗАРАБОТКА, а не новый квест: пока он копается в ноутбуке, игрок
	// может охотиться на волков (их снова видели к югу) и сдавать шкуры торговцу, у которого
	// новый завоз — уже есть новая броня, скоро будет оружие. Реплики по очереди, по образцу
	// IntroLines; показывается один раз (признак bElderNotebookHintShown в сейве). Поле Action
	// у реплик здесь НЕ действует (эффектов нет).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|NotebookHint",
		meta = (DisplayPriority = "1", TitleProperty = "ButtonLabel", DisplayName = "Сценка после сдачи ноутбука"))
	TArray<FElderIntroLine> NotebookHintLines;

	// Короткая реплика при повторных разговорах, когда намёк уже показан. Пусто — фолбэк на
	// DialogueTurnedInText («Спасибо тебе ещё раз…»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|NotebookHint",
		meta = (DisplayPriority = "2", MultiLine = "true", DisplayName = "Реплика при повторных разговорах"))
	FText NotebookHintRepeatText = NSLOCTEXT("Dialog", "ElderNotebookHintRepeat",
		"Копаюсь в ноутбуке, дай мне время. А волки к югу никуда не делись — и торговец за шкуры платит исправно.");

	// Сколько староста «разбирается с ноутбуком» после сценки (Ринат 10.10.2026), секунды
	// ИГРОВОГО времени: на паузе не идёт, выход из игры и смерть не сбрасывают. Пока время не
	// вышло, третье задание не открывается, а при разговоре звучит реплика выше. 0 — без ожидания.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog|NotebookHint",
		meta = (DisplayPriority = "3", ClampMin = "0.0", DisplayName = "Сколько староста разбирается с ноутбуком, сек"))
	float NotebookWaitSeconds = 300.0f;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
