// Fill out your copyright notice in the Description page of Project Settings.

// Новый вид окна смартфона по образцу Рината 20.10.2026: кнопки меню — плашки-картинки со
// значком слева и подписью справа, выбранная вкладка — своя яркая плашка; у заголовка значки
// сигнала и батареи и линия под ним; кнопки карты — квадратные плашки; подписи мест на карте —
// с тёмной обводкой. Пока картинок нет — запасной вид кодом (бирюзовые плашки, без значков).
//
// Живой вид здесь не проверить (Slate в прогонах не поднят) — проверяется, какие плашки и
// картинки стоят у элементов окна и как они меняются при переключении вкладок.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Components/MapExplorationComponent.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "Blueprint/UserWidget.h"
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
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

static constexpr EAutomationTestFlags PdaRestyleTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace PdaRestyleTest
{
	static const TCHAR* SaveSlot = TEXT("ContraryPdaRestyleQA");

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

	static void DeleteSlot()
	{
		if (UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
		}
	}

	// Картинки вида из настроек на время проверки; прежние возвращаются при выходе.
	struct FScopedSkinSettings
	{
		UContraryPdaSettings* Settings;
		TArray<TSoftObjectPtr<UTexture2D>> Saved;

		static TArray<TSoftObjectPtr<UTexture2D>*> Fields(UContraryPdaSettings* S)
		{
			return { &S->MenuButtonPlate, &S->MenuButtonPlateActive, &S->MapButtonPlate, &S->QuestsTabIcon,
				&S->MapTabIcon, &S->CloseIcon, &S->SignalIcon, &S->BatteryIcon };
		}

		FScopedSkinSettings()
			: Settings(GetMutableDefault<UContraryPdaSettings>())
		{
			for (TSoftObjectPtr<UTexture2D>* Field : Fields(Settings))
			{
				Saved.Add(*Field);
			}
		}

		~FScopedSkinSettings()
		{
			TArray<TSoftObjectPtr<UTexture2D>*> All = Fields(Settings);
			for (int32 Index = 0; Index < All.Num(); ++Index)
			{
				*All[Index] = Saved[Index];
			}
		}

		// Картинок ещё не импортировали: пути есть, ассетов нет.
		void SetMissing()
		{
			for (TSoftObjectPtr<UTexture2D>* Field : Fields(Settings))
			{
				*Field = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Textures/Pda/T_PdaQaMissing.T_PdaQaMissing")));
			}
		}
	};

	static UPdaScreenWidget* OpenWindow(FAutomationTestBase& Test, UWorld* World, UClass* WidgetClass, const FString& Path)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASaveTestPlayerCharacter* Player = World->SpawnActor<ASaveTestPlayerCharacter>(
			ASaveTestPlayerCharacter::StaticClass(), FVector(0.0, 0.0, 100.0), FRotator::ZeroRotator, Params);
		if (!Player)
		{
			Test.AddError(FString::Printf(TEXT("%s: герой не заспавнен"), *Path));
			return nullptr;
		}
		Player->UseTestSaveSlot(SaveSlot);
		if (UMapExplorationComponent* Exploration = Player->GetMapExploration())
		{
			Exploration->RevealAll(); // все места на карте открыты — их подписи видны
		}
		UPdaScreenWidget* Window = CreateWidget<UPdaScreenWidget>(World, WidgetClass);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s: окно создалось"), *Path), Window))
		{
			return nullptr;
		}
		Window->InitPda(Player);
		return Window;
	}

	template <typename T>
	static T* Find(UPdaScreenWidget* Window, const TCHAR* Name)
	{
		return Window && Window->WidgetTree ? Cast<T>(Window->WidgetTree->FindWidget(FName(Name))) : nullptr;
	}

	static const UObject* NormalPicture(UPdaScreenWidget* Window, const TCHAR* ButtonName)
	{
		const UButton* Button = Find<UButton>(Window, ButtonName);
		return Button ? Button->GetStyle().Normal.GetResourceObject() : nullptr;
	}

	// Плашка выбранной вкладки ходит за вкладкой: «Задания» -> «Карта» -> «Задания».
	static void CheckTabPlateFollowsTab(FAutomationTestBase& Test, UPdaScreenWidget* Window, const FString& Path)
	{
		auto Name = [&Path](const TCHAR* What) { return FString::Printf(TEXT("%s: %s"), *Path, What); };
		Test.TestFalse(*Name(TEXT("окно открыто на вкладке «Задания»")), Window->IsMapTabShown());
		Test.TestTrue(*Name(TEXT("у «Заданий» яркая плашка")), Window->IsTabPlateActive(false));
		Test.TestFalse(*Name(TEXT("у «Карты» обычная плашка")), Window->IsTabPlateActive(true));
		Window->ShowTab(true);
		Test.TestTrue(*Name(TEXT("после переключения яркая плашка у «Карты»")), Window->IsTabPlateActive(true));
		Test.TestFalse(*Name(TEXT("после переключения у «Заданий» обычная плашка")), Window->IsTabPlateActive(false));
		const UButton* Quests = Find<UButton>(Window, TEXT("QuestsTabButton"));
		const UButton* Map = Find<UButton>(Window, TEXT("MapTabButton"));
		Test.TestTrue(*Name(TEXT("плашки двух вкладок различаются")),
			Quests && Map && !(Quests->GetStyle().Normal == Map->GetStyle().Normal));
		Window->ShowTab(false);
		Test.TestTrue(*Name(TEXT("обратно: яркая плашка снова у «Заданий»")),
			Window->IsTabPlateActive(false) && !Window->IsTabPlateActive(true));
	}
}

