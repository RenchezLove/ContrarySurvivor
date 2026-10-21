// Fill out your copyright notice in the Description page of Project Settings.

// Правки Рината после игры на телефоне 11.10.2026 (вечер), часть «окна»:
//   - смартфон: меню вкладок слева, вкладка справа, весь экран смартфона занят, ни одна кнопка,
//     вкладка или надпись не выходит за экран смартфона; подпись «Моя позиция» не шире кнопки;
//   - торговля, обыск, личный ящик: сумка справа не заезжает на окно слева;
//   - цифра количества на плитке предмета крупнее и с чёрной обводкой;
//   - синяя метка побочного задания рядом с меткой основного;
//   - район поиска южного логова на карте: отметка, подпись, мигания, три минуты, приход героя,
//     сохранение.
// Проверяются готовые файлы окон, как они лежат в проекте, и окна, созданные из них. Живой вид
// на экране прогон не показывает — его смотрит Ринат.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/MasterEnemyBase.h"
#include "ContrarySurvivor/Actors/ScientistNPCs.h"
#include "ContrarySurvivor/Components/MapExplorationComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/UI/ItemTileWidget.h"
#include "ContrarySurvivor/UI/PdaMapMath.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "ContrarySurvivor/UI/QuestTargets.h"
#include "ContrarySurvivor/UI/QuestTrackerWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags PolishUiTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace PolishUiTest
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

	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World ? World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params) : nullptr;
	}

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}

	static const UWidgetTree* LoadTree(const TCHAR* ClassPath)
	{
		const UWidgetBlueprintGeneratedClass* Class = Cast<UWidgetBlueprintGeneratedClass>(
			StaticLoadClass(UUserWidget::StaticClass(), nullptr, ClassPath));
		return Class ? Class->GetWidgetTreeArchetype() : nullptr;
	}

	// Живых размеров текста в прогоне нет: строка занимает 1,6 кегля в высоту, знак — в среднем
	// 0,8 кегля в ширину (с запасом для жирного шрифта). Те же оценки, что в UiComfort.
	static FVector2D EstimateTextSize(const UTextBlock* Text)
	{
		TArray<FString> Lines;
		Text->GetText().ToString().ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);
		int32 Longest = 0;
		for (const FString& Line : Lines)
		{
			Longest = FMath::Max(Longest, Line.Len());
		}
		const double Size = Text->GetFont().Size;
		return FVector2D(Longest * Size * 0.8, FMath::Max(1, Lines.Num()) * FMath::CeilToDouble(Size * 1.6));
	}

	// Прямоугольник элемента в канве родителя размера Area; у растянутых якорей отступы — от краёв.
	static FBox2D BoxIn(const UWidget* Widget, const FVector2D& Area)
	{
		const UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		if (!CanvasSlot)
		{
			return FBox2D(FVector2D::ZeroVector, Area);
		}
		const FAnchors Anchors = CanvasSlot->GetAnchors();
		const FMargin Offsets = CanvasSlot->GetOffsets();
		const FVector2D AnchorMin = FVector2D(Anchors.Minimum) * Area;
		const FVector2D AnchorMax = FVector2D(Anchors.Maximum) * Area;
		const bool bStretchX = Anchors.Minimum.X != Anchors.Maximum.X;
		const bool bStretchY = Anchors.Minimum.Y != Anchors.Maximum.Y;
		FVector2D Auto = FVector2D::ZeroVector;
		if (CanvasSlot->GetAutoSize())
		{
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				Auto = EstimateTextSize(Text);
			}
		}
		const double W = bStretchX ? (AnchorMax.X - Offsets.Right) - (AnchorMin.X + Offsets.Left) : (CanvasSlot->GetAutoSize() ? Auto.X : Offsets.Right);
		const double H = bStretchY ? (AnchorMax.Y - Offsets.Bottom) - (AnchorMin.Y + Offsets.Top) : (CanvasSlot->GetAutoSize() ? Auto.Y : Offsets.Bottom);
		const FVector2D Align = CanvasSlot->GetAlignment();
		const FVector2D Min(bStretchX ? AnchorMin.X + Offsets.Left : AnchorMin.X + Offsets.Left - W * Align.X,
			bStretchY ? AnchorMin.Y + Offsets.Top : AnchorMin.Y + Offsets.Top - H * Align.Y);
		return FBox2D(Min, Min + FVector2D(W, H));
	}

	struct FPlaced
	{
		const UWidget* Widget;
		FBox2D Box; // в координатах экрана смартфона
	};

	// Все элементы внутри экрана смартфона с их прямоугольниками. Не заходит в закрытую вкладку
	// и в саму карту (MapArea: её ставит код, а поле карты обрезает всё лишнее) и в список
	// заданий (строки строит код).
	static void Collect(const UWidget* Widget, const FBox2D& Box, const UWidget* ClosedTab, TArray<FPlaced>& Out)
	{
		const UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
		if (!Panel || !Panel->IsA<UCanvasPanel>())
		{
			return;
		}
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			const UWidget* Child = Panel->GetChildAt(Index);
			if (!Child || Child == ClosedTab || Child->GetFName() == FName(TEXT("MapArea")))
			{
				continue;
			}
			FBox2D ChildBox = BoxIn(Child, Box.GetSize());
			ChildBox = FBox2D(ChildBox.Min + Box.Min, ChildBox.Max + Box.Min);
			Out.Add({ Child, ChildBox });
			Collect(Child, ChildBox, ClosedTab, Out);
		}
	}
}

// ===========================================================================
// Смартфон (слова Рината 11.10.2026: «у смартфона есть экран… некоторые кнопки на смартфоне
// выходят за пределы экрана. Такого быть не должно… Слева - меню вкладок смартфона. Справа -
// содержимое активной вкладки… ни одна вкладка и ни одна кнопка и ни один текст не должны
// выходить за пределы экрана смартфона»).
// С 21.10.2026 (Ринат: «в WBP_Pda я должен иметь возможность свободно двигать элементы») код
// элементы файла окна не двигает: на любом экране смартфон масштабируется целиком, расстановка
// внутри — как в файле. Поэтому:
//  - у файла окна проверяется, что на обеих вкладках всё лежит внутри экрана смартфона и
//    подписи помещаются в свои места на кнопках, а подгонка под четыре экрана — масштаб
//    подложки целиком, смартфон целиком на экране;
//  - форма раскладки по умолчанию (меню от левого края одной ширины на обеих вкладках, вкладка
//    до правого края, поле карты ровно под карту) проверяется на дереве BuildDefaultTree: файл
//    после создания принадлежит владельцу, его форма — его дело.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishUiPhoneContentInsideScreenTest,
	"ContrarySurvivor.PolishUi.PhoneContentStaysInsidePhoneScreen", PolishUiTestFlags)

