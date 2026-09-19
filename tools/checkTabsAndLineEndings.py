import os
import sys
from typing import Final, Tuple

# Extensions to check (must include the leading dot; matched case-insensitively).
kExtensionsToCheck: Final[Tuple[str, ...]] = (
    '.cpp', '.c', '.h', '.txt', '.md', '.mnu', '.mh', '.asm', '.inc', '.py', '.xml', '.pl',
)

# Exact filenames (no extension) to check, matched case-insensitively.
# Note: Makefiles legitimately require tab characters for recipe lines,
# so they're checked for line-ending consistency but skipped for the tab check.
kExactNamesToCheck: Final[Tuple[str, ...]] = (
    'makefile',
)
kExactNamesSkipTabCheck: Final[Tuple[str, ...]] = (
    'makefile',
)


def shouldCheckFile(fileName: str) -> bool:
    lowerName = fileName.lower()
    if lowerName in kExactNamesToCheck:
        return True
    _, ext = os.path.splitext(lowerName)
    return ext in kExtensionsToCheck


def shouldSkipTabCheck(fileName: str) -> bool:
    return fileName.lower() in kExactNamesSkipTabCheck


def findTabLines(content: bytes) -> list:
    """Return 1-based line numbers containing a tab character."""
    badLines = []
    for lineNo, line in enumerate(content.split(b'\n'), start=1):
        if b'\t' in line:
            badLines.append(str(lineNo))
    return badLines


def classifyLineEndings(content: bytes) -> str:
    """
    Classify line endings in the file content.
    Returns one of: 'none' (no newlines / single line), 'crlf', 'lf', 'mixed'.
    Note: bare '\\r' (old Mac-style) endings are not specifically distinguished
    here and would currently be invisible to this check, since content.split(b'\\n')
    keeps a trailing '\\r' as part of the "line" content -- if you need to support
    that style too, split on b'\\r' as well and mark it separately.
    """
    hasCRLF = b'\r\n' in content
    bareLFCount = content.replace(b'\r\n', b'').count(b'\n')
    hasBareLF = bareLFCount > 0

    if hasCRLF and hasBareLF:
        return 'mixed'
    elif hasCRLF:
        return 'crlf'
    elif hasBareLF:
        return 'lf'
    else:
        return 'none'


def checkFile(path: str) -> bool:
    ok = True

    with open(path, 'rb') as f:
        content = f.read()

    if not shouldSkipTabCheck(os.path.basename(path)):
        badTabLines = findTabLines(content)
        if badTabLines:
            print('**** TAB CHARACTER(S) DETECTED! ****')
            print(path)
            print(f'Lines: {",".join(badTabLines)}')
            ok = False

    endingStyle = classifyLineEndings(content)
    if endingStyle == 'lf':
        print('**** LF-ONLY LINE ENDINGS DETECTED (expected CRLF)! ****')
        print(path)
        ok = False
    elif endingStyle == 'mixed':
        print('**** MIXED LF/CRLF LINE ENDINGS DETECTED! ****')
        print(path)
        ok = False

    return ok


def scanDirectory(directory: str) -> bool:
    ok = True
    for root, dirs, files in os.walk(directory):
        for file in files:
            if shouldCheckFile(file):
                ok = checkFile(os.path.join(root, file)) and ok
    return ok


def main():
    ok = True
    if len(sys.argv) <= 1:
        ok = scanDirectory('.') and ok
    else:
        for directory in sys.argv[1:]:
            if not os.path.isdir(directory):
                sys.exit(f"Couldn't scan '{directory}'")
            ok = scanDirectory(directory) and ok

    if not ok:
        sys.exit(1)


if __name__ == '__main__':
    main()
