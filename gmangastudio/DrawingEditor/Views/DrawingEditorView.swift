//
//  DrawingEditorView.swift
//  gmangastudio
//

import SwiftUI

struct DrawingEditorView: View {
    var body: some View {
        ZStack {
            Color(white: 0.92)
                .ignoresSafeArea()
            Text("Drawing Editor")
                .font(.title2)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

#Preview {
    DrawingEditorView()
}
