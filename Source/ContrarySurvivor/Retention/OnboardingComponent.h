// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ContrarySurvivor/UI/OnboardingHintWidget.h" // FOnboardingHintStyle (стиль тоста)
#include "OnboardingComponent.generated.h"

class UContrarySaveGame;

// Контекстные подсказки онбординга (Этап F1). Каждая показывается ОДИН раз за профиль
// (флаг в сейве). Порядок значений — как флаги в UContrarySaveGame.
UENUM()
enum class EOnboardingHint : uint8
{
	Movement,   // старт новой игры: передвижение/атака/смена оружия
	Pickup,     // первый доступный подбор предмета
	Elder,      // первое приближение к старосте
	Inventory,  // первое открытие инвентаря
	Death,      // первая смерть (текст СТРОГО по ADR-044 п.3 — без «можно вернуться и забрать»)
	LeaveVillage, // первый выход из деревни (ADR-074): напоминание, что прогресс сохраняется у костра
	// --- 07.10.2026 (список Рината «разгрузить надписи начала игры»): новые значения только
	// в КОНЕЦ — номер значения едет числом в событие шага обучения, старые номера не сдвигаем.
	Attack,       // рядом впервые оказался враг: «Нажми на врага, чтобы ударить»
	WeaponSwitch, // у игрока впервые появилось второе оружие: как его сменить
	Armor,        // сумка впервые открыта при наличии брони: слоты брони и строка «Защита»
	Count UMETA(Hidden)
};

// Что очередь подсказок сделала с предложенной надписью.
enum class EOnboardingHintOffer : uint8
{
	ShowNow,        // экран свободен — показать сразу
	Queued,         // показана другая (или идёт пауза) — встала в очередь
	RefreshCurrent  // эта же надпись уже на экране — только продлить её показ
};

/**
 * Очередь всплывающих подсказок (решение Рината 07.10.2026: «Одна подсказка за раз. Следующая
 * ждёт, пока предыдущая уйдёт, и не накладывается»). Чистая логика без UObject и мира — под
 * headless-тест (паттерн FLimpFirstHintState): время приходит параметром, показ виджета и
 * таймеры ведёт UOnboardingComponent.
 *
 * Порядок: Offer -> (ShowNow) ... MarkHidden -> пауза -> TakeNext -> ... Одинаковые надписи
 * в очереди не дублируются (иначе, например, отказ торговца копился бы на каждое нажатие).
 */
struct FOnboardingHintQueue
{
	bool IsShowing() const { return bShowing; }
	bool IsWaitingGap() const { return bGap; }
	int32 NumPending() const { return Pending.Num(); }
	const FText& GetCurrent() const { return Current; }

	// Предложить надпись. ShowNow — вызывающий обязан показать её (очередь уже считает её
	// показанной с момента Now).
	EOnboardingHintOffer Offer(const FText& Text, double Now)
	{
		if (bShowing && Text.EqualTo(Current))
		{
			return EOnboardingHintOffer::RefreshCurrent;
		}
		if (!bShowing && !bGap && Pending.Num() == 0)
		{
			StartShowing(Text, Now);
			return EOnboardingHintOffer::ShowNow;
		}
		const bool bAlreadyQueued = Pending.ContainsByPredicate(
			[&Text](const FText& Queued) { return Queued.EqualTo(Text); });
		if (!bAlreadyQueued)
		{
			Pending.Add(Text);
		}
		return EOnboardingHintOffer::Queued;
	}

	// Показанная надпись ушла с экрана. Есть очередь — начинается пауза перед следующей.
	void MarkHidden()
	{
		bShowing = false;
		Current = FText::GetEmpty();
		bGap = Pending.Num() > 0;
	}

	// Пауза кончилась: взять следующую надпись. false — показывать нечего.
	bool TakeNext(double Now, FText& OutText)
	{
		bGap = false;
		if (bShowing || Pending.Num() == 0)
		{
			return false;
		}
		OutText = Pending[0];
		Pending.RemoveAt(0);
		StartShowing(OutText, Now);
		return true;
	}

	// Может ли ввод игрока погасить показанную надпись: только после MinShowSeconds на
	// экране. Без этого порога очередь пролистывалась бы мгновенно — на телефоне палец
	// почти всегда на экране, и каждая следующая надпись гасла бы, не успев появиться.
	bool MayDismissByInput(double Now, float MinShowSeconds) const
	{
		return bShowing && (Now - ShownAt) >= static_cast<double>(MinShowSeconds);
	}

