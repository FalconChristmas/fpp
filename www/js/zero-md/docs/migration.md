# &lt;zero-md&gt;

![version](https://img.shields.io/npm/v/zero-md) ![license](https://img.shields.io/npm/l/zero-md)
![stars](https://img.shields.io/github/stars/zerodevx/zero-md?style=flat&color=yellow)
![downloads](https://img.shields.io/jsdelivr/npm/hm/zero-md)
![old](<https://img.shields.io/jsdelivr/gh/hm/zerodevx/zero-md?label=jsdelivr(old)&color=lightgray>)

## Migration Guide

This documents migration from `v2` to `v3`. The motivations for `v3` include spec compliance, first
class support for math and diagrams, and upgrading to ESM consumption of libraries.

> [!WARNING]  
> These are breaking changes! Please read through the list carefully!

#### 1. New CDN URL

```text
https://cdn.jsdelivr.net/npm/zero-md@3?register
```

By default, the component definition is **not** defined into the `CustomElementRegistry`; opt-in to
auto-register the custom element with the `?register` query param.

#### 2. Syntax highlighting

[`Prismjs`](https://github.com/PrismJS/prism) is un-maintained and has been replaced by
[`highlightjs`](https://github.com/highlightjs/highlight.js).

#### 3. Merging style templates

`data-merge="append"` and `data-merge="prepend"` template tag attributes renamed to `data-append`
and `data-prepend` respectively.

#### 4. Normalising indentation

`data-dedent` script tag attribute is deprecated; please use **spec-compliant** markdown in your
inline-markdown.

#### 5. Manual render

`manual-render` element attribute has been renamed to `no-auto`.

#### 6. Global config

The `ZeroMdConfig` global is deprecated. Set global config the
[spec-compliant way](./advanced-usage.md#global-config) instead.

#### 7. Events

The `zero-md-error` event is deprecated - read which [events](./advanced-usage.md#events) are fired.

#### 8. Legacy builds

Web components are standard now so legacy (transpiled) builds will cease.
