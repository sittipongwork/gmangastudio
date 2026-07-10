//
//  ColorDiscView.swift
//  gmangastudio — Procreate-style HSV color disc (hue × saturation) + value slider
//

import SwiftUI

#if os(macOS)
import AppKit
#else
import UIKit
#endif

/// Hue (angle) × saturation (radius). Value is controlled separately.
struct ColorDiscView: View {
    @Binding var hue: Double
    @Binding var saturation: Double
    var value: Double
    var diameter: CGFloat = 220
    var onEditingChanged: (Bool) -> Void = { _ in }

    var body: some View {
        GeometryReader { geo in
            let size = min(geo.size.width, geo.size.height)
            let radius = size * 0.5
            ZStack {
                // Hue 0 (red) at top, clockwise — must match `handle` / `update` angle math.
                // Default AngularGradient starts at east (3 o'clock); that was the mismatch.
                Circle()
                    .fill(
                        AngularGradient(
                            gradient: Gradient(colors: Self.hueColors),
                            center: .center,
                            startAngle: .degrees(-90),
                            endAngle: .degrees(270)
                        )
                    )
                Circle()
                    .fill(
                        RadialGradient(
                            colors: [
                                Color.white,
                                Color.white.opacity(0),
                            ],
                            center: .center,
                            startRadius: 0,
                            endRadius: radius
                        )
                    )
                // Dim disc when value < 1 so the handle matches the picked color.
                Circle()
                    .fill(Color.black.opacity(1 - value))
                    .blendMode(.multiply)
                    .allowsHitTesting(false)

                handle(in: size)
            }
            .frame(width: size, height: size)
            .contentShape(Circle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        onEditingChanged(true)
                        update(from: drag.location, size: size)
                    }
                    .onEnded { _ in
                        onEditingChanged(false)
                    }
            )
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .frame(width: diameter, height: diameter)
        .accessibilityLabel("Color disc")
    }

    private func handle(in size: CGFloat) -> some View {
        let radius = size * 0.5
        let r = saturation * (radius - 10)
        let angle = hue * 2 * Double.pi - Double.pi / 2
        let x = cos(angle) * r
        let y = sin(angle) * r
        return Circle()
            .fill(Color(hue: hue, saturation: saturation, brightness: value))
            .frame(width: 28, height: 28)
            .overlay(Circle().strokeBorder(Color.white, lineWidth: 3))
            .shadow(color: .black.opacity(0.35), radius: 2, y: 1)
            .offset(x: x, y: y)
            .allowsHitTesting(false)
    }

    private func update(from location: CGPoint, size: CGFloat) {
        let mid = size * 0.5
        let dx = Double(location.x - mid)
        let dy = Double(location.y - mid)
        let dist = sqrt(dx * dx + dy * dy)
        let maxR = Double(mid - 10)
        saturation = min(max(dist / max(maxR, 1), 0), 1)
        // 0 at top, clockwise — matches AngularGradient.
        var angle = atan2(dy, dx) + Double.pi / 2
        if angle < 0 { angle += 2 * Double.pi }
        hue = angle / (2 * Double.pi)
    }

    private static let hueColors: [Color] = (0..<12).map { i in
        Color(hue: Double(i) / 12, saturation: 1, brightness: 1)
    } + [Color(hue: 0, saturation: 1, brightness: 1)]
}

struct ColorValueSlider: View {
    @Binding var value: Double
    var hue: Double
    var saturation: Double
    var onEditingChanged: (Bool) -> Void = { _ in }

    var body: some View {
        GeometryReader { geo in
            let w = geo.size.width
            let h = geo.size.height
            ZStack(alignment: .leading) {
                Capsule()
                    .fill(
                        LinearGradient(
                            colors: [
                                .black,
                                Color(hue: hue, saturation: saturation, brightness: 1),
                            ],
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                    )
                Circle()
                    .fill(Color(hue: hue, saturation: saturation, brightness: value))
                    .overlay(Circle().strokeBorder(Color.white, lineWidth: 2))
                    .frame(width: h, height: h)
                    .offset(x: CGFloat(value) * max(0, w - h))
            }
            .contentShape(Capsule())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        onEditingChanged(true)
                        let t = drag.location.x / max(w, 1)
                        value = min(max(Double(t), 0), 1)
                    }
                    .onEnded { _ in
                        onEditingChanged(false)
                    }
            )
        }
        .frame(height: 18)
        .accessibilityLabel("Brightness")
    }
}

enum ColorModel: String, CaseIterable, Identifiable {
    case hsl = "HSL"
    case rgba = "RGBA"
    case hsv = "HSV"
    case hex = "HEX"

    var id: String { rawValue }
}

enum ColorHSV {
    static func components(from color: Color) -> (h: Double, s: Double, v: Double, a: Double) {
        let rgba = rgbaComponents(from: color)
        let hsv = rgbToHsv(r: rgba.r, g: rgba.g, b: rgba.b)
        return (hsv.h, hsv.s, hsv.v, rgba.a)
    }

