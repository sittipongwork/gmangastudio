//
//  CanvasMetalView.swift
//  gmangastudio — 120fps Metal present of IllusStudioFramework texture
//

import IllusStudioFramework
import Metal
import MetalKit
import SwiftUI

#if os(macOS)
import AppKit
#else
import UIKit
#endif

final class CanvasMetalRenderer: NSObject, MTKViewDelegate {
    private let device: MTLDevice
    private let commandQueue: MTLCommandQueue
    private let pipeline: MTLRenderPipelineState
    private let sampler: MTLSamplerState
    private var editor: illus.CanvasEditor
    private let canvasWidth: Float
    private let canvasHeight: Float
    /// Cached present viewport (TX-7): update on pan/zoom — avoid 3× editor locks per frame.
    private var viewportScale: Float = 1
    private var viewportOffsetX: Float = 0
    private var viewportOffsetY: Float = 0

    init?(editor: illus.CanvasEditor, canvasWidth: Int32, canvasHeight: Int32) {
        // Must share the editor's MTLDevice — cross-device texture sample = EXC_BAD_ACCESS.
        let deviceAddr = editor.metalDeviceAddress()
        guard deviceAddr != 0,
              let devicePtr = UnsafeMutableRawPointer(bitPattern: UInt(deviceAddr)) else {
            return nil
        }
        let device = Unmanaged<MTLDevice>.fromOpaque(devicePtr).takeUnretainedValue()
        guard let queue = device.makeCommandQueue() else {
            return nil
        }

        // u = (xmin, ymin, xmax, ymax) in NDC (Metal: y+ up).
        // UV (0,0) = top-left of CPU/RGBA buffer — pair with NDC top (ymax).
        let shader = """
        #include <metal_stdlib>
        using namespace metal;
        struct VOut { float4 position [[position]]; float2 uv; };
        vertex VOut is_vertex(uint vid [[vertex_id]], constant float4* u [[buffer(0)]]) {
            // triangle strip: TL, TR, BL, BR
            float2 ndc[4] = {
                float2(u[0].x, u[0].w),
                float2(u[0].z, u[0].w),
                float2(u[0].x, u[0].y),
                float2(u[0].z, u[0].y)
            };
            float2 uv[4] = { float2(0,0), float2(1,0), float2(0,1), float2(1,1) };
            VOut o;
            o.position = float4(ndc[vid], 0, 1);
            o.uv = uv[vid];
            return o;
        }
        fragment float4 is_fragment(VOut in [[stage_in]],
                                    texture2d<float> tex [[texture(0)]],
                                    sampler samp [[sampler(0)]]) {
            return tex.sample(samp, in.uv);
        }
        """

        guard let library = try? device.makeLibrary(source: shader, options: nil),
              let vfn = library.makeFunction(name: "is_vertex"),
              let ffn = library.makeFunction(name: "is_fragment") else {
            return nil
        }

        let desc = MTLRenderPipelineDescriptor()
        desc.vertexFunction = vfn
        desc.fragmentFunction = ffn
        desc.colorAttachments[0].pixelFormat = .bgra8Unorm
        // Engine present texture is premultiplied (LayerCompositor).
        desc.colorAttachments[0].isBlendingEnabled = true
        desc.colorAttachments[0].sourceRGBBlendFactor = .one
        desc.colorAttachments[0].destinationRGBBlendFactor = .oneMinusSourceAlpha
        desc.colorAttachments[0].sourceAlphaBlendFactor = .one
        desc.colorAttachments[0].destinationAlphaBlendFactor = .oneMinusSourceAlpha

        guard let pipeline = try? device.makeRenderPipelineState(descriptor: desc) else {
            return nil
        }

        let sdesc = MTLSamplerDescriptor()
        sdesc.minFilter = .nearest
        sdesc.magFilter = .nearest
        guard let sampler = device.makeSamplerState(descriptor: sdesc) else {
            return nil
        }

        self.device = device
        self.commandQueue = queue
        self.pipeline = pipeline
        self.sampler = sampler
        self.editor = editor
        self.canvasWidth = Float(canvasWidth)
        self.canvasHeight = Float(canvasHeight)
        self.viewportScale = editor.viewportScale()
        self.viewportOffsetX = editor.viewportOffsetX()
        self.viewportOffsetY = editor.viewportOffsetY()
        super.init()
    }

