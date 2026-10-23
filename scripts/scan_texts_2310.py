# Ищет фразы реплик в бинарных файлах Content (UTF-16LE и UTF-8). Только чтение.
import os, sys
root = sys.argv[1]
phrases = [
    "засели бандиты", "Я не справился с ноутбуком", "когда они разберутся, мы узнаем",
    "Я всё ещё сижу над ноутбуком", "Здесь написано про тебя", "Если бы я знал, я бы сказал",
    "Староста с ним не справился",
    "обосновались бандиты", "Кое-что я всё-таки вытащил", "кто такой этот Куратор",
    "кое-что уже проступает", "Вот он, тот самый ноутбук", "Пока не знаю. Ноутбук",
    "Староста открыл только малую часть",
]
pats = [(p, p.encode("utf-16-le"), p.encode("utf-8")) for p in phrases]
for dp, dn, fn in os.walk(root):
    for f in fn:
        if not f.endswith((".uasset", ".umap", ".locres", ".archive", ".manifest", ".json", ".csv")):
            continue
        path = os.path.join(dp, f)
        try:
            data = open(path, "rb").read()
        except OSError:
            continue
        for p, u16, u8 in pats:
            if u16 in data or u8 in data:
                print(path, "|", p)
