import pathlib
import shutil
import subprocess
import sys


def main():
    assets_dir = pathlib.Path(__file__).resolve().parent
    output_dir = pathlib.Path(sys.argv[1]).resolve()
    generated_dir = assets_dir.parent / 'tmp' / 'assets'

    subprocess.check_call([sys.executable, str(assets_dir / 'compileAssets.py')], cwd=assets_dir)

    output_dir.mkdir(parents=True, exist_ok=True)
    for filename in (
        'spriteDatabase.h',
        'variableSprites.h',
        'pitchDatabase.h',
        'stadiumSprites.h',
        'assets.stamp',
    ):
        shutil.copy2(generated_dir / filename, output_dir / filename)


if __name__ == '__main__':
    main()
