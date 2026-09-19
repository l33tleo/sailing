"""Stabile ASCII-navn for øyer (filnavn i cache og asset-navn i Unreal)."""
import re
import unicodedata


def slugify(name: str) -> str:
    """«Håøya» → «haaoya»: stabile ASCII-filnavn/asset-navn."""
    s = name.lower().replace("å", "aa").replace("ø", "o").replace("æ", "ae")
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode()
    return re.sub(r"[^a-z0-9]+", "_", s).strip("_")
