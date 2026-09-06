#!/usr/bin/env python3

import argparse
import json
import pathlib
import re


kRepoRoot = pathlib.Path(__file__).resolve().parent.parent
kDefaultOutput = kRepoRoot / 'tests' / 'data' / 'menu-walker-inventory.json'


def readExisting(path):
    if not path.is_file():
        return {}
    with path.open(encoding='utf-8') as source:
        data = json.load(source)
    return {menu['id']: menu for menu in data.get('menus', [])}


def sourceLocation(path, text, offset):
    return {
        'file': path.relative_to(kRepoRoot).as_posix(),
        'line': text.count('\n', 0, offset) + 1,
    }


def discoverAssemblyMenus():
    path = kRepoRoot / 'swos' / 'swos.asm'
    text = path.read_text(encoding='latin1')
    pattern = re.compile(r'(?mi)^([A-Za-z_$?@][\w$?@]*menu)\s+dd\s+')
    return {
        match.group(1): sourceLocation(path, text, match.start())
        for match in pattern.finditer(text)
    }


def discoverCompiledMenus():
    result = {}
    pattern = re.compile(r'(?m)^\s*Menu\s+([A-Za-z_]\w*)')
    for path in sorted((kRepoRoot / 'src' / 'menus' / 'mnu').glob('*.mnu')):
        text = path.read_text(encoding='utf-8')
        for match in pattern.finditer(text):
            result[match.group(1)] = sourceLocation(path, text, match.start())
    return result


def discoverExistingTests():
    result = {}
    pattern = re.compile(r'(?m)^\s*(?:show|Show|Edit|Init)([A-Za-z_]\w*Menu)\s*\(')
    for path in sorted((kRepoRoot / 'tests' / 'src' / 'tests').glob('*MenuTest.cpp')):
        text = path.read_text(encoding='utf-8')
        for match in pattern.finditer(text):
            result.setdefault(match.group(1), []).append(path.stem)
    return result


def defaultPolicy(name):
    lower = name.lower()
    if 'audio' in lower:
        return 'blocked-global-audio'
    if 'video' in lower or 'windowmode' in lower:
        return 'blocked-global-video'
    return 'walk'


def mergeMenu(name, kind, location, previous, tests):
    old = previous.get(name, {})
    return {
        'id': name,
        'kind': kind,
        'source': location,
        'policy': old.get('policy', defaultPolicy(name)),
        'status': old.get('status', 'discovered'),
        'reachable_from_main': old.get('reachable_from_main'),
        'fixture': old.get('fixture'),
        'manual_cases': old.get('manual_cases', []),
        'existing_tests': sorted(set(old.get('existing_tests', [])) | set(tests.get(name, []))),
        'notes': old.get('notes', ''),
    }


def main():
    parser = argparse.ArgumentParser(description='Update the persistent automatic-menu-test inventory.')
    parser.add_argument('--output', type=pathlib.Path, default=kDefaultOutput)
    args = parser.parse_args()

    previous = readExisting(args.output)
    assembly = discoverAssemblyMenus()
    compiled = discoverCompiledMenus()
    tests = discoverExistingTests()

    menus = []
    for name, location in assembly.items():
        menus.append(mergeMenu(name, 'swos-asm', location, previous, tests))
    for name, location in compiled.items():
        kind = 'converted-mnu' if name not in assembly else 'converted-mnu+swos-asm'
        menus.append(mergeMenu(name, kind, location, previous, tests))

    # A converted menu supersedes the identically named assembly inventory row.
    byName = {}
    for menu in menus:
        old = byName.get(menu['id'])
        if old and menu['kind'].startswith('converted'):
            menu['legacy_source'] = old['source']
        byName[menu['id']] = menu

    output = {
        'schema': 1,
        'description': 'Persistent discovery and execution state for the automatic menu walker.',
        'menus': sorted(byName.values(), key=lambda menu: menu['id'].lower()),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', encoding='utf-8', newline='\r\n') as destination:
        json.dump(output, destination, indent=2, ensure_ascii=False)
        destination.write('\n')

    counts = {}
    for menu in output['menus']:
        counts[menu['kind']] = counts.get(menu['kind'], 0) + 1
    print(f'Wrote {len(output["menus"])} menus to {args.output}')
    for kind, count in sorted(counts.items()):
        print(f'  {kind}: {count}')


if __name__ == '__main__':
    main()
