"""Extract the native launcher's keyed messages, including plural templates."""
import pathlib
import re
from strings_cpp import cpp_literals

ROOT = pathlib.Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'native-tv/assets/i18n/launcher/pt-BR.json'


def messages():
    result = {}
    sources = list((ROOT / 'native-tv/src/ui').glob('*.cpp'))
    sources += [ROOT / 'native-tv/src/domain' / (name + '.cpp') for name in
                ('home', 'dashboard', 'setup', 'setup_pages', 'settings_items', 'live_cards')]
    for path in sources:
        source = path.read_text(encoding='utf-8')
        for match in re.finditer(r'(?<![\w.>])(?:(?:say::)?msg|localizedPlural)\(', source):
            # Ignore comments and declarations.
            line = source[source.rfind('\n', 0, match.start()) + 1:match.start()]
            if line.lstrip().startswith('//'):
                continue
            key = cpp_literals(source, match.end())
            if not key:
                continue
            english = cpp_literals(source, source.find(',', key[1]) + 1)
            if not english:
                raise ValueError(str(path) + ': message needs literal English: ' + key[0])
            value = english[0]
            if 'localizedPlural' in match.group():
                other = cpp_literals(source, source.find(',', english[1]) + 1)
                if not other:
                    raise ValueError('plural needs both English forms: ' + key[0])
                value = {'one': value, 'other': other[0]}
            if key[0] in result and result[key[0]] != value:
                raise ValueError('different English for key ' + key[0])
            result[key[0]] = value
    return dict(sorted(result.items()))
