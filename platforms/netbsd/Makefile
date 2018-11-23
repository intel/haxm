S?=	/usr/src/sys

KMOD=	haxm

# toplevel
SRCS+=	components.c
SRCS+=	hax_entry.c
SRCS+=	hax_entry_hax.c
SRCS+=	hax_entry_vcpu.c
SRCS+=	hax_entry_vm.c
SRCS+=	hax_event.c
SRCS+=	hax_host_mem.c
SRCS+=	hax_mem_alloc.c
SRCS+=	hax_mm.c
SRCS+=	hax_wrapper.c

# core
.PATH: ../../core
SRCS+=	chunk.c
SRCS+=	cpu.c
SRCS+=	cpuid.c
SRCS+=	dump.c
SRCS+=	emulate.c
SRCS+=	ept.c
SRCS+=	ept2.c
SRCS+=	ept_tree.c
SRCS+=	gpa_space.c
SRCS+=	hax.c
SRCS+=	ia32.c
SRCS+=	intr_exc.c
SRCS+=	memory.c
SRCS+=	memslot.c
SRCS+=	name.c
SRCS+=	page_walker.c
SRCS+=	ramblock.c
SRCS+=	vcpu.c
SRCS+=	vm.c
SRCS+=	vmx.c
SRCS+=	vtlb.c

.if ${MACHINE} == "amd64"
TARGET_ELF=	elf64
.else
.error Not supported
.endif

.SUFFIXES: .asm .o
.PATH.asm: ../../core
.asm.o:
	nasm -f ${TARGET_ELF} -o ${.TARGET} ${.IMPSRC}

SRCS+=	emulate_ops.asm
SRCS+=	ia32_ops.asm
SRCS+=	vmx_ops.asm

.include <bsd.kmodule.mk>
