// Fill out your copyright notice in the Description page of Project Settings.

// Окно-смартфон и приближение карты (слова Рината 10.10.2026: «сам виджет КПК и саму карту
// сделай побольше. Дай игроку возможность приближать и отдалять её как кнопками (+ и −), так и
// пальцами (раздвигая). Сама подложка виджета должна быть похожа на смартфон»).
//
// Проверяются правила вида карты (пределы приближения, приближение к точке, карта не уезжает
// за поле, пересчёт положения значков, щипок), размер смартфона на экране и запасной вид без
// картинок. Живые жесты (пальцы, колесо, мышь) в прогонах не подать: Slate здесь не поднят.
// Проверяются двери, в которые эти жесты входят (ZoomMapAt, PanMapBy, PinchMap), — в игре
// обработчики мыши и пальцев только переводят точку экрана в точку поля и зовут их.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Components/MapExplorationComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/UI/PdaMapMath.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

static constexpr EAutomationTestFlags PdaPhoneTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace PdaPhoneTest
{
	static UWorld* Create()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			return nullptr;
		}
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(World);

		const FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			WorldSettings->NotifyBeginPlay();
		}
		return World;
	}

	static void Destroy(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}

	// Известные настройки на время проверки; прежние возвращаются при выходе.
	struct FScopedPhoneSettings
	{
		UContraryPdaSettings* Settings;
		FVector2D TopLeft, BottomRight;
		float MaxZoom, ZoomStep, HeightFraction, Aspect, SideMargin;
		FMargin Insets;
		TSoftObjectPtr<UTexture2D> PhoneTexture;

		FScopedPhoneSettings()
			: Settings(GetMutableDefault<UContraryPdaSettings>())
		{
			TopLeft = Settings->MapWorldTopLeft;
			BottomRight = Settings->MapWorldBottomRight;
			MaxZoom = Settings->MapMaxZoom;
			ZoomStep = Settings->MapZoomStep;
			HeightFraction = Settings->PhoneHeightFraction;
			Aspect = Settings->PhoneAspectRatio;
			SideMargin = Settings->PhoneSideMargin;
			Insets = Settings->PhoneScreenInsets;
			PhoneTexture = Settings->PhoneTexture;

			// Квадратная карта 40000 на 40000: вверх карты +Y мира, вправо −X (как в игре).
			Settings->MapWorldTopLeft = FVector2D(20000.0, 20000.0);
			Settings->MapWorldBottomRight = FVector2D(-20000.0, -20000.0);
			Settings->MapMaxZoom = 4.0f;
			Settings->MapZoomStep = 1.5f;
			Settings->PhoneHeightFraction = 0.92f;
			Settings->PhoneAspectRatio = 1.5f;
			Settings->PhoneSideMargin = 24.0f;
			Settings->PhoneScreenInsets = FMargin(0.04f, 0.06f, 0.04f, 0.06f);
			Settings->PhoneTexture = nullptr;
		}

		~FScopedPhoneSettings()
		{
			Settings->MapWorldTopLeft = TopLeft;
			Settings->MapWorldBottomRight = BottomRight;
			Settings->MapMaxZoom = MaxZoom;
			Settings->MapZoomStep = ZoomStep;
			Settings->PhoneHeightFraction = HeightFraction;
			Settings->PhoneAspectRatio = Aspect;
			Settings->PhoneSideMargin = SideMargin;
			Settings->PhoneScreenInsets = Insets;
			Settings->PhoneTexture = PhoneTexture;
		}
	};

	static bool Near(const FVector2D& A, const FVector2D& B, double Tolerance = 0.01)
	{
		return A.Equals(B, Tolerance);
	}
}

// ===========================================================================
// 1. Правила вида карты: пределы, приближение к точке, края поля, значки, щипок.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaMapViewRulesTest,
	"ContrarySurvivor.Pda.Phone.MapZoomAndPanRules", PdaPhoneTestFlags)

