#!/usr/bin/env python3
"""
Generate www/api/v2/openapi.json from @route-v2 PHPDoc tags
in www/api/controllers/*.php.

Usage (run from www/api/):
    python3 tools/generate_openapi_v2.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from generate_openapi_base import main  # noqa: E402

main(
    version=2,
    output='v2/openapi.json',
    server_url='/api/v2',
    server_desc='Local FPP instance (v2)',
    info_version='2.0',
)
