/**
 * pothole_severity.cpp
 *
 * Modified OpenCV imgproc module  —  Pothole Severity Classifier
 *
 * HOW THIS EXTENDS OPENCV:
 *   Standard connectedComponentsWithStats() gives you blobs + bounding boxes.
 *   This file wraps that pipeline and adds:
 *     1. CLAHE-based contrast enhancement  (handles different lighting)
 *     2. Shadow/darkness analysis per ROI  (depth proxy)
 *     3. Morphological cleanup             (removes noise & lane markings)
 *     4. Severity scoring formula          (area × darkness weight)
 *     5. Road-health aggregate score       (0–100%)
 */

#include "pothole_severity.hpp"
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace cv {

// ═══════════════════════════════════════════════════════════
//  INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════

// Preprocesses frame → binary mask highlighting dark road anomalies
static Mat buildPotholeMask(const Mat& frame) {

    // 1. Convert to grayscale
    Mat gray;
    cvtColor(frame, gray, COLOR_BGR2GRAY);

    // 2. CLAHE: enhance local contrast so potholes in shadows are visible
    //    (This is the key trick — standard thresholding fails in bad lighting)
    Ptr<CLAHE> clahe = createCLAHE(3.0, Size(8, 8));
    Mat enhanced;
    clahe->apply(gray, enhanced);

    // 3. Gaussian blur to kill high-frequency noise (gravel, text, etc.)
    Mat blurred;
    GaussianBlur(enhanced, blurred, Size(7, 7), 2.0);

    // 4. Adaptive threshold — finds locally dark regions (potholes appear dark)
    Mat binary;
    adaptiveThreshold(blurred, binary, 255,
                      ADAPTIVE_THRESH_GAUSSIAN_C,
                      THRESH_BINARY_INV,
                      31,   // block size  (must be odd)
                      12);  // C constant  (higher = more aggressive)

    // 5. Morphological ops to clean up the mask
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));

    // Opening: removes small noise blobs
    Mat opened;
    morphologyEx(binary, opened, MORPH_OPEN, kernel, Point(-1,-1), 2);

    // Closing: fills holes inside pothole regions
    Mat closed;
    morphologyEx(opened, closed, MORPH_CLOSE, kernel, Point(-1,-1), 3);

    return closed;
}

// Computes mean darkness in a ROI relative to surrounding road (0=bright, 1=black)
static double computeDarkness(const Mat& grayFrame, const Rect& roi) {
    // Clamp ROI to frame bounds
    Rect safeRoi = roi & Rect(0, 0, grayFrame.cols, grayFrame.rows);
    if (safeRoi.area() == 0) return 0.0;

    Mat roiMat = grayFrame(safeRoi);
    Scalar mean = cv::mean(roiMat);

    // Invert: dark pixels → high darkness score
    double darkness = 1.0 - (mean[0] / 255.0);
    return darkness;
}

// ═══════════════════════════════════════════════════════════
//  PUBLIC API IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════

PotholeSeverity classifySeverity(double area, double darkness) {
    // Composite score: area matters more, darkness acts as a confidence booster
    // darkness > 0.55 → likely a real pothole shadow (not a road marking)
    double score = area * (1.0 + darkness * 0.6);

    if (score < 600.0  || darkness < 0.35) return POTHOLE_NONE;
    if (score < 2500.0)                    return POTHOLE_MINOR;
    if (score < 8000.0)                    return POTHOLE_MODERATE;
    return POTHOLE_SEVERE;
}

