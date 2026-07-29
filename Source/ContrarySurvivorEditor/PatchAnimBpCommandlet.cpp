// Fill out your copyright notice in the Description page of Project Settings.

#include "PatchAnimBpCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimData/BoneMaskFilter.h" // FBranchFilter — фильтр по кости C_Spine01
#include "Animation/AnimSequence.h"      // поза прицеливания как ассет анимации
#include "Animation/Skeleton.h"          // FAnimSlotGroup::DefaultSlotName — стандартное имя слота
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "ContrarySurvivor/Animation/HumanoidAnimInstance.h" // переменная силы наложения позы
#include "K2Node_VariableGet.h"
#include "AnimationGraphSchema.h"        // UAnimationGraphSchema::IsLocalSpacePosePin
#include "EdGraph/EdGraph.h"             // FGraphNodeCreator
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"            // UEdGraphSchema_K2::GN_AnimGraph — имя графа анимации
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h" // отчёт сборщика блюпринта — для проверки -verify
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"   // EMessageSeverity — отделить ошибки от предупреждений
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogPatchAnimBp, Log, All);

namespace
{
	// Правится один конкретный ассет — общий анимационный блюпринт гуманоидов.
	const TCHAR* GAnimBpPackage = TEXT("/Game/Characters/Shared/Humanoid/ABP_HumanoidCharacter");
	const TCHAR* GAnimBpObjectPath = TEXT("/Game/Characters/Shared/Humanoid/ABP_HumanoidCharacter.ABP_HumanoidCharacter");

	UAnimBlueprint* LoadHumanoidAnimBp()
	{
		UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, GAnimBpObjectPath);
		if (!AnimBP)
		{
			UE_LOG(LogPatchAnimBp, Error, TEXT("Не удалось загрузить анимационный блюпринт %s."), GAnimBpObjectPath);
		}
		return AnimBP;
	}

	// Главный граф анимации блюпринта. Ищем по каноническому имени графа
	// (UEdGraphSchema_K2::GN_AnimGraph = «AnimGraph»): GetAllGraphs отдаёт ВСЕ графы, включая
	// подграфы машины состояний и переходов, а нам нужен только верхний.
	UEdGraph* FindAnimGraph(const UAnimBlueprint* AnimBP)
	{
		TArray<UEdGraph*> AllGraphs;
		AnimBP->GetAllGraphs(AllGraphs);
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				return Graph;
			}
		}
		UE_LOG(LogPatchAnimBp, Error, TEXT("В блюпринте нет графа с именем '%s' (всего графов: %d)."),
			*UEdGraphSchema_K2::GN_AnimGraph.ToString(), AllGraphs.Num());
		return nullptr;
	}

	// Пин позы (локальное пространство) заданного направления. Ищем ПО ТИПУ, а не по имени:
	// имена пинов узлов анимации берутся из имён полей структуры узла и меняются от узла к узлу,
	// а тип позы един (UAnimationGraphSchema::IsLocalSpacePosePin, AnimationGraphSchema.h:101).
	UEdGraphPin* FindPosePin(const UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	// Читаемое имя узла для логов: класс + заголовок, как он подписан в графе.
	FString DescribeNode(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("(нет узла)");
		}
		return FString::Printf(TEXT("%s «%s»"),
			*Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	}

	// Узел, чей выход позы воткнут в Output Pose, плюс сам пин. Возвращает false, если граф
	// не в ожидаемом виде (нет корня, нет входного пина, вход пуст или ветвится).
	bool FindPoseFeedingRoot(UEdGraph* AnimGraph, UAnimGraphNode_Root*& OutRoot,
		UEdGraphPin*& OutRootPosePin, UEdGraphPin*& OutSourcePin)
	{
		// Корень ищем сами, а НЕ через FBlueprintEditorUtils::GetAnimGraphRoot: там стоит
		// check(Roots.Num() == 1) (BlueprintEditorUtils.cpp:4269), то есть при неожиданном
		// содержимом графа коммандлет упал бы вместо внятного сообщения.
		TArray<UAnimGraphNode_Root*> Roots;
		AnimGraph->GetNodesOfClass<UAnimGraphNode_Root>(Roots);
		if (Roots.Num() != 1 || !Roots[0])
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("В графе анимации узлов Output Pose: %d (ожидался ровно один) — НЕ трогаю ассет."),
				Roots.Num());
			return false;
		}
		OutRoot = Roots[0];

		OutRootPosePin = FindPosePin(OutRoot, EGPD_Input);
		if (!OutRootPosePin)
		{
			UE_LOG(LogPatchAnimBp, Error, TEXT("У узла Output Pose нет входного пина позы."));
			return false;
		}

		if (OutRootPosePin->LinkedTo.Num() != 1)
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("Во вход Output Pose воткнуто связей: %d (ожидалась ровно одна). Граф не в том виде, ")
				TEXT("который я умею править, — НЕ трогаю ассет."),
				OutRootPosePin->LinkedTo.Num());
			return false;
		}

		OutSourcePin = OutRootPosePin->LinkedTo[0];
		return OutSourcePin != nullptr;
	}
}

int32 UPatchAnimBpCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Switches.Contains(TEXT("verify")))
	{
		return VerifyMontageSlot();
	}
	if (Switches.Contains(TEXT("dump")))
	{
		return DumpAnimGraph();
	}
	if (Switches.Contains(TEXT("aim")))
	{
		return InsertAimPoseLayer();
	}
	return InsertMontageSlot();
}

