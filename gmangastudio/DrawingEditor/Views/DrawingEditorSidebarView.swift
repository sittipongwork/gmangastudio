//
//  DrawingEditorSidebarView.swift
//  gmangastudio
//

import SwiftUI

struct DrawingEditorSidebarView: View {
    var mode: DrawingEditorMode
    var isBrushLibraryVisible: Bool = false
    var isLayersVisible: Bool = false
    var onSelectMode: (DrawingEditorMode) -> Void
    var onToggleBrushLibrary: () -> Void = {}
    var onToggleLayers: () -> Void = {}

    private let buttonSize: CGFloat = 46

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

            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .frame(width: 60)
        .frame(maxHeight: .infinity)
        .background(Color.black)
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
