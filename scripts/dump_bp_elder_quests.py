# Сверка квестов: дефолты BP_Elder против C++ дефолтов AElderNPC (оба CDO).
# Пишет все поля OfferedQuest/SecondQuest обоих объектов в Saved/elder_quests_dump.txt
import unreal

out_path = r"E:/ContrarySurvior/ContrarySurvivor/Saved/elder_quests_dump.txt"
lines = []

def dump_quest(owner_label, obj):
    for prop in ("offered_quest", "second_quest"):
        try:
            q = obj.get_editor_property(prop)
        except Exception as e:
            lines.append("%s.%s: ОШИБКА ЧТЕНИЯ %s" % (owner_label, prop, e))
            continue
        lines.append("=== %s.%s ===" % (owner_label, prop))
        for field in ("quest_id", "title", "description", "quest_type",
                      "kill_target_tag", "target_count", "kill_objective_label",
                      "required_item_name", "required_item_count", "item_objective_label",
                      "map_marker_tag", "reward_money",
                      "accept_reply_text", "turn_in_reply_text", "close_reply_text"):
            try:
                v = q.get_editor_property(field)
                lines.append("  %s = %r" % (field, str(v)))
            except Exception:
                lines.append("  %s = <нет такого поля>" % field)

# C++ CDO
cpp_cls = unreal.load_class(None, "/Script/ContrarySurvivor.ElderNPC")
cpp_cdo = unreal.get_default_object(cpp_cls)
dump_quest("CPP_ElderNPC", cpp_cdo)

# BP CDO
bp_cls = unreal.load_class(None, "/Game/Characters/Elder/BP_Elder.BP_Elder_C")
bp_cdo = unreal.get_default_object(bp_cls)
dump_quest("BP_Elder", bp_cdo)

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log("ELDER_QUESTS_DUMP: %d строк -> %s" % (len(lines), out_path))
