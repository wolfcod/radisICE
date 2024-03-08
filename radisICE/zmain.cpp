#include <stdio.h>
#include <inttypes.h>
#include <Zydis/Zydis.h>
#include <tclap/CmdLine.h>

int zmain(const uint8_t *base, size_t image_size, const uint8_t *pc, const uint8_t* image_base)
{
    const uint8_t* max = (base + image_size);

    // Initialize decoder context
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    // Initialize formatter. Only required when you actually plan to do instruction
    // formatting ("disassembling"), like we do here
    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // Loop over the instructions in our buffer.
    // The runtime-address (instruction pointer) is chosen arbitrary here in order to better
    // visualize relative addressing
    ZyanU64 runtime_address = (ZyanU64)image_base;
    ZyanUSize offset = (ZyanUSize)(pc - base);
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, base + offset, image_size - offset,
        &instruction, operands)))
    {
        // Print current instruction pointer.
        printf("%016" PRIX64 "  ", runtime_address);

        // Format & print the binary instruction structure to human-readable format
        char buffer[256];
        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
            instruction.operand_count_visible, buffer, sizeof(buffer), runtime_address, ZYAN_NULL);
        puts(buffer);

        offset += instruction.length;
        runtime_address += instruction.length;
    }

    return 0;
}
