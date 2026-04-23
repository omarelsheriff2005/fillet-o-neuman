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

    return 0;
}
