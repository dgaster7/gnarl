# GNARL build progress

Resume rule: read this file and CLAUDE.md, continue from the first unchecked item. Commit + push after each item.

- [x] 1. Tools checked; PROGRESS.md, CLAUDE.md, docs/ARCHITECTURE.md, docs/PARAMETERS.md; GitHub repo created and pushed
- [ ] 2. Scaffold: CMake, JUCE, plugin targets, Catch2, render harness, pluginval + auval wired, CI green on empty plugins
- [ ] 3. Parameter registry + Check 1 harness (before real DSP)
- [ ] 4. Core voice: VA + wavetable engines, SUB, NOISE, SVF, amp envelope, glide
- [ ] 5. Modulation engine + matrix, then drag-and-drop UI
- [ ] 6. Remaining engines: HyperSaw, FM, Grain/Formant, Harmonic, Multi mode, osc chain, full Shaper; aliasing report
- [ ] 7. All filters, routing, link/offsets, drive stages; magnitude plots in docs/filters/
- [ ] 8. FX chain, all modules, then GNARL FX
- [ ] 9. Preset system, browser, Mutate, factory wavetables, factory presets (gated batches of 20)
- [ ] 10. Updater, release workflow, install.sh, README
- [ ] 11. Full Checks 1-3, CPU benchmarks, fix failures
- [ ] 12. Delivery: v1.0.0 release, install as user, prove update + rollback (v1.0.1 + content update)
- [ ] 13. Final report (docs/FINAL_REPORT.md)

## Notes / decisions log
- 2026-11-15: Toolchain: cmake 4.5.0, ninja 1.13.2, Apple clang 14.0.3 (Command Line Tools only, no Xcode), macOS 13.4 SDK 13.3, arm64 host. JUCE 9.0.5, Catch2 v3.16.1, pluginval v1.0.4, Monocypher 4.0.3.
- No screenshot was received with the prompt; Tri-Peak is modelled from the written description of Z3TA+ Reso 3 Boost.
