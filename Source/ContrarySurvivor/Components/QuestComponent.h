// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/FunctionFwd.h"
#include "QuestComponent.generated.h"

class UStatsComponent;
class UInventoryComponent;

/**
 * Тип квеста (GDD §7.7: «типы Kill/Collect/Deliver»). Фаза 5 (демка): реализованы все три
 * через ОБОБЩЁННУЮ модель из двух целей у квеста — KILL-цель (KillTargetTag/TargetCount/
 * Progress) и ITEM-цель (RequiredItemName/RequiredItemCount/ItemProgress). Type теперь
 * описательный (для текста/иконки); завершённость считается по обеим целям независимо от Type
 * (см. UQuestComponent::AreObjectivesMet). Это позволяет:
 *   - Kill    — только KILL-цель (волки/бандиты);
 *   - Collect — только ITEM-цель (собрать N предметов в рюкзак, напр. шкуры);
 *   - Deliver — ITEM-цель (+ опц. KILL-цель), напр. «убить N бандитов и принести ноутбук».
 */
UENUM(BlueprintType)
enum class EQuestType : uint8
{
	Kill    UMETA(DisplayName = "Kill"),     // убить N целей с тегом KillTargetTag
	Collect UMETA(DisplayName = "Collect"),  // задел: собрать N предметов
	Deliver UMETA(DisplayName = "Deliver")   // задел: доставить предмет
};

/**
 * Состояние квеста в журнале игрока.
 * NotStarted — предложен (offered), но не принят; Active — принят, идёт; Completed —
 * условие выполнено (Progress >= TargetCount), но награда не выдана; TurnedIn — сдан, награда выдана.
 */
UENUM(BlueprintType)
enum class EQuestState : uint8
{
	NotStarted UMETA(DisplayName = "NotStarted"),
	Active     UMETA(DisplayName = "Active"),
	Completed  UMETA(DisplayName = "Completed"),
	TurnedIn   UMETA(DisplayName = "TurnedIn")
};

/**
 * Вид задания (слова Рината 10.10.2026: «у нас есть основные квесты и побочные… основные и
 * побочные обозначаются оранжевыми и синими метками и текстом соответственно»). От вида зависят
 * цвет строки и метки (UContraryPdaSettings) и правило отслеживания: взятое основное задание
 * становится отслеживаемым само, побочное — только если ничего не отслеживается.
 * Основное стоит первым: задание из сохранения, записанного до появления поля, читается
 * основным — тогда в журнале были только задания старосты.
 */
UENUM(BlueprintType)
enum class EQuestKind : uint8
{
	Main UMETA(DisplayName = "Основное (сюжет)"),
	Side UMETA(DisplayName = "Побочное")
};

/**
 * Подготовительный шаг задания (11.10.2026, слова Рината: «Перед тем как игрок пойдет на
 * бандитов - надо как-то явно намекнуть, что бы он купил пистолет у торговца. Может вообще это
 * сделать частью квеста»). Шаг ВЕДЁТ, а не запирает: убийства и предметы засчитываются в любом
 * шаге, задание сдаётся как раньше. Какой предмет нужен, цена со скидкой и подписи — данные
 * задания (FQuest::Prep*), а не код.
 */
UENUM(BlueprintType)
enum class EQuestPrepStep : uint8
{
	NotChecked UMETA(DisplayName = "Ещё не проверялось"), // так читаются и задания из старых сохранений
	Buy        UMETA(DisplayName = "Купить предмет"),
	Equip      UMETA(DisplayName = "Поставить предмет в слот"),
	Done       UMETA(DisplayName = "Шаги пройдены или не нужны"),
};

/**
 * Проводник к предмету-награде (11.10.2026, слова Рината: «после диалога - мигает кнопка
 * инвентаря, а в нем мигает двустволка… когда игрок закрыл инвентарь - мигает кнопка смены
 * оружия»). Тот же приём, что у подготовительного шага: ведёт, но ничего не запирает.
 */
UENUM(BlueprintType)
enum class EQuestRewardGuideStep : uint8
{
	None   UMETA(DisplayName = "Не ведётся"), // так читаются и задания из старых сохранений
	Equip  UMETA(DisplayName = "Поставить награду в слот"),
	Switch UMETA(DisplayName = "Показать кнопку смены оружия"),
	Done   UMETA(DisplayName = "Пройдено"),
};

/** Где у героя предмет подготовительного шага. */
enum class EQuestPrepPossession : uint8
{
	None,   // нет нигде
	InBag,  // лежит в сумке
	InSlot, // стоит в слоте оружия
};

