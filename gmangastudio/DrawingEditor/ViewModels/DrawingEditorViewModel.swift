//
//  DrawingEditorViewModel.swift
//  gmangastudio
//

import Foundation
import IllusStudioFramework
import Observation
import CoreGraphics
import SwiftUI

#if os(macOS)
import AppKit
#else
import UIKit
#endif

@Observable
final class DrawingEditorViewModel {
    private(set) var engineVersion: String
    private(set) var selfCheckPassed: Bool
    private(set) var layerCount: Int32
    private(set) var metalAvailable: Bool
    private(set) var editor: illus.CanvasEditor
    private(set) var zoomPercent: Int = 100
    /// Mirrored viewport for Metal present (TX-7: no per-frame editor locks for NDC).
    private(set) var viewportScale: Float = 1
    private(set) var viewportOffsetX: Float = 0
    private(set) var viewportOffsetY: Float = 0
    private(set) var appActiveStatus: AppActiveStatus = .performanceMode
    /// App-recommended present rate (120 if exact, else 72).
    private(set) var presentFps: Int = 72
    private(set) var isBrushLibraryVisible = false
    private(set) var isLayersVisible = false
    private(set) var brushSets: [BrushLibrarySetItem] = []
    private(set) var brushPresets: [BrushLibraryPresetItem] = []
    private(set) var selectedBrushSetIndex: Int32 = 0
    private(set) var selectedBrushPresetIndex: Int32 = 0
    private(set) var layers: [LayerListItem] = []
    private(set) var activeLayerId: Int32 = 0
    /// Bumps when active layer pixels change (stroke / clear) so thumbs refresh.
    private var layerContentRevision: [Int32: Int] = [:]
    var isBrushImportPresented = false

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
        assert(FloatingWidgetPositionSelfCheck.run(), "FloatingWidgetPosition self-check failed")

        let check = illus.CanvasEditor.selfCheck()
        selfCheckPassed = check
        engineVersion = String(cString: illus.CanvasEditor.version())
        assert(check, "IllusStudioFramework self-check failed")

        let fps = Self.recommendedPresentFps()
        // Drop leftover key from removed FPS persist.
        UserDefaults.standard.removeObject(forKey: "drawingEditor.presentFps")