int32 UPatchAnimBpCommandlet::InsertMontageSlot()
{
	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}

	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	UAnimGraphNode_Root* Root = nullptr;
	UEdGraphPin* RootPosePin = nullptr;
	UEdGraphPin* SourcePin = nullptr;
	if (!FindPoseFeedingRoot(AnimGraph, Root, RootPosePin, SourcePin))
	{
		return 1;
	}

	UEdGraphNode* SourceNode = SourcePin->GetOwningNode();

	// Идемпотентность: слот уже стоит перед Output Pose — второй не плодим.
	if (const UAnimGraphNode_Slot* ExistingSlot = Cast<UAnimGraphNode_Slot>(SourceNode))
	{
		if (ExistingSlot->Node.SlotName == FAnimSlotGroup::DefaultSlotName)
		{
			UE_LOG(LogPatchAnimBp, Display,
				TEXT("Слот монтажа '%s' уже стоит перед Output Pose — делать нечего, ассет не трогаю."),
				*ExistingSlot->Node.SlotName.ToString());
		}
		else
		{
			UE_LOG(LogPatchAnimBp, Warning,
				TEXT("Перед Output Pose уже стоит слот, но с другим именем: '%s' (ожидалось '%s'). ")
				TEXT("Ассет не трогаю — решай вручную, какое имя слота правильное."),
				*ExistingSlot->Node.SlotName.ToString(), *FAnimSlotGroup::DefaultSlotName.ToString());
		}
		return 0;
	}

	UE_LOG(LogPatchAnimBp, Display, TEXT("Сейчас в Output Pose воткнут: %s. Вставляю слот монтажа между ними."),
		*DescribeNode(SourceNode));

	// Создаём узел слота штатным помощником (EdGraph.h:273): Finalize() выдаёт узлу GUID и
	// создаёт пины по структуре FAnimNode_Slot. Он же следит, что узел не забыли доделать.
	UAnimGraphNode_Slot* SlotNode = nullptr;
	{
		FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*AnimGraph);
		SlotNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		SlotNode->Node.SlotName = FAnimSlotGroup::DefaultSlotName;
		NodeCreator.Finalize();
	}

	// Раскладка: слот встаёт между источником и Output Pose. Если между ними тесно, разводим
	// узлы — это косметика, чтобы граф читался, когда его откроют мышкой.
	const int32 MinGap = 300;
	if (Root->NodePosX - SourceNode->NodePosX < 2 * MinGap)
	{
		Root->NodePosX = SourceNode->NodePosX + 2 * MinGap;
	}
	SlotNode->NodePosX = SourceNode->NodePosX + MinGap;
	SlotNode->NodePosY = Root->NodePosY;

	UEdGraphPin* SlotInputPin = FindPosePin(SlotNode, EGPD_Input);
	UEdGraphPin* SlotOutputPin = FindPosePin(SlotNode, EGPD_Output);
	if (!SlotInputPin || !SlotOutputPin)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("У созданного узла слота нет пинов позы (вход %s, выход %s)."),
			SlotInputPin ? TEXT("есть") : TEXT("НЕТ"), SlotOutputPin ? TEXT("есть") : TEXT("НЕТ"));
		return 1;
	}

	const UEdGraphSchema* Schema = AnimGraph->GetSchema();
	if (!Schema)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("У графа анимации нет схемы — связи создать нечем."));
		return 1;
	}

	// Порядок важен: сначала рвём старую связь (источник висел прямо в Output Pose), только
	// потом собираем цепочку через слот. Иначе вход Output Pose, который держит одну связь,
	// разорвал бы её сам и порядок стал бы неявным.
	RootPosePin->BreakLinkTo(SourcePin);

	// Сорваться на полпути не страшно: ниже, при любом выходе с кодом 1, ассет НЕ сохраняется,
	// а правки живут только в памяти коммандлета — файл на диске остаётся прежним.
	if (!Schema->TryCreateConnection(SourcePin, SlotInputPin))
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Не удалось соединить %s с входом слота — НЕ сохраняю, ассет на диске цел."),
			*DescribeNode(SourceNode));
		return 1;
	}
	if (!Schema->TryCreateConnection(SlotOutputPin, RootPosePin))
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Не удалось соединить выход слота с Output Pose — НЕ сохраняю, ассет на диске цел."));
		return 1;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	if (AnimBP->Status == BS_Error)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Блюпринт скомпилировался с ошибками — НЕ сохраняю, ассет на диске остаётся целым."));
		return 1;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(
		GAnimBpPackage, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(AnimBP->GetOutermost(), AnimBP, *Filename, SaveArgs))
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("SavePackage не сохранил %s."), *Filename);
		return 1;
	}

	UE_LOG(LogPatchAnimBp, Display,
		TEXT("ГОТОВО: слот монтажа '%s' вставлен между %s и Output Pose, блюпринт скомпилирован и сохранён (%s)."),
		*FAnimSlotGroup::DefaultSlotName.ToString(), *DescribeNode(SourceNode), *Filename);
	return 0;
}

