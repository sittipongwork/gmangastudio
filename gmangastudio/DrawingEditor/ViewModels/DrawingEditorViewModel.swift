//
//  DrawingEditorViewModel.swift
//  gmangastudio
//

import CoreGraphics
import Foundation
import IllusStudioFramework
import Observation

@Observable
final class DrawingEditorViewModel {
    private(set) var canvasImage: CGImage?
    private(set) var engineVersion: String = ""
    private(set) var selfCheckPassed: Bool = false

    private var canvas: OpaquePointer?
    private var isStroking = false
    private let canvasWidth: Int32 = 1920
    private let canvasHeight: Int32 = 1080

    init() {
        engineVersion = String(cString: ISCanvasGetVersion())
        selfCheckPassed = ISCanvasSelfCheck() == 1
        assert(selfCheckPassed, "IllusStudioFramework self-check failed")

        canvas = ISCanvasCreate(canvasWidth, canvasHeight)
        refreshImage()
    }

    deinit {
        if let canvas {
            ISCanvasDestroy(canvas)
        }
    }

    func clear() {
        guard let canvas else { return }
        ISCanvasClear(canvas, 255, 255, 255, 255)
        refreshImage()
    }

    func pointerChanged(at point: CGPoint) {
        guard let canvas else { return }
        if isStroking {
            ISCanvasContinueStroke(canvas, Float(point.x), Float(point.y), 1)
        } else {
            isStroking = true
            ISCanvasBeginStroke(canvas, Float(point.x), Float(point.y), 1)
        }
        refreshImage()
    }

    func pointerEnded() {
        guard let canvas, isStroking else { return }
        isStroking = false
        ISCanvasEndStroke(canvas)
        refreshImage()
    }

    private func refreshImage() {
        guard let canvas,
              let pixels = ISCanvasGetPixels(canvas) else {
            canvasImage = nil
            return
        }

        let width = Int(ISCanvasGetWidth(canvas))
        let height = Int(ISCanvasGetHeight(canvas))
        let bytesPerRow = width * 4
        let data = Data(bytes: pixels, count: height * bytesPerRow)

        guard let provider = CGDataProvider(data: data as CFData) else {
            canvasImage = nil
            return
        }

        canvasImage = CGImage(
            width: width,
            height: height,
            bitsPerComponent: 8,
            bitsPerPixel: 32,
            bytesPerRow: bytesPerRow,
            space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue),
            provider: provider,
            decode: nil,
            shouldInterpolate: false,
            intent: .defaultIntent
        )
    }
}
