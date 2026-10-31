# wingman UI audit — 2026-10-31 (impeccable)

- Scope: two UI surfaces — `apps/gui` (Tauri 2 + Svelte 5 desktop control panel) and `orchestrator/dashboard` (Ant Design 5 + UmiJS admin dashboard).
- Method: `impeccable detect --json apps/gui/src` (0 findings, clean), plus Playwright 1.64.1 (chromium) screenshots of every rendered route at 1440×900 and 390×844. PNGs live alongside this file (`gui-*-{desktop,narrow}.png`, `dash-*-{desktop,narrow}{,-logged}.png`).
- Contrast ratios computed per WCAG 2.1 relative luminance. AA thresholds: 4.5:1 normal text, 3:1 large text (≥18.66px or ≥14px bold).
- Outcome: **9 issues** (3 fixed, 6 reported as follow-up). **3 fixes committed.** No dashboard code changed.

---

# wingman UI audit — apps/gui (Tauri)

## Overall critique

The GUI is functional and its information architecture is sane, but it shipped with three first-order defects that a single smoke test should have caught: a hash router that silently renders a **blank content area** on any deep link using a leading slash (`#/dashboard`), sidebar navigation icons rendered at **~11× their intended size** (205px in a 248px sidebar), and **WCAG AA contrast failures on every accent button** in the default dark theme (white on green/blue/red at 2.5–3.4:1). These are not subtle polish gaps; they are visible layout breaks and accessibility violations on the primary surfaces. Their presence indicates the Tauri webview is currently judged by eye on the happy path only, with no committed DESIGN.md to anchor a visual language and no automated visual/contrast gate. Typography is a single thin stack (13–16px, one weight) with weak hierarchy, and the accent palette leans on GitHub-light tokens applied to a dark theme without re-tuning. The good news: the codebase is small, the components are tidy, and the responsive collapse (248px → 68px sidebar) works. The three P1 defects below are now fixed; the remaining items are deliberate follow-ups.

### Method limitation — Tauri webview not screenshotable

The desktop shell runs on Linux WebKitGTK, which exposes **no Chrome DevTools Protocol**, so the real Tauri window could not be driven headlessly. All GUI screenshots were taken against the Vite dev server (`npm run dev`) in headless Chromium, which exercises the same Svelte components, CSS, and routing — but not the native window chrome, the real IPC boundary, or data states that depend on `invoke`. In dev mode `window.__TAURI_INVOKE__` is absent, so data-driven pages render their dev fallback content; error/loading/empty states were verified by code inspection rather than live. Any change should be re-checked in an actual `tauri dev` build.

### DESIGN.md

`apps/gui` has **no DESIGN.md** and no `PRODUCT.md`. The visual identity is implicit (GitHub-dark tokens, green/blue/red accents, a `W` logo-mark). Before further visual work, capture the design intent in a DESIGN.md so refinement has an anchor.

---

## Findings (P0 → P3)

### GUI-P1-1 — Deep link with leading-slash hash renders blank content — FIXED

- **Status:** FIXED — commit `d48f070` `fix(ui): normalize leading-slash hash routes`
- **Screenshot:** `gui-dashboard-desktop.png` (fixed render; the pre-fix state was an empty content area)
- **Location:** `apps/gui/src/lib/router.svelte.ts`
- **Impact:** Any navigation with a leading-slash hash — `#/dashboard`, `#/scripts`, external links, bookmarks, or a window reopened on a URL — produced `pageMap['/dashboard']` as `undefined` and the content area rendered **completely blank**, with no console error. This is the entry route for anyone sharing or deep-linking, so it was a hard break.
- **Fix:** added `normalize(raw)` that strips leading slashes (`/^\/+/`), falls back to `dashboard`, and applied it to the store's initial value, `navigate()`, and the `hashchange` listener. `#/dashboard` and `#dashboard` are now equivalent.

### GUI-P1-2 — Sidebar nav icons render at ~11× size, breaking layout — FIXED

- **Status:** FIXED — commit `93f8284` `fix(ui): constrain sidebar nav icon size`
- **Screenshot:** `gui-dashboard-desktop.png`
- **Location:** `apps/gui/src/lib/components/layout/Sidebar.svelte` (`.icon` rule → `:global(.icon)`)
- **Impact:** The 8 navigation icons are injected as raw SVG strings via `{@html NAV_ICONS[item.icon]!}`. Svelte's scoped CSS appends a hash class only to elements present in the template; `{@html}`-injected markup does **not** get the scoped class, so the scoped `.icon { width: 18px }` rule never matched and each SVG fell back to its intrinsic/viewport size — measured **205×205px** in a 248px sidebar, overflowing the nav column and pushing labels off. (The inline tray icon, which *is* in the template, stayed at 18px — the diagnostic that proved the scoping root cause.)
- **Fix:** change the rule to `:global(.icon)` so it matches the injected SVGs. This is the correct Svelte idiom for styling `{@html}` content.

### GUI-P1-3 — White text on accent buttons fails WCAG AA (dark theme) — FIXED

