// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Централизованный QA-хелпер (Фаза 5, debug-инструменты под автотестера/Computer Use).
 *
 * НАЗНАЧЕНИЕ. Тестер читает игру по строкам "QA:" в логе и по экрану. Логи на экране
 * движка смещаются/теряются, поэтому:
 *   1) FQADebug::QA(...) пишет строку в LogQA (Display) и сразу FLUSH'ит GLog (строка не
 *      застревает в буфере при паузах/тормозах);
 *   2) хранит кольцевой буфер последних MaxMessages QA-сообщений, который HUD рисует
 *      экранным оверлеем (правый нижний угол) — стабильная «лента» последних событий.
 *
 * Плюс — глобальные QA-флаги для debug-клавиш (статические, project-agnostic, без
 * GameInstance-зависимостей): god-mode (неуязвимость+заморозка деградации), force-drop
 * (100% лут со всех врагов), видимость оверлея. Читаются в точках урона/деградации/дропа.
 *
 * Плоский plain-C++ класс (не UObject): статика переживает кадр и доступна из любого места
 * без поиска подсистемы. Это DEBUG-харнесс, не геймплей-состояние для сейва.
 */
class CONTRARYSURVIVOR_API FQADebug
{
public:
	// --- Глобальные QA-флаги (debug-клавиши) ---

	// God-mode: игрок неуязвим (TakeDamage игрока зануляется) И деградация голода/жажды
	// заморожена (тики деградации в UStatsComponent гейтятся этим флагом). Тумблер — клавиша J.
	static bool bGodMode;

	// Force-drop: ВСЕ враги роняют лут со 100% шансом (override ItemDropChance в APickup::DropLoot).
	// Тумблер — клавиша U.
	static bool bForceDrop;

	// Видимость экранного QA-оверлея (рисуется HUD). Тумблер — клавиша O; авто-вкл вместе с god-mode (J).
	static bool bOverlayVisible;

	// Сколько последних QA-сообщений держим в кольцевом буфере (рисует HUD).
	static int32 MaxMessages;

	// Логирует QA-сообщение. ВСЕГДА: UE_LOG(LogQA, Display) + GLog->Flush() (в лог-файл).
	// bScreen=true — ДОПОЛНИТЕЛЬНО кладёт в кольцевой буфер экранного оверлея (курируемые,
	// важные события). bScreen=false (дефолт) — только в лог-файл (спам: шаги/реген/частые тики),
	// чтобы не вытеснять полезные строки из оверлея. Повтор последней экранной строки сжимается
	// в "<msg> (xN)" вместо добавления новой.
	// WorldCtx сейчас не используется (буфер статический), оставлен для совместимости сигнатуры/будущего.
	static void QA(const UObject* WorldCtx, const FString& Msg, bool bScreen = false);

	// Доступ к буферу последних сообщений (для отрисовки оверлея на HUD). Старые — в начале.
	static const TArray<FString>& GetMessages();

	// Очистить буфер сообщений (например, при старте нового PIE-сеанса).
	static void ResetMessages();

private:
	static TArray<FString> Messages;
};
