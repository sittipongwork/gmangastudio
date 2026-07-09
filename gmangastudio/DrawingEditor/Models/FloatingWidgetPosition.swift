//
//  FloatingWidgetPosition.swift
//  gmangastudio
//

import CoreGraphics
import Foundation

/// Persists floating widget origins in UserDefaults.
enum FloatingWidgetPositionStore {
    static func load(key: String, defaultOrigin: CGPoint) -> CGPoint {
        guard let raw = UserDefaults.standard.string(forKey: key) else { return defaultOrigin }
        let parts = raw.split(separator: ",")
        guard parts.count == 2,
              let x = Double(parts[0]),
              let y = Double(parts[1]) else { return defaultOrigin }
        return CGPoint(x: x, y: y)
    }

    static func save(_ origin: CGPoint, key: String) {
        UserDefaults.standard.set("\(origin.x),\(origin.y)", forKey: key)
    }

    static func seedIfNeeded(key: String, defaultOrigin: CGPoint) -> CGPoint {
        let seededKey = key + ".seeded"
        if !UserDefaults.standard.bool(forKey: seededKey) {
            save(defaultOrigin, key: key)
            UserDefaults.standard.set(true, forKey: seededKey)
            return defaultOrigin
        }
        return load(key: key, defaultOrigin: defaultOrigin)
    }
}

enum BrushLibraryWidgetPositionStore {
    static let key = "drawingEditor.brushLibraryWidget.origin"
    static let defaultOrigin = CGPoint(x: 68, y: 48)

    static func load() -> CGPoint {
        FloatingWidgetPositionStore.load(key: key, defaultOrigin: defaultOrigin)
    }

    static func save(_ origin: CGPoint) {
        FloatingWidgetPositionStore.save(origin, key: key)
    }
}

enum LayersWidgetPositionStore {
    static let key = "drawingEditor.layersWidget.origin"
    static let defaultOrigin = CGPoint(x: 68, y: 48)

    static func load() -> CGPoint {
        FloatingWidgetPositionStore.load(key: key, defaultOrigin: defaultOrigin)
    }

    static func save(_ origin: CGPoint) {
        FloatingWidgetPositionStore.save(origin, key: key)
    }
}

enum FloatingWidgetPositionSelfCheck {
    static func run() -> Bool {
        let key = "drawingEditor.widgetPosition.selfCheck"
        let defaults = UserDefaults.standard
        let previous = defaults.string(forKey: key)
        defer {
            if let previous {
                defaults.set(previous, forKey: key)
            } else {
                defaults.removeObject(forKey: key)
            }
        }
        FloatingWidgetPositionStore.save(CGPoint(x: 120, y: 80), key: key)
        let loaded = FloatingWidgetPositionStore.load(key: key, defaultOrigin: .zero)
        return abs(loaded.x - 120) < 0.01 && abs(loaded.y - 80) < 0.01
    }
}
