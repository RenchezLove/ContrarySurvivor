// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тесты возрождения базы противника (ТЗ издателя 22.08.2026, бэклог №31/№35).
// Запуск (гоняет game-lead/qa, НЕ cpp-dev — правило Рината):
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.EnemyBase; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывают ЧИСТЫЕ правила ТЗ (EnemyBaseTierLogic + статики AMasterEnemyBase — база зовёт
// ровно их, тест не держит копию правил): цикл ступеней 1→…→5→3, паузы по зачищенной
// ступени, «пауза истекла» с подставным временем, прибавка здоровья, текст надписи по
// ступеням, «награда пустой не бывает», перенос памяти баз через CopyRetentionData.
// Сейв на диске и живой мир НЕ трогаются.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Actors/EnemyBaseTierLogic.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "UObject/UnrealType.h" // FArrayProperty/FScriptArrayHelper — чтение protected-дефолтов CDO

static constexpr EAutomationTestFlags EnemyBaseTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// ===========================================================================
// 1. Цикл ступеней (§2): +1 за зачистку, после пятой — третья, 3→4→5→3
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseTierCycleTest,
	"ContrarySurvivor.EnemyBase.TierCycle", EnemyBaseTestFlags)

bool FEnemyBaseTierCycleTest::RunTest(const FString& Parameters)
{
	using namespace EnemyBaseTierLogic;

	TestEqual(TEXT("Зачистка 1-й -> 2-я"), NextTierAfterClear(1), 2);
	TestEqual(TEXT("Зачистка 2-й -> 3-я"), NextTierAfterClear(2), 3);
	TestEqual(TEXT("Зачистка 3-й -> 4-я"), NextTierAfterClear(3), 4);
	TestEqual(TEXT("Зачистка 4-й -> 5-я"), NextTierAfterClear(4), 5);
	TestEqual(TEXT("Зачистка 5-й -> ТРЕТЬЯ (ТЗ §2 дословно)"), NextTierAfterClear(5), 3);

	// Цикл дальше: 3→4→5→3 без выхода за границы.
	int32 Tier = 5;
	const int32 Expected[] = { 3, 4, 5, 3, 4, 5 };
	for (int32 Step = 0; Step < 6; ++Step)
	{
		Tier = NextTierAfterClear(Tier);
		TestEqual(FString::Printf(TEXT("Цикл, шаг %d"), Step + 1), Tier, Expected[Step]);
	}

	// Кривые настройки экземпляра не роняют правила.
	TestEqual(TEXT("Ступень 0 клампится в 1"), ClampTier(0), 1);
	TestEqual(TEXT("Ступень 9 клампится в 5"), ClampTier(9), 5);
	TestEqual(TEXT("Зачистка «9-й» = зачистка пятой -> третья"), NextTierAfterClear(9), 3);
	return true;
}

// ===========================================================================
// 2. Паузы (§1) и значения «по ступени»: выбор по ЗАЧИЩЕННОЙ ступени, короткий
//    массив живёт по последнему значению, пустой — по запасному
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBasePauseLookupTest,
	"ContrarySurvivor.EnemyBase.PauseAndPerTierValues", EnemyBaseTestFlags)

bool FEnemyBasePauseLookupTest::RunTest(const FString& Parameters)
{
	using namespace EnemyBaseTierLogic;

	const TArray<float> Pauses = { 15.0f, 30.0f, 45.0f, 55.0f, 60.0f };
	TestEqual(TEXT("Пауза за 1-ю ступень — 15 мин"), PauseMinutesForClearedTier(1, Pauses), 15.0f);
	TestEqual(TEXT("Пауза за 3-ю ступень — 45 мин"), PauseMinutesForClearedTier(3, Pauses), 45.0f);
	TestEqual(TEXT("Пауза за 5-ю ступень — 60 мин"), PauseMinutesForClearedTier(5, Pauses), 60.0f);

	const TArray<float> Short = { 10.0f, 20.0f };
	TestEqual(TEXT("Короткий массив: старшие ступени по последнему значению"),
		PauseMinutesForClearedTier(5, Short), 20.0f);
	TestEqual(TEXT("Пустой массив: запасная пауза 60 мин"),
		PauseMinutesForClearedTier(2, TArray<float>()), 60.0f);

	// Прибавка здоровья (§3): по ступени, отрицательная настройка клампится в ноль.
	const TArray<float> Bonuses = { 0.0f, 12.0f, 19.0f, 22.0f, 27.0f };
	TestEqual(TEXT("Ступень 1 — без прибавки"), HealthBonusForTier(1, Bonuses), 0.0f);
	TestEqual(TEXT("Ступень 2 — +12"), HealthBonusForTier(2, Bonuses), 12.0f);
	TestEqual(TEXT("Ступень 5 — +27"), HealthBonusForTier(5, Bonuses), 27.0f);
	TestEqual(TEXT("Отрицательная настройка не лечит врага в минус"),
		HealthBonusForTier(1, TArray<float>({ -5.0f })), 0.0f);
	return true;
}

