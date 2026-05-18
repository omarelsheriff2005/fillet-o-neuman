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

static const char *opcode_name(int opcode) {
    switch (opcode) {
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_MOVI: return "MOVI";
        case OP_JEQ: return "JEQ";
        case OP_AND: return "AND";
        case OP_ORI: return "ORI";
        case OP_JMP: return "JMP";
        case OP_LSL: return "LSL";
        case OP_LSR: return "LSR";
        case OP_MOVR: return "MOVR";
        case OP_MOVM: return "MOVM";
        default: return "UNKNOWN";
    }
}

static const char *type_name(int type) {
    switch (type) {
        case TYPE_R: return "R";
        case TYPE_I: return "I";
        case TYPE_J: return "J";
        default: return "UNKNOWN";
    }
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

static int detect_hazard(Processor *cpu, PipelineReg *reg) {
    int op = reg->opcode;
    int src_r1 = -1;
    int src_r2 = -1;

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
        case OP_ORI:
            src_r2 = reg->r2;
            break;

        case OP_MOVR:
            src_r2 = reg->r2;
            break;

        case OP_MOVM:
            src_r1 = reg->r1; 
            src_r2 = reg->r2;  
            break;

        default:
            break;
    }


    if (cpu->ID_EX.valid && cpu->ID_EX.opcode == OP_MOVR &&
        cpu->ID_EX.dest_reg > 0) {
        if ((src_r1 > 0 && src_r1 == cpu->ID_EX.dest_reg) ||
            (src_r2 > 0 && src_r2 == cpu->ID_EX.dest_reg)) {
            printf("Hazard detected: load-use dependency on R%d in ID/EX. Stalling ID stage.\n",
                   cpu->ID_EX.dest_reg);
            return 1;
        }
    }

    if (cpu->EX_MEM.valid && cpu->EX_MEM.opcode == OP_MOVR &&
        cpu->EX_MEM.dest_reg > 0) {
        if ((src_r1 > 0 && src_r1 == cpu->EX_MEM.dest_reg) ||
            (src_r2 > 0 && src_r2 == cpu->EX_MEM.dest_reg)) {
            printf("Hazard detected: load-use dependency on R%d in EX/MEM. Stalling ID stage.\n",
                   cpu->EX_MEM.dest_reg);
            return 1;
        }
    }

    return 0;
}

