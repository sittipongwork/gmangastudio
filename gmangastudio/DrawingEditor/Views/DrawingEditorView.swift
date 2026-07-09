//
//  DrawingEditorView.swift
//  gmangastudio
//

import IllusStudioFramework
import SwiftUI

struct DrawingEditorView: View {
    @State private var viewModel = DrawingEditorViewModel()
    @State private var brushLibraryOrigin = BrushLibraryWidgetPositionStore.load()
    @State private var brushLibraryDragStart: CGPoint?
    @State private var layersOrigin = LayersWidgetPositionStore.load()
    @State private var layersDragStart: CGPoint?

    private let sidebarWidth: CGFloat = 60

    var body: some View {
        ZStack(alignment: .topLeading) {
            HStack(spacing: 0) {
                DrawingEditorSidebarView(
                    mode: viewModel.mode,
                    isBrushLibraryVisible: viewModel.isBrushLibraryVisible,
                    isLayersVisible: viewModel.isLayersVisible,
                    onSelectMode: { viewModel.setMode($0) },
                    onToggleBrushLibrary: { viewModel.toggleBrushLibrary() },
                    onToggleLayers: { viewModel.toggleLayers() }
                )

                Group {
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

            if viewModel.isBrushLibraryVisible {
                BrushLibraryWidgetView(
                    sets: viewModel.brushSets,
                    presets: viewModel.brushPresets,
                    selectedSetIndex: viewModel.selectedBrushSetIndex,
                    selectedPresetIndex: viewModel.selectedBrushPresetIndex,
                    onSelectSet: { viewModel.selectBrushSet($0) },
                    onSelectPreset: { viewModel.selectBrushPreset($0) },
                    onClose: { viewModel.toggleBrushLibrary() },
                    onMoveChanged: { value in
                        if brushLibraryDragStart == nil {
                            viewModel.noteUserActivity()
                            brushLibraryDragStart = brushLibraryOrigin
                        }
                        guard let start = brushLibraryDragStart else { return }
                        brushLibraryOrigin = CGPoint(
                            x: max(sidebarWidth, start.x + value.translation.width),
                            y: max(0, start.y + value.translation.height)
                        )
                    },
                    onMoveEnded: {
                        brushLibraryDragStart = nil
                        BrushLibraryWidgetPositionStore.save(brushLibraryOrigin)
                    }
                )
                .offset(x: brushLibraryOrigin.x, y: brushLibraryOrigin.y)
                .zIndex(10)
            }

            if viewModel.isLayersVisible {
                LayersWidgetView(
                    layers: viewModel.layers,
                    activeLayerId: viewModel.activeLayerId,
                    onSelect: { viewModel.selectLayer($0) },
                    onToggleVisible: { viewModel.toggleLayerVisible($0) },
                    onReorder: { viewModel.reorderPaintLayers(from: $0, to: $1) },
                    onAdd: { viewModel.addLayer() },
                    onClear: { viewModel.clearLayer($0) },
                    onDuplicate: { viewModel.duplicateLayer($0) },
                    onMergeDown: { viewModel.mergeLayerDown($0) },
                    onClose: { viewModel.toggleLayers() },
                    onMoveChanged: { value in
                        if layersDragStart == nil {
                            viewModel.noteUserActivity()
                            layersDragStart = layersOrigin
                        }
                        guard let start = layersDragStart else { return }
                        layersOrigin = CGPoint(
                            x: max(sidebarWidth, start.x + value.translation.width),
                            y: max(0, start.y + value.translation.height)
                        )
                    },
                    onMoveEnded: {
                        layersDragStart = nil
                        LayersWidgetPositionStore.save(layersOrigin)
                    },
                    documentWidth: viewModel.canvasWidth,
                    documentHeight: viewModel.canvasHeight
                )
                .fixedSize(horizontal: true, vertical: true)
                .offset(x: layersOrigin.x, y: layersOrigin.y)
                .zIndex(11)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .onAppear {
            brushLibraryOrigin = FloatingWidgetPositionStore.seedIfNeeded(
                key: BrushLibraryWidgetPositionStore.key,
                defaultOrigin: BrushLibraryWidgetPositionStore.defaultOrigin
            )
            layersOrigin = FloatingWidgetPositionStore.seedIfNeeded(
                key: LayersWidgetPositionStore.key,
                defaultOrigin: LayersWidgetPositionStore.defaultOrigin
            )
        }
    }
}

#Preview {
    DrawingEditorView()
}
