# Frontend Guidelines

FPP uses Bootstrap 5.3 with a custom FPP design system. When generating HTML/CSS,
follow these rules in strict priority order.

## Rule 1: Bootstrap utilities first

Before writing any `style=""` attribute or `<style>` block, exhaust Bootstrap's
built-in utility classes:

- Layout: `.d-flex`, `.d-grid`, `.gap-*`, `.row`, `.col-*`
- Spacing: `.mt-*`, `.mb-*`, `.ms-*`, `.me-*`, `.p-*`, `.gap-*`
- Display: `.d-none`, `.d-block`, `.d-inline`, `.d-lg-none`, etc.
- Typography: `.fw-bold`, `.fw-semibold`, `.fs-*`, `.text-truncate`
- Sizing: `.w-100`, `.h-100`, `.mw-100`, `.w-auto`

**If a Bootstrap utility exists for what you need, use it. Do not write custom CSS.**

## Rule 2: Never use inline styles for these patterns

The following inline styles are BANNED — use the Bootstrap equivalent instead:

| Banned inline style | Use instead |
| --- | --- |
| `style="display: none;"` | `class="d-none"` |
| `style="font-weight: bold;"` | `class="fw-bold"` |
| `style="width: 100%;"` | `class="w-100"` |
| `style="text-align: left/center/right;"` | `class="text-start/center/end"` |
| `style="margin-top: Xpx;"` | `class="mt-1"` through `mt-5` |
| `style="color: red/green/yellow;"` | `class="text-danger/success/warning"` |
| `style="font-size: X;"` | `class="fs-*"` or `<small>` tag |
| `width="100%"` on `<table>/<td>/<th>` | `class="w-100"` |

## Rule 3: Colors must use CSS variables or Bootstrap semantic classes

Never hardcode color values. FPP has two correct options:

**Option A — Bootstrap semantic classes (preferred for icons and text):**

```html
<i class="fas fa-exclamation-triangle text-danger"></i>
<span class="text-warning">Warning message</span>
<div class="alert alert-success">...</div>
```

**Option B — FPP CSS variables (for custom components only):**

```css
color: var(--fpp-text-primary);
background-color: var(--fpp-bg-card);
border-color: var(--fpp-border);
```

Available `--fpp-*` variable groups: `--fpp-text-*`, `--fpp-bg-*`, `--fpp-border-*`,
`--fpp-spacing-*`, `--fpp-fs-*`. See `css/fpp-system-design.css` for the full list.

**Never use:** hex values, named colors (`red`, `orange`), or `rgb()` literals in
generated HTML or inline styles.

## Rule 4: Custom CSS is strongly discouraged — push back and suggest removal

The default answer to any new custom class or inline style is **no**. If the user
asks for one, explain why it isn't needed and offer the Bootstrap or `--fpp-*`
alternative instead.

A `<style>` block or new CSS rule is only permitted when it meets ALL of these:

1. **No Bootstrap utility or component covers it** (genuinely novel behavior)
2. **It has a functional purpose** — a scroll boundary, a drag handle, a layout
   constraint driven by component behavior; never a cosmetic tweak
3. **It is named semantically** — `.log-viewer-scroll`, not `.my-special-div`
4. **It is NOT:** a font-size nudge, a minor color shift, a padding adjustment
   smaller than one Bootstrap spacing step, or anything matching an existing
   `--fpp-*` token

When existing code in a file contains a custom class or style that violates these
criteria, flag it and suggest the Bootstrap equivalent. Tables are a known problem
area: each page must not have its own table style — use Bootstrap's `.table`,
`.table-sm`, `.table-bordered`, `.table-hover`, and Bootstrap Table plugin classes
only. Remove page-specific table CSS when encountered.

## Rule 5: Structure — no gratuitous wrappers

- Do not add wrapper `<div>` elements solely to apply spacing. Apply spacing
  classes directly to the meaningful element.
- Do not create custom flex or grid containers when Bootstrap `.row`/`.col-*`
  or `.d-flex.gap-*` achieves the same result.
- Prefer semantic HTML (`<section>`, `<header>`, `<ul>`, `<dl>`) over generic
  `<div>` stacks where it improves readability.

## Rule 6: Dark mode compatibility

All generated HTML must work in both `[data-bs-theme="light"]` and
`[data-bs-theme="dark"]`. This is automatic when you follow Rules 3 and 4.
Never write CSS rules that assume a specific background or text color without
scoping them to the theme attribute.

## Rule 7: JavaScript-generated HTML follows the same rules

When building HTML strings in JavaScript, the same rules apply. Use Bootstrap
class names in string concatenation; do not use `style=` attributes for anything
on the banned list.

```javascript
// BAD
row += "<i class='fas fa-warning' style='color:red'></i>";

// GOOD
row += "<i class='fas fa-warning text-danger'></i>";
```

## Rule 8: Proactively flag existing violations when editing a file

When editing any file under `www/`, scan the surrounding code for violations of
these rules and report them before making your changes. Format findings as:

> **Style violation at line N:** `style="color: red"` → use `class="text-danger"`

Do not silently leave violations in place. If the fix is low-risk (a class swap,
removing a redundant style attribute), apply it in the same edit. If the fix
requires broader restructuring, list it as a follow-up suggestion.