        var ed = illus.CanvasEditor(width, height)
        ed.setTool(.Brush)
        ed.setTargetPresentFps(Int32(fps))
        layerCount = ed.layerCount()
        activeLayerId = ed.activeLayerId()
        _ = ed.presentMetalTextureAddress()
        metalAvailable = ed.metalAvailable()
        editor = ed
        presentFps = fps
        syncViewportFromEditor()
        reloadBrushLibrary()
        reloadLayers()
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
        if mode == .brushLibrary {
            isBrushLibraryVisible = true
        } else {
            isBrushLibraryVisible = false
        }
        var ed = editor
        switch mode {
        case .pointer: ed.setTool(.Pointer)
        case .brushLibrary: ed.setTool(.Brush)
        case .eraser: ed.setTool(.Eraser)
        }
        editor = ed
    }

    func toggleBrushLibrary() {
        noteUserActivity()
        if isBrushLibraryVisible {
            isBrushLibraryVisible = false
            return
        }
        isLayersVisible = false
        setMode(.brushLibrary)
    }

    func toggleLayers() {
        noteUserActivity()
        isLayersVisible.toggle()
        if isLayersVisible {
            isBrushLibraryVisible = false
            reloadLayers()
        }
    }

    func selectBrushSet(_ setIndex: Int32) {
        noteUserActivity()
        selectedBrushSetIndex = setIndex
        reloadBrushPresets()
        if let first = brushPresets.first {
            selectBrushPreset(first.id)
        }
    }

    func presentBrushImport() {
        noteUserActivity()
        isBrushImportPresented = true
    }

    func importBrushPackage(from url: URL) {
        noteUserActivity()
        let accessed = url.startAccessingSecurityScopedResource()
        defer {
            if accessed { url.stopAccessingSecurityScopedResource() }
        }
        guard let data = try? Data(contentsOf: url), !data.isEmpty else { return }
        var ed = editor
        var brushCount: Int32 = 0
        let name = url.lastPathComponent
        let setId = data.withUnsafeBytes { raw -> Int32 in
            guard let base = raw.bindMemory(to: UInt8.self).baseAddress else { return -1 }
            return ed.importBrushPackageBytes(
                base,
                Int32(data.count),
                .Auto,
                name,
                &brushCount
            )
        }
        editor = ed
        guard setId >= 0 else { return }
        reloadBrushLibrary()
        if let idx = brushSets.last(where: { $0.isImported })?.id {
            selectBrushSet(idx)
        }
    }

    func selectBrushPreset(_ presetIndexInSet: Int32) {
        noteUserActivity()
        selectedBrushPresetIndex = presetIndexInSet
        var ed = editor
        _ = ed.setBrushPresetInSet(selectedBrushSetIndex, presetIndexInSet)
        ed.setTool(.Brush)
        editor = ed
        mode = .brushLibrary
    }

    func clear() {
        noteUserActivity()
        var ed = editor
        ed.clearAll(255, 255, 255, 255)
        editor = ed
        bumpAllLayerContent()
        reloadLayers()
    }

    func addLayer() {
        noteUserActivity()
        var ed = editor
        // Engine auto-names `Layer N` and inserts at front (top of list).
        let id = ed.addLayer(nil)
        layerCount = ed.layerCount()
        activeLayerId = ed.activeLayerId()
        editor = ed
        if id >= 0 { bumpLayerContent(id) }
        reloadLayers()
    }

    func selectLayer(_ layerId: Int32) {
        noteUserActivity()
        guard layerId >= 0 else { return }
        var ed = editor
        if ed.setActiveLayer(layerId) {
            activeLayerId = layerId
            editor = ed
            reloadLayers()
        }
    }

    /// Reorder paint layers (Background stays last). Indices are within the paint-only list.
    func reorderPaintLayers(from source: IndexSet, to destination: Int) {
        noteUserActivity()
        var paintIds = layers.filter { !$0.isBackground }.map(\.id)
        guard let fromIndex = source.first, fromIndex < paintIds.count else { return }
        let id = paintIds.remove(at: fromIndex)
        let insertAt = destination > fromIndex ? destination - 1 : destination
        let clamped = min(max(0, insertAt), paintIds.count)
        paintIds.insert(id, at: clamped)
        guard let toIndex = paintIds.firstIndex(of: id) else { return }
        var ed = editor
        if ed.moveLayer(id, Int32(toIndex)) {
            editor = ed
            reloadLayers()
        }
    }

    func toggleLayerVisible(_ layerId: Int32) {
        noteUserActivity()
        guard layerId >= 0 else { return }
        var ed = editor
        let next = !ed.layerVisible(layerId)
        if ed.setLayerVisible(layerId, next) {
            editor = ed
            reloadLayers()
        }
    }

    func clearLayer(_ layerId: Int32) {
        noteUserActivity()
        guard layerId >= 0 else { return }
        var ed = editor
        let prev = ed.activeLayerId()
        _ = ed.setActiveLayer(layerId)
        ed.clearActiveLayer()
        _ = ed.setActiveLayer(prev)
        activeLayerId = ed.activeLayerId()
        editor = ed
        bumpLayerContent(layerId)
        reloadLayers()
    }

    func duplicateLayer(_ layerId: Int32) {
        noteUserActivity()
        guard layerId >= 0 else { return }
        var ed = editor
        let newId = ed.duplicateLayer(layerId)
        if newId >= 0 {
            activeLayerId = newId
            layerCount = ed.layerCount()
            editor = ed
            bumpLayerContent(newId)
            reloadLayers()
        }
    }

    func mergeLayerDown(_ layerId: Int32) {
        noteUserActivity()
        guard layerId >= 0 else { return }
        var ed = editor
        let idx = ed.layerIndex(layerId)
        guard idx >= 0, idx + 1 < ed.layerCount() else { return }
        let belowId = ed.layerIdAt(idx + 1)
        guard belowId >= 0, ed.mergeLayerDown(layerId, belowId) else { return }
        layerCount = ed.layerCount()
        activeLayerId = ed.activeLayerId()
        editor = ed
        bumpLayerContent(belowId)
        reloadLayers()
    }

    func resetViewport() {
        noteUserActivity()
        var ed = editor
        ed.setViewport(1, 0, 0)
        editor = ed
        syncViewportFromEditor()
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
        syncViewportFromEditor()
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
        syncViewportFromEditor()
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
        bumpLayerContent(activeLayerId)
        reloadLayers()
    }

    private func syncViewportFromEditor() {
        let ed = editor
        viewportScale = ed.viewportScale()
        viewportOffsetX = ed.viewportOffsetX()
        viewportOffsetY = ed.viewportOffsetY()
        zoomPercent = Int((viewportScale * 100).rounded())
    }

    /// 120 when the panel can run it exactly; else 72 (144Hz divisor; avoids 60→48).
    private static func recommendedPresentFps() -> Int {
        #if os(macOS)
        let panelMax = NSScreen.main?.maximumFramesPerSecond ?? 60
        #else
        let panelMax = UIScreen.main.maximumFramesPerSecond
        #endif
        let hz = panelMax > 0 ? panelMax : 60
        return (hz >= 120 && hz % 120 == 0) ? 120 : 72
    }

    private func reloadBrushLibrary() {
        let ed = editor
        let setCount = ed.brushSetCount()
        var sets: [BrushLibrarySetItem] = []
        for i in 0..<setCount {
            let name = String(cString: ed.brushSetName(i))
            let imported = ed.brushSetSource(i) == 2
            sets.append(.init(
                id: i,
                name: name.isEmpty ? "Set \(i)" : name,
                systemImage: iconForBrushSet(name, imported: imported),
                isImported: imported
            ))
        }
        brushSets = sets
        if selectedBrushSetIndex >= setCount {
            selectedBrushSetIndex = 0
        }
        reloadBrushPresets()
    }

    private func reloadBrushPresets() {
        let ed = editor
        let count = ed.brushPresetCountInSet(selectedBrushSetIndex)
        var items: [BrushLibraryPresetItem] = []
        for i in 0..<count {
            let name = String(cString: ed.brushPresetNameInSet(selectedBrushSetIndex, i))
            let weight: CGFloat = name.contains("air") || name.contains("soft") ? 8 : 4
            items.append(.init(
                id: i,
                name: name.isEmpty ? "Brush \(i)" : name,
                strokeWeight: weight,
                approximated: ed.brushPresetApproximated(selectedBrushSetIndex, i)
            ))
        }
        brushPresets = items
        if selectedBrushPresetIndex >= count {
            selectedBrushPresetIndex = 0
        }
    }

    private func iconForBrushSet(_ name: String, imported: Bool) -> String {
        if imported { return "square.and.arrow.down.on.square" }
        let n = name.lowercased()
        if n.contains("ink") { return "pencil.tip" }
        if n.contains("air") { return "cloud" }
        if n.contains("erase") { return "eraser" }
        return "paintbrush.pointed"
    }

    private func reloadLayers() {
        let ed = editor
        let count = ed.layerCount()
        var items: [LayerListItem] = []
        // Index 0 = front (top of panel).
        for i in 0..<count {
            let id = ed.layerIdAt(i)
            guard id >= 0 else { continue }
            let name = String(cString: ed.layerName(id))
            let rev = layerContentRevision[id] ?? 0
            items.append(.init(
                id: id,
                name: name.isEmpty ? "Layer \(id)" : name,
                visible: ed.layerVisible(id),
                isBackground: ed.layerIsBackground(id),
                contentRevision: rev,
                thumbnail: makeLayerThumbnail(editor: ed, layerId: id)
            ))
        }
        layers = items
        activeLayerId = ed.activeLayerId()
        layerCount = count
    }

    private func bumpLayerContent(_ layerId: Int32) {
        layerContentRevision[layerId, default: 0] += 1
    }

    private func bumpAllLayerContent() {
        for id in layers.map(\.id) {
            bumpLayerContent(id)
        }
    }

    /// Document-aspect thumb (~36px long edge).
    private func makeLayerThumbnail(editor ed: illus.CanvasEditor, layerId: Int32) -> Image? {
        let docW = max(1, canvasWidth)
        let docH = max(1, canvasHeight)
        let maxSide: Int32 = 36
        let outW: Int32
        let outH: Int32
        if docW >= docH {
            outW = maxSide
            outH = max(1, (maxSide * docH + docW / 2) / docW)
        } else {
            outH = maxSide
            outW = max(1, (maxSide * docW + docH / 2) / docH)
        }
        let byteCount = Int(outW * outH * 4)
        var bytes = [UInt8](repeating: 0, count: byteCount)
        let ok = bytes.withUnsafeMutableBufferPointer { buf in
            ed.copyLayerThumbnailRGBA(layerId, outW, outH, buf.baseAddress, Int32(byteCount))
        }
        guard ok else { return nil }

#if os(macOS)
        guard let cgImage = Self.cgImageRGBA(bytes: bytes, width: Int(outW), height: Int(outH)) else { return nil }
        let ns = NSImage(cgImage: cgImage, size: NSSize(width: Int(outW), height: Int(outH)))
        return Image(nsImage: ns)
#else
        guard let cgImage = Self.cgImageRGBA(bytes: bytes, width: Int(outW), height: Int(outH)) else { return nil }
        let ui = UIImage(cgImage: cgImage, scale: 1, orientation: .up)
        return Image(uiImage: ui)
#endif
    }

    private static func cgImageRGBA(bytes: [UInt8], width: Int, height: Int) -> CGImage? {
        guard width > 0, height > 0, bytes.count >= width * height * 4 else { return nil }
        let data = Data(bytes)
        guard let provider = CGDataProvider(data: data as CFData) else { return nil }
        return CGImage(
            width: width,
            height: height,
            bitsPerComponent: 8,
            bitsPerPixel: 32,
            bytesPerRow: width * 4,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue),
            provider: provider,
            decode: nil,
            shouldInterpolate: false,
            intent: .defaultIntent
        )
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
