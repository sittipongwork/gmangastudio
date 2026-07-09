//
//  LayersWidgetView.swift
//  gmangastudio
//

import SwiftUI

#if os(macOS)
import AppKit
#else
import UIKit
#endif

struct LayerListItem: Identifiable, Equatable {
    let id: Int32
    let name: String
    let visible: Bool
    let isBackground: Bool
    /// Bumps when layer pixels change so SwiftUI refreshes the thumb.
    let contentRevision: Int
    let thumbnail: Image?

    static func == (lhs: LayerListItem, rhs: LayerListItem) -> Bool {
        lhs.id == rhs.id
            && lhs.name == rhs.name
            && lhs.visible == rhs.visible
            && lhs.isBackground == rhs.isBackground
            && lhs.contentRevision == rhs.contentRevision
    }
}

struct LayersWidgetView: View {
    var layers: [LayerListItem]
    var activeLayerId: Int32
    var onSelect: (Int32) -> Void
    var onToggleVisible: (Int32) -> Void
    var onReorder: (IndexSet, Int) -> Void = { _, _ in }
    var onAdd: () -> Void = {}
    var onClear: (Int32) -> Void = { _ in }
    var onDuplicate: (Int32) -> Void = { _ in }
    var onMergeDown: (Int32) -> Void = { _ in }
    var onClose: () -> Void = {}
    var onMoveChanged: ((DragGesture.Value) -> Void)? = nil
    var onMoveEnded: (() -> Void)? = nil

    private let accent = Color(red: 0.18, green: 0.48, blue: 0.95)
    private let widgetWidth: CGFloat = 280
    private let rowHeight: CGFloat = 52
    private let rowSpacing: CGFloat = 4
    private let listBottomPad: CGFloat = 10
    private let headerHeight: CGFloat = 56
    /// Long edge of layer thumb (short edge follows document aspect).
    private let thumbMaxSide: CGFloat = 36

    var documentWidth: Int32 = 1
    var documentHeight: Int32 = 1

    @State private var draggingId: Int32?
    @State private var dragOriginIndex: Int = 0
    /// Finger travel from press — list order stays fixed so this matches the cursor 1:1.
    @State private var dragTranslation: CGFloat = 0
    /// Optimistic order after drop until parent catches up.
    @State private var localPaintOrder: [LayerListItem]?

    private var paintLayers: [LayerListItem] {
        localPaintOrder ?? layers.filter { !$0.isBackground }
    }

    private var backgroundLayer: LayerListItem? {
        layers.first(where: { $0.isBackground })
    }

    private var rowCount: CGFloat {
        CGFloat(paintLayers.count + (backgroundLayer == nil ? 0 : 1))
    }

    private var strideY: CGFloat {
        rowHeight + rowSpacing
    }

    private var maxWidgetHeight: CGFloat {
        screenHeight * 0.65
    }

    private var screenHeight: CGFloat {
#if os(macOS)
        NSScreen.main?.visibleFrame.height ?? 800
#else
        UIScreen.main.bounds.height
#endif
    }

    private var listMaxHeight: CGFloat {
        max(0, maxWidgetHeight - headerHeight)
    }

    private var idealListHeight: CGFloat {
        guard rowCount > 0 else { return 0 }
        return rowCount * rowHeight + max(0, rowCount - 1) * rowSpacing + listBottomPad
    }

    private var listHeight: CGFloat {
        min(idealListHeight, listMaxHeight)
    }

    private var needsScroll: Bool {
        idealListHeight > listMaxHeight
    }

    private var thumbSize: CGSize {
        let w = max(1, CGFloat(documentWidth))
        let h = max(1, CGFloat(documentHeight))
        let aspect = w / h
        if aspect >= 1 {
            return CGSize(width: thumbMaxSide, height: max(1, (thumbMaxSide / aspect).rounded()))
        }
        return CGSize(width: max(1, (thumbMaxSide * aspect).rounded()), height: thumbMaxSide)
    }

