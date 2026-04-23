#include "architecture.h"

#include <stdint.h>
#include <string.h>

void initialize_processor(Processor *cpu) {
    memset(cpu, 0, sizeof(*cpu));
}

void fetch(Processor *cpu) {
    if (cpu->PC > INSTR_MEM_END || cpu->PC >= cpu->num_instructions) {
        cpu->IF_ID.valid = 0;
        return;
    }

    cpu->IF_ID.instruction = cpu->memory[cpu->PC];
    cpu->IF_ID.pc_at_fetch = cpu->PC;
    cpu->IF_ID.valid = 1;

    cpu->PC++;
}

void decode(Processor *cpu) {
    int instr;
    int op;

    if (cpu->IF_ID.valid == 0) {
        cpu->ID_EX.valid = 0;
        return;
    }

    cpu->ID_EX.instruction = cpu->IF_ID.instruction;
    cpu->ID_EX.pc_at_fetch = cpu->IF_ID.pc_at_fetch;
    cpu->ID_EX.valid = 1;

    instr = cpu->IF_ID.instruction;
    cpu->ID_EX.opcode = (instr >> 28) & 0xF;
    cpu->ID_EX.type = get_type(cpu->ID_EX.opcode);
    cpu->ID_EX.r1 = (instr >> 23) & 0x1F;
    cpu->ID_EX.r2 = (instr >> 18) & 0x1F;
    cpu->ID_EX.r3 = (instr >> 13) & 0x1F;
    cpu->ID_EX.shamt = instr & 0x1FFF;
    cpu->ID_EX.imm = sign_extend(instr & 0x3FFFF, 18);
    cpu->ID_EX.address = instr & 0x0FFFFFFF;

    cpu->ID_EX.val1 = cpu->reg[cpu->ID_EX.r1];
    cpu->ID_EX.val2 = cpu->reg[cpu->ID_EX.r2];

    op = cpu->ID_EX.opcode;
    if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_AND) {
        cpu->ID_EX.dest_reg = cpu->ID_EX.r3;
    } else if (op == OP_MOVI || op == OP_ORI || op == OP_LSL ||
               op == OP_LSR || op == OP_MOVR) {
        cpu->ID_EX.dest_reg = cpu->ID_EX.r1;
    } else {
        cpu->ID_EX.dest_reg = -1;
    }
}

void execute(Processor *cpu) {
    if (cpu->ID_EX.valid == 0) {
        cpu->EX_MEM.valid = 0;
        return;
    }

    cpu->EX_MEM.valid = 1;
    cpu->EX_MEM.opcode = cpu->ID_EX.opcode;
    cpu->EX_MEM.dest_reg = cpu->ID_EX.dest_reg;
    cpu->EX_MEM.val1 = cpu->ID_EX.val1;

    switch (cpu->ID_EX.opcode) {
        case OP_ADD:
            cpu->EX_MEM.alu_result = cpu->ID_EX.val1 + cpu->ID_EX.val2;
            break;
        case OP_SUB:
            cpu->EX_MEM.alu_result = cpu->ID_EX.val1 - cpu->ID_EX.val2;
            break;
        case OP_MUL:
            cpu->EX_MEM.alu_result = cpu->ID_EX.val1 * cpu->ID_EX.val2;
            break;
        case OP_MOVI:
            cpu->EX_MEM.alu_result = cpu->ID_EX.imm;
            break;
        case OP_JEQ:
            if (cpu->ID_EX.val1 == cpu->ID_EX.val2) {
                cpu->PC = cpu->ID_EX.pc_at_fetch + 1 + cpu->ID_EX.imm;
            }
            break;
        case OP_AND:
            cpu->EX_MEM.alu_result = cpu->ID_EX.val1 & cpu->ID_EX.val2;
            break;
        case OP_ORI:
            cpu->EX_MEM.alu_result = cpu->ID_EX.val2 | cpu->ID_EX.imm;
            break;
        case OP_JMP: {
            int pc_top4 = (cpu->ID_EX.pc_at_fetch + 1) & 0xF0000000;
            cpu->PC = pc_top4 | cpu->ID_EX.address;
            break;
        }
        case OP_LSL:
            cpu->EX_MEM.alu_result = cpu->ID_EX.val2 << cpu->ID_EX.shamt;
            break;
        case OP_LSR:
            cpu->EX_MEM.alu_result = (int32_t)((uint32_t)cpu->ID_EX.val2 >> cpu->ID_EX.shamt);
            break;
        case OP_MOVR:
        case OP_MOVM:
            cpu->EX_MEM.alu_result = cpu->ID_EX.val2 + cpu->ID_EX.imm;
            break;
        default:
            break;
    }
}

void memory_stage(Processor *cpu) {
    if (cpu->EX_MEM.valid == 0) {
        cpu->MEM_WB.valid = 0;
        return;
    }

    cpu->MEM_WB.valid = 1;
    cpu->MEM_WB.opcode = cpu->EX_MEM.opcode;
    cpu->MEM_WB.dest_reg = cpu->EX_MEM.dest_reg;
    cpu->MEM_WB.alu_result = cpu->EX_MEM.alu_result;

    if (cpu->EX_MEM.opcode == OP_MOVR) {
        if (cpu->EX_MEM.alu_result >= 0 && cpu->EX_MEM.alu_result < MEMORY_SIZE) {
            cpu->MEM_WB.mem_result = cpu->memory[cpu->EX_MEM.alu_result];
        }
    } else if (cpu->EX_MEM.opcode == OP_MOVM) {
        if (cpu->EX_MEM.alu_result >= 0 && cpu->EX_MEM.alu_result < MEMORY_SIZE) {
            cpu->memory[cpu->EX_MEM.alu_result] = cpu->EX_MEM.val1;
        }
    }
}

void writeback(Processor *cpu) {
    if (cpu->MEM_WB.valid == 0) {
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
}
