MessageIdTypedef=NTSTATUS
SeverityNames = (
	Success       = 0x0:STATUS_SEVERITY_SUCCESS
	Informational = 0x1:STATUS_SEVERITY_INFORMATIONAL
	Warning       = 0x2:STATUS_SEVERITY_WARNING
	Error         = 0x3:STATUS_SEVERITY_ERROR
)

FacilityNames  = (
	HaxDriver	= 0x1:FACILITY_HAXDRIVER
	HaxVM		= 0x2:FACILITY_HAXVM
	HaxVcpu		= 0x3:FACILITY_HAXVCPU
)


MessageId 	= 	1
Severity	=	Error
Facility	= 	HaxDriver
SymbolicName	= 	HaxDriverCreateUpDevFailure	
Language	= 	English
HAXM failed to creat HAXM device node
.

MessageId 	= 	2
Severity	=	Error
Facility	= 	HaxDriver
SymbolicName	= 	HaxDriverCreateUpSymFailure
Language	= 	English
HAXM failed to create HAXM device node symblink
.

MessageId 	= 	3
Severity	=	Error
Facility	= 	HaxDriver
SymbolicName	= 	HaxDriverHostInitFailure
Language	= 	English
HAXM Failed to init VMX
.

MessageId 	= 	4
Severity	=	Informational
Facility	= 	HaxDriver
SymbolicName	= 	HaxDriverLoaded
Language	= 	English
HAXM is loaded successfully
.

MessageId 	= 	5
Severity	=	Informational
Facility	= 	HaxDriver
SymbolicName	= 	HaxDriverUnloaded
Language	= 	English
HAXM is Unloaded successfully
.

MessageId 	= 	6
Severity	=	Error
Facility	= 	HaxDriver
SymbolicName	= 	HaxDriverNoVT
Language	= 	English
HAXM can't work on system without VT support
.

MessageId 	= 	7
Severity	=	Error
Facility	= 	HaxDriver
SymbolicName	= 	HaxDriverVTEnableFailure
Language	= 	English
HAXM failed to enable VT
.

MessageId       =       8
Severity        =       Error
Facility        =       HaxDriver
SymbolicName    =       HaxDriverNoNX
Language        =       English
HAXM can't work on system without NX support
.

MessageId       =       9
Severity        =       Error
Facility        =       HaxDriver
SymbolicName    =       HaxDriverNoEMT64
Language        =       English
HAXM can't work on system without EM64T support
.

MessageId       =       10
Severity        =       Error
Facility        =       HaxDriver
SymbolicName    =       HaxDriverVTDisable
Language        =       English
HAXM can't work on system with VT disabled
.

MessageId       =       11
Severity        =       Error
Facility        =       HaxDriver
SymbolicName    =       HaxDriverNXDisable
Language        =       English
HAXM can't work on system with NX disabled
.
