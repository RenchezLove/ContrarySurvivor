// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFlowSubsystem.generated.h"

/**
 * С чем игрок едет с загрузочного уровня в игровой мир (ADR-067 п.5).
 *
 * ⚠ Имя перечисления с приставкой проекта намеренно: имена UENUM глобальны для всего движка
 * (UHT ругается «shares engine name»), поэтому короткое EWorldEntryIntent брать нельзя.
 */
UENUM()
enum class EContraryWorldEntryIntent : uint8
{
	// Намерения нет. Так выглядит запуск игры прямо на игровой карте (Play в редакторе) —
	// в этом случае мировой контроллер ведёт себя ровно как до переезда на загрузочный уровень.
	None,

	// Начать заново: сохранение уже стёрто на загрузочном уровне, в мире играет вступление.
	NewGame,

	// Продолжить: в мире сразу грузится сохранение, вступление не играет.
	Continue
};

/**
 * Намерение перехода в игровой мир (переезд запуска на загрузочный уровень, ADR-067 п.5).
 *
 * Зачем: решение «первый это запуск после установки или нет» принимается РОВНО ОДИН РАЗ и
 * только на загрузочном уровне `L_Boot`, потому что признак «игра уже запускалась» не
 * читается, а ставится в момент чтения (UAnalyticsSubsystem::MarkLaunchAndCheckWasLaunchedBefore).
 * Прочитай его второй раз уже в мире — и самый первый запуск после установки тихо сломается:
 * игрок увидел бы меню поверх мира. Поэтому в мир едет явное намерение, а мировой контроллер
 * ему подчиняется и заново ничего не решает.
 *
 * Почему подсистема экземпляра игры: она переживает смену уровня (объект игры один на всё
 * приложение) и не требует ни своего класса игры, ни правки настроек проекта. Тот же приём
 * уже применён в проекте — UAnalyticsSubsystem, UDataConsentSubsystem, UYandexAdService.
 */
UCLASS()
class CONTRARYSURVIVOR_API UGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Хелпер доступа из игрового кода (пусто вне игры/без объекта игры).
	static UGameFlowSubsystem* Get(const UObject* WorldContextObject);

	// Записать намерение перед переездом в мир.
	void SetWorldEntryIntent(EContraryWorldEntryIntent Intent);

	// Подсмотреть намерение, не сбрасывая его (диагностика и автотесты).
	EContraryWorldEntryIntent GetWorldEntryIntent() const { return WorldEntryIntent; }

	// Прочитать намерение и тут же сбросить его в «нет»: намерение одноразовое, оно относится
	// ровно к одному переезду. Не сбросишь — следующая загрузка мира (например, после смерти
	// с перезагрузкой уровня) отработала бы по старому намерению.
	EContraryWorldEntryIntent ConsumeWorldEntryIntent();

	// Человеческое имя намерения для журнала.
	static FString IntentToString(EContraryWorldEntryIntent Intent);

private:
	EContraryWorldEntryIntent WorldEntryIntent = EContraryWorldEntryIntent::None;
};