int32 UPatchAnimBpCommandlet::VerifyMontageSlot()
{
	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}

	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	UAnimGraphNode_Root* Root = nullptr;
	UEdGraphPin* RootPosePin = nullptr;
	UEdGraphPin* SourcePin = nullptr;
	if (!FindPoseFeedingRoot(AnimGraph, Root, RootPosePin, SourcePin))
	{
		return 1;
	}
	UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: в Output Pose воткнут %s."),
		*DescribeNode(SourcePin->GetOwningNode()));

	// Слот ищем по всему графу, а не только прямо перед Output Pose: после режима -aim между
	// ними стоит слой послойного смешивания, и жёсткая проверка «слот сразу перед выходом»
	// начала бы врать.
	TArray<UAnimGraphNode_Slot*> SlotNodes;
	AnimGraph->GetNodesOfClass<UAnimGraphNode_Slot>(SlotNodes);
	if (SlotNodes.Num() != 1 || !SlotNodes[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("ПРОВЕРКА НЕ ПРОЙДЕНА: узлов слота монтажа в графе %d (нужен ровно один) — монтажи играть не будут."),
			SlotNodes.Num());
		return 1;
	}
	const UAnimGraphNode_Slot* SlotNode = SlotNodes[0];

	int32 Problems = 0;

	// Выход слота обязан быть куда-то воткнут, иначе монтаж посчитается и пропадёт впустую.
	const UEdGraphPin* SlotOutPin = FindPosePin(SlotNode, EGPD_Output);
	if (!SlotOutPin || SlotOutPin->LinkedTo.Num() != 1 || !SlotOutPin->LinkedTo[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("ПРОВЕРКА: выход слота никуда не воткнут — монтаж считался бы впустую."));
		++Problems;
	}
	else
	{
		UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: выход слота идёт в %s."),
			*DescribeNode(SlotOutPin->LinkedTo[0]->GetOwningNode()));
	}

	// Послойное наложение: его наличие — не ошибка, а ожидаемое состояние после -aim. Просто
	// сообщаем, что видим, чтобы по логу было понятно, в каком виде граф.
	TArray<UAnimGraphNode_LayeredBoneBlend*> BlendNodes;
	AnimGraph->GetNodesOfClass<UAnimGraphNode_LayeredBoneBlend>(BlendNodes);
	for (const UAnimGraphNode_LayeredBoneBlend* BlendNode : BlendNodes)
	{
		const bool bHasFilter = BlendNode->Node.LayerSetup.Num() > 0
			&& BlendNode->Node.LayerSetup[0].BranchFilters.Num() > 0;
		UE_LOG(LogPatchAnimBp, Display,
			TEXT("ПРОВЕРКА: слой послойного смешивания, фильтр по кости '%s', слоёв в узле %d."),
			bHasFilter ? *BlendNode->Node.LayerSetup[0].BranchFilters[0].BoneName.ToString() : TEXT("(нет фильтра!)"),
			BlendNode->Node.BlendPoses.Num());
		if (!bHasFilter)
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("ПРОВЕРКА: у слоя нет фильтра по кости — наложение пойдёт на всё тело, ноги поедут."));
			++Problems;
		}
		// Лишние слои — это НЕ безобидно: второй слой без фильтра берёт всё тело из пустой позы.
		if (BlendNode->Node.BlendPoses.Num() != 1 || BlendNode->Node.LayerSetup.Num() != 1)
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("ПРОВЕРКА: у узла смешивания поз %d и фильтров %d, а нужно по одному."),
				BlendNode->Node.BlendPoses.Num(), BlendNode->Node.LayerSetup.Num());
			++Problems;
		}
		// Пустой вход смешивания — это дыра в цепочке: с той стороны придёт поза покоя.
		for (const UEdGraphPin* Pin : BlendNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input
				&& UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType)
				&& Pin->LinkedTo.Num() != 1)
			{
				UE_LOG(LogPatchAnimBp, Error,
					TEXT("ПРОВЕРКА: во вход '%s' узла смешивания воткнуто связей %d (нужна ровно одна)."),
					*Pin->PinName.ToString(), Pin->LinkedTo.Num());
				++Problems;
			}
		}

		// Сила наложения. Слой позы прицеливания обязан брать её из переменной, слой монтажа —
		// стоять на единице. Ноль в неподключённом слое означал бы, что наложения нет вообще:
		// граф выглядит собранным, а в игре ничего не меняется. Значение читаем из СОХРАНЁННОГО
		// ассета, поэтому это проверка результата, а не намерения.
		const UEdGraphPin* WeightPin = BlendNode->FindPin(TEXT("BlendWeights_0"), EGPD_Input);
		const bool bWeightDriven = WeightPin && WeightPin->LinkedTo.Num() == 1 && WeightPin->LinkedTo[0];
		const float StoredWeight = BlendNode->Node.BlendWeights.Num() > 0 ? BlendNode->Node.BlendWeights[0] : -1.0f;
		if (bWeightDriven)
		{
			UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: силу наложения этому слою даёт %s."),
				*DescribeNode(WeightPin->LinkedTo[0]->GetOwningNode()));
		}
		else
		{
			UE_LOG(LogPatchAnimBp, Display,
				TEXT("ПРОВЕРКА: сила наложения у этого слоя постоянная: в узле %.3f, в пине «%s»."),
				StoredWeight, WeightPin ? *WeightPin->DefaultValue : TEXT("(пина нет)"));
			if (!WeightPin || FCString::Atof(*WeightPin->DefaultValue) <= 0.0f || StoredWeight <= 0.0f)
			{
				UE_LOG(LogPatchAnimBp, Error,
					TEXT("ПРОВЕРКА: сила наложения нулевая и ниоткуда не приходит — слой не работал бы."));
				++Problems;
			}
		}
	}
	UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: слоёв послойного смешивания в графе %d (0 — режим -aim ещё не запускали)."),
		BlendNodes.Num());

	if (SlotNode->Node.SlotName != FAnimSlotGroup::DefaultSlotName)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("ПРОВЕРКА: имя слота '%s', ожидалось '%s'."),
			*SlotNode->Node.SlotName.ToString(), *FAnimSlotGroup::DefaultSlotName.ToString());
		++Problems;
	}

	const UEdGraphPin* SlotInputPin = FindPosePin(SlotNode, EGPD_Input);
	if (!SlotInputPin || SlotInputPin->LinkedTo.Num() != 1 || !SlotInputPin->LinkedTo[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("ПРОВЕРКА: во вход слота воткнуто связей: %d (ожидалась ровно одна — поза ходьбы). ")
			TEXT("Пустой вход слота = персонаж замрёт, когда монтаж не играет."),
			SlotInputPin ? SlotInputPin->LinkedTo.Num() : -1);
		++Problems;
	}
	else
	{
		UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: во вход слота '%s' идёт %s."),
			*SlotNode->Node.SlotName.ToString(),
			*DescribeNode(SlotInputPin->LinkedTo[0]->GetOwningNode()));
	}

	// Последний и самый весомый шаг: собрать блюпринт заново, с чистой загрузки с диска. Связи
	// могут выглядеть целыми, а блюпринт при этом не собираться (не та поза, потерянная
	// переменная, пустой вход узла) — тогда в игре анимации просто не будет. Компилируем ТОЛЬКО
	// в памяти и НИЧЕГО не сохраняем: файл на диске эта проверка не трогает.
	UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: родитель блюпринта — %s."),
		AnimBP->ParentClass ? *AnimBP->ParentClass->GetName() : TEXT("(пусто)"));

	FCompilerResultsLog CompileResults;
	CompileResults.bSilentMode = true;
	FKismetEditorUtilities::CompileBlueprint(AnimBP, EBlueprintCompileOptions::SkipGarbageCollection, &CompileResults);
	UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: пересборка блюпринта — ошибок %d, предупреждений %d."),
		CompileResults.NumErrors, CompileResults.NumWarnings);
	for (const TSharedRef<FTokenizedMessage>& Message : CompileResults.Messages)
	{
		if (Message->GetSeverity() == EMessageSeverity::Error || Message->GetSeverity() == EMessageSeverity::Warning)
		{
			UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: сборщик сказал — %s"), *Message->ToText().ToString());
		}
	}
	if (CompileResults.NumErrors > 0 || AnimBP->Status == BS_Error)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("ПРОВЕРКА: блюпринт не собирается — в игре анимации не будет."));
		++Problems;
	}

	if (Problems > 0)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("ПРОВЕРКА НЕ ПРОЙДЕНА: замечаний %d."), Problems);
		return 1;
	}

	UE_LOG(LogPatchAnimBp, Display,
		TEXT("ПРОВЕРКА ПРОЙДЕНА: слот монтажа на месте, связи целы, блюпринт собирается."));
	return 0;
}

