//
//  DrawingEditorSidebarView.swift
//  gmangastudio
//

import SwiftUI

struct DrawingEditorSidebarView: View {
    var mode: DrawingEditorMode
    var onSelectMode: (DrawingEditorMode) -> Void
    var onAddLayer: () -> Void = {}

    private let buttonSize: CGFloat = 46

    var body: some View {
        VStack(spacing: 8) {
            modeButton(mode: .pointer, systemImage: "cursorarrow", label: "Pointer")
            modeButton(mode: .brushLibrary, systemImage: "paintbrush.pointed", label: "Brush Library")
            modeButton(mode: .eraser, systemImage: "eraser", label: "Eraser")

            Menu {
                Button("Add Layer", action: onAddLayer)
                Button("Delete Layer") {}
                Button("Duplicate Layer") {}
            } label: {
                sidebarIcon(systemImage: "square.3.layers.3d", label: "Layer", selected: false)
            }
            .menuStyle(.borderlessButton)
            .frame(width: buttonSize, height: buttonSize)

            Menu {
                Button("Undo") {}
                Button("Redo") {}
                Divider()
                Button("Cut") {}
                Button("Copy") {}
                Button("Paste") {}
            } label: {
                sidebarIcon(systemImage: "slider.horizontal.3", label: "Edit", selected: false)
            }
            .menuStyle(.borderlessButton)
            .frame(width: buttonSize, height: buttonSize)

            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .frame(width: 60)
        .frame(maxHeight: .infinity)
        .background(Color.black)
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
