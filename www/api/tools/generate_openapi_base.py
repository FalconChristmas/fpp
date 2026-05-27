#!/usr/bin/env python3
"""
Shared OpenAPI generator logic for all FPP API versions.

Imported by generate_openapi_v1.py, generate_openapi_v2.py, etc.

Docblock tag reference:
  @route-vN METHOD /path     — route for version N (REQUIRED; path is prefix-free)
  @body-vN  JSON             — version-specific request body (falls back to @body)
  @response-vN STATUS DESC   — version-specific response (falls back to @response)
  @badge-vN "Label" level    — version-specific badge (falls back to @badge)
  @deprecated-vN             — marks the operation deprecated: true in version N
  @param TYPE NAME DESC      — query parameter (shared across all versions)

Paths in docblocks must be prefix-free: write /system/reboot, not
/api/system/reboot or /api/v2/system/reboot. The generator prepends the
server base URL; the OpenAPI spec paths are relative to the server entry.
"""

import glob
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

BADGE_COLORS = {
    'success':  '#2e7d32',
    'warning':  '#b25e00',
    'critical': '#c62828',
    'info':     '#546e7a',
}

MEDIA_HINT_MAP = {
    'json':   'application/json',
    'text':   'text/plain',
    'bytes':  'application/octet-stream',
    'binary': 'application/octet-stream',
    'xml':    'application/xml',
    'html':   'text/html',
}

DOCBLOCK_RE = re.compile(r'/\*\*(.*?)\*/', re.DOTALL)

# Matches @response (unversioned) or @response-vN.
# Groups: (1) status code, (2) description, (3) media hint, (4) block content
_RESPONSE_RE_TMPL = (
    r'@response{suffix}(?:\s+(\d{{3}}))?\s+([^\n]+)'
    r'(?:\n\s*```(\w+)\n(.*?)\n\s*```)?'
)


def _response_re(suffix=''):
    return re.compile(_RESPONSE_RE_TMPL.format(suffix=suffix), re.DOTALL)


# ---------------------------------------------------------------------------
# PHPDoc parsing
# ---------------------------------------------------------------------------

def strip_stars(block):
    lines = []
    for line in block.splitlines():
        line = re.sub(r'^\s*\*\s?', '', line)
        lines.append(line)
    return '\n'.join(lines)


