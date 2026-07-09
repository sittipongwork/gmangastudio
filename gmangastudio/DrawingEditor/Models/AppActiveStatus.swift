//
//  AppActiveStatus.swift
//  gmangastudio
//

import Foundation

enum AppActiveStatus: Equatable {
    case performanceMode
    case idleMode
}

/// Idle gate for 120fps present. Spec: canvas_document.md § App active status.
struct AppActiveIdleTracker {
    static let idleTimeout: TimeInterval = 30
    /// Present rate while in `idleMode` (MTKView preferredFramesPerSecond).
    static let idlePresentFps = 5

    private(set) var status: AppActiveStatus = .performanceMode
    private(set) var lastUse: Date

    init(now: Date = Date()) {
        lastUse = now
        status = .performanceMode
    }

    mutating func noteActivity(at now: Date = Date()) {
        lastUse = now
        status = .performanceMode
    }

    /// Apply idle timeout. Returns true if status changed.
    @discardableResult
    mutating func applyIdle(at now: Date = Date()) -> Bool {
        guard status == .performanceMode else { return false }
        guard now.timeIntervalSince(lastUse) >= Self.idleTimeout else { return false }
        status = .idleMode
        return true
    }

    /// Seconds until idle timeout from `lastUse`, or 0 if already idle_mode / overdue.
    func secondsUntilIdle(at now: Date = Date()) -> TimeInterval {
        guard status == .performanceMode else { return 0 }
        return max(0, Self.idleTimeout - now.timeIntervalSince(lastUse))
    }
}

enum AppActiveIdleTrackerSelfCheck {
    static func run() -> Bool {
        var t = AppActiveIdleTracker(now: Date(timeIntervalSince1970: 0))
        if t.status != .performanceMode { return false }
        t.applyIdle(at: Date(timeIntervalSince1970: 29))
        if t.status != .performanceMode { return false }
        t.applyIdle(at: Date(timeIntervalSince1970: 30))
        if t.status != .idleMode { return false }
        t.noteActivity(at: Date(timeIntervalSince1970: 31))
        if t.status != .performanceMode { return false }
        t.applyIdle(at: Date(timeIntervalSince1970: 60))
        if t.status != .performanceMode { return false }
        t.applyIdle(at: Date(timeIntervalSince1970: 61))
        return t.status == .idleMode
    }
}
