/**
 * @file fuzz_json_parser.c
 * @brief Fuzz Target: json_parser
 * 
 * Build: clang -fsanitize=fuzzer,address -o fuzz_json_parser fuzz_json_parser.c
 * Run: ./fuzz_json_parser corpus_json_parser/
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Fuzz Target: json_parser
// ============================================================================

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Size limits
    if (size < 4 || size > 10 * 1024 * 1024) return 0;
    
    // TODO: Implement json_parser fuzzing logic
    // This is a stub - replace with actual implementation
    
    // Example invariant checks:
    // if (result && !is_valid(result)) __builtin_trap();
    
    return 0;
}

#ifdef FUZZ_STANDALONE
int main(int argc, char** argv) {
    // For manual testing without fuzzer
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    
    int result = LLVMFuzzerTestOneInput(data, size);
    free(data);
    
    return result;
}
#endif
