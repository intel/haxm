# HAXM API Reference
HAXM runs in kernel space as a device driver (on Windows) or kernel extension
(on macOS), and can talk directly to hardware to control the execution of
virtual machines (VMs) by means of Intel Virtual Machine Extensions (VMX), a set
of privileged Intel Architecture (IA) instructions.

Although HAXM implements the core functionality of a hypervisor (virtual machine
manager), it cannot operate as a *standalone* hypervisor: after the driver or
kext is loaded by the host OS, it just waits to serve calls from user space to
its API functions, which are in the form of IOCTLs. By using the HAXM API
properly, an application can take advantage of Intel Virtualization Technology
and run hardware-accelerated IA virtual machines without root privileges or
interaction with VMX. An example of such an application is QEMU, which includes
a HAXM accelerator module that makes full use of the HAXM API.

HAXM tries to provide the same set of IOCTLs for Windows and macOS, with the
same behavior. Any exception to that is noted in the description of the IOCTL
below.

## Versioning and Extensibility
The HAXM API is versioned. Unlike the HAXM version (which identifies a specific
HAXM kernel module release, e.g. 6.2.0), the API version is more stable and
contains only a single number, which can be obtained by an IOCTL that is
guaranteed to be available (q.v. `HAX_IOCTL_VERSION`).

API v4 is the current version, while v3 is also in prevalent use (there is
actually only a minor difference between these two). Older versions are
effectively retired, although the current HAXM kernel module still supports
them.

It is possible to extend or modify the HAXM API without upgrading the API
version, by defining a new capability flag (q.v. `HAX_IOCTL_CAPABILITY`) for the
new feature. The caller can check if the feature is supported before using it.

## List of IOCTLs
HAXM IOCTLs can be grouped into three categories based on their scope:

1. System IOCTLs: These are issued to the global `HAX` device (`\\.\HAX` on
Windows, `/dev/HAX` on macOS).
1. VM IOCTLs: These are issued to one of the HAXM VM devices
(`\\.\hax_vm`*XX* on Windows, `/dev/hax_vm/vm`*XX* on macOS, where *XX* is a VM
ID), and only affect the specified VM. All IOCTLs for the same VM should be
issued from the same process.
1. VCPU IOCTLs: These are issued to one of the HAXM VCPU devices
(`\\.\hax_vm`*XX*`_vcpu`*YY* on Windows, `/dev/hax_vm`*XX*`/vcpu`*YY* on macOS,
where *XX* is a VM ID and *YY* a VCPU ID), and only affect the specified VCPU of
the specified VM. All IOCTLs for the same VCPU should be issued from the same
thread.

### System IOCTLs
#### HAX\_IOCTL\_VERSION
Reports the API versions supported by this HAXM kernel module. Note that this
IOCTL cannot be used to obtain the HAXM kernel module version (e.g. 6.2.0).

* Since: API v1
* Parameter: `struct hax_module_version version`, where
  ```
  struct hax_module_version {
      uint32_t compat_version;
      uint32_t cur_version;
  } __attribute__ ((__packed__));
  ```
  * (Output) `compat_version`: The lowest API version supported by this HAXM
kernel module.
  * (Output) `cur_version`: The highest API version supported by this HAXM
kernel module. (0 < `compat_version` <= `cur_version`)
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The output buffer provided by the
caller is smaller than the size of `struct hax_module_version`.

#### HAX\_IOCTL\_CAPABILITY
Reports capabilities of this HAXM kernel module, which depend on the module
itself as well as the host environment.

* Since: API v1
* Parameter: `struct hax_capabilityinfo cap`, where
  ```
  struct hax_capabilityinfo {
      uint16_t wstatus;
      uint16_t winfo;
      uint32_t win_refcount;
      uint64_t mem_quota;
  } __attribute__ ((__packed__));

  #define HAX_CAP_STATUS_WORKING     (1 << 0)
  #define HAX_CAP_MEMQUOTA           (1 << 1)
  #define HAX_CAP_WORKSTATUS_MASK    0x01

  #define HAX_CAP_FAILREASON_VT      (1 << 0)
  #define HAX_CAP_FAILREASON_NX      (1 << 1)

  #define HAX_CAP_EPT                (1 << 0)
  #define HAX_CAP_FASTMMIO           (1 << 1)
  #define HAX_CAP_UG                 (1 << 2)
  #define HAX_CAP_64BIT_RAMBLOCK     (1 << 3)
  #define HAX_CAP_64BIT_SETRAM       (1 << 4)
  #define HAX_CAP_TUNNEL_PAGE        (1 << 5)
  #define HAX_CAP_DEBUG              (1 << 7)
  #define HAX_CAP_IMPLICIT_RAMBLOCK  (1 << 8)
  #define HAX_CAP_CPUID              (1 << 9)
  ```
  * (Output) `wstatus`: The first set of capability flags reported to the
