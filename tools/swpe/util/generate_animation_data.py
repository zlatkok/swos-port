"""Generate SWPE's C animation data from the game's animation table source."""

import importlib.util
import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[3]
PARSER_PATH = ROOT / "src/game/animation/generateAnimationTables.py"
INPUT_PATH = ROOT / "src/game/animation/animationTables.in"
OUTPUT_PATH = ROOT / "tools/swpe/src/animation_data.inc"


def load_parser():
    sys.path.insert(0, str(PARSER_PATH.parent))
    spec = importlib.util.spec_from_file_location("animation_table_parser", PARSER_PATH)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main():
    parser = load_parser()
    constants = parser.readConstants(ROOT / "src/swos/swos.h")
    constants.update(parser.readConstants(ROOT / "src/sprites/sprites.h"))
    parsed = parser.parseText(INPUT_PATH.read_text(encoding="utf-8"), constants)
    lines = [
        "/* Generated from src/game/animation/animationTables.in. */",
        "/* Run tools/swpe/util/generate_animation_data.py after editing it. */",
        "",
    ]
    for index, table in enumerate(parsed.frameTables):
        values = ", ".join(str(value) for value in table.resolvedValues)
        lines.append(f"static const short animationFrames{index}[] = {{ {values} }};")
    lines.append("static const short ballMovingFrameTable[] = "
        "{ 1182, -5, 1181, -5, 1180, -5, 1179, -5, -999 };")

    frame_ids = {table.name: index for index, table in enumerate(parsed.frameTables)}
    lines.extend(["", "static const AnimationData animationData[] = {"])
    for table in parsed.animationTables:
        # Team 1 is the most useful preview; referee and goalkeeper-only tables
        # naturally fall back to their first populated section.
        section = table.sections[0]
        if not any(section):
            section = next((item for item in table.sections if any(item)), section)
        pointers = ["NULL" if name is None else f"animationFrames{frame_ids[name]}" for name in section]
        durations = []
        loops = []
        for name in section:
            if name is None:
                durations.append(0)
                loops.append(0)
                continue
            values = parsed.frameTables[frame_ids[name]].resolvedValues
            index = elapsed = 0
            delay = max(table.frameDelay, 1)
            seen = {}
            while True:
                state = (index, delay)
                if state in seen:
                    durations.append(elapsed - seen[state])
                    loops.append(1)
                    break
                seen[state] = elapsed
                value = values[index]
                if value >= 0:
                    elapsed += delay
                    index += 1
                elif value == -999:
                    durations.append(elapsed)
                    loops.append(1)
                    break
                elif value == -101:
                    durations.append(elapsed)
                    loops.append(0)
                    break
                elif value <= -100:
                    index += value + 100
                else:
                    delay = -value
                    index += 1
        duration_text = ", ".join(str(value) for value in durations)
        loop_text = ", ".join(str(value) for value in loops)
        lines.append(f'    {{ "{table.name}", {table.frameDelay}, '
            f'{{ {", ".join(pointers)} }}, {{ {duration_text} }}, {{ {loop_text} }} }},')
    lines.append('    { "ballMovingFrameTable", 5, '
        '{ NULL, NULL, ballMovingFrameTable, NULL, NULL, NULL, NULL, NULL }, '
        '{ 0, 0, 20, 0, 0, 0, 0, 0 }, { 0, 0, 1, 0, 0, 0, 0, 0 } },')
    lines.extend(["};", "", "#define NUM_ANIMATIONS sizeofarray(animationData)", ""])
    with OUTPUT_PATH.open("w", encoding="ascii", newline="\r\n") as output:
        output.write("\n".join(lines))


if __name__ == "__main__":
    main()
