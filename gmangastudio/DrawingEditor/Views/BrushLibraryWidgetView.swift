//
//  BrushLibraryWidgetView.swift
//  gmangastudio
//

import SwiftUI

struct BrushLibrarySetItem: Identifiable, Equatable {
    let id: Int32
    let name: String
    let systemImage: String
    let isImported: Bool
}

struct BrushLibraryPresetItem: Identifiable, Equatable {
    let id: Int32 // preset index within set
    let name: String
    let strokeWeight: CGFloat
    let approximated: Bool
}

struct BrushLibraryWidgetView: View {
    var sets: [BrushLibrarySetItem]
    var presets: [BrushLibraryPresetItem]
    var selectedSetIndex: Int32
    var selectedPresetIndex: Int32
    var onSelectSet: (Int32) -> Void
    var onSelectPreset: (Int32) -> Void
    var onImport: () -> Void = {}
    var onClose: () -> Void = {}
    var onMoveChanged: ((DragGesture.Value) -> Void)? = nil
    var onMoveEnded: (() -> Void)? = nil

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Capsule()
                .fill(Color.white.opacity(0.35))
                .frame(width: 36, height: 5)
                .frame(maxWidth: .infinity)
                .padding(.top, 8)
                .padding(.bottom, 6)
                .contentShape(Rectangle())
                .gesture(
                    // Global space: local translation shakes when .offset updates mid-drag.
                    DragGesture(coordinateSpace: .global)
                        .onChanged { onMoveChanged?($0) }
                        .onEnded { _ in onMoveEnded?() }
                )
                .accessibilityLabel("Move Brush Library")

            HStack {
                Text("Brush Library")
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundStyle(.white)
                Spacer()
                Button(action: onImport) {
                    Image(systemName: "square.and.arrow.down")
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(.white)
                        .frame(width: 28, height: 28)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Import Procreate brush")
                Button(action: onClose) {
                    Image(systemName: "xmark")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(.white.opacity(0.85))
                        .frame(width: 28, height: 28)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Close Brush Library")
            }
            .padding(.horizontal, 14)
            .padding(.bottom, 8)

            HStack(alignment: .top, spacing: 0) {
                ScrollView {
                    VStack(alignment: .leading, spacing: 2) {
                        ForEach(sets) { set in
                            Button {
                                onSelectSet(set.id)
                            } label: {
                                HStack(spacing: 8) {
                                    Image(systemName: set.systemImage)
                                        .font(.system(size: 12))
                                        .frame(width: 16)
                                    Text(set.name)
                                        .font(.system(size: 13))
                                        .lineLimit(1)
                                    Spacer(minLength: 0)
                                }
                                .foregroundStyle(selectedSetIndex == set.id ? Color.white : Color.white.opacity(0.75))
                                .padding(.horizontal, 10)
                                .padding(.vertical, 7)
                                .background(
                                    selectedSetIndex == set.id
                                        ? Color.white.opacity(0.12)
                                        : Color.clear
                                )
                                .clipShape(RoundedRectangle(cornerRadius: 6, style: .continuous))
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    .padding(.horizontal, 8)
                    .padding(.bottom, 10)
                }
                .frame(width: 148)

                Divider()
                    .background(Color.white.opacity(0.15))

                ScrollView {
                    VStack(spacing: 6) {
                        ForEach(presets) { preset in
                            Button {
                                onSelectPreset(preset.id)
                            } label: {
                                VStack(alignment: .leading, spacing: 6) {
                                    HStack(alignment: .firstTextBaseline) {
                                        Text(preset.name)
                                            .font(.system(size: 13, weight: .medium))
                                            .foregroundStyle(.white)
                                        Spacer(minLength: 4)
                                        if preset.approximated {
                                            Text("approx")
                                                .font(.system(size: 10, weight: .semibold))
                                                .foregroundStyle(.white.opacity(0.7))
                                                .padding(.horizontal, 6)
                                                .padding(.vertical, 2)
                                                .background(Color.white.opacity(0.12))
                                                .clipShape(Capsule())
                                        }
                                    }
                                    BrushStrokePreview(weight: preset.strokeWeight)
                                        .frame(height: 28)
                                }
                                .padding(.horizontal, 10)
                                .padding(.vertical, 8)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .background(
                                    selectedPresetIndex == preset.id
                                        ? Color(red: 0.18, green: 0.48, blue: 0.95)
                                        : Color.white.opacity(0.06)
                                )
                                .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    .padding(.horizontal, 10)
                    .padding(.bottom, 10)
                }
                .frame(maxWidth: .infinity)
            }
        }
        .frame(width: 360, height: 480)
        .background {
            ZStack {
                Rectangle().fill(.ultraThinMaterial) // blur behind
                Color.black.opacity(0.6)
            }
        }
        .environment(\.colorScheme, .dark)
        .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(Color.white.opacity(0.12), lineWidth: 1)
        )
        .shadow(color: .black.opacity(0.35), radius: 18, y: 8)
        .contentShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
    }
}

/// Simple stroke chip — ponytail: placeholder until tip previews exist.
private struct BrushStrokePreview: View {
    var weight: CGFloat

    var body: some View {
        GeometryReader { geo in
            let w = max(1.5, min(10, weight))
            Path { path in
                let y = geo.size.height * 0.55
                path.move(to: CGPoint(x: 4, y: y))
                path.addCurve(
                    to: CGPoint(x: geo.size.width - 4, y: y - 2),
                    control1: CGPoint(x: geo.size.width * 0.3, y: y - 10),
                    control2: CGPoint(x: geo.size.width * 0.65, y: y + 10)
                )
            }
            .stroke(Color.white, style: StrokeStyle(lineWidth: w, lineCap: .round, lineJoin: .round))
        }
    }
}

#Preview {
    BrushLibraryWidgetView(
        sets: [
            .init(id: 0, name: "Built-in", systemImage: "paintbrush.pointed", isImported: false),
            .init(id: 1, name: "Inking", systemImage: "pencil.tip", isImported: true),
        ],
        presets: [
            .init(id: 0, name: "ink.round", strokeWeight: 4, approximated: false),
            .init(id: 1, name: "air.soft", strokeWeight: 8, approximated: true),
        ],
        selectedSetIndex: 0,
        selectedPresetIndex: 0,
        onSelectSet: { _ in },
        onSelectPreset: { _ in }
    )
    .padding()
    .background(Color.gray)
}
