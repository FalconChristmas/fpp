/**
 * Bundled by jsDelivr using Rollup v2.79.2 and Terser v5.39.0.
 * Original file: /npm/marked-gfm-heading-id@4.1.3/src/index.js
 *
 * Do NOT use SRI with dynamically generated files! More information: https://www.jsdelivr.com/using-sri-with-dynamic-files
 */
import e from './github-slugger.esm.js';
let r = new e(),
	n = [];
const t = /&(#(?:\d+)|(?:#x[0-9A-Fa-f]+)|(?:\w+));?/gi;
function o (e) {
	return e.replace(t, (e, r) =>
		'colon' === (r = r.toLowerCase())
			? ':'
			: '#' === r.charAt(0)
			? 'x' === r.charAt(1)
				? String.fromCharCode(parseInt(r.substring(2), 16))
				: String.fromCharCode(+r.substring(1))
			: ''
	);
}
function s ({ prefix: e = '', globalSlugs: t = !1 } = {}) {
	return {
		headerIds: !1,
		hooks: { preprocess: e => (t || a(), e) },
		useNewRenderer: !0,
		renderer: {
			heading ({ tokens: t, depth: s }) {
				const i = this.parser.parseInline(t),
					a = o(i)
						.trim()
						.replace(/<[!\/a-z].*?>/gi, ''),
					u = s,
					l = `${e}${r.slug(a.toLowerCase())}`,
					p = { level: u, text: i, id: l, raw: a };
				return n.push(p), `<h${u} id="${l}">${i}</h${u}>\n`;
			}
		}
	};
}
function i () {
	return n;
}
function a () {
	(n = []), (r = new e());
}
export {
	i as getHeadingList,
	s as gfmHeadingId,
	a as resetHeadings,
	o as unescape
};
export default null;
//# sourceMappingURL=/sm/b95af1aa274394ff0b7fd28967de363f63af9d1d35c7dd154195b01c93b852ea.map
