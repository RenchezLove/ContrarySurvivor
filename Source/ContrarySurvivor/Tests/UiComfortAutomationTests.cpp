// Fill out your copyright notice in the Description page of Project Settings.

// Удобство экранных кнопок и обозначение слотов оружия (слова Рината 11.10.2026: «У нас появились
// новые кнопки (смартфон и атака ножом)… сейчас они маленькие и частично перекрывают другие
// кнопки. Игроку должно быть удобно их нажимать» и «игрок открывая инвентарь не совсем понимает
// где находятся слоты под оружие и есть ли они вообще. Обозначь их»).
//
// Проверяются готовые файлы окон, как они лежат в проекте: прямоугольники всех экранных кнопок
// на экранах 1920 и 2400 единиц в ширину при высоте 1080 (экран в единицах раскладки всегда
// 1080 в высоту), строка отслеживаемого задания и рамки слотов оружия в окне сумки. Живой вид
// в игре прогон не показывает — его смотрит Ринат.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "ContrarySurvivor/UI/TouchControlsWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

static constexpr EAutomationTestFlags UiComfortTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace UiComfortTest
{
	// Дерево готового файла окна, как оно записано (без создания окна и без мира).
	static UWidgetTree* LoadTree(const TCHAR* ClassPath)
	{
		const UWidgetBlueprintGeneratedClass* Class = Cast<UWidgetBlueprintGeneratedClass>(
			StaticLoadClass(UUserWidget::StaticClass(), nullptr, ClassPath));
		return Class ? Class->GetWidgetTreeArchetype() : nullptr;
	}

	static const UCanvasPanelSlot* PointSlot(const UWidget* Widget)
	{
		const UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		return CanvasSlot && !CanvasSlot->GetAutoSize()
			&& CanvasSlot->GetAnchors().Minimum == CanvasSlot->GetAnchors().Maximum ? CanvasSlot : nullptr;
	}

	// Прямоугольник элемента на экране (или в родительской канве) размера Area.
	static FBox2D BoxOn(const UCanvasPanelSlot* CanvasSlot, const FVector2D& Area, double Scale = 1.0)
	{
		const FVector2D Size = CanvasSlot->GetSize();
		const FVector2D Center = FVector2D(CanvasSlot->GetAnchors().Minimum) * Area
			+ CanvasSlot->GetPosition() + Size * (FVector2D(0.5, 0.5) - CanvasSlot->GetAlignment());
		return FBox2D(Center - Size * 0.5 * Scale, Center + Size * 0.5 * Scale);
	}

	struct FNamedBox
	{
		FString Name;
		FBox2D Box;
	};
}

// ===========================================================================
// Ни одна экранная кнопка не перекрывает другую и не выходит за экран на 16:9 и 20:9; кнопки
// ножа и смартфона не меньше кнопки перезарядки; кнопки не заходят на джойстик; строка задания
// стоит ниже кнопок сумки и смартфона.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiComfortTouchButtonsDoNotOverlapTest,
	"ContrarySurvivor.UiComfort.TouchButtonsDoNotOverlap", UiComfortTestFlags)

