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
        super.init()
    }

    func updateEditor(_ editor: illus.CanvasEditor) {
        self.editor = editor
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
        let scale = min(dw / canvasWidth, dh / canvasHeight)
        let w = canvasWidth * scale
        let h = canvasHeight * scale
        var uniforms = SIMD4<Float>(-w / dw, -h / dh, w / dw, h / dh)

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
    var onDragChanged: (CGPoint) -> Void
    var onDragEnded: () -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    func makeNSView(context: Context) -> MTKView {
        let view = FlippedMTKView(frame: .zero)
        view.device = context.coordinator.renderer?.deviceForView
        view.framebufferOnly = true
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.85, green: 0.85, blue: 0.85, alpha: 1)
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        view.preferredFramesPerSecond = 120
        view.delegate = context.coordinator.renderer
        context.coordinator.attachGestures(to: view)
        return view
    }

    func updateNSView(_ nsView: MTKView, context: Context) {
        context.coordinator.parent = self
        context.coordinator.renderer?.updateEditor(editor)
        nsView.preferredFramesPerSecond = 120
    }

    final class Coordinator: NSObject {
        var parent: CanvasMetalView
        let renderer: CanvasMetalRenderer?

        init(parent: CanvasMetalView) {
            self.parent = parent
            self.renderer = CanvasMetalRenderer(
                editor: parent.editor,
                canvasWidth: parent.canvasWidth,
                canvasHeight: parent.canvasHeight
            )
        }

        func attachGestures(to view: MTKView) {
            let pan = NSPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
            view.addGestureRecognizer(pan)
        }

        @objc func handlePan(_ g: NSPanGestureRecognizer) {
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

        /// View is flipped (top-left origin), matching canvas / UIKit.
        private func canvasPoint(_ location: CGPoint, in viewSize: CGSize) -> CGPoint {
            let canvasW = CGFloat(parent.canvasWidth)
            let canvasH = CGFloat(parent.canvasHeight)
            let scale = min(viewSize.width / canvasW, viewSize.height / canvasH)
            let drawnW = canvasW * scale
            let drawnH = canvasH * scale
            let originX = (viewSize.width - drawnW) * 0.5
            let originY = (viewSize.height - drawnH) * 0.5
            return CGPoint(
                x: (location.x - originX) / scale,
                y: (location.y - originY) / scale
            )
        }
    }
}

/// AppKit default is bottom-left; flip so gesture Y matches canvas (top-left, Y down).
final class FlippedMTKView: MTKView {
    override var isFlipped: Bool { true }
}

extension CanvasMetalRenderer {
    var deviceForView: MTLDevice { device }
}
#else
struct CanvasMetalView: UIViewRepresentable {
    let editor: illus.CanvasEditor
    let canvasWidth: Int32
    let canvasHeight: Int32
    var onDragChanged: (CGPoint) -> Void
    var onDragEnded: () -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero)
        view.device = context.coordinator.renderer?.deviceForView
        view.framebufferOnly = true
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.85, green: 0.85, blue: 0.85, alpha: 1)
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        view.preferredFramesPerSecond = 120
        view.delegate = context.coordinator.renderer
        context.coordinator.attachGestures(to: view)
        return view
    }

    func updateUIView(_ uiView: MTKView, context: Context) {
        context.coordinator.parent = self
        context.coordinator.renderer?.updateEditor(editor)
        uiView.preferredFramesPerSecond = 120
    }

    final class Coordinator: NSObject {
        var parent: CanvasMetalView
        let renderer: CanvasMetalRenderer?

        init(parent: CanvasMetalView) {
            self.parent = parent
            self.renderer = CanvasMetalRenderer(
                editor: parent.editor,
                canvasWidth: parent.canvasWidth,
                canvasHeight: parent.canvasHeight
            )
        }

        func attachGestures(to view: MTKView) {
            let pan = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
            view.addGestureRecognizer(pan)
        }

        @objc func handlePan(_ g: UIPanGestureRecognizer) {
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

        private func canvasPoint(_ location: CGPoint, in viewSize: CGSize) -> CGPoint {
            let canvasW = CGFloat(parent.canvasWidth)
            let canvasH = CGFloat(parent.canvasHeight)
            let scale = min(viewSize.width / canvasW, viewSize.height / canvasH)
            let drawnW = canvasW * scale
            let drawnH = canvasH * scale
            let originX = (viewSize.width - drawnW) * 0.5
            let originY = (viewSize.height - drawnH) * 0.5
            return CGPoint(
                x: (location.x - originX) / scale,
                y: (location.y - originY) / scale
            )
        }
    }
}

extension CanvasMetalRenderer {
    var deviceForView: MTLDevice { device }
}
#endif
