# DT_Quests: перенос реплик [002] и [008] (23.10.2026). Выгружает таблицу в JSON (до),
# меняет внутри ячеек ровно старый текст на новый, заливает обратно, сохраняет и выгружает (после).
# Если файл-флаг apply не задан (DTQ_MODE=dump) — только выгрузка.
import unreal, json, os, re

WS = r"E:/ContrarySurvior/ws-pda"
MODE = open(WS + "/logs/dtq_mode_2310.txt", encoding="utf-8").read().strip()  # dump | apply
TABLE = "/Game/Data/DT_Quests"

def load(path):
    src = open(path, encoding="utf-8-sig").read()
    return {k: t for k, t in re.findall(r"^\[(\d{3}) [^\]]*\]\r?\n(.*?)\r?$", src, re.M)}

old = load(WS + "/logs/replicas_old_2310.txt")
new = load(WS + "/logs/replicas_new_2310.txt")
table = unreal.load_asset(TABLE)
lib = unreal.DataTableFunctionLibrary
before = lib.export_data_table_to_json_string(table)
if isinstance(before, tuple):
    before = before[-1]
tag = "after" if MODE == "check" else "before"
open(WS + "/logs/dt_quests_%s_2310.json" % tag, "w", encoding="utf-8").write(before)
report = []
if MODE == "apply":
    text = before
    for key in ("002", "008"):
        o = json.dumps(old[key], ensure_ascii=False)[1:-1]
        n = json.dumps(new[key], ensure_ascii=False)[1:-1]
        c = text.count(o)
        report.append("%s: old occurrences=%d" % (key, c))
        if c != 1:
            raise RuntimeError("DTQ: key %s old text found %d times" % (key, c))
        text = text.replace(o, n)
    ok = lib.fill_data_table_from_json_string(table, text)
    report.append("fill=%s" % ok)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)
    report.append("saved=%s" % saved)
open(WS + "/logs/dtq_report_2310_%s.txt" % MODE, "w", encoding="utf-8").write("\n".join(report) + "\n")
unreal.log("DTQ done mode=%s %s" % (MODE, "; ".join(report)))