static void forward_operands(Processor *cpu, PipelineReg *reg) {
    if (cpu->EX_MEM.valid && cpu->EX_MEM.dest_reg > 0) {
        if (cpu->EX_MEM.opcode != OP_MOVR) {
            if (cpu->EX_MEM.dest_reg == reg->r1) {
                reg->val1 = cpu->EX_MEM.alu_result;
                printf("Forwarding: EX/MEM result forwarded to R%d input.\n", reg->r1);
            }

            if (cpu->EX_MEM.dest_reg == reg->r2) {
                reg->val2 = cpu->EX_MEM.alu_result;
                printf("Forwarding: EX/MEM result forwarded to R%d input.\n", reg->r2);
            }
        }
    }

    if (cpu->MEM_WB.valid && cpu->MEM_WB.dest_reg > 0) {
        int32_t forwarded_value;

        if (cpu->MEM_WB.opcode == OP_MOVR) {
            forwarded_value = cpu->MEM_WB.mem_result;
        } else {
            forwarded_value = cpu->MEM_WB.alu_result;
        }

        if (cpu->MEM_WB.dest_reg == reg->r1) {
            reg->val1 = forwarded_value;
            printf("Forwarding: MEM/WB result forwarded to R%d input.\n", reg->r1);
        }

        if (cpu->MEM_WB.dest_reg == reg->r2) {
            reg->val2 = forwarded_value;
            printf("Forwarding: MEM/WB result forwarded to R%d input.\n", reg->r2);
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

    reg->val1 = cpu->reg[reg->r1];
    reg->val2 = cpu->reg[reg->r2];

    forward_operands(cpu, reg);

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
                printf("Control update: JEQ taken. New PC = %d\n", cpu->PC);
            } else {
                printf("Control update: JEQ not taken. PC remains %d\n", cpu->PC);
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
            printf("Control update: JMP executed. New PC = %d\n", cpu->PC);
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

    printf("IF: fetched instruction from memory[%d] = 0x%08X\n",
           cpu->PC,
           (uint32_t)cpu->IF_ID.instruction);

    cpu->PC++;
}

void decode(Processor *cpu) {
    PipelineReg decoded;

    if (!cpu->IF_ID.valid) {
        return;
    }

    if (cpu->IF_ID.stage_cycles < ID_CYCLES - 1) {
        cpu->IF_ID.stage_cycles++;
        return;
    }

    if (cpu->ID_EX.valid) {
        return;
    }

    decoded = cpu->IF_ID;
    decode_fields(cpu, &decoded);

    if (detect_hazard(cpu, &decoded)) {
        cpu->stall = 1;
        cpu->IF_ID.stage_cycles = 0;
        return;
    }

    cpu->ID_EX = decoded;

    printf("ID: decoded %s instruction at PC=%d | type=%s | R1=%d R2=%d R3=%d IMM=%d SHAMT=%d ADDRESS=%d\n",
           opcode_name(cpu->ID_EX.opcode),
           cpu->ID_EX.pc_at_fetch,
           type_name(cpu->ID_EX.type),
           cpu->ID_EX.r1,
           cpu->ID_EX.r2,
           cpu->ID_EX.r3,
           cpu->ID_EX.imm,
           cpu->ID_EX.shamt,
           cpu->ID_EX.address);

    cpu->ID_EX.stage_cycles = 0;

    clear_latch(&cpu->IF_ID);
}
void execute(Processor *cpu) {
    if (!cpu->ID_EX.valid) {
        return;
    }

    if (cpu->ID_EX.stage_cycles < EX_CYCLES - 1) {
        cpu->ID_EX.stage_cycles++;
        return;
    }

    if (cpu->EX_MEM.valid) {
        return;
    }

    cpu->EX_MEM = cpu->ID_EX;
    execute_operation(cpu, &cpu->EX_MEM);

    printf("EX: executed %s at PC=%d | input1=%d input2=%d | ALU result=%d\n",
           opcode_name(cpu->EX_MEM.opcode),
           cpu->EX_MEM.pc_at_fetch,
           cpu->EX_MEM.val1,
           cpu->EX_MEM.val2,
           cpu->EX_MEM.alu_result);

    int branch_taken = (cpu->EX_MEM.opcode == OP_JMP) ||
                       (cpu->EX_MEM.opcode == OP_JEQ &&
                        cpu->EX_MEM.val1 == cpu->EX_MEM.val2);

    if (branch_taken) {
        clear_latch(&cpu->IF_ID);
        clear_latch(&cpu->ID_EX);
        cpu->fetching_done = 0;
        cpu->branch_taken = 1;
        printf("Flush: younger instructions in IF/ID and ID/EX were cleared.\n");
    }

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

        printf("MEM: MOVR loaded memory[%d] = %d\n",
               cpu->EX_MEM.alu_result,
               cpu->MEM_WB.mem_result);

    } else if (cpu->EX_MEM.opcode == OP_MOVM) {
        writeDataMemory(cpu, cpu->EX_MEM.alu_result, cpu->EX_MEM.val1);

        printf("MEM: MOVM stored value %d into memory[%d]\n",
               cpu->EX_MEM.val1,
               cpu->EX_MEM.alu_result);
    } else {
        printf("MEM: %s passed through memory stage with no data memory access.\n",
               opcode_name(cpu->EX_MEM.opcode));
    }

    cpu->MEM_WB.stage_cycles = 0;

    clear_latch(&cpu->EX_MEM);
}

void writeback(Processor *cpu) {
    if (!cpu->MEM_WB.valid) {
        return;
    }

    if (cpu->MEM_WB.dest_reg > 0) {
        int32_t value;

        if (cpu->MEM_WB.opcode == OP_MOVR) {
            value = cpu->MEM_WB.mem_result;
        } else {
            value = cpu->MEM_WB.alu_result;
        }

        cpu->reg[cpu->MEM_WB.dest_reg] = value;

        printf("WB: R%d updated to %d by %s\n",
               cpu->MEM_WB.dest_reg,
               value,
               opcode_name(cpu->MEM_WB.opcode));
    } else if (cpu->MEM_WB.dest_reg == 0) {
        printf("WB: R0 destination ignored. R0 remains 0.\n");
    } else {
        printf("WB: %s has no register writeback.\n",
               opcode_name(cpu->MEM_WB.opcode));
    }

    cpu->reg[0] = 0;

    clear_latch(&cpu->MEM_WB);
}

void pipeline_cycle(Processor *cpu) {
    cpu->clock++;
    cpu->branch_taken = 0;
    cpu->stall = 0;

   
    int mem_active_this_cycle = cpu->EX_MEM.valid;

    printf("\n================ Clock Cycle %d ================\n", cpu->clock);

    writeback(cpu);
    memory_stage(cpu);
    execute(cpu);
    decode(cpu);

    if (cpu->branch_taken) {
        printf("IF: skipped because branch/jump was taken and pipeline was flushed.\n");
    } else if (mem_active_this_cycle) {
        printf("IF: skipped because MEM is active this cycle in Von Neumann memory.\n");
    } else if (cpu->clock % 2 == 1) {
        fetch(cpu);
    } else {
        printf("IF: skipped this cycle because Package 2 fetches every 2 cycles.\n");
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
    if (!reg->valid) {
        printf("%s: empty\n", name);
        return;
    }

   
    if (strcmp(name, "IF/ID ") == 0 || strcmp(name, "IF/ID") == 0) {
        printf("%s: PC=%d | instruction=0x%08X | cycles=%d | not decoded yet\n",
               name,
               reg->pc_at_fetch,
               (uint32_t)reg->instruction,
               reg->stage_cycles);
        return;
    }

    printf("%s: PC=%d | instruction=0x%08X | opcode=%s | type=%s | cycles=%d | "
           "R1=%d R2=%d R3=%d | val1=%d val2=%d | imm=%d | shamt=%d | address=%d | "
           "ALU=%d | MEM=%d | dest=%d\n",
           name,
           reg->pc_at_fetch,
           (uint32_t)reg->instruction,
           opcode_name(reg->opcode),
           type_name(reg->type),
           reg->stage_cycles,
           reg->r1,
           reg->r2,
           reg->r3,
           reg->val1,
           reg->val2,
           reg->imm,
           reg->shamt,
           reg->address,
           reg->alu_result,
           reg->mem_result,
           reg->dest_reg);
}
void print_pipeline_state(const Processor *cpu) {
    printf("\nPipeline State After Clock Cycle %d\n", cpu->clock);
    printf("Current PC = %d\n", cpu->PC);

    print_stage("IF/ID ", &cpu->IF_ID);
    print_stage("ID/EX ", &cpu->ID_EX);
    print_stage("EX/MEM", &cpu->EX_MEM);
    print_stage("MEM/WB", &cpu->MEM_WB);
}
