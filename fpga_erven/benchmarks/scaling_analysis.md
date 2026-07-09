# Scaling Analysis — Pointer

This exact filename appears in one summary table of the CDC (an earlier,
generic overview section listing `results.csv` / `resource_report.md` /
`scaling_analysis.md`). The CDC's own detailed, week-by-week deliverable
list — the more specific and authoritative section — names a different file
for this content: `fpga_erven/benchmarks/matrix_scaling_notes.md` (Week 7).
The two names appear to be an internal inconsistency within the CDC document
itself, not two separate required deliverables.

Rather than duplicate content under two names (a real risk of the two
copies drifting out of sync over time), this file points to where the
content actually lives:

- **Matrix-size scaling analysis, latency model, memory bottleneck
  identification (the Week 7 CDC task):**
  [`matrix_scaling_notes.md`](matrix_scaling_notes.md)
- **Extended scaling analysis — TerEffic comparison, resource wall,
  GOPS/W efficiency across all 8 measured sizes, ASIC lane-count estimate
  (Week 8/9 extension beyond the Week 7 CDC ask):**
  [`feasibility_analysis.md`](feasibility_analysis.md)
- **Resource utilization (LUT/FF/BRAM/DSP), measured post-implementation:**
  [`resource_report.md`](resource_report.md)
