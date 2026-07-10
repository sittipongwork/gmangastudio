//
//  DrawingEditorViewModel.swift
//  gmangastudio
//

import Foundation
import IllusStudioFramework
import Observation
import CoreGraphics
import SwiftUI
import Metal

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
    private(set) var selectedBrushSetIndex: Int32 = 1 // Inking (default paint = ink.round)
    private(set) var selectedBrushPresetIndex: Int32 = 1 // ink.round within Inking
    private(set) var layers: [LayerListItem] = []
    private(set) var activeLayerId: Int32 = 0
    /// Bumps when active layer pixels change (stroke / clear) so thumbs refresh.
    private var layerContentRevision: [Int32: Int] = [:]
    var isBrushImportPresented = false
    /// True while a Procreate package is being imported (shows progress on Brush Library).
    private(set) var isBrushImporting = false

    /// Brush edit options (sidebar).
    var brushColor: Color = Color(.sRGB, red: 20 / 255, green: 20 / 255, blue: 20 / 255, opacity: 1)
    var brushSize: Double = 16
    var brushOpacity: Double = 1
    private(set) var colorHistory: [Color] = []
    private(set) var eyedropperPreviewColor: Color = .clear
    private(set) var eyedropperViewPoint: CGPoint = .zero
    private(set) var showEyedropperPreview = false

    private var isStroking = false
    private var idle = AppActiveIdleTracker()
    private var idleTask: Task<Void, Never>?
    private(set) var mode: DrawingEditorMode = .brushLibrary
    /// Only meaningful while `mode == .brushLibrary`. Always resets to `.brush` when entering brush.
    private(set) var brushSubmode: BrushSubmode = .brush
    let canvasWidth: Int32
    let canvasHeight: Int32

    private static let colorHistoryKey = "drawingEditor.brushColorHistory"
    private static let maxColorHistory = 10

    var isBrushEditOptionsVisible: Bool {
        mode == .brushLibrary
    }

    var isEyedropperActive: Bool {
        mode == .brushLibrary && brushSubmode == .eyedropper
    }

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
        brushSize = Double(ed.brushLineWidth())
        brushOpacity = Double(ed.brushOpacity())
        pushBrushColorToEditor()
        syncViewportFromEditor()
        colorHistory = Self.loadColorHistory()
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
            brushSubmode = .brush
            showEyedropperPreview = false
            isBrushLibraryVisible = true
        } else {
            brushSubmode = .brush
            showEyedropperPreview = false
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

    func setBrushSubmode(_ submode: BrushSubmode) {
        noteUserActivity()
        guard mode == .brushLibrary else { return }
        if isStroking {
            isStroking = false
            var ed = editor
            ed.endStroke()
            editor = ed
        }
        brushSubmode = submode
        showEyedropperPreview = submode == .eyedropper ? showEyedropperPreview : false
        var ed = editor
        ed.setTool(submode == .eyedropper ? .Pointer : .Brush)
        editor = ed
    }

    func enterEyedropper() {
        guard mode == .brushLibrary else {
            setMode(.brushLibrary)
            setBrushSubmode(.eyedropper)
            return
        }
        setBrushSubmode(.eyedropper)
    }

    func setBrushColor(_ color: Color) {
        noteUserActivity()
        brushColor = color
        pushBrushColorToEditor()
    }

    /// Record current brush color into History (Colors widget close, or first dab of a stroke).
    func recordBrushColorHistory() {
        pushColorHistory(brushColor)
    }

    func setBrushSize(_ size: Double) {
        noteUserActivity()
        brushSize = min(max(size, 1), 100)
        var ed = editor
        ed.setBrushLineWidth(Float(brushSize))
        editor = ed
    }

    func setBrushOpacity(_ opacity: Double) {
        noteUserActivity()
        brushOpacity = min(max(opacity, 0), 1)
        var ed = editor
        ed.setBrushOpacity(Float(brushOpacity))
        editor = ed
    }

    func clearColorHistory() {
        noteUserActivity()
        colorHistory = []
        UserDefaults.standard.removeObject(forKey: Self.colorHistoryKey)
    }

    func eyedropperMoved(viewPoint: CGPoint, canvasPoint: CGPoint) {
        guard isEyedropperActive else { return }
        noteUserActivity()
        eyedropperViewPoint = viewPoint
        if let sampled = sampleCompositeColor(at: canvasPoint) {
            eyedropperPreviewColor = sampled
        }
        showEyedropperPreview = true
    }

    func eyedropperPick(at canvasPoint: CGPoint) {
        guard isEyedropperActive else { return }
        noteUserActivity()
        if let sampled = sampleCompositeColor(at: canvasPoint) {
            setBrushColor(sampled)
        } else if showEyedropperPreview {
            setBrushColor(eyedropperPreviewColor)
        }
        showEyedropperPreview = false
        setBrushSubmode(.brush)
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
        guard !isBrushImporting else { return }
        isBrushImporting = true
        // Yield so Brush Library can paint ProgressView before the sync import blocks MainActor.
        Task { @MainActor in
            // ponytail: one frame for overlay; upgrade to C++ progress callback if import stays multi-second.
            try? await Task.sleep(for: .milliseconds(16))
            defer { isBrushImporting = false }
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
    }

    func selectBrushPreset(_ presetIndexInSet: Int32) {
        noteUserActivity()
        selectedBrushPresetIndex = presetIndexInSet
        var ed = editor
        _ = ed.setBrushPresetInSet(selectedBrushSetIndex, presetIndexInSet)
        ed.setTool(.Brush)
        brushSize = Double(ed.brushLineWidth())
        brushOpacity = Double(ed.brushOpacity())
        pushBrushColorToEditor()
        editor = ed
        mode = .brushLibrary
        brushSubmode = .brush
        showEyedropperPreview = false
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

    func pointerChanged(at point: CGPoint, pressure: Float = 1, tiltX: Float = 0, tiltY: Float = 0) {
        noteUserActivity()
        guard mode != .pointer, !isEyedropperActive else { return }
        let p = min(max(pressure, 0.05), 1)
        var ed = editor
        ed.setStrokeTilt(tiltX, tiltY)
        if isStroking {
            ed.continueStroke(Float(point.x), Float(point.y), p)
        } else {
            isStroking = true
            recordBrushColorHistory()
            ed.beginStroke(Float(point.x), Float(point.y), p)
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

    private func pushBrushColorToEditor() {
        var ed = editor
        let rgba = Self.rgba8(from: brushColor)
        ed.setBrushColor(rgba.0, rgba.1, rgba.2, rgba.3)
        editor = ed
    }

    private func pushColorHistory(_ color: Color) {
        let rgba = Self.rgba8(from: color)
        colorHistory.removeAll { Self.rgba8(from: $0) == rgba }
        colorHistory.insert(color, at: 0)
        if colorHistory.count > Self.maxColorHistory {
            colorHistory = Array(colorHistory.prefix(Self.maxColorHistory))
        }
        Self.saveColorHistory(colorHistory)
    }

    /// Sample present composite at canvas pixel (Shared MTLTexture).
    private func sampleCompositeColor(at canvasPoint: CGPoint) -> Color? {
        _ = editor.presentMetalTextureAddress()
        let x = Int(canvasPoint.x.rounded())
        let y = Int(canvasPoint.y.rounded())
        guard x >= 0, y >= 0, x < Int(canvasWidth), y < Int(canvasHeight) else { return nil }
        let addr = editor.presentMetalTextureAddress()
        guard addr != 0, let ptr = UnsafeMutableRawPointer(bitPattern: UInt(addr)) else { return nil }
        let tex = Unmanaged<MTLTexture>.fromOpaque(ptr).takeUnretainedValue()
        var pixel = [UInt8](repeating: 0, count: 4)
        pixel.withUnsafeMutableBytes { buf in
            guard let base = buf.baseAddress else { return }
            tex.getBytes(
                base,
                bytesPerRow: 4,
                from: MTLRegionMake2D(x, y, 1, 1),
                mipmapLevel: 0
            )
        }
        // Premultiplied present — unpremultiply for brush color when alpha > 0.
        let a = Double(pixel[3]) / 255
        guard a > 0.001 else {
            return Color(.sRGB, red: 1, green: 1, blue: 1, opacity: 1)
        }
        let r = min(1, Double(pixel[0]) / 255 / a)
        let g = min(1, Double(pixel[1]) / 255 / a)
        let b = min(1, Double(pixel[2]) / 255 / a)
        return Color(.sRGB, red: r, green: g, blue: b, opacity: 1)
    }

    // ponytail: pure Color↔hex; nonisolated so UserDefaults load/save can call them off MainActor isolation.
    private nonisolated static func rgba8(from color: Color) -> (UInt8, UInt8, UInt8, UInt8) {
        #if os(macOS)
        let ns = NSColor(color)
        guard let rgb = ns.usingColorSpace(.sRGB) else { return (20, 20, 20, 255) }
        return (
            UInt8(clamping: Int((rgb.redComponent * 255).rounded())),
            UInt8(clamping: Int((rgb.greenComponent * 255).rounded())),
            UInt8(clamping: Int((rgb.blueComponent * 255).rounded())),
            UInt8(clamping: Int((rgb.alphaComponent * 255).rounded()))
        )
        #else
        let ui = UIColor(color)
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        ui.getRed(&r, green: &g, blue: &b, alpha: &a)
        return (
            UInt8(clamping: Int((r * 255).rounded())),
            UInt8(clamping: Int((g * 255).rounded())),
            UInt8(clamping: Int((b * 255).rounded())),
            UInt8(clamping: Int((a * 255).rounded()))
        )
        #endif
    }

    private static func loadColorHistory() -> [Color] {
        guard let raw = UserDefaults.standard.array(forKey: colorHistoryKey) as? [String] else { return [] }
        return raw.compactMap(colorFromHex)
    }

    private static func saveColorHistory(_ colors: [Color]) {
        let hexes = colors.map(hexFromColor)
        UserDefaults.standard.set(hexes, forKey: colorHistoryKey)
    }

    private nonisolated static func hexFromColor(_ color: Color) -> String {
        let c = rgba8(from: color)
        return String(format: "%02X%02X%02X%02X", c.0, c.1, c.2, c.3)
    }

    private nonisolated static func colorFromHex(_ hex: String) -> Color? {
        guard hex.count == 8, let v = UInt32(hex, radix: 16) else { return nil }
        let r = Double((v >> 24) & 0xFF) / 255
        let g = Double((v >> 16) & 0xFF) / 255
        let b = Double((v >> 8) & 0xFF) / 255
        let a = Double(v & 0xFF) / 255
        return Color(.sRGB, red: r, green: g, blue: b, opacity: a)
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
            let lineW = ed.brushPresetLineWidthInSet(selectedBrushSetIndex, i)
            let weight = CGFloat(min(10, max(1.5, Double(lineW) * 0.2)))
            items.append(.init(
                id: i,
                name: name.isEmpty ? "Brush \(i)" : name,
                strokeWeight: weight,
                approximated: ed.brushPresetApproximated(selectedBrushSetIndex, i),
                preview: makeBrushPresetPreview(editor: ed, setIndex: selectedBrushSetIndex, presetIndex: i)
            ))
        }
        brushPresets = items
        if selectedBrushPresetIndex >= count {
            selectedBrushPresetIndex = 0
        }
    }

    /// Procreate QuickLook or engine tip/grain strip (~180×48).
    private func makeBrushPresetPreview(
        editor ed: illus.CanvasEditor,
        setIndex: Int32,
        presetIndex: Int32
    ) -> Image? {
        let outW: Int32 = 180
        let outH: Int32 = 48
        let byteCount = Int(outW * outH * 4)
        var bytes = [UInt8](repeating: 0, count: byteCount)
        let ok = bytes.withUnsafeMutableBufferPointer { buf in
            ed.copyBrushPresetPreviewRGBA(
                setIndex, presetIndex, outW, outH, buf.baseAddress, Int32(byteCount)
            )
        }
        guard ok else { return nil }
        // Skip empty strips (all transparent).
        guard bytes.contains(where: { $0 > 0 }) else { return nil }

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

    private func iconForBrushSet(_ name: String, imported: Bool) -> String {
        if imported { return "square.and.arrow.down.on.square" }
        let n = name.lowercased()
        if n.contains("sketch") { return "pencil" }
        if n.contains("ink") { return "pencil.tip" }
        if n.contains("draw") { return "scribble.variable" }
        if n.contains("paint") { return "paintbrush.pointed" }
        if n.contains("erase") { return "eraser" }
        if n.contains("user") { return "person" }
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