caller. The following bits may be set, while others are reserved:
    * `HAX_CAP_STATUS_WORKING`: Indicates whether the host system meets HAXM's
minimum requirements. If set, HAXM is usable, and `winfo` reports additional
capabilities. Otherwise, HAXM is not usable, and `winfo` reports failed checks.
    * `HAX_CAP_MEMQUOTA`: Indicates whether the global memory cap setting is
enabled (q.v. `mem_quota`).
  * (Output) `winfo`: The second set of capability flags reported to the caller.
Valid flags depend on whether HAXM is usable (q.v. `HAX_CAP_STATUS_WORKING`). If
HAXM is not usable, the following bits may be set:
    * `HAX_CAP_FAILREASON_VT`: If set, Intel VT-x is not supported by the host
CPU (which itself may be virtualized by an underlying hypervisor, e.g. KVM or
Hyper-V), or disabled in BIOS.
    * `HAX_CAP_FAILREASON_NX`: If set, Intel Execute Disable Bit is not
supported by the host CPU, or disabled in BIOS.

    If HAXM is usable, the following bits may be set:
    * `HAX_CAP_EPT`: If set, the host CPU supports the Extended Page Tables
(EPT) feature.
    * `HAX_CAP_FASTMMIO`: If set, this HAXM kernel module supports the fast MMIO
feature. This is always the case with API v2 and later.
    * `HAX_CAP_UG`: If set, the host CPU supports the Unrestricted Guest (UG)
feature.
    * `HAX_CAP_64BIT_SETRAM`: If set, `HAX_VM_IOCTL_SET_RAM2` is available.
    * `HAX_CAP_IMPLICIT_RAMBLOCK`: If set, `HAX_VM_IOCTL_SET_RAM2` supports the
`HAX_RAM_INFO_STANDALONE` flag.
    * `HAX_CAP_CPUID`: If set, `HAX_VCPU_IOCTL_SET_CPUID` is available.
  * (Output) `win_refcount`: (Windows only)
  * (Output) `mem_quota`: If the global memory cap setting is enabled (q.v.
`HAX_IOCTL_SET_MEMLIMIT`), reports the current quota on memory allocation (the
global memory cap minus the total RAM size of all active VMs), in MB. Otherwise,
reports 0.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The output buffer provided by the
caller is smaller than the size of `struct hax_capabilityinfo`.

#### HAX\_IOCTL\_SET\_MEMLIMIT
Configures the global memory cap setting, which, if enabled, puts a limit on the
amount of RAM that can be allocated (via `HAX_VM_IOCTL_ALLOC_RAM`) by
concurrently running VMs. E.g., if the global memory cap is set to 3GB, then it
is not possible to launch 2 VMs with 2GB of RAM each (2 x 2GB = 4GB > 3GB),
because `HAX_VM_IOCTL_ALLOC_RAM` will fail for the second VM. Note that this
setting is ignored by HAXM 6.2.0 and later, where `HAX_VM_IOCTL_ALLOC_RAM` no
longer keeps track of the total RAM size of all active VMs.

Q.v. `HAX_IOCTL_CAPABILITY`.

* Since: API v1
* Parameter: `struct hax_set_memlimit limit`, where
  ```
  struct hax_set_memlimit {
      uint8_t enable_memlimit;
      uint8_t pad[7];
      uint64_t memory_limit;
  } __attribute__ ((__packed__));
  ```
  * (Input) `enable_memlimit`: Whether or not the global memory cap setting is
effective. 0 means disabled (i.e. unlimited); any other value means enabled.
  * (Input) `pad`: Ignored.
  * (Input) `memory_limit`: The global memory cap, i.e. the maximum total RAM