bool FPolishUiPhoneContentInsideScreenTest::RunTest(const FString& Parameters)
{
	using namespace PolishUiTest;
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();

	// Экран смартфона в единицах окна: размер подложки в холсте минус её отступы экрана.
	auto InnerOf = [](const UBorder* Plate)
	{
		const UCanvasPanelSlot* PlateSlot = Plate ? Cast<UCanvasPanelSlot>(Plate->Slot) : nullptr;
		const FVector2D Size = PlateSlot ? PlateSlot->GetSize() : FVector2D::ZeroVector;
		const FMargin Pad = Plate ? Plate->GetPadding() : FMargin(0.0f);
		return FVector2D(Size.X - Pad.Left - Pad.Right, Size.Y - Pad.Top - Pad.Bottom);
	};

	// Обе вкладки дерева: всё внутри экрана смартфона, подписи на кнопках помещаются, значки
	// у заголовка не налезают на него. bShape — ещё и форма раскладки по умолчанию.
	auto CheckTree = [this, Settings, &InnerOf](const UWidgetTree* Tree, const TCHAR* What, bool bShape)
	{
		const UBorder* Plate = Cast<UBorder>(Tree->FindWidget(TEXT("PanelPlate")));
		const UWidget* Canvas = Plate ? Plate->GetContent() : nullptr;
		const UWidget* SideMenu = Tree->FindWidget(TEXT("SideMenu"));
		const UWidget* QuestsPanel = Tree->FindWidget(TEXT("QuestsPanel"));
		const UWidget* MapPanel = Tree->FindWidget(TEXT("MapPanel"));
		if (!TestTrue(*FString::Printf(TEXT("%s: подложка, её холст, меню вкладок и обе вкладки на месте"), What),
			Plate && Canvas && SideMenu && QuestsPanel && MapPanel))
		{
			return;
		}
		TestTrue(*FString::Printf(TEXT("%s: поле карты обрезает всё, что выходит за него"), What),
			MapPanel->GetClipping() == EWidgetClipping::ClipToBounds);
		const FVector2D Inner = InnerOf(Plate);
		TestTrue(*FString::Printf(TEXT("%s: экран смартфона %.0f на %.0f не меньше 600 на 400"), What, Inner.X, Inner.Y),
			Inner.X > 600.0 && Inner.Y > 400.0);
		const FBox2D InnerBox(FVector2D::ZeroVector, Inner);

		FBox2D MenuOnTab[2] = { FBox2D(ForceInit), FBox2D(ForceInit) };
		for (const bool bMapTab : { false, true })
		{
			const TCHAR* TabName = bMapTab ? TEXT("«Карта»") : TEXT("«Задания»");
			TArray<FPlaced> Placed;
			Collect(Canvas, InnerBox, bMapTab ? QuestsPanel : MapPanel, Placed);
			TestTrue(*FString::Printf(TEXT("%s, вкладка %s: элементов в окне не меньше десяти (нашлось %d)"), What, TabName, Placed.Num()),
				Placed.Num() >= 10);

			FBox2D MenuBox(ForceInit), TabBox(ForceInit), TitleBox(ForceInit), SignalBox(ForceInit);
			const UTextBlock* Title = nullptr;
			int32 ButtonCount = 0;
			for (const FPlaced& Item : Placed)
			{
				const FBox2D& Box = Item.Box;
				// Подсказка карты на вкладке «Задания» спрятана — её место там не проверяется.
				if (!bMapTab && Item.Widget->GetFName() == FName(TEXT("MapHintText")))
				{
					continue;
				}
				TestTrue(*FString::Printf(TEXT("%s, вкладка %s: %s внутри экрана смартфона — (%.0f, %.0f) — (%.0f, %.0f) при экране %.0f на %.0f"),
					What, TabName, *Item.Widget->GetName(), Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y, Inner.X, Inner.Y),
					Box.Min.X >= -0.5 && Box.Min.Y >= -0.5 && Box.Max.X <= Inner.X + 0.5 && Box.Max.Y <= Inner.Y + 0.5);
				if (Item.Widget == SideMenu)
				{
					MenuBox = Box;
				}
				if (Item.Widget == (bMapTab ? MapPanel : QuestsPanel))
				{
					TabBox = Box;
				}
				if (Item.Widget->GetFName() == FName(TEXT("TitleText")))
				{
					TitleBox = Box;
					Title = Cast<UTextBlock>(Item.Widget);
				}
				if (Item.Widget->GetFName() == FName(TEXT("TitleSignalIcon")))
				{
					SignalBox = Box;
				}

				// С 21.10.2026 внутри кнопки — холст: значок и подпись стоят в нём со своим местом
				// и размером. Каждая подпись — внутри кнопки, и текст помещается в её место.
				const UButton* Button = Cast<UButton>(Item.Widget);
				const UCanvasPanel* Content = Button ? Cast<UCanvasPanel>(Button->GetContent()) : nullptr;
				if (Button)
				{
					TestNotNull(*FString::Printf(TEXT("%s: внутри кнопки %s холст"), What, *Button->GetName()), Content);
				}
				if (!Content)
				{
					continue;
				}
				const FVector2D ButtonSize = Box.GetSize();
				for (const UWidget* Child : Content->GetAllChildren())
				{
					if (!Child || Child->GetVisibility() == ESlateVisibility::Collapsed)
					{
						continue;
					}
					const FBox2D ChildBox = BoxIn(Child, ButtonSize);
					TestTrue(*FString::Printf(TEXT("%s, вкладка %s: %s внутри кнопки %s — (%.0f, %.0f) — (%.0f, %.0f) при кнопке %.0f на %.0f"),
						What, TabName, *Child->GetName(), *Button->GetName(), ChildBox.Min.X, ChildBox.Min.Y, ChildBox.Max.X, ChildBox.Max.Y,
						ButtonSize.X, ButtonSize.Y),
						ChildBox.Min.X >= -0.5 && ChildBox.Min.Y >= -0.5 && ChildBox.Max.X <= ButtonSize.X + 0.5 && ChildBox.Max.Y <= ButtonSize.Y + 0.5);
					if (const UTextBlock* Caption = Cast<UTextBlock>(Child))
					{
						++ButtonCount;
						const FVector2D TextSize = EstimateTextSize(Caption);
						TestTrue(*FString::Printf(TEXT("%s, вкладка %s: надпись «%s» (около %.0f на %.0f) помещается на кнопке %s (место %.0f на %.0f)"),
							What, TabName, *Caption->GetText().ToString().Replace(TEXT("\n"), TEXT(" ")), TextSize.X, TextSize.Y,
							*Button->GetName(), ChildBox.GetSize().X, ChildBox.GetSize().Y),
							TextSize.X <= ChildBox.GetSize().X + 0.5 && TextSize.Y <= ChildBox.GetSize().Y + 0.5);
					}
				}
			}
			TestTrue(*FString::Printf(TEXT("%s, вкладка %s: кнопок с надписями не меньше четырёх (нашлось %d)"), What, TabName, ButtonCount),
				ButtonCount >= 4);

			// Значки сигнала и батареи не налезают на «Смартфон» (замечание лида 20.10 по кадру).
			if (Title && TitleBox.bIsValid && SignalBox.bIsValid)
			{
				const double TextEnd = TitleBox.Min.X + EstimateTextSize(Title).X;
				TestTrue(*FString::Printf(TEXT("%s, вкладка %s: значки у заголовка (от %.0f) правее текста «%s» (до %.0f)"),
					What, TabName, SignalBox.Min.X, *Title->GetText().ToString(), TextEnd), SignalBox.Min.X >= TextEnd);
			}
			MenuOnTab[bMapTab ? 1 : 0] = MenuBox;

			if (!bShape)
			{
				continue;
			}
			// Форма раскладки по умолчанию: весь экран смартфона занят, меню у левого края,
			// вкладка до правого, между ними один просвет.
			const double Gap = GetDefault<UPdaScreenWidget>()->SideMenuGap;
			TestTrue(*FString::Printf(TEXT("%s, вкладка %s: меню вкладок начинается у левого края и занимает всю высоту (%.0f, %.0f) — (%.0f, %.0f)"),
				What, TabName, MenuBox.Min.X, MenuBox.Min.Y, MenuBox.Max.X, MenuBox.Max.Y),
				MenuBox.bIsValid && FMath::Abs(MenuBox.Min.X) <= 0.5 && FMath::Abs(MenuBox.Min.Y) <= 0.5 && FMath::Abs(MenuBox.Max.Y - Inner.Y) <= 0.5);
			TestTrue(*FString::Printf(TEXT("%s, вкладка %s: вкладка кончается у правого края экрана смартфона и занимает всю высоту"), What, TabName),
				TabBox.bIsValid && FMath::Abs(TabBox.Max.X - Inner.X) <= 0.5 && FMath::Abs(TabBox.Min.Y) <= 0.5 && FMath::Abs(TabBox.Max.Y - Inner.Y) <= 0.5);
			TestTrue(*FString::Printf(TEXT("%s, вкладка %s: между меню (до %.0f) и вкладкой (от %.0f) ровно просвет %.0f"),
				What, TabName, MenuBox.Max.X, TabBox.Min.X, Gap), FMath::Abs(TabBox.Min.X - MenuBox.Max.X - Gap) <= 0.5);
			AddInfo(FString::Printf(TEXT("%s, вкладка %s: экран смартфона %.0f на %.0f, меню %.0f, вкладка %.0f."),
				What, TabName, Inner.X, Inner.Y, MenuBox.GetSize().X, TabBox.GetSize().X));
			// Карта не растянута и не уменьшена: поле карты ровно под карту, вписанную по высоте.
			if (bMapTab)
			{
				const FVector2D Fitted = PdaMap::FitMapSize(FVector2D(1.0e7, TabBox.GetSize().Y), Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
				TestTrue(*FString::Printf(TEXT("%s: поле карты (%.0f на %.0f) совпадает с картой, вписанной по высоте (%.0f на %.0f)"),
					What, TabBox.GetSize().X, TabBox.GetSize().Y, Fitted.X, Fitted.Y), FMath::Abs(TabBox.GetSize().X - Fitted.X) <= 2.0);
			}
		}
		// Меню одной ширины и на одном месте на обеих вкладках (замечание лида 20.10: на
		// «Заданиях» меню было узким). Его никто не двигает при смене вкладки.
		TestTrue(*FString::Printf(TEXT("%s: меню на «Заданиях» (%.0f) и на «Карте» (%.0f) одной ширины и на одном месте"),
			What, MenuOnTab[0].GetSize().X, MenuOnTab[1].GetSize().X),
			MenuOnTab[0].bIsValid && MenuOnTab[1].bIsValid && MenuOnTab[0].Min.Equals(MenuOnTab[1].Min, 0.5) && MenuOnTab[0].Max.Equals(MenuOnTab[1].Max, 0.5));
	};

	// Раскладка по умолчанию (ею генератор создаёт файл окна и строится окно без файла).
	UWidgetTree* DefaultTree = NewObject<UWidgetTree>(GetTransientPackage());
	UPdaScreenWidget::BuildDefaultTree(DefaultTree);
	CheckTree(DefaultTree, TEXT("Раскладка по умолчанию"), /*bShape=*/true);

	// Готовый файл окна, открытый как в игре.
	UClass* WindowClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	UWorld* World = Create();
	UPdaScreenWidget* Window = (World && WindowClass) ? CreateWidget<UPdaScreenWidget>(World, WindowClass) : nullptr;
	if (!TestNotNull(TEXT("Окно создано из готового файла WBP_Pda"), Window) || !Window->WidgetTree)
	{
		Destroy(World);
		return false;
	}
	Window->InitPda(nullptr);
	const UWidgetTree* Tree = Window->WidgetTree;
	const UBorder* Plate = Cast<UBorder>(Tree->FindWidget(TEXT("PanelPlate")));
	const UCanvasPanelSlot* PlateSlot = Plate ? Cast<UCanvasPanelSlot>(Plate->Slot) : nullptr;
	const FVector2D FileSize = PlateSlot ? PlateSlot->GetSize() : FVector2D::ZeroVector;

	// 16:9, телефонные 20:9 и 19,5:9, планшетное 16:10 (экран в единицах раскладки всегда 1080
	// в высоту): смартфон масштабируется целиком и целиком на экране.
	for (const double ScreenWidth : { 1920.0, 2400.0, 2340.0, 1728.0 })
	{
		const FVector2D Screen(ScreenWidth, 1080.0);
		Window->ApplyPhoneLayout(Screen);
		const FVector2D Phone = Window->GetPhoneSize();
		const double Scale = Window->GetPhoneScale();
		TestTrue(*FString::Printf(TEXT("Экран %.0f: смартфон %.0f на %.0f целиком на экране"), ScreenWidth, Phone.X, Phone.Y),
			Phone.X >= 1.0 && Phone.X <= Screen.X && Phone.Y <= Screen.Y);
		TestTrue(*FString::Printf(TEXT("Экран %.0f: смартфон — подложка из файла (%.0f на %.0f), увеличенная в %.3f раза целиком"),
			ScreenWidth, FileSize.X, FileSize.Y, Scale),
			Plate && Phone.Equals(FileSize * Scale, 0.5) && FMath::IsNearlyEqual(Plate->GetRenderTransform().Scale.X, Scale, 0.001));
		for (const bool bMapTab : { false, true })
		{
			Window->ShowTab(bMapTab);
		}
	}
	CheckTree(Window->WidgetTree, TEXT("Файл окна"), /*bShape=*/false);

	// Подпись кнопки «найти героя» (Ринат: «Замени на "моя позиция"»).
	if (const UTextBlock* FindHero = Cast<UTextBlock>(Tree->FindWidget(TEXT("MapFindHeroText"))))
	{
		TestEqual(TEXT("На кнопке «найти героя» написано «Моя позиция»"),
			FindHero->GetText().ToString().Replace(TEXT("\n"), TEXT(" ")), FString(TEXT("Моя позиция")));
	}
	else
	{
		AddError(TEXT("В окне нет подписи MapFindHeroText"));
	}

	Destroy(World);
	return true;
}

// ===========================================================================
// Торговля, обыск, личный ящик (Ринат 11.10.2026: «Виджет инвентаря заезжает на левый виджет»).
// Личный ящик открывается тем же окном, что обыск. Подложки привязаны к середине экрана, так
// что просвет один на любом экране; на 16:9 и на телефонных экранах обе подложки целиком видны.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishUiPairWindowsTest,
	"ContrarySurvivor.PolishUi.InventoryDoesNotOverlapLeftWindow", PolishUiTestFlags)