	void Reset()
	{
		Pending.Reset();
		Current = FText::GetEmpty();
		bShowing = false;
		bGap = false;
	}

private:
	void StartShowing(const FText& Text, double Now)
	{
		Current = Text;
		bShowing = true;
		ShownAt = Now;
	}

	TArray<FText> Pending;
	FText Current;
	bool bShowing = false;
	bool bGap = false;
	double ShownAt = 0.0;
};

/**
 * Онбординг первых минут (Этап F1, план v2). Живёт на APlayerCharacter.
 *
 * Показывает лёгкие UMG-всплывашки (UOnboardingHintWidget) по событиям, которые дёргает
 * контроллер (подбор/староста/инвентарь/смерть) и сам компонент (стартовая подсказка
 * движения по таймеру BeginPlay). Скрытие — по таймеру HintDuration или по ЛЮБОМУ вводу
 * (AnyKey-биндинг контроллера -> DismissCurrentHint). Флаги «показано» персистятся в
 * UContrarySaveGame сразу при показе.
 */
UCLASS(ClassGroup = (Retention), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UOnboardingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOnboardingComponent();

	// Показать подсказку, если она ещё не показывалась этому профилю. Идемпотентно
	// (повторные вызовы каждый тик безвредны — флаг проверяется в памяти, без чтения диска).
	void TryShowHint(EOnboardingHint Hint);

	// Скрыть активную подсказку по вводу игрока. Гасит только надпись, провисевшую не меньше
	// MinShowBeforeInputDismiss секунд (см. FOnboardingHintQueue::MayDismissByInput).
	void DismissCurrentHint();

	// Уже ли показана (в этом профиле) такая подсказка — чтобы вызывающий не считал условие
	// каждый кадр зря.
	bool IsHintShown(EOnboardingHint Hint) const;

	// Текст подсказки для данного способа управления (bTouch — экранные кнопки). Пустой
	// текст значит «шаг отмечается и уходит в статистику, но на экран ничего не выводится»
	// (подсказка старосты и первое открытие сумки — решения Рината 07.10.2026). Публичный,
	// чтобы тексты и это правило держал автотест.
	FText GetHintText(EOnboardingHint Hint, bool bTouch) const;

	// Пора ли показать подсказку про удар: враг не дальше TriggerDistance (см).
	static bool ShouldShowAttackHint(float DistanceToEnemy, float TriggerDistance);

	// Пора ли показать подсказку про смену оружия: у игрока есть и холодное, и огнестрельное.
	static bool ShouldShowWeaponSwitchHint(bool bHasMelee, bool bHasRanged);

	// Отменить отложенную стартовую подсказку движения (Build 1): во время интро её показывает
	// контроллер РОВНО при передаче управления, а не по авто-таймеру BeginPlay (иначе всплыла бы
	// на чёрном экране/во время авто-подхода и была бы потрачена — подсказка одноразовая).
	void CancelPendingMovementHint();

	// Латинское имя шага обучения для аналитики издателя (Б4): movement/pickup/elder/
	// inventory/death/leave_village. Отдельно от EOnboardingHint, чтобы имя события не
	// менялось от перестановки значений перечисления.
	static const TCHAR* GetHintAnalyticsId(EOnboardingHint Hint);

	// Входит ли подсказка в условие «обучение пройдено» (решение game-lead 08-06): входят
	// ВСЕ, КРОМЕ подсказки смерти. Причина: издатель меряет долю игроков, освоивших
	// управление в первые минуты; если засчитывать завершение только после первой гибели,
	// эта цифра начинает мерить смертность, а не обучение. Событие самого шага у подсказки
	// смерти при этом остаётся — оно полезно отдельно.
	//
	// ADR-074 (16.08): подсказка выхода из деревни тоже НЕ входит — показатель «обучение
	// пройдено» обязан остаться сравнимым с выпущенной 0.1.0 (1), где шагов четыре, иначе
	// цифры издателя сломаются. Событие самого шага (leave_village) отправляется отдельно.
	static bool CountsTowardTutorialCompletion(EOnboardingHint Hint);

	// Сколько всего шагов входит в условие завершения (едет числом в событие «обучение
	// пройдено», поэтому считается тем же правилом, а не пишется руками).
	static int32 GetTutorialCompletionStepCount();

	// Показать разовую всплывашку с готовым текстом — тем же тостом, что и подсказки
	// онбординга. В отличие от TryShowHint НЕ ведёт признак «показано»: одноразовость
	// обеспечивает тот, кто зовёт (например староста — по флагу в сейве). Нужно для
	// сообщений вида «Получено: Бинт».
	void ShowTransientHint(const FText& Text);

	// Время показа подсказки до автоскрытия, сек (задание: ~6-8).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "1.0", DisplayPriority = "1"))
	float HintDuration = 7.0f;

	// Задержка стартовой подсказки движения после начала игры, сек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "0.0", DisplayPriority = "2"))
	float MovementHintDelay = 1.5f;

	// --- Очередь подсказок (решение Рината 07.10.2026: «одна подсказка за раз») ---

	// Пауза между подсказками очереди, сек: предыдущая ушла — следующая появляется не сразу.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Пауза между подсказками, сек"))
	float HintGapSeconds = 1.0f;

	// Сколько секунд подсказка обязана провисеть, прежде чем её сможет погасить ввод игрока.
	// 0 — гаснет от первого же нажатия, как было раньше (тогда очередь на телефоне
	// пролистывается почти мгновенно).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Сколько секунд подсказку нельзя погасить нажатием"))
	float MinShowBeforeInputDismiss = 2.0f;

	// На каком расстоянии до врага (см) показывается подсказка про удар («когда рядом
	// окажется первый враг»). Проверку ведёт контроллер по захваченной ближайшей цели.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Подсказка про удар: расстояние до врага, см"))
	float AttackHintDistance = 900.0f;

	// Показывать ли текст подсказки у старосты. Выключено решением Рината 07.10.2026: её
	// полностью повторяют верхняя строка задачи и надпись у кнопки. Признак «показано» и
	// событие шага elder уходят в тот же момент в любом случае.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (DisplayPriority = "2",
		DisplayName = "Показывать подсказку «Поговори со старостой»"))
	bool bShowElderHint = false;

	// --- Тексты подсказок (директива Рината 07-18: настраиваются в BP_PlayerCharacter).
	// Локализация (ADR-050): FText, дефолты через NSLOCTEXT.
	//
	// ДВА ВАРИАНТА ТАМ, ГДЕ ПОДСКАЗКА НАЗЫВАЕТ СПОСОБ УПРАВЛЕНИЯ. На ПК управление
	// клавишами, на телефоне клавиш нет — поэтому у подсказок про движение и подбор есть
	// пара «клавиатурный текст / тач-текст». Какой показать, решает наличие тач-слоя
	// (AContrarySurvivorPlayerController::HasTouchLayer) — тот же признак, по которому HUD
	// выбирает подсказку прокрутки магазина. У подсказок без упоминания управления
	// (староста, инвентарь, смерть) вариант один — второй текст был бы копией.
	//
	// Клавиши сверены с реальными биндингами проекта: движение W/A/S/D (IMC_Default),
	// атака — клик (IA_Fire=ЛКМ), смена оружия Q, подбор E, инвентарь I/Tab
	// (legacy ActionMapping, Config/DefaultInput.ini). Названия тач-кнопок в текстах ниже
	// совпадают с подписями кнопок слоя (ContrarySurvivorPlayerController.cpp). ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "3"))
	// 07.10.2026 (Ринат): на старте — только про ходьбу. Про удар и смену оружия говорят
	// отдельные подсказки ниже, каждая в свой момент.
	FText HintTextMovement = NSLOCTEXT("OnboardingComponent", "HintMovement",
		"Ходи на W, A, S, D");

	// Тот же смысл для телефона: способ другой, действие то же.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "4"))
	FText HintTextMovementTouch = NSLOCTEXT("OnboardingComponent", "HintMovementTouch",
		"Веди пальцем по левой части экрана, чтобы идти");

	// Про удар — когда рядом впервые оказался враг (AttackHintDistance). Один текст на оба
	// управления: и мышью, и пальцем по врагу именно нажимают.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "4"))
	FText HintTextAttack = NSLOCTEXT("OnboardingComponent", "HintAttack",
		"Нажми на врага, чтобы ударить");

	// Про смену оружия — когда у игрока впервые появилось второе оружие.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "4"))
	FText HintTextWeaponSwitch = NSLOCTEXT("OnboardingComponent", "HintWeaponSwitch",
		"Q — сменить оружие, V — удар ножом");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "4"))
	FText HintTextWeaponSwitchTouch = NSLOCTEXT("OnboardingComponent", "HintWeaponSwitchTouch",
		"Кнопка ОРУЖИЕ — сменить оружие, кнопка с ножом — удар ножом");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "5"))
	FText HintTextPickup = NSLOCTEXT("OnboardingComponent", "HintPickup", "Нажми E, чтобы подобрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "6"))
	FText HintTextPickupTouch = NSLOCTEXT("OnboardingComponent", "HintPickupTouch",
		"Нажми ДЕЙСТВИЕ, чтобы подобрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "7"))
	FText HintTextElder = NSLOCTEXT("OnboardingComponent", "HintElder",
		"Поговори со старостой — у него есть работа");

	// 07.10.2026 (Ринат): этот текст про броню показывается не при первом открытии сумки, а
	// при первом её открытии, когда у игрока уже есть броня (значение Armor перечисления).
	// Само первое открытие сумки по-прежнему отмечается шагом inventory, но без текста.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "8"))
	FText HintTextInventory = NSLOCTEXT("OnboardingComponent", "HintInventory",
		"Слева — слоты брони. Броня снижает урон — следи за строкой «Защита»");

	// СТРОГО эта формулировка (ADR-044 п.3): БЕЗ «можно вернуться и забрать».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "9"))
	FText HintTextDeath = NSLOCTEXT("OnboardingComponent", "HintDeath",
		"Часть монет утрачена. Расходники обронены на месте гибели.");

	// ADR-074 (Ринат, 16.08.2026): первый ВЫХОД из деревни — напоминание, что прогресс
	// сохраняется только у костра (в этой версии костёр — только в деревне). Управления не
	// называет, поэтому вариант один. Показывается на переходе «был внутри → вышел», а не
	// «стоит снаружи»: игра начинается за деревней, и на старте подсказка всплывать не должна
	// (проверку ведёт контроллер, AContrarySurvivorPlayerController::UpdateLeaveVillageHint).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "10"))
	FText HintTextLeaveVillage = NSLOCTEXT("OnboardingComponent", "HintLeaveVillage",
		"Уходишь из деревни. Не забывай: прогресс сохраняется только у костра");

	// Стиль тоста (цвет плашки/текста, шрифт, позиция, размер) — применяется при создании виджета.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding", meta = (DisplayPriority = "11"))
	FOnboardingHintStyle HintStyle;

protected:
	virtual void BeginPlay() override;

private:
	// Предложить надпись очереди: экран свободен — показать, занят — поставить в очередь.
	void ShowWidget(const FText& Text);

	// Вывести надпись на экран и завести таймер автоскрытия. false — показать не удалось
	// (нет контроллера или виджета).
	bool PresentHint(const FText& Text);

	// Убрать показанную надпись; если в очереди есть следующая — завести паузу перед ней.
	void HideActiveWidget();

	// Пауза между подсказками кончилась — показать следующую из очереди.
	void ShowNextQueuedHint();

	// Текущее время мира для очереди (0 без мира).
	double GetQueueTime() const;

	// Очередь подсказок (одна на экране, остальные ждут).
	FOnboardingHintQueue HintQueue;

	FTimerHandle GapTimer;

	// Показан ли тач-слой у владельца (телефон, либо ПК с включённым тач-управлением).
	// Отвечает именно «показан ли слой», а не «телефон ли это» — так и задумано:
	// подсказка должна называть то управление, которое игрок видит на экране.
	bool IsTouchLayerActive() const;

	// Записать флаг «показано» в слот сейва (load-or-create, правит только свой флаг).
	void PersistShownFlag(EOnboardingHint Hint);

	// Показаны ли все подсказки, входящие в условие «обучение пройдено» (Б4). Какие входят —
	// решает CountsTowardTutorialCompletion: все, кроме подсказки смерти.
	bool AreTutorialCompletionHintsShown() const;

	// Кэш флагов «показано» в памяти (грузится один раз в BeginPlay из сейва): TryShowHint
	// зовётся из Tick контроллера — читать слот с диска каждый тик нельзя.
	bool bShown[static_cast<int32>(EOnboardingHint::Count)] = {};

	UPROPERTY()
	TObjectPtr<UOnboardingHintWidget> ActiveWidget;

	FTimerHandle HideTimer;
	FTimerHandle MovementHintTimer;
};
