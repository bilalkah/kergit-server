(() => {
  "use strict";

  // Fits display headings to their column. Markup:
  //   <h2 class="fit-heading">   (CSS --fit-floor tunes the level switch)
  //     <span class="hl"><span class="sub">…</span> <span class="sub">…</span></span>
  //     <span class="hl">…</span>
  //   </h2>
  // Level 1 keeps every .hl on one line, shrinking the CSS design size just
  // enough to fit. If that would drop below floor × design size, level 2 lets
  // each .hl break between its .sub units and fits the widest unit instead.

  const headings = [...document.querySelectorAll(".fit-heading")];
  if (!headings.length || !("ResizeObserver" in window)) return;

  const SAFETY = 0.985;
  const DEFAULT_FLOOR = 0.65;
  const MAX_PASSES = 4;
  const fittedWidth = new WeakMap();

  function textWidth(node) {
    const range = document.createRange();
    range.selectNodeContents(node);
    return range.getBoundingClientRect().width;
  }

  // Largest size <= designPx at which the widest of `nodes` fits on one line.
  // Measured at the candidate size itself and refined, because system fonts
  // with optical sizing (e.g. SF Pro) do not scale their widths linearly.
  function solve(heading, nodes, designPx, target) {
    let size = designPx;
    for (let pass = 0; pass < MAX_PASSES; pass += 1) {
      heading.style.fontSize = `${size}px`;
      const width = Math.max(...nodes.map(textWidth));
      if (width <= target && (size >= designPx || width >= target * 0.99)) break;
      size = Math.min(designPx, size * (target / width) * (width > target ? 0.999 : 1));
    }
    return size;
  }

  function fit(heading) {
    heading.style.removeProperty("font-size");
    delete heading.dataset.fitLevel;

    const box = heading.clientWidth;
    fittedWidth.set(heading, box);
    if (!box) return;

    const designPx = parseFloat(getComputedStyle(heading).fontSize);
    const floor = parseFloat(getComputedStyle(heading).getPropertyValue("--fit-floor")) || DEFAULT_FLOOR;
    const target = box * SAFETY;
    const lines = [...heading.querySelectorAll(".hl")];
    const units = lines.flatMap((line) => {
      const subs = [...line.querySelectorAll(".sub")];
      return subs.length ? subs : [line];
    });

    heading.classList.add("fit-measuring");
    const wholeLines = solve(heading, lines, designPx, target);
    const level = wholeLines >= designPx * floor ? 1 : 2;
    const size = level === 1 ? wholeLines : solve(heading, units, designPx, target);
    heading.classList.remove("fit-measuring");

    heading.style.fontSize = `${size.toFixed(2)}px`;
    heading.dataset.fitLevel = String(level);
  }

  function fitAll() {
    headings.forEach(fit);
  }

  // Refit when a heading's width changes; its own height changes when the
  // font size does, and that must not trigger another pass.
  const observer = new ResizeObserver((entries) => {
    entries.forEach(({ target }) => {
      if (Math.abs(target.clientWidth - (fittedWidth.get(target) ?? -1)) > 0.5) fit(target);
    });
  });

  // Design sizes are viewport-relative (vw clamps, media queries), so a
  // heading whose width is capped can still need a refit on window resize.
  let frame = 0;
  window.addEventListener("resize", () => {
    cancelAnimationFrame(frame);
    frame = requestAnimationFrame(fitAll);
  });

  fitAll();
  headings.forEach((heading) => observer.observe(heading));
  document.fonts?.ready.then(fitAll);
})();
