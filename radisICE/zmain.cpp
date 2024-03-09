#include <stdio.h>
#include <inttypes.h>
#include <Zydis/Zydis.h>
#include <tclap/CmdLine.h>
#include <map>

typedef struct _flow_node
{
    ZyanU64 original;
    ZyanU64 real;
    ZyanU64 in_function;
    ZydisDecodedInstruction decoded;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
} flow_node;

typedef std::list<flow_node> function_list;

typedef struct _flow_function
{
    std::list<flow_node> instructions;
    std::list<ZyanU64> x_ref;
} flow_function;


static bool is_conditional_jmp(const ZydisDecodedInstruction& instr)
{
    ZydisMnemonic mnemonic = instr.mnemonic;

    switch (mnemonic)
    {
    case ZYDIS_MNEMONIC_JCXZ:
    case ZYDIS_MNEMONIC_JECXZ:
    case ZYDIS_MNEMONIC_JKNZD:
    case ZYDIS_MNEMONIC_JKZD:
    case ZYDIS_MNEMONIC_JL:
    case ZYDIS_MNEMONIC_JLE:
    case ZYDIS_MNEMONIC_JNB:
    case ZYDIS_MNEMONIC_JNBE:
    case ZYDIS_MNEMONIC_JNL:
    case ZYDIS_MNEMONIC_JNLE:
    case ZYDIS_MNEMONIC_JNO:
    case ZYDIS_MNEMONIC_JNP:
    case ZYDIS_MNEMONIC_JNS:
    case ZYDIS_MNEMONIC_JNZ:
    case ZYDIS_MNEMONIC_JO:
    case ZYDIS_MNEMONIC_JP:
    case ZYDIS_MNEMONIC_JRCXZ:
    case ZYDIS_MNEMONIC_JS:
    case ZYDIS_MNEMONIC_JZ:
        return true;
        break;
    default:
        break;
    }
    return false;
}

static bool is_jmp(const ZydisDecodedInstruction& instr)
{
    if (instr.mnemonic == ZYDIS_MNEMONIC_JMP)
        return true;

    return false;
}

static bool break_flow(const ZydisDecodedInstruction& instr)
{
    if (instr.mnemonic == ZYDIS_MNEMONIC_RET)
        return true;
    if (instr.mnemonic == ZYDIS_MNEMONIC_INT3)
        return true;
    if (instr.mnemonic == ZYDIS_MNEMONIC_JMP && instr.length == 6)
        return true;
        
    if (instr.mnemonic == ZYDIS_MNEMONIC_INT)   // cannot be.. but it's a legacy int 29?
        return true;

    return false;
}

static ZyanU64 find_real_pc(ZyanU64 pc, const uint8_t* ImageBuffer, const size_t ImageSize)
{
    static std::map<ZyanU64, ZyanU64> translated;

    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    if (translated.find(pc) != translated.end())
    {
        return translated.at(pc);
    }

    //if (ImageSize < pc)
    //{
    //    return pc;
    //}
    //if (ImageSize > pc)
    //{
    //    return pc;
    //}

    bool first = true;
    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, ImageBuffer + pc, ImageSize - pc, &instruction, operands)))
    {
        if (is_jmp(instruction) && instruction.length == 5 && first == true)
        {
            first = false;
            ZyanU64 dst = pc + instruction.raw.imm->value.s + instruction.length;
            translated.insert({ pc, dst });
            pc = dst;
        }
        else
            break;
    }

    return pc;
}

bool xref_already_present(std::list<ZyanU64>& x_ref, ZyanU64 offset)
{
    for (ZyanU64 xref : x_ref)
    {
        if (xref == offset)
            return true;
    }
    return false;
}

std::map<ZyanU64, bool> visited_pc;

