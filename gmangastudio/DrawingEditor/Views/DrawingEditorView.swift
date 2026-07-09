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
                Text("layers: \(viewModel.layerCount)")
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
                Text(viewModel.metalAvailable ? "Metal 120Hz" : "CPU")
                    .font(.caption.monospaced())
                    .foregroundStyle(viewModel.metalAvailable ? .green : .orange)
                Spacer()
                Button("Add Layer") { viewModel.addLayer() }
                Button("Clear") { viewModel.clear() }
                    .keyboardShortcut("k", modifiers: [.command])
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)

            if viewModel.metalAvailable {
                CanvasMetalView(
                    editor: viewModel.editor,
                    canvasWidth: viewModel.canvasWidth,
                    canvasHeight: viewModel.canvasHeight,
                    onDragChanged: { viewModel.pointerChanged(at: $0) },
                    onDragEnded: { viewModel.pointerEnded() }
                )
            } else {
                Text("Metal unavailable")
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .background(Color(white: 0.85))
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

#Preview {
    DrawingEditorView()
}
