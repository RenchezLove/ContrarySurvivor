// Fill out your copyright notice in the Description page of Project Settings.
//
// Проверки вызова меню отладки на телефоне (Ринат 25.10.2026): касание экрана тремя пальцами
// разом открывает и закрывает меню, обычная игра двумя пальцами — нет. Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.DebugMenu.Gesture; Quit"
//        -unattended -nopause -nosplash -nullrhi -abslog=<log>
//
// Время касаний задаётся в проверке явно — от часов машины ничего не зависит.
// Весь файл — под выключателем отладочного харнесса: в сборке для магазина жеста нет.

#include "Misc/AutomationTest.h"
#include "ContrarySurvivor/Debug/QADebug.h" // CONTRARY_WITH_QA_CHEATS

#if WITH_DEV_AUTOMATION_TESTS && CONTRARY_WITH_QA_CHEATS

#include "ContrarySurvivor/Debug/ContraryDebugMenu.h"
#include "ContrarySurvivor/Debug/ContraryDebugMenuGesture.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

static constexpr EAutomationTestFlags DebugMenuGestureTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace DebugMenuGestureTest
{
	// Касание пальцем с номером PointerIndex (признак касания стоит у этого вида события).
	static FPointerEvent Touch(int32 PointerIndex)
	{
		const FVector2D Where(100.0 + 50.0 * PointerIndex, 300.0);
		return FPointerEvent(static_cast<uint32>(0), static_cast<uint32>(PointerIndex), Where, Where, 1.0f, /*bPressLeftMouseButton=*/true);
	}
}

// --- Три пальца разом открывают меню, касание третьего игре не уходит ---
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuGestureThreeFingersOpenTest,
	"ContrarySurvivor.DebugMenu.Gesture.ThreeFingersAtOnceToggle", DebugMenuGestureTestFlags)

bool FDebugMenuGestureThreeFingersOpenTest::RunTest(const FString& Parameters)
{
	FContraryThreeFingerTapDetector Detector;
	TestFalse(TEXT("Первый палец жестом не считается"), Detector.OnTouchStarted(0, 10.00));
	TestFalse(TEXT("Второй палец жестом не считается"), Detector.OnTouchStarted(1, 10.05));
	TestTrue(TEXT("Третий палец почти одновременно — жест сработал"), Detector.OnTouchStarted(2, 10.10));
	TestTrue(TEXT("Касание третьего пальца поглощено (игре не уходит)"), Detector.IsSwallowed(2));
	TestFalse(TEXT("Первые два пальца не поглощены"), Detector.IsSwallowed(0) || Detector.IsSwallowed(1));
	TestTrue(TEXT("Отпускание третьего пальца тоже поглощается"), Detector.OnTouchEnded(2));
	TestFalse(TEXT("Отпускание первого пальца уходит игре"), Detector.OnTouchEnded(0));
	TestFalse(TEXT("Отпускание второго пальца уходит игре"), Detector.OnTouchEnded(1));
	TestEqual(TEXT("После отпускания пальцев на экране никого"), Detector.GetActiveTouchCount(), 0);

	// Повторный жест через секунду — снова срабатывает (закрыть меню).
	TestFalse(TEXT("Повтор: первый палец"), Detector.OnTouchStarted(3, 11.50));
	TestFalse(TEXT("Повтор: второй палец"), Detector.OnTouchStarted(4, 11.52));
	TestTrue(TEXT("Повтор: третий палец — жест сработал снова"), Detector.OnTouchStarted(5, 11.55));
	return true;
}

// --- Обычная игра двумя пальцами и поздний третий палец меню не открывают ---
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuGestureTwoFingersDoNotTest,
	"ContrarySurvivor.DebugMenu.Gesture.TwoFingersAndGameplayDoNotToggle", DebugMenuGestureTestFlags)

bool FDebugMenuGestureTwoFingersDoNotTest::RunTest(const FString& Parameters)
{
	FContraryThreeFingerTapDetector Detector;

	// Стик и огонь разом, много раз подряд (стрельба на бегу).
	bool bAnyFired = false;
	double Now = 20.0;
	for (int32 Round = 0; Round < 20; ++Round)
	{
		bAnyFired |= Detector.OnTouchStarted(0, Now);
		bAnyFired |= Detector.OnTouchStarted(1, Now + 0.01);
		Detector.OnTouchEnded(1);
		bAnyFired |= Detector.OnTouchStarted(1, Now + 0.15); // снова огонь, стик держим
		Detector.OnTouchEnded(0);
		Detector.OnTouchEnded(1);
		Now += 0.2;
	}
	TestFalse(TEXT("Два пальца (стик и огонь) меню не открывают"), bAnyFired);
	TestEqual(TEXT("Все пальцы отпущены"), Detector.GetActiveTouchCount(), 0);

	// Стик и огонь зажаты давно, третьим пальцем нажата перезарядка — это игра, а не жест.
	TestFalse(TEXT("Стик зажат"), Detector.OnTouchStarted(0, 30.0));
	TestFalse(TEXT("Огонь зажат"), Detector.OnTouchStarted(1, 31.0));
	TestFalse(TEXT("Перезарядка третьим пальцем при зажатых стике и огне — не жест"), Detector.OnTouchStarted(2, 31.2));
	TestFalse(TEXT("Перезарядка не поглощена — игра её получит"), Detector.IsSwallowed(2));
	Detector.Reset();

	// Три касания вразнобой: первое вне окна от третьего.
	TestFalse(TEXT("Вразнобой: первый"), Detector.OnTouchStarted(0, 40.0));
	TestFalse(TEXT("Вразнобой: второй"), Detector.OnTouchStarted(1, 40.2));
	TestFalse(TEXT("Вразнобой: третий — дольше окна от первого, не жест"), Detector.OnTouchStarted(2, 40.4));
	Detector.Reset();

	// Четвёртый палец сразу после жеста меню не закрывает.
	Detector.OnTouchStarted(0, 50.0);
	Detector.OnTouchStarted(1, 50.02);
	TestTrue(TEXT("Жест сработал"), Detector.OnTouchStarted(2, 50.04));
	TestFalse(TEXT("Четвёртый палец сразу после жеста — не повторный жест"), Detector.OnTouchStarted(3, 50.06));
	return true;
}

