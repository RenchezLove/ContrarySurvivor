// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TalkerNPC.h"                                    // собеседник и собеседник с заданием
#include "ContrarySurvivor/Characters/MasterTrader.h"     // торговец (окно торговли то же, что в деревне)
#include "ContrarySurvivor/Ads/AdSeriesLogic.h"           // «не больше трёх подряд, потом перерыв»
#include "ScientistNPCs.generated.h"

/**
 * Персонажи базы учёных (поручение Рината 10.10.2026). Все тексты и числа — ЧЕРНОВИК: лежат в
 * редактируемых полях, Ринат правит их на чертеже без кода.
 */

/** Метки собеседников базы — одно место правды для кода и для заданий, которые на них ссылаются. */
namespace ScientistBaseTags
{
	inline const FName Guard(TEXT("ScientistGuard"));
	inline const FName QuestScientist(TEXT("ScientistQuest"));
	inline const FName HelmetScientist(TEXT("ScientistHelmet"));

	// Место, где лежит рация охранника (метка актора-раскладчика AQuestItemSpawner).
	inline const FName GuardRadioSpot(TEXT("GuardRadioSpot"));
}

/**
 * Охранник базы учёных. Задание «принести рацию, которую он обронил, удирая от волков»;
 * награда — двустволка и патроны. Доступно сразу, от заданий старосты не зависит.
 *
 * Героя с ноутбуком от старосты пропускает на базу (шлагбаум AQuestBarrier открывается по тому
 * же заданию) и говорит об этом первой фразой; своё задание про рацию при этом остаётся.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AScientistGuardNPC : public AQuestGiverNPC
{
	GENERATED_BODY()

public:
	AScientistGuardNPC();

	virtual void BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const override;

	// Начинает ли охранник с отказа (11.10.2026, слова Рината: «охранник говорит что староста за
	// ГГ не поручился и он не может его пропустить. После чего… хотя если ты сможешь мне помочь,
	// то пропущу»): шлагбаум для героя закрыт и задание про рацию ещё не взято. Чистое правило
	// для автопроверки; условие шлагбаума — то же, что у самого шлагбаума (AQuestBarrier).
	static bool ShouldStartWithRefusal(const TArray<FQuest>& Journal, FName LaptopQuestId, FName OwnQuestId);
	const FText& GetNotVouchedText() const { return NotVouchedText; }

	// Несёт ли герой ноутбук учёным: задание взято и ещё не сдано. Чистое правило для автотеста.
	static bool IsCarryingLaptop(const TArray<FQuest>& Journal, FName LaptopQuestId);

protected:
	// Вступление, пока героя на базу не пускают: отказ и «хотя, если поможешь…». За ним идёт
	// просьба про рацию («Реплика: просьба про рацию после пропуска») и кнопка взять задание.
	// Пусто — охранник сразу говорит обычное описание задания. Черновик.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Поручительство старосты", meta = (DisplayPriority = "1", MultiLine = "true",
		DisplayName = "Реплика: староста не поручился, но помоги — пропущу"))
	FText NotVouchedText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Пропуск с ноутбуком", meta = (DisplayPriority = "1",
		DisplayName = "Задание «отнести ноутбук» (идентификатор)"))
	FName LaptopQuestId = ContraryQuestIds::DeliverLaptopToScientists;

	// Первая фраза, пока герой несёт ноутбук. Пусто — охранник о ноутбуке молчит. Черновик.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Пропуск с ноутбуком", meta = (DisplayPriority = "2", MultiLine = "true",
		DisplayName = "Реплика: пропускаю с ноутбуком"))
	FText LaptopPassText;

	// Чем продолжить, если своё задание ещё не взято: просьба про рацию без слов «Стой, дальше
	// база» из обычного описания. Пусто — обычное описание задания. Черновик.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Пропуск с ноутбуком", meta = (DisplayPriority = "3", MultiLine = "true",
		DisplayName = "Реплика: просьба про рацию после пропуска"))
	FText LaptopPassOfferText;
};

/**
 * Квестовый учёный. Принимает ноутбук (третье задание старосты сдаётся ему), после чего
 * отвечает заглушкой «нужно время на расшифровку». Своё задание появится позже — точка
 * расширения: включить «Выдаёт своё задание» и заполнить его.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AScientistQuestNPC : public AQuestGiverNPC
{
	GENERATED_BODY()

public:
	AScientistQuestNPC();
};

/** Что сейчас происходит в разговоре с учёным со шлемом. */
enum class EHelmetTalkStage : uint8
{
	Offer,        // предлагает испытать шлем (или говорит, что прибор остывает)
	Running,      // идёт показ
	Paid,         // досмотрено, заплатил
	NotFinished,  // показ закрыт досрочно — платы нет
	NotReady      // показ не загрузился — «прибор не готов»
};

