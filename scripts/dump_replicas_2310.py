# Выгрузка семи реплик (23.10.2026) из игры: CDO BP_Elder / BP_ScientistQuest, экземпляры на уровне
# L_World_C, ячейки DT_Quests. Сравнение с новым РЕПЛИКИ.txt. Только чтение.
import unreal, re

WS = r"E:/ContrarySurvior/ws-pda"
out_path = WS + "/logs/replicas_ingame_2310.txt"
lines = []

def load(path):
    src = open(path, encoding="utf-8-sig").read()
    return {k: t for k, t in re.findall(r"^\[(\d{3}) [^\]]*\]\r?\n(.*?)\r?$", src, re.M)}

new = load(WS + "/logs/replicas_new_2310.txt")
old = load(WS + "/logs/replicas_old_2310.txt")
results = []

def check(where, key, value):
    v = str(value).replace("\r\n", "\n").replace("\n", "<br>")
    state = "НОВЫЙ" if v == new[key] else ("СТАРЫЙ" if v == old[key] else "ДРУГОЙ")
    results.append(state)
    lines.append("[%s] %s: %s | %s" % (key, where, state, v))

def read_elder(where, obj):
    for key, prop in (("013", "third_quest_done_text"), ("014", "notebook_hint_repeat_text")):
        check(where + "." + prop, key, obj.get_editor_property(prop))
    hints = obj.get_editor_property("notebook_hint_lines")
    check(where + ".notebook_hint_lines[0]", "023", hints[0].get_editor_property("npc_text"))
    lines.append("      кнопка[0] = %s" % hints[0].get_editor_property("button_label"))
    check(where + ".notebook_hint_lines[1]", "024", hints[1].get_editor_property("npc_text"))
    lines.append("      кнопка[1] = %s" % hints[1].get_editor_property("button_label"))
    lines.append("      реплика[2] = %s" % hints[2].get_editor_property("npc_text"))
    lines.append("      кнопка[2] = %s" % hints[2].get_editor_property("button_label"))
    check(where + ".second_quest.description (запасной, если нет строки таблицы)", "002",
          obj.get_editor_property("second_quest").get_editor_property("description"))
    check(where + ".third_quest.description (запасной, если нет строки таблицы)", "008",
          obj.get_editor_property("third_quest").get_editor_property("description"))

def read_sci(where, obj):
    check(where + ".foreign_turn_in_text", "036", obj.get_editor_property("foreign_turn_in_text"))

elder_cls = unreal.load_object(None, "/Game/Characters/Elder/BP_Elder.BP_Elder_C")
sci_cls = unreal.load_object(None, "/Game/Characters/Scientist/BP_ScientistQuest.BP_ScientistQuest_C")
read_elder("BP_Elder(CDO)", unreal.get_default_object(elder_cls))
read_sci("BP_ScientistQuest(CDO)", unreal.get_default_object(sci_cls))

table = unreal.load_asset("/Game/Data/DT_Quests")
names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
descs = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "Description")
for n, d in zip(names, descs):
    m = re.search(r'NSLOCTEXT\("[^"]*",\s*"[^"]*",\s*"(.*)"\)\s*$', d, re.S)
    text = m.group(1) if m else d
    if str(n) == "ClearBanditBase":
        check("DT_Quests[ClearBanditBase].Description", "002", text)
    elif str(n) == "DeliverLaptopToScientists":
        check("DT_Quests[DeliverLaptopToScientists].Description", "008", text)

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_World_C")
world = unreal.EditorLevelLibrary.get_editor_world()
for cls, fn in ((elder_cls, read_elder), (sci_cls, read_sci)):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls):
        fn("L_World_C:" + a.get_actor_label(), a)

lines.append("ИТОГ: новых %d, старых %d, других %d" % (results.count("НОВЫЙ"), results.count("СТАРЫЙ"), results.count("ДРУГОЙ")))
open(out_path, "w", encoding="utf-8").write("\n".join(lines) + "\n")
unreal.log("REPLDUMP " + lines[-1])
