//
//  DrawingEditorViewModel.swift
//  gmangastudio
//

import Foundation
import IllusStudioFramework
import Observation

@Observable
final class DrawingEditorViewModel {
    private(set) var engineVersion: String
    private(set) var selfCheckPassed: Bool
    private(set) var layerCount: Int32
    private(set) var metalAvailable: Bool
    private(set) var editor: illus.CanvasEditor
    private(set) var zoomPercent: Int = 100
    private(set) var appActiveStatus: AppActiveStatus = .performanceMode

    private var isStroking = false
    private var idle = AppActiveIdleTracker()
    private var idleTask: Task<Void, Never>?
    private(set) var mode: DrawingEditorMode = .brushLibrary
    let canvasWidth: Int32
    let canvasHeight: Int32

    init() {
        let width: Int32 = 400
        let height: Int32 = 600
        canvasWidth = width
        canvasHeight = height

        assert(AppActiveIdleTrackerSelfCheck.run(), "AppActiveIdleTracker self-check failed")

        let check = illus.CanvasEditor.selfCheck()
        selfCheckPassed = check
        engineVersion = String(cString: illus.CanvasEditor.version())
        assert(check, "IllusStudioFramework self-check failed")

        var ed = illus.CanvasEditor(width, height)
        ed.setTool(.Brush)
        layerCount = ed.layerCount()
        _ = ed.presentMetalTextureAddress()
        metalAvailable = ed.metalAvailable()
        editor = ed
        syncZoomLabel()
        scheduleIdleCheck()
    }

    deinit {
        idleTask?.cancel()
    }

    /// Tap / move / touch / pan / zoom / menu — resets `lastUse` and restores performance_mode.
    func noteUserActivity() {
        idle.noteActivity()
        appActiveStatus = idle.status
        scheduleIdleCheck()
    }

    func setMode(_ mode: DrawingEditorMode) {
        noteUserActivity()
        if isStroking {
            isStroking = false
            var ed = editor
            ed.endStroke()
            editor = ed
        }
        self.mode = mode
        var ed = editor
        switch mode {
        case .pointer: ed.setTool(.Pointer)
        case .brushLibrary: ed.setTool(.Brush)
        case .eraser: ed.setTool(.Eraser)
        }
        editor = ed
    }

    func clear() {
        noteUserActivity()
        var ed = editor
        ed.clearAll(255, 255, 255, 255)
        editor = ed
    }

    func addLayer() {
        noteUserActivity()
        var ed = editor
        _ = ed.addLayer("Layer")
        layerCount = ed.layerCount()
        editor = ed
    }

    func resetViewport() {
        noteUserActivity()
        var ed = editor
        ed.setViewport(1, 0, 0)
        editor = ed
        syncZoomLabel()
    }

    /// `delta` is view-space pixels (points). Converted to canvas-space pan.
    func panBy(_ delta: CGSize, viewSize: CGSize) {
        noteUserActivity()
        guard viewSize.width > 0, viewSize.height > 0 else { return }
        var ed = editor
        let fit = min(
            Float(viewSize.width) / Float(canvasWidth),
            Float(viewSize.height) / Float(canvasHeight)
        )
        let s = fit * ed.viewportScale()
        guard s > 0 else { return }
        // Drag right → content follows finger → offset decreases (see Viewport::viewOrigin).
        ed.setViewport(
            ed.viewportScale(),
            ed.viewportOffsetX() - Float(delta.width) / s,
            ed.viewportOffsetY() - Float(delta.height) / s
        )
        editor = ed
    }

    /// Zoom by `factor` around view-space focus point.
    func zoomBy(_ factor: CGFloat, focus: CGPoint, viewSize: CGSize) {
        noteUserActivity()
        guard factor > 0, viewSize.width > 0, viewSize.height > 0 else { return }
        var ed = editor
        let vw = Float(viewSize.width)
        let vh = Float(viewSize.height)
        let fx = Float(focus.x)
        let fy = Float(focus.y)

        let beforeX = ed.viewToCanvasX(fx, fy, vw, vh)
        let beforeY = ed.viewToCanvasY(fx, fy, vw, vh)

        let newScale = ed.viewportScale() * Float(factor)
        ed.setViewport(newScale, ed.viewportOffsetX(), ed.viewportOffsetY())

        let afterX = ed.viewToCanvasX(fx, fy, vw, vh)
        let afterY = ed.viewToCanvasY(fx, fy, vw, vh)
        ed.setViewport(
            ed.viewportScale(),
            ed.viewportOffsetX() + (beforeX - afterX),
            ed.viewportOffsetY() + (beforeY - afterY)
        )
        editor = ed
        syncZoomLabel()
    }

    func pointerChanged(at point: CGPoint) {
        noteUserActivity()
        guard mode != .pointer else { return }
        var ed = editor
        if isStroking {
            ed.continueStroke(Float(point.x), Float(point.y), 1)
        } else {
            isStroking = true
            ed.beginStroke(Float(point.x), Float(point.y), 1)
        }
        editor = ed
    }

    func pointerEnded() {
        noteUserActivity()
        guard isStroking else { return }
        isStroking = false
        var ed = editor
        ed.endStroke()
        editor = ed
    }

    private func syncZoomLabel() {
        zoomPercent = Int((editor.viewportScale() * 100).rounded())
    }

    private func scheduleIdleCheck() {
        idleTask?.cancel()
        let delay = idle.secondsUntilIdle()
        guard delay > 0 || idle.status == .performanceMode else { return }
        let wait = max(delay, 0.05)
        idleTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .seconds(wait))
            guard let self, !Task.isCancelled else { return }
            if self.idle.applyIdle() {
                self.appActiveStatus = self.idle.status
            } else if self.idle.status == .performanceMode {
                self.scheduleIdleCheck()
            }
        }
    }
}
