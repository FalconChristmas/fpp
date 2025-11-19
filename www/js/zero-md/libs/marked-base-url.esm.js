/**
 * Bundled by jsDelivr using Rollup v2.79.2 and Terser v5.39.0.
 * Original file: /npm/marked-base-url@1.1.8/src/index.js
 *
 * Do NOT use SRI with dynamically generated files! More information: https://www.jsdelivr.com/using-sri-with-dynamic-files
 */
function t(t){t=t.trim().replace(/\/+$/,"/");const e=/^[\w+]+:\/\//,r=e.test(t),h="http://__dummy__",f=new URL(t,h),s=16+(t.startsWith("/")?0:1);return{walkTokens(h){if(["link","image"].includes(h.type)&&!e.test(h.href)&&!h.href.startsWith("#"))if(r)try{h.href=new URL(h.href,t).href}catch{}else{if(h.href.startsWith("/"))return;try{const t=new URL(h.href,f).href;h.href=t.slice(s)}catch{}}}}}export{t as baseUrl};export default null;
//# sourceMappingURL=/sm/715dd9611a248e3a9948595a172f2d7a447e27831747b82d52f9f1c58206ff5e.map