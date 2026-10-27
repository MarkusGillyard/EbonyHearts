"""Fetch CC0 ambientCG material sets for the fighter costumes and record them in the ledger.

    python3 -I Tools/blender/fetch_ambientcg.py

For each asset: saves the API metadata, the asset page (HTML + extracted text) as licence
evidence, downloads the 1K-JPG zip, hashes it, refuses archives with absolute/parent paths or
symlinks, extracts into a new folder, and appends a ledger item with the same schema as the
existing entries in SourceArt/ThirdParty/ledger.json. Already-recorded ids are skipped.
Nothing downloaded is executed or opened by this script beyond reading bytes.
"""
import hashlib
import html
import json
import os
import re
import stat
import sys
import urllib.request
import zipfile
from datetime import date

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "SourceArt", "ThirdParty"))
LEDGER = os.path.join(ROOT, "ledger.json")
UA = {"User-Agent": "MortalSlopbat-asset-fetch/1.0"}

ASSETS = {
    "Leather021": ("Ashka Rook vest; Gorm Tallow straps and boots", "Dark brown natural-grain leather, tinted per fighter (1K)."),
    "Leather026": ("Gorm Tallow apron", "Smooth black leather used as the base for the rubberised apron (1K)."),
    "Fabric040": ("Ashka Rook fight shorts and trim", "Fine smooth weave standing in for silk (no silk/satin set exists on ambientCG); tinted rust-red and gold, low roughness (1K)."),
    "Fabric001": ("Ashka Rook hand and shin wraps", "Soft worn cotton cloth for bone-white wraps (1K)."),
    "Fabric066": ("Gorm Tallow trousers", "Linen/canvas weave for rolled canvas trousers, tinted khaki (1K)."),
    "Metal042A": ("Ashka Rook arm band and lamp charm", "Clean gold metal, tinted brass/gold (1K)."),
    "Metal055A": ("Gorm Tallow vambraces", "Worn iron (1K)."),
    "Fabric034": ("Bram Ketch watch coat and cap; Nix Hollow hood", "Felt / wool broadcloth, tinted navy or charcoal (1K)."),
}
CC0_PAGE_TEXT = "All assets are released under the Creative Commons CC0 license"


def get(url, binary=True):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=120) as r:
        data = r.read()
        return (data if binary else data.decode("utf-8", "replace")), r.geturl()


def html_to_text(src):
    src = re.sub(r"<script.*?</script>", "", src, flags=re.S | re.I)
    src = re.sub(r"<style.*?</style>", "", src, flags=re.S | re.I)
    src = re.sub(r"<[^>]+>", "\n", src)
    lines = [html.unescape(l).strip() for l in src.splitlines()]
    return "\n".join(" " + l + " " for l in lines if l)


def check_zip(path):
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            name = info.filename
            if name.startswith("/") or ".." in name.replace("\\", "/").split("/"):
                raise RuntimeError("unsafe path in %s: %s" % (path, name))
            if stat.S_ISLNK(info.external_attr >> 16):
                raise RuntimeError("symlink in %s: %s" % (path, name))
        return [i.filename for i in z.infolist()]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def dir_bytes(path):
    return sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fs in os.walk(path) for f in fs)