def parse_docblocks(php_source, version: int):
    """
    Yield endpoint dicts for every docblock that has @route-vN for the
    requested version.  Tags without a version suffix are shared/fallback.
    """
    route_re     = re.compile(rf'@route-v{version}\s+(GET|POST|PUT|DELETE|PATCH)\s+(\S+)')
    body_ver_re  = re.compile(rf'@body-v{version}\s+(.+)')
    body_re      = re.compile(r'@body\s+(.+)')
    depr_re      = re.compile(rf'@deprecated-v{version}')
    badge_ver_re = re.compile(rf'@badge-v{version}\s+"([^"]+)"\s+(\w+)')
    badge_re     = re.compile(r'@badge\s+"([^"]+)"\s+(\w+)')
    resp_ver_re  = _response_re(rf'-v{version}')
    resp_re      = _response_re()

    blocks = [strip_stars(m.group(1)) for m in DOCBLOCK_RE.finditer(php_source)]

    for text in blocks:
        route_match = route_re.search(text)
        if not route_match:
            continue

        method    = route_match.group(1).lower()
        oapi_path = route_match.group(2)

        # Description / summary (everything before the first @tag)
        desc_raw = text[:text.find('@')].strip() if '@' in text else text.strip()
        paragraphs = []
        current = []
        for ln in desc_raw.splitlines():
            stripped = ln.strip()
            if stripped:
                current.append(stripped)
            elif current:
                paragraphs.append(' '.join(current))
                current = []
        if current:
            paragraphs.append(' '.join(current))

        if len(paragraphs) == 0:
            summary = description = None
        elif len(paragraphs) == 1:
            summary     = None
            description = paragraphs[0]
        else:
            summary     = paragraphs[0]
            description = ' '.join(paragraphs[1:])

        # Body: versioned wins over generic
        bm       = body_ver_re.search(text) or body_re.search(text)
        body_raw = bm.group(1).strip() if bm else None

        # Deprecated flag
        deprecated = bool(depr_re.search(text))

        # Query params (shared; path params are derived from the route pattern)
        path_param_names = set(extract_path_params(oapi_path))
        params = []
        for pm in re.finditer(r'@param\s+(\S+)\s+(\S+)\s*(.*)', text):
            php_type, pname, pdesc = pm.group(1), pm.group(2), pm.group(3).strip()
            if pname in path_param_names:
                continue
            type_map = {
                'int': 'integer', 'integer': 'integer',
                'bool': 'boolean', 'boolean': 'boolean',
                'float': 'number', 'number': 'number',
            }
            params.append({
                'name':        pname,
                'in':          'query',
                'required':    False,
                'schema':      {'type': type_map.get(php_type.lower(), 'string')},
                'description': pdesc or None,
            })

        # Responses: versioned entries override generic ones by status code
        def collect_responses(pattern):
            result = {}
            for rm in pattern.finditer(text):
                status = int(rm.group(1)) if rm.group(1) else 200
                result[status] = (
                    status,
                    rm.group(2).strip(),
                    rm.group(3),
                    rm.group(4).strip() if rm.group(4) is not None else None,
                )
            return result

        resp_base = collect_responses(resp_re)
        resp_base.update(collect_responses(resp_ver_re))
        responses = list(resp_base.values())

        # Badges: versioned entries supplement / override generic ones by name
        def collect_badges(pattern):
            return [
                {
                    'name':  bm.group(1),
                    'color': BADGE_COLORS.get(bm.group(2).lower(), BADGE_COLORS['info']),
                }
                for bm in pattern.finditer(text)
            ]

        seen_names = {}
        for b in collect_badges(badge_re) + collect_badges(badge_ver_re):
            seen_names[b['name']] = b
        badges = list(seen_names.values())

        yield {
            'method':      method,
            'path':        oapi_path,
            'summary':     summary,
            'description': description,
            'body_raw':    body_raw,
            'deprecated':  deprecated,
            'responses':   responses,
            'badges':      badges,
            'params':      params,
        }


def load_endpoints(controllers_glob, version):
    endpoints = []
    for php_file in sorted(glob.glob(controllers_glob)):
        source = open(php_file, encoding='utf-8', errors='replace').read()
        for ep in parse_docblocks(source, version):
            endpoints.append(ep)
    endpoints.sort(key=lambda e: (e['path'], e['method']))
    return endpoints


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def parse_json_value(raw):
    try:
        return json.loads(raw)
    except (json.JSONDecodeError, TypeError):
        return raw


def extract_mime_override(block_content, default_type):
    """
    If the first line is a bracketed MIME-type hint such as
      [Content-Type: text/event-stream]
    return (mime_type, remaining_content).  Otherwise return (default_type, block_content).
    """
    if not block_content:
        return default_type, block_content
    lines = block_content.splitlines()
    m = re.match(r'^\[.*?([a-z]+/[a-z][a-z0-9+.\-]*)\]', lines[0], re.IGNORECASE)
    if m:
        return m.group(1), '\n'.join(lines[1:]).strip()
    return default_type, block_content


def extract_path_params(path):
    return re.findall(r'\{([^}]+)\}', path)


def tag_from_path(path):
    parts = [p for p in path.strip('/').split('/') if p and not p.startswith('{')]
    return parts[0] if parts else 'general'


# ---------------------------------------------------------------------------
# Build OpenAPI dict
# ---------------------------------------------------------------------------