// ===========================================================================
// 3. Пауза по РЕАЛЬНЫМ часам (§1): истечение с подставным временем
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBasePauseElapsedTest,
	"ContrarySurvivor.EnemyBase.RespawnPauseElapsed", EnemyBaseTestFlags)

bool FEnemyBasePauseElapsedTest::RunTest(const FString& Parameters)
{
	using namespace EnemyBaseTierLogic;

	const FDateTime ClearTime(2026, 8, 22, 12, 0, 0);

	TestFalse(TEXT("Через 14 минут пауза в 15 мин НЕ истекла"),
		IsRespawnPauseElapsed(ClearTime, ClearTime + FTimespan::FromMinutes(14.0), 15.0f));
	TestTrue(TEXT("Ровно через 15 минут — истекла"),
		IsRespawnPauseElapsed(ClearTime, ClearTime + FTimespan::FromMinutes(15.0), 15.0f));
	TestTrue(TEXT("Через час — истекла и подавно"),
		IsRespawnPauseElapsed(ClearTime, ClearTime + FTimespan::FromHours(1.0), 15.0f));
	TestTrue(TEXT("Зачисток не было (нулевое время) — паузы нет"),
		IsRespawnPauseElapsed(FDateTime(0), ClearTime, 60.0f));
	return true;
}

// ===========================================================================
// 4. Текст надписи при входе (§5): 1-я — только имя, 2-я — фраза «более
//    опытные», 3-я и выше — «ещё более»; пустые куски не оставляют мусора
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseAnnounceTextTest,
	"ContrarySurvivor.EnemyBase.AnnounceText", EnemyBaseTestFlags)

bool FEnemyBaseAnnounceTextTest::RunTest(const FString& Parameters)
{
	const FText Name = FText::FromString(TEXT("Лагерь бандитов"));
	const FText Sep = FText::FromString(TEXT(". "));
	const FText T2 = FText::FromString(TEXT("Эти выглядят более опытными"));
	const FText T3 = FText::FromString(TEXT("Эти выглядят ещё более опытными"));

	TestEqual(TEXT("1-я ступень — только название"),
		AMasterEnemyBase::BuildAnnounceText(Name, Sep, T2, T3, 1).ToString(),
		FString(TEXT("Лагерь бандитов")));
	TestEqual(TEXT("2-я ступень — название + фраза «более опытные»"),
		AMasterEnemyBase::BuildAnnounceText(Name, Sep, T2, T3, 2).ToString(),
		FString(TEXT("Лагерь бандитов. Эти выглядят более опытными")));
	TestEqual(TEXT("5-я ступень — название + фраза «ещё более»"),
		AMasterEnemyBase::BuildAnnounceText(Name, Sep, T2, T3, 5).ToString(),
		FString(TEXT("Лагерь бандитов. Эти выглядят ещё более опытными")));
	TestEqual(TEXT("Пустая фраза ступени — остаётся только название (без хвоста-разделителя)"),
		AMasterEnemyBase::BuildAnnounceText(Name, Sep, FText::GetEmpty(), T3, 2).ToString(),
		FString(TEXT("Лагерь бандитов")));
	return true;
}

// ===========================================================================
// 5. Награда (§4): «пусто не бывает» — выбор записи со всеми фолбэками;
//    дефолты конструктора базы держат все пять ступеней непустыми
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseRewardPickTest,
	"ContrarySurvivor.EnemyBase.RewardNeverEmpty", EnemyBaseTestFlags)