// ===========================================================================
// 1. С картинками художника из настроек проекта: плашки, значки, линия заголовка, обводка
//    подписей мест. На обоих деревьях: построенном кодом и взятом из готового файла окна.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaRestyleWithPicturesTest,
	"ContrarySurvivor.Pda.Restyle.PlatesIconsAndTabWithPictures", PdaRestyleTestFlags)

bool FPdaRestyleWithPicturesTest::RunTest(const FString& Parameters)
{
	using namespace PdaRestyleTest;
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	UTexture2D* Plate = UPdaScreenWidget::LoadPdaPicture(Settings->MenuButtonPlate);
	UTexture2D* PlateActive = UPdaScreenWidget::LoadPdaPicture(Settings->MenuButtonPlateActive);
	UTexture2D* MapPlate = UPdaScreenWidget::LoadPdaPicture(Settings->MapButtonPlate);
	if (!TestTrue(TEXT("Картинки плашек из настроек есть в проекте"), Plate && PlateActive && MapPlate))
	{
		return false;
	}
	struct FIconCase { const TCHAR* Widget; const TSoftObjectPtr<UTexture2D>* Picture; };
	const FIconCase Icons[] =
	{
		{ TEXT("QuestsTabIcon"), &Settings->QuestsTabIcon }, { TEXT("MapTabIcon"), &Settings->MapTabIcon },
		{ TEXT("CloseIcon"), &Settings->CloseIcon }, { TEXT("TitleSignalIcon"), &Settings->SignalIcon },
		{ TEXT("TitleBatteryIcon"), &Settings->BatteryIcon },
	};

	UClass* AssetClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	TestNotNull(TEXT("Готовый файл окна загружен"), AssetClass);
	const TPair<UClass*, FString> Cases[] =
	{
		{ UPdaScreenWidget::StaticClass(), TEXT("Окно из кода") },
		{ AssetClass, TEXT("Окно из готового файла") },
	};
	for (const TPair<UClass*, FString>& Case : Cases)
	{
		if (!Case.Key)
		{
			continue;
		}
		UWorld* World = Create();
		if (!TestNotNull(TEXT("Тестовый мир создан"), World))
		{
			return false;
		}
		const FString& Path = Case.Value;
		auto Name = [&Path](const TCHAR* What) { return FString::Printf(TEXT("%s: %s"), *Path, What); };
		if (UPdaScreenWidget* Window = OpenWindow(*this, World, Case.Key, Path))
		{
			TestTrue(*Name(TEXT("выбранная вкладка — яркая плашка художника")), NormalPicture(Window, TEXT("QuestsTabButton")) == PlateActive);
			TestTrue(*Name(TEXT("вторая вкладка — обычная плашка художника")), NormalPicture(Window, TEXT("MapTabButton")) == Plate);
			TestTrue(*Name(TEXT("«Закрыть» — обычная плашка художника")), NormalPicture(Window, TEXT("CloseButton")) == Plate);
			for (const TCHAR* MapButton : { TEXT("MapZoomInButton"), TEXT("MapZoomOutButton"), TEXT("MapFindHeroButton") })
			{
				TestTrue(*Name(*FString::Printf(TEXT("%s — квадратная плашка карты"), MapButton)), NormalPicture(Window, MapButton) == MapPlate);
			}
			if (const UButton* Close = Find<UButton>(Window, TEXT("CloseButton")))
			{
				const FSlateBrush& Brush = Close->GetStyle().Normal;
				TestTrue(*Name(TEXT("плашка тянется девятью кусками с краями из настроек")),
					Brush.DrawAs == ESlateBrushDrawType::Box && Brush.Margin == Settings->MenuButtonPlateMargin);
			}
			CheckTabPlateFollowsTab(*this, Window, Path);

			for (const FIconCase& Icon : Icons)
			{
				const UImage* Image = Find<UImage>(Window, Icon.Widget);
				TestTrue(*Name(*FString::Printf(TEXT("значок %s стоит с картинкой из настроек и виден"), Icon.Widget)),
					Image && Image->GetBrush().GetResourceObject() == UPdaScreenWidget::LoadPdaPicture(*Icon.Picture)
					&& Image->GetVisibility() != ESlateVisibility::Collapsed);
			}
			// Подпись кнопки — в одной строке со значком, справа от него.
			const UTextBlock* QuestsText = Find<UTextBlock>(Window, TEXT("QuestsTabText"));
			const UImage* QuestsIcon = Find<UImage>(Window, TEXT("QuestsTabIcon"));
			TestTrue(*Name(TEXT("подпись «Задания» справа от значка в одной строке")),
				QuestsText && QuestsIcon && QuestsText->GetParent() && QuestsText->GetParent() == QuestsIcon->GetParent()
				&& QuestsText->GetParent()->GetChildIndex(QuestsIcon) < QuestsText->GetParent()->GetChildIndex(QuestsText));
			TestTrue(*Name(TEXT("подпись «Задания» крупная (не меньше 24)")), QuestsText && QuestsText->GetFont().Size >= 24);
			const UBorder* Line = Find<UBorder>(Window, TEXT("TitleUnderline"));
			TestTrue(*Name(TEXT("под заголовком тонкая линия")), Line && Line->GetVisibility() != ESlateVisibility::Collapsed);

			// Подписи мест на карте — с тёмной обводкой.
			Window->ShowTab(true);
			Window->RefreshNow();
			int32 Labels = 0;
			int32 Outlined = 0;
			if (const UCanvasPanel* Markers = Find<UCanvasPanel>(Window, TEXT("MapMarkers")))
			{
				for (const UWidget* Child : Markers->GetAllChildren())
				{
					if (const UTextBlock* Label = Cast<UTextBlock>(Child))
					{
						++Labels;
						Outlined += Label->GetFont().OutlineSettings.OutlineSize >= 1 ? 1 : 0;
					}
				}
			}
			TestTrue(*Name(*FString::Printf(TEXT("на карте есть подписи мест (%d)"), Labels)), Labels > 0);
			TestEqual(*Name(TEXT("у всех подписей мест тёмная обводка")), Outlined, Labels);
		}
		Destroy(World);
	}
	DeleteSlot();
	return true;
}

