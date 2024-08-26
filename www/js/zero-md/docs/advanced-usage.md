# &lt;zero-md&gt;

![version](https://img.shields.io/npm/v/zero-md) ![license](https://img.shields.io/npm/l/zero-md)
![stars](https://img.shields.io/github/stars/zerodevx/zero-md?style=flat&color=yellow)
![downloads](https://img.shields.io/jsdelivr/npm/hm/zero-md)
![old](<https://img.shields.io/jsdelivr/gh/hm/zerodevx/zero-md?label=jsdelivr(old)&color=lightgray>)

## Advanced Usage

> [!WARNING]  
> By default, `<zero-md>` **does not sanitise** the output HTML. If you're processing markdown from
> **unknown or untrusted** sources, please use a HTML sanitisation library
> [as shown below](#sanitise-html-output).

- [API](#api)
- [Extensibility](#extensibility)

## API

### Element attributes

The following attributes apply to `<zero-md>` element.

| Attribute  | Type    | Description                                                                                                      |
| ---------- | ------- | ---------------------------------------------------------------------------------------------------------------- |
| src        | String  | URL to external markdown file.                                                                                   |
| no-auto    | Boolean | If set, disables auto-rendering. Call the `render()` [function](#the-render-function) on that instance manually. |
| no-shadow  | Boolean | If set, renders and stamps into **light DOM** instead. Please know what you are doing.                           |
| body-class | String  | Class names forwarded to `.markdown-body` block.                                                                 |

Notes:

1. Standard [CORS](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS) rules apply; the `src`
   file (if fetched from another origin) should be served with the corresponding
   `Access-Control-Allow-Origin` headers.

2. `no-shadow` is immutable; it must be declared on element creation time and should not be
   dynamically changed.

### Style template attributes

The following attributes apply to `<template>` tags.

| Attribute    | Type    | Description                                 |
| ------------ | ------- | ------------------------------------------- |
| data-append  | Boolean | Apply template **after** default template.  |
| data-prepend | Boolean | Apply template **before** default template. |

If left unset, `<template>` overrides the default template.

### Events

The following convenience events are dispatched.

| Event Name         | Detail                             | Description                                          |
| ------------------ | ---------------------------------- | ---------------------------------------------------- |
| `zero-md-ready`    | `undefined`                        | Instance setup complete and ready to render.         |
| `zero-md-rendered` | `{styles: boolean, body: boolean}` | Render completes; returns what was stamped into DOM. |

### The `render()` function

By default, `render()` is automatically called once when the component is ready, and whenever a
mutation is detected. To control when rendering happens, set `no-auto` on this instance.

```html
<zero-md src="example.md" no-auto></zero-md>
<script>
  // Render once when component is ready, but not when mutated
  addEventListener('zero-md-ready', (e) => e.target.render())
</script>
```

The function returns a promise that resolves to `{styles: boolean, body: boolean}` detailing which
block was stamped into DOM.

## Extensibility

Extending functionality should be done the spec-compliant way (i.e. by extending the `ZeroMd` class)
in line with the spirit of the [Custom Elements V1 spec](https://www.w3.org/TR/custom-elements/).

### The `load()` function

This runs **once** per element creation, just like the `constructor()`, so this will be a good place
to set default values.

#### Global config

Change the default template globally like so:

<!-- prettier-ignore -->
```html
<script type="module">
  import ZeroMd from 'https://cdn.jsdelivr.net/npm/zero-md@3'

  customElements.define('zero-md', class extends ZeroMd {
    async load() {
      await super.load()
      this.template = `
<style>:host { display: block; }</style>
<link rel="stylesheet" href="https://example.com/markdown-styles.css" />
<link rel="stylesheet" href="https://example.com/highlight-styles.css" />`
    }
  })
</script>
```

Or force `light` (or `dark`) theme:

<!-- prettier-ignore -->
```html
<script type="module">
  import ZeroMd, { STYLES } from 'https://cdn.jsdelivr.net/npm/zero-md@3'

  customElements.define('zero-md', class extends ZeroMd {
    async load() {
      await super.load()
      this.template = STYLES.preset('light') // or STYLES.preset('dark')
    }
  })
</script>
```

#### Adding `marked` extensions

Layer additional [marked extensions](https://marked.js.org/using_advanced#extensions) you may need
like so:

<!-- prettier-ignore -->
```html
<script type="module">
  // Add GFM Footnote functionality
  // https://github.com/bent10/marked-extensions/tree/main/packages/footnote
  import markedFootnote from 'https://cdn.jsdelivr.net/npm/marked-footnote@1/+esm'
  import ZeroMd from 'https://cdn.jsdelivr.net/npm/zero-md@3'

  customElements.define('zero-md', class extends ZeroMd {
    async load() {
      await super.load()
      this.marked.use(markedFootnote())
    }
  })
</script>
```

#### Async loaders

Dependency libraries can be passed into the `load()` function through async loaders looking like:

```js
const lib = () => import('/path/to/lib')
```

This may be useful if you like to change CDN hosts, or for web projects that require libraries to be
self-hosted (or bundled together).

```js
import ZeroMd from 'zero-md'
import { Marked } from 'marked'

customElements.define('zero-md', class extends ZeroMd {
  async load() {
    await super.load({
      marked: () => new Marked(), // self-hosted
      hljs: () => import('path/to/lib'), // change CDN url
      ...
    })
  }
})
```

### The `parse()` function

This is called **during every render** to parse markdown into its HTML string representation, so
this will be a good place to run any pre or post processing task.

#### Sanitise HTML output

If you are processing potentially unsafe markdown, always use a client-side sanitisation library.

<!-- prettier-ignore -->
```html
<head>
  <script defer src="https://cdn.jsdelivr.net/npm/dompurify@3/dist/purify.min.js"></script>
  <script type="module">
    import ZeroMd from 'https://cdn.jsdelivr.net/npm/zero-md@3'

    customElements.define('zero-md', class extends ZeroMd {
      async parse(obj) {
        const parsed = await super.parse(obj)
        return window.DOMPurify.sanitize(parsed)
      }
    })
  </script>
</head>
<body>
  <zero-md src="https://danger.com/unsafe.md">
</body>
```
