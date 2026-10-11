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
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/Data/ContraryQuestLibrary.h"
#include "ContrarySurvivor/UI/PdaMapMath.h"
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
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

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

	// Кнопка смартфона — обычная кнопка этого же файла (Ринат 11.10.2026: «Перенести её в WBP
	// touch controls»): место и размер берутся из файла, на кнопке лежит значок с картинкой.
	const UButton* PdaButton = Cast<UButton>(Tree->FindWidget(TEXT("PdaButton")));
	const UCanvasPanelSlot* PdaSlot = PointSlot(PdaButton);
	if (!TestNotNull(TEXT("Кнопка смартфона PdaButton лежит в файле экранных кнопок"), PdaSlot))
	{
		return false;
	}
	TestTrue(TEXT("Кнопка смартфона лежит прямо в корне слоя, как остальные кнопки"), PdaButton->GetParent() == Root);
	TestTrue(TEXT("Кнопка смартфона видна"), PdaButton->GetVisibility() == ESlateVisibility::Visible);
	const UImage* PdaIcon = Cast<UImage>(Tree->FindWidget(TEXT("PdaButtonIcon")));
	if (TestNotNull(TEXT("Значок PdaButtonIcon лежит в файле экранных кнопок"), PdaIcon))
	{
		TestTrue(TEXT("Значок лежит на кнопке смартфона"), PdaIcon->GetParent() == PdaButton);
		TestNotNull(TEXT("У значка есть картинка"), PdaIcon->GetBrush().GetResourceObject());
		const FVector2D IconSize = PdaIcon->GetBrush().GetImageSize();
		AddInfo(FString::Printf(TEXT("Кнопка смартфона %.0f на %.0f, значок %.1f на %.1f."),
			PdaSlot->GetSize().X, PdaSlot->GetSize().Y, IconSize.X, IconSize.Y));
		TestTrue(TEXT("Значок не больше своей кнопки"),
			IconSize.X > 1.0 && IconSize.Y > 1.0 && IconSize.X <= PdaSlot->GetSize().X && IconSize.Y <= PdaSlot->GetSize().Y);
		TestTrue(TEXT("Значок не перехватывает нажатие у кнопки"),
			PdaIcon->GetVisibility() == ESlateVisibility::HitTestInvisible || PdaIcon->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	}
	const double BagScale = GetDefault<UTouchControlsWidget>()->GetInventoryBlinkScale();
	const double PdaRadius = PdaSlot->GetSize().GetMin() * 0.5;

	// Размеры кнопок и место ножа относительно огня — справочно: их теперь задаёт владелец в
	// файле экранных кнопок, его правка не должна ронять проверку. Жёсткие требования ниже:
	// кнопки не пересекаются, не выходят за экран и не заходят на джойстик.
	const FVector2D ReloadSize = ReloadSlot->GetSize();
	const FVector2D MeleeSize = MeleeSlot->GetSize();
	AddInfo(FString::Printf(TEXT("Справочно: кнопка ножа %.0f на %.0f, кнопка смартфона %.0f на %.0f, кнопка перезарядки %.0f на %.0f (%s)."),
		MeleeSize.X, MeleeSize.Y, PdaSlot->GetSize().X, PdaSlot->GetSize().Y, ReloadSize.X, ReloadSize.Y,
		(MeleeSize.X >= ReloadSize.X && MeleeSize.Y >= ReloadSize.Y && PdaRadius * 2.0 >= ReloadSize.X && PdaRadius * 2.0 >= ReloadSize.Y)
			? TEXT("нож и смартфон не меньше перезарядки") : TEXT("нож или смартфон меньше перезарядки")));
	const FVector2D ProbeScreen(1920.0, 1080.0);
	const double MeleeToFire = FVector2D::Distance(BoxOn(MeleeSlot, ProbeScreen).GetCenter(), BoxOn(FireSlot, ProbeScreen).GetCenter());
	AddInfo(FString::Printf(TEXT("Справочно: между центрами кнопок ножа и огня %.0f (полтора поперечника огня — %.0f), угол привязки %s."),
		MeleeToFire, FireSlot->GetSize().GetMax() * 1.5,
		MeleeSlot->GetAnchors().Minimum == FireSlot->GetAnchors().Minimum ? TEXT("тот же") : TEXT("другой")));

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
		const FBox2D PdaBox = BoxOn(PdaSlot, Screen); // сама кнопка уже в общем списке
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
	const UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/T_PdaButtonIcon.T_PdaButtonIcon"));
	// Размер картинки здесь не сверяется: в прогоне без графики картинка сразу после загрузки
	// ещё готовится и отдаёт нулевой размер (сам размер снят с файла отдельно — 1536 на 824).
	TestNotNull(TEXT("Картинка смартфона из настроек загружается"), Phone);
	TestNotNull(TEXT("Картинка значка экранной кнопки лежит в проекте"), Icon);

	const FMargin Insets = Settings->PhoneScreenInsets;
	TestTrue(*FString::Printf(TEXT("Отступы экрана внутри картинки заданы под эту картинку (%.4f, %.4f, %.4f, %.4f)"),
		Insets.Left, Insets.Top, Insets.Right, Insets.Bottom),
		FMath::IsNearlyEqual(Insets.Left, 0.0911f, 0.001f) && FMath::IsNearlyEqual(Insets.Top, 0.1165f, 0.001f)
		&& FMath::IsNearlyEqual(Insets.Right, 0.0788f, 0.001f) && FMath::IsNearlyEqual(Insets.Bottom, 0.1274f, 0.001f));
	return true;
}

