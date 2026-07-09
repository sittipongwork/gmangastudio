# gmangastudio

SwiftUI app for manga studio work. **Primary targets: macOS and iPad (iPadOS).** Phone is secondary unless asked. Xcode project; keep diffs small and reuse Apple frameworks before adding anything new.

## Project Structure

```text
gmangastudio/                    App sources (SwiftUI) — UI layer
  gmangastudioApp.swift          App entry (`@main`), window / scene setup
  ContentView.swift              Root shell (routes into page COPs)
  Assets.xcassets/               Colors, app icon
  Shared/                        Cross-page helpers (only when reused by 2+ pages)
  DrawingEditor/                 Drawing page COP (imports IllusStudioFramework)
    Views/DrawingEditorView.swift
    ViewModels/DrawingEditorViewModel.swift
  <PageName>/                    One folder per page / feature (page COP)
    Views/  ViewModels/  Models/
IllusStudioFramework/            Core canvas engine (C++) — separate framework target
  README.md                      Architecture (read before engine work)
  docs/ROADMAP.md                Tasks & status (T0…T6) — single checklist
  docs/canvas_document.md        Page setting, zoom/pan, export
  docs/layer.md                  Layer management
  docs/brush_drawing.md          Hybrid brush / vector design + image import
  docs/history.md                Undo / redo / timelapse
  docs/animation_timeline.md     Animation & timeline
  IllusStudioFramework.h         Umbrella (C++)
  CanvasEditor.hpp               Public C++ API (Swift–C++ interop)
  module.modulemap
  src/
    CanvasEditor.cpp             Public API impl (pimpl)
    IllusStudioCanvasEditor.*    Internal facade
    document/ layers/ render/ math/
    render/MetalRenderer.*       Metal present texture (T6)
  third_party/metal-cpp/         Apple metal-cpp headers
gmangastudioTests/               Unit tests (mirror page folders when useful)
gmangastudioUITests/             UI tests
gmangastudio.xcodeproj/          Xcode project
```

### Page COP (Composition Of Page)

Each screen/feature is a **page COP**: one folder named `<PageName>` that owns its View + ViewModel (+ Models when needed).

| Piece | Path | Role |
|-------|------|------|
| Page folder | `gmangastudio/<PageName>/` | Boundary for that screen/feature |
| View | `…/Views/<PageName>View.swift` | Layout + bindings; no heavy logic |
| Child views | `…/Views/<Name>View.swift` | UI slices used only by this page |
| ViewModel | `…/ViewModels/<PageName>ViewModel.swift` | State, actions, side effects |
| Models | `…/Models/<Name>.swift` | Page-local data types |

**Naming**

- Folder + types: `PascalCase` page name, e.g. `Canvas`, `ProjectBrowser`, `Export`
- View: `<PageName>View` (e.g. `CanvasView`)
- ViewModel: `<PageName>ViewModel` (e.g. `CanvasViewModel`)
- Prefer one ViewModel per page root; split only when a child has real independent state

**Example**

```text
gmangastudio/
  Canvas/
    Views/
      CanvasView.swift
      CanvasToolbarView.swift
    ViewModels/
      CanvasViewModel.swift
    Models/
      Stroke.swift
```

**Rules**

- New UI work goes under a page COP — don't dump files at `gmangastudio/` root
- Views talk to their ViewModel; don't reach into another page's ViewModel
- Shared code moves to `Shared/` only after a second page needs it
- Keep `ContentView` thin: host navigation / layout chrome, present page COPs

## UI ↔ Framework flow

Swift UI talks to the public C++ API via Swift–C++ interop. No C bridge.

```text
+-------------------------------------------------------+
|                 UI Layer (Swift / SwiftUI)            |  <-- Xcode Managed
+-------------------------------------------------------+
                           │ ▲
                           ▼ │  (Swift–C++ interop)
+-------------------------------------------------------+
|          Public C++ API (`illus::CanvasEditor`)       |
+-------------------------------------------------------+
                           │ ▲
                           ▼ │
+-------------------------------------------------------+
|     IllusStudioCanvasEditor / Core (C++)              |  <-- This framework
+-------------------------------------------------------+
```

| Layer | Owns | Does not own |
|-------|------|--------------|
| **UI** (Swift / SwiftUI) | Page COPs, ViewModels, windowing, input, MTKView present | Stroke math, layer compositing |
| **Public C++ API** | `CanvasEditor.hpp` — stable surface for Swift | Internal headers under `src/` |
| **Core** (C++) | Document, layers, tools, render, Metal upload | App chrome |

**Flow rules**

- UI → `illus::CanvasEditor` → internal engine
- Keep `CanvasEditor.hpp` small and Swift-importable (avoid `void*`; use `uintptr_t` for Metal handles)
- Put new canvas logic in the engine; put new screens in page COPs
- App target uses `SWIFT_OBJC_INTEROP_MODE = objcxx`

## Stack

- SwiftUI + Swift (UI layer, Xcode-managed app target)
- **IllusStudioFramework** — C++ engine + `CanvasEditor` (`import IllusStudioFramework`); architecture: [IllusStudioFramework/README.md](IllusStudioFramework/README.md); tasks: [IllusStudioFramework/docs/ROADMAP.md](IllusStudioFramework/docs/ROADMAP.md)
- Xcode / Apple SDKs (no package manager unless one is added later)
- Design and test for **Mac** and **iPad** first (pointer/trackpad, large canvas, split views)
- Drawing present path targets **120fps** on high-refresh displays (do not throttle below 1/120 s)
- Deployment targets follow the Xcode project settings

## Conventions

- Smallest working diff; reuse patterns already in `gmangastudio/`
- Prefer Swift standard library and Apple frameworks (SwiftUI, Foundation, etc.) over new dependencies
- UI and interaction: optimize for macOS windowing and iPad; don't assume iPhone chrome
- Non-trivial logic: leave one runnable check behind (XCTest or a small assert path)
- Mark intentional shortcuts with `ponytail:` comments
- Follow the ponytail skill (`.cursor/skills/ponytail/`) and always-on rule (`.cursor/rules/ponytail.mdc`)

## Build & run

Open `gmangastudio.xcodeproj` in Xcode, pick a **Mac** or **iPad** destination, Run.

CLI (when needed):

```bash
xcodebuild -scheme gmangastudio -destination 'platform=macOS' build
xcodebuild -scheme gmangastudio -destination 'platform=iOS Simulator,name=iPad Pro 13-inch (M4)' build
```

## Out of scope unless asked

- New third-party dependencies
- Extra architecture layers (coordinators, DI containers, etc.) without a concrete need
