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

    private var isStroking = false
    let canvasWidth: Int32
    let canvasHeight: Int32

    init() {
        let width: Int32 = 1920
        let height: Int32 = 1080
        canvasWidth = width
        canvasHeight = height

        let check = illus.CanvasEditor.selfCheck()
        selfCheckPassed = check
        engineVersion = String(cString: illus.CanvasEditor.version())
        assert(check, "IllusStudioFramework self-check failed")

        var ed = illus.CanvasEditor(width, height)
        layerCount = ed.layerCount()
        _ = ed.presentMetalTextureAddress()
        metalAvailable = ed.metalAvailable()
        editor = ed
    }

    func clear() {
        var ed = editor
        ed.clearAll(255, 255, 255, 255)
        editor = ed
    }

    func addLayer() {
        var ed = editor
        _ = ed.addLayer("Layer")
        layerCount = ed.layerCount()
        editor = ed
    }

    func pointerChanged(at point: CGPoint) {
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
        guard isStroking else { return }
        isStroking = false
        var ed = editor
        ed.endStroke()
        editor = ed
    }
}