flow_function fetch_routine(const uint8_t* ImageBuffer, const size_t ImageSize, ZyanU64 offset, ZyanU64 ImageBase)
{
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    ZyanU64 runtime_address;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    std::map<ZyanU64, bool> rel_jmp;
    bool ignoreJmp = true;

    flow_function flow;
    runtime_address = 0;

_disassemble:
    while (true)
    {
        /*if (offset > (ZyanU64)(ImageSize))
            break;*/
        flow_node node;
        node.original = offset + ImageBase;
        node.in_function = runtime_address;
        ZyanU64 pc = find_real_pc(offset, ImageBuffer, ImageSize);
        offset = pc;

        ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, ImageBuffer + offset, ImageSize - offset, &instruction, operands));

        if (visited_pc.find(pc) != visited_pc.end())
        {
            break;
        }

        visited_pc.insert({ pc, true });
        ZyanU64 next_pc = offset + instruction.length;

        if (is_jmp(instruction))
        {
            offset = next_pc + instruction.raw.imm->value.s;

            if (rel_jmp.find(offset) == rel_jmp.end())
            {
                rel_jmp.insert({ offset, false });
                //offset = dst;
                //continue;
            }
            break;
        }


        memcpy(&node.decoded, &instruction, sizeof(instruction));
        memcpy(&node.operands, &operands, sizeof(operands));
        node.real = offset + ImageBase;

        flow.instructions.push_back(node);

        if (instruction.opcode == 0xe8)
        {
            ZyanU64 call_offset = next_pc + instruction.raw.imm->value.s;

            if (!xref_already_present(flow.x_ref, call_offset))
                flow.x_ref.push_back(call_offset);
        }

        if (is_conditional_jmp(instruction))    // conditional jmp
        {
            ZyanU64 dst = next_pc + instruction.raw.imm->value.s;// +instruction.length;

            if (rel_jmp.find(dst) == rel_jmp.end())
            {
                rel_jmp.insert({ dst, false });
                //offset = dst;
                //continue;
            }
        }

        if (instruction.opcode == 0xeb)
        {
            //ignoreJmp = true;
            ZyanU64 dst = offset + instruction.raw.imm->value.s;// +instruction.length;
            offset = dst;
            continue;
        }

        if (break_flow(instruction))
            break;

        ignoreJmp = true;
        runtime_address += instruction.length;
        offset = next_pc;
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
            //ignoreJmp
            goto _disassemble;
        }
    }

    return flow;
}

int zmain(int nested, const uint8_t* ImageBuffer, size_t ImageSize, const uint8_t* pc, const uint8_t* image_base)
{
    ZyanUSize offset = (ZyanUSize)(pc - ImageBuffer);

    //for (ZyanU64 addr : routine_covered)
    //{
    //    if (addr == offset)
    //        return 0;
    //}

    //routine_covered.push_back(offset);

    const uint8_t* max = (ImageBuffer + ImageSize);

   
    flow_function function = fetch_routine(ImageBuffer, ImageSize, offset, (ZyanU64)image_base);

    // Initialize formatter. Only required when you actually plan to do instruction
    // formatting ("disassembling"), like we do here
    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // Loop over the instructions in our buffer.
    // The runtime-address (instruction pointer) is chosen arbitrary here in order to better
    // visualize relative addressing
    int coverage = 0;

    char* block_nested = (char*)malloc(nested + 1);
    block_nested[nested + 1] = 0;
    for (int i = 0; i < nested; i++)
    {
        block_nested[i] = '>';
        block_nested[i + 1] = 0;
    }

    for (flow_node &node : function.instructions)
    {
        printf(block_nested);
        printf(" %016" PRIX64 "  %016" PRIX64 "  ", node.in_function, node.original);
        char buffer[256];

        ZydisFormatterFormatInstruction(&formatter, &node.decoded, node.operands,
            node.decoded.operand_count_visible, buffer, sizeof(buffer),
            node.real,
            ZYAN_NULL);
        
        puts(buffer);

        coverage++;
    }
    
    int count = 0;

    while (function.x_ref.begin() != function.x_ref.end())
    {
        auto first = *(function.x_ref.begin());
        function.x_ref.pop_front();
        printf("[%d] ----------------------- %08x -------------------\n", count++, first);

        if (nested == 1)
        {

        }
        coverage += zmain(nested + 1, ImageBuffer, ImageSize, (const uint8_t*)(ImageBuffer + first), image_base);

        printf("----------------------- %08x -------------------\n", first);

    }

    return coverage;
}