bool FPdaMapViewRulesTest::RunTest(const FString& Parameters)
{
	using PdaPhoneTest::Near;
	// Поле 1200 на 800, карта квадратная: при приближении 1 она 800 на 800 и стоит по центру.
	const FVector2D Viewport(1200.0, 800.0), Base(800.0, 800.0);
	const double MaxZoom = 4.0;

	PdaMap::FMapView View;
	TestTrue(TEXT("Карта целиком: по ширине стоит по центру поля"), Near(View.GetContentOffset(Viewport, Base), FVector2D(200.0, 0.0)));
	TestTrue(TEXT("Карта целиком: размер равен вписанному"), Near(View.GetContentSize(Base), Base));
	TestTrue(TEXT("Середина карты — в середине поля"), Near(View.MapToViewport(FVector2D(0.5, 0.5), Viewport, Base), FVector2D(600.0, 400.0)));
	TestTrue(TEXT("Левый-верхний угол карты — на левом краю вписанной карты"), Near(View.MapToViewport(FVector2D(0.0, 0.0), Viewport, Base), FVector2D(200.0, 0.0)));

	// Карту целиком сдвинуть нельзя: она меньше поля по ширине и ровно с поле по высоте.
	View.PanBy(FVector2D(300.0, -200.0), Viewport, Base);
	TestTrue(TEXT("Карта целиком не сдвигается"), Near(View.GetContentOffset(Viewport, Base), FVector2D(200.0, 0.0)));

	// --- Пределы приближения ---
	TestEqual(TEXT("Приближение меньше единицы не бывает"), PdaMap::FMapView::ClampZoom(0.3, MaxZoom), 1.0);
	TestEqual(TEXT("Приближение больше наибольшего не бывает"), PdaMap::FMapView::ClampZoom(9.0, MaxZoom), 4.0);
	TestEqual(TEXT("Наибольшее приближение меньше единицы читается как единица"), PdaMap::FMapView::ClampZoom(3.0, 0.5), 1.0);
	View.ZoomAt(FVector2D(600.0, 400.0), 100.0, MaxZoom, Viewport, Base);
	TestEqual(TEXT("Приближение к точке упирается в наибольшее"), View.Zoom, 4.0);
	View.ZoomAt(FVector2D(600.0, 400.0), 0.01, MaxZoom, Viewport, Base);
	TestEqual(TEXT("Отдаление упирается в «карту целиком»"), View.Zoom, 1.0);
	TestTrue(TEXT("После полного отдаления карта снова по центру"), Near(View.GetContentOffset(Viewport, Base), FVector2D(200.0, 0.0)));

	// --- Приближение к точке: место карты под курсором остаётся под курсором ---
	View = PdaMap::FMapView();
	const FVector2D Cursor(700.0, 300.0);
	const FVector2D UnderCursor = View.ViewportToMap(Cursor, Viewport, Base);
	TestTrue(TEXT("Под курсором точка карты (0,625; 0,375)"), Near(UnderCursor, FVector2D(0.625, 0.375), 0.0001));
	View.ZoomAt(Cursor, 2.0, MaxZoom, Viewport, Base);
	TestEqual(TEXT("Приближение стало 2"), View.Zoom, 2.0);
	TestTrue(TEXT("После приближения та же точка карты под курсором"), Near(View.MapToViewport(UnderCursor, Viewport, Base), Cursor));
	View.ZoomAt(Cursor, 3.0, MaxZoom, Viewport, Base);
	TestTrue(TEXT("После второго приближения она всё ещё под курсором"), Near(View.MapToViewport(UnderCursor, Viewport, Base), Cursor));

	// --- Значки: расходятся вместе с картой, расстояние между ними растёт в те же разы ---
	PdaMap::FMapView Whole, Twice;
	Twice.ZoomAt(FVector2D(600.0, 400.0), 2.0, MaxZoom, Viewport, Base);
	const FVector2D MarkA(0.40, 0.50), MarkB(0.55, 0.60);
	const double DistWhole = FVector2D::Distance(Whole.MapToViewport(MarkA, Viewport, Base), Whole.MapToViewport(MarkB, Viewport, Base));
	const double DistTwice = FVector2D::Distance(Twice.MapToViewport(MarkA, Viewport, Base), Twice.MapToViewport(MarkB, Viewport, Base));
	TestTrue(*FString::Printf(TEXT("При приближении 2 значки вдвое дальше друг от друга (%.1f и %.1f)"), DistWhole, DistTwice),
		FMath::IsNearlyEqual(DistTwice, DistWhole * 2.0, 0.01));
	TestTrue(TEXT("Значок в точке (0,4; 0,5) при приближении 2 стоит в (440, 400)"),
		Near(Twice.MapToViewport(MarkA, Viewport, Base), FVector2D(440.0, 400.0)));

	// --- Карта не уезжает за поле ---
	View = PdaMap::FMapView();
	View.ZoomAt(FVector2D(600.0, 400.0), 4.0, MaxZoom, Viewport, Base); // карта 3200 на 3200
	auto CheckInside = [&](const TCHAR* What)
	{
		const FVector2D Offset = View.GetContentOffset(Viewport, Base);
		const FVector2D Size = View.GetContentSize(Base);
		TestTrue(*FString::Printf(TEXT("%s: левый и верхний края карты не заходят в поле (%.1f, %.1f)"), What, Offset.X, Offset.Y),
			Offset.X <= 0.01 && Offset.Y <= 0.01);
		TestTrue(*FString::Printf(TEXT("%s: правый и нижний края карты не заходят в поле"), What),
			Offset.X + Size.X >= Viewport.X - 0.01 && Offset.Y + Size.Y >= Viewport.Y - 0.01);
	};
	View.PanBy(FVector2D(100000.0, 100000.0), Viewport, Base);
	CheckInside(TEXT("Сдвиг до упора вправо-вниз"));
	TestTrue(TEXT("Карта упёрлась левым-верхним углом"), Near(View.GetContentOffset(Viewport, Base), FVector2D(0.0, 0.0)));
	View.PanBy(FVector2D(-100000.0, -100000.0), Viewport, Base);
	CheckInside(TEXT("Сдвиг до упора влево-вверх"));
	TestTrue(TEXT("Карта упёрлась правым-нижним углом"), Near(View.GetContentOffset(Viewport, Base), FVector2D(1200.0 - 3200.0, 800.0 - 3200.0)));
	// От упора карта отлипает сразу, а не «съедает» накопленный лишний сдвиг.
	View.PanBy(FVector2D(50.0, 30.0), Viewport, Base);
	TestTrue(TEXT("Обратный сдвиг от упора действует сразу"), Near(View.GetContentOffset(Viewport, Base), FVector2D(-2000.0 + 50.0, -2400.0 + 30.0)));
	// Приближение у самого угла тоже не вытаскивает карту из поля.
	View = PdaMap::FMapView();
	View.ZoomAt(FVector2D(0.0, 0.0), 3.0, MaxZoom, Viewport, Base);
	CheckInside(TEXT("Приближение у угла поля"));
	// Обычный сдвиг приближенной карты — ровно на столько, на сколько повели.
	View = PdaMap::FMapView();
	View.ZoomAt(FVector2D(600.0, 400.0), 2.0, MaxZoom, Viewport, Base);
	const FVector2D BeforePan = View.GetContentOffset(Viewport, Base);
	View.PanBy(FVector2D(-120.0, 80.0), Viewport, Base);
	TestTrue(TEXT("Сдвиг приближенной карты — ровно на движение пальца"), Near(View.GetContentOffset(Viewport, Base), BeforePan + FVector2D(-120.0, 80.0)));

	// --- Кнопка «найти героя»: точка карты встаёт в середину поля ---
	View.CenterOn(FVector2D(0.5, 0.4), Viewport, Base);
	TestTrue(TEXT("Точка карты поставлена в середину поля"), Near(View.MapToViewport(FVector2D(0.5, 0.4), Viewport, Base), FVector2D(600.0, 400.0)));
	View.CenterOn(FVector2D(0.0, 0.0), Viewport, Base);
	CheckInside(TEXT("Центр на угол карты"));

	// --- Щипок: пальцы разошлись вдвое — карта вдвое крупнее и едет за серединой между ними ---
	View = PdaMap::FMapView();
	const FVector2D OldA(500.0, 400.0), OldB(700.0, 400.0); // середина (600, 400), расстояние 200
	const FVector2D NewA(420.0, 420.0), NewB(820.0, 420.0); // середина (620, 420), расстояние 400
	const FVector2D UnderFingers = View.ViewportToMap(FVector2D(600.0, 400.0), Viewport, Base);
	View.Pinch(OldA, OldB, NewA, NewB, MaxZoom, Viewport, Base);
	TestEqual(TEXT("Щипок: приближение выросло вдвое"), View.Zoom, 2.0);
	TestTrue(TEXT("Щипок: место карты между пальцами осталось между пальцами"),
		Near(View.MapToViewport(UnderFingers, Viewport, Base), FVector2D(620.0, 420.0)));
	View.Pinch(NewA, NewB, FVector2D(100.0, 420.0), FVector2D(1140.0, 420.0), MaxZoom, Viewport, Base);
	TestEqual(TEXT("Щипок не приближает сильнее наибольшего"), View.Zoom, 4.0);
	View.Pinch(FVector2D(100.0, 400.0), FVector2D(1100.0, 400.0), FVector2D(590.0, 400.0), FVector2D(610.0, 400.0), MaxZoom, Viewport, Base);
	TestEqual(TEXT("Сведённые пальцы возвращают карту целиком, не меньше"), View.Zoom, 1.0);

	// --- Размер смартфона на экране ---
	TestTrue(TEXT("Экран 16:9: смартфон 1490 на 994"),
		Near(PdaMap::ComputePhoneSize(FVector2D(1920.0, 1080.0), 0.92, 1.5, 24.0), FVector2D(1490.4, 993.6)));
	TestTrue(TEXT("Экран 20:9: тот же размер (высота экрана та же)"),
		Near(PdaMap::ComputePhoneSize(FVector2D(2400.0, 1080.0), 0.92, 1.5, 24.0), FVector2D(1490.4, 993.6)));
	TestTrue(TEXT("Узкий экран: смартфон уменьшается целиком, поля остаются"),
		Near(PdaMap::ComputePhoneSize(FVector2D(1200.0, 1080.0), 0.92, 1.5, 24.0), FVector2D(1152.0, 768.0)));
	const FMargin Padding = PdaMap::ComputePhoneScreenPadding(FVector2D(1500.0, 1000.0), FMargin(0.04f, 0.06f, 0.05f, 0.07f));
	TestTrue(TEXT("Отступы экрана: доли картинки переведены в единицы раскладки"),
		FMath::IsNearlyEqual(Padding.Left, 60.0f, 0.01f) && FMath::IsNearlyEqual(Padding.Top, 60.0f, 0.01f)
		&& FMath::IsNearlyEqual(Padding.Right, 75.0f, 0.01f) && FMath::IsNearlyEqual(Padding.Bottom, 70.0f, 0.01f));
	return true;
}

