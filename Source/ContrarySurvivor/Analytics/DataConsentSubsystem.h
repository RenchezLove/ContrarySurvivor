// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ContrarySurvivor/Analytics/AnalyticsProfileSave.h" // EDataConsentState
#include "DataConsentSubsystem.generated.h"

class UConsentScreenWidget;

/**
 * Согласие игрока на обработку данных и ссылка на политику (Б6, задание издателя ADR-059;
 * условие издателя по РИ-30: «согласие игрока НЕ ставить за игрока»).
 *
 * Что делает:
 *   1. При первом запуске показывает экран согласия (UConsentScreenWidget) — ДО первого
 *      показа рекламы и ДО отправки первого события статистики. Мир на это время ставится
 *      на паузу, чтобы за спиной у экрана ничего не происходило.
 *   2. Ответ игрока сохраняет в память на установку игры (UAnalyticsProfileSave, отдельный
 *      слот) — решение переживает перезапуск и НЕ сбрасывается кнопкой «Новая игра».
 *   3. Разносит решение по службам: статистике (UAnalyticsSubsystem) и рекламному набору
 *      (UYandexAdService).
 *   4. Открывает ссылку на политику из настройки проекта (UDataConsentSettings).
 *
 * Почему подсистема, а не контроллер игрока: остальные экраны открывает контроллер, но
 * согласие нужно спросить раньше и независимо от игрового процесса, а сам файл контроллера
 * в этой задаче принадлежит другому программисту. Момент показа ловим коротким ожиданием
 * готовности экрана (см. TryShowConsentScreen).
 */
UCLASS()
class CONTRARYSURVIVOR_API UDataConsentSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Хелпер доступа из игрового кода (null вне игры/без GameInstance).
	static UDataConsentSubsystem* Get(const UObject* WorldContextObject);

	// Сохранённый ответ игрока.
	EDataConsentState GetConsentState() const;
	bool IsConsentGranted() const { return GetConsentState() == EDataConsentState::Accepted; }

	// Записать решение игрока и применить его к статистике и рекламе. Зовётся и с экрана
	// согласия, и из меню паузы (игрок вправе передумать).
	void SetConsent(bool bGranted);

	// Есть ли в настройке непустой адрес политики.
	bool HasPrivacyPolicyUrl() const;

	// Открыть политику во внешнем браузере. Адреса нет — тихо ничего не делаем (пустую
	// страницу игроку не показываем), одна строка в журнал.
	void OpenPrivacyPolicy() const;

	// Готовая строка версии сборки для меню паузы («Версия 0.1.0 (сборка 1)»).
	FText GetBuildVersionText() const;

	// --- Ожидание ответа игрока (переезд запуска на загрузочный уровень, ADR-067 п.5) ---

	// Ждём ли мы прямо сейчас ответа игрока про сбор данных. Истина, пока окно согласия на
	// экране ЛИБО мы ещё ждём момента, когда его можно показать. На загрузочном уровне
	// главное меню не открывается, пока это истина: два окна друг поверх друга игроку не нужны.
	//
	// ⛔ Ожидание КОНЕЧНО при любом раскладе: если экрана так и не появилось, ожидание
	// сдаётся по счётчику шагов (см. MaxWaitSteps), а при выключенном в настройках экране
	// согласия и при уже сохранённом ответе игрока оно не начинается вовсе.
	bool IsWaitingForPlayerAnswer() const { return bWaitingForPlayerAnswer; }

	// --- Чистые правила ожидания (гоняются автотестами без живой игры) ---

	// Ждём ли ответа: окно на экране или ожидание готовности экрана ещё идёт.
	static bool ShouldWaitForConsentAnswer(bool bScreenOnScreen, bool bWaitingForScreen);

	// Продолжать ли ждать готовности экрана. Экран готов — ждать больше нечего; шагов
	// израсходовано не меньше предела — сдаёмся (иначе ожидание стало бы вечным).
	static bool ShouldKeepWaitingForScreen(bool bScreenReady, int32 StepsDone, int32 MaxSteps);

	// Предел шагов ожидания — публично, чтобы автотест доказывал конечность на реальном числе,
	// а не на выдуманном.
	static int32 GetMaxScreenWaitSteps();

private:
	// Показ экрана согласия: ждём, пока появятся вьюпорт и контроллер игрока, и показываем.
	// Возвращает true, пока нужно продолжать ждать (это же значение возвращает тикер).
	bool TryShowConsentScreen(float DeltaTime);

	void CloseConsentScreen();

	void HandleAccepted();
	void HandleDeclined();
	void HandlePolicyRequested();

	// Разнести решение по службам: статистика + рекламный набор.
	void ApplyConsentToServices(bool bGranted);

	UPROPERTY()
	TObjectPtr<UConsentScreenWidget> ConsentWidget;

	// Ожидание готовности экрана (тикер движка; снимается сразу после показа).
	FTSTicker::FDelegateHandle WaitHandle;

	// Сколько шагов ожидания уже прошло (страховка от бесконечного ожидания).
	int32 WaitSteps = 0;

	// Ждём ли ответа игрока прямо сейчас (см. IsWaitingForPlayerAnswer). Взводится, только
	// когда мы действительно собираемся спросить, и гасится в трёх случаях: игрок ответил,
	// окно не удалось создать, ожидание сдалось по счётчику шагов.
	bool bWaitingForPlayerAnswer = false;

	// Пауза стояла ещё ДО экрана согласия (например, стартовый экран) — тогда по закрытию
	// её не снимаем.
	bool bWasPausedBefore = false;

	// Курсор мыши был показан ещё до экрана — восстанавливаем ровно это состояние.
	bool bCursorShownBefore = false;
};
