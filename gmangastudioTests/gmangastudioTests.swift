//
//  gmangastudioTests.swift
//  gmangastudioTests
//

import XCTest
@testable import gmangastudio

final class gmangastudioTests: XCTestCase {
    func testAppActiveIdleTracker() throws {
        XCTAssertTrue(AppActiveIdleTrackerSelfCheck.run())
    }
}