// ===========================================================================
// 2. Окно: приближение кнопками и к точке, сдвиг, положение героя, запасной вид без картинок.
//    Гоняется на обоих деревьях: построенном кодом и взятом из готового файла окна.
// ===========================================================================
namespace PdaPhoneTest
{
	static void RunWindowChecks(FAutomationTestBase& Test, UWorld* World, UClass* WidgetClass, const TCHAR* Path)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Герой в мире (−5000, 4000): на карте это точка (0,625; 0,4).
		ASaveTestPlayerCharacter* Player = World->SpawnActor<ASaveTestPlayerCharacter>(
			ASaveTestPlayerCharacter::StaticClass(), FVector(-5000.0, 4000.0, 100.0), FRotator::ZeroRotator, Params);
		if (!Player)
		{
			Test.AddError(FString::Printf(TEXT("%s: герой не заспавнен"), Path));
			return;
		}
		Player->UseTestSaveSlot(TEXT("ContraryPdaPhoneQA"));

		UPdaScreenWidget* Window = CreateWidget<UPdaScreenWidget>(World, WidgetClass);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: окно создалось"), Path), Window))
		{
			return;
		}
		auto Name = [Path](const TCHAR* What) { return FString::Printf(TEXT("%s: %s"), Path, What); };

		Window->InitPda(Player);
		Window->ShowTab(true);
		// Поле карты как на экране 1920 на 1080: 1135 на 874.
		const FVector2D Viewport(1135.0, 874.0);
		Window->SetMapViewportSize(Viewport);

