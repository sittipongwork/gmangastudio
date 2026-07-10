//
//  DrawingEditorSidebarView.swift
//  gmangastudio
//

import SwiftUI

struct DrawingEditorSidebarView: View {
    var mode: DrawingEditorMode
    var brushSubmode: BrushSubmode = .brush
    var isBrushLibraryVisible: Bool = false
    var isLayersVisible: Bool = false
    var showBrushEditOptions: Bool = false
    var brushColor: Color = .black
    var brushSize: Double = 16
    var brushOpacity: Double = 1
    var colorHistory: [Color] = []
    var onSelectMode: (DrawingEditorMode) -> Void
    var onToggleBrushLibrary: () -> Void = {}
    var onToggleLayers: () -> Void = {}
    var onBrushColorChange: (Color) -> Void = { _ in }
    var onSelectHistoryColor: (Color) -> Void = { _ in }
    var onClearColorHistory: () -> Void = {}
    var onColorPickerDismiss: () -> Void = {}
    var onBrushSizeChange: (Double) -> Void = { _ in }
    var onBrushOpacityChange: (Double) -> Void = { _ in }
    var onEnterEyedropper: () -> Void = {}
    var onSelectBrushSubmode: (BrushSubmode) -> Void = { _ in }

    @State private var isColorPickerPresented = false

    private let buttonSize: CGFloat = 46
    private let sliderHeight: CGFloat = 120

    var body: some View {
        VStack(spacing: 8) {
            modeButton(mode: .pointer, systemImage: "cursorarrow", label: "Pointer")
            brushLibraryButton
            modeButton(mode: .eraser, systemImage: "eraser.fill", label: "Eraser")

            Button {
                onToggleLayers()
            } label: {
                sidebarIcon(
                    systemImage: "square.2.layers.3d.fill",
                    label: "Layer",
                    selected: isLayersVisible
                )
            }
            .buttonStyle(.plain)
            .frame(width: buttonSize, height: buttonSize)
            .accessibilityLabel("Layer")

            menuButton(systemImage: "plus.app.fill", label: "Edit") {
                Button("Undo") {}
                Button("Redo") {}
                Divider()
                Button("Cut") {}
                Button("Copy") {}
                Button("Paste") {}
            }

            if showBrushEditOptions {
                brushEditOptions
            }

            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .frame(width: 60)
        .frame(maxHeight: .infinity)
        .background(Color.black)
    }

    /// Brush color / eyedropper / size / opacity — visible while brush mode is active.
    private var brushEditOptions: some View {
        VStack(spacing: 8) {
            brushColorButton
            eyedropperButton
            VerticalPillSlider(
                value: Binding(
                    get: { brushSize },
                    set: { onBrushSizeChange($0) }
                ),
                range: 1...100,
                accessibilityLabel: "Brush Size"
            )
            .frame(width: 22, height: sliderHeight)

            VerticalPillSlider(
                value: Binding(
                    get: { brushOpacity },
                    set: { onBrushOpacityChange($0) }
                ),
                range: 0...1,
                accessibilityLabel: "Brush Opacity"
            )
            .frame(width: 22, height: sliderHeight)
        }
        .padding(.top, 4)
    }

    private var brushColorButton: some View {
        Button {
            isColorPickerPresented = true
        } label: {
            Circle()
                .fill(brushColor)
                .overlay(
                    Circle()
                        .strokeBorder(Color.white.opacity(0.85), lineWidth: 2)
                )
                .frame(width: buttonSize, height: buttonSize)
                .background(Color.white.opacity(0.12))
                .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
        }
        .buttonStyle(.plain)
        .frame(width: buttonSize, height: buttonSize)
        .accessibilityLabel("Brush Color")
        .popover(isPresented: $isColorPickerPresented, arrowEdge: .trailing) {
            BrushColorPickerView(
                color: Binding(
                    get: { brushColor },
                    set: { onBrushColorChange($0) }
                ),
                history: colorHistory,
                onSelectHistory: onSelectHistoryColor,
                onClearHistory: onClearColorHistory,
                onColorCommitted: onBrushColorChange
            )
        }
        .onChange(of: isColorPickerPresented) { wasOpen, isOpen in
            if wasOpen && !isOpen {
                onColorPickerDismiss()
            }
        }
    }

    private var eyedropperButton: some View {
        Button {
            if brushSubmode == .eyedropper {
                onSelectBrushSubmode(.brush)
            } else {
                onEnterEyedropper()
            }
        } label: {
            sidebarIcon(
                systemImage: "eyedropper",
                label: "Eyedropper",
                selected: brushSubmode == .eyedropper
            )
        }
        .buttonStyle(.plain)
        .frame(width: buttonSize, height: buttonSize)
        .accessibilityLabel("Eyedropper")
    }

    private var brushLibraryButton: some View {
        Button {
            onToggleBrushLibrary()
        } label: {
            sidebarIcon(
                systemImage: "paintbrush.pointed.fill",
                label: "Brush Library",
                selected: mode == .brushLibrary || isBrushLibraryVisible
            )
        }
        .buttonStyle(.plain)
        .frame(width: buttonSize, height: buttonSize)
        .accessibilityLabel("Brush Library")
    }

    private func modeButton(mode: DrawingEditorMode, systemImage: String, label: String) -> some View {
        Button {
            onSelectMode(mode)
        } label: {
            sidebarIcon(systemImage: systemImage, label: label, selected: self.mode == mode)
        }
        .buttonStyle(.plain)
        .frame(width: buttonSize, height: buttonSize)
        .accessibilityLabel(label)
    }

    private func menuButton<Content: View>(
        systemImage: String,
        label: String,
        @ViewBuilder content: () -> Content
    ) -> some View {
        Menu {
            content()
        } label: {
            sidebarIcon(systemImage: systemImage, label: label, selected: false)
        }
        .menuStyle(.button)
        .buttonStyle(.plain)
        .menuIndicator(.hidden)
        .frame(width: buttonSize, height: buttonSize)
        .accessibilityLabel(label)
    }

    private func sidebarIcon(systemImage: String, label: String, selected: Bool) -> some View {
        Image(systemName: systemImage)
            .font(.system(size: 18, weight: .medium))
            .foregroundStyle(selected ? Color.black : Color.white)
            .frame(width: buttonSize, height: buttonSize)
            .background(selected ? Color.white : Color.white.opacity(0.12))
            .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
            .accessibilityLabel(label)
    }
}