size of all active VMs, in MB. Ignored if `enable_memlimit == 0`.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct hax_set_memlimit`.
  * `STATUS_UNSUCCESSFUL` (Windows) or `-EINVAL` (macOS): There is at least one
VM already created, which must be destroyed before the global memory cap can be
adjusted.

#### HAX\_IOCTL\_CREATE\_VM
Creates a VM and returns its VM ID. VM IDs are managed by HAXM. Currently, HAXM
supports up to 8 active VMs.

* Since: API v1
* Parameter: `uint32_t vm_id`
  * (Output) `vm_id`: The VM ID that uniquely identifies the newly created VM.
* Error codes:
  * `STATUS_BUFFER_TOO_SMALL` (Windows): The output buffer provided by the
caller is smaller than the size of `uint32_t`.
  * `STATUS_UNSUCCESSFUL` (Windows) or `-ENOMEM` (macOS): The VM was not created
due to an internal error.

### VM IOCTLs
#### HAX\_VM\_IOCTL\_VCPU\_CREATE
Adds to this VM a VCPU with the given VCPU ID. VCPU IDs are managed by the
caller, who must ensure VCPU IDs are unique within the same VM. Currently, HAXM
supports up to 16 active VCPUs per VM.

* Since: API v1
* Parameter: `uint32_t vcpu_id`
  * (Input) `vcpu_id`: The VCPU ID that uniquely identifies the new VCPU among
the VCPUs in the same VM. Must be less than 16. Before API v3, only one VCPU was
allowed per VM, and this parameter was ignored.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `uint32_t`.
  * `STATUS_UNSUCCESSFUL` (Windows) or `-EINVAL` (macOS): The VCPU was not
created due to an internal error.

#### HAX\_VM\_IOCTL\_ALLOC\_RAM
Registers with HAXM a user space buffer to be used as memory for this VM.
Currently, HAXM does not allow mapping a guest physical address (GPA) range to a
host virtual address (HVA) range that does not belong to any previously
registered buffers.

Q.v. `HAX_VM_IOCTL_SET_RAM`.

* Since: API v1
* Parameter: `struct hax_alloc_ram_info info`, where
  ```
  struct hax_alloc_ram_info {
      uint32_t size;
      uint32_t pad;
      uint64_t va;
  } __attribute__ ((__packed__));
  ```
  * (Input) `size`: The size of the user buffer to register, in bytes. Must be
in whole pages (i.e. a multiple of 4KB), and must not be 0. Note that this IOCTL
can only handle buffers smaller than 4GB.
  * (Input) `pad`: Ignored.
  * (Input) `va`: The start address of the user buffer. Must be page-aligned
(i.e. a multiple of 4KB), and must not be 0. The HVA range specified by `va` and
`size` must not overlap with that of any previously registered user buffer for
the same VM.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct hax_alloc_ram_info`.
  * `STATUS_UNSUCCESSFUL` (Windows):
  * `-EINVAL` (macOS):
  * `-ENOMEM` (macOS):

#### HAX\_VM\_IOCTL\_ADD\_RAMBLOCK
Same as `HAX_VM_IOCTL_ALLOC_RAM`, but takes a 64-bit size instead of 32-bit.
* Since: Capability `HAX_CAP_64BIT_RAMBLOCK`
* Parameter: `struct hax_ramblock_info info`, where
  ```
  struct hax_ramblock_info {
      uint64_t start_va;
      uint64_t size;
      uint32_t reserved;
  } __attribute__ ((__packed__));
  ```
  * (Input) `start_va`: The start address of the user buffer to register. Must
be page-aligned (i.e. a multiple of 4KB), and must not be 0. The HVA range
specified by `start_va` and `size` must not overlap with that of any previously
registered user buffer for the same VM.
  * (Input) `size`: The size of the user buffer, in bytes. Must be in whole