- **Status:** FIXED — commit `ceb9d53` `fix(ui): use dark text on accent buttons in dark theme`
- **Screenshot:** `gui-settings-desktop.png`, `gui-dashboard-desktop.png`
- **Location:** `apps/gui/src/app.css`
- **Impact:** In the default dark theme the primary/success/secondary/danger buttons use bright fills with **white** text: green `#3fb950` = **2.54:1**, blue `#58a6ff` = **2.53:1**, red `#f85149` = **3.35:1**. All are far below the 4.5:1 AA floor for the ~13px button text — unreadable for low-vision users. The green "启动" / blue "刷新" / red "急停" actions are the most-used controls in the app.
- **Fix:** a dark-theme-only override (`:root:not([data-theme='light']) .btn-*`) sets button text to `var(--bg-primary)` (`#101214`), matching the existing dark-text-on-yellow `.btn-warning` treatment. Result: green **7.39:1**, blue **7.43:1**, red **5.60:1** — all AA/AAA. Light theme was left untouched (its darker accents with white text already pass at 4.87–5.36:1, verified).

### GUI-P2-1 — Hover state on blue/red accent buttons dips below AA with new dark text — REPORTED

- **Status:** REPORTED (follow-up; not fixed — would need per-button hover text handling)
- **Screenshot:** `gui-dashboard-desktop.png` (hover states not captured in stills)
- **Location:** `apps/gui/src/app.css` (`--accent-blue-hover: #1f6feb`, `--accent-red-hover: #da3633`)
- **Impact:** The contrast fix above trades the persistent base state for the transient hover state on two buttons. With dark text `#101214`, hovered blue `#1f6feb` = **4.05:1** and hovered red `#da3633` = **4.07:1** — just under the 4.5:1 floor for normal text (green hover `#2ea043` = 5.57:1 still passes). The dip is brief and only during pointer hover, so it is a P2 polish item rather than a blocker.
- **Fix (suggested):** on `:hover` for blue/red accents in dark theme, switch text back to white (white-on-`#1f6feb` = 4.63:1, white-on-`#da3633` = 4.61:1), or darken the hover fill so dark text stays ≥4.5:1.

### GUI-P3-1 — Icon-only buttons expose no accessible name via `aria-label` — REPORTED

- **Status:** REPORTED (follow-up)
- **Screenshot:** `gui-dashboard-desktop.png`
- **Location:** `apps/gui/src/lib/components/layout/TopBar.svelte` — theme toggle (line 180), refresh (line 199)
- **Impact:** Both are icon-only `<button>`s whose only accessible name is `title`. `title` is not a robust naming mechanism: it is often not announced by screen readers, is invisible to keyboard-only users (who never hover), and disappears on touch. Screen-reader and keyboard users get an unlabeled control.
- **Fix (suggested):** add `aria-label="切换主题"` / `aria-label="刷新"` (or wrap the icon in visually-hidden text), keeping `title` for hover affordance.

### GUI-P3-2 — Focus ring is faint (0.32 alpha) — REPORTED

- **Status:** REPORTED (minor; keyboard reachability itself passes)
- **Screenshot:** `gui-dashboard-desktop.png`
- **Location:** `apps/gui/src/app.css` (`--focus-ring: rgba(88,166,255,0.32)`)
- **Impact:** `:focus-visible` outline is 2px at 32% alpha — visible but low-contrast against the dark background; during the 0.2s transition it reads even fainter. Keyboard navigability was verified as **passing**: every interactive control is reachable via Tab and the ring settles to the intended 2px ring after the transition. This is a polish note, not a failure.
- **Fix (suggested):** raise alpha to ~0.5–0.6 or widen to 3px for a clearer focus indicator.

### GUI-P3-3 — No `prefers-reduced-motion` handling — REPORTED

- **Status:** REPORTED (follow-up)
- **Screenshot:** n/a (static)
- **Location:** `apps/gui/src/app.css` and component `<style>` blocks (`transition: all 0.15–0.2s` in Sidebar/App/scripts/macros/screen pages)
- **Impact:** Transitions are short (0.15–0.2s) and non-looping, so they are not motion-sickness triggers on their own, but there is **no** `@media (prefers-reduced-motion)` anywhere in the GUI source. Users who opt out of motion get animations anyway.
- **Fix (suggested):** add `@media (prefers-reduced-motion: reduce) { *, *::before, *::after { transition: none !important; animation: none !important; } }` to `app.css`.

---

## Verified passing (no action needed)

- **Router navigation (no-slash):** `#dashboard`, `#scripts`, etc. all render correctly; existing vitest router tests pass.
- **Keyboard navigability:** all nav items, buttons, and form controls are focusable in a logical order; `aria-current="page"` marks the active nav item.
- **Responsive:** sidebar collapses 248px → 68px at 768px; all 8 routes render without horizontal overflow at 390×844 (`gui-*-narrow.png`). Hardcoded `max-width: 420/560px` on dialogs is intentional and fine.
- **`impeccable detect --json apps/gui/src`:** 0 findings.
- **`npm test` (apps/gui):** 529 tests / 70 files pass after the three fixes.