bool FUiComfortTouchButtonsDoNotOverlapTest::RunTest(const FString& Parameters)
{
	using namespace UiComfortTest;

	const UWidgetTree* Tree = LoadTree(TEXT("/Game/UI/WBP_TouchControls.WBP_TouchControls_C"));
	const UCanvasPanel* Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	if (!TestNotNull(TEXT("Готовый файл экранных кнопок загружен, корень — канва"), Root))
	{
		return false;
	}

	const UCanvasPanelSlot* BagSlot = PointSlot(Tree->FindWidget(TEXT("InventoryButton")));
	const UCanvasPanelSlot* ReloadSlot = PointSlot(Tree->FindWidget(TEXT("ReloadButton")));
	const UCanvasPanelSlot* MeleeSlot = PointSlot(Tree->FindWidget(TEXT("MeleeButton")));
	const UCanvasPanelSlot* FireSlot = PointSlot(Tree->FindWidget(TEXT("FireButton")));
	const UCanvasPanelSlot* StickSlot = PointSlot(Tree->FindWidget(TEXT("StickBase")));
	if (!TestNotNull(TEXT("Кнопка сумки"), BagSlot) || !TestNotNull(TEXT("Кнопка перезарядки"), ReloadSlot)
		|| !TestNotNull(TEXT("Кнопка ножа"), MeleeSlot) || !TestNotNull(TEXT("Кнопка огня"), FireSlot)
		|| !TestNotNull(TEXT("Подложка джойстика"), StickSlot))
	{
		return false;
	}

	// Кнопка смартфона: место считается так же, как его считает контроллер при создании слоя.
	const AContrarySurvivorPlayerController* Defaults = GetDefault<AContrarySurvivorPlayerController>();
	const double PdaRadius = Defaults->GetPdaTouchButtonRadius();
	const double BagScale = GetDefault<UTouchControlsWidget>()->GetInventoryBlinkScale();
	const FVector2D BagSize = BagSlot->GetSize();
	const FVector2D BagCenter = BagSlot->GetPosition() + BagSize * (FVector2D(0.5, 0.5) - BagSlot->GetAlignment());
	const FVector2D PdaCenter = UPdaTouchButtonWidget::ComputeCenterBeside(BagCenter, BagSize,
		BagScale, PdaRadius, Defaults->GetPdaTouchButtonGap(), BagSlot->GetAnchors().Minimum.X >= 0.5);

	// Размеры: нож и смартфон не меньше кнопки перезарядки.
	const FVector2D ReloadSize = ReloadSlot->GetSize();
	const FVector2D MeleeSize = MeleeSlot->GetSize();
	TestTrue(*FString::Printf(TEXT("Кнопка ножа (%.0f на %.0f) не меньше кнопки перезарядки (%.0f на %.0f)"),
		MeleeSize.X, MeleeSize.Y, ReloadSize.X, ReloadSize.Y),
		MeleeSize.X >= ReloadSize.X && MeleeSize.Y >= ReloadSize.Y);
	TestTrue(*FString::Printf(TEXT("Кнопка смартфона (поперечник %.0f) не меньше кнопки перезарядки (%.0f на %.0f)"),
		PdaRadius * 2.0, ReloadSize.X, ReloadSize.Y),
		PdaRadius * 2.0 >= ReloadSize.X && PdaRadius * 2.0 >= ReloadSize.Y);

	// Нож — под большим пальцем правой руки: в том же углу, что огонь, и не дальше полутора
	// поперечников кнопки огня от её центра.
	TestTrue(TEXT("Кнопка ножа привязана к тому же углу, что кнопка огня"),
		MeleeSlot->GetAnchors().Minimum == FireSlot->GetAnchors().Minimum);
	const FVector2D ProbeScreen(1920.0, 1080.0);
	const double MeleeToFire = FVector2D::Distance(BoxOn(MeleeSlot, ProbeScreen).GetCenter(), BoxOn(FireSlot, ProbeScreen).GetCenter());
	TestTrue(*FString::Printf(TEXT("Кнопка ножа рядом с кнопкой огня (между центрами %.0f)"), MeleeToFire),
		MeleeToFire <= FireSlot->GetSize().GetMax() * 1.5);

	for (const double ScreenWidth : { 1920.0, 2400.0 })
	{
		const FVector2D Screen(ScreenWidth, 1080.0);
		TArray<FNamedBox> Boxes;
		for (int32 Index = 0; Index < Root->GetChildrenCount(); ++Index)
		{
			const UWidget* Child = Root->GetChildAt(Index);
			const UCanvasPanelSlot* ChildSlot = PointSlot(Child);
			if (ChildSlot && Child->IsA<UButton>())
			{
				Boxes.Add({ Child->GetName(), BoxOn(ChildSlot, Screen) });
			}
		}
		const FVector2D PdaOnScreen = FVector2D(BagSlot->GetAnchors().Minimum) * Screen + PdaCenter;
		const FBox2D PdaBox(PdaOnScreen - FVector2D(PdaRadius), PdaOnScreen + FVector2D(PdaRadius));
		Boxes.Add({ TEXT("кнопка смартфона"), PdaBox });
		TestTrue(*FString::Printf(TEXT("На экране шириной %.0f кнопок не меньше девяти (нашлось %d)"), ScreenWidth, Boxes.Num()),
			Boxes.Num() >= 9);

		const FBox2D StickBox = BoxOn(StickSlot, Screen);
		for (int32 A = 0; A < Boxes.Num(); ++A)
		{
			const FBox2D& Box = Boxes[A].Box;
			TestTrue(*FString::Printf(TEXT("%s целиком на экране шириной %.0f: (%.0f, %.0f) — (%.0f, %.0f)"),
				*Boxes[A].Name, ScreenWidth, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y),
				Box.Min.X >= 0.0 && Box.Min.Y >= 0.0 && Box.Max.X <= Screen.X && Box.Max.Y <= Screen.Y);
			TestFalse(*FString::Printf(TEXT("%s не заходит на джойстик на экране шириной %.0f"), *Boxes[A].Name, ScreenWidth),
				Box.Intersect(StickBox));
			for (int32 B = A + 1; B < Boxes.Num(); ++B)
			{
				TestFalse(*FString::Printf(TEXT("%s не перекрывает %s на экране шириной %.0f"),
					*Boxes[A].Name, *Boxes[B].Name, ScreenWidth), Box.Intersect(Boxes[B].Box));
			}
			AddInfo(FString::Printf(TEXT("Экран %.0f: %s — (%.0f, %.0f) — (%.0f, %.0f)"),
				ScreenWidth, *Boxes[A].Name, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y));
		}

		// Сумка в наибольшем размере (мигание) не задевает кнопку смартфона.
		TestFalse(*FString::Printf(TEXT("Мигающая кнопка сумки не задевает кнопку смартфона на экране шириной %.0f"), ScreenWidth),
			BoxOn(BagSlot, Screen, BagScale).Intersect(PdaBox));

		// Строка отслеживаемого задания: её верх ниже сумки в наибольшем размере и ниже
		// кнопки смартфона, поэтому сколько бы строк в ней ни было, кнопок она не коснётся.
		const UWidgetTree* TrackerTree = LoadTree(TEXT("/Game/UI/WBP_QuestTracker.WBP_QuestTracker_C"));
		const UWidget* Plate = TrackerTree ? TrackerTree->FindWidget(TEXT("TrackerPlate")) : nullptr;
		const UCanvasPanelSlot* PlateSlot = Plate ? Cast<UCanvasPanelSlot>(Plate->Slot) : nullptr;
		if (TestNotNull(TEXT("Плашка строки задания"), PlateSlot))
		{
			TestTrue(TEXT("Строка задания привязана к тому же углу, что кнопка сумки, и растёт вниз"),
				PlateSlot->GetAnchors().Minimum == BagSlot->GetAnchors().Minimum && PlateSlot->GetAlignment().Y == 0.0);
			const double PlateTop = PlateSlot->GetAnchors().Minimum.Y * Screen.Y + PlateSlot->GetPosition().Y;
			const double ButtonsBottom = FMath::Max(BoxOn(BagSlot, Screen, BagScale).Max.Y, PdaBox.Max.Y);
			TestTrue(*FString::Printf(TEXT("Верх строки задания (%.0f) ниже кнопок сумки и смартфона (%.0f)"), PlateTop, ButtonsBottom),
				PlateTop > ButtonsBottom);
			// Ниже строки в правой колонке стоит кнопка смены оружия — две строки до неё не достают.
			if (const UCanvasPanelSlot* WeaponSlot = PointSlot(Tree->FindWidget(TEXT("WeaponButton"))))
			{
				const double TwoLinesBottom = PlateTop + 2.0 * 30.0 + 8.0;
				TestTrue(*FString::Printf(TEXT("Две строки задания (низ около %.0f) не достают до кнопки смены оружия (верх %.0f)"),
					TwoLinesBottom, BoxOn(WeaponSlot, Screen).Min.Y), TwoLinesBottom < BoxOn(WeaponSlot, Screen).Min.Y);
			}
		}
	}
	return true;
}

