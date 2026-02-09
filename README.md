# jcan

CAN bus tool with DBC decoding, frame logging, and transmit scheduling.

![imgui](https://img.shields.io/badge/gui-imgui-blue)
![c++](https://img.shields.io/badge/c%2B%2B-23-orange)

## build

```
cmake -B build
cmake --build build -j$(nproc)
```

## run

```
./build/jcan_gui
```

there's also a headless cli dump:

```
./build/jcan_cli
```

## features

- live bus monitor with per-id collapse and delta timing
- scrollback buffer (50k frames)
- dbc loading via file dialog or drag-and-drop
- signal watcher with rolling plots
- periodic tx scheduler with dbc signal encoding
- csv/asc logging and replay with variable speed
- slcan (canable/candapter) and socketcan adapters
- multi-adapter support
- can fd
