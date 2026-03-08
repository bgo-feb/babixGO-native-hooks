#!/usr/bin/env python3
"""Findet ein Template in einem Vollbild-Screenshot und dokumentiert die Koordinaten.

Das Script ist interaktiv nutzbar (auch in Termux), fragt nach Kategorie/Name,
lädt das Template aus dem Ordner `templates/` und sucht die Position im
Vollbild-Screenshot.
"""

from __future__ import annotations

from dataclasses import dataclass
import importlib.util
from pathlib import Path
from typing import Any, Iterable

TEMPLATES_ROOT = Path("templates")
DOC_PATH = TEMPLATES_ROOT / "TEMPLATE_COORDS_DOC.md"
ALLOWED_GROUPS: tuple[str, ...] = ("RR/RR1", "RR/RR2", "Coin", "Build", "BOM", "Main")


@dataclass(frozen=True)
class MatchResult:
    """Ergebnis einer Template-Suche."""

    x: int
    y: int
    width: int
    height: int
    score: float


def _load_image(path: Path) -> Any:
    """Lädt ein Bild als RGB-Array."""
    from PIL import Image  # type: ignore
    import numpy as np  # type: ignore

    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32)


def _match_template_numpy(screen: Any, template: Any) -> MatchResult:
    """Fallback-Template-Matching per NumPy (SSD), ohne OpenCV-Abhängigkeit."""
    import numpy as np  # type: ignore

    screen_h, screen_w, _ = screen.shape
    temp_h, temp_w, _ = template.shape

    if temp_h > screen_h or temp_w > screen_w:
        raise ValueError("Template ist größer als der Screenshot.")

    best_score = float("inf")
    best_x = 0
    best_y = 0

    # Schrittweise Iteration; bewusst simpel für maximale Kompatibilität.
    for y in range(screen_h - temp_h + 1):
        for x in range(screen_w - temp_w + 1):
            patch = screen[y : y + temp_h, x : x + temp_w, :]
            diff = patch - template
            score = float(np.mean(diff * diff))
            if score < best_score:
                best_score = score
                best_x, best_y = x, y

    normalized = max(0.0, 1.0 - best_score / (255.0 * 255.0))
    return MatchResult(x=best_x, y=best_y, width=temp_w, height=temp_h, score=normalized)


def _match_template(screen_path: Path, template_path: Path) -> MatchResult:
    """Findet das Template im Screenshot (OpenCV wenn verfügbar, sonst NumPy-Fallback)."""
    import numpy as np  # type: ignore

    screen = _load_image(screen_path)
    template = _load_image(template_path)

    try:
        import cv2  # type: ignore

        result = cv2.matchTemplate(
            screen.astype(np.uint8),
            template.astype(np.uint8),
            cv2.TM_CCOEFF_NORMED,
        )
        _, max_val, _, max_loc = cv2.minMaxLoc(result)
        x, y = int(max_loc[0]), int(max_loc[1])
        return MatchResult(x=x, y=y, width=template.shape[1], height=template.shape[0], score=float(max_val))
    except Exception:
        return _match_template_numpy(screen, template)


def _choose_group(groups: Iterable[str]) -> str:
    """Fragt die erlaubte Template-Gruppe interaktiv ab."""
    options = list(groups)
    print("Verfügbare Pfade:")
    for index, group in enumerate(options, start=1):
        print(f"  {index}. {group}")

    while True:
        selection = input("Nummer auswählen (z. B. 1): ").strip()
        if selection.isdigit() and 1 <= int(selection) <= len(options):
            return options[int(selection) - 1]
        print("Ungültige Auswahl, bitte erneut versuchen.")


def _append_to_doc(relative_template_path: Path, match: MatchResult) -> None:
    """Hängt den gefundenen Bereich an die Doku-Datei an."""
    DOC_PATH.parent.mkdir(parents=True, exist_ok=True)
    if not DOC_PATH.exists():
        DOC_PATH.write_text(
            "# Template-Koordinaten-Dokumentation\n\n"
            "`path | size in px | koordinaten zum suchen (Startpunkt x,y größe x,y)`\n\n",
            encoding="utf-8",
        )

    line = (
        f"{relative_template_path.as_posix()} | {match.width}x{match.height} "
        f"| {match.x},{match.y} {match.width},{match.height}\n"
    )
    with DOC_PATH.open("a", encoding="utf-8") as handle:
        handle.write(line)


def main() -> int:
    """Interaktiver Einstiegspunkt."""
    missing = [module for module in ("numpy", "PIL") if importlib.util.find_spec(module) is None]
    if missing:
        print(
            "Fehlende Python-Module: "
            + ", ".join(missing)
            + ". Installieren z. B. mit: pip install numpy pillow"
        )
        return 1

    print("=== Template Finder (Termux-kompatibel) ===")
    screenshot_input = input("Pfad zum Vollbild-Screenshot: ").strip()
    screenshot_path = Path(screenshot_input)
    if not screenshot_path.exists():
        print("Screenshot wurde nicht gefunden.")
        return 1

    group = _choose_group(ALLOWED_GROUPS)
    template_name = input("Template-Dateiname (z. B. target.png): ").strip()
    if not template_name:
        print("Kein Dateiname eingegeben.")
        return 1

    template_path = TEMPLATES_ROOT / group / template_name
    if not template_path.exists():
        print(f"Template nicht gefunden: {template_path}")
        return 1

    match = _match_template(screenshot_path, template_path)
    print(
        "Gefunden:",
        f"x={match.x}, y={match.y}, size={match.width}x{match.height}, score={match.score:.4f}",
    )

    _append_to_doc(template_path.relative_to(TEMPLATES_ROOT), match)
    print(f"Eintrag in {DOC_PATH} gespeichert.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
