/***************************************************************************************************

  Zyan Disassembler Library (Zydis)

  Original Author : Florian Bernd, Joel Hoener

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

/**
 * @file
 * Demonstrates disassembling using the decoder and formatter API.
 *
 * Compared to the disassemble API, they are a bit more complicated to use, but provide much more
 * control about the decoding and formatting process.
 */

#include <Windows.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <Zydis/Zydis.h>
#include <tclap/CmdLine.h>
#include <version.h>

#pragma comment(lib, "zycore.lib")
#pragma comment(lib, "zydis.lib")

void* MapInMemory(const char* lpFileName);
int zmain(int nested, const uint8_t* base, size_t image_size, const uint8_t* pc, const uint8_t* image_base);

int main(int argc, char* argv[])
{
    TCLAP::CmdLine cmd("radisICE", ' ', RADISICE_VERSION);

    TCLAP::ValueArg<std::string> inputArg("i", "input", "randgrid driver", true, "randgrid.sys", "string");
    cmd.add(inputArg);

    cmd.parse(argc, argv);
    
    PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER) MapInMemory(inputArg.getValue().c_str());

    PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS) ((ULONG_PTR)DosHeader + DosHeader->e_lfanew);
    
    uint8_t* pc = (uint8_t*)DosHeader;
    pc += pNtHeader->OptionalHeader.AddressOfEntryPoint;

    int coverage = zmain(0, (const uint8_t*)DosHeader, pNtHeader->OptionalHeader.SizeOfImage, pc, (const uint8_t *)pNtHeader->OptionalHeader.ImageBase);

    printf("Number of instruction fetched: %d\n", coverage);
    return 0;
}
