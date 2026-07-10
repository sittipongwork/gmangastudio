//
//  BrushColorPickerView.swift
//  gmangastudio — Colours widget with inline disc picker + history (Procreate-inspired)
//

import SwiftUI

struct BrushColorPickerView: View {
    @Binding var color: Color
    var history: [Color]
    var onSelectHistory: (Color) -> Void
    var onClearHistory: () -> Void
    /// Called when the user finishes a disc/slider edit (record history).
    var onColorCommitted: (Color) -> Void

    @State private var previousColor: Color = .black
    @State private var hue: Double = 0
    @State private var saturation: Double = 0
    @State private var value: Double = 0.08
    @State private var alpha: Double = 1
    @State private var isEditing = false
    @State private var colorModel: ColorModel = .hsl

    // Display fields (synced from color / written back on edit).
    @State private var fieldH: String = "0"
    @State private var fieldS: String = "0"
    @State private var fieldL: String = "0"
    @State private var fieldV: String = "0"
    @State private var fieldR: String = "0"
    @State private var fieldG: String = "0"
    @State private var fieldB: String = "0"
    @State private var fieldA: String = "255"
    @State private var fieldHex: String = "#000000"

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .top) {
                Text("Colours")
                    .font(.headline)
                Spacer()
                ZStack(alignment: .topTrailing) {
                    RoundedRectangle(cornerRadius: 6, style: .continuous)
                        .fill(previousColor)
                        .frame(width: 28, height: 28)
                        .offset(x: 10, y: 8)
                        .onTapGesture {
                            let swap = color
                            applyExternalColor(previousColor)
                            previousColor = swap
                            pushLive()
                            onColorCommitted(color)
                        }
                    RoundedRectangle(cornerRadius: 6, style: .continuous)
                        .fill(color)
                        .frame(width: 36, height: 36)
                        .overlay(
                            RoundedRectangle(cornerRadius: 6, style: .continuous)
                                .strokeBorder(Color.white.opacity(0.35), lineWidth: 1)
                        )
                }
                .frame(width: 48, height: 44)
            }

            ColorDiscView(
                hue: $hue,
                saturation: $saturation,
                value: value,
                onEditingChanged: { editing in
                    isEditing = editing
                    if !editing { onColorCommitted(currentColor) }
                }
            )
            .frame(maxWidth: .infinity)
            .onChange(of: hue) { _, _ in pushLive() }
            .onChange(of: saturation) { _, _ in pushLive() }

            ColorValueSlider(
                value: $value,
                hue: hue,
                saturation: saturation,
                onEditingChanged: { editing in
                    isEditing = editing
                    if !editing { onColorCommitted(currentColor) }
                }
            )
            .onChange(of: value) { _, _ in pushLive() }

            colorModelSection

            HStack {
                Text("History")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(.secondary)
                Spacer()
                Button("Clear") { onClearHistory() }
                    .font(.subheadline)
                    .disabled(history.isEmpty)
            }

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    ForEach(Array(history.enumerated()), id: \.offset) { _, swatch in
                        Button {
                            previousColor = color
                            applyExternalColor(swatch)
                            onSelectHistory(swatch)
                        } label: {
                            RoundedRectangle(cornerRadius: 6, style: .continuous)
                                .fill(swatch)
                                .frame(width: 28, height: 28)
                                .overlay(
                                    RoundedRectangle(cornerRadius: 6, style: .continuous)
                                        .strokeBorder(Color.primary.opacity(0.15), lineWidth: 1)
                                )
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
            .frame(height: 28)
        }
        .padding(14)
        .frame(width: 280)
        .onAppear {
            previousColor = color
            applyExternalColor(color)
        }
        .onChange(of: color) { _, newValue in
            guard !isEditing else { return }
            applyExternalColor(newValue)
        }
        .onChange(of: colorModel) { _, _ in
            syncFieldsFromColor()
        }
    }

    private var colorModelSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Model")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(.secondary)
                Spacer()
                Picker("Color Model", selection: $colorModel) {
                    ForEach(ColorModel.allCases) { model in
                        Text(model.rawValue).tag(model)
                    }
                }
                .pickerStyle(.menu)
                .labelsHidden()
                .frame(minWidth: 88, alignment: .trailing)
            }

            switch colorModel {
            case .hsl:
                HStack(spacing: 6) {
                    modelField("H", text: $fieldH, suffix: "°")
                    modelField("S", text: $fieldS, suffix: "%")
                    modelField("L", text: $fieldL, suffix: "%")
                }
            case .rgba:
                HStack(spacing: 6) {
                    modelField("R", text: $fieldR)
                    modelField("G", text: $fieldG)
                    modelField("B", text: $fieldB)
                    modelField("A", text: $fieldA)
                }
            case .hsv:
                HStack(spacing: 6) {
                    modelField("H", text: $fieldH, suffix: "°")
                    modelField("S", text: $fieldS, suffix: "%")
                    modelField("V", text: $fieldV, suffix: "%")
                }
            case .hex:
                HStack(spacing: 6) {
                    Text("HEX")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                        .frame(width: 32, alignment: .leading)
                    TextField("", text: $fieldHex)
                        .textFieldStyle(.roundedBorder)
                        .font(.system(.body, design: .monospaced))
                        .onSubmit { applyFields() }
                    Button("Set") { applyFields() }
                        .buttonStyle(.bordered)
                }
            }
        }
    }

    private func modelField(_ label: String, text: Binding<String>, suffix: String = "") -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.secondary)
            HStack(spacing: 2) {
                TextField("", text: text)
                    .textFieldStyle(.roundedBorder)
                    .font(.system(.caption, design: .monospaced))
                    .onSubmit { applyFields() }
                if !suffix.isEmpty {
                    Text(suffix)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .frame(maxWidth: .infinity)
    }

    private var currentColor: Color {
        ColorHSV.color(h: hue, s: saturation, v: value, a: alpha)
    }

    private func pushLive() {
        color = currentColor
        syncFieldsFromColor()
    }

    private func applyExternalColor(_ c: Color) {
        let hsv = ColorHSV.components(from: c)
        hue = hsv.h
        saturation = hsv.s
        value = hsv.v
        alpha = hsv.a
        color = c
        syncFieldsFromColor()
    }

    private func syncFieldsFromColor() {
        switch colorModel {
        case .hsl:
            let hsl = ColorHSV.hslComponents(from: color)
            fieldH = "\(Int((hsl.h * 360).rounded()))"
            fieldS = "\(Int((hsl.s * 100).rounded()))"
            fieldL = "\(Int((hsl.l * 100).rounded()))"
        case .rgba:
            let rgba = ColorHSV.rgbaComponents(from: color)
            fieldR = "\(Int((rgba.r * 255).rounded()))"
            fieldG = "\(Int((rgba.g * 255).rounded()))"
            fieldB = "\(Int((rgba.b * 255).rounded()))"
            fieldA = "\(Int((rgba.a * 255).rounded()))"
        case .hsv:
            fieldH = "\(Int((hue * 360).rounded()))"
            fieldS = "\(Int((saturation * 100).rounded()))"
            fieldV = "\(Int((value * 100).rounded()))"
        case .hex:
            fieldHex = ColorHSV.hexString(from: color)
        }
    }

    private func applyFields() {
        switch colorModel {
        case .hsl:
            guard let h = Double(fieldH), let s = Double(fieldS), let l = Double(fieldL) else { return }
            let next = ColorHSV.colorFromHsl(
                h: clamp(h / 360, 0, 1),
                s: clamp(s / 100, 0, 1),
                l: clamp(l / 100, 0, 1),
                a: alpha
            )
            applyExternalColor(next)
            onColorCommitted(next)
        case .rgba:
            guard let r = Double(fieldR), let g = Double(fieldG),
                  let b = Double(fieldB), let a = Double(fieldA) else { return }
            let next = ColorHSV.color(
                r: clamp(r / 255, 0, 1),
                g: clamp(g / 255, 0, 1),
                b: clamp(b / 255, 0, 1),
                a: clamp(a / 255, 0, 1)
            )
            applyExternalColor(next)
            onColorCommitted(next)
        case .hsv:
            guard let h = Double(fieldH), let s = Double(fieldS), let v = Double(fieldV) else { return }
            hue = clamp(h / 360, 0, 1)
            saturation = clamp(s / 100, 0, 1)
            value = clamp(v / 100, 0, 1)
            pushLive()
            onColorCommitted(currentColor)
        case .hex:
            guard let next = ColorHSV.color(fromHex: fieldHex) else { return }
            applyExternalColor(next)
            onColorCommitted(next)
        }
    }

    private func clamp(_ x: Double, _ lo: Double, _ hi: Double) -> Double {
        min(max(x, lo), hi)
    }
}
