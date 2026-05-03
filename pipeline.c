#include "architecture.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ID_CYCLES 2
#define EX_CYCLES 2

static void clear_latch(PipelineReg *reg) {
    memset(reg, 0, sizeof(*reg));
    reg->dest_reg = -1;
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

    reg->val1 = cpu->reg[reg->r1];
    reg->val2 = cpu->reg[reg->r2];

    op = reg->opcode;

    if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_AND) {
        reg->dest_reg = reg->r3;
    } else if (op == OP_MOVI || op == OP_ORI || op == OP_LSL ||
               op == OP_LSR || op == OP_MOVR) {
        reg->dest_reg = reg->r1;
    } else {
        reg->dest_reg = -1;
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

    cpu->IF_ID.instruction = cpu->memory[cpu->PC];
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
        if (cpu->EX_MEM.alu_result >= 0 && cpu->EX_MEM.alu_result < MEMORY_SIZE) {
            cpu->MEM_WB.mem_result = cpu->memory[cpu->EX_MEM.alu_result];
        }
    } else if (cpu->EX_MEM.opcode == OP_MOVM) {
        if (cpu->EX_MEM.alu_result >= 0 && cpu->EX_MEM.alu_result < MEMORY_SIZE) {
            cpu->memory[cpu->EX_MEM.alu_result] = cpu->EX_MEM.val1;
        }
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