bool FPolishUiPairWindowsTest::RunTest(const FString& Parameters)
{
	using namespace PolishUiTest;
	auto PlateSlotOf = [this](const TCHAR* ClassPath, const TCHAR* What) -> const UCanvasPanelSlot*
	{
		const UWidgetTree* Tree = LoadTree(ClassPath);
		const UWidget* Plate = Tree ? Tree->FindWidget(TEXT("PanelPlate")) : nullptr;
		const UCanvasPanelSlot* PlateSlot = Plate ? Cast<UCanvasPanelSlot>(Plate->Slot) : nullptr;
		TestNotNull(*FString::Printf(TEXT("Подложка окна %s"), What), PlateSlot);
		return PlateSlot;
	};
	const UCanvasPanelSlot* Inventory = PlateSlotOf(TEXT("/Game/UI/WBP_Inventory.WBP_Inventory_C"), TEXT("сумки"));
	if (!Inventory)
	{
		return false;
	}

	struct FLeftWindow { const TCHAR* ClassPath; const TCHAR* What; };
	static const FLeftWindow LeftWindows[] =
	{
		{ TEXT("/Game/UI/WBP_Shop.WBP_Shop_C"), TEXT("торговли") },
		{ TEXT("/Game/UI/WBP_SearchWindow.WBP_SearchWindow_C"), TEXT("обыска и личного ящика") },
	};
	for (const FLeftWindow& Left : LeftWindows)
	{
		const UCanvasPanelSlot* LeftSlot = PlateSlotOf(Left.ClassPath, Left.What);
		if (!LeftSlot)
		{
			continue;
		}
		for (const double ScreenWidth : { 1920.0, 2400.0, 2340.0 })
		{
			const FVector2D Screen(ScreenWidth, 1080.0);
			const FBox2D LeftBox = BoxIn(LeftSlot->Content, Screen);
			const FBox2D InventoryBox = BoxIn(Inventory->Content, Screen);
			TestFalse(*FString::Printf(TEXT("Экран %.0f: сумка не перекрывает окно %s"), ScreenWidth, Left.What), LeftBox.Intersect(InventoryBox));
			TestTrue(*FString::Printf(TEXT("Экран %.0f: между окном %s (до %.0f) и сумкой (от %.0f) просвет не меньше 12"),
				ScreenWidth, Left.What, LeftBox.Max.X, InventoryBox.Min.X), InventoryBox.Min.X - LeftBox.Max.X >= 12.0);
			TestTrue(*FString::Printf(TEXT("Экран %.0f: окно %s и сумка целиком на экране по ширине (от %.0f до %.0f)"),
				ScreenWidth, Left.What, LeftBox.Min.X, InventoryBox.Max.X), LeftBox.Min.X >= 0.0 && InventoryBox.Max.X <= Screen.X);
		}
	}
	return true;
}

