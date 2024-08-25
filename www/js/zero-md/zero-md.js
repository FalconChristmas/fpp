import ZeroMdBase from './zero-md-base.js'
import katexExtension from './katex-extension.js'
import { STYLES, LOADERS } from './presets.js'

/** @type {*} */ let hljsHoisted
/** @type {*} */ let mermaidHoisted
/** @type {*} */ let katexHoisted
let uid = 0

/**
 * Extends ZeroMdBase with marked.js, syntax highlighting, math and mermaid features
 */
export default class ZeroMd extends ZeroMdBase {
  async load(loaders = {}) {
    /** @type {*} */ const {
      marked,
      markedBaseUrl,
      markedHighlight,
      markedGfmHeadingId,
      markedAlert,
      hljs,
      mermaid,
      katex,
      katexOptions = { nonStandard: true, throwOnError: false }
    } = { ...LOADERS, ...loaders }
    this.template = STYLES.preset()
    const modules = await Promise.all([
      marked(),
      markedBaseUrl(),
      markedGfmHeadingId(),
      markedAlert(),
      markedHighlight()
    ])
    this.marked = modules[0]
    this.setBaseUrl = modules[1]
    const parseKatex = async (
      /** @type {string} */ text,
      /** @type {boolean|undefined} */ displayMode
    ) => {
      if (!katexHoisted) katexHoisted = await katex()
      return `${katexHoisted.renderToString(text, { ...katexOptions, displayMode })}${displayMode ? '' : '\n'}`
    }
    this.marked.use(
      modules[2](),
      modules[3](),
      {
        ...modules[4]({
          async: true,
          highlight: async (/** @type {string} */ code, /** @type {string} */ lang) => {
            if (lang === 'mermaid') {
              if (!mermaidHoisted) {
                mermaidHoisted = await mermaid()
                mermaidHoisted.initialize({ startOnLoad: false })
              }
              const { svg } = await mermaidHoisted.render(`mermaid-svg-${uid++}`, code)
              return svg
            }
            if (lang === 'math') return await parseKatex(code, true)
            if (!hljsHoisted) hljsHoisted = await hljs()
            return hljsHoisted.getLanguage(lang)
              ? hljsHoisted.highlight(code, { language: lang }).value
              : hljsHoisted.highlightAuto(code).value
          }
        }),
        renderer: {
          code: (/** @type {*} */ { text, lang }) => {
            if (lang === 'mermaid') return `<div class="mermaid">${text}</div>`
            if (lang === 'math') return text
            return `<pre><code class="hljs${lang ? ` language-${lang}` : ''}">${text}\n</code></pre>`
          }
        }
      },
      {
        ...katexExtension(katexOptions),
        walkTokens: async (/** @type {*} */ token) => {
          if (['inlineKatex', 'blockKatex'].includes(token.type))
            token.text = await parseKatex(token.text, token.type === 'blockKatex')
        }
      }
    )
  }

  /** @param {import('./zero-md-base.js').ZeroMdRenderObject} _obj */
  async parse({ text, baseUrl }) {
    this.marked.use(this.setBaseUrl(baseUrl || ''))
    return this.marked.parse(text)
  }
}