    private var dropIndex: Int {
        min(max(0, dragOriginIndex + Int((dragTranslation / strideY).rounded())), max(0, paintLayers.count - 1))
    }

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
                    DragGesture(coordinateSpace: .global)
                        .onChanged { onMoveChanged?($0) }
                        .onEnded { _ in onMoveEnded?() }
                )
                .accessibilityLabel("Move Layers")

            HStack {
                Text("Layers")
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundStyle(.white)
                Spacer(minLength: 0)
                Button(action: onAdd) {
                    Image(systemName: "plus")
                        .font(.system(size: 16, weight: .semibold))
                        .foregroundStyle(.white)
                        .frame(width: 28, height: 28)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Add layer")
                Button(action: onClose) {
                    Image(systemName: "xmark")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(.white.opacity(0.85))
                        .frame(width: 28, height: 28)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Close Layers")
            }
            .padding(.horizontal, 14)
            .padding(.bottom, 8)

            if needsScroll {
                ScrollViewReader { proxy in
                    ScrollView {
                        layerStack
                    }
                    .onChange(of: layers.count) { oldCount, newCount in
                        guard newCount > oldCount, let topId = paintLayers.first?.id else { return }
                        withAnimation(.easeOut(duration: 0.2)) {
                            proxy.scrollTo(topId, anchor: .top)
                        }
                    }
                }
                .frame(height: listHeight, alignment: .top)
            } else {
                layerStack
            }
        }
        .frame(width: widgetWidth)
        .fixedSize(horizontal: false, vertical: true)
        .background {
            ZStack {
                Rectangle().fill(.ultraThinMaterial)
                Color.black.opacity(0.8)
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
        .onChange(of: layers) { _, newLayers in
            guard draggingId == nil else { return }
            let parentIds = newLayers.filter { !$0.isBackground }.map(\.id)
            if localPaintOrder?.map(\.id) == parentIds {
                localPaintOrder = nil
            }
        }
    }

    private var layerStack: some View {
        VStack(spacing: rowSpacing) {
            ForEach(Array(paintLayers.enumerated()), id: \.element.id) { index, layer in
                layerRow(layer, showMoveHandle: true, paintIndex: index)
                    .id(layer.id)
                    .offset(y: rowOffset(for: layer.id, at: index))
                    .zIndex(draggingId == layer.id ? 1 : 0)
                    .animation(.easeOut(duration: 0.12), value: dropIndex)
            }

            if let background = backgroundLayer {
                layerRow(background, showMoveHandle: false, paintIndex: nil)
                    .id(background.id)
            }
        }
        .padding(.horizontal, 10)
        .padding(.bottom, listBottomPad)
    }

    /// Dragged row follows finger; others slide to open a gap at `dropIndex`.
    private func rowOffset(for id: Int32, at index: Int) -> CGFloat {
        guard let draggingId else { return 0 }
        if id == draggingId {
            return dragTranslation
        }
        let from = dragOriginIndex
        let to = dropIndex
        if from < to, index > from, index <= to { return -strideY }
        if from > to, index >= to, index < from { return strideY }
        return 0
    }

    private func endDrag() {
        let from = dragOriginIndex
        let to = dropIndex

        var next = paintLayers
        var t = Transaction()
        t.disablesAnimations = true
        withTransaction(t) {
            draggingId = nil
            dragTranslation = 0
            if to != from, from < next.count {
                let item = next.remove(at: from)
                next.insert(item, at: to)
                localPaintOrder = next
            }
        }

        if to != from {
            onReorder(IndexSet(integer: from), to > from ? to + 1 : to)
        }
    }

    private func layerRow(_ layer: LayerListItem, showMoveHandle: Bool, paintIndex: Int?) -> some View {
        let selected = layer.id == activeLayerId
        let dim = layer.visible ? 1.0 : 0.4
        let isDragging = draggingId == layer.id
        return HStack(spacing: 8) {
            if showMoveHandle, let paintIndex {
                Image(systemName: "line.3.horizontal")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(Color.white.opacity(0.45))
                    .frame(width: 22, height: 36)
                    .contentShape(Rectangle())
                    .gesture(
                        // Global: translation stays cursor-true even if layout shifts.
                        DragGesture(minimumDistance: 4, coordinateSpace: .global)
                            .onChanged { value in
                                if draggingId == nil {
                                    draggingId = layer.id
                                    dragOriginIndex = paintIndex
                                    dragTranslation = 0
                                }
                                dragTranslation = value.translation.height
                            }
                            .onEnded { _ in
                                endDrag()
                            }
                    )
                    .accessibilityLabel("Reorder layer")
            } else {
                Color.clear.frame(width: 22)
            }

            HStack(spacing: 8) {
                layerThumbnail(layer)
                    .frame(width: thumbSize.width, height: thumbSize.height)
                    .clipShape(RoundedRectangle(cornerRadius: 4, style: .continuous))
                    .overlay(
                        RoundedRectangle(cornerRadius: 4, style: .continuous)
                            .stroke(selected ? accent : Color.white.opacity(0.2), lineWidth: selected ? 2 : 1)
                    )

                Text(layer.name)
                    .font(.system(size: 14, weight: .medium))
                    .foregroundStyle(.white)
                    .lineLimit(1)

                if !layer.isBackground {
                    Text("N")
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundStyle(.white.opacity(0.55))
                }

                Spacer(minLength: 0)
            }
            .contentShape(Rectangle())
            .onTapGesture { onSelect(layer.id) }

            Button {
                onToggleVisible(layer.id)
            } label: {
                Image(systemName: layer.visible ? "checkmark.square.fill" : "square")
                    .font(.system(size: 16))
                    .foregroundStyle(layer.visible ? Color.white : Color.white.opacity(0.45))
                    .frame(width: 28, height: 28)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel(layer.visible ? "Hide layer" : "Show layer")
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 8)
        .frame(height: rowHeight)
        .opacity(dim)
        .background(selected ? accent.opacity(layer.visible ? 1 : 0.55) : Color.white.opacity(0.06))
        .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
        .shadow(color: isDragging ? .black.opacity(0.4) : .clear, radius: isDragging ? 10 : 0, y: 3)
        .scaleEffect(isDragging ? 1.02 : 1)
        .contextMenu {
            if !layer.isBackground {
                layerContextMenu(layer)
            }
        }
    }

    @ViewBuilder
    private func layerThumbnail(_ layer: LayerListItem) -> some View {
        ZStack {
            // Checker so transparent paint layers read clearly.
            CheckerboardBackground()
            if let thumbnail = layer.thumbnail {
                thumbnail
                    .resizable()
                    .interpolation(.none)
                    .scaledToFill()
            } else if layer.isBackground {
                Color.white
            }
        }
    }

    @ViewBuilder
    private func layerContextMenu(_ layer: LayerListItem) -> some View {
        // Mac: right-click · iPad: long-press (SwiftUI contextMenu)
        Button("Rename") {}
        Button("Select") { onSelect(layer.id) }
        Button("Copy") { onDuplicate(layer.id) }
        Button("Fill Layer") {}
        Button("Clear") { onClear(layer.id) }
        Divider()
        Button("Alpha Lock") {}
        Button("Mask") {}
        Button("Invert") {}
        Button("Reference") {}
        Divider()
        Button("Merge Down") { onMergeDown(layer.id) }
        Button("Combine Down") { onMergeDown(layer.id) }
    }
}

/// Tiny checker for transparent layer thumbs.
private struct CheckerboardBackground: View {
    var body: some View {
        Canvas { ctx, size in
            let cell: CGFloat = 4
            for y in stride(from: 0, to: size.height, by: cell) {
                for x in stride(from: 0, to: size.width, by: cell) {
                    let odd = (Int(x / cell) + Int(y / cell)) % 2 == 1
                    let rect = CGRect(x: x, y: y, width: cell, height: cell)
                    ctx.fill(Path(rect), with: .color(odd ? Color.white.opacity(0.22) : Color.white.opacity(0.08)))
                }
            }
        }
    }
}

#Preview {
    LayersWidgetView(
        layers: [
            .init(id: 2, name: "Layer 2", visible: true, isBackground: false, contentRevision: 0, thumbnail: nil),
            .init(id: 1, name: "Layer 1", visible: false, isBackground: false, contentRevision: 0, thumbnail: nil),
            .init(id: 0, name: "Background Layer", visible: true, isBackground: true, contentRevision: 0, thumbnail: nil),
        ],
        activeLayerId: 2,
        onSelect: { _ in },
        onToggleVisible: { _ in },
        documentWidth: 400,
        documentHeight: 600
    )
    .padding()
    .background(Color.gray)
}