    static func rgbaComponents(from color: Color) -> (r: Double, g: Double, b: Double, a: Double) {
        #if os(macOS)
        let ns = NSColor(color)
        guard let rgb = ns.usingColorSpace(.deviceRGB) ?? ns.usingColorSpace(.sRGB) else {
            return (0.08, 0.08, 0.08, 1)
        }
        return (
            Double(rgb.redComponent),
            Double(rgb.greenComponent),
            Double(rgb.blueComponent),
            Double(rgb.alphaComponent)
        )
        #else
        let ui = UIColor(color)
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        ui.getRed(&r, green: &g, blue: &b, alpha: &a)
        return (Double(r), Double(g), Double(b), Double(a))
        #endif
    }

    static func color(h: Double, s: Double, v: Double, a: Double = 1) -> Color {
        Color(hue: h, saturation: s, brightness: v, opacity: a)
    }

    static func color(r: Double, g: Double, b: Double, a: Double = 1) -> Color {
        Color(.sRGB, red: r, green: g, blue: b, opacity: a)
    }

    /// HSL in 0…1. Convert via RGB.
    static func hslComponents(from color: Color) -> (h: Double, s: Double, l: Double, a: Double) {
        let rgba = rgbaComponents(from: color)
        let hsl = rgbToHsl(r: rgba.r, g: rgba.g, b: rgba.b)
        return (hsl.h, hsl.s, hsl.l, rgba.a)
    }

    static func colorFromHsl(h: Double, s: Double, l: Double, a: Double = 1) -> Color {
        let rgb = hslToRgb(h: h, s: s, l: l)
        return color(r: rgb.r, g: rgb.g, b: rgb.b, a: a)
    }

    static func hexString(from color: Color) -> String {
        let c = rgbaComponents(from: color)
        let r = Int((c.r * 255).rounded())
        let g = Int((c.g * 255).rounded())
        let b = Int((c.b * 255).rounded())
        return String(format: "#%02X%02X%02X", r, g, b)
    }

    static func color(fromHex hex: String) -> Color? {
        var s = hex.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
        if s.hasPrefix("#") { s.removeFirst() }
        guard s.count == 6 || s.count == 8, let v = UInt64(s, radix: 16) else { return nil }
        let hasAlpha = s.count == 8
        let r: Double
        let g: Double
        let b: Double
        let a: Double
        if hasAlpha {
            r = Double((v >> 24) & 0xFF) / 255
            g = Double((v >> 16) & 0xFF) / 255
            b = Double((v >> 8) & 0xFF) / 255
            a = Double(v & 0xFF) / 255
        } else {
            r = Double((v >> 16) & 0xFF) / 255
            g = Double((v >> 8) & 0xFF) / 255
            b = Double(v & 0xFF) / 255
            a = 1
        }
        return color(r: r, g: g, b: b, a: a)
    }

    static func rgbToHsv(r: Double, g: Double, b: Double) -> (h: Double, s: Double, v: Double) {
        let maxC = max(r, g, b)
        let minC = min(r, g, b)
        let d = maxC - minC
        var h: Double = 0
        if d > 0.00001 {
            if maxC == r {
                h = ((g - b) / d).truncatingRemainder(dividingBy: 6)
            } else if maxC == g {
                h = (b - r) / d + 2
            } else {
                h = (r - g) / d + 4
            }
            h /= 6
            if h < 0 { h += 1 }
        }
        let s = maxC <= 0 ? 0 : d / maxC
        return (h, s, maxC)
    }

    static func rgbToHsl(r: Double, g: Double, b: Double) -> (h: Double, s: Double, l: Double) {
        let maxC = max(r, g, b)
        let minC = min(r, g, b)
        let l = (maxC + minC) * 0.5
        let d = maxC - minC
        guard d > 0.00001 else { return (0, 0, l) }
        let s = l > 0.5 ? d / (2 - maxC - minC) : d / (maxC + minC)
        var h: Double
        if maxC == r {
            h = ((g - b) / d).truncatingRemainder(dividingBy: 6)
        } else if maxC == g {
            h = (b - r) / d + 2
        } else {
            h = (r - g) / d + 4
        }
        h /= 6
        if h < 0 { h += 1 }
        return (h, s, l)
    }

    static func hslToRgb(h: Double, s: Double, l: Double) -> (r: Double, g: Double, b: Double) {
        if s <= 0.00001 { return (l, l, l) }
        func hue2rgb(_ p: Double, _ q: Double, _ tIn: Double) -> Double {
            var t = tIn
            if t < 0 { t += 1 }
            if t > 1 { t -= 1 }
            if t < 1.0 / 6 { return p + (q - p) * 6 * t }
            if t < 0.5 { return q }
            if t < 2.0 / 3 { return p + (q - p) * (2.0 / 3 - t) * 6 }
            return p
        }
        let q = l < 0.5 ? l * (1 + s) : l + s - l * s
        let p = 2 * l - q
        return (
            hue2rgb(p, q, h + 1.0 / 3),
            hue2rgb(p, q, h),
            hue2rgb(p, q, h - 1.0 / 3)
        )
    }
}