		// --- Карта целиком ---
		Test.TestEqual(*Name(TEXT("окно открывается с картой целиком")), Window->GetMapZoom(), 1.0);
		Test.TestTrue(*Name(TEXT("карта вписана в поле по высоте")), Near(Window->GetMapBaseSize(), FVector2D(874.0, 874.0)));
		Test.TestTrue(*Name(TEXT("карта целиком стоит по центру поля")), Near(Window->GetMapAreaOffset(), FVector2D(130.5, 0.0)));
		Test.TestTrue(*Name(TEXT("герой на карте целиком — в точке (676,75; 349,6)")),
			Near(Window->WorldToMapViewport(Player->GetActorLocation()), FVector2D(130.5 + 0.625 * 874.0, 0.4 * 874.0)));

		// --- Кнопки «+» и «−»: к середине поля, с упором в пределы ---
		const FVector2D Middle = Viewport * 0.5;
		const FVector2D UnderMiddle = Window->GetMapView().ViewportToMap(Middle, Viewport, Window->GetMapBaseSize());
		Window->ZoomMapBySteps(1);
		Test.TestEqual(*Name(TEXT("«+» приближает в полтора раза")), Window->GetMapZoom(), 1.5);
		Test.TestTrue(*Name(TEXT("«+» приближает к середине поля")),
			Near(Window->GetMapView().MapToViewport(UnderMiddle, Viewport, Window->GetMapBaseSize()), Middle));
		Test.TestTrue(*Name(TEXT("размер карты вырос в полтора раза")), Near(Window->GetMapAreaSize(), FVector2D(1311.0, 1311.0)));
		Window->ZoomMapBySteps(10);
		Test.TestEqual(*Name(TEXT("«+» упирается в наибольшее приближение 4")), Window->GetMapZoom(), 4.0);
		Window->ZoomMapBySteps(-20);
		Test.TestEqual(*Name(TEXT("«−» упирается в карту целиком")), Window->GetMapZoom(), 1.0);

