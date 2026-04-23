# CSEN601 Package 2 Requirements

Package: Fillet-O-Neumann with moves on the side

This file is a strict Package 2 checklist derived from the project PDF. It intentionally excludes team workflow, suggested file hierarchy, implementation schedules, optional helper APIs, and custom test plans.

## 1. Memory Architecture

- Architecture: Von Neumann.
- Main memory size: 2048 words.
- Word size: 32 bits.
- Memory is word-addressable.
- Valid memory addresses: 0 to 2047.
- Program instructions are stored in addresses 0 to 1023.
- Data is stored starting at address 1024.

## 2. Registers

- Total registers: 33.
- General-purpose registers: R1 to R31.
- Zero register: R0.
- R0 is hard-wired to 0 and cannot be overwritten.
- Program counter: PC.
- PC contains the address of the instruction being fetched/executed.
- PC is incremented during instruction fetch to point to the next instruction.
- If R0 is the destination register, the instruction must continue normally through the pipeline, but R0 must remain 0.
- Do not throw an error if R0 is the destination register.

## 3. Instruction Format

- Instruction size: 32 bits.
- Instruction types: 3.

```text
R-format: opcode[31:28] | R1[27:23] | R2[22:18] | R3[17:13] | shamt[12:0]
I-format: opcode[31:28] | R1[27:23] | R2[22:18] | immediate[17:0]
J-format: opcode[31:28] | address[27:0]
```

- Immediate values are signed two's-complement values.
- Shift amounts are the exception; they are always positive.
- Use data types that match the project description. Package 2 instructions and registers are 32-bit.
- The parser must store the encoded instruction in memory.
- The parser must not keep instruction fields as extra execution metadata.
- During decode, decode the stored instruction into all possible formats.

## 4. Instruction Set

The opcodes are 0 to 11 according to this instruction order:

| Opcode | Mnemonic | Type | Format | Operation |
|---:|---|---|---|---|
| 0 | ADD | R | ADD R1 R2 R3 | R3 = R1 + R2 |
| 1 | SUB | R | SUB R1 R2 R3 | R3 = R1 - R2 |
| 2 | MUL | R | MUL R1 R2 R3 | R3 = R1 * R2 |
| 3 | MOVI | I | MOVI R1 IMM | R1 = IMM |
| 4 | JEQ | I | JEQ R1 R2 IMM | If R1 == R2, branch |
| 5 | AND | R | AND R1 R2 R3 | R3 = R1 & R2 |
| 6 | ORI | I | ORI R1 R2 IMM | R1 = R2 \| IMM |
| 7 | JMP | J | JMP ADDRESS | PC = PC[31:28] \|\| ADDRESS |
| 8 | LSL | R | LSL R1 R2 SHAMT | R1 = R2 << SHAMT |
| 9 | LSR | R | LSR R1 R2 SHAMT | R1 = R2 >>> SHAMT |
| 10 | MOVR | I | MOVR R1 R2 IMM | R1 = MEM[R2 + IMM] |
| 11 | MOVM | I | MOVM R1 R2 IMM | MEM[R2 + IMM] = R1 |

Encoding notes:

- For MOVI, R2 is 0 in the instruction format.
- For LSL and LSR, R3 is 0 in the instruction format.
- `||` means concatenation.

## 5. Datapath

- Pipeline stages: 5.
- Stages: IF, ID, EX, MEM, WB.
- All instructions must pass through all 5 stages, even if an instruction does not need a particular stage.
- IF fetches the next instruction from main memory using PC and increments PC.
- ID decodes the instruction and reads required operands from the register file.
- EX executes the instruction. ALU operations are done in this stage.
- MEM performs required memory access. Loads read from memory; stores write to memory.
- WB writes a result back to the register file for instructions that have a destination register.

## 6. Pipeline Timing

- Maximum parallelism: 4 instructions.
- IF and MEM cannot be done in parallel because they access the same physical memory.
- At a given clock cycle, the active stage set is either IF, ID, EX, WB or ID, EX, MEM, WB.
- Number of clock cycles for normal straight-line execution: `7 + ((n - 1) * 2)`, where `n` is the number of instructions.
- Fetch an instruction every 2 clock cycles starting from clock cycle 1.
- ID takes 2 clock cycles.
- EX takes 2 clock cycles.
- MEM takes 1 clock cycle.
- WB takes 1 clock cycle.
- Do not pre-calculate total cycles and use that as the stopping condition. The cycle formula is only a debugging reference.

## 7. Program Flow

- Store parsed/encoded instructions in the instruction segment of main memory.
- Start execution by fetching the first instruction from memory at clock cycle 1.
- Continue execution according to the Package 2 datapath pattern.
- Clock cycles may be simulated with a variable incremented after the required stages for that cycle finish.

## 8. Data Hazards

- Data hazards must be handled.
- Use forwarding or stalling as needed.

## 9. Branches And Jumps

- JEQ branch target:

```text
PC = address of branch instruction + 1 + IMM
```

- The `+ 1` is the branch instruction PC increment that normally occurs during fetch.
- For conditional branch instructions, do not use the current PC value after later fetches as the branch instruction address. Use the branch instruction address.
- JMP target:

```text
PC = PC[31:28] || ADDRESS
```

- Conditional branches and jumps update PC during the execute stage.
- For Package 2, update PC after the second execute cycle of the branch/jump instruction.
- If a JEQ condition is true, or if a JMP is executed, all instructions that entered the datapath after that branch/jump instruction must be ignored and dropped.
- The branch/jump instruction itself continues to the MEM stage and then WB.
- After a Package 2 branch/jump resolves, fetch the target instruction after the required delay caused by the branch/jump finishing EX and then occupying MEM.
- If a JEQ condition is false, continue executing following instructions normally.
- Flushed instructions must not affect register values or data memory values.
- Flushing may be implemented by clearing the instruction state or by using flags.

## 10. Required Printing

After each clock cycle, print:

- Clock cycle number.
- Which instruction is being executed at each pipeline stage.
- Input parameters/values for each stage.
- Output of each stage.
- Register updates when a register value changes, including the new value and the stage where the change happened.
- Memory updates when a data memory value is stored or updated.

After the last clock cycle, print:

- The content of all registers, including PC.
- The full content of main memory.

## 11. Deliverables And Quality Notes

- The implementation must be in C.
- The implementation should be generic and well-commented.
- The submitted project should include the required report and video according to the project instructions.
