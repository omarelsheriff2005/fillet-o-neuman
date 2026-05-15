# Pipeline Simulator GUI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a native C raylib GUI window that lets you drag-and-drop an assembly file, step through or auto-play the pipeline cycle-by-cycle, and see the pipeline stages, registers, and memory update live.

**Architecture:** Separate `gui.exe` binary that links against the existing `parser.o` and `pipeline.o` objects. `gui.c` maintains a `CycleSnapshot[]` array (one saved `Processor` per cycle) so you can step forward and backward freely. All original files are untouched.

**Tech Stack:** C11, raylib 5.0 (prebuilt MinGW), raygui.h (single-header), gcc/MinGW, existing Makefile extended with a `gui` target.

---

## File Map

| Action | Path | Responsibility |
|---|---|---|
| Create | `gui.c` | All GUI logic — rendering, interaction, state machine |
| Create | `raylib/` | Prebuilt raylib libs and headers (not committed) |
| Modify | `Makefile` | Add `gui` and `gui-clean` targets |
| Modify | `.gitignore` | Ignore `raylib/` and `gui.exe` |

`parser.c`, `pipeline.c`, `architecture.h`, `main.c` — **do not touch**.

---

## Task 1: Install raylib and add the Makefile build target

**Files:**
- Modify: `Makefile`
- Modify: `.gitignore`
- Create: `gui.c` (minimal window only)

- [ ] **Step 1: Download raylib prebuilt MinGW package**

  Go to https://github.com/raysan5/raylib/releases/tag/5.0 and download `raylib-5.0_win64_mingw-w64.zip`. Extract it so the project has:

  ```
  fillet-o-neuman/
    raylib/
      include/
        raylib.h
        raylib_bool.h   (may not exist — that's fine)
      lib/
        libraylib.a
  ```

- [ ] **Step 2: Download raygui single-header**

  Download `raygui.h` from https://github.com/raysan5/raygui/releases/tag/4.0 (pick the raw `.h` file from the release assets or the repo).
  Place it at `raylib/include/raygui.h`.

- [ ] **Step 3: Add gui target to Makefile**

  Open `Makefile`. The current content is:
  ```makefile
  CC := gcc
  CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
  TARGET := fillet-o-neuman.exe
  OBJS := main.o parser.o pipeline.o

  .PHONY: all clean run

  all: $(TARGET)

  $(TARGET): $(OBJS)
  	$(CC) $(CFLAGS) -o $@ $(OBJS)

  %.o: %.c architecture.h
  	$(CC) $(CFLAGS) -c $< -o $@

  run: $(TARGET)
  	./$(TARGET) program.txt

  clean:
  	rm -f $(TARGET) $(OBJS)
  ```

  Replace it with:
  ```makefile
  CC := gcc
  CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g

  TARGET  := fillet-o-neuman.exe
  OBJS    := main.o parser.o pipeline.o

  RAYLIB_DIR := raylib
  RAYLIB_INC := $(RAYLIB_DIR)/include
  RAYLIB_LIB := $(RAYLIB_DIR)/lib
  RAYLIB_LINK := -L$(RAYLIB_LIB) -lraylib -lopengl32 -lgdi32 -lwinmm

  GUI_TARGET := gui.exe
  GUI_OBJS   := gui.o parser.o pipeline.o

  .PHONY: all gui run clean gui-clean

  all: $(TARGET)

  $(TARGET): $(OBJS)
  	$(CC) $(CFLAGS) -o $@ $(OBJS)

  gui: $(GUI_TARGET)

  $(GUI_TARGET): $(GUI_OBJS)
  	$(CC) $(CFLAGS) -o $@ $(GUI_OBJS) $(RAYLIB_LINK)

  gui.o: gui.c architecture.h
  	$(CC) $(CFLAGS) -I$(RAYLIB_INC) -c gui.c -o gui.o

  %.o: %.c architecture.h
  	$(CC) $(CFLAGS) -c $< -o $@

  run: $(TARGET)
  	./$(TARGET) program.txt

  clean:
  	rm -f $(TARGET) $(OBJS) $(GUI_TARGET) $(GUI_OBJS)
  ```

- [ ] **Step 4: Add raylib/ to .gitignore**

  Open `.gitignore`. Append:
  ```
  raylib/
  ```

- [ ] **Step 5: Create minimal gui.c that opens a blank window**

  Create `gui.c`:
  ```c
  #include "raylib.h"
  #define RAYGUI_IMPLEMENTATION
  #include "raygui.h"
  #include "architecture.h"
  #include <stdio.h>
  #include <string.h>

  int main(void) {
      InitWindow(1100, 620, "Fillet-O-Neumann Pipeline Simulator");
      SetTargetFPS(60);

      while (!WindowShouldClose()) {
          BeginDrawing();
          ClearBackground((Color){13, 17, 23, 255});
          DrawText("Fillet-O-Neumann", 20, 40, 24, WHITE);
          EndDrawing();
      }

      CloseWindow();
      return 0;
  }
  ```

- [ ] **Step 6: Build and verify**

  ```
  make gui
  ```

  Expected: `gui.exe` appears with no errors. Running `./gui.exe` opens a dark window with the title text. Close it with the X button.

- [ ] **Step 7: Commit**

  ```bash
  git add Makefile .gitignore gui.c
  git commit -m "feat: add raylib build target and minimal GUI window"
  ```

---

## Task 2: Add constants, types, and global state to gui.c

**Files:**
- Modify: `gui.c`

