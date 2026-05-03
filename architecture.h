#ifndef ARCHITECTURE_H
#define ARCHITECTURE_H

#include <stdint.h>

#define MEMORY_SIZE    2048
#define INSTR_MEM_END  1023
#define DATA_MEM_START 1024
#define NUM_REGISTERS  32

#define OP_ADD   0
#define OP_SUB   1
#define OP_MUL   2
#define OP_MOVI  3
#define OP_JEQ   4
#define OP_AND   5
#define OP_ORI   6
#define OP_JMP   7
#define OP_LSL   8
#define OP_LSR   9
#define OP_MOVR  10
#define OP_MOVM  11

#define TYPE_R   0
#define TYPE_I   1
#define TYPE_J   2

typedef struct {
    int valid;
    int stage_cycles;
    int32_t instruction;
    int opcode;
    int type;

    int r1;
    int r2;
    int r3;

    int shamt;
    int imm;
    int address;
    int dest_reg;

    int pc_at_fetch;

    int32_t val1;
    int32_t val2;
    int32_t alu_result;
    int32_t mem_result;

    char mnemonic[8];
} PipelineReg;

typedef struct {
    int32_t memory[MEMORY_SIZE];
    int32_t reg[NUM_REGISTERS];
    int32_t PC;
    int num_instructions;
    int clock;

    PipelineReg IF_ID;
    PipelineReg ID_EX;
    PipelineReg EX_MEM;
    PipelineReg MEM_WB;

    int id_subcycle;
    int ex_subcycle;

    int fwd_ex_valid;
    int fwd_ex_dest;
    int32_t fwd_ex_value;

    int fwd_mem_valid;
    int fwd_mem_dest;
    int32_t fwd_mem_value;

    int fetching_done;
    int stall;
} Processor;

void initialize_processor(Processor *cpu);
void fetch(Processor *cpu);
void decode(Processor *cpu);
void execute(Processor *cpu);
void memory_stage(Processor *cpu);
void writeback(Processor *cpu);

int get_opcode(const char *operation);
int get_type(int opcode);
int parse_register(const char *token);
int sign_extend(int value, int bits);

int32_t parse_instruction(char *line);
void load_program_from_file(Processor *cpu, const char *filename);
void print_binary32(int32_t value);
void print_loaded_instructions(const Processor *cpu);

void pipeline_cycle(Processor *cpu);
int pipeline_empty(const Processor *cpu);
void print_pipeline_state(const Processor *cpu);


#endif
