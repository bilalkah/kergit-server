export const qs = (sel, root = document) => root.querySelector(sel);
export const qsa = (sel, root = document) => Array.from(root.querySelectorAll(sel));
export const el = (tag, cls) => { const n = document.createElement(tag); if (cls) n.className = cls; return n; };