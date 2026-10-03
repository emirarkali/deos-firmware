# AI Agent Instructions for DEOS Firmware

These rules apply to ALL AI assistants, coders, and agents (Gemini, Claude, DeepSeek, Cursor, Copilot, etc.) working on this repository.

## 1. Architectural Boundaries & Permissions

- **Read-Only Access to Core Library**: AI agents are strictly prohibited from modifying the `deos_core` library located in the `lib/` directory. You are only allowed to read it to understand the underlying infrastructure.
- **Approval Required for Core Changes**: Any modifications to the `deos_core` library must be explicitly asked to and approved by Emir Arkalı. Unauthorized changes will break the wire protocol and cause communication failures with other CAN nodes.
- **Reporting Necessary Core Changes**: If a modification in the `lib/` directory is absolutely necessary for an application to function properly, the AI agent must report the situation and clearly explain the required fix to the user instead of applying the changes directly.
- **Scope of Permissions**: In general, developers and AI agents are only authorized to create and modify applications within the `apps/` directory (e.g., `apps/traction`, `apps/steering`). They do not have the authority to edit the core library without explicit permission.

## 2. Zephyr RTOS & Build Rules

- **Build Commands**: Do not run a generic `west build` at the root of the project. Each application is built independently. Always specify the board and application directory:
  `west build -b <board_name> apps/<app_name>` (e.g., `west build -b native_sim apps/traction`)
- **Logging Standards**: Do NOT use standard C `printf()`. Always use Zephyr's logging subsystem (`<zephyr/logging/log.h>`) with `LOG_INF()`, `LOG_ERR()`, `LOG_WRN()`, etc. Ensure `LOG_MODULE_REGISTER()` is declared at the top of the C files.
- **Include Paths**: When an application needs to include DEOS core headers, it must use the system include format `<deos/deos.h>` or `<deos/deos_icd.h>`. Never use relative paths (like `../../lib/deos_core/...`) to access the library headers.
