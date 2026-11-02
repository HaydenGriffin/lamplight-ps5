#!/usr/bin/env python3
# Lamplight for PS5 - Switches the title between a game (streams over Remote Play) and a media app.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/category.py game|media

Remote Play streams a game's picture but a media app's is black, so console testing uses game;
releases are media. Rebuild and deploy afterwards, then re-register the title so the console
reads the new param.json."""
import json
from pathlib import Path
import sys

path = Path(__file__).resolve().parents[1] / 'sce_sys/param.json'
if len(sys.argv) != 2 or sys.argv[1] not in ('game', 'media'):
    sys.exit(__doc__)
param = json.loads(path.read_text())
if sys.argv[1] == 'game':
    param.update(applicationCategoryType=0, contentBadgeType=1, attribute=0,
                 gameIntent={'permittedIntents': [{'intentType': 'launchActivity'}]})
else:
    param.update(applicationCategoryType=65536, contentBadgeType=2, attribute=1644167168)
    param.pop('gameIntent', None)
path.write_text(json.dumps(dict(sorted(param.items())), indent=2) + '\n')
print(f'{path.name}: {sys.argv[1]}')
