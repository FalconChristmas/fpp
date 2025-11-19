const localPath = (/** @type {string} */ path) => `/js/zero-md/${path}`;
const link = (
	/** @type {string} */ href,
	/** @type {string|undefined} */ attrs
) => `<link rel="stylesheet" href="${href}"${attrs ? ` ${attrs}` : ''}>`;
const load = async (/** @type {string} */ url, name = 'default') =>
	(await import(/* @vite-ignore */ url))[name];

export const STYLES = {
	HOST: '<style>:host{display:block;position:relative;contain:content;}:host([hidden]){display:none;}.markdown-body .markdown-alert{padding:0.25rem 0 0 1rem;}</style>',
	MARKDOWN: link(localPath('css/github-markdown.min.css')),
	MARKDOWN_LIGHT: link(localPath('css/github-markdown-light.min.css')),
	MARKDOWN_DARK: link(localPath('css/github-markdown-dark.min.css')),
	HIGHLIGHT_LIGHT: link(localPath('css/github.min.css')),
	HIGHLIGHT_DARK: link(localPath('css/github-dark.min.css')),
	HIGHLIGHT_PREFERS_DARK: link(
		localPath('css/github-dark.min.css'),
		`media="(prefers-color-scheme:dark)"`
	),
	KATEX: link(localPath('css/katex.min.css')),
	preset (theme = '') {
		const {
			HOST,
			MARKDOWN,
			MARKDOWN_LIGHT,
			MARKDOWN_DARK,
			HIGHLIGHT_LIGHT,
			HIGHLIGHT_DARK,
			HIGHLIGHT_PREFERS_DARK,
			KATEX
		} = this;
		const get = (/** @type {string} */ sheets) => `${HOST}${sheets}${KATEX}`;
		switch (theme) {
			case 'light':
				return get(MARKDOWN_LIGHT + HIGHLIGHT_LIGHT);
			case 'dark':
				return get(MARKDOWN_DARK + HIGHLIGHT_DARK);
			default:
				return get(MARKDOWN + HIGHLIGHT_LIGHT + HIGHLIGHT_PREFERS_DARK);
		}
	}
};

export const LOADERS = {
	marked: async () => {
		const Marked = await load(localPath('libs/marked.esm.js'), 'Marked');
		return new Marked({ async: true });
	},
	markedBaseUrl: () =>
		load(localPath('libs/marked-base-url.esm.js'), 'baseUrl'),
	markedHighlight: () =>
		load(localPath('libs/marked-highlight.esm.js'), 'markedHighlight'),
	markedGfmHeadingId: () =>
		load(localPath('libs/marked-gfm-heading-id.esm.js'), 'gfmHeadingId'),
	markedAlert: () => load(localPath('libs/marked-alert.esm.js')),
	hljs: () => load(localPath('libs/highlight.min.js')),
	mermaid: () => load(localPath('libs/mermaid.min.js')),
	katex: () => load(localPath('libs/katex.mjs'))
};
