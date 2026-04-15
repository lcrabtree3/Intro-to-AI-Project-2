# Intro-to-AI-Project-2
# Co-op AI Dungeon Master

## Overview
A real-time cooperative dungeon exploration game with:
- Procedural map generation
- Turn-based actions
- WebSocket multiplayer
- Narrative system
- Combat (Rock/Paper/Scissors)

---

## Requirements

### Backend
- C++17
- CMake
- uWebSockets
- nlohmann/json

### Frontend
- Any browser

---

## Build Backend

```bash
cd backend
mkdir build
cd build
cmake ..
make
./server
```

## GPT-2 setup for AI summary

Install Python dependencies for local GPT-2 summary generation:

```bash
python -m pip install torch transformers
```

If `python` is not available, use:

```bash
py -m pip install torch transformers
```
