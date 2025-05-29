OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_main)

SECTIONS
{
	. = 0x90000000000;

	.text :
	{
		*(.text)
	}
	.data :
	{
		*(.data)
	}
	.rodata :
	{
		*(.rodata)
	}
	.bss :
	{
		*(COMMON)
		*(.bss)
	}
}