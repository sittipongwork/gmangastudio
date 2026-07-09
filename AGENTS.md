# gmangastudio

SwiftUI app for manga studio work. **Primary targets: macOS and iPad (iPadOS).** Phone is secondary unless asked. Xcode project; keep diffs small and reuse Apple frameworks before adding anything new.

## Project Structure

```text
gmangastudio/                    App sources (SwiftUI)
  gmangastudioApp.swift          App entry (`@main`), window / scene setup
  ContentView.swift              Root shell (routes into page COPs)
  Assets.xcassets/               Colors, app icon
  Shared/                        Cross-page helpers (only when reused by 2+ pages)
    Views/                       Shared SwiftUI pieces
    Models/                      Shared types
    Services/                    Shared I/O (files, export, etc.)
  <PageName>/                    One folder per page / feature (page COP)
    Views/                       SwiftUI views for this page
      <PageName>View.swift       Page root view (entry for this COP)
      <Something>View.swift      Child / section views (same page only)
    ViewModels/
      <PageName>ViewModel.swift  Page state + intents (`@Observable` / `ObservableObject`)
    Models/                      Page-local types (omit folder if none yet)
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

Canvas work is split across three layers. UI never talks to C++ directly; it goes through the bridge.

```text
+-------------------------------------------------------+
|                 UI Layer (Swift / SwiftUI)            |  <-- Xcode Managed
+-------------------------------------------------------+
                           │ ▲
                           ▼ │  (Swift-C++ Interop / Unsafe FFI)
+-------------------------------------------------------+
|                 Bridge Layer (C API / Obj-C++)        |
+-------------------------------------------------------+
                           │ ▲
                           ▼ │
+-------------------------------------------------------+
|          Core Canvas Engine (C++)                     |  <-- Separate Library/Framework
+-------------------------------------------------------+
```

| Layer | Owns | Does not own |
|-------|------|--------------|
| **UI** (Swift / SwiftUI) | Page COPs, ViewModels, windowing, input capture, displaying frames/textures from the engine | Stroke math, layer compositing, document persistence internals |
| **Bridge** (C API / Obj-C++) | Stable FFI surface, type marshaling, lifetime of engine handles, thread handoff into/out of Swift | SwiftUI views, engine algorithms |
| **Core Canvas Engine** (C++) | Drawing, layers, tools, document model, render buffer — shipped as a separate library/framework | App chrome, navigation, platform UI |

**Flow rules**

- UI → Bridge → Engine for commands (pointer events, tool changes, undo, export requests)
- Engine → Bridge → UI for results (dirty rects, preview buffers, status/errors)
- Prefer a small C API on the bridge; keep Obj-C++ only where Apple types need wrapping
- Engine stays buildable/testable as its own target; UI depends on the bridge, not on engine headers
- Put new canvas logic in the engine; put new screens in page COPs

## Stack

- SwiftUI + Swift (UI layer, Xcode-managed app target)
- Bridge: C API / Obj-C++ (Swift ↔ engine)
- Core Canvas Engine: C++ (separate library/framework)
- Xcode / Apple SDKs (no package manager unless one is added later)
- Design and test for **Mac** and **iPad** first (pointer/trackpad, large canvas, split views)
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