// ===========================================================================
// Цифра количества на плитке предмета (Ринат 11.10.2026: «Сделай шрифт чуть больше. Ты можешь
// добавить черную обводку этим символам?»). Плитка одна на сумку, торговлю, обыск и ящик —
// проверяется файл плитки и то, что все три окна берут именно её.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishUiStackCountTest,
	"ContrarySurvivor.PolishUi.StackCountIsBiggerAndOutlined", PolishUiTestFlags)

bool FPolishUiStackCountTest::RunTest(const FString& Parameters)
{
	using namespace PolishUiTest;
	const UWidgetTree* Tree = LoadTree(TEXT("/Game/UI/WBP_ItemTile.WBP_ItemTile_C"));
	const UTextBlock* Count = Tree ? Cast<UTextBlock>(Tree->FindWidget(TEXT("TileCountText"))) : nullptr;
	if (!TestNotNull(TEXT("Цифра количества в готовом файле плитки"), Count))
	{
		return false;
	}
	const FSlateFontInfo Font = Count->GetFont();
	TestTrue(*FString::Printf(TEXT("Кегль цифры количества вырос с 14 примерно на пятую часть (сейчас %.0f)"), static_cast<double>(Font.Size)),
		Font.Size >= 14.0f * 1.15f && Font.Size <= 14.0f * 1.3f);
	TestTrue(*FString::Printf(TEXT("У цифры количества есть обводка (толщина %d)"), Font.OutlineSettings.OutlineSize),
		Font.OutlineSettings.OutlineSize >= 1);
	const FLinearColor Outline = Font.OutlineSettings.OutlineColor;
	TestTrue(TEXT("Обводка чёрная и непрозрачная"), Outline.R <= 0.02f && Outline.G <= 0.02f && Outline.B <= 0.02f && Outline.A >= 0.9f);

	UClass* TileClass = StaticLoadClass(UItemTileWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_ItemTile.WBP_ItemTile_C"));
	TestNotNull(TEXT("Класс плитки из готового файла"), TileClass);
	for (const TCHAR* WindowPath : { TEXT("/Game/UI/WBP_Inventory.WBP_Inventory_C"), TEXT("/Game/UI/WBP_Shop.WBP_Shop_C"),
		TEXT("/Game/UI/WBP_SearchWindow.WBP_SearchWindow_C") })
	{
		UClass* WindowClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, WindowPath);
		const FClassProperty* TileProp = WindowClass ? FindFProperty<FClassProperty>(WindowClass, TEXT("TileWidgetClass")) : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("%s: поле класса плитки"), WindowPath), TileProp))
		{
			TestTrue(*FString::Printf(TEXT("%s берёт плитку из файла WBP_ItemTile"), WindowPath),
				TileProp->GetPropertyValue_InContainer(WindowClass->GetDefaultObject()) == TileClass);
		}
	}
	return true;
}

