//
//  DrawingEditorView.swift
//  gmangastudio
//

import IllusStudioFramework
import SwiftUI

struct DrawingEditorView: View {
    @State private var viewModel = DrawingEditorViewModel()

    var body: some View {
        HStack(spacing: 0) {
            DrawingEditorSidebarView(
                mode: viewModel.mode,
                onSelectMode: { viewModel.setMode($0) },
                onAddLayer: { viewModel.addLayer() }
            )

            VStack(spacing: 0) {
                HStack {
                    Text(viewModel.engineVersion)
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                    Text("layers: \(viewModel.layerCount)")
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                    Text(presentLabel)
                        .font(.caption.monospaced())
                        .foregroundStyle(presentColor)
                    Text("\(viewModel.zoomPercent)%")
                        .font(.caption.monospaced())
                        .foregroundStyle(.secondary)
                    Spacer()
                    Button("Fit") { viewModel.resetViewport() }
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
                        highPerformancePresent: viewModel.appActiveStatus == .performanceMode,
                        onDragChanged: { viewModel.pointerChanged(at: $0) },
                        onDragEnded: { viewModel.pointerEnded() },
                        onPan: { delta, size in viewModel.panBy(delta, viewSize: size) },
                        onZoom: { factor, focus, size in
                            viewModel.zoomBy(factor, focus: focus, viewSize: size)
                        }
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
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var presentLabel: String {
        guard viewModel.metalAvailable else { return "CPU" }
        return viewModel.appActiveStatus == .performanceMode ? "Metal 120Hz" : "Metal 10Hz"
    }

    private var presentColor: Color {
        guard viewModel.metalAvailable else { return .orange }
        return viewModel.appActiveStatus == .performanceMode ? .green : .secondary
    }
}

#Preview {
    DrawingEditorView()
}
