# Version changelog
## 2026 09 30
### Main changes summary
- General changes:
    - License change to BSD-3-Clause or GPL-2.0-only.
    - Created a .gitignore and CONTRIBUTING.md
    - EFI acpidump now builds for the RISCV64 architecture
    - New Linux port_commit.sh script to convert ACPICA git history to Linux
      patches
- New features:
    - Tables : KEYP, MISC, UBRT (no subtable support yet)
    - New method allowed : _Ixx (where xx is a hexadecimal number)
- More notable fixes:
    - Printf function family compiles again
    - Fixed the acpidump EFI binaries not building (both EDK2 and gnu-efi)
    - Fixed the Switch/While disassembler confusion
    - Fixed the external method count over 255 not working
    - Fixed incorrect PCC command offset

### Full shortlog since 20260408
```
Chmielewski, Pawel (1):
      Linuxize: add port_commit.sh

Colin Ian King (15):
      Grammar test: Fix typos "Battey" -> "Battery"
      ASL test suite (ASLTS): Fix typo "oprators" -> "operators"
      ASLTS: Mutex SPEC: Fix typos "occuppied" -> "occupied"
      ASL test suite (ASLTS): common.asl: Fix typo "cammand" -> "command"
      ASLTS: mutex_proc: Fix typo in comment "derect" -> "direct"
      ASLTS: SPEC: Fix typos "obects" and "perpose"
      ASLTS: while: Fix typos "complited" and "poin"
      ASLTS: ssdt2: Fix typo "decleared" -> "declared"
      ASLTS: crbuffield: Fix typo in comment "unerlying" -> "underlying"
      ASLTS: abbu: Fix typo in comment "Aplicable" -> "Applicable"
      ASLTS: DECL: Fix typo "reporeted" -> "reported"
      ASLTS: exc: Fix typo "restictions" -> "restrictions"
      ASLTS: provoke: Fix typo "exersice" -> "exercise"
      components: Fix typo "Supress" -> "Suppress"
      components: Fix typo: "numer" -> "number"

Dave Jiang (2):
      CEDT: RDPAS: Add missing reserved field in RDPAS structure
      CEDT: RDPAS: Fix incorrect field positions for RDPAS

Hanjun Guo (2):
      acpica: add UBRT (UnifiedBus Root Table) definitions
      acpica: add UBRT disassembler, compiler, and integration support

Himanshu Chauhan (1):
      Introduce a new HEST notification type for RISC-V SSE events. The GHES entry's notification structure contains the notification to be used for a given error source. For error sources delivering events over SSE, it should contain the new SSE notification type.

Icenowy Zheng (5):
      generate/efi: add missing utcksum.c source file
      generate/efi: adapt to EDK2 202605
      generate/efi: commonize GCC flags
      EFI: support HW Reduced ACPI w/o FACS
      EFI: add support for RISCV64

Ivan Hu (3):
      iASL: Fix Printf/Fprintf broken by AslKeywordMapping bounds check
      Namespace: Initialize ParserState.AmlEnd in AcpiInstallMethod
      iASL: DTPR: Fix dead code in DTPR buffer NULL check

Jessica Marz (1):
      Create SECURITY.md

Maciej Strozek (1):
      Utilities: Fix for AcpiUtVerifyChecksum warning display

Maciej Wieczor-Retman (11):
      generate/efi: Fix EFI building and page fault
      generate/efi: Add missing steps to the readme build process
      gitignore: Update gitignore with generate/efi build files
      executer: Fix NULL pointer dereference in AcpiExStoreObjectToIndex()
      compiler: Add _Ixx GPE object
      compiler: Fix possible NULL pointer
      namespace: Fix possible underflow and NULL pointer dereference
      docs: Describe in detail adding a new table
      docs: Remove old new_table.txt file
      iASL: Validate the entire Switch pattern before rewriting the parse tree
      docs: 202609XX changelog

Mario Limonciello (AMD) (1):
      ACPICA: Add ACPI_MSG_DEBUG to set KERN_DEBUG for debug output

Rafael J. Wysocki (6):
      Revert "Merge documentation guides"
      Change ACPICA project license
      Update ACPICA repository URL in all files
      Adjust the ACPICA license
      Change license text in ACPICA source files
      Add a CONTRIBUTING file

Rong Zhang (1):
      parser: Set AML length for bytelist

Saket Dumbre (24):
      Merge pull request #1135 from void0red/fix-issue29
      Merge pull request #1150 from void0red/fix-issue5-v2
      Merge pull request #1143 from void0red/fix-issue1
      Recover the ASLTS test count (pass + fail + skipped/blocked) that had dropped due to this previous error mandating either _HID or _ADR presence to generate AML
      Merge pull request #1152 from aegl/einjv2defines
      Merge pull request #1189 from ColinIanKing/fix-typos
      Merge pull request #1170 from hschauhan/riscv-sse-notify
      Merge pull request #1200 from mstrozek/verify-checksum
      CEDT: Add full compiler, disassembler, and template support for RDPAS and fix CXIMS hang
      Merge pull request #1201 from davejiang/rdpas-fixes
      docs: add minimal contributor, testing, review, and agent guides
      docs: add project philosophy guide
      docs: add getting-started guide
      Merge pull request #1161 from void0red/fix-1160
      Merge pull request #1163 from void0red/fix-1162
      Merge pull request #1166 from void0red/fix-1165
      Merge pull request #1172 from void0red/fix-1171
      Merge pull request #1174 from void0red/fix-1173
      Merge pull request #1176 from void0red/fix-1175
      Merge pull request #1180 from void0red/fix-1179
      Merge pull request #1182 from void0red/fix-1181
      Merge pull request #1184 from void0red/fix-1183
      Update build.txt with new link to the Adobe platform to file IDZ website tickets
      Merge documentation guides

Sudeep Holla (1):
      Fix PCC OperationRegion command offsets

Tony Luck (1):
      Provide #defines for EINJV2 error types

Xianglai Li (1):
      actbl2: Add bit constants of the Flags field in the IOVT IOMMU structure

abhinavmir (1):
      iASL: Fix the truncated count of external control methods

ikaros (13):
      fix buffer overflow risk by validating descriptor length in resource template
      Add package limit checks in parser functions to prevent out-of-bounds access.
      fix: add boundary checks in AcpiPsGetNextNamestring and AcpiPsPeekOpcode to prevent out-of-bounds access
      Validate 4-char name in AcpiDsGetFieldNames and AcpiDsInitFieldObjects to prevent OOB read in AcpiNsLookup
      Improve error handling in table loading process to ensure proper cleanup on failure
      executer: Add bounds checking for PCC field read access
      executer: Add bounds checking for PCC field write access
      tables: Fix heap-buffer-overflow in AcpiTbFindTable
      Fix object reference update to skip non-local notify types
      namespace: Recognize alias node types in detach
      Events: Clear GPE dispatch by owner during table unload
      AcpiExec: Clean up GPE dispatch and GED handlers on table unload
      Events: Track in-flight notify handlers to prevent use-after-free on table unload

purofle (1):
      fix: build error with gcc 16: unused-but-set-variable
```