// ===========================================================================
// Две метки в мире (Ринат 11.10.2026: «Взял квест у охранника… Предлагаю сразу ставить в
// отслеживаемое этот квест. Но таким образом чтобы маркер на экране был синим, но при этом
// коричневый маркер (маркер основного квеста) оставался на экране»).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishUiSideQuestMarkerTest,
	"ContrarySurvivor.PolishUi.SideQuestGetsItsOwnBlueMarker", PolishUiTestFlags)

bool FPolishUiSideQuestMarkerTest::RunTest(const FString& Parameters)
{
	using namespace PolishUiTest;

	// --- Метка лежит в файле окна строки задания ---
	const UWidgetTree* Tree = LoadTree(TEXT("/Game/UI/WBP_QuestTracker.WBP_QuestTracker_C"));
	const UWidget* Marker = Tree ? Tree->FindWidget(TEXT("SideQuestMarker")) : nullptr;
	const UTextBlock* MarkerText = Tree ? Cast<UTextBlock>(Tree->FindWidget(TEXT("SideQuestMarkerText"))) : nullptr;
	const UImage* MarkerIcon = Tree ? Cast<UImage>(Tree->FindWidget(TEXT("SideQuestMarkerIcon"))) : nullptr;
	const UImage* Arrow = Tree ? Cast<UImage>(Tree->FindWidget(TEXT("SideQuestEdgeArrow"))) : nullptr;
	if (TestTrue(TEXT("В файле окна лежат блок метки, её значок, подпись и краевая стрелка"), Marker && MarkerText && MarkerIcon && Arrow))
	{
		TestTrue(TEXT("Значок и подпись лежат внутри блока метки"), MarkerIcon->GetParent() == Marker && MarkerText->GetParent() == Marker);
		TestNotNull(TEXT("У значка метки есть картинка"), MarkerIcon->GetBrush().GetResourceObject());
		TestNotNull(TEXT("У краевой стрелки есть картинка"), Arrow->GetBrush().GetResourceObject());
		TestTrue(TEXT("Метка и стрелка спрятаны, пока побочного задания нет"),
			Marker->GetVisibility() == ESlateVisibility::Collapsed && Arrow->GetVisibility() == ESlateVisibility::Collapsed);
		for (const FLinearColor& Color : { MarkerIcon->GetColorAndOpacity(), Arrow->GetColorAndOpacity(), MarkerText->GetColorAndOpacity().GetSpecifiedColor() })
		{
			TestTrue(*FString::Printf(TEXT("Метка побочного задания синяя (цвет %.2f, %.2f, %.2f)"), Color.R, Color.G, Color.B),
				Color.B > Color.R + 0.2f && Color.B >= Color.G);
		}
		const UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot);
		TestTrue(TEXT("Середина блока метки — точка цели"), MarkerSlot && MarkerSlot->GetAlignment().Equals(FVector2D(0.5, 0.5)));
	}
	const FLinearColor MainColor = UContraryPdaSettings::Get()->MainQuestColor;
	TestTrue(TEXT("Метка основного задания остаётся оранжевой"), MainColor.R > MainColor.B + 0.3f);

	// --- Где стоит метка на экране ---
	const FVector2D Area(2400.0, 1080.0);
	const float Margin = 56.0f;
	auto Place = [&Area, Margin](bool bInFront, const FVector2D& Point, const FVector2D& BehindDir = FVector2D(0.0, -1.0))
	{
		return UQuestTrackerWidget::ComputeMarkerPlacement(bInFront, Point, BehindDir, Area, Margin);
	};
	UQuestTrackerWidget::FMarkerPlacement Spot = Place(true, FVector2D(700.0, 400.0));
	TestTrue(TEXT("Цель в кадре: метка стоит на точке цели"), Spot.bInView && Spot.Position.Equals(FVector2D(700.0, 400.0)));
	Spot = Place(true, FVector2D(5000.0, 540.0));
	TestTrue(*FString::Printf(TEXT("Цель правее экрана: стрелка у правого края (%.0f, %.0f), смотрит вправо (%.0f°)"), Spot.Position.X, Spot.Position.Y, Spot.ArrowAngle),
		!Spot.bInView && FMath::IsNearlyEqual(Spot.Position.X, Area.X - Margin, 0.5) && FMath::IsNearlyEqual(Spot.Position.Y, 540.0, 0.5)
		&& FMath::IsNearlyEqual(Spot.ArrowAngle, 90.0f, 0.5f));
	Spot = Place(true, FVector2D(1200.0, -3000.0));
	TestTrue(TEXT("Цель выше экрана: стрелка у верхнего края, смотрит вверх"),
		!Spot.bInView && FMath::IsNearlyEqual(Spot.Position.Y, Margin, 0.5) && FMath::IsNearlyEqual(Spot.ArrowAngle, 0.0f, 0.5f));
	Spot = Place(true, FVector2D(1200.0, 5000.0));
	TestTrue(TEXT("Цель ниже экрана: стрелка у нижнего края, смотрит вниз"),
		!Spot.bInView && FMath::IsNearlyEqual(Spot.Position.Y, Area.Y - Margin, 0.5) && FMath::IsNearlyEqual(FMath::Abs(Spot.ArrowAngle), 180.0f, 0.5f));
	Spot = Place(false, FVector2D(700.0, 400.0), FVector2D(-1.0, 0.0));
	TestTrue(TEXT("Цель за спиной камеры слева: стрелка у левого края, смотрит влево"),
		!Spot.bInView && FMath::IsNearlyEqual(Spot.Position.X, Margin, 0.5) && FMath::IsNearlyEqual(Spot.ArrowAngle, -90.0f, 0.5f));
	Spot = Place(true, FVector2D(20.0, 540.0));
	TestTrue(TEXT("Цель в кадре, но у самого края: вместо метки стрелка внутри поля"), !Spot.bInView && Spot.Position.X >= Margin - 0.5);

	// --- Задание охранника — побочное ---
	TestEqual(TEXT("Задание охранника — побочное"), GetDefault<AScientistGuardNPC>()->GetOfferedQuest().Kind, EQuestKind::Side);

	// --- Журнал: основное идёт, взято побочное ---
	UWorld* World = Create();
	ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(0.0, 0.0, 100.0));
	AActor* MainTarget = Spawn<AStaticMeshActor>(World, FVector(3000.0, 0.0, 0.0));
	AActor* SideTarget = Spawn<AStaticMeshActor>(World, FVector(-2000.0, 4000.0, 50.0));
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	UClass* TrackerClass = StaticLoadClass(UQuestTrackerWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_QuestTracker.WBP_QuestTracker_C"));
	UQuestTrackerWidget* Tracker = (World && TrackerClass) ? CreateWidget<UQuestTrackerWidget>(World, TrackerClass) : nullptr;
	if (!TestTrue(TEXT("Мир, герой, цели и окно строки задания созданы"), Player && Quests && MainTarget && SideTarget && Tracker))
	{
		Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(TEXT("ContraryPolishUiMarkerQA"));
	MainTarget->Tags.Add(FName(TEXT("PolishMainTarget")));
	SideTarget->Tags.Add(FName(TEXT("PolishSideTarget")));

	FQuest Main;
	Main.QuestId = FName(TEXT("PolishMain"));
	Main.Title = FText::FromString(TEXT("QA основное"));
	Main.Kind = EQuestKind::Main;
	Main.TargetCount = 3;
	Main.KillObjectiveLabel = FText::FromString(TEXT("Перебить врагов"));
	Main.MapMarkerTag = FName(TEXT("PolishMainTarget"));
	FQuest Side = Main;
	Side.QuestId = FName(TEXT("PolishSide"));
	Side.Title = FText::FromString(TEXT("QA побочное"));
	Side.Kind = EQuestKind::Side;
	Side.TargetCount = 2;
	Side.KillObjectiveLabel = FText::FromString(TEXT("Найти рацию"));
	Side.MapMarkerTag = FName(TEXT("PolishSideTarget"));

	Quests->OfferQuest(Main);
	TestTrue(TEXT("Основное задание взято"), Quests->AcceptQuest(Main.QuestId));
	Tracker->RefreshSideQuestMarker(Player);
	TestFalse(TEXT("Пока побочного задания нет, синей метки нет"), Tracker->IsSideQuestMarkerShown());

	Quests->OfferQuest(Side);
	TestTrue(TEXT("Побочное задание взято"), Quests->AcceptQuest(Side.QuestId));
	const FQuest* Tracked = Quests->GetTrackedQuest();
	TestTrue(TEXT("Взятое побочное задание сразу отслеживается"), Tracked && Tracked->QuestId == Side.QuestId);
	const FQuest* MainPick = QuestTargets::PickMarkerQuest(Quests->GetQuests(), Tracked, EQuestKind::Main);
	TestTrue(TEXT("Оранжевая метка основного задания остаётся"), MainPick && MainPick->QuestId == Main.QuestId
		&& QuestTargets::FindTargetActor(World, *MainPick) == MainTarget);

	Tracker->RefreshSideQuestMarker(Player);
	TestTrue(TEXT("Синяя метка побочного задания показана"), Tracker->IsSideQuestMarkerShown());
	TestTrue(*FString::Printf(TEXT("Синяя метка стоит над целью побочного задания (%s)"), *Tracker->GetSideQuestMarkerPoint().ToString()),
		Tracker->GetSideQuestMarkerPoint().Equals(SideTarget->GetActorLocation() + FVector(0.0, 0.0, Tracker->SideMarkerZOffset), 0.5));
	TestEqual(TEXT("Подпись синей метки — текущая цель побочного задания"), Tracker->GetSideQuestMarkerLabel().ToString(), FString(TEXT("Найти рацию (0/2)")));

	// Игрок вернул отслеживание основному — меток по-прежнему две.
	TestTrue(TEXT("Отслеживание возвращено основному заданию"), Quests->SetTrackedQuest(Main.QuestId));
	Tracker->RefreshSideQuestMarker(Player);
	TestTrue(TEXT("При отслеживаемом основном синяя метка побочного остаётся"), Tracker->IsSideQuestMarkerShown());

	// Побочное задание из журнала ушло (сдано) — синяя метка убирается.
	FQuest MainActive = Main;
	MainActive.State = EQuestState::Active;
	FQuest SideDone = Side;
	SideDone.State = EQuestState::TurnedIn;
	Quests->RestoreQuests({ MainActive, SideDone }, Main.QuestId);
	Tracker->RefreshSideQuestMarker(Player);
	TestFalse(TEXT("После сдачи побочного синяя метка убрана"), Tracker->IsSideQuestMarkerShown());

	Destroy(World);
	DeleteSlot(TEXT("ContraryPolishUiMarkerQA"));
	return true;
}

// ===========================================================================
// Район поиска южного логова (Ринат 11.10.2026: «Я отметил на твоей карте». После диалога
// начинает мигать иконка смартфона. А в смартфоне иконка карты… на карте зеленым кругом
// отображается примерный район нахождения логова волков (южного от деревни) и подпись…»;
// «По приходу игрока в логово и (или) по прошествии 3 минут пропадает и отметка района поиска
// (зеленый круг) и подпись»). Функцию, которую позовёт разговор со старостой, проверка зовёт
// напрямую. Мигание — движение цвета в кадре окна; здесь проверяется, когда оно включено.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishUiSearchAreaTest,
	"ContrarySurvivor.PolishUi.SouthDenAreaIsMarkedBlinksExpiresAndSurvivesSave", PolishUiTestFlags)