void detectPotholeRegions(
    const Mat&                  frame,
    std::vector<PotholeRegion>& regions,
    PotholeStats&               stats,
    Mat&                        debugFrame,
    double                      minArea)
{
    regions.clear();

    // ── Step 1: Build mask ───────────────────────────────────
    Mat mask = buildPotholeMask(frame);

    // ── Step 2: Gray frame for darkness computation ──────────
    Mat gray;
    cvtColor(frame, gray, COLOR_BGR2GRAY);

    // ── Step 3: Find contours (extended connectedComponents) ─
    std::vector<std::vector<Point>> contours;
    std::vector<Vec4i> hierarchy;
    findContours(mask, contours, hierarchy,
                 RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // ── Step 4: Classify each contour ───────────────────────
    for (const auto& contour : contours) {
        double area = contourArea(contour);
        if (area < minArea) continue;  // ignore tiny blobs

        Rect bbox = boundingRect(contour);

        // Filter out very wide, thin shapes (likely lane markings)
        double aspectRatio = (double)bbox.width / (double)bbox.height;
        if (aspectRatio > 5.0 || aspectRatio < 0.2) continue;

        // Filter out huge blobs that span half the frame (sky, walls, etc.)
        double frameArea = frame.rows * frame.cols;
        if (area > frameArea * 0.20) continue;

        double darkness = computeDarkness(gray, bbox);
        PotholeSeverity severity = classifySeverity(area, darkness);

        if (severity == POTHOLE_NONE) continue;

        // Compute centroid
        Moments m = moments(contour);
        Point2f center(
            (m.m00 > 0) ? (float)(m.m10 / m.m00) : bbox.x + bbox.width / 2.f,
            (m.m00 > 0) ? (float)(m.m01 / m.m00) : bbox.y + bbox.height / 2.f
        );

        // Build label
        std::string label;
        switch (severity) {
            case POTHOLE_MINOR:    label = "MINOR";    break;
            case POTHOLE_MODERATE: label = "MODERATE"; break;
            case POTHOLE_SEVERE:   label = "SEVERE";   break;
            default:               label = "UNKNOWN";  break;
        }

        PotholeRegion region;
        region.boundingBox = bbox;
        region.area        = area;
        region.aspectRatio = aspectRatio;
        region.darkness    = darkness;
        region.severity    = severity;
        region.center      = center;
        region.label       = label;
        regions.push_back(region);
    }

    // ── Step 5: Compute aggregate stats ─────────────────────
    stats.totalDetected  = (int)regions.size();
    stats.minorCount     = 0;
    stats.moderateCount  = 0;
    stats.severeCount    = 0;

    for (const auto& r : regions) {
        if (r.severity == POTHOLE_MINOR)    stats.minorCount++;
        if (r.severity == POTHOLE_MODERATE) stats.moderateCount++;
        if (r.severity == POTHOLE_SEVERE)   stats.severeCount++;
    }

    // Road health: severe potholes penalise most
    double penalty = stats.minorCount    * 2.0
                   + stats.moderateCount * 6.0
                   + stats.severeCount   * 15.0;
    stats.overallRoadHealth = std::max(0.0, 100.0 - penalty);

    // ── Step 6: Build debug frame if requested ───────────────
    if (!debugFrame.empty() || debugFrame.data == nullptr) {
        debugFrame = frame.clone();
        drawPotholeOverlay(debugFrame, regions, stats);
    }
}

void drawPotholeOverlay(Mat& frame,
                        const std::vector<PotholeRegion>& regions,
                        const PotholeStats& stats)
{
    // Color map per severity
    auto severityColor = [](PotholeSeverity s) -> Scalar {
        switch (s) {
            case POTHOLE_MINOR:    return Scalar(0, 255, 180);   // cyan-green
            case POTHOLE_MODERATE: return Scalar(0, 165, 255);   // orange
            case POTHOLE_SEVERE:   return Scalar(0, 0, 255);     // red
            default:               return Scalar(200, 200, 200);
        }
    };

    for (const auto& r : regions) {
        Scalar color = severityColor(r.severity);

        // Draw bounding box
        rectangle(frame, r.boundingBox, color, 2);

        // Draw centroid dot
        circle(frame, r.center, 5, color, -1);

        // Label background + text
        std::ostringstream oss;
        oss << r.label << " (" << std::fixed << std::setprecision(0)
            << r.area << "px)";
        std::string text = oss.str();

        int baseline = 0;
        Size textSize = getTextSize(text, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        Point textOrg(r.boundingBox.x, r.boundingBox.y - 5);

        // Clamp label above frame
        if (textOrg.y < 15) textOrg.y = r.boundingBox.y + textSize.height + 5;

        rectangle(frame,
                  textOrg + Point(0, baseline),
                  textOrg + Point(textSize.width, -textSize.height - 2),
                  color, FILLED);
        putText(frame, text, textOrg,
                FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 0), 1);
    }

    // ── HUD: Road health bar ─────────────────────────────────
    int barW = 200, barH = 18;
    int barX = frame.cols - barW - 15, barY = 15;

    // Background
    rectangle(frame,
              Point(barX - 2, barY - 2),
              Point(barX + barW + 2, barY + barH + 2),
              Scalar(30, 30, 30), FILLED);

    // Health fill
    double health = stats.overallRoadHealth / 100.0;
    int fillW = (int)(barW * health);
    Scalar barColor = health > 0.6 ? Scalar(0, 200, 80)
                    : health > 0.3 ? Scalar(0, 165, 255)
                                   : Scalar(0, 0, 220);
    rectangle(frame,
              Point(barX, barY),
              Point(barX + fillW, barY + barH),
              barColor, FILLED);

    // Label
    std::ostringstream healthStr;
    healthStr << "Road Health: "
              << std::fixed << std::setprecision(1)
              << stats.overallRoadHealth << "%";
    putText(frame, healthStr.str(),
            Point(barX, barY + barH + 18),
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255), 1);

    // Count summary
    std::ostringstream summary;
    summary << "Minor:" << stats.minorCount
            << "  Mod:" << stats.moderateCount
            << "  Severe:" << stats.severeCount;
    putText(frame, summary.str(),
            Point(barX, barY + barH + 36),
            FONT_HERSHEY_SIMPLEX, 0.42, Scalar(200, 200, 200), 1);
}

} // namespace cv