This task expands gui.c with all the shared definitions that every subsequent task will use. The window still shows a blank screen — no visual change yet.

- [ ] **Step 1: Replace gui.c with the expanded version**

  ```c
  #include "raylib.h"
  #define RAYGUI_IMPLEMENTATION
  #include "raygui.h"
  #include "architecture.h"
  #include <stdio.h>
  #include <string.h>
  #include <stdlib.h>

  /* ── Window / layout ─────────────────────────────────────── */
  #define WIN_W        1100
  #define WIN_H        620
  #define TITLEBAR_H   32
  #define BOTTOMBAR_H  44
  #define LEFT_W       715
  #define RIGHT_W      (WIN_W - LEFT_W)
  #define CONTENT_Y    TITLEBAR_H
  #define CONTENT_H    (WIN_H - TITLEBAR_H - BOTTOMBAR_H)

  /* ── Colors (GitHub dark) ─────────────────────────────────── */
  #define COL_BG        (Color){13,17,23,255}
  #define COL_PANEL     (Color){22,27,34,255}
  #define COL_BORDER    (Color){48,54,61,255}
  #define COL_TEXT      (Color){230,237,243,255}
  #define COL_DIM       (Color){139,148,158,255}
  #define COL_VDIM      (Color){72,79,88,255}

  /* stage colors */
  #define COL_FETCH_BG  (Color){26,58,26,255}
  #define COL_FETCH_BD  (Color){63,185,80,255}
  #define COL_FETCH_TX  (Color){86,211,100,255}
  #define COL_ACT_BG    (Color){13,65,157,255}
  #define COL_ACT_BD    (Color){56,139,253,255}
  #define COL_ACT_TX    (Color){121,192,255,255}
  #define COL_EMPTY_BG  (Color){22,27,34,255}
  #define COL_EMPTY_BD  (Color){33,38,45,255}
  #define COL_EMPTY_TX  (Color){110,118,129,255}
  #define COL_FLUSH_BG  (Color){58,26,26,255}
  #define COL_FLUSH_BD  (Color){218,54,51,255}
  #define COL_FLUSH_TX  (Color){255,123,114,255}
  #define COL_STALL_BG  (Color){58,46,26,255}
  #define COL_STALL_BD  (Color){210,153,34,255}
  #define COL_STALL_TX  (Color){227,179,65,255}

  /* event colors */
  #define COL_GREEN     (Color){63,185,80,255}
  #define COL_PURPLE    (Color){188,140,255,255}
  #define COL_BLUE      (Color){88,166,255,255}
  #define COL_RED       (Color){255,123,114,255}
  #define COL_AMBER     (Color){227,179,65,255}

  /* ── State machine ────────────────────────────────────────── */
  typedef enum { SIM_IDLE, SIM_READY, SIM_PAUSED, SIM_RUNNING, SIM_DONE } SimState;

  /* ── Snapshots ────────────────────────────────────────────── */
  #define MAX_CYCLES 512

  typedef struct { Processor cpu; } CycleSnapshot;

  static CycleSnapshot snapshots[MAX_CYCLES];
  static int snapshot_count = 0;   /* number of saved snapshots          */
  static int current_cycle  = 0;   /* index into snapshots[] being shown */
  static SimState sim_state = SIM_IDLE;

  /* working copy — always equals snapshots[current_cycle].cpu */
  static Processor cpu;

  /* loaded filename for display */
  static char loaded_file[260] = "";

  /* speed: frames per cycle advance (at 60 FPS) */
  static const int SPEED_FRAMES[] = {120, 60, 30, 12}; /* 0.5x 1x 2x 5x */
  static int speed_idx = 1;   /* default 1x */
  static int play_frame = 0;  /* frame counter for auto-play */

  /* log scroll offset (lines from top) */
  static int log_scroll = 0;

  /* forward declarations */
  static void advance_cycle(void);
  static void step_back(void);
  static void reset_simulation(void);
  static void load_dropped_file(const char *path);
  static void draw_titlebar(void);
  static void draw_pipeline(void);
  static void draw_forwarding_banner(int y);
  static void draw_event_log(int y, int h);
  static void draw_registers(void);
  static void draw_memory(void);
  static void draw_bottom_bar(void);

  int main(void) {
      InitWindow(WIN_W, WIN_H, "Fillet-O-Neumann Pipeline Simulator");
      SetTargetFPS(60);

      while (!WindowShouldClose()) {
          /* ── update ── */
          if (sim_state == SIM_RUNNING) {
              play_frame++;
              if (play_frame >= SPEED_FRAMES[speed_idx]) {
                  play_frame = 0;
                  advance_cycle();
              }
          }

          /* ── draw ── */
          BeginDrawing();
          ClearBackground(COL_BG);
          DrawText("skeleton", 20, 40, 20, WHITE);
          EndDrawing();
      }

      CloseWindow();
      return 0;
  }
  ```

- [ ] **Step 2: Add stub implementations for all forward-declared functions**

  Append to the bottom of `gui.c` (before `main`):

  ```c
  static void advance_cycle(void) { }
  static void step_back(void)     { }
  static void reset_simulation(void) { }
  static void load_dropped_file(const char *path) { (void)path; }
  static void draw_titlebar(void)        { }
  static void draw_pipeline(void)        { }
  static void draw_forwarding_banner(int y) { (void)y; }
  static void draw_event_log(int y, int h)  { (void)y; (void)h; }
  static void draw_registers(void)       { }
  static void draw_memory(void)          { }
  static void draw_bottom_bar(void)      { }
  ```