bool FPolishUiSearchAreaTest::RunTest(const FString& Parameters)
{
	using namespace PolishUiTest;
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FString Slot(TEXT("ContraryPolishUiAreaQA"));
	const FString Phrase(TEXT("Ещё одно логово волков, староста предлагает отправиться туда, раздобыть их шкуры. Торговец хорошо за них платит"));
	DeleteSlot(Slot);

	TestEqual(TEXT("Отметка держится три минуты"), Settings->SearchAreaSeconds, 180.0f);
	// Южное логово в настройках карты: ниже деревни по карте — значит, меньше Y мира.
	TestEqual(TEXT("Метка южного логова в настройках"), Settings->SouthWolfDenMarkerTag, FName(TEXT("WolfDen2")));

	// Подпись района лежит в файле окна дословно.
	const UWidgetTree* AssetTree = LoadTree(TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	const UTextBlock* AssetCaption = AssetTree ? Cast<UTextBlock>(AssetTree->FindWidget(TEXT("MapAreaMarkText"))) : nullptr;
	const UBorder* AssetMark = AssetTree ? Cast<UBorder>(AssetTree->FindWidget(TEXT("MapAreaMark"))) : nullptr;
	if (TestTrue(TEXT("Круг района и его подпись лежат в файле окна смартфона"), AssetCaption && AssetMark))
	{
		TestEqual(TEXT("Подпись района в файле окна — слова Рината дословно"), AssetCaption->GetText().ToString(), Phrase);
		const FLinearColor Fill = AssetMark->Background.TintColor.GetSpecifiedColor();
		TestTrue(*FString::Printf(TEXT("Круг района зелёный (цвет %.2f, %.2f, %.2f)"), Fill.R, Fill.G, Fill.B), Fill.G > Fill.R + 0.3f && Fill.G > Fill.B + 0.3f);
		TestTrue(TEXT("Круг района — круг, а не прямоугольник"),
			AssetMark->Background.DrawAs == ESlateBrushDrawType::RoundedBox
			&& AssetMark->Background.OutlineSettings.RoundingType == ESlateBrushRoundingType::HalfHeightRadius);
		TestTrue(TEXT("Круг и подпись спрятаны, пока района нет"),
			AssetMark->GetVisibility() == ESlateVisibility::Collapsed && AssetCaption->GetVisibility() == ESlateVisibility::Collapsed);
	}

	const FVector DenLocation(-6000.0, -14000.0, 0.0);
	float RemainingAtSave = 0.0f;

	// --- Сессия 1: отметка, мигания, отсчёт, сохранение ---
	{
		UWorld* World = Create();
		ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(-4000.0, -4600.0, 100.0));
		UMapExplorationComponent* Exploration = Player ? Player->GetMapExploration() : nullptr;
		if (!TestTrue(TEXT("Мир и герой с памятью о карте созданы"), World && Player && Exploration))
		{
			Destroy(World);
			return false;
		}
		Player->UseTestSaveSlot(Slot);

		TestFalse(TEXT("Сначала района на карте нет"), Exploration->IsSearchAreaMarked());
		// Отказ пишет в журнал предупреждение — оно здесь ожидаемое.
		AddExpectedError(TEXT("map search area NOT marked"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Логова на уровне нет — отметить нечего"), Exploration->MarkSouthWolfDenArea());
		TestFalse(TEXT("Неудачная попытка района не ставит"), Exploration->IsSearchAreaMarked());

		// Два логова: восточное и южное. Отмечается то, чья метка записана в настройках.
		AMasterEnemyBase* EastDen = Spawn<AMasterEnemyBase>(World, FVector(13900.0, -6000.0, 0.0));
		AMasterEnemyBase* SouthDen = Spawn<AMasterEnemyBase>(World, DenLocation);
		const FNameProperty* MarkerProp = FindFProperty<FNameProperty>(AMasterEnemyBase::StaticClass(), TEXT("QuestMarkerTag"));
		if (!TestTrue(TEXT("Два логова заспавнены, поле метки найдено"), EastDen && SouthDen && MarkerProp))
		{
			Destroy(World);
			return false;
		}
		MarkerProp->SetPropertyValue_InContainer(EastDen, FName(TEXT("WolfDen")));
		MarkerProp->SetPropertyValue_InContainer(SouthDen, Settings->SouthWolfDenMarkerTag);

		TestTrue(TEXT("Район южного логова отмечен"), Exploration->MarkSouthWolfDenArea());
		TestTrue(TEXT("Район на карте есть"), Exploration->IsSearchAreaMarked());
		TestTrue(*FString::Printf(TEXT("Середина района — южное логово (%s)"), *Exploration->GetSearchAreaCenter().ToString()),
			Exploration->GetSearchAreaCenter().Equals(FVector2D(DenLocation.X, DenLocation.Y), 0.5));
		TestEqual(TEXT("Отсчёт начат с трёх минут"), Exploration->GetSearchAreaRemaining(), 180.0f);
		TestTrue(TEXT("После разговора мигает кнопка смартфона"), Exploration->IsPhoneAttentionOn());
		TestTrue(TEXT("В смартфоне мигает вкладка «Карта»"), Exploration->IsMapTabAttentionOn());

		// Игрок открывает смартфон: открывается вкладка «Задания».
		UClass* WindowClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
		UPdaScreenWidget* Window = WindowClass ? CreateWidget<UPdaScreenWidget>(World, WindowClass) : nullptr;
		if (TestNotNull(TEXT("Окно смартфона создано из готового файла"), Window))
		{
			Window->InitPda(Player);
			TestFalse(TEXT("Смартфон открыт на вкладке «Задания»"), Window->IsMapTabShown());
			TestFalse(TEXT("Смартфон открыт — кнопка на экране больше не мигает"), Exploration->IsPhoneAttentionOn());
			TestTrue(TEXT("Вкладка «Карта» ещё мигает: игрок на неё не переходил"), Exploration->IsMapTabAttentionOn());
			TestTrue(TEXT("Круг района и подпись показаны на карте"), Window->IsSearchAreaMarkShown());

			const UBorder* Mark = Cast<UBorder>(Window->WidgetTree->FindWidget(TEXT("MapAreaMark")));
			const UTextBlock* Caption = Cast<UTextBlock>(Window->WidgetTree->FindWidget(TEXT("MapAreaMarkText")));
			if (TestTrue(TEXT("Круг и подпись есть в окне"), Mark && Caption))
			{
				TestTrue(TEXT("Круг и подпись видны"), Mark->GetVisibility() == ESlateVisibility::HitTestInvisible
					&& Caption->GetVisibility() == ESlateVisibility::HitTestInvisible);
				TestEqual(TEXT("Подпись в окне — слова Рината дословно"), Caption->GetText().ToString(), Phrase);
				const FVector2D UV = PdaMap::WorldToMapUV(FVector2D(DenLocation.X, DenLocation.Y), Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
				const FVector2D VillageUV = PdaMap::WorldToMapUV(Settings->VillageCenterFallback, Settings->MapWorldTopLeft, Settings->MapWorldBottomRight);
				TestTrue(*FString::Printf(TEXT("Район на карте ниже деревни, то есть южнее (%.3f против %.3f)"), UV.Y, VillageUV.Y), UV.Y > VillageUV.Y + 0.1);
				for (const UWidget* Element : { static_cast<const UWidget*>(Mark), static_cast<const UWidget*>(Caption) })
				{
					const UCanvasPanelSlot* ElementSlot = Cast<UCanvasPanelSlot>(Element->Slot);
					TestTrue(*FString::Printf(TEXT("%s стоит на месте логова на карте"), *Element->GetName()),
						ElementSlot && FVector2D(ElementSlot->GetAnchors().Minimum).Equals(UV, 0.001) && FVector2D(ElementSlot->GetAnchors().Maximum).Equals(UV, 0.001));
				}
			}

			Window->ShowTab(true);
			TestFalse(TEXT("Игрок на вкладке «Карта» — она больше не мигает"), Exploration->IsMapTabAttentionOn());
			TestTrue(TEXT("Район остаётся на карте"), Exploration->IsSearchAreaMarked());
		}

		// Прошло сто секунд игры — район ещё на месте.
		Exploration->AdvanceSearchArea(100.0f);
		TestTrue(TEXT("Через сто секунд район ещё отмечен"), Exploration->IsSearchAreaMarked());
		TestEqual(TEXT("Осталось восемьдесят секунд"), Exploration->GetSearchAreaRemaining(), 80.0f);
		RemainingAtSave = Exploration->GetSearchAreaRemaining();
		TestTrue(TEXT("Сохранение сессии 1 прошло"), Player->SaveGame());
		Destroy(World);
	}

	// --- Сессия 2: выход из игры район не снял; время вышло — снят ---
	{
		UWorld* World = Create();
		ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(-4000.0, -4600.0, 100.0));
		UMapExplorationComponent* Exploration = Player ? Player->GetMapExploration() : nullptr;
		if (TestTrue(TEXT("Мир и герой сессии 2 созданы"), World && Player && Exploration))
		{
			Player->UseTestSaveSlot(Slot);
			TestTrue(TEXT("«Продолжить» сессии 2 отработал"), Player->LoadGameForContinue());
			TestTrue(TEXT("После выхода из игры район на карте остался"), Exploration->IsSearchAreaMarked());
			TestEqual(TEXT("Отсчёт продолжается с записанного"), Exploration->GetSearchAreaRemaining(), RemainingAtSave);
			TestTrue(TEXT("Середина района та же"), Exploration->GetSearchAreaCenter().Equals(FVector2D(DenLocation.X, DenLocation.Y), 0.5));
			TestFalse(TEXT("Погашенные мигания после загрузки не вернулись"), Exploration->IsPhoneAttentionOn() || Exploration->IsMapTabAttentionOn());

			UClass* WindowClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
			UPdaScreenWidget* Window = WindowClass ? CreateWidget<UPdaScreenWidget>(World, WindowClass) : nullptr;
			if (Window)
			{
				Window->InitPda(Player);
				TestTrue(TEXT("После загрузки круг района на карте показан"), Window->IsSearchAreaMarkShown());
			}

			Exploration->AdvanceSearchArea(79.0f);
			TestTrue(TEXT("За секунду до срока район ещё отмечен"), Exploration->IsSearchAreaMarked());
			Exploration->AdvanceSearchArea(1.5f);
			TestFalse(TEXT("Три минуты прошли — район снят"), Exploration->IsSearchAreaMarked());
			if (Window)
			{
				Window->RefreshNow();
				TestFalse(TEXT("Круг района и подпись с карты убраны"), Window->IsSearchAreaMarkShown());
				const UWidget* Mark = Window->WidgetTree->FindWidget(TEXT("MapAreaMark"));
				const UWidget* Caption = Window->WidgetTree->FindWidget(TEXT("MapAreaMarkText"));
				TestTrue(TEXT("Круг и подпись спрятаны"), Mark && Caption
					&& Mark->GetVisibility() == ESlateVisibility::Collapsed && Caption->GetVisibility() == ESlateVisibility::Collapsed);
			}

			// Район отмечен снова, герой идёт к логову: вдали район держится, у логова снимается.
			Exploration->MarkSearchArea(FVector2D(DenLocation.X, DenLocation.Y), Settings->SearchAreaSeconds);
			Exploration->AdvanceSearchArea(0.25f);
			TestTrue(TEXT("Герой далеко от логова — район отмечен"), Exploration->IsSearchAreaMarked());
			Player->SetActorLocation(DenLocation + FVector(Settings->SearchAreaArrivalRadius + 500.0, 0.0, 100.0));
			Exploration->AdvanceSearchArea(0.25f);
			TestTrue(TEXT("Герой рядом, но ещё не дошёл — район отмечен"), Exploration->IsSearchAreaMarked());
			Player->SetActorLocation(DenLocation + FVector(Settings->SearchAreaArrivalRadius - 300.0, 0.0, 100.0));
			Exploration->AdvanceSearchArea(0.25f);
			TestFalse(TEXT("Герой пришёл к логову — район снят"), Exploration->IsSearchAreaMarked());
			TestFalse(TEXT("Вместе с районом сняты мигания"), Exploration->IsPhoneAttentionOn() || Exploration->IsMapTabAttentionOn());

			// Новая игра района не наследует.
			Exploration->MarkSearchArea(FVector2D(DenLocation.X, DenLocation.Y), Settings->SearchAreaSeconds);
			Exploration->ResetToVillage();
			TestFalse(TEXT("Новая игра: района на карте нет"), Exploration->IsSearchAreaMarked());
		}
		Destroy(World);
	}

	DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// Разговор со старостой после сдачи ноутбука включает отметку на карте: шаг, который
// контроллер делает при закрытии того разговора (CloseDialog -> NotifyElderMarkedSouthDen),
// ставит район южного логова и сообщает подписчикам.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPolishUiElderDialogMarksAreaTest,
	"ContrarySurvivor.PolishUi.ElderDialogAfterNotebookMarksSouthDenOnMap", PolishUiTestFlags)

bool FPolishUiElderDialogMarksAreaTest::RunTest(const FString& Parameters)
{
	using namespace PolishUiTest;
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	const FString Slot(TEXT("ContraryPolishUiElderMarkQA"));
	DeleteSlot(Slot);

	UWorld* World = Create();
	ASaveTestPlayerCharacter* Player = Spawn<ASaveTestPlayerCharacter>(World, FVector(-4000.0, -4600.0, 100.0));
	AContrarySurvivorPlayerController* PC = Spawn<AContrarySurvivorPlayerController>(World, FVector::ZeroVector);
	AMasterEnemyBase* SouthDen = Spawn<AMasterEnemyBase>(World, FVector(-6000.0, -14000.0, 0.0));
	const FNameProperty* MarkerProp = FindFProperty<FNameProperty>(AMasterEnemyBase::StaticClass(), TEXT("QuestMarkerTag"));
	UMapExplorationComponent* Exploration = Player ? Player->GetMapExploration() : nullptr;
	if (!TestTrue(TEXT("Мир, герой, контроллер и южное логово созданы"), World && Player && PC && SouthDen && MarkerProp && Exploration))
	{
		Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(Slot);
	PC->Possess(Player);
	MarkerProp->SetPropertyValue_InContainer(SouthDen, Settings->SouthWolfDenMarkerTag);

	int32 Fired = 0;
	PC->OnElderMarkedSouthDen.AddLambda([&Fired]() { ++Fired; });
	TestFalse(TEXT("До разговора района на карте нет"), Exploration->IsSearchAreaMarked());

	PC->NotifyElderMarkedSouthDen();
	TestEqual(TEXT("Событие «староста отметил логово» сработало один раз"), Fired, 1);
	TestTrue(TEXT("После разговора район южного логова отмечен на карте"), Exploration->IsSearchAreaMarked());
	TestTrue(TEXT("Середина района — южное логово"), Exploration->GetSearchAreaCenter().Equals(FVector2D(-6000.0, -14000.0), 0.5));
	TestTrue(TEXT("Кнопка смартфона и вкладка «Карта» мигают"), Exploration->IsPhoneAttentionOn() && Exploration->IsMapTabAttentionOn());

	Destroy(World);
	DeleteSlot(Slot);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
