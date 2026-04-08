/**
 * heatmap.cpp
 *
 * Road Health Heatmap Generator
 *
 * Accumulates pothole detections across all video frames and
 * renders a colour-coded grid heatmap showing which road zones
 * are most damaged.
 *
 * Usage:
 *   RoadHeatmap hm(frameWidth, frameHeight, gridCols=16, gridRows=12);
 *   hm.update(regions);          // call every frame
 *   Mat viz = hm.render(frame);  // get the overlay
 *   hm.saveCSV("report.csv");    // export for Excel / GIS tools
 */

#pragma once
#include <opencv2/opencv.hpp>
#include "modules/imgproc/include/pothole_severity.hpp"
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

class RoadHeatmap {
public:
    RoadHeatmap(int frameW, int frameH, int gridCols = 16, int gridRows = 12)
        : frameW_(frameW), frameH_(frameH),
          gridCols_(gridCols), gridRows_(gridRows)
    {
        // Initialize score grid to zero
        grid_.assign(gridRows_, std::vector<double>(gridCols_, 0.0));
        cellW_ = (double)frameW_  / gridCols_;
        cellH_ = (double)frameH_ / gridRows_;
    }

    // Call once per frame with that frame's detected potholes
    void update(const std::vector<cv::PotholeRegion>& regions) {
        for (const auto& r : regions) {
            // Which grid cell does this pothole's center fall into?
            int col = std::min((int)(r.center.x / cellW_), gridCols_ - 1);
            int row = std::min((int)(r.center.y / cellH_), gridRows_ - 1);

            // Add weighted score based on severity
            double weight = 0.0;
            switch (r.severity) {
                case cv::POTHOLE_MINOR:    weight = 1.0; break;
                case cv::POTHOLE_MODERATE: weight = 3.0; break;
                case cv::POTHOLE_SEVERE:   weight = 7.0; break;
                default: break;
            }
            grid_[row][col] += weight;
        }
        frameCount_++;
    }

    // Returns a blended heatmap overlay on top of the reference frame
    cv::Mat render(const cv::Mat& baseFrame) const {
        cv::Mat overlay = baseFrame.clone();

        // Find max grid value for normalisation
        double maxVal = 0.0;
        for (const auto& row : grid_)
            for (double v : row)
                maxVal = std::max(maxVal, v);

        if (maxVal < 1e-6) return overlay;  // nothing detected yet

        for (int r = 0; r < gridRows_; r++) {
            for (int c = 0; c < gridCols_; c++) {
                double normalised = grid_[r][c] / maxVal;   // 0.0 – 1.0
                if (normalised < 0.01) continue;

                // Map 0→green, 0.5→orange, 1→red  using BGR
                cv::Scalar color = scoreToColor(normalised);

                cv::Point tl((int)(c * cellW_),       (int)(r * cellH_));
                cv::Point br((int)((c+1) * cellW_),   (int)((r+1) * cellH_));

                // Draw semi-transparent coloured cell
                cv::Mat roi = overlay(cv::Rect(tl, br));
                cv::Mat colorMat(roi.size(), roi.type(), color);
                double alpha = 0.35 + normalised * 0.30;   // more opaque = more damage
                cv::addWeighted(roi, 1.0 - alpha, colorMat, alpha, 0, roi);

                // Draw cell score if significant
                if (normalised > 0.15) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(1) << grid_[r][c];
                    cv::putText(overlay, oss.str(),
                                tl + cv::Point(4, (int)(cellH_ * 0.55)),
                                cv::FONT_HERSHEY_SIMPLEX,
                                std::min(cellW_, cellH_) / 55.0,
                                cv::Scalar(255, 255, 255), 1);
                }
            }
        }

        // Draw grid lines
        for (int c = 0; c <= gridCols_; c++) {
            int x = (int)(c * cellW_);
            cv::line(overlay, {x, 0}, {x, frameH_}, {60,60,60}, 1);
        }
        for (int r = 0; r <= gridRows_; r++) {
            int y = (int)(r * cellH_);
            cv::line(overlay, {0, y}, {frameW_, y}, {60,60,60}, 1);
        }

        // Legend
        drawLegend(overlay);

        return overlay;
    }

    // Export grid scores to CSV (open in Excel / QGIS)
    void saveCSV(const std::string& path) const {
        std::ofstream f(path);
        f << "row,col,cell_x_center,cell_y_center,damage_score\n";
        for (int r = 0; r < gridRows_; r++) {
            for (int c = 0; c < gridCols_; c++) {
                double cx = (c + 0.5) * cellW_;
                double cy = (r + 0.5) * cellH_;
                f << r << "," << c << ","
                  << std::fixed << std::setprecision(2)
                  << cx << "," << cy << ","
                  << grid_[r][c] << "\n";
            }
        }
        f.close();
        std::cout << "[Heatmap] Saved CSV → " << path << "\n";
    }

    void reset() {
        for (auto& row : grid_)
            std::fill(row.begin(), row.end(), 0.0);
        frameCount_ = 0;
    }

    int frameCount() const { return frameCount_; }

private:
    int    frameW_, frameH_;
    int    gridCols_, gridRows_;
    double cellW_, cellH_;
    int    frameCount_ = 0;
    std::vector<std::vector<double>> grid_;

    // Maps 0→1 score to BGR colour (green → yellow → red)
    static cv::Scalar scoreToColor(double t) {
        // t: 0=good (green), 1=bad (red)
        double r, g, b;
        if (t < 0.5) {
            // green → yellow
            r = t * 2.0 * 255;
            g = 200;
            b = 0;
        } else {
            // yellow → red
            r = 255;
            g = (1.0 - (t - 0.5) * 2.0) * 200;
            b = 0;
        }
        return cv::Scalar(b, g, r);  // BGR
    }

    void drawLegend(cv::Mat& frame) const {
        int lx = 10, ly = frame.rows - 80;
        cv::putText(frame, "Damage Heatmap", {lx, ly},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, {220,220,220}, 1);

        // Gradient bar
        for (int i = 0; i < 120; i++) {
            double t = i / 120.0;
            cv::Scalar c = scoreToColor(t);
            cv::rectangle(frame, {lx + i, ly + 8}, {lx + i + 1, ly + 22}, c, cv::FILLED);
        }
        cv::putText(frame, "Low",  {lx,       ly + 36}, cv::FONT_HERSHEY_SIMPLEX, 0.38, {180,255,180}, 1);
        cv::putText(frame, "High", {lx + 100, ly + 36}, cv::FONT_HERSHEY_SIMPLEX, 0.38, {100,100,255}, 1);
    }
};
