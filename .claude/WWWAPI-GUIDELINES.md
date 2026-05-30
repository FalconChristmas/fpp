# FPP Web API Guidelines

These rules apply whenever you add, modify, or review routes in `www/api/`. Read this
file before making any changes to `index.php` or `controllers/*.php`.

## Rule 1 — Route registration lives entirely in `index.php`

- `dispatch_all(path, method, handler)` registers the route for **all active API
  versions** in one call. Use it whenever the route has the **same HTTP verb** across all
  versions being registered.
- Use **explicit** `dispatch_get()` / `dispatch_post()` (with the version-prefixed path)
  only when a route must differ by HTTP verb between versions — for example when a
  legacy version accepted GET for a state-mutating operation and a newer version corrects
  it to POST. Always append inline comments identifying the version and verb:
  `// legacy: GET` / `// v2: POST`.
- A single-version explicit dispatch is also valid for routes that exist only in one
  version (e.g. a route present only in the legacy surface while it awaits deprecation).
- Group routes under a `// Resource Name` comment block that matches the resource area.
- **Never** `require_once` a controller — Limonade auto-loads every `controllers/*.php`
  on `run()`. Only `controllers/helpers.php` is explicitly required at the top of
  `index.php` because it is needed before routing starts.

**Example — same verb across all versions:**

```php
dispatch_all('/pipewire/control/status', 'get', 'PWCtl_GetStatus');
```

**Example — verb differs between versions:**

```php
dispatch_get('/system/reboot', 'RebootDevice');          // legacy: GET
dispatch_post('/v2/system/reboot', 'RebootDevice');      // v2: POST
```

**Example — exists only in legacy while pending deprecation:**

```php
dispatch_get('/legacy/thing', 'LegacyThing');            // legacy only; deprecated
```

---

## Rule 2 — Handler naming: PascalCase with semantic verb prefix

Public endpoint handlers (functions registered as route callbacks) use **PascalCase** with
a verb that describes the operation:

| Verb prefix | When to use |
| --- | --- |
| `Get` / `List` | Read a resource or collection |
| `Post` / `Create` | Create a new resource |
| `Save` / `Update` / `Put` | Replace or update an existing resource |
| `Delete` / `Remove` | Delete a resource |
| `Apply` / `Reload` / `Restart` | Trigger a side-effect action |

A module-scoped prefix (`PWCtl_`, `GitCtl_`) is acceptable when a controller file is a
self-contained facade, but the overall name must still be PascalCase after the prefix.

Non-routed internal helpers may use `camelCase` or `snake_case` — they are never
dispatched, so naming style is relaxed.

**Versioned handlers:** The **unversioned name is always the latest implementation.**
When a breaking change is introduced, rename the old handler to `FunctionName_vX`
(where `X` is the version it was current for), update its registrations to point to the
versioned name, and implement the new behavior in the unversioned `FunctionName`. This
way the current canonical implementation never carries a version suffix.

---

## Rule 3 — Every dispatched function requires a PHPDoc block

Minimum required doc block:

```php
/**
 * Short imperative title (becomes the OpenAPI summary).
 *
 * Optional longer description separated by a blank line.
 * It can span as many lines as needed — the blank line between
 * the summary and this paragraph is what separates them.
 * All lines here form a single description in the generated spec.
 *
 * @route-v1 GET /resource/{Id}
 * @route-v2 GET /resource/{Id}
 * @response 200 Description of success payload
 * ```json
 * {"status": "OK", "id": 1}
 * ```
 */
function GetResource() { ... }
```

**Tag reference:**

| Tag | Required | Notes |
| --- | --- | --- |
| `@route-vN METHOD /path/{Param}` | Yes — one per version | Prefix-free path. `{Param}` = OpenAPI path parameter (PascalCase). |
| `@response [statusCode] description` | Yes | Default status = 200 when omitted. Fenced body block is optional. |
| `@body {"json": "example"}` | POST/PUT only | Shared unless `@body-vN` overrides for a specific version. |
| `@param type name Description` | No | Documents a query parameter. |
| `@badge "Label" level` | No | Levels: `success`, `warning`, `critical`, `info`. |
| `@badge-vN "Label" level` | No | Version-specific badge (e.g. `@badge-v1 "DEPRECATED" warning`). |
| `@deprecated-vN` | No | Marks the route deprecated in version N's spec. |

**Prose rules:**

- One prose paragraph → becomes the description; summary falls back to the route slug.
- Two or more prose paragraphs → first = summary, rest = description.
- Path parameters must use `{PascalCase}` in `@route-vN` tags to match OpenAPI convention.

---

## Rule 4 — Helper functions to use in controllers

All three are available in every controller (Limonade + helpers.php):

| Function | Source | Purpose |
| --- | --- | --- |
| `getJsonBody(bool $required = true)` | `controllers/helpers.php` | Decode POST body. Halts with 400 if required and missing/invalid. Returns `array\|null`. |
| `json($data)` | `lib/limonade.php` | Set `Content-Type: application/json` and return `json_encode($data)`. Always use for JSON responses. |
| `params('Key')` | `lib/limonade.php` | Return a route segment or query param value. |

---

## Rule 5 — Rebuild OpenAPI specs after every annotation change

Run the generator for every active API version from the `www/api/` directory:

```bash
cd www/api && python3 tools/generate_openapi_v1.py && python3 tools/generate_openapi_v2.py
```

Add a new version-specific generator call whenever a new API version is introduced (see
`www/api/README.md` for how to add a version). The pattern is always:

```bash
python3 tools/generate_openapi_v<N>.py
```

- All generated `v<N>/openapi.json` files are **tracked artifacts** — commit them along
  with the controller change.
- **Never edit any `openapi.json` by hand.** The generator overwrites it completely.
- Optional lint: `npx @redocly/cli lint --config openapi.lint.yaml v<N>/openapi.json`

---

## Rule 6 — Version semantics

The legacy API surface (the unprefixed `/api/` routes) is the compatibility layer and is
planned for eventual deprecation as a whole. Newer versions introduce corrections and new
capabilities; their scope is not bounded by what the legacy surface does.

When a route's behavior, signature, or HTTP verb differs between versions:

- Register each version explicitly with the appropriate verb and path prefix.
- Annotate with version-specific `@route-vN` tags reflecting the actual method and path.
- Use `@badge-vN "DEPRECATED" warning` and `@deprecated-vN` on any version-specific route
  that is being phased out.

Only split into separate handler functions (`Foo_vX` / `Foo`) when the parameter contract
**genuinely differs** between versions. The unversioned name (`Foo`) is always the latest
implementation; older registrations point to the versioned name (`Foo_vX`).
