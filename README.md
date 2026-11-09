# REDasm Core Engine
This repository contains the standalone disassembler library driving [GUI](https://github.com/redasm-dev/redasm) and [Unit Testing Suite](https://github.com/redasm-dev/tests).  

It's minimal and lightweight on purpose, written in **pure C17** and completely decoupled from the user interface.

## Features
*   **Headless-First**: Can be used as a standalone library for automation pipelines, custom tools, or alternative frontends.
*   **Decoupled Architecture**: Hot-pluggable modules. Formats and architectures are loaded dynamically at runtime via shared modules.
*   **External Database**: TOML based [Knowledge Base](https://github.com/redasm-dev/kb).
*   **RDIL**: A tiny architecture-neutral intermediate Language.
