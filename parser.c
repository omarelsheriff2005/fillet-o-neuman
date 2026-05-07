#include "architecture.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int get_opcode(const char *operation) {
    if (strcmp(operation, "ADD") == 0) return OP_ADD;
    if (strcmp(operation, "SUB") == 0) return OP_SUB;
    if (strcmp(operation, "MUL") == 0) return OP_MUL;
    if (strcmp(operation, "MOVI") == 0) return OP_MOVI;
    if (strcmp(operation, "JEQ") == 0) return OP_JEQ;
    if (strcmp(operation, "AND") == 0) return OP_AND;
    if (strcmp(operation, "ORI") == 0) return OP_ORI;
    if (strcmp(operation, "JMP") == 0) return OP_JMP;
    if (strcmp(operation, "LSL") == 0) return OP_LSL;
    if (strcmp(operation, "LSR") == 0) return OP_LSR;
    if (strcmp(operation, "MOVR") == 0) return OP_MOVR;
    if (strcmp(operation, "MOVM") == 0) return OP_MOVM;

    fprintf(stderr, "ERROR: Unknown operation '%s'\n", operation);
    exit(1);
}

int get_type(int opcode) {
    switch (opcode) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_AND:
        case OP_LSL:
        case OP_LSR:
            return TYPE_R;

        case OP_JMP:
            return TYPE_J;

        default:
            return TYPE_I;
    }
}

static void require_token_count(const char *operation, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr,
                "ERROR: %s expects %d tokens, got %d\n",
                operation,
                expected,
                actual);
        exit(1);
    }
}

static long parse_integer_token(const char *token, const char *field_name) {
    char *endptr;
    long value;

    if (token == NULL || *token == '\0') {
        fprintf(stderr, "ERROR: Missing %s value\n", field_name);
        exit(1);
    }

    value = strtol(token, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "ERROR: Invalid %s value '%s'\n", field_name, token);
        exit(1);
    }

    return value;
}

static void validate_register_range(int reg_number, const char *token) {
    if (reg_number < 0 || reg_number >= NUM_REGISTERS) {
        fprintf(stderr, "ERROR: Register out of range '%s'\n", token);
        exit(1);
    }
}

int parse_register(const char *token) {
    char *endptr;
    long reg_number;

    if (token == NULL || token[0] == '\0') {
        fprintf(stderr, "ERROR: Missing register token\n");
        exit(1);
    }

    if (token[0] != 'R' && token[0] != 'r') {
        fprintf(stderr, "ERROR: Expected register, got '%s'\n", token);
        exit(1);
    }

    if (!isdigit((unsigned char)token[1])) {
        fprintf(stderr, "ERROR: Invalid register '%s'\n", token);
        exit(1);
    }

    reg_number = strtol(token + 1, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "ERROR: Invalid register '%s'\n", token);
        exit(1);
    }

    validate_register_range((int)reg_number, token);
    return (int)reg_number;
}

int sign_extend(int value, int bits) {
    int sign_bit = 1 << (bits - 1);
    if (value & sign_bit) {
        value |= ~((1 << bits) - 1);
    }
    return value;
}

static void validate_signed18(long imm) {
    if (imm < -131072L || imm > 131071L) {
        fprintf(stderr, "ERROR: Immediate out of 18-bit signed range: %ld\n", imm);
        exit(1);
    }
}

static void validate_shamt(long shamt) {
    if (shamt < 0 || shamt > 0x1FFFL) {
        fprintf(stderr, "ERROR: Shift amount out of 13-bit range: %ld\n", shamt);
        exit(1);
    }
}

static void validate_j_address(long address) {
    if (address < 0 || address > 0x0FFFFFFFL) {
        fprintf(stderr, "ERROR: Jump address out of 28-bit range: %ld\n", address);
        exit(1);
    }
}

static void validate_register_field(int reg_number) {
    if (reg_number < 0 || reg_number >= NUM_REGISTERS) {
        fprintf(stderr, "ERROR: Register field out of range: %d\n", reg_number);
        exit(1);
    }
}

