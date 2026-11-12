---
name: photo-converter
description: Export graduation portraits into 8-by-10-inch photo packages using the Photo Converter CLI.
---

Use `photo-converter check INPUT... --json` to inspect JPEG/PNG sources.
Use `photo-converter export INPUT... --output DESTINATION --format jpg --recursive --json` to export one sheet per source, with centered crops at 300 dpi. Formats: jpg, png, pdf.
Keep DESTINATION outside source folders. Recursive processing preserves subfolder paths. Files are never overwritten; repeated names receive suffixes. Errors use stderr and a nonzero exit code. Per-image results are returned in JSON.
Use the desktop interface for custom crop framing. The CLI uses centered crops; the 3x2 and 4x3 prints default to a 90-degree counterclockwise rotation, while the 4x6 print stays unrotated.

Cut margins default to 0.5 mm per side along internal cut lines only; photo edges touching the paper perimeter have no white margin. Set `--margin-mm` from 0 to 10 in 0.1 mm steps; 0 disables them. Margins are inside the fixed print rectangles, so a 1 mm margin produces a 2 mm gap between adjacent photos. JSON exports include `margin_mm_per_side`.

The GUI remembers its last margin across sessions; CLI exports use 0.5 mm unless `--margin-mm` is supplied. Values are accepted at one decimal place, including 0.1, 0.2, 0.3 and 0.4 mm.