/**
 * Учёный со шлемом виртуальной реальности: добровольный просмотр рекламы за игровую валюту
 * (GDD §12 — реклама как «эксперименты учёных»). Показ идёт существующим слоем рекламы
 * (IAdService::ShowRewarded) со своим местом показа AdPlacements::ScientistHelmet.
 * Не больше MaxViewsInRow просмотров подряд, потом перерыв BreakMinutes — «прибор остывает».
 * Счёт серии живёт в памяти персонажа до конца запуска игры (в сохранение не пишется).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AScientistHelmetNPC : public ATalkerNPC
{
	GENERATED_BODY()

public:
	AScientistHelmetNPC();

	virtual void BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const override;
	virtual bool HandleTalkPrimary(APlayerCharacter* Player) override;
	virtual void OnTalkOpened(APlayerCharacter* Player) override;

	// Можно ли начать просмотр в этот момент (правило серии и перерыва).
	bool CanStartExperiment(const FDateTime& NowUtc) const;

	// Итог показа. bWatched — досмотрен до конца: засчитывается в серию и оплачивается.
	// Публичная, потому что ровно это правило проверяет автотест (живого показа в нём нет).
	void CompleteExperiment(APlayerCharacter* Player, bool bWatched, const FDateTime& NowUtc);

	EHelmetTalkStage GetStage() const { return Stage; }
	float GetRewardMoney() const { return RewardMoney; }

protected:
	// --- Числа (черновик) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "1", ClampMin = "0.0",
		DisplayName = "Плата за просмотр (монеты)"))
	float RewardMoney = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "2", ClampMin = "1",
		DisplayName = "Просмотров подряд до перерыва"))
	int32 MaxViewsInRow = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "3", ClampMin = "0.0",
		DisplayName = "Перерыв (минуты)"))
	float BreakMinutes = 4.0f;

	// --- Тексты (черновик) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "4", MultiLine = "true",
		DisplayName = "Реплика: предложение ({Reward} — плата)"))
	FText OfferText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "5",
		DisplayName = "Ответ героя: согласиться"))
	FText AcceptReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "6",
		DisplayName = "Ответ героя: отказаться"))
	FText DeclineReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "7", MultiLine = "true",
		DisplayName = "Реплика: идёт испытание"))
	FText RunningText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "8", MultiLine = "true",
		DisplayName = "Реплика: заплатил ({Reward} — плата)"))
	FText PaidText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "9",
		DisplayName = "Ответ героя: ещё раз"))
	FText AgainReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "10", MultiLine = "true",
		DisplayName = "Реплика: испытание прервано, платы нет"))
	FText NotFinishedText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "11", MultiLine = "true",
		DisplayName = "Реплика: прибор не готов (показ не загрузился)"))
	FText NotReadyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Шлем", meta = (DisplayPriority = "12", MultiLine = "true",
		DisplayName = "Реплика: прибор остывает ({Minutes} — сколько минут ждать)"))
	FText CoolingText;

private:
	void StartExperiment(APlayerCharacter* Player);
	void HandleAdSuccess();
	void HandleAdFail();

	EHelmetTalkStage Stage = EHelmetTalkStage::Offer;
	FAdSeriesState Series;
	float LastPaid = 0.0f;

	// Кому платить, когда показ закончится (показ ставит игру на паузу и возвращается позже).
	TWeakObjectPtr<APlayerCharacter> ExperimentPlayer;
};

/**
 * Учёный-торговец: то же окно торговли, что у деревенского торговца, со своим составом
 * прайс-листа (двустволка, патроны к ней, расходники). Состав и цены правятся в поле
 * «Состав прайс-листа» на чертеже, как у деревенского.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AScientistTrader : public AMasterTrader
{
	GENERATED_BODY()

public:
	AScientistTrader();
};
