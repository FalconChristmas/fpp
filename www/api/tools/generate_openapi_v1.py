#!/usr/bin/env python3
"""
Generate www/api/v1/openapi.json from @route-v1 PHPDoc tags
in www/api/controllers/*.php.

Usage (run from www/api/):
    python3 tools/generate_openapi_v1.py
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from generate_openapi_base import main  # noqa: E402

main(
    version=1,
    output='v1/openapi.json',
    server_url='/api/',
    server_desc='Local FPP instance',
    info_version='1.0',
)