pages (i.e. a multiple of 4KB), and must not be 0.
  * (Input) `reserved`: Reserved. Must be set to 0.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct hax_alloc_ram_info`.
  * `STATUS_UNSUCCESSFUL` (Windows):
  * `-EINVAL` (macOS):
  * `-ENOMEM` (macOS):

#### HAX\_VM\_IOCTL\_SET\_RAM
Creates, updates or removes a mapping between the guest physical address (GPA)
space of this VM and the host virtual address (HVA) space of the calling
process. When a guest physical page is mapped to a host virtual page that is
backed by a buffer in user space, any access to the former from within the VM is
translated to an access to the latter, i.e. reading or writing a user page. On
the other hand, any access to an unmapped guest physical page is treated as
memory-mapped I/O (MMIO).

Initially, the entire GPA space of a newly created VM is unmapped. To map or
remap a GPA range, the caller must first make sure the destination HVA range
is already registered with HAXM (q.v. `HAX_VM_IOCTL_ALLOC_RAM`) before invoking
this IOCTL.

(Since API v4) To remove a mapping, or to reserve a GPA range for MMIO, the
caller should invoke this IOCTL with `flags == HAX_RAM_INFO_INVALID` and
`va == 0`.

* Since: API v1
* Parameter: `struct hax_set_ram_info info`, where
  ```
  struct hax_set_ram_info {
      uint64_t pa_start;
      uint32_t size;
      uint8_t flags;
      uint8_t pad[3];
      uint64_t va;
  } __attribute__ ((__packed__));

  #define HAX_RAM_INFO_ROM     0x01
  #define HAX_RAM_INFO_INVALID 0x80
  ```
  * (Input) `pa_start`: The start address of the GPA range to map. Must be page-
aligned (i.e. a multiple of 4KB).
  * (Input) `size`: The size of the GPA range, in bytes. Must be in whole pages
(i.e. a multiple of 4KB), and must not be 0. If the GPA range covers any guest
physical pages that are already mapped, those pages will be remapped.
  * (Input) `flags`: Properties of the mapping. The following bits may be set,
while others are reserved:
    * `HAX_RAM_INFO_ROM`: If set, the GPA range will be mapped as read-only
memory (ROM).
    * `HAX_RAM_INFO_INVALID`: (Since API v4) If set, any existing mappings for
any guest physical pages in the GPA range will be removed, i.e. the GPA range
will be reserved for MMIO. This flag must not be combined with any other flags,
and its presence requires `va` to be set to 0.
  * (Input) `pad`: Ignored.
  * (Input) `va`: The start address of the HVA range to map to. Must be page-
aligned (i.e. a multiple of 4KB), and must not be 0 (except when
`flags == HAX_RAM_INFO_INVALID`). The size of the HVA range is specified by
`size`. The entire HVA range must fall within a previously registered user
buffer.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct hax_set_ram_info`, or any of the
input parameters .

#### HAX\_VM\_IOCTL\_SET\_RAM2
Same as `HAX_VM_IOCTL_SET_RAM`, but takes a 64-bit size instead of 32-bit.
* Since: Capability `HAX_CAP_64BIT_SETRAM`
* Parameter: `struct hax_set_ram_info2 info`, where
  ```
  struct hax_set_ram_info {
      uint64_t pa_start;
      uint64_t size;
      uint64_t va;
      uint32_t flags;
      uint32_t reserved1;
      uint64_t reserved2;
  } __attribute__ ((__packed__));

  #define HAX_RAM_INFO_ROM (1 << 0)
  #define HAX_RAM_INFO_STANDALONE (1 << 6)
  #define HAX_RAM_INFO_INVALID (1 << 7)
  ```
  * (Input) `pa_start`: The start address of the GPA range to map. Must be page-
aligned (i.e. a multiple of 4KB).
  * (Input) `size`: The size of the GPA range, in bytes. Must be in whole pages
(i.e. a multiple of 4KB), and must not be 0. If the GPA range covers any guest
physical pages that are already mapped, those pages will be remapped.
  * (Input) `va`: The start address of the HVA range to map to. Must be page-
aligned (i.e. a multiple of 4KB), and must not be 0 (except when
`flags == HAX_RAM_INFO_INVALID`). The size of the HVA range is specified by
`size`. The entire HVA range must fall within a previously registered user
buffer.
  * (Input) `flags`: Properties of the mapping. The following bits may be set,
while others are reserved:
    * `HAX_RAM_INFO_ROM`: If set, the GPA range will be mapped as read-only