		// --- Приближение к точке (колесо мыши над точкой, пальцы) ---
		const FVector2D HeroBefore = Window->WorldToMapViewport(Player->GetActorLocation());
		Window->ZoomMapAt(HeroBefore, 3.0);
		Test.TestEqual(*Name(TEXT("приближение к точке стало 3")), Window->GetMapZoom(), 3.0);
		Test.TestTrue(*Name(TEXT("герой остался под той же точкой поля")),
			Near(Window->WorldToMapViewport(Player->GetActorLocation()), HeroBefore));

		// --- Сдвиг и края поля ---
		const FVector2D BeforePan = Window->GetMapAreaOffset();
		Window->PanMapBy(FVector2D(-60.0, 40.0));
		Test.TestTrue(*Name(TEXT("сдвиг двигает карту на движение пальца")), Near(Window->GetMapAreaOffset(), BeforePan + FVector2D(-60.0, 40.0)));
		Test.TestTrue(*Name(TEXT("значок героя уехал вместе с картой")),
			Near(Window->WorldToMapViewport(Player->GetActorLocation()), HeroBefore + FVector2D(-60.0, 40.0)));
		Window->PanMapBy(FVector2D(50000.0, 50000.0));
		Test.TestTrue(*Name(TEXT("сдвиг до упора: карта не уходит за левый-верхний угол поля")), Near(Window->GetMapAreaOffset(), FVector2D(0.0, 0.0)));
		Window->PanMapBy(FVector2D(-50000.0, -50000.0));
		Test.TestTrue(*Name(TEXT("сдвиг до упора: карта не уходит за правый-нижний угол поля")),
			Near(Window->GetMapAreaOffset() + Window->GetMapAreaSize(), Viewport));