def build_openapi(endpoints, server_url, server_desc, info_version):
    tags = sorted(set(tag_from_path(e['path']) for e in endpoints))

    spec = {
        'openapi': '3.0.3',
        'info': {
            'title':       'FPP API',
            'description': 'Falcon Player (FPP) REST API',
            'version':     info_version,
        },
        'servers': [{'url': server_url, 'description': server_desc}],
        'tags':    [{'name': t} for t in tags],
        'paths':   {},
    }

    by_path = defaultdict(list)
    for ep in endpoints:
        by_path[ep['path']].append(ep)

    for path in sorted(by_path):
        eps       = by_path[path]
        path_params = extract_path_params(path)
        path_item = {}

        if path_params:
            path_item['parameters'] = [
                {'name': p, 'in': 'path', 'required': True, 'schema': {'type': 'string'}}
                for p in path_params
            ]

        for ep in sorted(eps, key=lambda e: e['method']):
            method     = ep['method']
            tag        = tag_from_path(path)
            route_slug = path.lstrip('/')
            summary    = ep['summary'] if ep['summary'] else route_slug

            operation = {
                'tags':    [tag],
                'summary': summary,
            }

            if ep.get('deprecated'):
                operation['deprecated'] = True

            if ep['description']:
                operation['description'] = ep['description']

            if ep['badges']:
                operation['x-badges'] = ep['badges']

            if ep['params']:
                operation['parameters'] = [
                    {k: v for k, v in p.items() if v is not None}
                    for p in ep['params']
                ]

            if ep['body_raw']:
                body_val = parse_json_value(ep['body_raw'])
                if isinstance(body_val, str):
                    content = {
                        'text/plain': {'schema': {'type': 'string'}, 'example': body_val}
                    }
                else:
                    content = {
                        'application/json': {
                            'schema':  {'type': 'array' if isinstance(body_val, list) else 'object'},
                            'example': body_val,
                        }
                    }
                operation['requestBody'] = {'content': content}

            responses = {}
            if ep['responses']:
                seen = set()
                for status, desc, media_hint, block_content in ep['responses']:
                    if status in seen:
                        continue
                    seen.add(status)

                    if media_hint is not None and block_content is not None:
                        content_type = MEDIA_HINT_MAP.get(media_hint, 'application/octet-stream')

                        if media_hint == 'json':
                            parsed = parse_json_value(block_content)
                            schema  = {'type': 'array' if isinstance(parsed, list) else 'object'}
                            example = parsed
                            if isinstance(parsed, str):
                                schema  = {'type': 'string'}
                                example = parsed
                            responses[str(status)] = {
                                'description': desc,
                                'content': {content_type: {'schema': schema, 'example': example}},
                            }
                        elif media_hint == 'bytes':
                            content_type, _ = extract_mime_override(block_content, content_type)
                            responses[str(status)] = {
                                'description': desc,
                                'content': {
                                    content_type: {'schema': {'type': 'string', 'format': 'binary'}}
                                },
                            }
                        else:
                            content_type, example = extract_mime_override(block_content, content_type)
                            responses[str(status)] = {
                                'description': desc,
                                'content': {
                                    content_type: {'schema': {'type': 'string'}, 'example': example},
                                },
                            }
                    else:
                        val = parse_json_value(desc)
                        if isinstance(val, str):
                            responses[str(status)] = {'description': val}
                        else:
                            fallback_desc = 'Success' if status == 200 else f'HTTP {status}'
                            responses[str(status)] = {
                                'description': fallback_desc,
                                'content': {
                                    'application/json': {
                                        'schema':  {'type': 'array' if isinstance(val, list) else 'object'},
                                        'example': val,
                                    }
                                },
                            }
            else:
                responses['200'] = {'description': 'Success'}

            operation['responses'] = responses
            path_item[method] = operation

        spec['paths'][path] = path_item

    return spec


# ---------------------------------------------------------------------------
# Entry point called by version-specific wrappers
# ---------------------------------------------------------------------------

def main(*, version, output, server_url, server_desc, info_version):
    controllers_glob = str(
        Path(__file__).parent.parent / 'controllers' / '*.php'
    )
    endpoints = load_endpoints(controllers_glob, version)
    if not endpoints:
        print(
            f'ERROR: No @route-v{version} annotations found. '
            'Are you running from www/api/?',
            file=sys.stderr,
        )
        sys.exit(1)

    spec     = build_openapi(endpoints, server_url, server_desc, info_version)
    out_path = Path(__file__).parent.parent / output
    out_path.write_text(json.dumps(spec, indent=2), encoding='utf-8')

    path_count = len(set(e['path'] for e in endpoints))
    op_count   = len(endpoints)
    print(f'Written {out_path} ({path_count} paths, {op_count} operations)')