memory (ROM).
    * `HAX_RAM_INFO_STANDALONE`: If set, the HVA range must not overlap with any
existing RAM block, and a new RAM block will be implicitly created for this
stand-alone mapping. In other words, when using this flag, the caller should not
call `HAX_VM_IOCTL_ADD_RAMBLOCK` in advance for the same HVA range. As soon as
the stand-alone mapping is destroyed (via `HAX_RAM_INFO_INVALID`), the
implicitly-created RAM block will also go away.
    * `HAX_RAM_INFO_INVALID`: (Since API v4) If set, any existing mappings for
any guest physical pages in the GPA range will be removed, i.e. the GPA range
will be reserved for MMIO. This flag must not be combined with any other flags,
and its presence requires `va` to be set to 0.
  * (Input) `reserved1`: Reserved, must be 0.
  * (Input) `reserved2`: Reserved, must be 0.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct hax_set_ram_info`, or any of the
input parameters .

#### HAX\_VM\_IOCTL\_NOTIFY\_QEMU\_VERSION
TODO: Describe

* Since: API v2
* Parameter: `struct hax_qemu_version qversion`, where
  ```
  struct hax_qemu_version {
      uint32_t cur_version;
      uint32_t least_version;
  } __attribute__ ((__packed__));
  ```
  * (Input) `cur_version`:
  * (Input) `least_version`:
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct hax_qemu_version`.

### VCPU IOCTLs
#### HAX\_VCPU\_IOCTL\_SETUP\_TUNNEL
In order to avoid the backward compatibility issue caused by that new fields
are added in `struct hax_tunnel`, it allocates one while page, instead of just
the size of `struct hax_tunnel`, to store `struct hax_tunnel` and then maps
this page to user space and kernel space seperately as the communication tunnel
between HAXM kernel and user (QEMU) modules. Meanwhile, a new capability flag
`HAX_CAP_TUNNEL_PAGE` is added for backward compatibility with low verions.

* Since: API v1
* Parameter: `struct hax_tunnel_info info`, where
  ```
  struct hax_tunnel_info {
      uint64_t va;
      uint64_t io_va;
      uint16_t size;
      uint16_t pad[3];
  } __attribute__ ((__packed__));
  ```
  * (Output) `va`:
  * (Output) `io_va`:
  * (Output) `size`:
  * (Output) `pad`: Unused.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The output buffer provided by the
caller is smaller than the size of `struct hax_tunnel_info`.

#### HAX\_VCPU\_IOCTL\_RUN
Starts running this VCPU.

* Since: API v1
* Parameter: None
* Error codes:

#### HAX\_VCPU\_IOCTL\_SET\_MSRS
TODO: Describe

* Since: API v1
* Parameter: `struct hax_msr_data msrs`, where
  ```
  struct hax_msr_data {
      uint16_t nr_msr;
      uint16_t done;
      uint16_t pad[2];
      struct vmx_msr entries[20];
  } __attribute__ ((__packed__));
  ```
  where
  ```
  #define HAX_MAX_MSR_ARRAY 0x20
  struct vmx_msr {
      uint64_t entry;
      uint64_t value;
  } __attribute__ ((__packed__));
  ```
  * (Input) `nr_msr`:
  * (Output) `done`:
  * (Input) `pad`: Ignored.
  * (Input) `entries`:
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input/output buffer provided by
the caller is smaller than the size of `struct hax_msr_data`.

#### HAX\_VCPU\_IOCTL\_GET\_MSRS
TODO: Describe

* Since: API v1
* Parameter: `struct hax_msr_data msrs` (q.v. `HAX_VCPU_IOCTL_SET_MSRS`)
  * (Input) `nr_msr`:
  * (Output) `done`:
  * (Output) `pad`: Unused.
  * (Output) `entries`:
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input/output buffer provided by
the caller is smaller than the size of `struct hax_msr_data`.

#### HAX\_VCPU\_IOCTL\_SET\_FPU
TODO: Describe

