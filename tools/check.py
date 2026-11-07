#!/usr/bin/env python3
# Lamplight for PS5 - Formatting, source headers, package metadata and shell checks.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
"""`make lint`. With --release, the title must also be a media app (the console's home screen
puts it with the media apps); during development it is a game, so Remote Play can stream it."""
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
owned = sorted((root / 'src/lamplight').glob('*.[ch]pp'))
owned += [root / 'src/main.cpp', root / 'host/preview.cpp', root / 'host/tour.cpp',
          root / 'host/offscreen.hpp', root / 'host/fixture_home.hpp',
          root / 'tests/tests.cpp']
subprocess.run(['clang-format-18', '--dry-run', '--Werror', *map(str, owned)], check=True)
for path in owned + [root / 'tools/make-fonts.py', root / 'tools/check.py', root / 'tests/integration.py']:
    text = path.read_text()
    assert 'SPDX-License-Identifier: GPL-3.0-or-later' in text, path
    assert 'Copyright (C) 2026' in text, path
    assert not re.search('TO' 'DO|FIX' 'ME', text), path
param = json.loads((root / 'sce_sys/param.json').read_text())
assert param['titleId'] == 'PPSA99484'
assert param['localizedParameters']['en-US']['titleName'] == 'Lamplight'
assert re.fullmatch(r'UP9000-PPSA99484_00-[A-Z0-9]{16}', param['contentId'])
category = param['applicationCategoryType']
assert category in (0, 65536), category
if '--release' in sys.argv:
    assert category == 65536 and param['contentBadgeType'] == 2 and 'gameIntent' not in param, \
        'a release is a media app: run tools/category.py media'
subprocess.run(['bash', str(root / 'tools/validate-assets.sh')], check=True)
for path in (root / 'tools').glob('*.sh'):
    subprocess.run(['bash', '-n', str(path)], check=True)
example = json.loads((root / 'lamplight.conf.example').read_text())
assert example['host'] == '192.0.2.20'
assert example['token'] == 'REPLACE_WITH_A_HOME_ASSISTANT_LONG_LIVED_TOKEN'
assert not (root / 'dist/PPSA99484/lamplight.conf').exists(), 'credentials must never be packaged'
print('Lamplight format, source, metadata, assets and shell checks passed'
      f' ({"media" if category == 65536 else "game, for development"})')
