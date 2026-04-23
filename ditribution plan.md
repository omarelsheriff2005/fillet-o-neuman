# Package 2 Distribution Plan

This document divides the remaining work for Package 2, **Fillet-O-Neumann with moves on the side**, across 6 GitHub repo members.

It is based on the current project state and the strict requirements in `CSEN601_Package2_Plan.md`. It focuses only on remaining required work, expected outputs, and dependencies between members.

## Current Project Status

Implemented so far:

- Project compiles with `gcc`.
- Core files exist: `architecture.h`, `parser.c`, `pipeline.c`, `main.c`, and `Makefile`.
- The parser can load assembly instructions and encode the 12 Package 2 instructions into 32-bit memory words.
- Basic pipeline stage functions exist, but they are not yet cycle-accurate.
- `main.c` currently loads and prints encoded instructions only.

Remaining major work:

- Implement a real clock-cycle simulation loop.
- Make ID take exactly 2 cycles and EX take exactly 2 cycles.
- Enforce Package 2 pipeline timing and IF/MEM memory conflict rules.
- Add correct data hazard handling.
- Add correct JEQ/JMP flushing behavior.
- Add required per-cycle and final output printing.
- Verify final behavior with representative programs.

## Shared GitHub Collaboration Rules

- Each member works on a separate branch.
- Pull the latest shared branch before starting new work.
- Compile locally before pushing.
- Open pull requests into the shared integration branch.
- No one changes shared structs or function signatures without notifying the team.
- If a change affects another member's area, mention that dependency clearly in the pull request.
- Member 1 resolves integration conflicts and performs final compile/run checks.

## Work Distribution Table

| Member | Role | Primary Ownership | Main Output |
|---:|---|---|---|
| 1 | Integrator and Simulation Loop | Main execution flow | Complete clock-cycle simulator |
| 2 | Parser, Encoding, and Memory Rules | Instruction loading and memory correctness | Verified parser and memory helpers |
| 3 | Pipeline Stage Timing | IF, ID, EX, MEM, WB mechanics | Cycle-accurate pipeline stages |
| 4 | Data Hazard Handling | Forwarding and/or stalling | Correct dependent-instruction behavior |
| 5 | Branch and Jump Control Hazards | JEQ/JMP PC updates and flushing | Correct branch/jump behavior |
| 6 | Required Output and Final Verification | Console output and acceptance checks | Required trace and final state output |

## Shahd: Integrator And Simulation Loop

Member 1 owns the main execution flow and final integration.

Expected outputs:

- A real clock-cycle simulation loop.
- Pipeline stopping condition based on no more fetches and empty pipeline latches.
- Clock cycle counter.
- Stage execution ordering per cycle.
- Integration of all other members' work.
- Final compile/run verification.

Detailed responsibilities:

- Replace the current load-and-print-only behavior in `main.c` with full simulation execution.
- Ensure the first instruction is fetched from memory at clock cycle 1.
- Keep advancing clock cycles until there are no more instructions to fetch and no active pipeline stages.
- Coordinate stage calls so instructions do not move through multiple stages in one cycle.
- Ensure the final integrated program compiles and runs from the terminal.

Dependencies:

- Needs stage behavior from Member 3.
- Needs hazard API from Member 4.
- Needs branch/jump behavior from Member 5.
- Needs printing API from Member 6.

Done when:

- Running the program executes the pipeline instead of only printing loaded instructions.
- The simulator stops naturally when the pipeline is empty.
- The final integrated code compiles cleanly.

## Hossam: Parser, Encoding, And Memory Rules

Member 2 owns instruction loading, instruction encoding, and memory correctness.

Expected outputs:

- Confirm all 12 instructions encode correctly.
- Confirm parser stores only encoded 32-bit instructions.
- Confirm no decoded metadata is carried from parser into execution.
- Validate instruction memory limit `0-1023`.
- Provide memory read/write helpers for main memory.
- Enforce R0 and memory behavior required by Package 2 where relevant.

Detailed responsibilities:

- Review the current parser against the Package 2 instruction table.
- Verify R-format, I-format, and J-format bit layouts.
- Verify `MOVI` encodes R2 as `0`.
- Verify `LSL` and `LSR` encode R3 as `0`.
- Verify signed immediates are encoded as 18-bit two's-complement values.
- Add safe memory read/write helpers if the pipeline needs a shared memory access interface.
- Ensure data memory access uses valid main memory addresses.

Dependencies:

- Must coordinate memory write reporting with Member 6.
- Must coordinate load/store memory access with Member 3.

Done when:

- Parser verification is documented in the pull request.
- Load/store helpers, if added, are used consistently by the pipeline.
- Parser still stores only raw encoded instructions in memory.

## Lina: Pipeline Stage Timing

Member 3 owns the mechanics of IF, ID, EX, MEM, and WB.

Expected outputs:

- IF fetches starting at clock cycle 1, then every 2 cycles.
- ID takes exactly 2 cycles.
- EX takes exactly 2 cycles.
- MEM takes 1 cycle.
- WB takes 1 cycle.
- All instructions pass through all 5 stages.
- Latches hold instructions correctly between cycles.
- No instruction falls through multiple stages in one cycle.

Detailed responsibilities:

- Update pipeline latch behavior so each instruction remains in ID for 2 cycles.
- Update execute behavior so each instruction remains in EX for 2 cycles.
- Ensure MEM and WB each take 1 cycle.
- Preserve the fetch-time PC inside the instruction state for branch/jump handling.
- Keep all instruction state available for printing.
- Ensure instructions that do not use a stage still pass through that stage.

Dependencies:

- Needs encoded instruction format from Member 2.
- Must expose stage state to Member 6 for printing.
- Must call hazard logic from Member 4.
- Must call branch/jump logic from Member 5.

Done when:

- A single instruction follows the expected 7-cycle path: IF, ID, ID, EX, EX, MEM, WB.
- Multiple instructions overlap according to the Package 2 timing pattern.
- Stage state is stable enough for Member 6 to print.

## Som3a: Data Hazard Handling

Member 4 owns data hazard detection and correction.

Expected outputs:

- Detect data hazards between dependent instructions.
- Implement forwarding or stalling as required.
- Handle `MOVR` load-use cases correctly.
- Ensure R0 always behaves as 0 during forwarding.
- Provide clear expected behavior for dependent instruction tests.

Detailed responsibilities:

- Identify when an instruction needs a value produced by an earlier instruction still in the pipeline.
- Add forwarding paths or stalls so dependent instructions receive correct values.
- Make sure forwarding never changes R0 behavior.
- Verify back-to-back arithmetic dependencies.
- Verify `MOVR` followed by an instruction using the loaded register.

Dependencies:

- Needs accurate pipeline timing from Member 3.
- Needs register writeback behavior from Member 3.
- Must expose hazard events to Member 6 for printing.

Done when:

- Dependent instructions compute correct results.
- Load-use cases compute correct results.
- Any forwarding or stalling event can be shown in the output.

## Elsh: Branch And Jump Control Hazards
Member 5 owns JEQ/JMP PC updates and flushing.

Expected outputs:

- JEQ target uses branch instruction address + 1 + immediate.
- JMP target uses PC top 4 bits concatenated with 28-bit address.
- PC updates after the second EX cycle.
- Instructions after a taken JEQ or JMP are flushed/dropped.
- Flushed instructions do not update registers or memory.
- Not-taken JEQ continues normally.

Detailed responsibilities:

- Use the fetch-time PC of the branch instruction for JEQ target calculation.
- Update PC only after JEQ/JMP completes its second EX cycle.
- Flush instructions that entered the pipeline after the taken branch or jump.
- Ensure the branch/jump instruction itself still proceeds to MEM and WB.
- Ensure not-taken JEQ does not flush following instructions.

Dependencies:

- Needs fetch-time PC carried by Member 3.
- Needs flushing hooks in pipeline latches from Member 3.
- Must expose branch/jump events to Member 6 for printing.

Done when:

- Taken JEQ jumps to the correct target and drops younger instructions.
- Not-taken JEQ continues normally.
- JMP jumps to the correct target and drops younger instructions.
- Flushed instructions produce no register or memory side effects.

## Arwa: Required Output And Final Verification

Member 6 owns console output and acceptance checks.

Expected outputs:

- Print clock cycle number after each cycle.
- Print instruction in each stage.
- Print input values and outputs for each stage.
- Print register updates with new value and stage.
- Print memory updates with new value and stage.
- Print all registers including PC after final cycle.
- Print full main memory after final cycle.
- Prepare final test programs and expected-output notes.

Detailed responsibilities:

- Add per-cycle trace output that matches the PDF requirements.
- Ensure every active stage reports the instruction it is handling.
- Ensure stage inputs and outputs are visible.
- Print register updates when WB changes a register.
- Print data memory updates when MEM stores a value.
- Print all registers after simulation ends.
- Print full main memory after simulation ends.
- Prepare simple final verification programs for arithmetic, memory, hazards, branch taken, branch not taken, and jump.

Dependencies:

- Needs stage state from Member 3.
- Needs register/memory update events from Members 2 and 3.
- Needs hazard events from Member 4.
- Needs branch/flush events from Member 5.

Done when:

- The final run output is enough to trace every clock cycle.
- The final output includes all registers and full main memory.
- Final test programs have documented expected outcomes.

## Dependency Map

```text
Member 2 -> Member 3
Parser and memory rules feed pipeline stage execution.

Member 3 -> Member 1
Cycle-accurate stages are needed before the final simulation loop can be fully correct.

Member 3 -> Member 4
Hazard handling depends on exact stage timing.

Member 3 -> Member 5
Branch and jump flushing depends on latch timing and fetch-time PC.

Members 2, 3, 4, 5 -> Member 6
Printing depends on stage state, memory/register changes, hazards, and branch events.

Members 3, 4, 5, 6 -> Member 1
Final integration depends on all subsystem outputs.
```

## Integration Order

1. Member 2 verifies parser and memory helpers.
2. Member 3 completes cycle-accurate stage timing.
3. Member 1 connects the simulation loop to stage functions.
4. Member 6 adds per-cycle and final output.
5. Member 4 adds hazard handling.
6. Member 5 adds branch/jump flushing.
7. Member 1 performs final integration and compile/run checks.
8. Member 6 verifies final output against Package 2 requirements.

## Final Acceptance Checklist

- Program starts execution by fetching the first instruction at clock cycle 1.
- All instructions pass through IF, ID, EX, MEM, and WB.
- ID lasts exactly 2 cycles.
- EX lasts exactly 2 cycles.
- MEM and WB last exactly 1 cycle each.
- IF and MEM are never active in parallel.
- R0 remains 0 in all cases.
- Parser stores only encoded instructions in memory.
- Data hazards are handled by forwarding or stalling.
- JEQ uses branch instruction address + 1 + immediate.
- JMP uses PC top 4 bits concatenated with address.
- Taken JEQ and JMP flush younger instructions.
- Flushed instructions do not affect registers or memory.
- Not-taken JEQ continues normally.
- Each clock cycle prints stage activity, inputs, and outputs.
- Register and memory updates are printed when they happen.
- Final output prints all registers including PC.
- Final output prints full main memory.
- Project compiles cleanly from the terminal.