* Since: API v1
* Parameter: `struct fx_layout fpu`, where
  ```
  struct fx_layout {
      uint16  fcw;
      uint16  fsw;
      uint8   ftw;
      uint8   res1;
      uint16  fop;
      union {
          struct {
              uint32  fip;
              uint16  fcs;
              uint16  res2;
          };
          uint64  fpu_ip;
      };
      union {
          struct {
              uint32  fdp;
              uint16  fds;
              uint16  res3;
          };
          uint64  fpu_dp;
      };
      uint32  mxcsr;
      uint32  mxcsr_mask;
      uint8   st_mm[8][16];
      uint8   mmx_1[8][16];
      uint8   mmx_2[8][16];
      uint8   pad[96];
  } __attribute__ ((aligned(8)));
  ```
  * (Input) `fpu`:
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct fx_layout`.

#### HAX\_VCPU\_IOCTL\_GET\_FPU
TODO: Describe

* Since: API v1
* Parameter: `struct fx_layout fpu` (q.v. `HAX_VCPU_IOCTL_SET_FPU`)
  * (Output) `fpu`:
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The output buffer provided by the
caller is smaller than the size of `struct fx_layout`.

#### HAX\_VCPU\_SET\_REGS
TODO: Describe

* Since: API v1
* Parameter: `struct vcpu_state_t regs`, where
  ```
  union interruptibility_state_t {
      uint32 raw;
      struct {
          uint32 sti_blocking   : 1;
          uint32 movss_blocking : 1;
          uint32 smi_blocking   : 1;
          uint32 nmi_blocking   : 1;
          uint32 reserved       : 28;
      };
      uint64_t pad;
  };
  
  typedef union interruptibility_state_t interruptibility_state_t;
  
  struct segment_desc_t {
      uint16 selector;
      uint16 _dummy;
      uint32 limit;
      uint64 base;
      union {
          struct {
              uint32 type             : 4;
              uint32 desc             : 1;
              uint32 dpl              : 2;
              uint32 present          : 1;
              uint32                  : 4;
              uint32 available        : 1;
              uint32 long_mode        : 1;
              uint32 operand_size     : 1;
              uint32 granularity      : 1;
              uint32 null             : 1;
              uint32                  : 15;
          };
          uint32 ar;
      };
      uint32 ipad;
  };
  
  typedef struct segment_desc_t segment_desc_t;
  
  struct vcpu_state_t {
      union {
          uint64 _regs[16];
          struct {
              union {
                  struct {
                      uint8 _al,
                            _ah;
                  };
                  uint16    _ax;
                  uint32    _eax;
                  uint64    _rax;
              };
              union {
                  struct {
                      uint8 _cl,
                            _ch;
                  };
                  uint16    _cx;
                  uint32    _ecx;
                  uint64    _rcx;
              };
              union {
                  struct {
                      uint8 _dl,
                            _dh;
                  };
                  uint16    _dx;
                  uint32    _edx;
                  uint64    _rdx;
              };
              union {
                  struct {
                      uint8 _bl,
                            _bh;
                  };
                  uint16    _bx;
                  uint32    _ebx;
                  uint64    _rbx;
              };
              union {
                  uint16    _sp;
                  uint32    _esp;
                  uint64    _rsp;
              };
              union {
                  uint16    _bp;
                  uint32    _ebp;
                  uint64    _rbp;
              };
              union {
                  uint16    _si;
                  uint32    _esi;
                  uint64    _rsi;
              };
              union {
                  uint16    _di;
                  uint32    _edi;
                  uint64    _rdi;
              };
  
              uint64 _r8;
              uint64 _r9;
              uint64 _r10;
              uint64 _r11;
              uint64 _r12;
              uint64 _r13;
              uint64 _r14;
              uint64 _r15;
          };
      };
  
      union {
          uint32 _eip;
          uint64 _rip;
      };
  
      union {
          uint32 _eflags;
          uint64 _rflags;
      };
  
      segment_desc_t _cs;
      segment_desc_t _ss;
      segment_desc_t _ds;
      segment_desc_t _es;
      segment_desc_t _fs;
      segment_desc_t _gs;
      segment_desc_t _ldt;
      segment_desc_t _tr;
  
      segment_desc_t _gdt;
      segment_desc_t _idt;
  
      uint64 _cr0;
      uint64 _cr2;
      uint64 _cr3;
      uint64 _cr4;
  
      uint64 _dr0;
      uint64 _dr1;
      uint64 _dr2;
      uint64 _dr3;
      uint64 _dr6;
      uint64 _dr7;
      uint64 _pde;
  
      uint32 _efer;
  
      uint32 _sysenter_cs;
      uint64 _sysenter_eip;
      uint64 _sysenter_esp;
  
      uint32 _activity_state;
      uint32 pad;
      interruptibility_state_t _interruptibility_state;
  };
  ```
  * (Input) `regs`:
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct vcpu_state_t`.

