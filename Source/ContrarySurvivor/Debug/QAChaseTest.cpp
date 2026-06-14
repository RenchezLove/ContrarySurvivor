// Fill out your copyright notice in the Description page of Project Settings.

// QA headless-автотест погони волков (CU-free): консольная команда `cs.TestWolfChase`.
//
// Зачем отдельная console-команда (а не UFUNCTION(Exec)): запускается через
// `-ExecCmds="cs.TestWolfChase"` в headless `-game` режиме. Регистрируется в IConsoleManager
// глобальным FAutoConsoleCommandWithWorld (живёт весь процесс). Сама оркестрация сценария — на
// AContrarySurvivorPlayerController::QA_RunWolfChaseTest() (там вся плумбинг: телепорт V, спавн,
// сэмплинг). Здесь только: получить игровой мир -> PlayerController 0 -> запустить тест, с
// ретраями, если пешка игрока ещё не заспавнилась к моменту выполнения ExecCmds.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Debug/QADebug.h"

// Число попыток дождаться пешки игрока (ExecCmds может отработать до её спавна). Процесс-глобально.
static int32 GQAWolfChaseAttempts = 0;

static void QAWolfChase_Run(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("QA-TEST: WOLF-CHASE FAIL (no game world for cs.TestWolfChase)"));
		return;
	}

	AContrarySurvivorPlayerController* PC =
		Cast<AContrarySurvivorPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (!PC || !Pawn)
	{
		// Пешка/контроллер ещё не готовы — короткий ретрай (до ~10с), мир тикает headless.
		if (GQAWolfChaseAttempts++ < 10)
		{
			FTimerHandle Th;
			FTimerDelegate Del = FTimerDelegate::CreateLambda([World]() { QAWolfChase_Run(World); });
			World->GetTimerManager().SetTimer(Th, Del, 1.0f, /*bLoop=*/false);
			UE_LOG(LogTemp, Display, TEXT("QA-TEST: waiting for player pawn (attempt %d/10)"), GQAWolfChaseAttempts);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("QA-TEST: WOLF-CHASE FAIL (no player pawn after 10 retries)"));
		}
		return;
	}

	GQAWolfChaseAttempts = 0;
	PC->QA_RunWolfChaseTest();
}

static FAutoConsoleCommandWithWorld GQATestWolfChaseCmd(
	TEXT("cs.TestWolfChase"),
	TEXT("Headless CU-free автотест погони волков: телепорт к Логову, спавн волков, сэмплинг ~15с, вердикт QA-TEST: WOLF-CHASE PASS/FAIL."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&QAWolfChase_Run));