// ===========================================================================
// Слоты оружия в окне сумки обозначены всегда: под значком каждого слота стоит видимая рамка,
// название слота стоит в ширину своей колонки и не заходит ни на рюкзак, ни на соседний слот,
// подложка окна целиком на экране 16:9.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiComfortWeaponSlotsMarkedTest,
	"ContrarySurvivor.UiComfort.InventoryWeaponSlotsAreMarked", UiComfortTestFlags)

bool FUiComfortWeaponSlotsMarkedTest::RunTest(const FString& Parameters)
{
	using namespace UiComfortTest;

	const UWidgetTree* Tree = LoadTree(TEXT("/Game/UI/WBP_Inventory.WBP_Inventory_C"));
	if (!TestNotNull(TEXT("Готовый файл окна сумки загружен"), Tree))
	{
		return false;
	}

	struct FSlotNames { const TCHAR* Title; const TCHAR* Icon; const TCHAR* Text; const TCHAR* Frame; };
	static const FSlotNames Slots[] =
	{
		{ TEXT("пистолета"), TEXT("RangedSlotIcon"), TEXT("RangedSlotText"), TEXT("RangedSlotFrame") },
		{ TEXT("длинного оружия"), TEXT("LongSlotIcon"), TEXT("LongSlotText"), TEXT("LongSlotFrame") },
		{ TEXT("ножа"), TEXT("MeleeSlotIcon"), TEXT("MeleeSlotText"), TEXT("MeleeSlotFrame") },
	};

	const UWidget* Backpack = Tree->FindWidget(TEXT("BackpackList"));
	const UCanvasPanelSlot* BackpackSlot = Backpack ? Cast<UCanvasPanelSlot>(Backpack->Slot) : nullptr;
	const UBorder* Plate = Cast<UBorder>(Tree->FindWidget(TEXT("PanelPlate")));
	const UCanvasPanelSlot* PlateSlot = PointSlot(Plate);
	if (!TestNotNull(TEXT("Подложка окна сумки"), PlateSlot) || !TestNotNull(TEXT("Список рюкзака"), BackpackSlot))
	{
		return false;
	}
	const FMargin Pad = Plate->GetPadding();
	const FVector2D Canvas(PlateSlot->GetSize().X - Pad.Left - Pad.Right, PlateSlot->GetSize().Y - Pad.Top - Pad.Bottom);
	const double BackpackLeft = BackpackSlot->GetAnchors().Minimum.X * Canvas.X + BackpackSlot->GetOffsets().Left;

	double PrevTextBottom = 0.0;
	for (const FSlotNames& Names : Slots)
	{
		const UWidget* Icon = Tree->FindWidget(Names.Icon);
		const UImage* Frame = Cast<UImage>(Tree->FindWidget(Names.Frame));
		const UTextBlock* Text = Cast<UTextBlock>(Tree->FindWidget(Names.Text));
		const UCanvasPanelSlot* IconSlot = PointSlot(Icon);
		const UCanvasPanelSlot* FrameSlot = PointSlot(Frame);
		const UCanvasPanelSlot* TextSlot = PointSlot(Text);
		if (!TestNotNull(*FString::Printf(TEXT("Значок слота %s"), Names.Title), IconSlot)
			|| !TestNotNull(*FString::Printf(TEXT("Рамка слота %s"), Names.Title), FrameSlot)
			|| !TestNotNull(*FString::Printf(TEXT("Название слота %s стоит в своём размере, а не по длине текста"), Names.Title), TextSlot))
		{
			continue;
		}
		const FBox2D IconBox = BoxOn(IconSlot, Canvas);
		const FBox2D FrameBox = BoxOn(FrameSlot, Canvas);
		const FBox2D TextBox = BoxOn(TextSlot, Canvas);

		// Рамка видна всегда (значок слота спрятан, пока слот пуст), нажатий не ловит и лежит
		// под значком, а не поверх него.
		TestEqual(*FString::Printf(TEXT("Рамка слота %s видна и не ловит нажатия"), Names.Title),
			Frame->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestTrue(*FString::Printf(TEXT("Рамка слота %s не прозрачная"), Names.Title),
			Frame->GetColorAndOpacity().A * Frame->GetBrush().TintColor.GetSpecifiedColor().A > 0.02f
			&& Frame->GetBrush().GetDrawType() != ESlateBrushDrawType::NoDrawType);
		TestTrue(*FString::Printf(TEXT("Рамка слота %s стоит по месту значка"), Names.Title),
			FrameBox.Min.Equals(IconBox.Min, 0.5) && FrameBox.Max.Equals(IconBox.Max, 0.5));
		TestTrue(*FString::Printf(TEXT("Рамка слота %s лежит под значком"), Names.Title),
			FrameSlot->GetZOrder() < IconSlot->GetZOrder());
		TestTrue(*FString::Printf(TEXT("Рамка слота %s квадратная (стороны %.0f и %.0f)"), Names.Title,
			FrameSlot->GetSize().X, FrameSlot->GetSize().Y),
			FMath::Abs(FrameSlot->GetSize().X - FrameSlot->GetSize().Y) <= 12.0);

		// Название: в пределах колонки, под своей рамкой, не заходит на рюкзак и на следующий слот.
		TestTrue(*FString::Printf(TEXT("Название слота %s (до %.0f) не заходит на рюкзак (начало %.0f)"),
			Names.Title, TextBox.Max.X, BackpackLeft), TextBox.Max.X <= BackpackLeft);
		TestTrue(*FString::Printf(TEXT("Название слота %s стоит под своей рамкой"), Names.Title),
			TextBox.Min.Y >= FrameBox.Max.Y && TextBox.Min.X >= FrameBox.Min.X - 0.5);
		TestTrue(*FString::Printf(TEXT("Рамка слота %s (верх %.0f) ниже названия предыдущего слота (низ %.0f)"),
			Names.Title, FrameBox.Min.Y, PrevTextBottom), FrameBox.Min.Y >= PrevTextBottom);
		PrevTextBottom = TextBox.Max.Y;
		AddInfo(FString::Printf(TEXT("Слот %s: рамка (%.0f, %.0f) — (%.0f, %.0f), название до (%.0f, %.0f)."),
			Names.Title, FrameBox.Min.X, FrameBox.Min.Y, FrameBox.Max.X, FrameBox.Max.Y, TextBox.Max.X, TextBox.Max.Y));
	}

	// Подложка окна на экране 16:9 (1920 на 1080).
	const FBox2D PlateBox = BoxOn(PlateSlot, FVector2D(1920.0, 1080.0));
	TestTrue(*FString::Printf(TEXT("Подложка окна сумки целиком на экране 16:9: от %.0f до %.0f по ширине"),
		PlateBox.Min.X, PlateBox.Max.X), PlateBox.Min.X >= 0.0 && PlateBox.Max.X <= 1920.0);
	return true;
}

// ===========================================================================
// Окно-смартфон: картинка корпуса и значок экранной кнопки заведены в проект и прописаны в
// настройках; отступы экрана внутри картинки — те, что сняты с неё (доли ширины и высоты).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiComfortPhonePicturesConfiguredTest,
	"ContrarySurvivor.UiComfort.PhonePicturesAreConfigured", UiComfortTestFlags)

