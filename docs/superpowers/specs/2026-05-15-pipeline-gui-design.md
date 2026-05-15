# Pipeline Simulator GUI — Design Spec

**Date:** 2026-05-15
**Branch:** feature/gui
**Project:** Fillet-O-Neumann (CSEN601 Package 2)

---

## Overview

A native C GUI for the pipeline simulator built with **raylib + raygui**. Compiles as a separate binary (`gui`) that links against the existing `parser.c` and `pipeline.c` objects. The terminal simulator is left completely untouched.

---

## Architecture

### Files

| File | Role |
|---|---|
| `gui.c` | New file. Own `main()`. All rendering and interaction logic. |
| `parser.c` | Unchanged. Linked into both `simulator` and `gui` binaries. |
| `pipeline.c` | Unchanged. Linked into both binaries. |
| `architecture.h` | Unchanged. Shared header. |
| `Makefile` | Add `gui` target alongside existing `simulator` target. |

### Build

```makefile
CC = gcc
CFLAGS = -Wall -O2
RAYLIB = -lraylib -lm

OBJS = parser.o pipeline.o

simulator: main.c $(OBJS)
	$(CC) $(CFLAGS) -o simulator main.c $(OBJS)

gui: gui.c $(OBJS)
	$(CC) $(CFLAGS) -o gui gui.c $(OBJS) $(RAYLIB)

%.o: %.c architecture.h
	$(CC) $(CFLAGS) -c $< -o $@
```

### State management

`gui.c` maintains a `CycleSnapshot` array — a saved copy of the full `Processor` struct after every `pipeline_cycle()` call:

```c
#define MAX_CYCLES 512

typedef struct {
    Processor state;
} CycleSnapshot;

CycleSnapshot snapshots[MAX_CYCLES];
int snapshot_count = 0;
int current_cycle  = 0;
```

- **Step forward:** call `pipeline_cycle()`, copy `Processor` into `snapshots[snapshot_count++]`, advance `current_cycle`.
- **Step backward:** decrement `current_cycle`, restore `snapshots[current_cycle]` into the active `Processor`.
- **Auto-play:** a frame timer advances `current_cycle` at the selected speed (0.5×, 1×, 2×, 5×).
- **File drop:** `IsFileDropped()` → `LoadDroppedFiles()` → `load_program_from_file()` → reset snapshots.

### Simulator states

```
IDLE    → file dropped  → READY
READY   → Step          → PAUSED  (runs one cycle, then waits)
READY   → Play          → RUNNING
PAUSED  → Step          → PAUSED  (advances one more cycle)
PAUSED  → Play          → RUNNING
RUNNING → Pause / Step  → PAUSED
RUNNING → last cycle    → DONE
DONE    → Reset         → READY
Any     → Reset         → READY
```

---

## Layout

Window size: **1100 × 620 px** (fixed, no resize needed).

```
┌─────────────────────────────────────────────────────────┐
│ Title bar:  ● ● ●   FILLET-O-NEUMANN · Pipeline         │  ← 32px
├──────────────────────────────┬──────────────────────────┤
│                              │                          │
│  LEFT PANEL (65%)            │  RIGHT PANEL (35%)       │
│                              │                          │
│  ┌ PIPELINE STAGES ────────┐ │  ┌ REGISTERS ──────────┐ │
│  │  IF → ID → EX → MEM→WB │ │  │  R0–R31 grid + PC   │ │
│  │  (color-coded boxes)    │ │  │  changed = green     │ │
│  └─────────────────────────┘ │  └─────────────────────┘ │
│                              │                          │
│  Forwarding banner (if FWD)  │  ┌ DATA MEMORY ────────┐ │
│                              │  │  addr: value         │ │
│  ┌ EVENT LOG ─────────────┐ │  │  zeros hidden        │ │
│  │  scrollable cycle log  │ │  │  writes = purple      │ │
│  └─────────────────────────┘ │  └─────────────────────┘ │
│                              │                          │
├──────────────────────────────┴──────────────────────────┤
│ Bottom bar: [drop zone] [◀] [Step] [▶Play] [↺] [speeds] │  ← 44px
└─────────────────────────────────────────────────────────┘
```

---

## Visual Components

### Pipeline stage boxes

Five boxes rendered left-to-right: IF → ID → EX → MEM → WB.

Each box shows:
- Stage name (bold)
- Instruction mnemonic
- Key values (operands for ID, ALU result for EX, dest register for WB)

Color coding:

| State | Background | Border | Text |
|---|---|---|---|
| Fetching (IF) | Dark green | Green | Light green |
| Active | Dark blue | Blue | Light blue |
| Empty / no-op | Dark grey | Mid grey | Dim grey |
| Flushed | Dark red | Red | Light red |
| Stalled | Dark amber | Amber | Light amber |

### Forwarding banner

A single dashed-border strip rendered below the pipeline row. Only visible when `cpu.fwd_ex_valid` or `cpu.fwd_mem_valid` is set. Shows: `FWD  EX→ID  Rn = value`.

### Event log

Scrollable text area in the lower-left. One entry per clock cycle, indented sub-entries for events within that cycle. Color coding:

| Event | Color |
|---|---|
| Stage activity | Grey |
| Register write (WB) | Green |
| Memory write (MEM) | Purple |
| Branch taken / flush | Red |
| Stall inserted | Amber |
| Forwarding | Light blue |

### Register panel

Grid of R0–R31 plus PC (34 entries). Each entry shows register name and current value. Registers whose value changed on the last completed cycle are highlighted with a green left border and bright green value text. PC always shown with blue left border.

### Memory panel

Shows only the data segment (addresses 1024–2047). Zero-value addresses are hidden with a `· · · (zeros hidden)` placeholder. Addresses written in the last completed cycle are highlighted with a purple left border.

### Bottom bar

Left to right:
- **Drop zone:** dashed border. Shows `Drop .txt file here` when empty; shows filename when loaded. Accepts `.txt` and `.asm`.
- **◀ Back:** step back one cycle. Disabled at cycle 0.
- **⏵ Step:** advance one cycle. Disabled when simulation is complete.
- **▶ Play / ⏸ Pause:** toggles auto-play.
- **↺ Reset:** resets to cycle 0, keeps program loaded.
- **Speed selector:** ½× · 1× · 2× · 5× toggle buttons.
- **Progress:** `Cycle n` while simulation is in progress; `Cycle n / n (done)` once DONE state is reached.

---

## raylib Integration Notes

- Use `IsFileDropped()` + `LoadDroppedFiles()` for drag-and-drop.
- Use `raygui.h` (single-file include) for buttons and the speed toggle group.
- Render text directly with `DrawText()` / `DrawTextEx()` for the log and register values — no need for raygui text widgets there.
- Target 60 FPS with `SetTargetFPS(60)`. Auto-play speed in frames per cycle:
  - ½× = 120 frames/cycle (~0.5 cycles/sec)
  - 1×  =  60 frames/cycle (~1 cycle/sec) — default, comfortable to watch
  - 2×  =  30 frames/cycle (~2 cycles/sec)
  - 5×  =  12 frames/cycle (~5 cycles/sec)
- Window title: `"Fillet-O-Neumann Pipeline Simulator"`.
- raylib include path must be set in the Makefile `CFLAGS` (e.g. `-I./raylib/include`). The exact path depends on where raylib is installed — document in a README note.

---

## Out of Scope

- Editing assembly in the GUI (write programs in a text editor, drop the file in).
- Instruction memory viewer (only data memory shown).
- Saving/exporting the event log.
- Dark/light theme toggle.
