#include "architecture.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_CYCLES 2
#define EX_CYCLES 2

static void clear_latch(PipelineReg *reg) {
    memset(reg, 0, sizeof(*reg));
    reg->dest_reg = -1;
}

int32_t readMainMemory(Processor *cpu, int address) {
    if (address < 0 || address >= MEMORY_SIZE) {
        fprintf(stderr, "ERROR: Main memory read out of range: %d\n", address);
        exit(1);
    }

    return cpu->memory[address];
}

void writeMainMemory(Processor *cpu, int address, int32_t value) {
    if (address < 0 || address >= MEMORY_SIZE) {
        fprintf(stderr, "ERROR: Main memory write out of range: %d\n", address);
        exit(1);
    }

    cpu->memory[address] = value;
}

int32_t readInstructionMemory(Processor *cpu, int address) {
    if (address < 0 || address > INSTR_MEM_END) {
        fprintf(stderr, "ERROR: Instruction memory read out of range: %d\n", address);
        exit(1);
    }

    return readMainMemory(cpu, address);
}

void writeInstructionMemory(Processor *cpu, int address, int32_t value) {
    if (address < 0 || address > INSTR_MEM_END) {
        fprintf(stderr, "ERROR: Instruction memory write out of range: %d\n", address);
        exit(1);
    }

    writeMainMemory(cpu, address, value);
}

int32_t readDataMemory(Processor *cpu, int address) {
    if (address < DATA_MEM_START || address >= MEMORY_SIZE) {
        fprintf(stderr, "ERROR: Data memory read out of range: %d\n", address);
        exit(1);
    }

    return readMainMemory(cpu, address);
}

void writeDataMemory(Processor *cpu, int address, int32_t value) {
    if (address < DATA_MEM_START || address >= MEMORY_SIZE) {
        fprintf(stderr, "ERROR: Data memory write out of range: %d\n", address);
        exit(1);
    }

    writeMainMemory(cpu, address, value);
}

/*
 * Detect data hazards and return 1 if a stall is needed, 0 otherwise.
 * 
 * Data hazards occur when:
 * 1. An instruction in ID stage needs a register value
 * 2. A previous instruction (in EX, MEM, or WB) is writing to that register
 * 3. The value is not yet available for forwarding
 * 
 * MOVR (load) is a special case: it writes the loaded value in WB stage.
 * If the current instruction needs the result of a MOVR:
 * - If MOVR is in EX stage: the loaded value isn't ready yet (in MEM stage next)
 * - If MOVR is in MEM stage: the loaded value is being loaded now, available next cycle for WB
 * We need to stall if MOVR is in EX stage and we need its destination register.
 */
static int detect_hazard(Processor *cpu, PipelineReg *reg) {
    int op = reg->opcode;
    int src_r1 = -1, src_r2 = -1;
    
    /* Determine source registers based on instruction type */
    switch (op) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_AND:
        case OP_JEQ:
            src_r1 = reg->r1;
            src_r2 = reg->r2;
            break;
        case OP_LSL:
        case OP_LSR:
            src_r1 = -1;  /* R1 is destination */
            src_r2 = reg->r2;
            break;
        case OP_MOVI:
            src_r1 = -1;  /* R1 is destination */
            src_r2 = -1;
            break;
        case OP_ORI:
            src_r1 = -1;  /* R1 is destination */
            src_r2 = reg->r2;
            break;
        case OP_MOVR:
        case OP_MOVM:
            src_r1 = -1;  /* R1 is destination (for MOVR) or source (for MOVM) */
            src_r2 = reg->r2;
            break;
        case OP_JMP:
            src_r1 = -1;
            src_r2 = -1;
            break;
    }
    
    /* Special case: MOVM reads both R1 and R2 */
    if (op == OP_MOVM) {
        src_r1 = reg->r1;
        src_r2 = reg->r2;
    }
    
    /* Check ID/EX stage for MOVR hazard.
       A load-use hazard occurs when the instruction currently in EX
       is a MOVR and the next instruction in ID depends on its destination.
       The result is not available until after the MEM stage completes.
    */
    if (cpu->ID_EX.valid && cpu->ID_EX.dest_reg > 0) {
        if (cpu->ID_EX.opcode == OP_MOVR) {
            if ((src_r1 > 0 && src_r1 == cpu->ID_EX.dest_reg) ||
                (src_r2 > 0 && src_r2 == cpu->ID_EX.dest_reg)) {
                return 1;  /* Stall needed */
            }
        }
    }
    
    return 0;  /* No stall needed */
}

/*
 * Forward operands from earlier pipeline stages if available.
 * This handles data hazards by providing the computed result early
 * instead of waiting for it to be written to the register file.
 */
