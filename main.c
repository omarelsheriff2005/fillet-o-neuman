#include "architecture.h"
#include <stdio.h>

int main(int argc, char **argv) {
    const char *program_path = "program.txt";
    Processor cpu;

    if (argc > 1) {
        program_path = argv[1];
    }

    initialize_processor(&cpu);
    load_program_from_file(&cpu, program_path);

    print_loaded_instructions(&cpu);

    printf("\nStarting Pipeline Simulation...\n");

    while (!pipeline_empty(&cpu)) {
        pipeline_cycle(&cpu);
        print_pipeline_state(&cpu);
    }

    printf("\nPipeline Simulation Finished.\n");

    printf("\nFinal Registers:\n");
    for (int i = 0; i < NUM_REGISTERS; i++) {
        printf("R%d = %d\n", i, cpu.reg[i]);
    }

    printf("\nFinal Memory:\n");
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (cpu.memory[i] != 0) {
            printf("mem[%d] = %d\n", i, cpu.memory[i]);
        }
    }

    return 0;
}