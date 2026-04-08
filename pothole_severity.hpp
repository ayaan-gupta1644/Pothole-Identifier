#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace cv {

// ─────────────────────────────────────────────
//  Severity levels for detected potholes
// ─────────────────────────────────────────────
enum PotholeSeverity {
    POTHOLE_NONE     = 0,
    POTHOLE_MINOR    = 1,   // small crack / surface damage  (<500 px area)
    POTHOLE_MODERATE = 2,   // medium pothole               (500–2000 px area)
    POTHOLE_SEVERE   = 3    // large / deep pothole          (>2000 px area)
};

// ─────────────────────────────────────────────
//  A single detected pothole and its metadata
// ─────────────────────────────────────────────
struct PotholeRegion {
    Rect           boundingBox;    // location in frame
    double         area;           // pixel area of the contour
    double         aspectRatio;    // width/height (helps filter false positives)
    double         darkness;       // mean pixel darkness in ROI (proxy for depth)
    PotholeSeverity severity;      // classified severity
    Point2f        center;         // centroid of contour
    std::string    label;          // human-readable label string
};

// ─────────────────────────────────────────────
//  Extended connected-components result
// ─────────────────────────────────────────────
struct PotholeStats {
    int    totalDetected;
    int    minorCount;
    int    moderateCount;
    int    severeCount;
    double overallRoadHealth;   // 0.0 (destroyed) – 1.0 (perfect)
};

// ─────────────────────────────────────────────
//  Core API  (the "modified OpenCV function")
// ─────────────────────────────────────────────

/**
 * detectPotholeRegions()
 *
 * Extends OpenCV's connectedComponentsWithStats pipeline with:
 *   • Adaptive shadow-based depth estimation
 *   • Morphological noise filtering
 *   • Per-region severity scoring
 *
 * @param frame       Input BGR video frame
 * @param regions     Output: vector of detected PotholeRegions
 * @param stats       Output: aggregate road health statistics
 * @param debugFrame  Output: annotated frame for display (pass empty Mat to skip)
 * @param minArea     Minimum contour area to consider (default 300 px²)
 */
void detectPotholeRegions(
    const Mat&               frame,
    std::vector<PotholeRegion>& regions,
    PotholeStats&            stats,
    Mat&                     debugFrame,
    double                   minArea = 300.0
);

/**
 * classifySeverity()
 *
 * Pure severity classifier – call this if you already have contour data.
 * Uses area + darkness score to return a PotholeSeverity enum value.
 */
PotholeSeverity classifySeverity(double area, double darkness);

/**
 * drawPotholeOverlay()
 *
 * Draws annotated bounding boxes, severity labels, and a road health
 * score bar onto the given frame (in-place).
 */
void drawPotholeOverlay(Mat& frame,
                        const std::vector<PotholeRegion>& regions,
                        const PotholeStats& stats);

} // namespace cv
