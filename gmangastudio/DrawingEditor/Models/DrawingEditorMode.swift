//
//  DrawingEditorMode.swift
//  gmangastudio
//

enum DrawingEditorMode: Equatable {
    case pointer
    case brushLibrary
    case eraser
}

/// Submodes while `DrawingEditorMode.brushLibrary` (brush tool) is active.
enum BrushSubmode: Equatable {
    /// Default when brush button is clicked — paint strokes.
    case brush
    /// Sample canvas color into brush color, then return to `.brush`.
    case eyedropper
}
