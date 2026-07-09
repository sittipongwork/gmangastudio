# gmangastudio

SwiftUI app for manga studio work. **Primary targets: macOS and iPad (iPadOS).** Phone is secondary unless asked. Xcode project; keep diffs small and reuse Apple frameworks before adding anything new.

## Layout

```text
gmangastudio/           App sources (SwiftUI)
  gmangastudioApp.swift App entry (`@main`)
  ContentView.swift     Root UI
  Assets.xcassets/      Colors, app icon
gmangastudioTests/      Unit tests
gmangastudioUITests/    UI tests
gmangastudio.xcodeproj/ Xcode project
```

## Stack

- SwiftUI + Swift
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
