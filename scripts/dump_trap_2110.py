# Дамп для разбора «заперт на базе учёных» (21.10): костры, шлагбаумы, охранник, старт, базы врагов.
import unreal

out_path = r"E:/ContrarySurvior/ws-pda/logs/trap_level_dump_2110.txt"
lines = []

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_World_C")
world = unreal.EditorLevelLibrary.get_editor_world()
lines.append("map = %s" % world.get_name())

def dump(class_path, extra=()):
    cls = unreal.load_class(None, class_path)
    if cls is None:
        lines.append("--- %s: класса нет" % class_path)
        return
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    lines.append("--- %s: %d" % (class_path, len(actors)))
    for a in actors:
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        fwd = a.get_actor_forward_vector()
        s = "  %s [%s] x=%.0f y=%.0f z=%.0f yaw=%.1f fwd=(%.2f,%.2f)" % (
            a.get_actor_label(), a.get_class().get_name(), loc.x, loc.y, loc.z, rot.yaw, fwd.x, fwd.y)
        for p in extra:
            try:
                s += " %s=%s" % (p, str(a.get_editor_property(p)))
            except Exception:
                s += " %s=<нет>" % p
        lines.append(s)

dump("/Script/ContrarySurvivor.Campfire", ("is_save_point", "safe_zone_radius"))
dump("/Script/ContrarySurvivor.QuestBarrier", ("open_when_taken_quest_id", "open_when_turned_in_quest_id", "passage_width"))
dump("/Script/ContrarySurvivor.ScientistGuardNPC")
dump("/Script/ContrarySurvivor.TalkerNPC")
dump("/Script/ContrarySurvivor.MasterEnemyBase", ("quest_marker_tag",))
dump("/Script/ContrarySurvivor.QuestItemSpawner")
dump("/Script/Engine.PlayerStart")

cls = unreal.load_class(None, "/Script/ContrarySurvivor.ElderNPC")
for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls):
    for prop in ("offered_quest", "second_quest"):
        try:
            q = a.get_editor_property(prop)
            lines.append("elder.%s id=%s kill_tag=%s marker=%s" % (prop, q.get_editor_property("quest_id"),
                q.get_editor_property("kill_target_tag"), q.get_editor_property("map_marker_tag")))
        except Exception as e:
            lines.append("elder.%s <%s>" % (prop, e))

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log("TRAP_DUMP: %d строк -> %s" % (len(lines), out_path))
