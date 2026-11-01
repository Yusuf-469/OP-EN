"""WCAG 2.2 contrast audit of the colour pairs used by the UI (src/web/app.css, js/charts.js).

Text needs 4.5:1 (AA, normal size); UI component boundaries, focus rings and chart lines need 3:1 (SC 1.4.11).
Usage: python tests/contrast_check.py   (exit code 1 if any pair fails)
"""
import sys


def luminance(hex_color):
    def channel(c):
        c /= 255
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (int(hex_color[i:i + 2], 16) for i in (1, 3, 5))
    return 0.2126 * channel(r) + 0.7152 * channel(g) + 0.0722 * channel(b)


def ratio(a, b):
    hi, lo = sorted((luminance(a), luminance(b)), reverse=True)
    return (hi + 0.05) / (lo + 0.05)


TEXT = 4.5
NON_TEXT = 3.0
PAIRS = [  # (what, foreground, background, required)
    ("body text on page", "#17202b", "#f3f5f8", TEXT),
    ("body text on panel", "#17202b", "#ffffff", TEXT),
    ("muted text on panel", "#4d5b6a", "#ffffff", TEXT),
    ("muted text on page", "#4d5b6a", "#f3f5f8", TEXT),
    ("link / ghost button text", "#0b5cad", "#ffffff", TEXT),
    ("primary button label", "#ffffff", "#0b5cad", TEXT),
    ("ok text on panel", "#17703a", "#ffffff", TEXT),
    ("ok pill", "#17703a", "#def3e5", TEXT),
    ("warning pill", "#8a5300", "#fff4d6", TEXT),
    ("error text on panel", "#b3261e", "#ffffff", TEXT),
    ("error pill", "#b3261e", "#fbe1df", TEXT),
    ("disconnected state badge", "#4d5b6a", "#e9edf2", TEXT),
    ("simulated banner text", "#4a3500", "#fff4d6", TEXT),
    ("inline code", "#17202b", "#eef2f6", TEXT),
    # outline-offset: 2px places the focus ring on the panel/page background, not on the control itself
    ("focus ring vs panel", "#b45309", "#ffffff", NON_TEXT),
    ("focus ring vs page", "#b45309", "#f3f5f8", NON_TEXT),
    ("input border vs panel", "#7a8896", "#ffffff", NON_TEXT),
    ("chart series 1", "#0b5cad", "#ffffff", NON_TEXT),
    ("chart series 2", "#c4421a", "#ffffff", NON_TEXT),
    ("chart series 3", "#2e8540", "#ffffff", NON_TEXT),
    ("chart series 4", "#7b3fa0", "#ffffff", NON_TEXT),
    ("chart series 5", "#a06d00", "#ffffff", NON_TEXT),
    ("chart series 6", "#00838f", "#ffffff", NON_TEXT),
    ("chart reference line", "#8796a5", "#ffffff", NON_TEXT),
    ("3D viewer help text", "#4d5b6a", "#eef2f6", TEXT),
]


def main():
    failures = 0
    for name, fg, bg, need in PAIRS:
        r = ratio(fg, bg)
        ok = r >= need
        failures += not ok
        print(f"{'PASS' if ok else 'FAIL'} {r:5.2f}:1 (need {need}:1)  {name}  {fg} on {bg}")
    print(f"{len(PAIRS) - failures}/{len(PAIRS)} pairs pass")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