    func updateEditor(_ editor: illus.CanvasEditor) {
        self.editor = editor
    }

    func updateViewport(scale: Float, offsetX: Float, offsetY: Float) {
        viewportScale = scale
        viewportOffsetX = offsetX
        viewportOffsetY = offsetY
    }

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

    func draw(in view: MTKView) {
        // Resolve texture before encoding so a failed present doesn't leave an open encoder.
        let addr = editor.presentMetalTextureAddress()
        guard addr != 0,
              let texPtr = UnsafeMutableRawPointer(bitPattern: UInt(addr)) else {
            return
        }
        let source = Unmanaged<MTLTexture>.fromOpaque(texPtr).takeUnretainedValue()

        guard let drawable = view.currentDrawable,
              let rpd = view.currentRenderPassDescriptor,
              let commandBuffer = commandQueue.makeCommandBuffer(),
              let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: rpd) else {
            return
        }

        let dw = Float(drawable.texture.width)
        let dh = Float(drawable.texture.height)
        guard dw > 0, dh > 0 else {
            encoder.endEncoding()
            return
        }

        // Scalar NDC from cached viewport (TX-7 best use) — no GLM, no extra editor locks.
        let fit = min(dw / canvasWidth, dh / canvasHeight)
        let s = fit * viewportScale
        let drawnW = canvasWidth * s
        let drawnH = canvasHeight * s
        let originX = (dw - drawnW) * 0.5 - viewportOffsetX * s
        let originY = (dh - drawnH) * 0.5 - viewportOffsetY * s
        var uniforms = SIMD4<Float>(
            -1 + 2 * originX / dw,
            1 - 2 * (originY + drawnH) / dh,
            -1 + 2 * (originX + drawnW) / dw,
            1 - 2 * originY / dh
        )

        encoder.setRenderPipelineState(pipeline)
        encoder.setVertexBytes(&uniforms, length: MemoryLayout<SIMD4<Float>>.size, index: 0)
        encoder.setFragmentTexture(source, index: 0)
        encoder.setFragmentSamplerState(sampler, index: 0)
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
        encoder.endEncoding()

        commandBuffer.present(drawable)
        commandBuffer.commit()
    }
}

