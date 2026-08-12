// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BootGameMode.generated.h"

/**
 * Режим игры ЗАГРУЗОЧНОГО уровня `L_Boot` (переезд запуска, ADR-067 п.5).
 *
 * Зачем нужен: до переезда игра стартовала прямо в лесу и деревне (GameDefaultMap указывал на
 * игровую карту), поэтому на телефоне до главного меню успевали загрузиться лес, звуки птиц и
 * весь мир — время и батарея тратились впустую. Теперь запуск идёт на пустом уровне, а мир
 * грузится только после «Продолжить» или «Новая игра».
 *
 * Чем отличается от игрового режима (BP_ContrarySurviorGameMode):
 *   • ПЕШКИ НЕТ вовсе (DefaultPawnClass пуст + игрок не «оживляется» при входе). Персонаж
 *     игрока завёл бы лесной фон и потянул свои ассеты — ровно то, от чего мы уходим;
 *   • ИГРОВОГО ИНТЕРФЕЙСА НЕТ (HUDClass пуст): на пустом уровне рисовать нечего;
 *   • контроллер игрока — ТОТ ЖЕ, что в мире (BP_ContrarySurviorPlayerController): в нём живут
 *     настройки Рината — картинки меню, ссылки на окна, привязки ввода. Потерять их нельзя.
 *
 * Как этот режим достаётся уровню: одной строкой в Config/DefaultEngine.ini, раздел
 * [/Script/EngineSettings.GameMapsSettings]:
 *     +GameModeMapPrefixes=(Name="L_Boot",GameMode="/Script/ContrarySurvivor.BootGameMode")
 * Движок разбирает начало имени карты в UGameInstance::CreateGameModeForURL (GameInstance.cpp:1543
 * движка 5.5), причём ПОСЛЕ настроек самого уровня — а у L_Boot режим в настройках уровня не
 * задан, значит сработает эта строка. Игровая карта L_World_C под начало имени "L_Boot" не
 * подпадает и свой режим сохраняет. Файл уровня руками не правим.
 */
UCLASS()
class CONTRARYSURVIVOR_API ABootGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABootGameMode();

	// Здесь подставляется класс контроллера из PlayerControllerAssetPath. Момент выбран не
	// случайно: InitGame зовётся сразу после создания режима (World.cpp:5298 движка 5.5) и
	// заведомо раньше входа игрока, то есть раньше, чем движок спросит PlayerControllerClass.
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	// Контроллер игрока для загрузочного уровня. Ссылка МЯГКАЯ и правится в редакторе: жёсткая
	// ссылка из конструктора тянула бы ассет ещё при создании класса, а нам нужен запасной путь.
	// Ассет не нашёлся — берём C++-класс контроллера и пишем предупреждение в журнал: меню
	// останется рабочим, потеряется только оформление, заданное Ринатом.
	UPROPERTY(EditDefaultsOnly, Category = "Загрузочный уровень",
		meta = (DisplayName = "Контроллер игрока (пусто — взять C++-класс)"))
	FSoftClassPath PlayerControllerAssetPath;
};