int32 UPatchAnimBpCommandlet::InsertAimPoseLayer()
{
	// ЗАЧЕМ ЭТОТ РЕЖИМ. В импортированных анимациях дорожки есть на ВСЕ 22 кости, включая таз,
	// корень и обе ноги (факт оператора, лог b11-anim-verify-2026-07-28). Значит любую из них,
	// проигранную полным телом, нельзя пускать поверх ходьбы: на бегу персонаж поедет с прямыми
	// ногами. Поэтому послойное наложение от кости C_Spine01 обязательно ДВАЖДЫ — и для позы
	// прицеливания, и для самих монтажей (слота).
	//
	// ЧТО СОБИРАЕТСЯ (стрелка = связь поз):
	//   ходьба -> кэш позы
	//   кэш -> [слой позы: база] , поза прицеливания -> [слой позы: слой, вес из переменной]
	//   слой позы -> слот монтажа
	//   кэш -> [слой монтажа: база] , слот монтажа -> [слой монтажа: слой, вес 1]
	//   слой монтажа -> Output Pose
	// Ноги всегда приходят из ходьбы, верх тела — из позы прицеливания, а когда играет монтаж,
	// он перекрывает верх тела собой.
	//
	// ПОЧЕМУ КЭШ ПОЗЫ, А НЕ ДВЕ СВЯЗИ ОТ ХОДЬБЫ. В графе анимации поза не может уходить в два
	// входа: схема прямо навязывает дерево и при второй связи РВЁТ первую
	// (AnimationGraphSchema.cpp:375-388, CONNECT_RESPONSE_BREAK_OTHERS_AB). Причём tryCreate
	// вернул бы true, то есть поломка была бы молчаливой. Кэш позы — штатное средство ветвления.
	static const TCHAR* AimPoseObjectPath =
		TEXT("/Game/Characters/Shared/Humanoid/Anim_AimPistol_Humanoid.Anim_AimPistol_Humanoid");
	static const FName SpineBoneName(TEXT("C_Spine01"));
	static const FName AimWeightPropertyName(TEXT("AimBlendWeight"));
	static const TCHAR* LocomotionCacheName = TEXT("LocomotionPose");

	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	// Слот монтажа должен уже стоять — его ставит основной проход без ключей.
	TArray<UAnimGraphNode_Slot*> SlotNodes;
	AnimGraph->GetNodesOfClass<UAnimGraphNode_Slot>(SlotNodes);
	if (SlotNodes.Num() != 1 || !SlotNodes[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Узлов слота монтажа в графе: %d (нужен ровно один). Сначала прогони основной ")
			TEXT("проход без ключей — он ставит слот. Ассет не трогаю."),
			SlotNodes.Num());
		return 1;
	}
	UAnimGraphNode_Slot* SlotNode = SlotNodes[0];

	// Что сейчас входит в слот (ходьба) и что выходит в Output Pose.
	UEdGraphPin* SlotInputPin = FindPosePin(SlotNode, EGPD_Input);
	if (!SlotInputPin || SlotInputPin->LinkedTo.Num() != 1 || !SlotInputPin->LinkedTo[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Во вход слота монтажа воткнуто связей: %d (ожидалась ровно одна — поза ходьбы). ")
			TEXT("Ассет не трогаю."),
			SlotInputPin ? SlotInputPin->LinkedTo.Num() : -1);
		return 1;
	}
	UEdGraphPin* LocomotionPin = SlotInputPin->LinkedTo[0];
	UEdGraphNode* LocomotionNode = LocomotionPin->GetOwningNode();

	UAnimGraphNode_Root* Root = nullptr;
	UEdGraphPin* RootPosePin = nullptr;
	UEdGraphPin* RootFeedPin = nullptr;
	if (!FindPoseFeedingRoot(AnimGraph, Root, RootPosePin, RootFeedPin))
	{
		return 1;
	}

	// Идемпотентность: слои уже стоят — второй раз не собираем.
	{
		TArray<UAnimGraphNode_LayeredBoneBlend*> ExistingBlends;
		AnimGraph->GetNodesOfClass<UAnimGraphNode_LayeredBoneBlend>(ExistingBlends);
		if (ExistingBlends.Num() > 0)
		{
			UE_LOG(LogPatchAnimBp, Display,
				TEXT("Узлов послойного смешивания в графе уже %d — считаю, что слои собраны, ассет не трогаю."),
				ExistingBlends.Num());
			return 0;
		}
	}

	// Тип именно UAnimSequence, а не общий UAnimSequenceBase: внутри SetAnimationAsset стоит
	// Cast<UAnimSequence> (AnimGraphNode_SequenceEvaluator.cpp:124-130), и на монтаже или
	// композиции присвоение прошло бы МОЛЧА мимо — узел остался бы пустым.
	UAnimSequence* AimPose = LoadObject<UAnimSequence>(nullptr, AimPoseObjectPath);
	if (!AimPose)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Поза прицеливания '%s' не найдена (или это не обычная анимация) — ассет не трогаю. ")
			TEXT("Импортирует её оператор."),
			AimPoseObjectPath);
		return 1;
	}

	// Кость фильтра и скелет проверяем ЗАРАНЕЕ. Если кости в скелете нет, послойное наложение
	// молча посчитается с нулевыми весами (FAnimationRuntime::CreateMaskWeights, AnimationRuntime.cpp:2273-2276
	// просто пропускает ненайденную кость) — граф соберётся, а работать не будет. Это самый
	// неприятный вид поломки, поэтому ловим его до правки.
	USkeleton* TargetSkeleton = AnimBP->TargetSkeleton;
	if (!TargetSkeleton)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("У анимационного блюпринта не задан скелет — ассет не трогаю."));
		return 1;
	}
	if (TargetSkeleton->GetReferenceSkeleton().FindBoneIndex(SpineBoneName) == INDEX_NONE)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("В скелете '%s' нет кости '%s' — наложение считалось бы с нулевым весом и поза ")
			TEXT("никогда бы не появилась. Ассет не трогаю."),
			*TargetSkeleton->GetName(), *SpineBoneName.ToString());
		return 1;
	}
	if (AimPose->GetSkeleton() != TargetSkeleton
		&& !TargetSkeleton->IsCompatibleForEditor(AimPose->GetSkeleton()))
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Поза прицеливания сделана на скелете '%s', а блюпринт работает со скелетом '%s', ")
			TEXT("и они несовместимы — ассет не трогаю."),
			AimPose->GetSkeleton() ? *AimPose->GetSkeleton()->GetName() : TEXT("(нет)"),
			*TargetSkeleton->GetName());
		return 1;
	}

	// Силу наложения граф читает из переменной класса-родителя. Родителя меняем ДО создания узла
	// чтения переменной: иначе переменной ещё не существует и узел повиснет.
	if (AnimBP->ParentClass != UHumanoidAnimInstance::StaticClass())
	{
		UE_LOG(LogPatchAnimBp, Display, TEXT("Меняю родителя блюпринта на UHumanoidAnimInstance (был %s)."),
			AnimBP->ParentClass ? *AnimBP->ParentClass->GetName() : TEXT("(пусто)"));
		AnimBP->ParentClass = UHumanoidAnimInstance::StaticClass();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
		FKismetEditorUtilities::CompileBlueprint(AnimBP); // пересобрать класс, чтобы переменная стала видна
		if (AnimBP->Status == BS_Error)
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("После смены родителя блюпринт не компилируется — НЕ сохраняю, ассет на диске цел."));
			return 1;
		}
	}

	// --- Создание узлов ---

	UAnimGraphNode_SaveCachedPose* CacheNode = nullptr;
	{
		FGraphNodeCreator<UAnimGraphNode_SaveCachedPose> NodeCreator(*AnimGraph);
		CacheNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		CacheNode->CacheName = LocomotionCacheName;
		NodeCreator.Finalize();
	}

	auto MakeUseCacheNode = [&](UAnimGraphNode_SaveCachedPose* Source) -> UAnimGraphNode_UseCachedPose*
	{
		UAnimGraphNode_UseCachedPose* UseNode = nullptr;
		FGraphNodeCreator<UAnimGraphNode_UseCachedPose> NodeCreator(*AnimGraph);
		UseNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		UseNode->SaveCachedPoseNode = Source; // имя кэша узел подтягивает отсюда сам
		NodeCreator.Finalize();
		return UseNode;
	};
	UAnimGraphNode_UseCachedPose* UseCacheForAim = MakeUseCacheNode(CacheNode);
	UAnimGraphNode_UseCachedPose* UseCacheForMontage = MakeUseCacheNode(CacheNode);

	UAnimGraphNode_SequenceEvaluator* PoseNode = nullptr;
	{
		FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> NodeCreator(*AnimGraph);
		PoseNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		NodeCreator.Finalize();
	}
	// Тот же путь, которым редактор кладёт анимацию в узел при перетаскивании ассета мышкой
	// (AnimationGraphSchema.cpp:634). Присвоение молчаливое — поэтому сразу перечитываем обратно.
	PoseNode->SetAnimationAsset(AimPose);
	if (PoseNode->Node.GetSequence() != AimPose)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Поза прицеливания не легла в узел (узел остался пустым) — НЕ сохраняю, ассет на диске цел."));
		return 1;
	}

	// Оба слоя устроены одинаково: база — ходьба, слой — что-то на верх тела от C_Spine01.
	//
	// ⛔ ОДИН СЛОЙ У УЗЛА УЖЕ ЕСТЬ, добавлять его руками НЕЛЬЗЯ. Конструктор редакторного узла
	// сам зовёт Node.AddPose() (AnimGraphNode_LayeredBoneBlend.cpp:19-23), а AddPose при режиме
	// BranchFilter кладёт по одной записи сразу в BlendWeights, BlendPoses и LayerSetup
	// (AnimNode_LayeredBoneBlend.h:131-144; режим BranchFilter ставит конструктор структуры,
	// там же строки 110-120). Вызов AddPinToBlendByFilter() делает ЕЩЁ ОДИН AddPose
	// (AnimGraphNode_LayeredBoneBlend.cpp:107-115) — именно на этом прошлый прогон получил два
	// слоя вместо одного. Пины слоя («BlendPoses_0», «BlendWeights_0») создаёт Finalize через
	// AllocateDefaultPins по фактической длине массивов (EdGraph.h:296-306, K2Node.cpp:1834-1841),
	// то есть к этому моменту они уже на месте, перестраивать узел не нужно.
	auto MakeBoneLayerNode = [&](const TCHAR* WhatFor) -> UAnimGraphNode_LayeredBoneBlend*
	{
		UAnimGraphNode_LayeredBoneBlend* BlendNode = nullptr;
		{
			FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
			BlendNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
			NodeCreator.Finalize();
		}
		// Предохранитель, а не действие: если движок когда-нибудь перестанет добавлять слой в
		// конструкторе, мы это увидим сообщением, а не молчаливо кривым графом.
		if (BlendNode->Node.BlendMode != ELayeredBoneBlendMode::BranchFilter
			|| BlendNode->Node.LayerSetup.Num() != 1
			|| BlendNode->Node.BlendPoses.Num() != 1
			|| BlendNode->Node.BlendWeights.Num() != 1)
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("Узел послойного смешивания (%s) сразу после создания выглядит не так, как ожидалось: ")
				TEXT("режим по веткам костей %s, фильтров %d, поз %d, весов %d (ожидалось «да» и по одному)."),
				WhatFor,
				BlendNode->Node.BlendMode == ELayeredBoneBlendMode::BranchFilter ? TEXT("да") : TEXT("нет"),
				BlendNode->Node.LayerSetup.Num(), BlendNode->Node.BlendPoses.Num(),
				BlendNode->Node.BlendWeights.Num());
			return nullptr;
		}
		FBranchFilter Filter;
		Filter.BoneName = SpineBoneName;
		Filter.BlendDepth = 0; // 0 = кость и вся ветка ниже неё берутся из накладываемой позы
		                       // (AnimationRuntime.cpp:2278 — при нулевой глубине вес сразу единичный)
		BlendNode->Node.LayerSetup[0].BranchFilters.Empty();
		BlendNode->Node.LayerSetup[0].BranchFilters.Add(Filter);
		return BlendNode;
	};
	UAnimGraphNode_LayeredBoneBlend* AimLayer = MakeBoneLayerNode(TEXT("поза прицеливания"));
	UAnimGraphNode_LayeredBoneBlend* MontageLayer = MakeBoneLayerNode(TEXT("монтажи"));
	if (!AimLayer || !MontageLayer)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("НЕ сохраняю, ассет на диске цел."));
		return 1;
	}
	// Слой монтажа всегда в полную силу: когда монтаж не играет, слот отдаёт свою входящую позу
	// без изменений, и наложение получается тождественным.
	MontageLayer->Node.BlendWeights[0] = 1.0f;

	// Переменную берём КАК СВОЮ (SetSelfMember), а не как чужого класса. Разница не косметическая:
	// при внешней ссылке VariableReference.IsSelfContext() == false, и узел чтения создаёт ВИДИМЫЙ
	// вход «Target», который остался бы висеть пустым (K2Node_Variable.cpp:151, 200-206). При
	// своей ссылке тот же вход создаётся скрытым и разрешается в сам экземпляр анимации — ровно
	// так, как если перетащить переменную в граф мышкой. Родитель блюпринта к этому моменту уже
	// UHumanoidAnimInstance, значит переменная блюпринту своя.
	UK2Node_VariableGet* WeightNode = nullptr;
	{
		FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*AnimGraph);
		WeightNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		// Ссылку ставим ДО Finalize: пины узла чтения создаются по имени переменной, при пустом
		// имени узел остался бы вообще без пинов (K2Node_VariableGet.cpp:168-181).
		WeightNode->VariableReference.SetSelfMember(AimWeightPropertyName);
		NodeCreator.Finalize();
	}

	// --- Поиск пинов. Позы ищем по типу, базу от слоя отличаем по имени свойства «BasePose»
	// (у немассивных свойств имя пина совпадает с именем поля). Вес — единственный
	// вещественный вход узла. ---
	struct FLayerPins
	{
		UEdGraphPin* Base = nullptr;
		UEdGraphPin* Layer = nullptr;
		UEdGraphPin* Weight = nullptr;
		UEdGraphPin* Out = nullptr;
	};
	auto GatherLayerPins = [](UAnimGraphNode_LayeredBoneBlend* BlendNode) -> FLayerPins
	{
		FLayerPins Pins;
		for (UEdGraphPin* Pin : BlendNode->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			if (UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType))
			{
				if (Pin->Direction == EGPD_Output)
				{
					Pins.Out = Pin;
				}
				else if (Pin->PinName == TEXT("BasePose"))
				{
					Pins.Base = Pin;
				}
				else
				{
					Pins.Layer = Pin;
				}
			}
			else if (Pin->Direction == EGPD_Input
				&& (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real
					|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float
					|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Double))
			{
				Pins.Weight = Pin;
			}
		}
		return Pins;
	};

	const FLayerPins AimPins = GatherLayerPins(AimLayer);
	const FLayerPins MontagePins = GatherLayerPins(MontageLayer);
	UEdGraphPin* CacheInPin = FindPosePin(CacheNode, EGPD_Input);
	UEdGraphPin* UseAimOutPin = FindPosePin(UseCacheForAim, EGPD_Output);
	UEdGraphPin* UseMontageOutPin = FindPosePin(UseCacheForMontage, EGPD_Output);
	UEdGraphPin* PoseOutPin = FindPosePin(PoseNode, EGPD_Output);
	UEdGraphPin* SlotOutPin = FindPosePin(SlotNode, EGPD_Output);
	UEdGraphPin* WeightOutPin = WeightNode->FindPin(AimWeightPropertyName, EGPD_Output);

	if (!AimPins.Base || !AimPins.Layer || !AimPins.Weight || !AimPins.Out
		|| !MontagePins.Base || !MontagePins.Layer || !MontagePins.Weight || !MontagePins.Out
		|| !CacheInPin || !UseAimOutPin || !UseMontageOutPin || !PoseOutPin || !SlotOutPin || !WeightOutPin)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Не нашёл нужные пины (слой позы: база %d слой %d вес %d выход %d; слой монтажа: ")
			TEXT("база %d слой %d вес %d выход %d; кэш вход %d, кэш выходы %d/%d, поза %d, слот %d, переменная %d) ")
			TEXT("— НЕ сохраняю, ассет на диске цел."),
			AimPins.Base ? 1 : 0, AimPins.Layer ? 1 : 0, AimPins.Weight ? 1 : 0, AimPins.Out ? 1 : 0,
			MontagePins.Base ? 1 : 0, MontagePins.Layer ? 1 : 0, MontagePins.Weight ? 1 : 0, MontagePins.Out ? 1 : 0,
			CacheInPin ? 1 : 0, UseAimOutPin ? 1 : 0, UseMontageOutPin ? 1 : 0,
			PoseOutPin ? 1 : 0, SlotOutPin ? 1 : 0, WeightOutPin ? 1 : 0);
		return 1;
	}

	// Вес слоя монтажа никуда не подключается, значит в собранный класс поедет ЗНАЧЕНИЕ ПИНА, а не
	// то, что лежит в структуре узла (значение пина берётся из структуры один раз, при создании
	// пина — AnimBlueprintNodeOptionalPinManager.cpp:75-81). Пишем единицу и туда, и туда, чтобы
	// два источника не разъехались.
	MontagePins.Weight->DefaultValue = TEXT("1.000000");

	// --- Раскладка: слева направо, чтобы граф читался, когда его откроют мышкой ---
	const int32 BaseX = LocomotionNode->NodePosX;
	const int32 BaseY = SlotNode->NodePosY;
	CacheNode->NodePosX = BaseX + 260;      CacheNode->NodePosY = BaseY + 220;
	UseCacheForAim->NodePosX = BaseX + 460; UseCacheForAim->NodePosY = BaseY + 120;
	PoseNode->NodePosX = BaseX + 460;       PoseNode->NodePosY = BaseY - 200;
	WeightNode->NodePosX = BaseX + 460;     WeightNode->NodePosY = BaseY - 60;
	AimLayer->NodePosX = BaseX + 760;       AimLayer->NodePosY = BaseY;
	SlotNode->NodePosX = BaseX + 1060;      SlotNode->NodePosY = BaseY;
	UseCacheForMontage->NodePosX = BaseX + 1060; UseCacheForMontage->NodePosY = BaseY + 200;
	MontageLayer->NodePosX = BaseX + 1320;  MontageLayer->NodePosY = BaseY;
	Root->NodePosX = BaseX + 1620;          Root->NodePosY = BaseY;

	const UEdGraphSchema* Schema = AnimGraph->GetSchema();
	if (!Schema)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("У графа анимации нет схемы — связи создать нечем."));
		return 1;
	}

	// Сначала рвём старые связи, потом собираем новую цепочку. Порядок важен: схема поз рвёт
	// «лишние» связи молча, и на полурасплетённом графе это дало бы неожиданный результат.
	SlotInputPin->BreakLinkTo(LocomotionPin);
	RootPosePin->BreakLinkTo(RootFeedPin);

	struct FLink { UEdGraphPin* A; UEdGraphPin* B; const TCHAR* What; };
	const FLink Links[] = {
		{ LocomotionPin,    CacheInPin,          TEXT("ходьба -> кэш позы") },
		{ UseAimOutPin,     AimPins.Base,        TEXT("кэш -> база слоя позы") },
		{ PoseOutPin,       AimPins.Layer,       TEXT("поза прицеливания -> слой позы") },
		{ WeightOutPin,     AimPins.Weight,      TEXT("сила наложения -> вес слоя позы") },
		{ AimPins.Out,      SlotInputPin,        TEXT("слой позы -> слот монтажа") },
		{ UseMontageOutPin, MontagePins.Base,    TEXT("кэш -> база слоя монтажа") },
		{ SlotOutPin,       MontagePins.Layer,   TEXT("слот монтажа -> слой монтажа") },
		{ MontagePins.Out,  RootPosePin,         TEXT("слой монтажа -> Output Pose") },
	};
	for (const FLink& Link : Links)
	{
		if (!Schema->TryCreateConnection(Link.A, Link.B))
		{
			UE_LOG(LogPatchAnimBp, Error, TEXT("Не удалось соединить: %s — НЕ сохраняю, ассет на диске цел."),
				Link.What);
			return 1;
		}
	}

	// Контроль: схема поз рвёт чужие связи МОЛЧА, поэтому после сборки перепроверяем, что каждая
	// связь действительно на месте, а не была вытеснена следующей.
	for (const FLink& Link : Links)
	{
		if (!Link.A->LinkedTo.Contains(Link.B))
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("Связь «%s» не удержалась (её вытеснила другая) — НЕ сохраняю, ассет на диске цел."),
				Link.What);
			return 1;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	if (AnimBP->Status == BS_Error)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Блюпринт скомпилировался с ошибками — НЕ сохраняю, ассет на диске остаётся целым."));
		return 1;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(
		GAnimBpPackage, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(AnimBP->GetOutermost(), AnimBP, *Filename, SaveArgs))
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("SavePackage не сохранил %s."), *Filename);
		return 1;
	}

	UE_LOG(LogPatchAnimBp, Display,
		TEXT("ГОТОВО: верх тела от кости '%s' — поза прицеливания (сила из переменной '%s') и монтажи; ")
		TEXT("ноги всегда из ходьбы (%s). Цепочка: ходьба -> кэш -> слой позы -> слот -> слой монтажа -> Output Pose. Сохранено (%s)."),
		*SpineBoneName.ToString(), *AimWeightPropertyName.ToString(), *DescribeNode(LocomotionNode), *Filename);
	return 0;
}

