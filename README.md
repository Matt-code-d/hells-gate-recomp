# Dante's Inferno - Xbox 360 to PC Port (ReXGlue)

<p align="center">
  <img src="assets/fan_artwork.png" alt="Dante's Inferno - Fan Artwork" width="256" />
</p>

<p align="center">
  <em>Fan artwork by <a href="https://www.deviantart.com/pooterman">POOTERMAN</a> (<a href="https://github.com/florinp93/hells-gate-recomp/issues/12">#12</a>)</em>
</p>

A static recompilation port of **Dante's Inferno** (Xbox 360) to native PC,
built with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

ReXGlue converts Xbox 360 PowerPC XEX executables into portable C++ that runs
natively on Windows (D3D12) and Linux (Vulkan) - no emulation, no JIT at
runtime.

<p align="center">
  <a href="https://ko-fi.com/zerkiller">
    <img src="https://img.shields.io/badge/Ko--Fi-Buy%20me%20a%20coffee-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white" alt="Ko-fi" />
  </a>
  <a href="https://discord.gg/mjGfv7ysG8">
    <img src="https://img.shields.io/badge/Discord-Join%20the%20server-5865F2?style=for-the-badge&logo=discord&logoColor=white" alt="Discord" />
  </a>
  <a href="https://github.com/florinp93">
    <img src="https://img.shields.io/badge/Other-Projects-0AB4F5?style=for-the-badge&logo=github&logoColor=white" alt="Other Projects" />
  </a>
</p>

## 🤖 AI Usage Disclosure

Transparency and integrity are important to this project. Artificial Intelligence (AI) tools were utilized as part of the development and maintenance workflow, strictly serving as an assistant to handle repetitive, time-consuming, and low-level tasks.

### How AI Was Used:
* **Documentation:** Generating initial drafts, organizing notes, and structuring documentation to keep project progress up to date.
* **Research & Exploration:** Investigating APIs, syntax references, and conceptual troubleshooting.
* **Git Workflows:** Assisting with routine commit descriptions, repository maintenance tasks, and boilerplate structuring.

### Human Oversight:
While AI accelerated the auxiliary workflow, all core architectural decisions, advanced problem-solving, code implementation, and final reviews were entirely human-driven. The AI served to eliminate friction, allowing focus on high-level logic and feature development.

## Progress Tracker

- [x] Game boots, runs, and is fully playable
  - ReXGlue SDK v0.10.0 codegen + native build
  - VMX/AltiVec PowerPC instructions supported
  - VP6/Bink FMV corruption fixed (upstream PR to be made)
  - Save system fixed (fiber/setjmp/longjmp + `XUserFindUsers` handler)

- [x] Graphics & input configured
  - Resolution scaling, anisotropic override, post-effect cvars
  - Aspect ratio control: 4:3 / 16:9 / 16:10 / 21:9 / 32:9
  - SDL input backend set as default
  - Mouse & keyboard keybind defaults configured

### In progress
- [ ] DLC auto-install hook (`OnPostSetup` STFS package scan)
- [ ] 120 Hz / high-refresh timing polish (gameplay OK; menu/minigame timing under reverse engineering)
- [ ] Native DiligentCore/Vulkan renderer migration (working, not fully implemented)

## Roadmap

| Phase | Feature | Status |
|-------|---------|--------|
| 1 | Graphics quality cvars (resolution scale, AA, filtering) | Done |
| 2 | Input defaults + SDL backend | Done |
| 3 | DLC auto-install | In progress |
| 4 | Ultrawide / aspect-ratio support | Done |
| 5 | Button glyph replacement (input-device RE) | Planned |
| 6 | Native DiligentCore/Vulkan renderer migration | In progress (working, not final) |