// ===========================================================================
// 2. Картинок ещё нет: запасной вид кодом, значки спрятаны, плашка выбранной вкладки ходит.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaRestyleFallbackTest,
	"ContrarySurvivor.Pda.Restyle.FallbackWithoutPictures", PdaRestyleTestFlags)

bool FPdaRestyleFallbackTest::RunTest(const FString& Parameters)
{
	using namespace PdaRestyleTest;
	FScopedSkinSettings Scoped;
	Scoped.SetMissing();
	const UContraryPdaSettings* Settings = UContraryPdaSettings::Get();
	TestNull(TEXT("Картинки нет в проекте — загрузка тихо даёт пусто"), UPdaScreenWidget::LoadPdaPicture(Settings->MenuButtonPlate));

	// Окно из кода: всё в запасном виде.
	{
		UWorld* World = Create();
		if (!TestNotNull(TEXT("Тестовый мир создан"), World))
		{
			return false;
		}
		const FString Path = TEXT("Окно из кода без картинок");
		auto Name = [&Path](const TCHAR* What) { return FString::Printf(TEXT("%s: %s"), *Path, What); };
		if (UPdaScreenWidget* Window = OpenWindow(*this, World, UPdaScreenWidget::StaticClass(), Path))
		{
			const FSlateBrush IdleMenu = UPdaScreenWidget::MakeFallbackMenuStyle(false).Normal;
			const FSlateBrush ActiveMenu = UPdaScreenWidget::MakeFallbackMenuStyle(true).Normal;
			const FSlateBrush MapPlate = UPdaScreenWidget::MakeFallbackMapStyle().Normal;
			TestTrue(*Name(TEXT("запасная плашка — скруглённая, с рамкой, без картинки")),
				IdleMenu.DrawAs == ESlateBrushDrawType::RoundedBox && IdleMenu.OutlineSettings.Width > 0.0f && !IdleMenu.GetResourceObject());
			TestFalse(*Name(TEXT("запасная яркая плашка отличается от обычной")), IdleMenu == ActiveMenu);
			const UButton* Quests = Find<UButton>(Window, TEXT("QuestsTabButton"));
			const UButton* Map = Find<UButton>(Window, TEXT("MapTabButton"));
			const UButton* Close = Find<UButton>(Window, TEXT("CloseButton"));
			TestTrue(*Name(TEXT("выбранная вкладка — запасная яркая плашка")), Quests && Quests->GetStyle().Normal == ActiveMenu);
			TestTrue(*Name(TEXT("вторая вкладка — запасная обычная плашка")), Map && Map->GetStyle().Normal == IdleMenu);
			TestTrue(*Name(TEXT("«Закрыть» — запасная обычная плашка")), Close && Close->GetStyle().Normal == IdleMenu);
			for (const TCHAR* MapButton : { TEXT("MapZoomInButton"), TEXT("MapZoomOutButton"), TEXT("MapFindHeroButton") })
			{
				const UButton* Button = Find<UButton>(Window, MapButton);
				TestTrue(*Name(*FString::Printf(TEXT("%s — запасная квадратная плашка карты"), MapButton)), Button && Button->GetStyle().Normal == MapPlate);
			}
			for (const TCHAR* IconName : { TEXT("QuestsTabIcon"), TEXT("MapTabIcon"), TEXT("CloseIcon"), TEXT("TitleSignalIcon"), TEXT("TitleBatteryIcon") })
			{
				const UImage* Icon = Find<UImage>(Window, IconName);
				TestTrue(*Name(*FString::Printf(TEXT("значок %s без картинки спрятан"), IconName)),
					Icon && !Icon->GetBrush().GetResourceObject() && Icon->GetVisibility() == ESlateVisibility::Collapsed);
			}
			CheckTabPlateFollowsTab(*this, Window, Path);
		}
		Destroy(World);
	}

	// Готовый файл окна: плашки и значки, поставленные в файле, остаются; выбранная вкладка
	// без картинки яркой плашки — запасная яркая.
	if (UClass* AssetClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C")))
	{
		UWorld* World = Create();
		if (TestNotNull(TEXT("Тестовый мир создан"), World))
		{
			const FString Path = TEXT("Окно из файла без картинок в настройках");
			if (UPdaScreenWidget* Window = OpenWindow(*this, World, AssetClass, Path))
			{
				const UButton* Quests = Find<UButton>(Window, TEXT("QuestsTabButton"));
				TestTrue(*FString::Printf(TEXT("%s: выбранная вкладка — запасная яркая плашка"), *Path),
					Quests && Quests->GetStyle().Normal == UPdaScreenWidget::MakeFallbackMenuStyle(true).Normal);
				CheckTabPlateFollowsTab(*this, Window, Path);
			}
		}
		Destroy(World);
	}
	DeleteSlot();
	return true;
}

// ===========================================================================
// 3. Картинку подложили в настройки — она заменяет запасной вид (проверочные картинки).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaRestyleSettingsPictureTest,
	"ContrarySurvivor.Pda.Restyle.PictureFromSettingsReplacesFallback", PdaRestyleTestFlags)