/**
 * Предметы, которые задание выдаёт герою (10.10.2026: ноутбук при взятии третьего задания
 * старосты, двустволка с патронами в награду у охранника базы учёных). Предмет задаётся
 * строкой таблицы предметов — как в остальной игре (ADR-075).
 */
USTRUCT(BlueprintType)
struct FQuestItemGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayName = "Предмет (строка таблицы предметов)"))
	FName ItemRow = NAME_None;

	// Для патронов — число патронов, для остального — число штук.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "1", DisplayName = "Сколько штук"))
	int32 Count = 1;
};

/**
 * Описание квеста (расширяемая структура, GDD §7.7). MVP: один Kill-квест от старосты
 * («убить 5 волков», награда 150 денег). Поля рассчитаны на разные типы (TargetCount/
 * Progress универсальны; KillTargetTag используется типом Kill).
 */
USTRUCT(BlueprintType)
struct FQuest
{
	GENERATED_BODY()

	// Стабильный идентификатор квеста (для поиска в журнале). DRAFT: "KillWolves".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName QuestId = NAME_None;

	// Короткое название (для диалога/журнала). ПЕРЕВОДИМЫЙ текст: ключом квеста служит
	// QuestId, название игрок только читает — в сравнениях не участвует (ADR-050).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Title;

	// Текст задания (что сказать игроку). Переводимый, в сравнениях не участвует.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQuestType Type = EQuestType::Kill;

	// --- KILL-цель ---
	// Тег цели, чьи убийства засчитываются (например, "Wolf"/"Bandit"). Пусто = любая цель.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName KillTargetTag = NAME_None;

	// Сколько целей нужно убить. 0 = у квеста НЕТ kill-цели (только сбор/доставка).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 TargetCount = 5;

	// Текущий прогресс убийств (инкрементит NotifyKill).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 Progress = 0;

	// Подпись KILL-цели для метки на карте («Перебить бандитов»): метка показывает её с
	// прогрессом, пока цель не выполнена (фидбек Рината 07-06). Пусто = фолбэк на KillTargetTag
	// (технический тег, для игрока не годится — заполнять при наличии kill-цели).
	// ПЕРЕВОДИМЫЙ текст: это надпись, а не ключ (ключ цели — KillTargetTag рядом).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText KillObjectiveLabel;

	// --- ITEM-цель (Collect/Deliver, Фаза 5 демка) ---
	// Имя предмета (AMasterInventoryItem::ItemName), который нужно собрать/принести
	// (например, "Шкура волка"/"Ноутбук"). Пусто = у квеста нет item-цели.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FString RequiredItemName;

	// Сколько таких предметов нужно сдать. 0 = item-цели нет. При сдаче они ИЗЫМАЮТСЯ из рюкзака.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 RequiredItemCount = 0;

	// Текущий прогресс по предметам в рюкзаке (пересчитывает SyncInventoryQuests; min с RequiredItemCount).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 ItemProgress = 0;

	// Подпись ITEM-цели для метки на карте («Забрать ноутбук»). Пусто = фолбэк на RequiredItemName.
	// ПЕРЕВОДИМЫЙ текст: это надпись, а не ключ (ключ предмета — RequiredItemName выше,
	// он остаётся обычной строкой и посимвольно сравнивается с ItemName предмета).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText ItemObjectiveLabel;

	// Метка цели квеста на карте (Этап D, реюз маркеров NPC): HUD ищет актор-цель по этому
	// тегу (QuestMarkerTag у AMasterEnemyBase либо стандартный Actor Tag) и рисует метку/
	// краевую стрелку, пока квест Active/Completed. NAME_None = квест без метки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName MapMarkerTag = NAME_None;

	// Награда деньгами при сдаче (TurnedIn). DRAFT (решение Рината): 150. Тюнингуется на старосте/квесте.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	float RewardMoney = 150.0f;

	// --- Реплики героя на кнопках диалога (Build 1, решение Рината 07-24: кнопка = осмысленный
	// ответ героя, а не служебное «Принять»). Переводимые тексты, черновики утверждает Ринат. ---

	// Ответ героя при принятии квеста («Хорошо, берусь.»). Пусто — панель диалога покажет свою
	// общую подпись (AcceptButtonLabel).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText AcceptReplyText;

	// Ответ героя при сдаче квеста («Готово. Вот, что ты просил.»). Можно вставить {Reward} —
	// подставится награда монетами. Пусто — панель покажет свой формат «Сдать (+N)».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText TurnInReplyText;

