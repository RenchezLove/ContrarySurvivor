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
	Count UMETA(Hidden)
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

	// Скрыть активную подсказку (любой ввод игрока / смена экрана).
	void DismissCurrentHint();

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
	FText HintTextMovement = NSLOCTEXT("OnboardingComponent", "HintMovement",
		"Ходи на W, A, S, D. Нажми на врага, чтобы ударить. Q — сменить оружие");

	// Тот же смысл для телефона: способ другой, действие то же.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "4"))
	FText HintTextMovementTouch = NSLOCTEXT("OnboardingComponent", "HintMovementTouch",
		"Веди пальцем по левой части экрана, чтобы идти. Нажми на врага, чтобы ударить. Кнопка ОРУЖИЕ — сменить оружие");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "5"))
	FText HintTextPickup = NSLOCTEXT("OnboardingComponent", "HintPickup", "Нажми E, чтобы подобрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "6"))
	FText HintTextPickupTouch = NSLOCTEXT("OnboardingComponent", "HintPickupTouch",
		"Нажми ДЕЙСТВИЕ, чтобы подобрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Onboarding|Texts", meta = (DisplayPriority = "7"))
	FText HintTextElder = NSLOCTEXT("OnboardingComponent", "HintElder",
		"Поговори со старостой — у него есть работа");

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
	// Текст подсказки — из EditAnywhere-полей выше (у движения и подбора выбирается
	// клавиатурный или тач-вариант, см. IsTouchLayerActive).
	FText GetHintText(EOnboardingHint Hint) const;

	// Показан ли тач-слой у владельца (телефон, либо ПК с включённым тач-управлением).
	// Отвечает именно «показан ли слой», а не «телефон ли это» — так и задумано:
	// подсказка должна называть то управление, которое игрок видит на экране.
	bool IsTouchLayerActive() const;

	// Записать флаг «показано» в слот сейва (load-or-create, правит только свой флаг).
	void PersistShownFlag(EOnboardingHint Hint);

	// Показаны ли все подсказки, входящие в условие «обучение пройдено» (Б4). Какие входят —
	// решает CountsTowardTutorialCompletion: все, кроме подсказки смерти.
	bool AreTutorialCompletionHintsShown() const;

	void ShowWidget(const FText& Text);
	void HideActiveWidget();

	// Кэш флагов «показано» в памяти (грузится один раз в BeginPlay из сейва): TryShowHint
	// зовётся из Tick контроллера — читать слот с диска каждый тик нельзя.
	bool bShown[static_cast<int32>(EOnboardingHint::Count)] = {};

	UPROPERTY()
	TObjectPtr<UOnboardingHintWidget> ActiveWidget;

	FTimerHandle HideTimer;
	FTimerHandle MovementHintTimer;
};
