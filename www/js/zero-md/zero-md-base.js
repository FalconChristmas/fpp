/**
 * @typedef {object} ZeroMdRenderObject
 * @property {'styles'|'body'} [target]
 * @property {string} [text]
 * @property {string} [hash]
 * @property {boolean} [changed]
 * @property {string} [baseUrl]
 * @property {boolean} [stamped]
 */

export default class ZeroMdBase extends HTMLElement {
  get src() {
    return this.getAttribute('src')
  }

  set src(val) {
    val ? this.setAttribute('src', val) : this.removeAttribute('src')
  }

  get auto() {
    return !this.hasAttribute('no-auto')
  }

  get bodyClass() {
    const classes = this.getAttribute('body-class')
    return `markdown-body${classes ? ' ' + classes : ''}`
  }

  constructor() {
    super()
    try {
      this.version = __VERSION__
    } catch {} // eslint-disable-line no-empty
    this.template = ''
    const handler = (/** @type {*} */ e) => {
      if (e.metaKey || e.ctrlKey || e.altKey || e.shiftKey || e.defaultPrevented) return
      const a = e.target?.closest('a')
      if (a && a.hash && a.host === location.host && a.pathname === location.pathname)
        this.goto(a.hash)
    }
    this._clicked = handler.bind(this)
    this._observer = new MutationObserver(() => {
      this._observe()
      if (this.auto) this.render()
    })
    this._loaded = false
    /** @type {HTMLElement|ShadowRoot} */
    this.root = this
  }

  static get observedAttributes() {
    return ['src', 'body-class']
  }

  /**
   * @param {string} name
   * @param {string} old
   * @param {string} val
   */
  attributeChangedCallback(name, old, val) {
    if (this.ready && old !== val) {
      switch (name) {
        case 'body-class':
          this.root.querySelector('.markdown-body')?.setAttribute('class', this.bodyClass)
          break
        case 'src':
          if (this.auto) this.render()
      }
    }
  }

  async connectedCallback() {
    if (!this._loaded) {
      if (!this.hasAttribute('no-shadow')) this.root = this.attachShadow({ mode: 'open' })
      this.root.prepend(
        this.frag(`<div class="markdown-styles"></div><div class="${this.bodyClass}"></div>`)
      )
      await this.load()
      this._loaded = true
    }
    this.shadowRoot?.addEventListener('click', this._clicked)
    this._observer.observe(this, { childList: true })
    this._observe()
    this.ready = true
    this.fire('zero-md-ready')
    if (this.auto) this.render()
  }

  disconnectedCallback() {
    this.shadowRoot?.removeEventListener('click', this._clicked)
    this._observer.disconnect()
    this.ready = false
  }

  _observe() {
    this.querySelectorAll('template,script[type="text/markdown"]').forEach(
      (/** @type {*} */ node) =>
        this._observer.observe(node.content || node, {
          childList: true,
          subtree: true,
          attributes: true,
          characterData: true
        })
    )
  }

  /**
   * Async load function that runs after constructor. Like constructor, only runs once.
   * @returns {Promise<*>}
   */
  async load() {}

  /**
   * Async parse function that takes in markdown and returns the html-formatted string.
   * Can use any md parser you prefer, like marked.js
   * @param {ZeroMdRenderObject} obj
   * @returns {Promise<string>}
   */
  async parse({ text = '' }) {
    return text
  }

  /**
   * Scroll to heading id
   * @param {string} id
   */
  goto(id) {
    const ctx = this.shadowRoot || document
    id && ctx.getElementById(decodeURIComponent(id[0] === '#' ? id.slice(1) : id))?.scrollIntoView()
  }

  /**
   * Convert html string to document fragment
   * @param {string} html
   * @returns {DocumentFragment}
   */
  frag(html) {
    const tpl = document.createElement('template')
    tpl.innerHTML = html
    return tpl.content
  }

  /**
   * Compute 32-bit DJB2a hash in base36
   * @param {string} str
   * @returns {string}
   */
  hash(str) {
    let hash = 5381
    for (let index = 0; index < str.length; index++) {
      hash = ((hash << 5) + hash) ^ str.charCodeAt(index)
    }
    return (hash >>> 0).toString(36)
  }

  /**
   * Await the next tick
   * @returns {Promise<*>}
   */
  tick() {
    return new Promise((resolve) => requestAnimationFrame(resolve))
  }

  /**
   * Fire custom event
   * @param {string} name
   * @param {*} [detail]
   */
  fire(name, detail = {}) {
    this.dispatchEvent(new CustomEvent(name, { detail, bubbles: true }))
  }

  /**
   * Retrieve raw style templates and markdown strings
   * @param {ZeroMdRenderObject} obj
   * @returns {Promise<ZeroMdRenderObject>}
   */
  async read(obj) {
    const { target } = obj
    const results = (text = '', baseUrl = '') => {
      const hash = this.hash(text)
      const changed =
        this.root.querySelector(`.markdown-${target}`)?.getAttribute('data-hash') !== hash
      return { ...obj, text, hash, changed, baseUrl }
    }
    switch (target) {
      case 'styles': {
        const get = (query = '') => this.querySelector(query)?.innerHTML
        return results(
          (get('template[data-prepend]') ?? '') +
            (get('template:not([data-prepend],[data-append])') ?? this.template) +
            (get('template[data-append]') ?? '')
        )
      }
      case 'body': {
        if (this.src) {
          const response = await fetch(this.src)
          if (response.ok) {
            const getBaseUrl = () => {
              const a = document.createElement('a')
              a.href = this.src || ''
              return a.href.substring(0, a.href.lastIndexOf('/') + 1)
            }
            return results(await response.text(), getBaseUrl())
          } else {
            console.warn('[zero-md] error reading src', this.src)
          }
        }
        /** @type {HTMLScriptElement|null} */
        const script = this.querySelector('script[type="text/markdown"]')
        return results(script?.text || '')
      }
      default:
        return results()
    }
  }

  /**
   * Stamp parsed html strings into dom
   * @param {ZeroMdRenderObject} obj
   * @returns {Promise<ZeroMdRenderObject>}
   */
  async stamp(obj) {
    const { target, text = '', hash = '' } = obj
    const node = this.root.querySelector(`.markdown-${target}`)
    if (!node) return obj
    node.setAttribute('data-hash', hash)
    const frag = this.frag(text)
    /** @type {HTMLLinkElement[]} */
    const links = Array.from(frag.querySelectorAll('link[rel="stylesheet"]') || [])
    const whenLoaded = Promise.all(
      links.map(
        (link) =>
          new Promise((resolve) => {
            link.onload = resolve
            link.onerror = (err) => {
              console.warn('[zero-md] error loading stylesheet', link.href)
              resolve(err)
            }
          })
      )
    )
    node.innerHTML = ''
    node.append(frag)
    await whenLoaded
    return { ...obj, stamped: true }
  }

  /**
   * Start rendering
   * @param {{ fire?: boolean, goto?: string|false }} obj
   * @returns {Promise<*>}
   */
  async render({ fire = true, goto = location.hash } = {}) {
    const styles = await this.read({ target: 'styles' })
    const pending = styles.changed && this.stamp(styles)
    const md = await this.read({ target: 'body' })
    if (md.changed) {
      const parsed = this.parse(md)
      await pending
      await this.tick()
      await this.stamp({ ...md, text: await parsed })
    } else await pending
    await this.tick()
    const detail = { styles: styles.changed, body: md.changed }
    if (fire) this.fire('zero-md-rendered', detail)
    if (this.auto && goto) this.goto(goto)
    return detail
  }
}