#if os(macOS)
struct CanvasMetalView: NSViewRepresentable {
    let editor: illus.CanvasEditor
    let canvasWidth: Int32
    let canvasHeight: Int32
    var viewportScale: Float = 1
    var viewportOffsetX: Float = 0
    var viewportOffsetY: Float = 0
    var highPerformancePresent: Bool = true
    var presentFps: Int = 72
    var onDragChanged: (CGPoint) -> Void
    var onDragEnded: () -> Void
    var onPan: (CGSize, CGSize) -> Void
    var onZoom: (CGFloat, CGPoint, CGSize) -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    func makeNSView(context: Context) -> MTKView {
        let view = FlippedMTKView(frame: .zero)
        view.device = context.coordinator.renderer?.deviceForView
        view.framebufferOnly = true
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.85, green: 0.85, blue: 0.85, alpha: 1)
        applyPresentMode(to: view)
        view.delegate = context.coordinator.renderer
        context.coordinator.wireScroll(view)
        context.coordinator.attachGestures(to: view)
        context.coordinator.renderer?.updateViewport(
            scale: viewportScale,
            offsetX: viewportOffsetX,
            offsetY: viewportOffsetY
        )
        return view
    }

    func updateNSView(_ nsView: MTKView, context: Context) {
        context.coordinator.parent = self
        context.coordinator.renderer?.updateEditor(editor)
        context.coordinator.renderer?.updateViewport(
            scale: viewportScale,
            offsetX: viewportOffsetX,
            offsetY: viewportOffsetY
        )
        applyPresentMode(to: nsView)
        if let flipped = nsView as? FlippedMTKView {
            context.coordinator.wireScroll(flipped)
        }
    }

    private func applyPresentMode(to view: MTKView) {
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        // performance_mode: app-recommended divisor of panel Hz; idle_mode: low refresh.
        view.preferredFramesPerSecond = highPerformancePresent ? presentFps : AppActiveIdleTracker.idlePresentFps
    }

    final class Coordinator: NSObject {
        var parent: CanvasMetalView
        let renderer: CanvasMetalRenderer?
        private var lastPanTranslation: CGPoint = .zero

        init(parent: CanvasMetalView) {
            self.parent = parent
            self.renderer = CanvasMetalRenderer(
                editor: parent.editor,
                canvasWidth: parent.canvasWidth,
                canvasHeight: parent.canvasHeight
            )
        }

        func wireScroll(_ view: FlippedMTKView) {
            view.scrollHandler = { [weak self] dx, dy, command, loc in
                guard let self else { return }
                let size = view.bounds.size
                if command {
                    let factor: CGFloat = dy > 0 ? 1.05 : (dy < 0 ? 1 / 1.05 : 1)
                    if factor != 1 { self.parent.onZoom(factor, loc, size) }
                } else {
                    self.parent.onPan(CGSize(width: dx, height: dy), size)
                }
            }
        }

        func attachGestures(to view: MTKView) {
            let draw = NSPanGestureRecognizer(target: self, action: #selector(handleDraw(_:)))
            draw.buttonMask = 0x1
            view.addGestureRecognizer(draw)

            let pan = NSPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
            pan.buttonMask = 0x2
            view.addGestureRecognizer(pan)

            let magnify = NSMagnificationGestureRecognizer(target: self, action: #selector(handleMagnify(_:)))
            view.addGestureRecognizer(magnify)
        }

        @objc func handleDraw(_ g: NSPanGestureRecognizer) {
            guard let view = g.view else { return }
            let loc = g.location(in: view)
            let p = canvasPoint(loc, in: view.bounds.size)
            switch g.state {
            case .began, .changed:
                parent.onDragChanged(p)
            case .ended, .cancelled:
                parent.onDragEnded()
            default:
                break
            }
        }

        @objc func handlePan(_ g: NSPanGestureRecognizer) {
            guard let view = g.view else { return }
            let t = g.translation(in: view)
            switch g.state {
            case .began:
                lastPanTranslation = t
            case .changed:
                let dx = t.x - lastPanTranslation.x
                let dy = t.y - lastPanTranslation.y
                lastPanTranslation = t
                parent.onPan(CGSize(width: dx, height: dy), view.bounds.size)
            case .ended, .cancelled:
                lastPanTranslation = .zero
            default:
                break
            }
        }

        @objc func handleMagnify(_ g: NSMagnificationGestureRecognizer) {
            guard let view = g.view, g.state == .changed || g.state == .ended else { return }
            let factor = 1 + g.magnification
            g.magnification = 0
            parent.onZoom(factor, g.location(in: view), view.bounds.size)
        }

        private func canvasPoint(_ location: CGPoint, in viewSize: CGSize) -> CGPoint {
            let ed = parent.editor
            let x = ed.viewToCanvasX(
                Float(location.x), Float(location.y),
                Float(viewSize.width), Float(viewSize.height)
            )
            let y = ed.viewToCanvasY(
                Float(location.x), Float(location.y),
                Float(viewSize.width), Float(viewSize.height)
            )
            return CGPoint(x: CGFloat(x), y: CGFloat(y))
        }
    }
}

/// AppKit default is bottom-left; flip so gesture Y matches canvas (top-left, Y down).
final class FlippedMTKView: MTKView {
    var scrollHandler: ((CGFloat, CGFloat, Bool, CGPoint) -> Void)?

    override var isFlipped: Bool { true }

    override func scrollWheel(with event: NSEvent) {
        scrollHandler?(
            event.scrollingDeltaX,
            event.scrollingDeltaY,
            event.modifierFlags.contains(.command),
            convert(event.locationInWindow, from: nil)
        )
    }
}

extension CanvasMetalRenderer {
    var deviceForView: MTLDevice { device }
}
#else
struct CanvasMetalView: UIViewRepresentable {
    let editor: illus.CanvasEditor
    let canvasWidth: Int32
    let canvasHeight: Int32
    var viewportScale: Float = 1
    var viewportOffsetX: Float = 0
    var viewportOffsetY: Float = 0
    var highPerformancePresent: Bool = true
    var presentFps: Int = 72
    var onDragChanged: (CGPoint) -> Void
    var onDragEnded: () -> Void
    var onPan: (CGSize, CGSize) -> Void
    var onZoom: (CGFloat, CGPoint, CGSize) -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero)
        view.device = context.coordinator.renderer?.deviceForView
        view.framebufferOnly = true
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.85, green: 0.85, blue: 0.85, alpha: 1)
        applyPresentMode(to: view)
        view.delegate = context.coordinator.renderer
        context.coordinator.attachGestures(to: view)
        context.coordinator.renderer?.updateViewport(
            scale: viewportScale,
            offsetX: viewportOffsetX,
            offsetY: viewportOffsetY
        )
        return view
    }

    func updateUIView(_ uiView: MTKView, context: Context) {
        context.coordinator.parent = self
        context.coordinator.renderer?.updateEditor(editor)
        context.coordinator.renderer?.updateViewport(
            scale: viewportScale,
            offsetX: viewportOffsetX,
            offsetY: viewportOffsetY
        )
        applyPresentMode(to: uiView)
    }

    private func applyPresentMode(to view: MTKView) {
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        // performance_mode: app-recommended divisor of panel Hz; idle_mode: low refresh.
        view.preferredFramesPerSecond = highPerformancePresent ? presentFps : AppActiveIdleTracker.idlePresentFps
    }

    final class Coordinator: NSObject {
        var parent: CanvasMetalView
        let renderer: CanvasMetalRenderer?
        private var lastPanTranslation: CGPoint = .zero

        init(parent: CanvasMetalView) {
            self.parent = parent
            self.renderer = CanvasMetalRenderer(
                editor: parent.editor,
                canvasWidth: parent.canvasWidth,
                canvasHeight: parent.canvasHeight
            )
        }

        func attachGestures(to view: MTKView) {
            let draw = UIPanGestureRecognizer(target: self, action: #selector(handleDraw(_:)))
            draw.maximumNumberOfTouches = 1
            view.addGestureRecognizer(draw)

            let pan = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
            pan.minimumNumberOfTouches = 2
            pan.maximumNumberOfTouches = 2
            view.addGestureRecognizer(pan)

            let pinch = UIPinchGestureRecognizer(target: self, action: #selector(handlePinch(_:)))
            view.addGestureRecognizer(pinch)
        }

        @objc func handleDraw(_ g: UIPanGestureRecognizer) {
            let loc = g.location(in: g.view)
            let size = g.view?.bounds.size ?? .zero
            let p = canvasPoint(loc, in: size)
            switch g.state {
            case .began, .changed:
                parent.onDragChanged(p)
            case .ended, .cancelled:
                parent.onDragEnded()
            default:
                break
            }
        }

        @objc func handlePan(_ g: UIPanGestureRecognizer) {
            let t = g.translation(in: g.view)
            switch g.state {
            case .began:
                lastPanTranslation = t
            case .changed:
                let dx = t.x - lastPanTranslation.x
                let dy = t.y - lastPanTranslation.y
                lastPanTranslation = t
                parent.onPan(CGSize(width: dx, height: dy), g.view?.bounds.size ?? .zero)
            case .ended, .cancelled:
                lastPanTranslation = .zero
            default:
                break
            }
        }

        @objc func handlePinch(_ g: UIPinchGestureRecognizer) {
            guard let view = g.view, g.state == .changed || g.state == .ended else { return }
            let factor = g.scale
            g.scale = 1
            parent.onZoom(factor, g.location(in: view), view.bounds.size)
        }

        private func canvasPoint(_ location: CGPoint, in viewSize: CGSize) -> CGPoint {
            let ed = parent.editor
            let x = ed.viewToCanvasX(
                Float(location.x), Float(location.y),
                Float(viewSize.width), Float(viewSize.height)
            )
            let y = ed.viewToCanvasY(
                Float(location.x), Float(location.y),
                Float(viewSize.width), Float(viewSize.height)
            )
            return CGPoint(x: CGFloat(x), y: CGFloat(y))
        }
    }
}

extension CanvasMetalRenderer {
    var deviceForView: MTLDevice { device }
}
#endif
