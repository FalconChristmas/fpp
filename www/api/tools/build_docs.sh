#!/usr/bin/env bash
# Regenerate versioned OpenAPI specs from @route PHPDoc annotations in controllers/*.php
# Run from www/api/: bash tools/build_docs.sh
set -e
TOOLS="$(dirname "$0")"
python3 "$TOOLS/generate_openapi.py"
python3 "$TOOLS/generate_openapi_v2.py"
echo "✓ Lint with: npx @redocly/cli lint --config openapi.lint.yaml v1/openapi.json"
echo "✓ Lint with: npx @redocly/cli lint --config openapi.lint.yaml v2/openapi.json"