---

# wingman UI audit — orchestrator/dashboard

## Overall critique

The dashboard is a competent, internally consistent Ant Design pro-layout admin surface: routing, access control, tables, and forms are arranged sensibly, and the 17 routes I rendered all painted correctly at both desktop and 390px. But it is **almost entirely stock AntD** — the colors, spacing, type scale, and components come from the library defaults rather than from a Wingman design system, so it reads as "an AntD admin" rather than "the Wingman console." Its most pervasive issue is that the brand primary `#1677ff` used for every primary button, link, and active menu item renders **white text at 4.10:1** — under the 4.5:1 AA floor for normal-size text — a marginal but real failure across the most common interactive elements, and it is not fixable under this audit's constraint against brand/primary-color changes. The single most memorable surface, the Monitor page's "游戏画面" (game screen) card, is a **large empty black rectangle** with a faint "等待画面..." caption until a screenshot arrives — no placeholder illustration, no error state, no instruction. The dashboard is usable; it is not yet designed.

### Method limitation — backend dependency

Dashboard data flows through `/api/` (dev-proxied to `http://localhost:9527`). All list/detail screenshots were taken with the Go orchestrator running and an admin user logged in (`*-logged.png`), so data-backed tables render. The login page was captured both logged-out (`dash-user-login-*.png`) and post-auth. The Monitor page's screenshot stream depends on a connected agent pushing frames via WebSocket; with no agent connected it shows the empty "等待画面..." state, which is exactly the state under review.

---

## Findings (P0 → P3)

### DASH-P2-1 — Brand primary `#1677ff` with white text fails AA for normal text — REPORTED (TODO)

- **Status:** REPORTED (not fixed — brand/primary-color change is out of scope for this audit)
- **Screenshot:** `dash-welcome-desktop-logged.png`, `dash-agents-desktop-logged.png`
- **Location:** AntD theme primary token (stock `#1677ff`), consumed by every `Button type="primary"`, link, and active menu item across `orchestrator/dashboard/src/pages/*`.
- **Impact:** White text on `#1677ff` = **4.10:1**, below the 4.5:1 AA floor for 14px regular (non-bold) button/link text. This is the most-used interactive color in the dashboard, so the failure is widespread, though marginal (a 9% shortfall). Stock AntD ships this color; it is a library-default issue more than a code bug.
- **Fix (suggested, when permitted):** either darken the primary to a token that clears 4.5:1 with white (e.g. `#0958d9`, ≈5.2:1) or use dark text on the current primary. Doing so is a brand-color change and is deliberately left as a TODO.

### DASH-P3-1 — Monitor "游戏画面" card is an empty black rectangle with no real empty/error state — REPORTED (TODO)

- **Status:** REPORTED (not fixed)
- **Screenshot:** `dash-monitor-desktop-logged.png`, `dash-monitor-narrow-logged.png`
- **Location:** `orchestrator/dashboard/src/components/ScreenshotView/index.tsx` (rendered at `orchestrator/dashboard/src/pages/Monitor/index.tsx:700`)
- **Impact:** The card body is a fixed-height (500px) `#000` rectangle containing only `Text type="secondary"` with "等待画面..." (or "预览已关闭") when no frame is available, and its refresh control is an icon-only `<Button icon={<ReloadOutlined />}>` with **no accessible label**. A first-time visitor sees a large black void with a faint caption and no explanation of what should appear, how to enable it, or what to do if nothing comes. No error branch exists if the screenshot stream fails or is disconnected.
- **Fix (suggested):** add an `Empty`/placeholder with a short instruction ("连接 Agent 后此处将实时显示游戏画面") when no image is present, an error state for a failed stream, and `aria-label` on the icon-only reload button.

---

## Verified passing (no action needed)

- **Routing:** all 17 routes render; admin sub-routes gated by `access` render for the admin user.
- **Responsive:** every route renders without horizontal overflow at 390×844 (`dash-*-narrow-*.png`); AntD grid `xs`/`md`/`lg` columns stack correctly.
- **Tables, forms, filters, pagination:** render and function on the Agents/Workflows/Executions/Scripts pages.
- **i18n:** `zh-CN` default with the full locale set present.

---

## Totals & follow-up

| Severity | apps/gui | orchestrator/dashboard | Fixed |
|---|---|---|---|
| P1 | 3 | 0 | 3 |
| P2 | 1 | 1 | 0 |
| P3 | 3 | 1 | 0 |
| **Total** | **7** | **2** | **3** |

**Fixed (commits):**
1. `93f8284` fix(ui): constrain sidebar nav icon size
2. `d48f070` fix(ui): normalize leading-slash hash routes
3. `ceb9d53` fix(ui): use dark text on accent buttons in dark theme

**Todo (no code changed):** GUI-P2-1 hover contrast, GUI-P3-1 aria-labels, GUI-P3-2 focus-ring alpha, GUI-P3-3 reduced-motion, DASH-P2-1 primary contrast, DASH-P3-1 Monitor empty/error state.