bool FPdaRestyleSettingsPictureTest::RunTest(const FString& Parameters)
{
	using namespace PdaRestyleTest;
	FScopedSkinSettings Scoped;
	Scoped.SetMissing();
	UContraryPdaSettings* Settings = GetMutableDefault<UContraryPdaSettings>();

	UWorld* World = Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	const FString Path = TEXT("Окно из кода");
	if (UPdaScreenWidget* Window = OpenWindow(*this, World, UPdaScreenWidget::StaticClass(), Path))
	{
		const UImage* MapIconBefore = Find<UImage>(Window, TEXT("MapTabIcon"));
		TestTrue(TEXT("Сначала картинок нет — значок «Карта» спрятан"),
			MapIconBefore && MapIconBefore->GetVisibility() == ESlateVisibility::Collapsed);

		UTexture2D* Plate = UTexture2D::CreateTransient(1048, 288, PF_B8G8R8A8);
		UTexture2D* Active = UTexture2D::CreateTransient(1048, 288, PF_B8G8R8A8);
		UTexture2D* Icon = UTexture2D::CreateTransient(128, 64, PF_B8G8R8A8);
		if (TestTrue(TEXT("Проверочные картинки созданы"), Plate && Active && Icon))
		{
			Plate->AddToRoot();
			Active->AddToRoot();
			Icon->AddToRoot();
			Settings->MenuButtonPlate = Plate;
			Settings->MenuButtonPlateActive = Active;
			Settings->MapTabIcon = Icon;
			Window->ApplySkin();

			TestTrue(TEXT("Выбранная вкладка — яркая плашка из настроек"), NormalPicture(Window, TEXT("QuestsTabButton")) == Active);
			TestTrue(TEXT("Вторая вкладка — обычная плашка из настроек"), NormalPicture(Window, TEXT("MapTabButton")) == Plate);
			if (const UButton* MapTab = Find<UButton>(Window, TEXT("MapTabButton")))
			{
				const FSlateBrush& Brush = MapTab->GetStyle().Normal;
				TestTrue(TEXT("Плашка ужата под высоту кнопки 96 с пропорциями картинки"),
					FMath::IsNearlyEqual(Brush.ImageSize.Y, 96.0, 0.01) && FMath::IsNearlyEqual(Brush.ImageSize.X, 96.0 * 1048.0 / 288.0, 0.1));
			}
			const UImage* MapIcon = Find<UImage>(Window, TEXT("MapTabIcon"));
			TestTrue(TEXT("Значок «Карта» получил картинку и виден"),
				MapIcon && MapIcon->GetBrush().GetResourceObject() == Icon && MapIcon->GetVisibility() != ESlateVisibility::Collapsed);
			TestTrue(TEXT("Высота значка — из окна (58, около 60 % кнопки), ширина — по пропорциям картинки (116)"),
				MapIcon && FMath::IsNearlyEqual(MapIcon->GetBrush().ImageSize.Y, 58.0, 0.01) && FMath::IsNearlyEqual(MapIcon->GetBrush().ImageSize.X, 116.0, 0.01));
			const UImage* QuestsIcon = Find<UImage>(Window, TEXT("QuestsTabIcon"));
			TestTrue(TEXT("Значок без картинки в настройках по-прежнему спрятан"),
				QuestsIcon && QuestsIcon->GetVisibility() == ESlateVisibility::Collapsed);
			CheckTabPlateFollowsTab(*this, Window, Path);

			// Картинки плашек убрали — снова запасной вид плашек.
			Scoped.SetMissing();
			Window->ApplySkin();
			const UButton* MapTab = Find<UButton>(Window, TEXT("MapTabButton"));
			TestTrue(TEXT("Картинку плашки убрали — снова запасная плашка"),
				MapTab && MapTab->GetStyle().Normal == UPdaScreenWidget::MakeFallbackMenuStyle(false).Normal);

			Plate->RemoveFromRoot();
			Active->RemoveFromRoot();
			Icon->RemoveFromRoot();
		}
	}
	Destroy(World);
	DeleteSlot();
	return true;
}

