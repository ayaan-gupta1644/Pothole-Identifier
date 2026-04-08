/**
 * main.cpp
 *
 * Pothole Severity Classifier — Main Pipeline
 *
 * Usage:
 *   ./pothole_detector --video road.mp4
 *   ./pothole_detector --camera 0
 *   ./pothole_detector --image road.jpg
 *
 * Controls (during playback):
 *   H  — toggle heatmap overlay
 *   D  — toggle debug mask view
 *   S  — save current heatmap CSV
 *   R  — reset heatmap
 *   Q / ESC — quit
 */

#include <opencv2/opencv.hpp>
#include "modules/imgproc/include/pothole_severity.hpp"
#include "heatmap.cpp"

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

// ── CLI argument parser ──────────────────────────────────────
struct Config {
    std::string videoPath;
    int         cameraId   = -1;
    std::string imagePath;
    double      minArea    = 300.0;
    bool        saveOutput = false;
    std::string outputPath = "output.mp4";
};

Config parseArgs(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--video"  && i+1 < argc) cfg.videoPath  = argv[++i];
        if (arg == "--camera" && i+1 < argc) cfg.cameraId   = std::stoi(argv[++i]);
        if (arg == "--image"  && i+1 < argc) cfg.imagePath  = argv[++i];
        if (arg == "--minarea"&& i+1 < argc) cfg.minArea    = std::stod(argv[++i]);
        if (arg == "--save"   && i+1 < argc) { cfg.saveOutput = true; cfg.outputPath = argv[++i]; }
    }
    return cfg;
}

// ── FPS counter ──────────────────────────────────────────────
class FPSCounter {
    std::chrono::steady_clock::time_point last_;
    double fps_ = 0.0;
public:
    void tick() {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_).count();
        fps_ = (dt > 0) ? 0.9 * fps_ + 0.1 * (1.0 / dt) : fps_;
        last_ = now;
    }
    double fps() const { return fps_; }
};

// ── Process a single frame ───────────────────────────────────
void processFrame(const cv::Mat& frame,
                  cv::Mat& display,
                  RoadHeatmap& heatmap,
                  bool showHeatmap,
                  const Config& cfg)
{
    std::vector<cv::PotholeRegion> regions;
    cv::PotholeStats stats;
    cv::Mat debugFrame;  // will be filled by detectPotholeRegions

    // ── CORE DETECTION CALL ──────────────────────────────────
    cv::detectPotholeRegions(frame, regions, stats, debugFrame, cfg.minArea);

    // Update heatmap accumulator
    heatmap.update(regions);

    if (showHeatmap) {
        display = heatmap.render(debugFrame);
    } else {
        display = debugFrame;
    }
}

// ── Render HUD overlay ───────────────────────────────────────
void renderHUD(cv::Mat& frame, double fps, bool showHeatmap,
               int frameIdx, const cv::PotholeStats& stats)
{
    // Top-left info panel
    std::vector<std::string> lines = {
        "FPS: " + [&]{ std::ostringstream s; s << std::fixed << std::setprecision(1) << fps; return s.str(); }(),
        "Frame: " + std::to_string(frameIdx),
        "Potholes: " + std::to_string(stats.totalDetected),
        "Mode: " + std::string(showHeatmap ? "HEATMAP" : "LIVE"),
    };

    int y = 22;
    for (const auto& line : lines) {
        cv::putText(frame, line, {10, y},
                    cv::FONT_HERSHEY_SIMPLEX, 0.52,
                    cv::Scalar(255, 255, 100), 1);
        y += 20;
    }

    // Controls hint (bottom left)
    std::vector<std::string> hints = {
        "[H] Heatmap  [D] Debug  [S] Save CSV  [R] Reset  [Q] Quit"
    };
    cv::putText(frame, hints[0],
                {10, frame.rows - 10},
                cv::FONT_HERSHEY_SIMPLEX, 0.4,
                cv::Scalar(160, 160, 160), 1);
}