int32 UPatchAnimBpCommandlet::DumpAnimGraph()
{
	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);
	UE_LOG(LogPatchAnimBp, Display, TEXT("СРЕЗ %s: графов всего %d."), GAnimBpObjectPath, AllGraphs.Num());

	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	// Стабильный порядок: два прогона сравниваются диффом, поэтому сортируем по координатам
	// узла и классу, а не полагаемся на порядок в массиве.
	TArray<UEdGraphNode*> Nodes = AnimGraph->Nodes;
	Nodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
	{
		if (A.NodePosX != B.NodePosX)
		{
			return A.NodePosX < B.NodePosX;
		}
		if (A.NodePosY != B.NodePosY)
		{
			return A.NodePosY < B.NodePosY;
		}
		return A.GetClass()->GetName() < B.GetClass()->GetName();
	});

	for (const UEdGraphNode* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}
		UE_LOG(LogPatchAnimBp, Display, TEXT("УЗЕЛ x=%d y=%d %s"),
			Node->NodePosX, Node->NodePosY, *DescribeNode(Node));

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			TArray<FString> Links;
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked)
				{
					Links.Add(FString::Printf(TEXT("%s.%s"),
						*DescribeNode(Linked->GetOwningNode()), *Linked->PinName.ToString()));
				}
			}
			Links.Sort();
			UE_LOG(LogPatchAnimBp, Display, TEXT("    ПИН %s %s поза=%d -> %s"),
				Pin->Direction == EGPD_Input ? TEXT("вход ") : TEXT("выход"),
				*Pin->PinName.ToString(),
				UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType) ? 1 : 0,
				Links.Num() > 0 ? *FString::Join(Links, TEXT(", ")) : TEXT("(не подключён)"));
		}
	}

	return 0;
}