// ===========================================================================
// Смартфон на экране не сужен (замечание Рината 11.10.2026: «смартфон… сужено сильно… Иконка
// смартфона слишком маленькая»). У окна, созданного из настоящего WBP_Pda при настоящих
// настройках, подложка имеет пропорции картинки корпуса на экранах 1920 и 2400 и не выходит
// за экран. Причина сужения была в том, что размер спрашивали у картинки «сейчас»: в
// редакторе в первый кадр после загрузки она отвечает размером заглушки 32 на 32.
// Значок на кнопке: рисунок без прозрачных полей, занимает 85% поперечника кнопки.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiComfortPhoneIsNotNarrowTest,
	"ContrarySurvivor.UiComfort.PhoneKeepsPictureProportions", UiComfortTestFlags)

bool FUiComfortPhoneIsNotNarrowTest::RunTest(const FString& Parameters)
{
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const UTexture2D* Phone = Settings->PhoneTexture.LoadSynchronous();
	const FIntPoint Picture = PdaMap::GetPictureSize(Phone);
	if (!TestTrue(*FString::Printf(TEXT("Размер картинки смартфона известен сразу после загрузки (%d на %d)"), Picture.X, Picture.Y),
		Picture.X > Picture.Y && Picture.Y > 0))
	{
		return false;
	}
	const double PictureAspect = static_cast<double>(Picture.X) / Picture.Y;
	TestTrue(*FString::Printf(TEXT("Запасные пропорции без картинки (%.3f) те же, что у картинки (%.3f)"),
		Settings->PhoneAspectRatio, PictureAspect), FMath::Abs(Settings->PhoneAspectRatio / PictureAspect - 1.0) <= 0.01);

	UClass* WindowClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	UWorld* World = nullptr;
	if (GEngine)
	{
		World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (World)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
	}
	UPdaScreenWidget* Window = (World && WindowClass) ? CreateWidget<UPdaScreenWidget>(World, WindowClass) : nullptr;
	if (TestNotNull(TEXT("Окно создано из готового файла WBP_Pda"), Window))
	{
		// Пропорции подложки в самом файле окна (то, что видно до первой подгонки).
		const UWidget* Plate = Window->WidgetTree ? Window->WidgetTree->FindWidget(TEXT("PanelPlate")) : nullptr;
		const UCanvasPanelSlot* PlateSlot = Plate ? Cast<UCanvasPanelSlot>(Plate->Slot) : nullptr;
		if (TestNotNull(TEXT("Подложка окна лежит в холсте"), PlateSlot))
		{
			const FVector2D Stored = PlateSlot->GetSize();
			TestTrue(*FString::Printf(TEXT("Подложка в файле окна имеет пропорции картинки (%.0f на %.0f)"), Stored.X, Stored.Y),
				Stored.Y > 1.0 && FMath::Abs(Stored.X / Stored.Y / PictureAspect - 1.0) <= 0.01);
		}

		Window->InitPda(nullptr);
		for (const double ScreenWidth : { 1920.0, 2400.0 })
		{
			const FVector2D Screen(ScreenWidth, 1080.0);
			Window->ApplyPhoneLayout(Screen);
			TestTrue(*FString::Printf(TEXT("Экран %.0f: картинка смартфона подложена"), ScreenWidth), Window->IsPhoneTextureShown());
			const FVector2D Size = PlateSlot ? PlateSlot->GetSize() : FVector2D::ZeroVector;
			AddInfo(FString::Printf(TEXT("Экран %.0f на 1080: подложка %.1f на %.1f, пропорции %.3f (у картинки %.3f)."),
				ScreenWidth, Size.X, Size.Y, Size.Y > 0.0 ? Size.X / Size.Y : 0.0, PictureAspect));
			TestTrue(*FString::Printf(TEXT("Экран %.0f: подложка имеет пропорции картинки с допуском 1%% (%.1f на %.1f)"), ScreenWidth, Size.X, Size.Y),
				Size.Y > 1.0 && FMath::Abs(Size.X / Size.Y / PictureAspect - 1.0) <= 0.01);
			TestTrue(*FString::Printf(TEXT("Экран %.0f: подложка не выходит за экран"), ScreenWidth),
				Size.X <= Screen.X && Size.Y <= Screen.Y);
			TestTrue(*FString::Printf(TEXT("Экран %.0f: смартфон занимает не меньше 85%% высоты или ширины экрана"), ScreenWidth),
				Size.Y >= Screen.Y * 0.85 || Size.X >= Screen.X * 0.85);
			TestTrue(*FString::Printf(TEXT("Экран %.0f: размер, который помнит окно, тот же, что у подложки"), ScreenWidth),
				Window->GetPhoneSize().Equals(Size, 0.5));
		}
	}
	if (World && GEngine)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}

	// Картинка значка кнопки: обрезана по рисунку. Сам значок и его размер на кнопке теперь
	// лежат в файле экранных кнопок, их правит владелец (проверка — TouchButtonsDoNotOverlap).
	const UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/Textures/T_PdaButtonIcon.T_PdaButtonIcon"));
	const FIntPoint IconPicture = PdaMap::GetPictureSize(Icon);
	TestTrue(*FString::Printf(TEXT("Картинка значка обрезана по рисунку, без прозрачных полей сверху и снизу (%d на %d)"),
		IconPicture.X, IconPicture.Y), IconPicture.Y > 0 && static_cast<double>(IconPicture.X) / IconPicture.Y >= 1.7);
	return true;
}

