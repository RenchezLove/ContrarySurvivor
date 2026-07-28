// Fill out your copyright notice in the Description page of Project Settings.

#include "PatchAnimBpCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"          // FAnimSlotGroup::DefaultSlotName — стандартное имя слота
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AnimationGraphSchema.h"        // UAnimationGraphSchema::IsLocalSpacePosePin
#include "EdGraph/EdGraph.h"             // FGraphNodeCreator
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"            // UEdGraphSchema_K2::GN_AnimGraph — имя графа анимации
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
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

	UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
	const UAnimGraphNode_Slot* SlotNode = Cast<UAnimGraphNode_Slot>(SourceNode);
	if (!SlotNode)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("ПРОВЕРКА НЕ ПРОЙДЕНА: в Output Pose воткнут %s, а не слот монтажа — монтажи играть не будут."),
			*DescribeNode(SourceNode));
		return 1;
	}

	int32 Problems = 0;

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
		UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: цепочка целая — %s → слот '%s' → Output Pose."),
			*DescribeNode(SlotInputPin->LinkedTo[0]->GetOwningNode()),
			*SlotNode->Node.SlotName.ToString());
	}

	if (Problems > 0)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("ПРОВЕРКА НЕ ПРОЙДЕНА: замечаний %d."), Problems);
		return 1;
	}

	UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА ПРОЙДЕНА: слот монтажа на месте, связи целы."));
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
