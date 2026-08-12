// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Modes/BootGameMode.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

ABootGameMode::ABootGameMode()
{
	// Пешки на загрузочном уровне нет. Двух заслонов намеренно два, и они разные:
	//   • пустой DefaultPawnClass — «спавнить нечего»;
	//   • bStartPlayersAsSpectators — «и не пытайся». Без второго движок всё равно позвал бы
	//     RestartPlayer и написал бы в журнал предупреждение о неудачном спавне
	//     (GameModeBase.cpp:1237 движка 5.5), а игроку это ничего не даёт.
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = true;

	// Игрового интерфейса на пустом уровне нет: рисовать полосы здоровья и деньги не над чем.
	HUDClass = nullptr;

	// Контроллер игрока — тот же, что в мире: в нём лежат настройки Рината (картинки меню,
	// ссылки на окна, привязки ввода). Путь ассета вынесен в поле, чтобы правился в редакторе.
	PlayerControllerAssetPath = FSoftClassPath(
		TEXT("/Game/System/BP_ContrarySurviorPlayerController.BP_ContrarySurviorPlayerController_C"));

	// Значение на случай, если ассет не найдётся: игра обязана остаться играбельной.
	PlayerControllerClass = AContrarySurvivorPlayerController::StaticClass();
}

void ABootGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (PlayerControllerAssetPath.IsValid())
	{
		if (UClass* Loaded = PlayerControllerAssetPath.TryLoadClass<APlayerController>())
		{
			PlayerControllerClass = Loaded;
			UE_LOG(LogQA, Display,
				TEXT("QA: загрузочный уровень — контроллер игрока взят из ассета '%s'"),
				*PlayerControllerAssetPath.ToString());
			return;
		}

		UE_LOG(LogQA, Warning,
			TEXT("QA: загрузочный уровень — не нашёлся контроллер игрока '%s'. Работаем на C++-классе %s: меню будет рабочим, но без оформления, заданного в редакторе."),
			*PlayerControllerAssetPath.ToString(),
			*GetNameSafe(AContrarySurvivorPlayerController::StaticClass()));
	}
	else
	{
		UE_LOG(LogQA, Warning,
			TEXT("QA: загрузочный уровень — путь контроллера игрока не задан, берём C++-класс %s"),
			*GetNameSafe(AContrarySurvivorPlayerController::StaticClass()));
	}

	PlayerControllerClass = AContrarySurvivorPlayerController::StaticClass();
}
