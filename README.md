# 🚧 Pothole Severity Classifier
### Modified OpenCV `imgproc` Module — Road Health Analysis System

---

## What This Does

Processes dashcam video (or a live camera / single image) and:

1. **Detects** pothole regions using adaptive thresholding + morphological cleanup
2. **Classifies** each pothole as `MINOR / MODERATE / SEVERE` using area + shadow-depth scoring
3. **Draws** annotated bounding boxes with severity labels on every frame
4. **Renders** a live road-health percentage bar
5. **Accumulates** a damage heatmap across the full video
6. **Exports** a CSV of grid-cell damage scores for GIS / Excel analysis

---

## Project Structure

```
pothole_detector/
├── modules/
│   └── imgproc/
│       ├── include/
│       │   └── pothole_severity.hpp   ← New OpenCV header (your modification)
│       └── src/
│           └── pothole_severity.cpp   ← New OpenCV source  (your modification)
├── main.cpp                           ← Pipeline entry point
├── heatmap.cpp                        ← Grid-based damage accumulator
└── CMakeLists.txt                     ← Build config
```

---

## How to Build

### Prerequisites
```bash
# Ubuntu / Debian
sudo apt update && sudo apt install -y libopencv-dev cmake build-essential

# macOS
brew install opencv cmake
```

### Build Steps
```bash
git clone <your-repo>
cd pothole_detector

mkdir build && cd build
cmake ..
make -j4

# Binary will be at: build/pothole_detector
```

---

## How to Run

```bash
# On a dashcam video
./pothole_detector --video road.mp4

# On your laptop camera
./pothole_detector --camera 0

# On a single image
./pothole_detector --image road.jpg

# Save annotated output video
./pothole_detector --video road.mp4 --save output_annotated.mp4

# Tune sensitivity (default minArea=300, lower = detect smaller potholes)
./pothole_detector --video road.mp4 --minarea 150
```

---

## Keyboard Controls (during video playback)

| Key | Action |
|-----|--------|
| `H` | Toggle heatmap overlay |
| `S` | Save damage heatmap as CSV |
| `R` | Reset heatmap accumulator |
| `Q` / `ESC` | Quit |

---

## The OpenCV Modification Explained

Standard OpenCV's `connectedComponentsWithStats()` gives you:
- Blob count
- Bounding boxes
- Area

**This project extends it with:**

| Addition | File | What it does |
|---|---|---|
| `detectPotholeRegions()` | `pothole_severity.cpp` | Full pipeline: CLAHE → adaptive threshold → morphology → classify |
| `classifySeverity()` | `pothole_severity.cpp` | Scores potholes using `area × darkness_weight` formula |
| `drawPotholeOverlay()` | `pothole_severity.cpp` | Renders severity labels + road health bar |
| `PotholeRegion` struct | `pothole_severity.hpp` | Extended blob metadata (severity, darkness, centroid) |
| `PotholeStats` struct | `pothole_severity.hpp` | Aggregate road health score |
| `RoadHeatmap` class | `heatmap.cpp` | Grid-based accumulator + CSV export |

---

## How the Severity Scoring Works

```
composite_score = area_px × (1.0 + darkness × 0.6)

NONE:     score < 600   OR  darkness < 0.35   (likely road marking / noise)
MINOR:    score < 2500                         (surface crack)
MODERATE: score < 8000                         (medium pothole)
SEVERE:   score ≥ 8000                         (large / deep pothole)
```

`darkness` is the mean pixel darkness in the bounding-box ROI — a proxy for depth (deeper potholes cast darker shadows).

---

## Output Files

After running, the program auto-saves:
- `road_health_final.csv` — Grid damage scores (open in Excel / QGIS)

CSV format:
```
row,col,cell_x_center,cell_y_center,damage_score
0,0,32.0,24.0,0.00
0,1,96.0,24.0,3.50
...
```

---
