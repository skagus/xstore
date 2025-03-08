/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_littlefs.h"
#include "cli.h"
#include "uart.h"
#include "loader.h"
#include "elf.h"  // documentation: "man 5 elf"

#define printf UART_Printf

#define VERSION_STRING "0.0.1"

#define MAX_MEM_CHUNK		(16)
typedef struct _mem_chunk
{
	uint32_t address;
	uint32_t size;
	uint32_t file_offset;
} mem_chunk;


static bool verbose = true;
static mem_chunk a_mem_chunks[MAX_MEM_CHUNK];

static int read_elf_file_header(uint32_t* p_entry, FILE* elf_file)
{
	Elf32_Ehdr ehdr;
	if(1 != fread(&ehdr, sizeof(ehdr), 1, elf_file))
	{
		printf("could not read the elf file header!\n");
		return -8;
	}

	// magic numbers in header
	if((ELFMAG0 != ehdr.e_ident[EI_MAG0])
	   || (ELFMAG1 != ehdr.e_ident[EI_MAG1])
	   || (ELFMAG2 != ehdr.e_ident[EI_MAG2])
	   || (ELFMAG3 != ehdr.e_ident[EI_MAG3]))
	{
		printf("not an elf file !\n");
		return -9;
	}
	if(ELFCLASS32 != ehdr.e_ident[EI_CLASS])
	{
		printf("ERROR: not an 32 bit elf file !\n");
		return -10;
	}
	if(true == verbose)
	{
		printf("Entry point at 0x%08lX !\n", (uint32_t)ehdr.e_entry);
		// program header
		if(0 == ehdr.e_phoff)
		{
			printf("No Program header table !\n");
		}
		else
		{
			printf("Program header table at offset 0x%lX !\n", (uint32_t)ehdr.e_phoff);
			printf("program header entries are 0x%X bytes long\n", (uint16_t)ehdr.e_phentsize);
			printf("program header table has %d entries\n", (uint16_t)ehdr.e_phnum);
		}
		// section header
		if(0 == ehdr.e_shoff)
		{
			printf("No section header table !\n");
		}
		else
		{
			printf("section header table at offset 0x%lX !\n", (uint32_t)ehdr.e_shoff);
			printf("section header entries are 0x%X bytes long\n", (uint16_t)ehdr.e_shentsize);
			printf("section header table has %d entries\n", (uint16_t)ehdr.e_shnum);
		}
	}
	if(0 == ehdr.e_phoff)
	{
		printf("ERROR: does not contain a program header table !\n");
		return -11;
	}
	if(ehdr.e_phentsize != sizeof(Elf32_Phdr))
	{
		printf("ERROR: reports wrong program header table entry size !\n");
		return -12;
	}
	if(1 > ehdr.e_phnum)
	{
		printf("ERROR: contains an empty program header table !\n");
		return -13;
	}
	if((uint16_t)PN_XNUM < ehdr.e_phnum)
	{
		printf("ERROR: contains an program header table with an invalid number of entries!\n");
		return -15;
	}
	*p_entry = ehdr.e_entry;
	return ehdr.e_phnum;
}

void add_mem_chunk(int index, uint32_t target_addr, uint32_t size, uint32_t file_offset)
{
	mem_chunk* p_mem = a_mem_chunks + index;
	printf("size:%4lX  target addr: %8lX, file offset: %lX\n", size, target_addr, file_offset);
	p_mem->address = target_addr;
	p_mem->size = size;
	p_mem->file_offset = file_offset;
}

static void dump_mem(int cnt_load, FILE* elf_file)
{
	for(int i = 0; i < cnt_load; i++)
	{
		mem_chunk* p_mem = a_mem_chunks + i;
		fseek(elf_file, p_mem->file_offset, SEEK_SET);

		printf("chunk %2d size:%4lX  target addr: %8lX, file offset: %lX\n\t\t",
				i, p_mem->size, p_mem->address, p_mem->file_offset);

		int loaded = 0;
		uint8_t* dst = (uint8_t*)p_mem->address;
		if((uint32_t)dst > 0x40000000)
		{
			dst -= 0x700000;
		}
		while(loaded < p_mem->size)
		{
			int to_read = p_mem->size - loaded;
			if(to_read > 16)
			{
				to_read = 16;
			}
			fread(dst, to_read, 1, elf_file);
			dst += to_read;
			loaded += to_read;
		}
		dst = (uint8_t*)p_mem->address;
		if((uint32_t)dst > 0x40000000)
		{
			dst -= 0x700000;
		}
		printf("0x%8X : ", dst);
		for(int i = 0; i< 16; i++)
		{
			printf("%02X ", dst[i]);
		}

		printf("\nload done\n");
	}
}


