# MidnightBSD AI Contribution Policy

1. Core Philosophy: "The Pilot is Human"
MidnightBSD is a community-driven desktop operating system. While we acknowledge that AI tools (LLMs, Copilots, etc.) can accelerate development,
the contributor remains 100% responsible for the correctness, security, and performance of their submission.

"The AI wrote it" is never an acceptable defense for broken code.

2. Technical Requirements by Language

C / C++ - Restricted - AI often ignores manual memory management or introduces buffer overflows. Every line must be manually audited for malloc/free symmetry and bounds checking.

contrib/mksh - Prohibited.  The author of the shell does not want it indexed or scanned with AI tools.

Assembly - Prohibited - We do not accept AI-generated Assembly. AI lacks the nuanced understanding of specific CPU architectures and side-channel mitigations required for MidnightBSD.

Shell (sh) - Permitted - High utility for boilerplate, but must be checked for POSIX compliance. Avoid "Bash-isms".

Documentation -  Encouraged - Great for proofreading and formatting, provided technical accuracy is maintained.

3. The "Midnight Test" for Contributions

Before submitting a Pull Request containing AI-assisted code, you must ensure it meets these three criteria:

Traceability: You must be able to explain exactly how the code interacts with the kernel or userland.

No License Contamination: You warrant that the code does not contain snippets of GPL, proprietary, or otherwise "regurgitated" code that violates the BSD 2-Clause License.

No Blind Commits: Do not copy-paste blocks of code that you do not fully understand. If you cannot debug it without the AI's help, do not submit it.

4. Submission Protocol

To maintain transparency in our git history, please follow these conventions:

Commit Trailers
If AI assisted in the creation of a commit, add the following tag to the end of your commit message:

AI-Assisted-by: [Tool Name] (e.g., AI-Assisted-by: Claude 3.7)

Pull Request Checklist
Every PR must check the following:

[ ] I have manually reviewed every line of AI-generated code for security flaws.

[ ] The code adheres to the MidnightBSD style guide (style(9)).

[ ] No proprietary or copy-left code was introduced via the AI tool.

5. Enforcement

The MidnightBSD maintainers reserve the right to:

Reject any PR that appears to be "low-effort AI spam."

Request a Rewrite if the code structure is nonsensical or overly verbose (common with AI).

Revert any commits found to have introduced vulnerabilities due to unverified AI output.


