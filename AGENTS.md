# AGENTS.md

Instructions for coding agents working in this repository.

Before making changes, read these files:

- `CLAUDE.md`
- `AI_POLICY.md`
- `DCO.md`

Treat the repository policy documents above as authoritative for:

- what parts of the tree may or may not be modified with AI assistance
- language-specific restrictions
- contribution and sign-off requirements
- build, style, and testing expectations

If an AI-assisted commit is created, include the appropriate `AI-Assisted-by:` trailer for the agent actually used.

## Upstream Sources and Vendor Branches

MidnightBSD is derived from FreeBSD, but we manually integrate upstream FreeBSD changes. There is no persistent tracking branch that follows FreeBSD tip.

Separately, the base system includes a number of third-party components (e.g., extraction utilities, compilers, time zone data, OpenZFS, etc.). When those components are imported from upstream projects, we often track them using **vendor branches** in git.

### Where vendor-managed code lives

Vendor imports commonly land in:

- `contrib/` (userland third-party software)
- `crypto/` (cryptographic libraries/commands)
- `sys/contrib/` (kernel third-party sources)

### Vendor branch naming conventions

Vendor branches are typically named under `vendor/` and a few conventions exist:

- Newer imports: `vendor/<softwarename>` (example: `vendor/bc`)
- Multi-package upstreams: `vendor/<Upstream>/<package>` using the NetBSD-style layout (example: `vendor/NetBSD/bmake`)
- Historical Subversion-era layouts: `vendor/<softwarename>/dist` (example: `vendor/diffutils/dist`)

### Documentation

- Overview of many vendor branches: https://github.com/MidnightBSD/src/wiki/Vendor-Branches
- Instructions for updating vendor branches: https://github.com/MidnightBSD/src/wiki/Git