static int read_program_table(int cnt_ptbl, FILE* elf_file)
{
	Elf32_Phdr program_table;
	int cnt_load = 0;
	for(int prog_entry = 0; prog_entry < cnt_ptbl; prog_entry++)
	{
		if(1 != fread(&program_table, sizeof(Elf32_Phdr), 1, elf_file))
		{
			printf("ERROR: could not read the program table !\n");
			return 8;
		}

		// check Program Table Entry
		uint32_t target_start_addr;
		if(PT_LOAD == program_table.p_type)
		{
			if(0 == program_table.p_paddr)
			{
				// physical address is 0
				// -> use virtual address
				target_start_addr = program_table.p_vaddr;
			}
			else
			{
				// physical address is valid
				// -> use it
				target_start_addr = program_table.p_paddr;
			}

			if((0 == program_table.p_memsz) || (0 == program_table.p_filesz))
			{
				// No data for that section
				continue;
			}

			//		add_mem_chunk(target_start_addr, program_table.p_filesz, program_table.p_offset);
			add_mem_chunk(cnt_load, program_table.p_vaddr, program_table.p_filesz, program_table.p_offset);
			cnt_load++;
		}
		// else not a load able segment -> skip
	}
	return cnt_load;
}

typedef int (MainFunc)(int argc, char* argv[]);
typedef MainFunc* FuncPtr;

int load(FuncPtr* entry, char* filename)
{
	FILE* elf_file;

	elf_file = fopen(filename, "r");
	if(NULL == elf_file)
	{
		printf("ERROR: could not read the elf file %s !\n", filename);
		return -7;
	}

	// read elf file header
	int cnt_ptbl = read_elf_file_header((uint32_t*)entry, elf_file);
	if(cnt_ptbl <= 0)
	{
		// something went wrong -> exit
		fclose(elf_file);
		return cnt_ptbl;
	}
	// else -> go on
	int cnt_load = read_program_table(cnt_ptbl, elf_file);
	if(cnt_load <= 0)
	{
		// something went wrong -> exit
		fclose(elf_file);
		return cnt_load;
	}
	dump_mem(cnt_load, elf_file);
	fclose(elf_file);
	return 0;
}

FuncPtr gp_entry;

void cmd_Load(uint8_t argc, char* argv[])
{
	if(argc < 2)
	{
		printf("needs file name\n");
		return;
	}
	int load_result = load(&gp_entry, argv[1]);
	if(load_result >= 0)
	{
		printf("entry point: %p\n", gp_entry);
	}
	else
	{
		printf("load error: %d\n", load_result);
	}
}

void cmd_MemDump(uint8_t argc, char* argv[])
{
	if(argc < 2)
	{
		printf("Need Address.\n");
		return;
	}

	uint32_t nAddr = CLI_GetInt(argv[1]);
	uint8_t* pAddr = (uint8_t*)nAddr;
	printf("Dumping memory at %p\n",pAddr);
	for(uint8_t nIdx = 0; nIdx < 16; nIdx++)
	{
		printf("%02X ",pAddr[nIdx]);
	}
	printf("\n");
}

void cmd_Jump(uint8_t argc, char* argv[])
{
	if(argc > 1)
	{
		gp_entry = (FuncPtr)CLI_GetInt(argv[1]);
	}
	printf("Entry point: %p\n", gp_entry);
	int ret = gp_entry(0xAA, 0xBB);
	printf("\n\nResult: %X\n", ret);
}

void Loader_Init()
{
	CLI_Register("load", cmd_Load);
	CLI_Register("jump", cmd_Jump);
	CLI_Register("dump", cmd_MemDump);
}
