//
//  DrawingEditorView.swift
//  gmangastudio
//

import IllusStudioFramework
import SwiftUI
import UniformTypeIdentifiers

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
                    brushSubmode: viewModel.brushSubmode,
                    isBrushLibraryVisible: viewModel.isBrushLibraryVisible,
                    isLayersVisible: viewModel.isLayersVisible,
                    showBrushEditOptions: viewModel.isBrushEditOptionsVisible,
                    brushColor: viewModel.brushColor,
                    brushSize: viewModel.brushSize,
                    brushOpacity: viewModel.brushOpacity,
                    colorHistory: viewModel.colorHistory,
                    onSelectMode: { viewModel.setMode($0) },
                    onToggleBrushLibrary: { viewModel.toggleBrushLibrary() },
                    onToggleLayers: { viewModel.toggleLayers() },
                    onBrushColorChange: { viewModel.setBrushColor($0) },
                    onSelectHistoryColor: { viewModel.setBrushColor($0) },
                    onClearColorHistory: { viewModel.clearColorHistory() },
                    onColorPickerDismiss: { viewModel.recordBrushColorHistory() },
                    onBrushSizeChange: { viewModel.setBrushSize($0) },
                    onBrushOpacityChange: { viewModel.setBrushOpacity($0) },
                    onEnterEyedropper: { viewModel.enterEyedropper() },
                    onSelectBrushSubmode: { viewModel.setBrushSubmode($0) }
                )

                Group {
                    if viewModel.metalAvailable {
                        CanvasMetalView(
                            editor: viewModel.editor,
                            canvasWidth: viewModel.canvasWidth,
                            canvasHeight: viewModel.canvasHeight,
                            viewportScale: viewModel.viewportScale,
                            viewportOffsetX: viewModel.viewportOffsetX,
                            viewportOffsetY: viewModel.viewportOffsetY,
                            highPerformancePresent: viewModel.appActiveStatus == .performanceMode,
                            presentFps: viewModel.presentFps,
                            eyedropperActive: viewModel.isEyedropperActive,
                            onDragChanged: { viewModel.pointerChanged(at: $0, pressure: $1) },
                            onDragEnded: { viewModel.pointerEnded() },
                            onPan: { delta, size in viewModel.panBy(delta, viewSize: size) },
                            onZoom: { factor, focus, size in
                                viewModel.zoomBy(factor, focus: focus, viewSize: size)
                            },
                            onHoverMoved: { viewPt, canvasPt in
                                viewModel.eyedropperMoved(viewPoint: viewPt, canvasPoint: canvasPt)
                            },
                            onEyedropperPick: { _, canvasPt in
                                viewModel.eyedropperPick(at: canvasPt)
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

            if viewModel.showEyedropperPreview {
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(viewModel.eyedropperPreviewColor)
                    .overlay(
                        RoundedRectangle(cornerRadius: 4, style: .continuous)
                            .strokeBorder(Color.white, lineWidth: 2)
                    )
                    .frame(width: 28, height: 28)
                    .shadow(color: .black.opacity(0.35), radius: 2, y: 1)
                    .position(
                        x: sidebarWidth + viewModel.eyedropperViewPoint.x + 22,
                        y: viewModel.eyedropperViewPoint.y
                    )
                    .allowsHitTesting(false)
                    .zIndex(20)
            }

            if viewModel.isBrushLibraryVisible {
                BrushLibraryWidgetView(
                    sets: viewModel.brushSets,
                    presets: viewModel.brushPresets,
                    selectedSetIndex: viewModel.selectedBrushSetIndex,
                    selectedPresetIndex: viewModel.selectedBrushPresetIndex,
                    isImporting: viewModel.isBrushImporting,
                    onSelectSet: { viewModel.selectBrushSet($0) },
                    onSelectPreset: { viewModel.selectBrushPreset($0) },
                    onImport: { viewModel.presentBrushImport() },
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
                    widgetOriginX: layersOrigin.x,
                    documentWidth: viewModel.canvasWidth,
                    documentHeight: viewModel.canvasHeight
                )
                .fixedSize(horizontal: true, vertical: true)
                .offset(x: layersOrigin.x, y: layersOrigin.y)
                .zIndex(11)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .fileImporter(
            isPresented: Binding(
                get: { viewModel.isBrushImportPresented },
                set: { viewModel.isBrushImportPresented = $0 }
            ),
            allowedContentTypes: [
                UTType(filenameExtension: "brush") ?? .data,
                UTType(filenameExtension: "brushset") ?? .data,
                UTType(filenameExtension: "brushlibrary") ?? .data,
            ],
            allowsMultipleSelection: false
        ) { result in
            guard case .success(let urls) = result, let url = urls.first else { return }
            viewModel.importBrushPackage(from: url)
        }
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
