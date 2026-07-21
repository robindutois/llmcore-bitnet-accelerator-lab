# Week 10 Report — Final Delivery: Documentation Closeout and Repository Audit
**Student**: Erven LE BIVIC — Seoul National University
**Date**: 2026-07-20
**Project**: BitLinear-FPGA Alpha — LLMCore Accelerator Lab

---

## Objective

Final BitLinear-FPGA Alpha delivery (spec Week 10): cleaned repository, final
report, final demo. With the technical work — HLS kernel, PS-PL integration,
benchmarking, and the K%4 regression fix — closed out in Week 9, Week 10's
remaining scope was to bring the repository's own documentation into sync
with that finished state, and to audit it end-to-end for exactly the kind of
staleness a fast-moving 10-week sprint accumulates: status lines that say
"in progress" for work that finished weeks ago, directory maps that don't
list files added since they were written, and small inconsistencies of that
kind.

---

## Work Completed

### 1. Root and FPGA-track README — Status and Directory Map Audit

Both `README.md` (root) and `fpga_erven/README.md` still read "Week 9 of 10"
in their status lines, despite the K%4 fix and final report both being
complete. Corrected in both files, and cross-checked against `git ls-files`
rather than assumed:

- Directory maps updated to include files/folders that were tracked in git
  but missing from the documented tree: `fpga_erven/petalinux/` (the
  PetaLinux app recipe and device-tree overlay used to build the board's
  Linux image — previously undocumented anywhere in the repo),
  `fpga_erven/ps_host/result_check.md`, `gen_test_vectors_h.py`,
  `host_skeleton.cpp`, `test_vectors_data.h`, `week6_petalinux_result.txt`,
  `fpga_erven/hls/bitlinear/stress_test_kpad.cpp`, and the
  `fpga_erven/benchmarks/` files (`results.csv`, `feasibility_analysis.md`,
  `scaling_analysis.md`) that existed on disk but weren't listed.
- A phantom `tenstorrent_robin/benchmarks/` entry — present in the root
  README's tree but never actually tracked in git — was removed.
- The `fpga_erven/README.md` validation-chain table was missing its final
  row: the on-board K%4 verification (`bench_scaling` M=5/K=7 and
  `run_bitlinear_linux.c` 10/10, incl. `test09_manual`) was already in the
  root README's results table but not mirrored here.
- Root README's repository-status table: the FPGA-track line updated to
  reflect final delivery; the Tenstorrent-track line left content-wise
  as-is (not this track's call to make), but flagged with a note that
  `main` carries Tenstorrent commits made after this README was last
  synced, to be confirmed with Robin before the joint submission.

### 2. `.gitignore` Cleanup

Removed accumulated duplication: `fpga_erven/hls/bitlinear/tv_path_generated.h`
was listed four times, `fpga_erven/vitis_project/bitlinear_system/` twice,
and a stray, unexplained `#Robin` comment sat above an unrelated ignore rule.
No functional change — same paths ignored, just written once each.

### 3. Spec Path Question — `reference/bitlinear_reference.cpp`

Audited whether the C++ reference belongs at `reference/bitlinear_reference.cpp`
(as shown in the project spec's illustrative shared-repository tree, Section 4) or at
`fpga_erven/hls/reference/bitlinear_reference.cpp` (where it has lived since
Week 2, and where the spec's own Layer 2 deliverable list — the detailed,
per-layer specification this track is actually assessed against — explicitly
names it). Conclusion: this is an internal inconsistency in the spec document
itself, the same pattern already seen with `results.csv` vs.
`results_week7.csv`/`results_week8.csv`. No file move made; the current
location satisfies the specific, authoritative deliverable path.

### 4. Investor Demo Script — Decision Recorded

The spec's Week 10 checklist lists "Investor demo script" as an item distinct
from the final report and the demo video. Decision: not producing a separate
script file — the investor-facing narrative already lives in the final
report (Section 15) and in the demo video itself once recorded; a third,
separate script document was judged to add no additional value over
duplicating that content a third time.

### 5. Second Independent Audit Pass

Re-checked `correctness_summary.md`, `rtl_cosim_result.md`, `run_benchmark.cpp`,
and `scaling_analysis.md` specifically for stale "Pending" language or
undocumented stub content. All four were already correctly handled from
earlier passes: the two Week 5 reports carry an explicit dated-snapshot
disclaimer pointing to the current status elsewhere, and `run_benchmark.cpp`
/ `scaling_analysis.md` each explain in-file why they are thin
wrappers/pointers rather than duplicated content. No further changes needed.

---

## Deliverables Committed

| File | Description |
|---|---|
| `README.md` | Status and directory map synced to final-delivery state |
| `fpga_erven/README.md` | Status, directory map, and validation-chain table synced |
| `.gitignore` | Duplicate rules and stray comment removed |
| `docs/weekly_reports/week10_erven.md` | This report |

---

## Success Criterion

✅ Repository documentation (both READMEs) matches the actual final-delivery
state and the real, git-tracked file set
✅ `.gitignore` cleaned of duplication
✅ Spec path ambiguity investigated and resolved with a documented rationale
✅ Investor demo script scope decision made and recorded
⏳ Demo video — being delivered separately (file size), not yet committed to
`docs/investor_demo/`
⏳ Joint final report — Sections 6 and 15.2 remain placeholders pending
Robin's Tenstorrent-track text

---

## Next

- Receive and merge Robin's Sections 6 and 15.2 into
  `docs/final_report/erven_fpga_report_draft.md`
- Deliver the demo video and the final report PDF (handled outside this
  repository update due to file size)
- Final joint submission