bool FEnemyBaseRewardPickTest::RunTest(const FString& Parameters)
{
	// Синтетика: запись 3-й ступени пуста -> фолбэк на первую; всё пусто -> INDEX_NONE.
	TArray<FEnemyBaseTierReward> Rewards;
	Rewards.SetNum(5);
	Rewards[0].Money = 10.0f;                 // первая непустая
	Rewards[1].Money = 20.0f;
	// Rewards[2] пустая намеренно
	Rewards[3].Money = 40.0f;
	Rewards[4].Money = 50.0f;

	TestEqual(TEXT("Запись своей ступени, когда она непустая"),
		AMasterEnemyBase::PickRewardTierIndex(Rewards, 2), 1);
	TestEqual(TEXT("Пустая запись ступени -> фолбэк на первую (пусто не бывает)"),
		AMasterEnemyBase::PickRewardTierIndex(Rewards, 3), 0);

	TArray<FEnemyBaseTierReward> AllEmpty;
	AllEmpty.SetNum(5);
	TestEqual(TEXT("Все записи пусты -> INDEX_NONE (вызывающий предупреждает)"),
		AMasterEnemyBase::PickRewardTierIndex(AllEmpty, 4), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Массива нет -> INDEX_NONE"),
		AMasterEnemyBase::PickRewardTierIndex(TArray<FEnemyBaseTierReward>(), 1), static_cast<int32>(INDEX_NONE));

	// Дефолты конструктора: пять ступеней, каждая непуста (контракт «пусто не бывает»).
	const AMasterEnemyBase* BaseCDO = GetDefault<AMasterEnemyBase>();
	if (!BaseCDO)
	{
		AddError(TEXT("Нет CDO базы"));
		return false;
	}
	// Читаем дефолты через рефлексию (поле protected — как его видел бы редактор).
	if (FArrayProperty* RewardsProp = FindFProperty<FArrayProperty>(AMasterEnemyBase::StaticClass(), TEXT("TierRewards")))
	{
		FScriptArrayHelper Helper(RewardsProp,
			RewardsProp->ContainerPtrToValuePtr<void>(const_cast<AMasterEnemyBase*>(BaseCDO)));
		TestEqual(TEXT("Дефолт: пять ступеней награды"), Helper.Num(), 5);
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const FEnemyBaseTierReward* Reward =
				reinterpret_cast<const FEnemyBaseTierReward*>(Helper.GetRawPtr(Index));
			TestTrue(FString::Printf(TEXT("Дефолт награды ступени %d непуст"), Index + 1),
				Reward && !Reward->IsEmpty());
		}
	}
	else
	{
		AddError(TEXT("Нет свойства TierRewards"));
	}
	return true;
}

// ===========================================================================
// 6. Память баз переживает автосейв костра: CopyRetentionData переносит записи
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseSaveCarryTest,
	"ContrarySurvivor.EnemyBase.SaveStateSurvivesRetentionCopy", EnemyBaseTestFlags)

bool FEnemyBaseSaveCarryTest::RunTest(const FString& Parameters)
{
	UContrarySaveGame* From = NewObject<UContrarySaveGame>(GetTransientPackage());
	UContrarySaveGame* To = NewObject<UContrarySaveGame>(GetTransientPackage());

	FSavedEnemyBaseState State;
	State.BaseId = FName(TEXT("BP_BanditBase_QA"));
	State.Tier = 4;
	State.LastClearedTier = 3;
	State.LastClearUtc = FDateTime(2026, 8, 22, 18, 30, 0);
	From->EnemyBaseStates.Add(State);

	UContrarySaveGame::CopyRetentionData(From, To);

	TestEqual(TEXT("Запись базы перенесена"), To->EnemyBaseStates.Num(), 1);
	if (To->EnemyBaseStates.Num() == 1)
	{
		TestEqual(TEXT("Идентификатор цел"), To->EnemyBaseStates[0].BaseId, State.BaseId);
		TestEqual(TEXT("Ступень цела"), To->EnemyBaseStates[0].Tier, 4);
		TestEqual(TEXT("Зачищенная ступень цела"), To->EnemyBaseStates[0].LastClearedTier, 3);
		TestTrue(TEXT("Время зачистки цело"), To->EnemyBaseStates[0].LastClearUtc == State.LastClearUtc);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