		// --- Щипок ---
		Window->ZoomMapBySteps(-20);
		Window->PinchMap(FVector2D(500.0, 400.0), FVector2D(600.0, 400.0), FVector2D(450.0, 400.0), FVector2D(650.0, 400.0));
		Test.TestEqual(*Name(TEXT("щипок вдвое приближает вдвое")), Window->GetMapZoom(), 2.0);

		// --- «Найти героя» и повторное открытие ---
		Window->ZoomMapAt(Middle, 4.0);
		Window->PanMapBy(FVector2D(50000.0, 50000.0));
		Window->CenterMapOnHero();
		Test.TestTrue(*Name(TEXT("«найти героя» ставит героя в середину поля")),
			Near(Window->WorldToMapViewport(Player->GetActorLocation()), Middle));
		Window->PanMapBy(FVector2D(300.0, 300.0));
		Window->InitPda(Player); // окно закрыли и открыли снова
		Test.TestEqual(*Name(TEXT("приближение пережило повторное открытие")), Window->GetMapZoom(), 4.0);
		Test.TestTrue(*Name(TEXT("при открытии герой снова в середине поля")),
			Near(Window->WorldToMapViewport(Player->GetActorLocation()), Middle));

		// Поле сменило размер (другой экран) — карта остаётся в поле.
		Window->SetMapViewportSize(FVector2D(900.0, 700.0));
		const FVector2D Offset = Window->GetMapAreaOffset();
		const FVector2D Size = Window->GetMapAreaSize();
		Test.TestTrue(*Name(TEXT("после смены размера поля карта его закрывает")),
			Offset.X <= 0.01 && Offset.Y <= 0.01 && Offset.X + Size.X >= 900.0 - 0.01 && Offset.Y + Size.Y >= 700.0 - 0.01);

		// --- Смартфон без картинки: вид — тёмная плашка. С 21.10.2026 (Ринат: «в WBP_Pda я должен
		// иметь возможность свободно двигать элементы») размер и отступы подложки — как в окне,
		// под экран смартфон масштабируется целиком: на экране 1920 на 1080 подложка, вписанная
		// в 92 % высоты, остаётся своего размера ---
		const UBorder* PlateWidget = Window->WidgetTree ? Cast<UBorder>(Window->WidgetTree->FindWidget(TEXT("PanelPlate"))) : nullptr;
		const UCanvasPanelSlot* PlateSlot = PlateWidget ? Cast<UCanvasPanelSlot>(PlateWidget->Slot) : nullptr;
		const FVector2D PlateInWindow = PlateSlot ? PlateSlot->GetSize() : FVector2D::ZeroVector;
		const FMargin PadInWindow = PlateWidget ? PlateWidget->GetPadding() : FMargin(0.0f);
		Window->ApplyPhoneLayout(FVector2D(1920.0, 1080.0));
		Test.TestFalse(*Name(TEXT("картинки смартфона нет — запасной вид")), Window->IsPhoneTextureShown());
		const FVector2D PhoneAtDesign = Window->GetPhoneSize();
		Test.TestTrue(*Name(*FString::Printf(TEXT("на экране 1920 на 1080 смартфон — подложка из окна (%.1f на %.1f), высотой 994"),
			PlateInWindow.X, PlateInWindow.Y)), Near(PhoneAtDesign, PlateInWindow) && FMath::IsNearlyEqual(PhoneAtDesign.Y, 993.6, 0.1));
		const FMargin Padding = Window->GetPhoneScreenPadding();
		Test.TestTrue(*Name(TEXT("отступы экрана смартфона — как у подложки в окне")),
			FMath::IsNearlyEqual(Padding.Left, PadInWindow.Left, 0.01f) && FMath::IsNearlyEqual(Padding.Top, PadInWindow.Top, 0.01f)
			&& FMath::IsNearlyEqual(Padding.Right, PadInWindow.Right, 0.01f) && FMath::IsNearlyEqual(Padding.Bottom, PadInWindow.Bottom, 0.01f));
		// Экран вдвое ниже — смартфон целиком вдвое меньше, расстановка внутри та же.
		Window->ApplyPhoneLayout(FVector2D(1920.0, 540.0));
		Test.TestTrue(*Name(TEXT("на экране 1920 на 540 смартфон уменьшен целиком вдвое")),
			Near(Window->GetPhoneSize(), PhoneAtDesign * 0.5) && FMath::IsNearlyEqual(Window->GetPhoneScale(), 0.5, 0.001)
			&& PlateSlot && Near(PlateSlot->GetSize(), PlateInWindow));
		Window->ApplyPhoneLayout(FVector2D(1920.0, 1080.0));

