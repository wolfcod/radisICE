#include <stdio.h>
#include <inttypes.h>
#include <Zydis/Zydis.h>
#include <tclap/CmdLine.h>
#include <map>

int zmain(int nested, const uint8_t* base, size_t image_size, const uint8_t* pc, const uint8_t* image_base)
{
    if (nested != 0)
    {
        printf("----------------------- %08x -------------------\n", pc-base+image_base);
    }

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
    ZyanUSize offset = (ZyanUSize)(pc - base);
    ZyanU64 runtime_address = (ZyanU64)offset;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    std::map<ZyanU64, bool> rel_jmp;
    std::list<ZyanU64> rel_call;
    std::map<ZyanU64, bool> visited_pc;

_disassemble:
    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, base + offset, image_size - offset,
        &instruction, operands)))
    {
        runtime_address = offset;

        if (instruction.opcode == 0xE9)
        {   // follow the jump
            ZyanU64 dst = offset + instruction.raw.imm->value.s + instruction.length;

            if (rel_jmp.find(dst) == rel_jmp.end())
            {
                rel_jmp.insert({ dst, true });
                offset = dst;
                continue;
            }
            else
            {
                printf("Node visited\n");
                break;
            }
        }
        if (instruction.opcode == 0xe8)
        {
            ZyanU64 dst = offset + instruction.raw.imm->value.s + instruction.length;

            rel_call.push_back(dst);
        }
        // Print current instruction pointer.
        for (int i = 0; i < nested; i++)
        {
            printf(">");
        }
        printf(" %016" PRIX64 "  ", runtime_address + +(ZyanU64)image_base);

        // Format & print the binary instruction structure to human-readable format
        char buffer[256];
        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
            instruction.operand_count_visible, buffer, sizeof(buffer),
            runtime_address + (ZyanU64)image_base,
            ZYAN_NULL);
        puts(buffer);

        offset += instruction.length;

        if (instruction.mnemonic == ZYDIS_MNEMONIC_JZ)
        {
            // follow the jump
            ZyanU64 dst = offset + instruction.raw.imm->value.s;// +instruction.length;

            if (rel_jmp.find(dst) == rel_jmp.end())
            {
                rel_jmp.insert({ dst, false });
                //offset = dst;
                //continue;
            }
        }

        if (instruction.opcode == 0xeb)
        {
            ZyanU64 dst = offset + instruction.raw.imm->value.s;// +instruction.length;
            offset = dst;
            continue;
        }
        if (instruction.opcode == 0xc3)
            break;
        if (instruction.opcode == 0xcc)
            break;
        if (instruction.mnemonic == ZYDIS_MNEMONIC_JMP && instruction.length == 6)
            break;
        if (instruction.mnemonic == ZYDIS_MNEMONIC_INT)
        {   // int instruction => 29h??? similar to intcc
            break;
        }

    }
  
    while (rel_jmp.begin() != rel_jmp.end())
    {
        auto first = rel_jmp.begin();

        if (first->second == true)
        {
            rel_jmp.erase(first->first);
        }
        else
        {
            first->second = true;   // mark as visited and go back
            offset = first->first;
            goto _disassemble;
        }
    }

    if (nested != 0)
    {
        printf("-----------------------\n");
    }

    for (auto dst : rel_call)
    {
        zmain(nested + 1, base, image_size, (const uint8_t *)(base + dst), image_base);
    }
    return 0;
}