// ===========================================================================
// 4. Код не двигает элементы, расставленные владельцем в файле окна (Ринат 21.10.2026: «в
//    WBP_Pda я должен иметь возможность свободно двигать элементы (кнопки, тексты и т д)»).
//    Окно из готового файла; «владелец» сдвигает и растягивает меню, вкладки, кнопки, значок
//    и подпись; затем окно открывается, подгоняется под четыре экрана, переключает вкладки и
//    ставит карту. Место, размер и якоря КАЖДОГО элемента в холсте должны остаться как были.
//    Исключения — живые данные: картинка карты в поле (MapArea), круг района с подписью и
//    отметки на карте. Масштаб под экран — только у подложки целиком.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdaLayoutCodeKeepsOwnerLayoutTest,
	"ContrarySurvivor.Pda.Layout.CodeKeepsOwnerLayout", PdaRestyleTestFlags)

bool FPdaLayoutCodeKeepsOwnerLayoutTest::RunTest(const FString& Parameters)
{
	using namespace PdaRestyleTest;
	struct FSlotState
	{
		FAnchors Anchors;
		FMargin Offsets;
		FVector2D Alignment;
		bool bAutoSize;
		int32 ZOrder;
	};
	auto Snapshot = [](UWidgetTree* Tree)
	{
		TMap<FName, FSlotState> Out;
		Tree->ForEachWidget([&Out](UWidget* Widget)
		{
			const UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
			if (!CanvasSlot)
			{
				return;
			}
			const FName Name = Widget->GetFName();
			// Живые данные — их ставит код: картинка карты в поле со всем, что на ней (круг
			// района, отметки), и строки списка заданий.
			for (const UWidget* Up = Widget; Up; Up = Up->GetParent())
			{
				if (Up->GetFName() == TEXT("MapArea") || Up->GetFName() == TEXT("QuestList"))
				{
					return;
				}
			}
			Out.Add(Name, { CanvasSlot->GetAnchors(), CanvasSlot->GetOffsets(), CanvasSlot->GetAlignment(),
				CanvasSlot->GetAutoSize(), CanvasSlot->GetZOrder() });
		});
		return Out;
	};

	DeleteSlot();
	UWorld* World = Create();
	UClass* WidgetClass = StaticLoadClass(UPdaScreenWidget::StaticClass(), nullptr, TEXT("/Game/UI/WBP_Pda.WBP_Pda_C"));
	UPdaScreenWidget* Window = (World && WidgetClass) ? CreateWidget<UPdaScreenWidget>(World, WidgetClass) : nullptr;
	if (!TestNotNull(TEXT("Окно создано из готового файла WBP_Pda"), Window) || !Window->WidgetTree)
	{
		Destroy(World);
		return false;
	}

	// «Владелец» передвинул и растянул элементы (как мышкой в редакторе окна).
	struct FOwnerMove
	{
		const TCHAR* Name;
		FVector2D Pos;
		FVector2D Size;
	};
	const FOwnerMove Moves[] =
	{
		{ TEXT("SideMenu"),          FVector2D(13.0, 7.0),    FVector2D(301.0, 611.0) },
		{ TEXT("QuestsPanel"),       FVector2D(333.0, 21.0),  FVector2D(517.0, 603.0) },
		{ TEXT("MapPanel"),          FVector2D(347.0, 11.0),  FVector2D(509.0, 577.0) },
		{ TEXT("QuestsTabButton"),   FVector2D(9.0, 71.0),    FVector2D(257.0, 83.0) },
		{ TEXT("QuestsTabIcon"),     FVector2D(5.0, 3.0),     FVector2D(41.0, 39.0) },
		{ TEXT("QuestsTabText"),     FVector2D(61.0, 17.0),   FVector2D(171.0, 43.0) },
		{ TEXT("TitleSignalIcon"),   FVector2D(201.0, 5.0),   FVector2D(19.0, 17.0) },
		{ TEXT("MapZoomInButton"),   FVector2D(23.0, 29.0),   FVector2D(77.0, 71.0) },
		{ TEXT("MapFindHeroText"),   FVector2D(3.0, 4.0),     FVector2D(70.0, 50.0) },
	};
	for (const FOwnerMove& Move : Moves)
	{
		UWidget* Widget = Window->WidgetTree->FindWidget(FName(Move.Name));
		UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s лежит в холсте (его можно двигать мышкой)"), Move.Name), CanvasSlot))
		{
			continue;
		}
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetPosition(Move.Pos);
		CanvasSlot->SetSize(Move.Size);
	}
	const TMap<FName, FSlotState> Before = Snapshot(Window->WidgetTree);
	TestTrue(*FString::Printf(TEXT("Элементов в холстах не меньше тридцати (нашлось %d)"), Before.Num()), Before.Num() >= 30);

	// Окно живёт, как в игре: открыто героем, подогнано под экраны, вкладки переключаются,
	// карта ставится в поле, приближается и встаёт на героя.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASaveTestPlayerCharacter* Player = World->SpawnActor<ASaveTestPlayerCharacter>(
		ASaveTestPlayerCharacter::StaticClass(), FVector(0.0, 0.0, 100.0), FRotator::ZeroRotator, Params);
	if (Player)
	{
		Player->UseTestSaveSlot(SaveSlot);
	}
	Window->InitPda(Player);
	const UBorder* Plate = Find<UBorder>(Window, TEXT("PanelPlate"));
	for (const double ScreenWidth : { 1920.0, 2400.0, 2340.0, 1728.0 })
	{
		Window->ApplyPhoneLayout(FVector2D(ScreenWidth, 1080.0));
		TestTrue(*FString::Printf(TEXT("Экран %.0f: смартфон масштабирован целиком (масштаб подложки %.3f, подгонка %.3f)"),
			ScreenWidth, Plate ? Plate->GetRenderTransform().Scale.X : -1.0, Window->GetPhoneScale()),
			Plate && FMath::IsNearlyEqual(Plate->GetRenderTransform().Scale.X, Window->GetPhoneScale(), 0.001)
				&& FMath::IsNearlyEqual(Plate->GetRenderTransform().Scale.Y, Window->GetPhoneScale(), 0.001));
		Window->ShowTab(true);
		Window->SetMapViewportSize(FVector2D(509.0, 577.0));
		Window->ZoomMapAt(FVector2D(250.0, 280.0), 2.0);
		Window->CenterMapOnHero();
		Window->ShowTab(false);
		Window->RefreshNow();
	}
	Window->ShowTab(true);

	const TMap<FName, FSlotState> After = Snapshot(Window->WidgetTree);
	for (const TPair<FName, FSlotState>& Pair : Before)
	{
		const FSlotState* Now = After.Find(Pair.Key);
		const FSlotState& Was = Pair.Value;
		const bool bSame = Now && Now->Anchors.Minimum.Equals(Was.Anchors.Minimum) && Now->Anchors.Maximum.Equals(Was.Anchors.Maximum)
			&& Now->Offsets == Was.Offsets && Now->Alignment.Equals(Was.Alignment) && Now->bAutoSize == Was.bAutoSize
			&& Now->ZOrder == Was.ZOrder;
		TestTrue(*FString::Printf(TEXT("%s: место, размер и якоря как в файле (было (%.0f, %.0f, %.0f, %.0f), стало (%.0f, %.0f, %.0f, %.0f))"),
			*Pair.Key.ToString(), Was.Offsets.Left, Was.Offsets.Top, Was.Offsets.Right, Was.Offsets.Bottom,
			Now ? Now->Offsets.Left : -1.0f, Now ? Now->Offsets.Top : -1.0f, Now ? Now->Offsets.Right : -1.0f, Now ? Now->Offsets.Bottom : -1.0f),
			bSame);
	}

	// Внутри кнопок меню — холст со значком и подписью; вокруг элементов окна нет контейнеров,
	// которые сами расставляют детей (рядов, обёрток размера).
	for (const TCHAR* ButtonName : { TEXT("QuestsTabButton"), TEXT("MapTabButton"), TEXT("CloseButton") })
	{
		const UButton* Button = Find<UButton>(Window, ButtonName);
		TestTrue(*FString::Printf(TEXT("%s: внутри кнопки холст"), ButtonName), Button && Cast<UCanvasPanel>(Button->GetContent()) != nullptr);
	}
	int32 Arrangers = 0;
	Window->WidgetTree->ForEachWidget([&Arrangers, this](UWidget* Widget)
	{
		// Список заданий и строки в нём строит код (живые данные) — их не считаем.
		for (const UWidget* Up = Widget; Up; Up = Up->GetParent())
		{
			if (Up->GetFName() == TEXT("QuestList") || Up->GetFName() == TEXT("MapArea"))
			{
				return;
			}
		}
		const UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
		if (Panel && !Panel->IsA<UCanvasPanel>() && !Panel->IsA<UButton>() && !Panel->IsA<UBorder>())
		{
			++Arrangers;
			AddError(FString::Printf(TEXT("В окне есть раскладывающий контейнер %s (%s)"), *Widget->GetName(), *Widget->GetClass()->GetName()));
		}
	});
	TestEqual(TEXT("Раскладывающих контейнеров вокруг элементов окна нет"), Arrangers, 0);

	Destroy(World);
	DeleteSlot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