static int32_t encode_r(int opcode, int r1, int r2, int r3, int shamt) {
    validate_register_field(r1);
    validate_register_field(r2);
    validate_register_field(r3);
    validate_shamt(shamt);

    return ((int32_t)opcode << 28) |
           ((int32_t)r1 << 23) |
           ((int32_t)r2 << 18) |
           ((int32_t)r3 << 13) |
           (int32_t)shamt;
}

static int32_t encode_i(int opcode, int r1, int r2, int immediate) {
    validate_register_field(r1);
    validate_register_field(r2);
    validate_signed18(immediate);

    return ((int32_t)opcode << 28) |
           ((int32_t)r1 << 23) |
           ((int32_t)r2 << 18) |
           (int32_t)(immediate & 0x3FFFF);
}

static int32_t encode_j(int opcode, int address) {
    validate_j_address(address);

    return ((int32_t)opcode << 28) | (int32_t)(address & 0x0FFFFFFF);
}

int32_t parse_instruction(char *line) {
    char *tokens[5];
    int count = 0;
    int opcode;
    char *tok;

    for (tok = strtok(line, " \t\n\r");
         tok != NULL;
         tok = strtok(NULL, " \t\n\r")) {
        if (count >= 5) {
            fprintf(stderr, "ERROR: Too many tokens in instruction\n");
            exit(1);
        }
        tokens[count++] = tok;
    }

    if (count == 0) {
        return -1;
    }

    opcode = get_opcode(tokens[0]);

    switch (opcode) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_AND: {
            require_token_count(tokens[0], count, 4);
            int r1 = parse_register(tokens[1]);
            int r2 = parse_register(tokens[2]);
            int r3 = parse_register(tokens[3]);
            return encode_r(opcode, r1, r2, r3, 0);
        }

        case OP_LSL:
        case OP_LSR: {
            require_token_count(tokens[0], count, 4);
            int r1 = parse_register(tokens[1]);
            int r2 = parse_register(tokens[2]);
            int shamt = (int)parse_integer_token(tokens[3], "shift amount");
            return encode_r(opcode, r1, r2, 0, shamt);
        }

        case OP_MOVI: {
            require_token_count(tokens[0], count, 3);
            int r1 = parse_register(tokens[1]);
            int imm = (int)parse_integer_token(tokens[2], "immediate");
            return encode_i(opcode, r1, 0, imm);
        }

        case OP_JEQ:
        case OP_ORI:
        case OP_MOVR:
        case OP_MOVM: {
            require_token_count(tokens[0], count, 4);
            int r1 = parse_register(tokens[1]);
            int r2 = parse_register(tokens[2]);
            int imm = (int)parse_integer_token(tokens[3], "immediate");
            return encode_i(opcode, r1, r2, imm);
        }

        case OP_JMP: {
            require_token_count(tokens[0], count, 2);
            int address = (int)parse_integer_token(tokens[1], "jump address");
            return encode_j(opcode, address);
        }

        default:
            fprintf(stderr, "ERROR: Unhandled opcode for '%s'\n", tokens[0]);
            exit(1);
    }
}

void load_program_from_file(Processor *cpu, const char *filename) {
    FILE *file = fopen(filename, "r");
    char line[256];

    if (file == NULL) {
        fprintf(stderr, "ERROR: Could not open file '%s'\n", filename);
        exit(1);
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        int32_t instruction;

        instruction = parse_instruction(line);
        if (instruction == -1) {
            continue;
        }

        if (cpu->num_instructions >= INSTR_MEM_END + 1) {
            fprintf(stderr, "ERROR: Instruction memory overflow\n");
            fclose(file);
            exit(1);
        }

        cpu->memory[cpu->num_instructions++] = instruction;
    }

    fclose(file);
}

void print_binary32(int32_t value) {
    uint32_t x = (uint32_t)value;

    for (int i = 31; i >= 0; --i) {
        printf("%u", (x >> i) & 1u);
        if (i % 4 == 0) {
            printf(" ");
        }
    }
}

void print_loaded_instructions(const Processor *cpu) {
    printf("Loaded Instructions:\n");

    for (int i = 0; i < cpu->num_instructions; ++i) {
        printf("memory[%d] = %d | unsigned = %u | binary = ",
               i,
               cpu->memory[i],
               (uint32_t)cpu->memory[i]);
        print_binary32(cpu->memory[i]);
        printf("\n");
    }
}
