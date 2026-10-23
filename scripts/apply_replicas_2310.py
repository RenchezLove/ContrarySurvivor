# Переносит семь реплик (23.10.2026) из РЕПЛИКИ.txt в исходники C++: старая строка -> новая, дословно.
# Аргументы: <старый РЕПЛИКИ.txt> <новый РЕПЛИКИ.txt> <корень Source/ContrarySurvivor>
import sys, re, os
KEYS = {
    "002": "Actors/ElderNPC.cpp",
    "008": "Actors/ElderNPC.cpp",
    "013": "Actors/ElderNPC.h",
    "014": "Actors/ElderNPC.h",
    "023": "Actors/ElderNPC.cpp",
    "024": "Actors/ElderNPC.cpp",
    "036": "Actors/ScientistNPCs.cpp",
}

def load(path):
    src = open(path, encoding="utf-8-sig").read()
    return {k: t for k, t in re.findall(r"^\[(\d{3}) [^\]]*\]\r?\n(.*?)\r?$", src, re.M)}

old, new = load(sys.argv[1]), load(sys.argv[2])
root = sys.argv[3]
changed = [k for k in new if old.get(k) != new[k]]
print("changed keys in file:", sorted(changed))
assert sorted(changed) == sorted(KEYS), "set of changed keys differs from the task"
for key, rel in KEYS.items():
    path = os.path.join(root, rel)
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read()
    o = old[key].replace("<br>", "\\n")
    n = new[key].replace("<br>", "\\n")
    assert '"' not in n and "\\" not in new[key]
    count = text.count('"' + o + '"')
    print(key, rel, "occurrences of old:", count)
    assert count == 1, key
    text = text.replace('"' + o + '"', '"' + n + '"')
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
print("OK")
