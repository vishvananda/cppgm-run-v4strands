# PA3 final plan and audit ledger

- **Target / disposition:** `pa3 full-stage`, audit complete for PA3's standalone `ppexpr` surface. See [audit.md](audit.md) for independent data-flow reconstruction, language/architecture review, findings and frozen measurement detail.
- **Reviewed history:** base `a2af5efaf69c05acf888546f278721110c47d32`; implementation `1ff7afd8c8a246eb93cc655000376f965d12e5b1`; review marker `013530228`. Initial stage tests were 0/20 (not implemented); no test, reference, coverage or comparison rules were changed.

## Final design / behavior ledger

1. `dev/ppexpr.cpp`: stdin or batch-file bytes → one owned source string → shared phase-1–3 scanner → PA3 token callbacks/current logical-line vector → result/error output and final `eof`. Phase failures remain process-fatal; expression errors are line-local.
2. `dev/src/preprocess/pp_tokenizer.cpp`: inherited UTF-8 decode, phase-1/2 translation and source-offset/raw-literal restoration, phase-3 tokenization and synchronous callbacks. Linear in source/tokens; multiple source-sized code-point views remain a known architecture handoff.
3. `dev/src/pa3_expression.cpp`: current-line token vector → precedence parser and checked integer/character decoding → signed/unsigned course-value semantics, lazy logical/conditional evaluation with static conditional common type → decimal result. No optimization/IR pass exists in PA3.

Course integer and character semantics were reviewed against C++11 N3337 [lex.icon], [lex.ccon] plus PA3's explicit promotion, scalar, and evaluation rules. No unsupported reference defect was proved, so references remain untouched. Required behavior groups 1–3 are closed; no reference corrections or coverage edits were made.

## Audit fix and performance disposition

- Replaced `ostringstream` staging plus `.str()` source copies in ordinary and batch CLI input with one string filled by a 64 KiB buffered read loop, kept alive through the synchronous scanner call. Exact behavior is unchanged.
- Final frozen serial A/A calibration plus four balanced ABBA A/C blocks are under `$RALPH_ARTIFACT_DIR/pa3-final-audit/`. On a fixed 9,675,000-byte, 600,000-expression workload all output hashes matched. Candidate peak RSS consistently decreased about 9.3 MiB (A roughly 180.3 MiB; C roughly 171.0 MiB); `.text` fell from 112,743 to 110,888 bytes. Wall time was too noisy to claim any repeatable benefit. One forced C build was 8.22 s / 146,108 KiB; descriptive only. This is source-ownership cleanup, not optimizer or generated-code claim.
- No numeric performance pass/fail limits exist in PA1–PA3 specs/plans. PA1's inherited A/B evidence demonstrates ~63.5% lower tokenizer runtime, while disclosing +5.9% RSS, +13% `.text`, and slower object build; without a numeric budget this remains a stage-scoped measured benefit with tradeoffs, not an unsupported rejection gate. PA2's measurements are descriptive, not an A/B claim. No invented latency/RSS/runtime/text/IR gate is retained. PA3 produces no generated programs, so executable runtime/text acceptance is inapplicable. There is no whole-compiler benchmark suite on these stages; front-to-ELF benchmarks remain an explicit handoff.

## Final validation / remaining handoffs

- `perl scripts/cppgm_file_audit.pl --stage pa3 --paths dev/src`: pass; one inherited complexity advisory at `dev/src/preprocess/pp_tokenizer.cpp:825` (nesting depth 7).
- `make test-report-through-pa3`: pass, 103/103 (PA1 57/57, PA2 26/26, PA3 20/20); all tracked stages pass. Primary log: `/home/vishvananda/work/.ralph/v4strands-gpt-6-luna-max/last-test.log`.
- Remaining unaudited handoffs since PA2: convert shared phase-1–3 scanning from full-input/vector views to source-backed streaming token cursor; later parser/semantic graph, template demand/caches, direct typed LowIR, bounded optimization, native backend and ELF generation, including the spec's nontrivial declaration/template-to-ELF and optimization-runtime audit. PA3 does not claim these absent surfaces are complete.
- Final commit and clean-tree status are recorded after commit in the repository history/state.