#### HAX\_VCPU\_GET\_REGS
TODO: Describe

* Since: API v1
* Parameter: `struct vcpu_state_t regs` (q.v. `HAX_VCPU_SET_REGS`)
  * (Output) `regs`:
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The output buffer provided by the
caller is smaller than the size of `struct vcpu_state_t`.

#### HAX\_VCPU\_IOCTL\_INTERRUPT
Injects an interrupt into this VCPU.

* Since: API v1
* Parameter: `uint32_t vector`
  * (Input) `vector`: The interrupt vector. Bits 31..8 are ignored.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `uint32_t`.

#### HAX\_VCPU\_IOCTL\_SET\_CPUID
Defines the VCPU responses to the CPU identification (CPUID) instructions.

HAXM initializes a minimal feature set for guest VCPUs in kernel space. This
ensures that most modern CPUs can support these basic CPUID features. Only the
supported CPUID instructions in the feature set will be passed to the physical
CPU for processing.

This IOCTL is used to dynamically adjust the supported feature set of CPUID for
guest VCPUs so as to leverage the latest features from modern CPUs. The features
to be enabled will be incorporated into the feature set, while the features to
be disabled will be removed. If the physical CPU does not support some specified
CPUID features, the enabling operation will be ignored. Usually, this IOCTL is
invoked when the VM is initially configured.

All VCPUs share the same feature set in a VM. This can avoid confusion caused by
the case that when VCPU has multiple cores, different VCPUs executing the same
instruction will produce different results. Send this IOCTL to any VCPU to set
CPUID features, then all VCPUs will change accordingly.

* Since: Capability `HAX_CAP_CPUID`
* Parameter: `struct hax_cpuid cpuid`, where
  ```
  struct hax_cpuid {
      uint32_t total;
      uint32_t pad;
      hax_cpuid_entry entries[0];
  } __attribute__ ((__packed__));
  ```
  where
  ```
  #define HAX_MAX_CPUID_ENTRIES 0x40
  struct hax_cpuid_entry {
      uint32_t function;
      uint32_t index;
      uint32_t flags;
      uint32_t eax;
      uint32_t ebx;
      uint32_t ecx;
      uint32_t edx;
      uint32_t pad[3];
  } __attribute__ ((__packed__));
  ```
  `hax_cpuid` is a variable-length type. The accessible memory of `entries` is
  decided by the actual allocation from user space. For macOS, the argument of
  user data should pass the address of the pointer to `hax_cpuid` when `ioctl()`
  is invoked.
  * (Input) `total`: Number of CPUIDs in entries. The valid value should be in
the range (0, `HAX_MAX_CPUID_ENTRIES`].
  * (Input) `pad`: Ignored.
  * (Input) `entries`: Array of `struct hax_cpuid_entry`. This array contains
the CPUID feature set of the guest VCPU that is pre-configured by the VM in user
space.

  For each entry in `struct hax_cpuid_entry`
  * (Input) `function`: CPUID function code, i.e., initial EAX value.
  * (Input) `index`: Sub-leaf index.
  * (Input) `flags`: Feature flags.
  * (Input) `eax`: EAX register value.
  * (Input) `ebx`: EBX register value.
  * (Input) `ecx`: ECX register value.
  * (Input) `edx`: EDX register value.
  * (Input) `pad`: Ignored.
* Error codes:
  * `STATUS_INVALID_PARAMETER` (Windows): The input buffer provided by the
caller is smaller than the size of `struct hax_cpuid`.
  * `STATUS_UNSUCCESSFUL` (Windows): Failed to set CPUID features.
  * `-E2BIG` (macOS): The input value of `total` is greater than
`HAX_MAX_CPUID_ENTRIES`.
  * `-EFAULT` (macOS): Failed to copy contents in `entries` to the memory in
kernel space.
