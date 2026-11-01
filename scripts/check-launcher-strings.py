#!/usr/bin/env python3
"""Validate native catalog coverage, parameters, plural forms and font coverage."""
import ast
import json
import re
import subprocess
import sys
import unicodedata
from launcher_strings import CATALOG, ROOT, messages

english = messages()
if '--template' in sys.argv:
    print(json.dumps({key: {'from': value, 'text': ''} for key, value in english.items()},
                     ensure_ascii=False, indent=2))
    sys.exit(0)

catalog = json.loads(CATALOG.read_text(encoding='utf-8'))
all_sources = json.loads(subprocess.check_output(
    [sys.executable, '-X', 'utf8', str(ROOT / 'scripts/check-strings.py'), '--template'], encoding='utf-8'))
problems = []
for key in english:
    if key not in catalog:
        problems.append('missing ' + key)
for key, entry in catalog.items():
    if not isinstance(entry, dict):
        problems.append('entry must be an object: ' + key)
        continue
    expected = english.get(key)
    if expected is None and key.startswith('srv.') and key in all_sources:
        expected = all_sources[key]['from']
    if expected is None:
        problems.append('unused ' + key)
        continue
    if entry.get('from') != expected:
        problems.append('outdated ' + key)
    original, translated = expected, entry.get('text')
    if isinstance(original, dict):
        if not isinstance(translated, dict) or set(translated) != {'one', 'many', 'other'}:
            problems.append('plural needs one, many and other: ' + key)
            continue
        pairs = [(original.get('one', ''), translated['one']),
                 (original.get('other', ''), translated['many']),
                 (original.get('other', ''), translated['other'])]
    else:
        pairs = [(original, translated)]
    for source, text in pairs:
        if not isinstance(text, str) or not text.strip():
            problems.append('empty text: ' + key)
            continue
        if sorted(re.findall(r'\{\w+\}', source)) != sorted(re.findall(r'\{\w+\}', text)):
            problems.append('different placeholders: ' + key)
        if text != unicodedata.normalize('NFC', text):
            problems.append('text must use NFC Unicode: ' + key)

# The bundled font subset is deliberate; catch an unsupported glyph before packaging.
tree = ast.parse((ROOT / 'native-tv/tools/gen_fonts.py').read_text(encoding='utf-8'))
ranges = next(ast.literal_eval(node.value) for node in tree.body
              if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'LATIN' for t in node.targets))
supported = set()
for part in ranges.split(','):
    first, _, last = part[2:].partition('-')
    supported.update(range(int(first, 16), int(last or first, 16) + 1))
for key, entry in catalog.items():
    texts = entry.get('text', '')
    for text in texts.values() if isinstance(texts, dict) else [texts]:
        for char in text:
            if not char.isspace() and ord(char) not in supported:
                problems.append('unsupported font glyph U+%04X in %s' % (ord(char), key))
for problem in problems:
    print(problem)
print('%d native messages, %d Portuguese entries, %d problems' % (len(english), len(catalog), len(problems)))
sys.exit(bool(problems))