// ===========================================================================
// Окно смартфона под палец (замечания Рината с телефона 11.10.2026: «Сделай кнопки в смартфоне
// игрока побольше… Убери желтую рамку вокруг смартфона. Смотри чтоб надписи не поехали.
// Надписям поставь шрифт - жирный»). Проверяется файл окна WBP_Pda, как он лежит в проекте:
// размеры и просветы кнопок, жирное начертание всех надписей, подложка-картинка без рамки,
// и что после этого ничего не налезает и не выходит за экран смартфона на 1920 и 2400.
// Высота строк текста оценивается по кеглю (живого окна в прогоне нет): строка занимает 1,6
// кегля, знак в среднем 0,8 кегля в ширину — с запасом для жирного шрифта.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiComfortPhoneWindowUnderFingerTest,
	"ContrarySurvivor.UiComfort.PhoneWindowButtonsFontAndFrame", UiComfortTestFlags)

bool FUiComfortPhoneWindowUnderFingerTest::RunTest(const FString& Parameters)
{
	using namespace UiComfortTest;
	const UWidgetTree* Tree = LoadTree(TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	if (!TestNotNull(TEXT("Готовый файл окна смартфона загружен"), Tree))
	{
		return false;
	}
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();

	// --- Подложка: картинка смартфона чистой кистью, без рамки запасной плашки ---
	const UBorder* Plate = Cast<UBorder>(Tree->FindWidget(TEXT("PanelPlate")));
	if (TestNotNull(TEXT("Подложка окна"), Plate))
	{
		TestTrue(TEXT("Подложка рисуется картинкой смартфона"),
			Plate->Background.GetResourceObject() != nullptr && Plate->Background.GetResourceObject() == Settings->PhoneTexture.LoadSynchronous());
		TestTrue(TEXT("У подложки нет скруглённой плашки с контуром (жёлтой рамки)"),
			Plate->Background.DrawAs == ESlateBrushDrawType::Image);
	}

	// --- Все надписи файла окна — жирным ---
	int32 TextCount = 0;
	Tree->ForEachWidget([this, &TextCount](UWidget* Widget)
	{
		if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			++TextCount;
			TestEqual(*FString::Printf(TEXT("Надпись %s — жирным начертанием"), *Text->GetName()),
				Text->GetFont().TypefaceFontName, FName(TEXT("Bold")));
		}
	});
	TestTrue(*FString::Printf(TEXT("В окне есть надписи (нашлось %d)"), TextCount), TextCount >= 12);

	// --- Раскладка внутри экрана смартфона на двух экранах ---
	for (const double ScreenWidth : { 1920.0, 2400.0 })
	{
		const FVector2D Screen(ScreenWidth, 1080.0);
		const FIntPoint Picture = PdaMap::GetPictureSize(Settings->PhoneTexture.LoadSynchronous());
		const double Aspect = Picture.Y > 0 ? static_cast<double>(Picture.X) / Picture.Y : Settings->PhoneAspectRatio;
		const FVector2D Phone = PdaMap::ComputePhoneSize(Screen, Settings->PhoneHeightFraction, Aspect, Settings->PhoneSideMargin);
		const FMargin Pad = PdaMap::ComputePhoneScreenPadding(Phone, Settings->PhoneScreenInsets);
		const FVector2D Inner(Phone.X - Pad.Left - Pad.Right, Phone.Y - Pad.Top - Pad.Bottom);
		AddInfo(FString::Printf(TEXT("Экран %.0f: смартфон %.0f на %.0f, его экран %.0f на %.0f."), ScreenWidth, Phone.X, Phone.Y, Inner.X, Inner.Y));

		// Прямоугольник элемента в своей канве; у растянутых якорей отступы — от краёв.
		auto BoxIn = [](const UWidget* Widget, const FVector2D& Area, double AutoHeight) -> FBox2D
		{
			const UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
			if (!CanvasSlot)
			{
				return FBox2D(FVector2D::ZeroVector, FVector2D::ZeroVector);
			}
			const FAnchors Anchors = CanvasSlot->GetAnchors();
			const FMargin Offsets = CanvasSlot->GetOffsets();
			const FVector2D AnchorMin = FVector2D(Anchors.Minimum) * Area;
			const FVector2D AnchorMax = FVector2D(Anchors.Maximum) * Area;
			const bool bStretchX = Anchors.Minimum.X != Anchors.Maximum.X;
			const bool bStretchY = Anchors.Minimum.Y != Anchors.Maximum.Y;
			const double W = bStretchX ? (AnchorMax.X - Offsets.Right) - (AnchorMin.X + Offsets.Left) : (CanvasSlot->GetAutoSize() ? 0.0 : Offsets.Right);
			const double H = bStretchY ? (AnchorMax.Y - Offsets.Bottom) - (AnchorMin.Y + Offsets.Top) : (CanvasSlot->GetAutoSize() ? AutoHeight : Offsets.Bottom);
			const FVector2D Align = CanvasSlot->GetAlignment();
			const FVector2D Min(bStretchX ? AnchorMin.X + Offsets.Left : AnchorMin.X + Offsets.Left - W * Align.X,
				bStretchY ? AnchorMin.Y + Offsets.Top : AnchorMin.Y + Offsets.Top - H * Align.Y);
			return FBox2D(Min, Min + FVector2D(W, H));
		};
		auto LineHeight = [](const UWidget* Widget)
		{
			const UTextBlock* Text = Cast<UTextBlock>(Widget);
			return Text ? FMath::CeilToDouble(Text->GetFont().Size * 1.6) : 0.0;
		};
		auto Find = [Tree](const TCHAR* WidgetName) { return Tree->FindWidget(WidgetName); };

		// Левый столбец: заголовок, две вкладки, подсказка, «Закрыть».
		const UWidget* Title = Find(TEXT("TitleText"));
		const UWidget* QuestsTab = Find(TEXT("QuestsTabButton"));
		const UWidget* MapTab = Find(TEXT("MapTabButton"));
		const UWidget* Hint = Find(TEXT("MapHintText"));
		const UWidget* Close = Find(TEXT("CloseButton"));
		const UWidget* QuestsPanel = Find(TEXT("QuestsPanel"));
		const UWidget* MapPanel = Find(TEXT("MapPanel"));
		if (!TestTrue(TEXT("Элементы левого столбца и вкладки на месте"), Title && QuestsTab && MapTab && Hint && Close && QuestsPanel && MapPanel))
		{
			return false;
		}
		TArray<FNamedBox> Column;
		Column.Add({ TEXT("заголовок"), BoxIn(Title, Inner, LineHeight(Title)) });
		Column.Add({ TEXT("вкладка «Задания»"), BoxIn(QuestsTab, Inner, 0.0) });
		Column.Add({ TEXT("вкладка «Карта»"), BoxIn(MapTab, Inner, 0.0) });
		Column.Add({ TEXT("подсказка карты"), BoxIn(Hint, Inner, 0.0) });
		Column.Add({ TEXT("кнопка «Закрыть»"), BoxIn(Close, Inner, 0.0) });
		const FBox2D ContentBox = BoxIn(QuestsPanel, Inner, 0.0);
		for (int32 A = 0; A < Column.Num(); ++A)
		{
			const FBox2D& Box = Column[A].Box;
			TestTrue(*FString::Printf(TEXT("Экран %.0f: %s внутри экрана смартфона и левее вкладки: (%.0f, %.0f) — (%.0f, %.0f)"),
				ScreenWidth, *Column[A].Name, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y),
				Box.Min.X >= 0.0 && Box.Min.Y >= 0.0 && Box.Max.Y <= Inner.Y && Box.Max.X <= ContentBox.Min.X);
			for (int32 B = A + 1; B < Column.Num(); ++B)
			{
				TestTrue(*FString::Printf(TEXT("Экран %.0f: %s не налезает на «%s»"), ScreenWidth, *Column[A].Name, *Column[B].Name),
					Box.Max.Y <= Column[B].Box.Min.Y || Column[B].Box.Max.Y <= Box.Min.Y);
			}
		}
		for (int32 Index = 1; Index <= 4; Index += (Index == 2 ? 2 : 1)) // вкладки и «Закрыть»
		{
			TestTrue(*FString::Printf(TEXT("Экран %.0f: %s высотой не меньше 96 (вышло %.0f)"), ScreenWidth, *Column[Index].Name, Column[Index].Box.GetSize().Y),
				Column[Index].Box.GetSize().Y >= 96.0);
		}
		TestTrue(*FString::Printf(TEXT("Экран %.0f: между вкладками просвет не меньше 12 (вышло %.0f)"), ScreenWidth, Column[2].Box.Min.Y - Column[1].Box.Max.Y),
			Column[2].Box.Min.Y - Column[1].Box.Max.Y >= 12.0);
		TestTrue(*FString::Printf(TEXT("Экран %.0f: между подсказкой и «Закрыть» просвет не меньше 12 (вышло %.0f)"), ScreenWidth, Column[4].Box.Min.Y - Column[3].Box.Max.Y),
			Column[4].Box.Min.Y - Column[3].Box.Max.Y >= 12.0);
		// Подсказка карты помещается в свой блок.
		if (const UTextBlock* HintText = Cast<UTextBlock>(Hint))
		{
			const double CharsPerLine = FMath::FloorToDouble(Column[3].Box.GetSize().X / (HintText->GetFont().Size * 0.8));
			const double Lines = FMath::CeilToDouble(HintText->GetText().ToString().Len() / FMath::Max(1.0, CharsPerLine)) + 1.0;
			TestTrue(*FString::Printf(TEXT("Экран %.0f: подсказка карты (около %.0f строк) помещается в свой блок высотой %.0f"), ScreenWidth, Lines, Column[3].Box.GetSize().Y),
				Lines * LineHeight(Hint) <= Column[3].Box.GetSize().Y);
		}

		// Вкладка «Задания»: список слева, сведения справа, «Отслеживать» внизу.
		const FVector2D Content = ContentBox.GetSize();
		const UWidget* List = Find(TEXT("QuestList"));
		const UWidget* DTitle = Find(TEXT("DetailTitleText"));
		const UWidget* DGiver = Find(TEXT("DetailGiverText"));
		const UWidget* DDesc = Find(TEXT("DetailDescriptionText"));
		const UWidget* DObj = Find(TEXT("DetailObjectivesText"));
		const UWidget* DReward = Find(TEXT("DetailRewardText"));
		const UWidget* Track = Find(TEXT("TrackButton"));
		if (TestTrue(TEXT("Элементы вкладки «Задания» на месте"), List && DTitle && DGiver && DDesc && DObj && DReward && Track))
		{
			const FBox2D ListBox = BoxIn(List, Content, 0.0);
			TArray<FNamedBox> Detail;
			Detail.Add({ TEXT("название задания"), BoxIn(DTitle, Content, LineHeight(DTitle)) });
			Detail.Add({ TEXT("кто выдал и кому сдать"), BoxIn(DGiver, Content, LineHeight(DGiver) * 2.0) });
			Detail.Add({ TEXT("описание"), BoxIn(DDesc, Content, 0.0) });
			Detail.Add({ TEXT("цели"), BoxIn(DObj, Content, 0.0) });
			Detail.Add({ TEXT("награда"), BoxIn(DReward, Content, 0.0) });
			Detail.Add({ TEXT("кнопка «Отслеживать»"), BoxIn(Track, Content, 0.0) });
			for (int32 A = 0; A < Detail.Num(); ++A)
			{
				const FBox2D& Box = Detail[A].Box;
				TestTrue(*FString::Printf(TEXT("Экран %.0f: %s правее списка и внутри экрана смартфона: (%.0f, %.0f) — (%.0f, %.0f)"),
					ScreenWidth, *Detail[A].Name, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y),
					Box.Min.X >= ListBox.Max.X && Box.Min.Y >= 0.0 && Box.Max.Y <= Content.Y && Box.Max.X <= Content.X + 0.5);
				if (A + 1 < Detail.Num())
				{
					TestTrue(*FString::Printf(TEXT("Экран %.0f: %s не налезает на «%s»"), ScreenWidth, *Detail[A].Name, *Detail[A + 1].Name),
						Box.Max.Y <= Detail[A + 1].Box.Min.Y);
				}
			}
			TestTrue(*FString::Printf(TEXT("Экран %.0f: кнопка «Отслеживать» высотой не меньше 96 (вышло %.0f)"), ScreenWidth, Detail[5].Box.GetSize().Y),
				Detail[5].Box.GetSize().Y >= 96.0);
			TestTrue(*FString::Printf(TEXT("Экран %.0f: между наградой и кнопкой «Отслеживать» просвет не меньше 12"), ScreenWidth),
				Detail[5].Box.Min.Y - Detail[4].Box.Max.Y >= 12.0);

			// Самое длинное описание задания помещается в блок описания.
			TArray<FQuest> AllQuests;
			const AElderNPC* Elder = GetDefault<AElderNPC>();
			for (const FQuest* Def : { &Elder->GetOfferedQuest(), &Elder->GetSecondQuest(), &Elder->GetThirdQuest() })
			{
				FQuest FromTable;
				AllQuests.Add(ContraryQuests::BuildQuestFromRow(Def->QuestId, FromTable) ? FromTable : *Def);
			}
			AllQuests.Add(GetDefault<AScientistGuardNPC>()->GetOfferedQuest());
			int32 Longest = 0;
			FString LongestTitle;
			for (const FQuest& Quest : AllQuests)
			{
				if (Quest.Description.ToString().Len() > Longest)
				{
					Longest = Quest.Description.ToString().Len();
					LongestTitle = Quest.Title.ToString();
				}
			}
			if (const UTextBlock* DescText = Cast<UTextBlock>(DDesc))
			{
				const double CharsPerLine = FMath::FloorToDouble(Detail[2].Box.GetSize().X / (DescText->GetFont().Size * 0.8));
				const double Lines = FMath::CeilToDouble(Longest / FMath::Max(1.0, CharsPerLine)) + 1.0;
				AddInfo(FString::Printf(TEXT("Экран %.0f: самое длинное описание — «%s», %d знаков, около %.0f строк по %.0f знаков; блок вмещает %.0f строк."),
					ScreenWidth, *LongestTitle, Longest, Lines, CharsPerLine, FMath::FloorToDouble(Detail[2].Box.GetSize().Y / LineHeight(DDesc))));
				TestTrue(*FString::Printf(TEXT("Экран %.0f: самое длинное описание задания помещается в свой блок"), ScreenWidth),
					Lines * LineHeight(DDesc) <= Detail[2].Box.GetSize().Y);
			}
		}

		// Вкладка «Карта»: три кнопки столбиком, под палец; подписи мест не выходят за поле.
		const FBox2D MapBox = BoxIn(MapPanel, Inner, 0.0);
		TestTrue(*FString::Printf(TEXT("Экран %.0f: поле карты внутри экрана смартфона"), ScreenWidth),
			MapBox.Min.X >= 0.0 && MapBox.Min.Y >= 0.0 && MapBox.Max.X <= Inner.X + 0.5 && MapBox.Max.Y <= Inner.Y + 0.5);
		TestTrue(TEXT("Поле карты обрезает всё, что выходит за него (подписи мест за экран не вылезут)"),
			MapPanel->GetClipping() == EWidgetClipping::ClipToBounds);
		TArray<FNamedBox> MapButtons;
		for (const TCHAR* ButtonName : { TEXT("MapZoomInButton"), TEXT("MapZoomOutButton"), TEXT("MapFindHeroButton") })
		{
			if (const UWidget* Button = Find(ButtonName))
			{
				MapButtons.Add({ ButtonName, BoxIn(Button, MapBox.GetSize(), 0.0) });
			}
		}
		if (TestEqual(TEXT("На поле карты три кнопки"), MapButtons.Num(), 3))
		{
			for (int32 A = 0; A < 3; ++A)
			{
				const FBox2D& Box = MapButtons[A].Box;
				TestTrue(*FString::Printf(TEXT("Экран %.0f: %s не меньше 96 на 96 (вышло %.0f на %.0f)"), ScreenWidth, *MapButtons[A].Name, Box.GetSize().X, Box.GetSize().Y),
					Box.GetSize().X >= 96.0 && Box.GetSize().Y >= 96.0);
				TestTrue(*FString::Printf(TEXT("Экран %.0f: %s внутри поля карты"), ScreenWidth, *MapButtons[A].Name),
					Box.Min.X >= 0.0 && Box.Min.Y >= 0.0 && Box.Max.X <= MapBox.GetSize().X && Box.Max.Y <= MapBox.GetSize().Y);
				if (A + 1 < 3)
				{
					TestTrue(*FString::Printf(TEXT("Экран %.0f: между %s и %s просвет не меньше 12 (вышло %.0f)"), ScreenWidth,
						*MapButtons[A].Name, *MapButtons[A + 1].Name, MapButtons[A + 1].Box.Min.Y - Box.Max.Y),
						MapButtons[A + 1].Box.Min.Y - Box.Max.Y >= 12.0);
				}
			}
		}
	}

	// --- Строки списка заданий (их строит код, размер — в настройках окна) ---
	const UWidgetBlueprintGeneratedClass* WindowClass = Cast<UWidgetBlueprintGeneratedClass>(
		StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C")));
	if (const UPdaScreenWidget* WindowDefaults = WindowClass ? Cast<UPdaScreenWidget>(WindowClass->GetDefaultObject()) : nullptr)
	{
		const double RowHeight = WindowDefaults->QuestRowPadding.Top + WindowDefaults->QuestRowPadding.Bottom + FMath::CeilToDouble(17.0 * 1.6);
		TestTrue(*FString::Printf(TEXT("Строка задания в списке высотой не меньше 96 (вышло около %.0f)"), RowHeight), RowHeight >= 96.0);
		TestTrue(TEXT("Между строками заданий просвет не меньше 12"), WindowDefaults->QuestRowGap >= 12.0f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