static void forward_operands(Processor *cpu, PipelineReg *reg) {
    /* Forward from EX/MEM stage (ALU results available, but not MOVR loads yet) */
    if (cpu->EX_MEM.valid && cpu->EX_MEM.dest_reg > 0) {
        if (cpu->EX_MEM.dest_reg == reg->r1) {
            if (cpu->EX_MEM.opcode != OP_MOVR) {
                reg->val1 = cpu->EX_MEM.alu_result;
            }
        }
        if (cpu->EX_MEM.dest_reg == reg->r2) {
            if (cpu->EX_MEM.opcode != OP_MOVR) {
                reg->val2 = cpu->EX_MEM.alu_result;
            }
        }
    }
    
    /* Forward from MEM/WB stage (all results available) */
    if (cpu->MEM_WB.valid && cpu->MEM_WB.dest_reg > 0) {
        if (cpu->MEM_WB.dest_reg == reg->r1) {
            if (cpu->MEM_WB.opcode == OP_MOVR) {
                reg->val1 = cpu->MEM_WB.mem_result;
            } else {
                reg->val1 = cpu->MEM_WB.alu_result;
            }
        }
        if (cpu->MEM_WB.dest_reg == reg->r2) {
            if (cpu->MEM_WB.opcode == OP_MOVR) {
                reg->val2 = cpu->MEM_WB.mem_result;
            } else {
                reg->val2 = cpu->MEM_WB.alu_result;
            }
        }
    }
}

static void decode_fields(Processor *cpu, PipelineReg *reg) {
    int instr = reg->instruction;
    int op;

    reg->opcode = (instr >> 28) & 0xF;
    reg->type = get_type(reg->opcode);

    reg->r1 = (instr >> 23) & 0x1F;
    reg->r2 = (instr >> 18) & 0x1F;
    reg->r3 = (instr >> 13) & 0x1F;

    reg->shamt = instr & 0x1FFF;
    reg->imm = sign_extend(instr & 0x3FFFF, 18);
    reg->address = instr & 0x0FFFFFFF;

    /* Read operands from register file */
    reg->val1 = cpu->reg[reg->r1];
    reg->val2 = cpu->reg[reg->r2];

    /* Apply forwarding if available */
    forward_operands(cpu, reg);

    op = reg->opcode;

    /* Determine destination register */
    if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_AND) {
        reg->dest_reg = reg->r3;
    } else if (op == OP_MOVI || op == OP_ORI || op == OP_LSL ||
               op == OP_LSR || op == OP_MOVR) {
        reg->dest_reg = reg->r1;
    } else {
        reg->dest_reg = -1;  /* JEQ, JMP, MOVM don't write to registers */
    }
}

static void execute_operation(Processor *cpu, PipelineReg *reg) {
    switch (reg->opcode) {
        case OP_ADD:
            reg->alu_result = reg->val1 + reg->val2;
            break;

        case OP_SUB:
            reg->alu_result = reg->val1 - reg->val2;
            break;

        case OP_MUL:
            reg->alu_result = reg->val1 * reg->val2;
            break;

        case OP_MOVI:
            reg->alu_result = reg->imm;
            break;

        case OP_JEQ:
            if (reg->val1 == reg->val2) {
                cpu->PC = reg->pc_at_fetch + 1 + reg->imm;
            }
            break;

        case OP_AND:
            reg->alu_result = reg->val1 & reg->val2;
            break;

        case OP_ORI:
            reg->alu_result = reg->val2 | reg->imm;
            break;

        case OP_JMP: {
            int pc_top4 = (reg->pc_at_fetch + 1) & 0xF0000000;
            cpu->PC = pc_top4 | reg->address;
            break;
        }

        case OP_LSL:
            reg->alu_result = reg->val2 << reg->shamt;
            break;

        case OP_LSR:
            reg->alu_result = (int32_t)((uint32_t)reg->val2 >> reg->shamt);
            break;

        case OP_MOVR:
        case OP_MOVM:
            reg->alu_result = reg->val2 + reg->imm;
            break;

        default:
            break;
    }
}

void initialize_processor(Processor *cpu) {
    memset(cpu, 0, sizeof(*cpu));

    clear_latch(&cpu->IF_ID);
    clear_latch(&cpu->ID_EX);
    clear_latch(&cpu->EX_MEM);
    clear_latch(&cpu->MEM_WB);

    cpu->PC = 0;
    cpu->clock = 0;
    cpu->fetching_done = 0;
    cpu->stall = 0;
}

void fetch(Processor *cpu) {
    if (cpu->IF_ID.valid) {
        return;
    }

    if (cpu->PC > INSTR_MEM_END || cpu->PC >= cpu->num_instructions) {
        cpu->fetching_done = 1;
        return;
    }

    clear_latch(&cpu->IF_ID);

    cpu->IF_ID.instruction = readInstructionMemory(cpu, cpu->PC);
    cpu->IF_ID.pc_at_fetch = cpu->PC;
    cpu->IF_ID.valid = 1;
    cpu->IF_ID.stage_cycles = 0;

    cpu->PC++;
}