bool FUiComfortPhonePicturesConfiguredTest::RunTest(const FString& Parameters)
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const UTexture2D* Phone = Settings->PhoneTexture.LoadSynchronous();
	const UTexture2D* Icon = Settings->PdaButtonIcon.LoadSynchronous();
	// Размер картинки здесь не сверяется: в прогоне без графики картинка сразу после загрузки
	// ещё готовится и отдаёт нулевой размер (сам размер снят с файла отдельно — 1536 на 824).
	TestNotNull(TEXT("Картинка смартфона из настроек загружается"), Phone);
	TestNotNull(TEXT("Значок экранной кнопки из настроек загружается"), Icon);

	const FMargin Insets = Settings->PhoneScreenInsets;
	TestTrue(*FString::Printf(TEXT("Отступы экрана внутри картинки заданы под эту картинку (%.4f, %.4f, %.4f, %.4f)"),
		Insets.Left, Insets.Top, Insets.Right, Insets.Bottom),
		FMath::IsNearlyEqual(Insets.Left, 0.0911f, 0.001f) && FMath::IsNearlyEqual(Insets.Top, 0.1165f, 0.001f)
		&& FMath::IsNearlyEqual(Insets.Right, 0.0788f, 0.001f) && FMath::IsNearlyEqual(Insets.Bottom, 0.1274f, 0.001f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
