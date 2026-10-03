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

If a Bootstrap utility covers what you need, use it rather than custom CSS:
custom rules are what break dark mode and page-to-page consistency.

## Rule 2: Inline styles that have a Bootstrap equivalent

Use the class instead of the inline style:

| Inline style | Use instead |
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

## Rule 4: Custom CSS needs a functional reason

A `<style>` block, new CSS rule, or custom class is permitted only when it meets
all of these. When a request asks for one that doesn't, offer the Bootstrap or
`--fpp-*` alternative and explain why it fits better:

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
in the Rule 2 table.

```javascript
// BAD
row += "<i class='fas fa-warning' style='color:red'></i>";

// GOOD
row += "<i class='fas fa-warning text-danger'></i>";
```

## Rule 8: Fix violations near your change

When editing a file under `www/`, fix low-risk violations of these rules in the
code around your change (a class swap, removing a redundant style attribute) in
the same edit. List any that need broader restructuring, with line numbers, as
follow-up suggestions in your summary.