// --- Обработчик ввода: мышь не считается, касания — да; третье касание поглощается ---
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuGestureProcessorTest,
	"ContrarySurvivor.DebugMenu.Gesture.ProcessorRoutesTouchesOnly", DebugMenuGestureTestFlags)

bool FDebugMenuGestureProcessorTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate не поднят — обработчик ввода проверить нечем"));
		return false;
	}
	FSlateApplication& SlateApp = FSlateApplication::Get();

	int32 GestureCount = 0;
	FContraryDebugMenuTouchProcessor Processor([&GestureCount]() { ++GestureCount; });
	double Now = 100.0;
	Processor.NowProvider = [&Now]() { return Now; };

	// Три щелчка мышью подряд — не касания, жеста нет, щелчки уходят игре.
	const TSet<FKey> LeftPressed = { EKeys::LeftMouseButton };
	for (int32 Click = 0; Click < 3; ++Click)
	{
		const FPointerEvent Mouse(static_cast<uint32>(0), FVector2D(10.0, 10.0), FVector2D(10.0, 10.0),
			LeftPressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
		TestFalse(TEXT("Щелчок мышью не поглощается"), Processor.HandleMouseButtonDownEvent(SlateApp, Mouse));
		Now += 0.01;
	}
	TestEqual(TEXT("Мышь меню не открывает"), GestureCount, 0);

	// Два пальца — жеста нет, оба касания уходят игре.
	TestFalse(TEXT("Палец 1 уходит игре"), Processor.HandleMouseButtonDownEvent(SlateApp, DebugMenuGestureTest::Touch(0)));
	Now += 0.03;
	TestFalse(TEXT("Палец 2 уходит игре"), Processor.HandleMouseButtonDownEvent(SlateApp, DebugMenuGestureTest::Touch(1)));
	TestEqual(TEXT("Два пальца меню не открывают"), GestureCount, 0);

	// Третий палец — жест, касание поглощено, его движение и отпускание тоже.
	Now += 0.03;
	TestTrue(TEXT("Палец 3 поглощён"), Processor.HandleMouseButtonDownEvent(SlateApp, DebugMenuGestureTest::Touch(2)));
	TestEqual(TEXT("Три пальца открыли меню ровно один раз"), GestureCount, 1);
	TestTrue(TEXT("Движение пальца 3 поглощено"), Processor.HandleMouseMoveEvent(SlateApp, DebugMenuGestureTest::Touch(2)));
	TestFalse(TEXT("Движение пальца 1 уходит игре"), Processor.HandleMouseMoveEvent(SlateApp, DebugMenuGestureTest::Touch(0)));
	TestTrue(TEXT("Отпускание пальца 3 поглощено"), Processor.HandleMouseButtonUpEvent(SlateApp, DebugMenuGestureTest::Touch(2)));
	TestFalse(TEXT("Отпускание пальца 1 уходит игре"), Processor.HandleMouseButtonUpEvent(SlateApp, DebugMenuGestureTest::Touch(0)));
	TestFalse(TEXT("Отпускание пальца 2 уходит игре"), Processor.HandleMouseButtonUpEvent(SlateApp, DebugMenuGestureTest::Touch(1)));
	TestEqual(TEXT("На экране пальцев нет"), Processor.GetDetector().GetActiveTouchCount(), 0);
	return true;
}

// --- Панель меню: раскладка под палец включается при показе с телефона ---
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugMenuGestureTouchLayoutTest,
	"ContrarySurvivor.DebugMenu.Gesture.PanelTouchLayout", DebugMenuGestureTestFlags)

bool FDebugMenuGestureTouchLayoutTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate не поднят — панель собрать нечем"));
		return false;
	}
	TSharedRef<SContraryDebugMenu> Menu = SNew(SContraryDebugMenu);
	TestFalse(TEXT("Созданная панель закрыта"), Menu->IsOpen());

	Menu->SetOpen(true, /*bInTouchLayout=*/true);
	TestTrue(TEXT("Открыта жестом"), Menu->IsOpen());
	TestTrue(TEXT("Открыта в раскладке под палец"), Menu->IsTouchLayout());
	TestTrue(TEXT("Под палец панель крупнее обычной"), SContraryDebugMenu::TouchLayoutScale >= 1.5f);

	Menu->SetOpen(false);
	TestFalse(TEXT("Закрыта"), Menu->IsOpen());

	Menu->SetOpen(true, /*bInTouchLayout=*/false);
	TestFalse(TEXT("Открыта клавишей на ПК — обычная раскладка"), Menu->IsTouchLayout());
	Menu->SetOpen(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && CONTRARY_WITH_QA_CHEATS
