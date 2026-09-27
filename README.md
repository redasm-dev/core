# REDasm Core Engine
This repository contains the standalone, headless binary analysis engine and disassembly library driving **[REDasm GUI](https://github.com/redasm-dev/redasm)**.  

It's minimal an lightweight on purpose, written in **pure C17** and completely decoupled from the user interface.

## Features
*   **Headless-First**: Can be used as a standalone library for automation pipelines, custom tools, or alternative frontends.
*   **Decoupled Architecture**: Hot-pluggable modules. Formats and architectures are loaded dynamically at runtime via shared modules.
*   **Ecosystem**: Powered by our [Knowledge Base (kb)](https://github.com/redasm-dev/kb) database.
*   **RDIL**: A tiny architecture-neutral intermediate Language.