// ═══════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
    Config cfg = parseArgs(argc, argv);

    // ── Open source ─────────────────────────────────────────
    cv::VideoCapture cap;
    bool isImage = false;
    cv::Mat staticImage;

    if (!cfg.imagePath.empty()) {
        staticImage = cv::imread(cfg.imagePath);
        if (staticImage.empty()) {
            std::cerr << "[ERROR] Cannot load image: " << cfg.imagePath << "\n";
            return 1;
        }
        isImage = true;
    } else if (!cfg.videoPath.empty()) {
        cap.open(cfg.videoPath);
    } else if (cfg.cameraId >= 0) {
        cap.open(cfg.cameraId);
        cap.set(cv::CAP_PROP_FRAME_WIDTH,  1280);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    } else {
        std::cerr << "Usage: ./pothole_detector --video <file> | --camera <id> | --image <file>\n";
        return 1;
    }

    if (!isImage && !cap.isOpened()) {
        std::cerr << "[ERROR] Cannot open video source\n";
        return 1;
    }

    // ── Get frame dimensions ─────────────────────────────────
    int frameW, frameH;
    if (isImage) {
        frameW = staticImage.cols;
        frameH = staticImage.rows;
    } else {
        frameW = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
        frameH = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    }

    std::cout << "[INFO] Frame size: " << frameW << "x" << frameH << "\n";

    // ── Setup heatmap & writer ───────────────────────────────
    RoadHeatmap heatmap(frameW, frameH, 20, 15);

    cv::VideoWriter writer;
    if (cfg.saveOutput && !isImage) {
        writer.open(cfg.outputPath,
                    cv::VideoWriter::fourcc('m','p','4','v'),
                    30, cv::Size(frameW, frameH));
    }

    // ── State flags ──────────────────────────────────────────
    bool showHeatmap = false;
    int  frameIdx    = 0;
    FPSCounter fps;

    cv::namedWindow("Pothole Detector", cv::WINDOW_NORMAL);
    cv::resizeWindow("Pothole Detector", std::min(frameW, 1280),
                                         std::min(frameH, 720));

    // ── Main loop ────────────────────────────────────────────
    while (true) {
        cv::Mat frame;

        if (isImage) {
            frame = staticImage.clone();
        } else {
            cap >> frame;
            if (frame.empty()) {
                std::cout << "[INFO] End of video.\n";
                break;
            }
        }

        fps.tick();

        // Detect + render
        cv::Mat display;
        std::vector<cv::PotholeRegion> regions;
        cv::PotholeStats stats;
        cv::Mat debugFrame;
        cv::detectPotholeRegions(frame, regions, stats, debugFrame, cfg.minArea);
        heatmap.update(regions);

        if (showHeatmap)
            display = heatmap.render(debugFrame);
        else
            display = debugFrame;

        renderHUD(display, fps.fps(), showHeatmap, frameIdx, stats);

        cv::imshow("Pothole Detector", display);
        if (writer.isOpened()) writer.write(display);

        frameIdx++;

        // Key handling
        int key = cv::waitKey(isImage ? 0 : 1) & 0xFF;
        if (key == 'q' || key == 27)   break;
        if (key == 'h' || key == 'H')  showHeatmap = !showHeatmap;
        if (key == 'r' || key == 'R')  { heatmap.reset(); std::cout << "[INFO] Heatmap reset\n"; }
        if (key == 's' || key == 'S')  {
            std::string csvPath = "road_health_frame" + std::to_string(frameIdx) + ".csv";
            heatmap.saveCSV(csvPath);
        }
        if (isImage) break;  // Single image — show once then exit
    }

    if (writer.isOpened()) {
        writer.release();
        std::cout << "[INFO] Saved video → " << cfg.outputPath << "\n";
    }

    // Auto-save final heatmap CSV
    heatmap.saveCSV("road_health_final.csv");
    std::cout << "[INFO] Done. Processed " << frameIdx << " frames.\n";

    return 0;
}