- [ ] **Step 3: Build and verify**

  ```
  make gui
  ```

  Expected: compiles with no errors (possibly some unused-parameter warnings from stubs — that's fine). Window opens and shows "skeleton" text.

- [ ] **Step 4: Commit**

  ```bash
  git add gui.c
  git commit -m "feat: add GUI constants, state machine, and snapshot types"
  ```

---

## Task 3: File drop + program loading

**Files:**
- Modify: `gui.c` — implement `load_dropped_file`, handle `IsFileDropped()` in main loop, show filename in draw loop

- [ ] **Step 1: Implement load_dropped_file()**

  Replace the stub:
  ```c
  static void load_dropped_file(const char *path) {
      initialize_processor(&cpu);
      load_program_from_file(&cpu, path);

      /* save initial snapshot (cycle 0 = before any cycles run) */
      snapshots[0].cpu = cpu;
      snapshot_count   = 1;
      current_cycle    = 0;
      log_scroll       = 0;
      play_frame       = 0;

      /* store just the filename (not full path) for display */
      const char *slash = strrchr(path, '\\');
      const char *name  = slash ? slash + 1 : path;
      strncpy(loaded_file, name, sizeof(loaded_file) - 1);
      loaded_file[sizeof(loaded_file) - 1] = '\0';

      sim_state = SIM_READY;
  }
  ```

- [ ] **Step 2: Handle file drop event in the main loop**

  In `main()`, inside the `while (!WindowShouldClose())` loop, add before the draw block:
  ```c
  if (IsFileDropped()) {
      FilePathList dropped = LoadDroppedFiles();
      if (dropped.count > 0) {
          load_dropped_file(dropped.paths[0]);
      }
      UnloadDroppedFiles(dropped);
  }
  ```

- [ ] **Step 3: Draw the loaded filename and state as feedback**

  Replace `DrawText("skeleton", ...)` in BeginDrawing with:
  ```c
  const char *state_text =
      sim_state == SIM_IDLE    ? "Drop a .txt file to load a program" :
      sim_state == SIM_READY   ? "Program loaded — press Space to step" :
      sim_state == SIM_PAUSED  ? "Paused" :
      sim_state == SIM_RUNNING ? "Running..." : "Done";
  DrawText(state_text,  20, 40, 18, COL_DIM);
  DrawText(loaded_file, 20, 65, 16, COL_GREEN);
  ```

- [ ] **Step 4: Build and verify**

  ```
  make gui
  ```

  Run `./gui.exe`. Drag `program.txt` onto the window. Expected: "Program loaded — press Space to step" appears and the filename shows in green. Terminal prints the loaded instruction list (from `load_program_from_file`'s own output).

- [ ] **Step 5: Commit**

  ```bash
  git add gui.c
  git commit -m "feat: implement file drop and program loading"
  ```

---

## Task 4: Snapshot system — step forward, step back, reset

**Files:**
- Modify: `gui.c` — implement `advance_cycle`, `step_back`, `reset_simulation`, wire to keyboard

- [ ] **Step 1: Implement advance_cycle()**

  Replace the stub:
  ```c
  static void advance_cycle(void) {
      if (sim_state == SIM_DONE) return;
      if (sim_state == SIM_IDLE) return;

      if (current_cycle + 1 < snapshot_count) {
          /* already computed — replay cached snapshot, no printf noise */
          current_cycle++;
          cpu = snapshots[current_cycle].cpu;
      } else {
          if (snapshot_count >= MAX_CYCLES) return;
          pipeline_cycle(&cpu);
          snapshots[snapshot_count].cpu = cpu;
          current_cycle = snapshot_count;
          snapshot_count++;
      }

      sim_state = pipeline_empty(&cpu) ? SIM_DONE : SIM_PAUSED;
  }
  ```

- [ ] **Step 2: Implement step_back()**

  Replace the stub:
  ```c
  static void step_back(void) {
      if (current_cycle <= 0) return;
      current_cycle--;
      cpu = snapshots[current_cycle].cpu;
      sim_state = SIM_PAUSED;
  }
  ```

- [ ] **Step 3: Implement reset_simulation()**

  Replace the stub:
  ```c
  static void reset_simulation(void) {
      if (snapshot_count == 0) return;
      current_cycle = 0;
      cpu = snapshots[0].cpu;
      play_frame = 0;
      log_scroll = 0;
      sim_state = SIM_READY;
  }
  ```

- [ ] **Step 4: Wire keyboard shortcuts in the main loop**

  In `main()`, add inside the while loop (before draw):
  ```c
  if (sim_state != SIM_IDLE) {
      if (IsKeyPressed(KEY_SPACE))      advance_cycle();
      if (IsKeyPressed(KEY_LEFT))       step_back();
      if (IsKeyPressed(KEY_R))          reset_simulation();
      if (IsKeyPressed(KEY_P)) {
          if (sim_state == SIM_RUNNING)     sim_state = SIM_PAUSED;
          else if (sim_state == SIM_PAUSED) sim_state = SIM_RUNNING;
      }
  }
  ```

- [ ] **Step 5: Show cycle number on screen**

  Add to the draw block:
  ```c
  char cycle_buf[32];
  if (sim_state != SIM_IDLE)
      snprintf(cycle_buf, sizeof(cycle_buf), "Cycle %d", current_cycle);
  else
      cycle_buf[0] = '\0';
  DrawText(cycle_buf, 20, 90, 16, COL_BLUE);
  ```

- [ ] **Step 6: Build and verify**

  ```
  make gui
  ```

  Load `program.txt`. Press Space repeatedly — the cycle counter increments and terminal prints pipeline state each new cycle. Press Left — counter decrements (no terminal output, using cached snapshot). Press R — resets to 0. Press P then Space — auto-advances. Press P again — stops.

- [ ] **Step 7: Commit**

  ```bash
  git add gui.c
  git commit -m "feat: implement snapshot system and step/back/reset/play controls"
  ```

---

## Task 5: Draw the pipeline stage boxes

**Files:**
- Modify: `gui.c` — implement `draw_pipeline()`, call it from main draw block

- [ ] **Step 1: Implement draw_pipeline()**

  Replace the stub:
  ```c
  static void draw_pipeline(void) {
      const Processor *p = &snapshots[current_cycle].cpu;

      /* section header */
      DrawRectangle(0, CONTENT_Y, LEFT_W, 20, COL_PANEL);
      DrawLine(0, CONTENT_Y + 20, LEFT_W, CONTENT_Y + 20, COL_BORDER);
      DrawText("PIPELINE STAGES", 12, CONTENT_Y + 4, 10, COL_DIM);

      int base_y = CONTENT_Y + 28;
      int box_w  = 100;
      int box_h  = 72;
      int gap    = 18;
      int start_x = 14;

      /* stage definitions: label, latch pointer, which kind */
      typedef struct { const char *name; const PipelineReg *latch; int is_if; } StageDef;
      StageDef stages[5] = {
          { "IF",  NULL,           1 },
          { "ID",  &p->IF_ID,      0 },
          { "EX",  &p->ID_EX,      0 },
          { "MEM", &p->EX_MEM,     0 },
          { "WB",  &p->MEM_WB,     0 },
      };

      for (int i = 0; i < 5; i++) {
          int x = start_x + i * (box_w + gap);

          /* pick colors */
          Color bg, bd, tx;
          const PipelineReg *latch = stages[i].latch;

          if (stages[i].is_if) {
              /* IF: active when we haven't finished fetching */
              if (!p->fetching_done) {
                  bg = COL_FETCH_BG; bd = COL_FETCH_BD; tx = COL_FETCH_TX;
              } else {
                  bg = COL_EMPTY_BG; bd = COL_EMPTY_BD; tx = COL_EMPTY_TX;
              }
          } else if (latch && latch->valid) {
              if (p->stall && i == 1) { /* ID stalled */
                  bg = COL_STALL_BG; bd = COL_STALL_BD; tx = COL_STALL_TX;
              } else if (p->branch_taken && (i == 1 || i == 2)) { /* flushed */
                  bg = COL_FLUSH_BG; bd = COL_FLUSH_BD; tx = COL_FLUSH_TX;
              } else {
                  bg = COL_ACT_BG; bd = COL_ACT_BD; tx = COL_ACT_TX;
              }
          } else {
              bg = COL_EMPTY_BG; bd = COL_EMPTY_BD; tx = COL_EMPTY_TX;
          }

          /* box fill + border */
          DrawRectangle(x, base_y, box_w, box_h, bg);
          DrawRectangleLines(x, base_y, box_w, box_h, bd);

          /* stage name */
          DrawText(stages[i].name, x + box_w/2 - MeasureText(stages[i].name, 14)/2,
                   base_y + 8, 14, tx);

          /* instruction detail */
          if (latch && latch->valid) {
              DrawText(latch->mnemonic,
                       x + box_w/2 - MeasureText(latch->mnemonic, 11)/2,
                       base_y + 30, 11, COL_TEXT);

              char detail[32] = "";
              if (i == 1) /* ID: show source operands */
                  snprintf(detail, sizeof(detail), "R%d R%d", latch->r1, latch->r2);
              else if (i == 2) /* EX: show ALU result */
                  snprintf(detail, sizeof(detail), "out=%d", (int)latch->alu_result);
              else if (i == 3 && latch->dest_reg > 0) /* MEM */
                  snprintf(detail, sizeof(detail), "R%d<-%d",
                           latch->dest_reg, (int)latch->alu_result);
              else if (i == 4 && latch->dest_reg > 0) /* WB */
                  snprintf(detail, sizeof(detail), "R%d=%d",
                           latch->dest_reg, (int)p->reg[latch->dest_reg]);
              DrawText(detail, x + box_w/2 - MeasureText(detail, 10)/2,
                       base_y + 48, 10, COL_DIM);
          } else if (stages[i].is_if && !p->fetching_done) {
              char pc_buf[16];
              snprintf(pc_buf, sizeof(pc_buf), "PC=%d", (int)p->PC);
              DrawText(pc_buf, x + box_w/2 - MeasureText(pc_buf, 10)/2,
                       base_y + 30, 10, COL_DIM);
          }

          /* arrow between boxes */
          if (i < 4) {
              int ax = x + box_w + 2;
              int ay = base_y + box_h / 2;
              DrawText("->", ax, ay - 7, 14, COL_VDIM);
          }
      }
  }
  ```

- [ ] **Step 2: Wire draw_pipeline into the draw block**

  In `main()`, inside `BeginDrawing()`/`EndDrawing()`, add after `ClearBackground`:
  ```c
  if (sim_state != SIM_IDLE) {
      draw_pipeline();
  }
  ```

  Keep the existing text draws (state, filename, cycle counter) below it.

- [ ] **Step 3: Build and verify**

  ```
  make gui
  ```

  Load `program.txt`. Step through a few cycles. Expected: 5 colored boxes appear in the upper-left. Active stages light up blue, IF stage is green when fetching, empty stages are grey. Stall turns the ID box amber. After a branch, IF/ID briefly show red (flush).

- [ ] **Step 4: Commit**

  ```bash
  git add gui.c
  git commit -m "feat: render pipeline stage boxes with color coding"
  ```

---

## Task 6: Draw the register and memory panels

**Files:**
- Modify: `gui.c` — implement `draw_registers()` and `draw_memory()`, call from main draw block

- [ ] **Step 1: Implement draw_registers()**

  Replace the stub:
  ```c
  static void draw_registers(void) {
      const Processor *cur  = &snapshots[current_cycle].cpu;
      const Processor *prev = current_cycle > 0 ? &snapshots[current_cycle-1].cpu : NULL;

      int rx = LEFT_W;
      int ry = CONTENT_Y;

      /* section header */
      DrawRectangle(rx, ry, RIGHT_W, 20, COL_PANEL);
      DrawLine(rx, ry + 20, rx + RIGHT_W, ry + 20, COL_BORDER);
      DrawText("REGISTERS", rx + 10, ry + 4, 10, COL_DIM);
      ry += 20;

      /* R0–R31 + PC in two columns */
      int col_w  = RIGHT_W / 2;
      int row_h  = 16;
      int pad    = 6;

      for (int i = 0; i < NUM_REGISTERS; i++) {
          int col  = i % 2;
          int row  = i / 2;
          int cx   = rx + col * col_w;
          int cy   = ry + row * row_h;

          int changed = prev && (cur->reg[i] != prev->reg[i]);

          if (changed)
              DrawRectangle(cx, cy, col_w, row_h, (Color){26,58,26,255});

          char name[8], val[16];
          snprintf(name, sizeof(name), "R%d", i);
          snprintf(val,  sizeof(val),  "%d", (int)cur->reg[i]);

          DrawText(name, cx + pad,       cy + 3, 10, COL_DIM);
          DrawText(val,  cx + col_w - pad - MeasureText(val, 10),
                         cy + 3, 10, changed ? COL_GREEN : COL_TEXT);
      }

      /* PC row below */
      int pc_y = ry + (NUM_REGISTERS / 2) * row_h;
      DrawRectangle(rx, pc_y, RIGHT_W, row_h, (Color){13,58,110,255});
      char pc_val[16];
      snprintf(pc_val, sizeof(pc_val), "%d", (int)cur->PC);
      DrawText("PC", rx + pad, pc_y + 3, 10, COL_BLUE);
      DrawText(pc_val, rx + RIGHT_W - pad - MeasureText(pc_val, 10),
               pc_y + 3, 10, COL_BLUE);

      /* divider */
      int div_y = pc_y + row_h + 4;
      DrawLine(rx, div_y, rx + RIGHT_W, div_y, COL_BORDER);
  }
  ```

- [ ] **Step 2: Implement draw_memory()**

  Replace the stub:
  ```c
  static void draw_memory(void) {
      const Processor *cur  = &snapshots[current_cycle].cpu;
      const Processor *prev = current_cycle > 0 ? &snapshots[current_cycle-1].cpu : NULL;

      /* position below registers */
      int regs_h = 20 + (NUM_REGISTERS / 2 + 1) * 16 + 8;
      int rx = LEFT_W;
      int ry = CONTENT_Y + regs_h;
      int rh = WIN_H - BOTTOMBAR_H - ry;

      /* section header */
      DrawRectangle(rx, ry, RIGHT_W, 20, COL_PANEL);
      DrawLine(rx, ry + 20, rx + RIGHT_W, ry + 20, COL_BORDER);
      DrawText("DATA MEMORY", rx + 10, ry + 4, 10, COL_DIM);
      ry += 20;

      int row_h = 16;
      int pad   = 6;
      int rows  = (rh - 20) / row_h;
      int drawn = 0;

      for (int addr = DATA_MEM_START; addr < MEMORY_SIZE && drawn < rows; addr++) {
          if (cur->memory[addr] == 0) continue; /* hide zeros */

          int cy = ry + drawn * row_h;
          int changed = prev && (cur->memory[addr] != prev->memory[addr]);

          if (changed)
              DrawRectangle(rx, cy, RIGHT_W, row_h, (Color){26,26,58,255});

          char addr_s[12], val_s[16];
          snprintf(addr_s, sizeof(addr_s), "%d:", addr);
          snprintf(val_s,  sizeof(val_s),  "%d", (int)cur->memory[addr]);

          DrawText(addr_s, rx + pad, cy + 3, 10, COL_DIM);
          DrawText(val_s,  rx + RIGHT_W - pad - MeasureText(val_s, 10),
                           cy + 3, 10, changed ? COL_PURPLE : COL_TEXT);
          drawn++;
      }

      if (drawn == 0) {
          DrawText("(all zeros)", rx + pad, ry + 6, 10, COL_VDIM);
      }
  }
  ```

- [ ] **Step 3: Wire both into the draw block**

  In `main()` draw block, add:
  ```c
  if (sim_state != SIM_IDLE) {
      draw_pipeline();
      draw_registers();
      draw_memory();
  }
  ```

  Draw the vertical divider between panels:
  ```c
  DrawLine(LEFT_W, CONTENT_Y, LEFT_W, WIN_H - BOTTOMBAR_H, COL_BORDER);
  ```

- [ ] **Step 4: Build and verify**

  ```
  make gui
  ```

  Load `program.txt` and step through cycles. Expected: right side shows R0–R31 + PC. After a WB cycle, changed registers highlight green. After a MOVM, the written memory address highlights purple.

- [ ] **Step 5: Commit**

  ```bash
  git add gui.c
  git commit -m "feat: render register and memory panels with change highlighting"
  ```

---

## Task 7: Title bar, forwarding banner, and event log

**Files:**
- Modify: `gui.c` — implement `draw_titlebar()`, `draw_forwarding_banner()`, `draw_event_log()`

- [ ] **Step 1: Implement draw_titlebar()**

  Replace the stub:
  ```c
  static void draw_titlebar(void) {
      DrawRectangle(0, 0, WIN_W, TITLEBAR_H, COL_PANEL);
      DrawLine(0, TITLEBAR_H, WIN_W, TITLEBAR_H, COL_BORDER);

      DrawText("FILLET-O-NEUMANN  \xc2\xb7  Pipeline Simulator",
               12, 8, 13, COL_DIM);

      if (sim_state != SIM_IDLE) {
          char badge[32];
          snprintf(badge, sizeof(badge), "Clock  %d", current_cycle);
          int bw = MeasureText(badge, 12) + 20;
          int bx = WIN_W - bw - 12;
          DrawRectangle(bx, 6, bw, 20, (Color){31,111,235,20});
          DrawRectangleLines(bx, 6, bw, 20, COL_ACT_BD);
          DrawText(badge, bx + 10, 10, 12, COL_BLUE);
      }
  }
  ```

- [ ] **Step 2: Implement draw_forwarding_banner()**

  Replace the stub (note: `y` is the top y-coordinate to render at):
  ```c
  static void draw_forwarding_banner(int y) {
      const Processor *p = &snapshots[current_cycle].cpu;

      /* detect forwarding: check if ID_EX inputs depend on EX_MEM or MEM_WB outputs */
      if (!p->ID_EX.valid) return;

      int r1 = p->ID_EX.r1;
      int r2 = p->ID_EX.r2;
      char fwd_text[64] = "";

      if (p->EX_MEM.valid && p->EX_MEM.dest_reg > 0 && p->EX_MEM.opcode != OP_MOVR) {
          if ((r1 > 0 && r1 == p->EX_MEM.dest_reg) ||
              (r2 > 0 && r2 == p->EX_MEM.dest_reg)) {
              snprintf(fwd_text, sizeof(fwd_text),
                       "FWD  EX\xe2\x86\x92ID  R%d = %d",
                       p->EX_MEM.dest_reg, (int)p->EX_MEM.alu_result);
          }
      }
      if (fwd_text[0] == '\0' && p->MEM_WB.valid && p->MEM_WB.dest_reg > 0) {
          if ((r1 > 0 && r1 == p->MEM_WB.dest_reg) ||
              (r2 > 0 && r2 == p->MEM_WB.dest_reg)) {
              int32_t val = (p->MEM_WB.opcode == OP_MOVR)
                            ? p->MEM_WB.mem_result : p->MEM_WB.alu_result;
              snprintf(fwd_text, sizeof(fwd_text),
                       "FWD  MEM\xe2\x86\x92ID  R%d = %d",
                       p->MEM_WB.dest_reg, (int)val);
          }
      }

      if (fwd_text[0] == '\0') return;

      int tw = MeasureText(fwd_text, 11) + 20;
      DrawRectangle(14, y + 2, tw, 20, (Color){13,65,157,30});
      DrawRectangleLines(14, y + 2, tw, 20, (Color){56,139,253,120});
      DrawText(fwd_text, 24, y + 6, 11, COL_BLUE);
  }
  ```

- [ ] **Step 3: Implement draw_event_log()**

  Replace the stub:
  ```c
  static void draw_event_log(int y, int h) {
      /* section header */
      DrawRectangle(0, y, LEFT_W, 20, COL_PANEL);
      DrawLine(0, y + 20, LEFT_W, y + 20, COL_BORDER);
      DrawText("EVENT LOG", 12, y + 4, 10, COL_DIM);
      y += 20; h -= 20;

      int line_h = 15;
      int pad    = 10;
      int max_lines = h / line_h;

      /* build log lines by scanning snapshots */
      typedef struct { char text[128]; Color col; } LogLine;
      static LogLine lines[MAX_CYCLES * 6];
      int total = 0;

      for (int c = 1; c <= current_cycle && c < snapshot_count; c++) {
          const Processor *cur  = &snapshots[c].cpu;
          const Processor *prev = &snapshots[c-1].cpu;

          /* cycle header */
          char hdr[32];
          snprintf(hdr, sizeof(hdr), "[%d]", c);
          snprintf(lines[total].text, 128, "%s", hdr);
          lines[total].col = COL_BLUE;
          total++;

          /* stage activity */
          const PipelineReg *latches[4] = { &cur->IF_ID, &cur->ID_EX, &cur->EX_MEM, &cur->MEM_WB };
          const char *names[4] = { "  ID", "  EX", " MEM", "  WB" };
          for (int s = 0; s < 4; s++) {
              if (latches[s]->valid) {
                  snprintf(lines[total].text, 128, "%s: %s", names[s], latches[s]->mnemonic);
                  lines[total].col = COL_DIM;
                  total++;
              }
          }

          /* register writes */
          for (int r = 1; r < NUM_REGISTERS; r++) {
              if (cur->reg[r] != prev->reg[r]) {
                  snprintf(lines[total].text, 128, "  \xe2\x86\x92 WB R%d = %d",
                           r, (int)cur->reg[r]);
                  lines[total].col = COL_GREEN;
                  total++;
              }
          }

          /* memory writes */
          for (int addr = DATA_MEM_START; addr < MEMORY_SIZE; addr++) {
              if (cur->memory[addr] != prev->memory[addr]) {
                  snprintf(lines[total].text, 128, "  \xe2\x86\x92 MEM[%d] = %d",
                           addr, (int)cur->memory[addr]);
                  lines[total].col = COL_PURPLE;
                  total++;
              }
          }

          /* branch / flush */
          if (cur->branch_taken) {
              snprintf(lines[total].text, 128, "  \xe2\x86\x92 branch taken, pipeline flushed");
              lines[total].col = COL_RED;
              total++;
          }

          /* stall */
          if (cur->stall) {
              snprintf(lines[total].text, 128, "  \xe2\x86\x92 stall inserted");
              lines[total].col = COL_AMBER;
              total++;
          }

          if (total >= MAX_CYCLES * 6 - 10) break;
      }

      /* clamp scroll */
      int max_scroll = total > max_lines ? total - max_lines : 0;
      if (log_scroll > max_scroll) log_scroll = max_scroll;

      /* mouse wheel scroll */
      float wheel = GetMouseWheelMove();
      if (wheel != 0 && GetMouseX() < LEFT_W && GetMouseY() > y) {
          log_scroll -= (int)wheel * 2;
          if (log_scroll < 0) log_scroll = 0;
          if (log_scroll > max_scroll) log_scroll = max_scroll;
      }

      /* auto-scroll to bottom when stepping forward */
      if (sim_state == SIM_RUNNING || sim_state == SIM_PAUSED)
          log_scroll = max_scroll;

      /* render visible lines */
      BeginScissorMode(0, y, LEFT_W, h);
      for (int i = log_scroll; i < total && i - log_scroll < max_lines; i++) {
          int ly = y + (i - log_scroll) * line_h + 4;
          DrawText(lines[i].text, pad, ly, 11, lines[i].col);
      }
      EndScissorMode();
  }
  ```

- [ ] **Step 4: Wire all three into the main draw block**

  Replace the current draw block in `main()`:
  ```c
  BeginDrawing();
  ClearBackground(COL_BG);

  draw_titlebar();
  DrawLine(0, CONTENT_Y, WIN_W, CONTENT_Y, COL_BORDER);
  DrawLine(LEFT_W, CONTENT_Y, LEFT_W, WIN_H - BOTTOMBAR_H, COL_BORDER);

  if (sim_state != SIM_IDLE) {
      /* left panel */
      draw_pipeline();

      int pipeline_bottom = CONTENT_Y + 20 + 28 + 72 + 8; /* header + box area */
      draw_forwarding_banner(pipeline_bottom);
      int log_top = pipeline_bottom + 28;
      draw_event_log(log_top, WIN_H - BOTTOMBAR_H - log_top);

      /* right panel */
      draw_registers();
      draw_memory();
  } else {
      DrawText("Drop a .txt assembly file onto this window to begin.",
               LEFT_W / 2 - 180, WIN_H / 2, 16, COL_DIM);
  }

  draw_bottom_bar();
  EndDrawing();
  ```

  Remove the old DrawText calls for state/filename/cycle — those are now in the proper panels.

- [ ] **Step 5: Build and verify**

  ```
  make gui
  ```

  Load `program.txt` and step through several cycles. Expected: title bar shows "Clock N" badge. Log area fills with per-cycle entries, color coded. Forwarding banner appears when a forwarding condition exists. Mouse wheel scrolls the log.

- [ ] **Step 6: Commit**

  ```bash
  git add gui.c
  git commit -m "feat: add title bar, forwarding banner, and event log"
  ```

---

## Task 8: Bottom bar with raygui buttons

**Files:**
- Modify: `gui.c` — implement `draw_bottom_bar()`, remove keyboard-only controls, wire buttons

- [ ] **Step 1: Implement draw_bottom_bar()**

  Replace the stub:
  ```c
  static void draw_bottom_bar(void) {
      int by = WIN_H - BOTTOMBAR_H;
      DrawRectangle(0, by, WIN_W, BOTTOMBAR_H, COL_PANEL);
      DrawLine(0, by, WIN_W, by, COL_BORDER);

      int y = by + 7;

      /* drop zone */
      const char *drop_label = loaded_file[0] ? loaded_file : "Drop .txt here";
      Color drop_bd = loaded_file[0] ? COL_GREEN : COL_BORDER;
      Color drop_tx = loaded_file[0] ? COL_GREEN : COL_DIM;
      DrawRectangleLines(10, y, 160, 28, drop_bd);
      DrawText(drop_label, 18, y + 8, 10, drop_tx);

      int bx = 182;

      /* back button */
      GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,  0x21262dff);
      GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,  0x8b949eff);
      GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x30363dff);
      if (GuiButton((Rectangle){bx, y, 44, 28}, "< Bk") &&
          sim_state != SIM_IDLE)
          step_back();
      bx += 50;

      /* step button */
      GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,  0x238636ff);
      GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,  0xffffffff);
      GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x238636ff);
      if (GuiButton((Rectangle){bx, y, 60, 28}, "> Step") &&
          (sim_state == SIM_READY || sim_state == SIM_PAUSED || sim_state == SIM_DONE))
          advance_cycle();
      bx += 66;

      /* play/pause button */
      const char *pp_label = (sim_state == SIM_RUNNING) ? "|| Pse" : "> Play";
      GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,  0x1f6febff);
      GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,  0xffffffff);
      GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x1f6febff);
      if (GuiButton((Rectangle){bx, y, 68, 28}, pp_label) &&
          sim_state != SIM_IDLE) {
          if (sim_state == SIM_RUNNING) {
              sim_state = SIM_PAUSED;
          } else if (sim_state == SIM_PAUSED || sim_state == SIM_READY) {
              sim_state = SIM_RUNNING;
              play_frame = 0;
          }
      }
      bx += 74;

      /* reset button */
      GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,  0x21262dff);
      GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,  0x8b949eff);
      GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x30363dff);
      if (GuiButton((Rectangle){bx, y, 48, 28}, "Rst") &&
          sim_state != SIM_IDLE)
          reset_simulation();
      bx += 58;

      /* speed selector */
      DrawText("Speed", bx, y + 8, 10, COL_DIM);
      bx += 46;

      static const char *speed_labels[] = {"1/2x", "1x", "2x", "5x"};
      for (int i = 0; i < 4; i++) {
          if (i == speed_idx) {
              GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,  0x1f6febff);
              GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,  0xffffffff);
              GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x1f6febff);
          } else {
              GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,  0x21262dff);
              GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,  0x8b949eff);
              GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, 0x30363dff);
          }
          if (GuiButton((Rectangle){bx, y, 36, 28}, speed_labels[i]))
              speed_idx = i;
          bx += 40;
      }

      /* progress */
      if (sim_state != SIM_IDLE) {
          char prog[40];
          if (sim_state == SIM_DONE)
              snprintf(prog, sizeof(prog), "Cycle %d / %d (done)",
                       current_cycle, current_cycle);
          else
              snprintf(prog, sizeof(prog), "Cycle %d", current_cycle);
          DrawText(prog, WIN_W - MeasureText(prog, 11) - 14, y + 8, 11, COL_DIM);
      }
  }
  ```

- [ ] **Step 2: Remove keyboard shortcuts (buttons are the primary control now)**

  In `main()`, remove the `IsKeyPressed` block added in Task 4. The keyboard shortcuts can remain as-is if preferred — just make sure the button actions call the same functions.

- [ ] **Step 3: Build and verify**

  ```
  make gui
  ```

  Load `program.txt`. All buttons appear in the bottom bar. Step, Back, Play/Pause, Reset all work. Speed selector highlights active speed. Progress shows current cycle. Drop zone shows filename in green once loaded.

- [ ] **Step 4: Commit**

  ```bash
  git add gui.c
  git commit -m "feat: implement bottom bar with raygui buttons and speed control"
  ```

---

## Task 9: Final polish and done-state display

**Files:**
- Modify: `gui.c` — DONE state visual, idle hint text, section dividers

- [ ] **Step 1: Add done-state overlay in the pipeline area**

  In `draw_pipeline()`, after the stage boxes, add:
  ```c
  if (sim_state == SIM_DONE) {
      const char *done = "Simulation complete";
      int tw = MeasureText(done, 14);
      DrawText(done, LEFT_W/2 - tw/2, base_y + box_h + 10, 14, COL_GREEN);
  }
  ```

- [ ] **Step 2: Show final register dump when done**

  In `draw_registers()`, after the PC row, add:
  ```c
  if (sim_state == SIM_DONE) {
      int dy = pc_y + row_h + 8;
      DrawText("Final state", rx + pad, dy, 10, COL_GREEN);
  }
  ```

- [ ] **Step 3: Build and run the full program.txt simulation**

  ```
  make gui
  ```

  Load `program.txt`. Press Play at 2× speed. Watch the simulation run to completion. Verify:
  - Pipeline stages update each cycle
  - Registers update when WB fires
  - Log accumulates all events
  - "Simulation complete" appears when done
  - Progress shows final cycle count

- [ ] **Step 4: Test with branch program**

  Load `tests/test_branch_taken.asm` (or any test from the `tests/` folder). Step through manually. Verify:
  - Branch taken shows red flush on IF/ID boxes
  - Red log entry "branch taken, pipeline flushed" appears
  - PC jumps to correct target

- [ ] **Step 5: Final commit**

  ```bash
  git add gui.c Makefile .gitignore
  git commit -m "feat: complete pipeline GUI with done state and section polish"
  ```

---

## Spec Coverage Check

| Spec requirement | Task |
|---|---|
| Split panel layout | Tasks 5, 6, 7 (left/right draw functions) |
| Pipeline stage boxes, color-coded | Task 5 |
| Forwarding banner | Task 7 |
| Event log, scrollable, color-coded | Task 7 |
| Register panel with change highlighting | Task 6 |
| Memory panel (data only, zeros hidden, writes purple) | Task 6 |
| Step forward | Task 4, 8 |
| Step backward | Task 4, 8 |
| Play/pause + speed control | Task 4, 8 |
| Drag-and-drop file load | Task 3 |
| Drop zone shows filename | Task 8 |
| Progress indicator | Task 8 |
| Title bar with clock badge | Task 7 |
| Separate `gui.exe` binary | Task 1 |
| `make gui` target | Task 1 |
| `.exe` and `raylib/` gitignored | Task 1 |
| Terminal simulator untouched | All tasks (never modify `main.c`, `pipeline.c`, `parser.c`) |
