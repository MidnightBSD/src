.if ${MACHINE_ARCH} == "aarch64" || \
    ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || \
    (${MACHINE} == "arm" && ${MACHINE_ARCH:Marm*eb*} == "") || \
    ${MACHINE_CPUARCH} == "riscv"
TARGET_ENDIANNESS= 1234
CAP_MKDB_ENDIAN= -l
LOCALEDEF_ENDIAN= -l
.elif (${MACHINE} == "arm" && ${MACHINE_ARCH:Marm*eb*} != "")
TARGET_ENDIANNESS= 4321
CAP_MKDB_ENDIAN= -b
LOCALEDEF_ENDIAN= -b
.endif
