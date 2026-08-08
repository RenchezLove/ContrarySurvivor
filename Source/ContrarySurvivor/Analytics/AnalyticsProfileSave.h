// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AnalyticsProfileSave.generated.h"

/**
 * Ответ игрока на экран согласия (Б6, задание издателя ADR-059). Хранится вместе с прочей
 * памятью на установку игры, поэтому переживает перезапуск и НЕ сбрасывается кнопкой
 * «Новая игра». Значение по умолчанию — «ещё не спрашивали»: согласие за игрока не ставится
 * (прямое требование издателя по РИ-30).
 */
UENUM()
enum class EDataConsentState : uint8
{
	// Игрок ещё не отвечал — экран согласия надо показать.
	Unknown,
	// «Принимаю»: рекламному набору уходит согласие, игровая статистика включается.
	Accepted,
	// «Не сейчас»: рекламному набору уходит отказ, статистика молчит. Игра полностью проходима.
	Declined
};

/**
 * Служебная память аналитики на УСТАНОВКУ игры (Б4, задание издателя ADR-059).
 *
 * Лежит в ОТДЕЛЬНОМ слоте сохранения ('ContraryAnalytics'), а не в игровом 'ContrarySave',
 * специально: кнопка «Новая игра» и отладочная очистка (F12) стирают именно игровой слот
 * (APlayerCharacter::ResetToNewGame, AContrarySurvivorPlayerController), а признак «игра уже
 * запускалась на этом устройстве» переживать новую игру ОБЯЗАН — новая игра это не новая
 * установка. Слот пропадает только вместе с данными приложения (переустановка/очистка данных),
 * а это ровно и есть «новая установка».
 *
 * Здесь же ведётся защита от повторной отправки событий обучения: издателю нужна воронка
 * «установка → шаги обучения → завершение», в которой каждое событие приходит не более
 * одного раза за установку, даже если игрок начал игру заново.
 */
UCLASS()
class CONTRARYSURVIVOR_API UAnalyticsProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	// Событие первого запуска уже отправлено.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	bool bFirstLaunchReported = false;

	// Игра уже запускалась на этом устройстве (волна «Главное меню», ADR-062: со второго
	// запуска игра открывается главным меню). НАРОЧНО отдельно от bFirstLaunchReported: тот
	// отмечает отправку события статистики и взводится только после согласия игрока и при
	// найденных ключах — для решения «показывать ли меню» он не годится.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	bool bGameLaunchedBefore = false;

	// Событие завершения обучения уже отправлено.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	bool bTutorialCompletedReported = false;

	// Имена шагов обучения, о которых уже отправлено событие (movement/pickup/elder/...).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	TArray<FString> ReportedTutorialSteps;

	// Ответ игрока на экран согласия (Б6). Здесь же — по той же причине, что и признак
	// первого запуска: решение должно пережить перезапуск, но не сбрасываться новой игрой.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analytics")
	EDataConsentState ConsentState = EDataConsentState::Unknown;

	// --- Пометки «отправлено». Возвращают true ТОЛЬКО в первый раз: вызывающий по этому
	// признаку решает, слать событие или промолчать. Диска не касаются — запись слота
	// делает UAnalyticsSubsystem, чтобы место записи было одно. ---

	bool MarkFirstLaunchReported();
	bool MarkGameLaunched();
	bool MarkTutorialStepReported(const FString& StepId);
	bool MarkTutorialCompletedReported();

	// Сколько разных шагов обучения уже отмечено (для проверки «пройдены все»).
	int32 GetReportedTutorialStepCount() const { return ReportedTutorialSteps.Num(); }

	// --- Единственное место, где эта память лежит на диске. Через эти три метода к слоту
	// обращаются ВСЕ (иначе две подсистемы держали бы два своих снимка и затирали друг друга). ---

	// Имя слота сохранения. Отдельное от игрового 'ContrarySave' намеренно (см. описание класса).
	static const TCHAR* GetSlotName() { return TEXT("ContraryAnalytics"); }
	static int32 GetUserIndex() { return 0; }

	// Читает слот с диска, а при его отсутствии создаёт чистый объект (никогда не null,
	// если движок сумел создать объект).
	static UAnalyticsProfileSave* LoadOrCreate();

	// Записывает объект в слот. Возвращает успех записи.
	static bool Write(UAnalyticsProfileSave* Save);
};