	// Ответ героя, закрывающий диалог («Мне пора.»): им подписываются кнопки [Отказаться]
	// (отказ = уйти) и [Закрыть]. Пусто — остаются статичные подписи из WBP_Dialog.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText CloseReplyText;

	// --- Расширение 10.10.2026 (база учёных). Старые сохранения читаются как есть: у заданий,
	// записанных раньше, эти поля пусты и поведение прежнее. ---

	// Кому сдаётся задание: метка собеседника (ATalkerNPC::NpcTag). Пусто — тому, кто выдал,
	// как раньше (староста). Непустое — сдать можно ТОЛЬКО собеседнику с этой меткой, а метка
	// цели на экране при готовом задании ведёт к нему (третье задание старосты сдаётся учёному).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName TurnInNpcTag = NAME_None;

	// Что герой получает В МОМЕНТ ВЗЯТИЯ задания (ноутбук, который надо отнести).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FQuestItemGrant> GiveOnAccept;

	// Награда предметами при сдаче — в дополнение к монетам (двустволка и патроны охранника).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FQuestItemGrant> RewardItems;

	// Куда идти с готовым заданием — хвост строки на экране («отнеси ноутбук учёным»). Пусто —
	// прежняя общая строка «вернись к старосте». Переводимый текст.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText TurnInHintText;

	// --- Расширение 10.10.2026 (карманный компьютер) ---

	// Основное или побочное. Проставляется там, где задание создаётся: у старосты — основное,
	// у остальных собеседников — побочное; в таблице заданий — своё поле строки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQuestKind Kind = EQuestKind::Main;

	// Кто выдал задание: метка собеседника (ATalkerNPC::NpcTag). Пусто — староста (у него метки
	// нет, и так же читаются задания из старых сохранений). Нужна окну КПК для строки «Выдал».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName GiverNpcTag = NAME_None;

	// --- Подготовительный шаг (11.10.2026): купить предмет у торговца и поставить его в слот ---

	// Строка таблицы предметов, которую герою советуют купить перед заданием (пистолет).
	// Пусто — у задания подготовительных шагов нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Подготовка")
	FName PrepItemRow = NAME_None;

	// Цена этого предмета со скидкой, пока идёт шаг покупки (одна покупка). 0 — скидки нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Подготовка", meta = (ClampMin = "0.0"))
	float PrepDealPrice = 0.0f;

	// Цель на экране на шаге покупки («Купи пистолет у торговца») и на шаге слота.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Подготовка")
	FText PrepBuyObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Подготовка")
	FText PrepEquipObjectiveText;

	// Подпись на плитке товара у торговца: скидка и — когда монет не хватает — продажа в долг.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Подготовка")
	FText PrepDealText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Подготовка")
	FText PrepDebtText;

	// Всплывающая подсказка сразу после покупки предмета: что делать дальше (закрыть торговлю,
	// открыть сумку, поставить в слот). Пусто — подсказки нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Подготовка")
	FText PrepBoughtHintText;

	// --- Предупреждение о малом здоровье при выходе из деревни (11.10.2026, слова Рината:
	// «проверку хп игрока в момент его выхода из деревни в сторону бандитов… если у него
	// маленькое хп (например ниже 45) - ХП будет мигать и будет появляться сообщение») ---

	// Порог здоровья: ниже него герою, уходящему из деревни с этим заданием, показывается
	// предупреждение. 0 — задание о здоровье не предупреждает. Действует, пока задание взято и
	// его цель убийств не выполнена.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Здоровье", meta = (ClampMin = "0.0"))
	float LowHealthWarnThreshold = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Здоровье", meta = (MultiLine = "true"))
	FText LowHealthWarnText;

	// --- Проводник к предмету-награде (11.10.2026) ---

	// Строка таблицы предметов из награды, которую игроку помогают поставить в слот оружия
	// (двустволка охранника): после разговора мигает кнопка сумки, в сумке — плитка предмета,
	// затем кнопка смены оружия. Пусто — награду никто не ведёт.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName RewardEquipItemRow = NAME_None;

	// Ход проводника (пишется в сохранение вместе с журналом).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestRewardGuideStep RewardGuideStep = EQuestRewardGuideStep::None;

	// Ход подготовки (пишется в сохранение вместе с журналом).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Подготовка")
	EQuestPrepStep PrepStep = EQuestPrepStep::NotChecked;

