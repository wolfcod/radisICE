#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

#define IMAGE_LAST_SECTION( ntheader ) ((PIMAGE_SECTION_HEADER)        \
    ((ULONG_PTR)(ntheader) +                                            \
     FIELD_OFFSET( IMAGE_NT_HEADERS, OptionalHeader ) +                 \
     ((ntheader))->FileHeader.SizeOfOptionalHeader   +	\
	sizeof(IMAGE_SECTION_HEADER) * ((ntheader))->FileHeader.NumberOfSections \
    ))

static void* load_file(uint8_t* src, size_t size)
{
	PIMAGE_DOS_HEADER pdos = (PIMAGE_DOS_HEADER)src;

	PIMAGE_NT_HEADERS64 pNtHeader = (PIMAGE_NT_HEADERS64)(src + pdos->e_lfanew);

	uint8_t *dst = (uint8_t *)VirtualAlloc(NULL, pNtHeader->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_READWRITE);

	memcpy(dst, src, pNtHeader->OptionalHeader.SizeOfHeaders);

	PIMAGE_SECTION_HEADER sect = IMAGE_FIRST_SECTION(pNtHeader);
	
	for (; sect < IMAGE_LAST_SECTION(pNtHeader); sect++)
	{
		printf("[%-8s] %08x %08x %08x\n", sect->Name, sect->VirtualAddress, sect->PointerToRawData, sect->SizeOfRawData);
		uint8_t* page = dst + sect->VirtualAddress;

		memcpy(page, src + sect->PointerToRawData, sect->SizeOfRawData);
	}
	return dst;
}


void *MapInMemory(const char *lpFileName)
{
	HANDLE hFile = CreateFile(lpFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("Error. Cannot open %s\n", lpFileName);
		return NULL;
	}

	DWORD dwDiskSize = GetFileSize(hFile, NULL);
	
	void* src = malloc((size_t)dwDiskSize);

	ReadFile(hFile, src, dwDiskSize, NULL, NULL);

	CloseHandle(hFile);

	return load_file((uint8_t *)src, dwDiskSize);
}