# Дамп квестов ЭКЗЕМПЛЯРА старосты на боевой карте L_World_C (сверка с BP/C++ дефолтами).
import unreal

out_path = r"E:/ContrarySurvior/ContrarySurvivor/Saved/elder_instance_dump.txt"
lines = []

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_World_C")
world = unreal.EditorLevelLibrary.get_editor_world()
lines.append("map = %s" % world.get_name())

cls = unreal.load_class(None, "/Script/ContrarySurvivor.ElderNPC")
actors = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
lines.append("elder_count = %d" % len(actors))

for a in actors:
    lines.append("actor = %s (class %s)" % (a.get_name(), a.get_class().get_name()))
    for prop in ("offered_quest", "second_quest"):
        q = a.get_editor_property(prop)
        lines.append("=== %s.%s ===" % (a.get_name(), prop))
        for field in ("quest_id", "title", "kill_target_tag", "target_count",
                      "required_item_name", "required_item_count",
                      "map_marker_tag", "reward_money"):
            try:
                lines.append("  %s = %r" % (field, str(q.get_editor_property(field))))
            except Exception:
                lines.append("  %s = <нет такого поля>" % field)

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log("ELDER_INSTANCE_DUMP: %d строк -> %s" % (len(lines), out_path))