	// Скидкой уже воспользовались (она на одну покупку).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Подготовка")
	bool bPrepDealUsed = false;

	// Долг торговцу за предмет, взятый без денег: спишется с денежной награды при сдаче.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|Подготовка")
	float PrepDebt = 0.0f;

	// Сколько монет герой получит при сдаче: награда минус долг, не меньше нуля.
	float GetPayoutMoney() const { return FMath::Max(0.0f, RewardMoney - FMath::Max(0.0f, PrepDebt)); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestState State = EQuestState::NotStarted;
};

/**
 * Идентификаторы заданий, на которые ссылается не только выдающий (10.10.2026: шлагбаум базы
 * учёных и реплика охранника смотрят на задание старосты, раскладчик рации — на задание
 * охранника). Одно место правды: строку руками не повторять.
 */
namespace ContraryQuestIds
{
	// Третье задание старосты «Отнести ноутбук учёным».
	inline const FName DeliverLaptopToScientists(TEXT("DeliverLaptopToScientists"));

	// Задание охранника базы учёных «Рация охранника».
	inline const FName FetchGuardRadio(TEXT("FetchGuardRadio"));

	// Второе задание старосты «Зачистить базу бандитов»: пока оно не сдано, староста за героя
	// не поручился и охранник базы учёных с ним дел не имеет.
	inline const FName ClearBanditBase(TEXT("ClearBanditBase"));
}

// Делегат изменения квеста (для HUD/звука/журнала). Передаёт изменённый квест.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestChanged, const FQuest&, Quest);

/**
 * Журнал квестов игрока (Фаза 5, GDD §7.7 «UQuestComponent / структура квеста»).
 *
 * Каркас расширяемый: хранит МАССИВ FQuest, поддерживает типы Kill/Collect/Deliver
 * (в MVP инкрементируется только Kill через NotifyKill). Живёт на APlayerCharacter
 * (C++-сабобъект, editor-независимо) — игрок владеет своим журналом.
 *
 * Поток: староста предлагает квест (OfferQuest, состояние NotStarted) -> игрок принимает
 * (AcceptQuest, Active) -> убийства целей с нужным тегом инкрементят Progress (NotifyKill);
 * при Progress >= TargetCount -> Completed -> сдача у старосты (TurnInQuest) выдаёт деньги -> TurnedIn.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestComponent();

	// Делегат изменения состояния любого квеста (offer/accept/progress/complete/turn-in).
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestChanged OnQuestChanged;

	// Журнал заменён целиком (RestoreQuests: «Продолжить», кнопки сюжета меню отладки). Нужен
	// тем, кто выводит своё состояние из всего журнала (шлагбаум базы учёных): OnQuestChanged
	// приходит по одному заданию и молчит, когда журнал стал ПУСТЫМ.
	FSimpleMulticastDelegate OnJournalRestored;

	// Предложить квест: добавляет его в журнал в состоянии NotStarted, если такого ещё нет.
	// Идемпотентно (повторный offer того же QuestId ничего не меняет). Логирует OFFERED один раз.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void OfferQuest(const FQuest& Quest);

	// Принять предложенный квест (NotStarted -> Active). true при успехе.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool AcceptQuest(FName QuestId);

	// Уведомить журнал об убийстве цели с тегом TargetTag: для всех Active квестов с kill-целью
	// (TargetCount>0) и совпадающим KillTargetTag инкрементит Progress; пересчитывает завершённость.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void NotifyKill(FName TargetTag);

	// Пересчитать ITEM-прогресс Active/Completed квестов по содержимому рюкзака игрока
	// (считает предметы с ItemName == RequiredItemName). Вызывается реактивно (контроллер в Tick).
	// Меняет ItemProgress/State и шлёт OnQuestChanged ТОЛЬКО при реальном изменении (без спама).
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void SyncInventoryQuests(UInventoryComponent* Inventory);

	// Сдать выполненный квест (Completed -> TurnedIn): если у квеста есть item-цель — проверяет
	// наличие RequiredItemCount предметов «RequiredItemName» в рюкзаке владельца и ИЗЫМАЕТ их
	// (RemoveItem + Destroy), затем начисляет RewardMoney в UStatsComponent владельца.
	// true при успехе (квест найден, был Completed и предметы изъяты).
	// ReceiverNpcTag — метка собеседника, который принимает задание (10.10.2026). Обязана
	// совпасть с FQuest::TurnInNpcTag: у старых заданий старосты обе пусты, а задание
	// «отнести учёным» староста принять не может, какой бы путь окна его ни вызвал.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TurnInQuest(FName QuestId, FName ReceiverNpcTag = NAME_None);

	// Первое готовое к сдаче задание (Completed), которое принимает собеседник с этой меткой.
	// null — такого нет. Пустая метка не ищется (это задания «сдать тому, кто выдал»).
	const FQuest* FindCompletedQuestForReceiver(FName ReceiverNpcTag) const;

	// Есть ли уже СДАННОЕ задание, которое принимал собеседник с этой меткой.
	bool HasTurnedInQuestForReceiver(FName ReceiverNpcTag) const;

	// Б3 («Продолжить»): заменяет журнал целиком сохранённым снимком (полные FQuest, не только
	// id/состояние — см. комментарий FSavedInventoryEntry/Quests в ContrarySaveGame.h) и
	// оповещает подписчиков (HUD-трекер/маркеры) по каждому квесту, чтобы UI не ждал следующего
	// offer/accept события старосты.
	// SavedTrackedQuestId — какое задание отслеживалось в момент сохранения. Пусто (сохранение
	// записано до 10.10.2026) — прежнее поведение «первое активное задание журнала».
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void RestoreQuests(const TArray<FQuest>& SavedQuests, FName SavedTrackedQuestId = NAME_None);

	// --- Геттеры для UI/диалога ---

	// Указатель на квест по id (const). null, если нет.
	const FQuest* FindQuest(FName QuestId) const;

	// --- Отслеживаемое задание (10.10.2026): его показывают строка на экране, метка-указатель
	// в мире и крупная отметка на карте КПК ---

	// Задание, выбранное для отслеживания, если оно ещё идёт (Active или Completed); иначе —
	// первое идущее задание журнала (так было всегда и так читаются старые сохранения).
	// null — идущих заданий нет.
	const FQuest* GetTrackedQuest() const;

	// Сделать задание отслеживаемым (кнопка «Отслеживать» и отметка на карте КПК). Отказ —
	// задания нет в журнале или оно не идёт (не взято либо уже сдано).
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool SetTrackedQuest(FName QuestId);

	// Явный выбор игрока или правил (пусто — выбора не было, действует «первое идущее»).
	// Это значение пишется в сохранение.
	FName GetTrackedQuestId() const { return TrackedQuestId; }

	// Идёт ли задание: взято и ещё не сдано.
	static bool IsInActivePhase(const FQuest& Quest)
	{
		return Quest.State == EQuestState::Active || Quest.State == EQuestState::Completed;
	}

	const TArray<FQuest>& GetQuests() const { return Quests; }

	// Сколько квестов сдано (State == TurnedIn) — для экрана смерти (#26 «сколько квестов выполнено»).
	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetTurnedInQuestCount() const;

	// Сдано ли задание с таким идентификатором (State == TurnedIn). Задания нет в журнале или
	// идентификатор пуст — false. Одной функцией, потому что по этому признаку живут правила
	// «первого боя»: замедление волка и выкуп шкур торговцем (решения Рината 05–06.10.2026).
	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsQuestTurnedIn(FName QuestId) const;

	// --- Подготовительный шаг задания (11.10.2026) ---

	// Чистое правило шага: что должно стоять у задания при таком положении дел с предметом.
	static EQuestPrepStep ComputePrepStep(const FQuest& Quest, EQuestPrepPossession Possession);

	// Показывается ли сейчас игроку подготовительный шаг (вместо обычных целей задания).
	static bool IsPrepStepShown(const FQuest& Quest)
	{
		return Quest.State == EQuestState::Active && !Quest.PrepItemRow.IsNone()
			&& (Quest.PrepStep == EQuestPrepStep::Buy || Quest.PrepStep == EQuestPrepStep::Equip);
	}

	// Цель подготовительного шага для строки на экране (пусто — шага сейчас нет).
	static FText GetPrepObjectiveText(const FQuest& Quest);

	// Есть ли идущее задание, у которого шаг ещё не пройден / ещё ни разу не проверялся.
	bool HasPendingPrep() const;
	bool HasUncheckedPrep() const;

	// Сверить шаги всех заданий с тем, где сейчас предмет (GetPossession — по строке предмета).
	void UpdatePrepSteps(TFunctionRef<EQuestPrepPossession(FName)> GetPossession);

	// Задание, по которому на эту строку предмета сейчас действует скидка (иначе null).
	const FQuest* FindPrepDeal(FName ItemRow) const;

	// Предмет куплен по скидке задания; Debt — сколько монет не хватило (взято в долг).
	bool NotePrepPurchase(FName QuestId, float Debt);

	// Донести описание подготовительного шага заданию, которое уже лежит в журнале без него
	// (сохранение записано до 11.10.2026). true — задание дополнено.
	bool MergePrepDefinition(const FQuest& Definition);

	// Идёт ли у какого-нибудь задания такой шаг.
	bool IsAnyPrepStepShown(EQuestPrepStep Step) const;

	// Донести до задания, уже лежащего в журнале, тексты и подсказки его описания (журнал
	// хранит копию задания, снятую при выдаче, — без этого правка текста в таблице заданий не
	// дошла бы до сохранения, записанного раньше). Ход задания не трогает.
	static void RefreshDefinitionTexts(const FQuest& Definition, FQuest& InJournal);

	// --- Предупреждение о малом здоровье при выходе из деревни (11.10.2026) ---

	// Задание, которое при таком здоровье предупреждает героя (иначе null): взято, порог задан,
	// цель убийств ещё не выполнена, здоровье ниже порога.
	const FQuest* FindLowHealthWarning(float Health) const;

	// То же правило для одного задания — чистой функцией для автопроверки.
	static bool ShouldWarnLowHealth(const FQuest& Quest, float Health);

	// --- Проводник к предмету-награде (11.10.2026) ---

	// Чистое правило: каким должен стать шаг проводника при таком положении дел.
	// bCanSwitchFirearms — заняты оба слота огнестрела, то есть кнопке смены оружия есть что менять.
	static EQuestRewardGuideStep ComputeRewardGuideStep(const FQuest& Quest,
		EQuestPrepPossession Possession, bool bCanSwitchFirearms);

	// Есть ли задание, у которого проводник ещё идёт / стоит на таком шаге.
	bool HasPendingRewardGuide() const;
	bool IsAnyRewardGuideStep(EQuestRewardGuideStep Step) const;

	// Сверить проводники всех заданий с тем, где сейчас предмет награды.
	void UpdateRewardGuides(TFunctionRef<EQuestPrepPossession(FName)> GetPossession, bool bCanSwitchFirearms);

	// Игрок нажал кнопку смены оружия: шаг «показать кнопку» пройден. true — что-то изменилось.
	bool NoteWeaponSwitched();

	// Выполнены ли ОБЕ цели квеста (kill + item). Цель с нулевым требованием считается выполненной.
	static bool AreObjectivesMet(const FQuest& Quest);

	// Сторож датчика «квест предложен» (сводное ТЗ издателя 13.08, задача 3). Возвращает true
	// ТОЛЬКО в первый раз для этого QuestId — по нему решается, слать ли событие.
	//
	// ⛔ Зачем отдельный сторож, если OfferQuest и так идемпотентен: сам OfferQuest зовётся
	// КАЖДЫЙ КАДР отрисовки диалога со старостой (ContrarySurvivorHUD.cpp), а журнал можно
	// очистить загрузкой сохранения или «Новой игрой» — и тогда тот же квест «предложился» бы
	// заново. Повторная отправка портит проценты воронки сильнее, чем её отсутствие.
	// ПУБЛИЧНЫЙ намеренно: ровно эту гарантию проверяет автотест (живой отправки в headless нет).
	bool MarkQuestOfferedForAnalytics(FName QuestId);

	// Сколько разных квестов уже отмечено отправленными — для автотеста.
	int32 GetOfferedAnalyticsCount() const { return AnalyticsOfferedQuests.Num(); }

private:
	// Журнал квестов игрока (расширяемо — массив).
	UPROPERTY()
	TArray<FQuest> Quests;

	// Квесты, по которым событие «предложен» уже ушло (см. MarkQuestOfferedForAnalytics).
	// Не сохраняется: память живёт столько же, сколько журнал в этом запуске.
	TSet<FName> AnalyticsOfferedQuests;

	// Идентификатор отслеживаемого задания (см. GetTrackedQuest).
	FName TrackedQuestId = NAME_None;

	// Отслеживаемое задание сдано или пропало: следующим становится первое идущее основное,
	// а если таких нет — первое идущее. Идущих нет вовсе — выбор сбрасывается.
	void PickNextTrackedQuest();

	// Изменяемый поиск (внутренний).
	FQuest* FindQuestMutable(FName QuestId);

	// Пересчитать состояние одной активной фазы квеста (Active<->Completed) по целям.
	// Возвращает true, если состояние изменилось (тогда вызывающий шлёт OnQuestChanged).
	bool RecomputeState(FQuest& Quest);
};
