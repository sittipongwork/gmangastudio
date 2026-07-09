//
//  DrawingEditorView.swift
//  gmangastudio
//

import IllusStudioFramework
import SwiftUI

struct DrawingEditorView: View {
    @State private var viewModel = DrawingEditorViewModel()

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text(viewModel.engineVersion)
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
                Spacer()
                Button("Clear") { viewModel.clear() }
                    .keyboardShortcut("k", modifiers: [.command])
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)

            GeometryReader { geo in
                ZStack {
                    Color(white: 0.85)
                    if let image = viewModel.canvasImage {
                        Image(decorative: image, scale: 1)
                            .resizable()
                            .interpolation(.none)
                            .aspectRatio(contentMode: .fit)
                            .gesture(
                                DragGesture(minimumDistance: 0)
                                    .onChanged { value in
                                        viewModel.pointerChanged(
                                            at: canvasPoint(value.location, in: geo.size)
                                        )
                                    }
                                    .onEnded { _ in
                                        viewModel.pointerEnded()
                                    }
                            )
                    } else {
                        Text("Canvas unavailable")
                            .foregroundStyle(.secondary)
                    }
                }
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    /// Map view coordinates into 1920×1080 canvas space (aspect-fit).
    private func canvasPoint(_ location: CGPoint, in viewSize: CGSize) -> CGPoint {
        let canvasW: CGFloat = 1920
        let canvasH: CGFloat = 1080
        let scale = min(viewSize.width / canvasW, viewSize.height / canvasH)
        let drawnW = canvasW * scale
        let drawnH = canvasH * scale
        let originX = (viewSize.width - drawnW) * 0.5
        let originY = (viewSize.height - drawnH) * 0.5
        let x = (location.x - originX) / scale
        let y = (location.y - originY) / scale
        return CGPoint(x: x, y: y)
    }
}

#Preview {
    DrawingEditorView()
}
