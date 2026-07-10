# gmangastudio

SwiftUI app for manga studio work. **Primary targets: macOS and iPad (iPadOS).** Phone is secondary unless asked. Xcode project; keep diffs small and reuse Apple frameworks before adding anything new.

The C++ canvas engine previously lived in this repo as `IllusStudioFramework`. It was **moved out** to `/Users/sittipongjungsakul/work/ai/IllusStudioFramework` and is **not** part of this Xcode project. DrawingEditor UI that depended on it is archived under that tree as `archived_from_gmangastudio/DrawingEditor`.

## Project Structure

```text
gmangastudio/                    App sources (SwiftUI) — UI layer
  gmangastudioApp.swift          App entry (`@main`), window / scene setup
  ContentView.swift              Root shell (routes into page COPs)
  Assets.xcassets/               Colors, app icon
  Shared/                        Cross-page helpers (only when reused by 2+ pages)
  <PageName>/                    One folder per page / feature (page COP)
    Views/  ViewModels/  Models/
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

## Stack

- SwiftUI + Swift (UI layer, Xcode-managed app target)
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
