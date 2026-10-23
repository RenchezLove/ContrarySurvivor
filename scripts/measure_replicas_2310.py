# Оценка числа строк реплик в окне разговора (только чтение файла реплик).
# Окно: ширина текста ~1552, высота ~472 (WBP_Dialog, slots_pdafree_final.log), кегль 35.
import sys, re, math
src = open(sys.argv[1], encoding="utf-8-sig").read()
blocks = re.findall(r"^\[(\d{3}) [^\]]*\]\r?\n(.*?)\r?$", src, re.M)
W, H, S = 1552.0, 472.3, 35.0
LH = S * 1.6
for key, text in blocks:
    if key not in sys.argv[2].split(","):
        continue
    parts = text.split("<br>")
    for k in (0.8, 0.55):
        cpl = math.floor(W / (S * k))
        lines = sum(max(1, math.ceil(len(p) / cpl)) for p in parts)
        print(f"[{key}] len={len(text)} k={k} cpl={cpl} lines={lines} fits={lines * LH <= H} (block {int(H // LH)} lines)")