		// --- Смартфон с картинкой: картинка подложена, размер подложки — как в окне (другие
		// пропорции картинки владелец подгоняет размером подложки сам) ---
		UContraryPdaSettings* Settings = GetMutableDefault<UContraryPdaSettings>();
		UTexture2D* Phone = UTexture2D::CreateTransient(400, 200, PF_B8G8R8A8);
		if (Test.TestNotNull(*Name(TEXT("проверочная картинка смартфона создана")), Phone))
		{
			Phone->AddToRoot();
			Settings->PhoneTexture = Phone;
			Window->ApplyPhoneLayout(FVector2D(1920.0, 1080.0));
			Test.TestTrue(*Name(TEXT("картинка смартфона подложена")), Window->IsPhoneTextureShown());
			Test.TestTrue(*Name(TEXT("с картинкой 2:1 смартфон того же размера, что подложка в окне")), Near(Window->GetPhoneSize(), PhoneAtDesign));
			Settings->PhoneTexture = nullptr;
			Window->ApplyPhoneLayout(FVector2D(1920.0, 1080.0));
			Test.TestFalse(*Name(TEXT("картинку убрали — снова запасной вид")), Window->IsPhoneTextureShown());
			Phone->RemoveFromRoot();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaPhoneWindowTest,
	"ContrarySurvivor.Pda.Phone.WindowZoomsPansAndFallsBackWithoutPictures", PdaPhoneTestFlags)

bool FPdaPhoneWindowTest::RunTest(const FString& Parameters)
{
	PdaPhoneTest::FScopedPhoneSettings Scoped;

	// Дерево, построенное кодом.
	{
		UWorld* World = PdaPhoneTest::Create();
		if (!TestNotNull(TEXT("Тестовый мир создан"), World))
		{
			return false;
		}
		PdaPhoneTest::RunWindowChecks(*this, World, UPdaScreenWidget::StaticClass(), TEXT("Окно из кода"));
		PdaPhoneTest::Destroy(World);
	}

	// Готовый файл окна.
	{
		UClass* AssetClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
		UWorld* World = PdaPhoneTest::Create();
		if (TestNotNull(TEXT("Готовый файл окна загружен"), AssetClass) && TestNotNull(TEXT("Тестовый мир создан"), World))
		{
			PdaPhoneTest::RunWindowChecks(*this, World, AssetClass, TEXT("Окно из готового файла"));
		}
		PdaPhoneTest::Destroy(World);
	}


	PdaPhoneTest::DeleteSlot(TEXT("ContraryPdaPhoneQA"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