def fetch(asset_id, use, purpose, today):
    folder = os.path.join(ROOT, "ambientCG", asset_id)
    os.makedirs(folder, exist_ok=True)
    api_url = ("https://ambientcg.com/api/v2/full_json?id=%s&include=downloadData,mapData,"
               "imageData,displayData,tagData,dimensionsData,relationshipData" % asset_id)
    api, _ = get(api_url, binary=False)
    with open(os.path.join(folder, "api_v2_%s.json" % asset_id), "w") as f:
        f.write(api)
    meta = json.loads(api)["foundAssets"][0]
    page_url = "https://ambientcg.com/view?id=%s" % asset_id
    page, _ = get(page_url, binary=False)
    with open(os.path.join(folder, "asset_page_%s.html" % asset_id), "w") as f:
        f.write(page)
    text = html_to_text(page)
    with open(os.path.join(folder, "asset_page_%s.txt" % asset_id), "w") as f:
        f.write("Snapshot of %s\nRetrieved %s (text extracted from the saved HTML next to this file)\n\n"
                % (page_url, today) + text + "\n")
    if CC0_PAGE_TEXT not in text:
        raise RuntimeError("CC0 statement not found on the asset page of " + asset_id)
    zname = "%s_1K-JPG.zip" % asset_id
    dl_url = "https://ambientcg.com/get?file=" + zname
    data, final_url = get(dl_url)
    zpath = os.path.join(folder, zname)
    with open(zpath, "wb") as f:
        f.write(data)
    names = check_zip(zpath)
    out_dir = os.path.join(folder, zname[:-4])
    if os.path.exists(out_dir):
        raise RuntimeError("extraction folder already exists: " + out_dir)
    os.makedirs(out_dir)
    with zipfile.ZipFile(zpath) as z:
        z.extractall(out_dir)
    maps = sorted({re.sub(r"^%s_1K-JPG_" % asset_id, "", n).rsplit(".", 1)[0]
                   for n in names if n.lower().endswith(".jpg")})
    sidecars = sorted({n.rsplit(".", 1)[-1] for n in names if not n.lower().endswith(".jpg")})
    title = meta.get("displayName") or asset_id
    rel = "ambientCG/" + asset_id
    item = {
        "id": "ambientcg/" + asset_id,
        "type": "surface_material",
        "title": title,
        "creator": "ambientCG (Lennart Demes, per https://docs.ambientcg.com/legal/)",
        "source": "ambientCG",
        "source_page_url": page_url,
        "short_url": "https://ambientcg.com/a/" + asset_id,
        "download_url": dl_url,
        "download_redirected_to": final_url,
        "acquired": today,
        "resolution_format": "1K JPG set (zip). Maps: %s. Normals: both NormalDX and NormalGL are "
                             "included; use NormalDX in Unreal." % ", ".join(maps),
        "normal_map_convention": "DirectX (NormalDX) and OpenGL (NormalGL) both included; use DX for UE5",
        "sha256": sha256(zpath),
        "sha256_of": "downloaded zip archive",
        "size_bytes": os.path.getsize(zpath),
        "license": {
            "name": "Creative Commons CC0 1.0 Universal (Public Domain Dedication)",
            "spdx": "CC0-1.0", "version": "1.0",
            "legalcode_url": "https://creativecommons.org/publicdomain/zero/1.0/legalcode"},
        "license_evidence": {
            "urls": [page_url, "https://docs.ambientcg.com/license/"],
            "local_copies": [rel + "/asset_page_%s.html" % asset_id, rel + "/asset_page_%s.txt" % asset_id,
                             "ambientCG/_license/ambientcg_license_docs.html",
                             "ambientCG/_license/ambientcg_license_docs.txt",
                             "ambientCG/_license/CC0-1.0_legalcode.txt"],
            "statement": "Asset page: 'All assets are released under the Creative Commons CC0 license, "
                         "making them free to use without attribution - even in commercial circumstances.' "
                         "License docs: 'All ambientCG assets are provided under the Creative Commons CC0 "
                         "1.0 Universal License.'",
            "caveat": "The zip has no license file and the API returns no per-asset license field, so the "
                      "evidence is the CC0 statement on the asset's own page plus the site license page (both saved)."},
        "attribution_text": "Created using %s from ambientCG.com, licensed under the Creative Commons CC0 1.0 "
                            "Universal License. (Not required; this is ambientCG's suggested wording.)" % title,
        "attribution_required": False,
        "intended_use": {"stage": "Fighters: " + use, "purpose": purpose},
        "local_path": rel,
        "local_files": {"archive": rel + "/" + zname, "extracted": rel + "/" + zname[:-4],
                        "api_metadata": rel + "/api_v2_%s.json" % asset_id},
        "conclusions": {
            "commercial_game_use": {"verdict": "permitted", "basis": "CC0 1.0 waiver; source states commercial use is allowed; no fee, no attribution required."},
            "modification": {"verdict": "permitted", "basis": "CC0 1.0 waives all copyright and related rights, including adaptation."},
            "packaged_redistribution": {"verdict": "permitted", "basis": "CC0 1.0 allows distribution in any form, including cooked/compiled into a commercial game."},
            "raw_source_redistribution": {"verdict": "permitted", "basis": "permitted: CC0 waives copyright; ambientCG's license page also states 'You can include the raw files in your project, for example a video game.' Ship-as-is or in a source repo is fine."}},
        "notes": [
            "CC0 1.0 section 4(a): no trademark or patent rights are licensed, and no warranty of non-infringement is given.",
            "The archive also contains %s sidecar files (data only, never opened or run). If you open a .blend in "
            "Blender, leave 'Auto Run Python Scripts' off." % ", ".join("." + s for s in sidecars),
            "Added by Tools/blender/fetch_ambientcg.py for the fighter costume pipeline (Tools/blender/build_fighter.py)."],
    }
    return item, item["size_bytes"], dir_bytes(folder)


def main():
    today = date.today().isoformat()
    with open(LEDGER) as f:
        have = {i["id"] for i in json.load(f)["items"]}
    new, dl, disk = [], 0, 0
    for asset_id, (use, purpose) in ASSETS.items():
        if "ambientcg/" + asset_id in have:
            print("skip (already in ledger)", asset_id)
            continue
        item, b, d = fetch(asset_id, use, purpose, today)
        print("fetched", asset_id, item["sha256"], item["size_bytes"])
        new.append(item)
        dl += b
        disk += d
    if not new:
        return
    # re-read right before writing so concurrent additions by others are kept
    with open(LEDGER) as f:
        ledger = json.load(f)
    ids = {i["id"] for i in ledger["items"]}
    added = [i for i in new if i["id"] not in ids]
    ledger["items"].extend(added)
    t = ledger.setdefault("totals", {})
    t["items"] = len(ledger["items"])
    t["surface_materials"] = sum(1 for i in ledger["items"] if i.get("type") == "surface_material")
    t["downloaded_bytes"] = t.get("downloaded_bytes", 0) + sum(i["size_bytes"] for i in added)
    t["on_disk_bytes_including_extracted_and_evidence"] = t.get(
        "on_disk_bytes_including_extracted_and_evidence", 0) + disk
    tmp = LEDGER + ".tmp"
    with open(tmp, "w") as f:
        f.write(json.dumps(ledger, indent=2, ensure_ascii=False))   # same layout as the file had
    os.replace(tmp, LEDGER)
    print("ledger: added", [i["id"] for i in added], "items now", t["items"])


if __name__ == "__main__":
    sys.exit(main())
