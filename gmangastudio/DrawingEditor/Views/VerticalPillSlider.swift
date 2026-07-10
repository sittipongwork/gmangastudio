//
//  VerticalPillSlider.swift
//  gmangastudio — Procreate-style vertical pill slider (size / opacity)
//

import SwiftUI

struct VerticalPillSlider: View {
    @Binding var value: Double
    var range: ClosedRange<Double>
    var accessibilityLabel: String

    private let thumbLength: CGFloat = 28

    var body: some View {
        GeometryReader { geo in
            let trackW = geo.size.width
            let trackH = geo.size.height
            let travel = max(0, trackH - thumbLength)
            let t = CGFloat((value - range.lowerBound) / (range.upperBound - range.lowerBound))
            // High value at top (Procreate size feel).
            let y = (1 - t.clamped(to: 0...1)) * travel

            ZStack(alignment: .top) {
                Capsule()
                    .fill(Color(white: 0.12))
                Capsule()
                    .fill(Color(white: 0.55))
                    .frame(width: max(4, trackW - 6), height: thumbLength)
                    .offset(y: y)
            }
            .frame(width: trackW, height: trackH)
            .contentShape(Capsule())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        let clampedY = min(max(0, drag.location.y - thumbLength * 0.5), travel)
                        let nextT = 1 - (clampedY / max(travel, 1))
                        let next = range.lowerBound + Double(nextT) * (range.upperBound - range.lowerBound)
                        value = min(max(next, range.lowerBound), range.upperBound)
                    }
            )
        }
        .accessibilityLabel(accessibilityLabel)
        .accessibilityValue(Text("\(Int((value * (range.upperBound <= 1 ? 100 : 1)).rounded()))"))
    }
}

private extension Comparable {
    func clamped(to range: ClosedRange<Self>) -> Self {
        min(max(self, range.lowerBound), range.upperBound)
    }
}