void decode(Processor *cpu) {
    if (!cpu->IF_ID.valid) {
        return;
    }

    /*
       ID takes exactly 2 cycles.
       Even if ID_EX is busy, the instruction is still spending time in ID,
       so we should count the ID cycle.
    */
    if (cpu->IF_ID.stage_cycles < ID_CYCLES - 1) {
        cpu->IF_ID.stage_cycles++;
        return;
    }

    /*
       After finishing the 2 ID cycles, move to EX only if ID_EX is free.
       If ID_EX is busy, keep waiting here.
    */
    if (cpu->ID_EX.valid) {
        return;
    }

    /* Stall for load-use hazards before moving the instruction forward. */
    if (detect_hazard(cpu, &cpu->IF_ID)) {
        return;
    }

    cpu->ID_EX = cpu->IF_ID;
    decode_fields(cpu, &cpu->ID_EX);

    cpu->ID_EX.stage_cycles = 0;

    clear_latch(&cpu->IF_ID);
}


void execute(Processor *cpu) {
    if (!cpu->ID_EX.valid) {
        return;
    }

    /*
       EX takes exactly 2 cycles.
       Even if EX_MEM is busy, the instruction is still spending time in EX,
       so we should count the EX cycle.
    */
    if (cpu->ID_EX.stage_cycles < EX_CYCLES - 1) {
        cpu->ID_EX.stage_cycles++;
        return;
    }

    /*
       After finishing the 2 EX cycles, move to MEM only if EX_MEM is free.
       If EX_MEM is busy, keep waiting here.
    */
    if (cpu->EX_MEM.valid) {
        return;
    }

    cpu->EX_MEM = cpu->ID_EX;
    execute_operation(cpu, &cpu->EX_MEM);

    cpu->EX_MEM.stage_cycles = 0;

    clear_latch(&cpu->ID_EX);
}


void memory_stage(Processor *cpu) {
    if (!cpu->EX_MEM.valid) {
        return;
    }

    if (cpu->MEM_WB.valid) {
        return;
    }

    cpu->MEM_WB = cpu->EX_MEM;

    if (cpu->EX_MEM.opcode == OP_MOVR) {
        cpu->MEM_WB.mem_result = readDataMemory(cpu, cpu->EX_MEM.alu_result);
    } else if (cpu->EX_MEM.opcode == OP_MOVM) {
        writeDataMemory(cpu, cpu->EX_MEM.alu_result, cpu->EX_MEM.val1);
    }

    cpu->MEM_WB.stage_cycles = 0;

    clear_latch(&cpu->EX_MEM);
}



void writeback(Processor *cpu) {
    if (!cpu->MEM_WB.valid) {
        return;
    }

    if (cpu->MEM_WB.dest_reg > 0) {
        if (cpu->MEM_WB.opcode == OP_MOVR) {
            cpu->reg[cpu->MEM_WB.dest_reg] = cpu->MEM_WB.mem_result;
        } else {
            cpu->reg[cpu->MEM_WB.dest_reg] = cpu->MEM_WB.alu_result;
        }
    }

    cpu->reg[0] = 0;

    clear_latch(&cpu->MEM_WB);
}

void pipeline_cycle(Processor *cpu) {
    cpu->clock++;

    /*
       We update backwards to prevent an instruction from passing through
       multiple stages in one clock cycle.
    */
    writeback(cpu);
    memory_stage(cpu);
    execute(cpu);
    decode(cpu);

    /*
       IF starts at clock cycle 1, then every 2 cycles:
       1, 3, 5, 7, ...
    */
    if (cpu->clock % 2 == 1) {
        fetch(cpu);
    }
}

int pipeline_empty(const Processor *cpu) {
    return cpu->fetching_done &&
           !cpu->IF_ID.valid &&
           !cpu->ID_EX.valid &&
           !cpu->EX_MEM.valid &&
           !cpu->MEM_WB.valid;
}

static void print_stage(const char *name, const PipelineReg *reg) {
    if (reg->valid) {
        printf("%s: PC=%d Instruction=0x%08X cycles=%d\n",
               name,
               reg->pc_at_fetch,
               (uint32_t)reg->instruction,
               reg->stage_cycles);
    } else {
        printf("%s: empty\n", name);
    }
}

void print_pipeline_state(const Processor *cpu) {
    printf("\nClock Cycle %d\n", cpu->clock);
    print_stage("IF/ID", &cpu->IF_ID);
    print_stage("ID/EX", &cpu->ID_EX);
    print_stage("EX/MEM", &cpu->EX_MEM);
    print_stage("MEM/WB", &cpu->MEM_WB);
}
