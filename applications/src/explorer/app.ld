OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_main)

SECTIONS
{
	. = 0x70000000000;
	_Start = .;
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