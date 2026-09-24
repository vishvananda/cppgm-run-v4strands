# PA2 full-stage final audit

- **Scope:** PA2 `posttoken` full-stage, against `spec.md`, `pa2/README.md`, and the PA2 test/reference contract. This closes the review markers recorded in `plan.md`; it does not claim that later compiler stages have been implemented or audited.
- **Disposition:** aligned for the PA2 stage. No reference or test corrections were made. The only pending source change removes the posttoken-wide owning token collection and processes scanner callbacks directly.

## Spec alignment and reviewed flow

The actual PA2 path is: stdin bytes are read by the CLI; the shared phase-1–3 scanner in `dev/src/preprocess/pp_tokenizer.cpp` decodes/translates the input and scans preprocessing tokens; its synchronous `IPPTokenStream` callbacks feed `PostTokenProcessor` in `dev/posttoken.cpp`; identifiers, numbers, characters, operators and invalid tokens are classified and emitted immediately to the debug output stream. For string and user-defined-string callbacks, the processor retains only the current adjacent-string run, because phase-6 concatenation must wait until a non-string callback or EOF. It preserves ordered source spellings, decoded units, encoding and suffix facts for that run, reduces it once, emits one result, and clears the run. EOF flushes the run before emitting `eof`.

This matches the stage's required categorization, literal decoding, adjacent-string concatenation and explicit textual tool output. It removes the former `vector<PostToken>` containing every token and avoids constructing a second posttoken representation. The current scanner is push/callback-based and retains source-sized phase-translation buffers; PA2 has no parser consuming a production token cursor. The spec's future production integration still needs to preserve this no-owning-token-stream property and provide the required cursor/source-range architecture. PA2's CLI output boundary does not itself transport tokens to another production phase. Parser/semantic graph, templates, LowIR, backend and ELF requirements are not PA2 implementation surfaces and are not represented as audited or completed here.

## Findings and changes

1. **Whole-stream posttoken collection:** the previous collector stored `{kind, source}` for every preprocessing token before classification. Replaced it with direct `PostTokenProcessor` callbacks. Non-string tokens are emitted as they arrive; only the adjacent string run is deferred and reduced. This is bounded by the active run, which is necessary to form the required combined source and literal value. The transition preserves output order and flushes before every non-string token and at EOF.
2. **String-run behavior:** regular and user-defined strings enter the same run; compatible encodings and at most one distinct UDL suffix are combined, while incompatible facts or invalid constituents produce the single required invalid result for the whole run. Source spellings remain space-separated and ordered. Existing fixtures and comparisons remain unchanged.
3. **Reference corrections:** none in this audit. Existing language decisions remain documented in `plan.md`: integer candidate lists follow C++11 N3337 [lex.icon], quoted-literal handling follows [lex.ccon] and [lex.string], and suffix handling follows [lex.ext]. The handout's reserved `operator\"\"sv` case remains distinct from underscore-led UDL suffixes. No output oracle, test coverage, or comparison rule was relaxed.
4. **Self-containment and complexity:** the changed path invokes no reference/host compiler and does not recognize fixtures. Classification work is per callback; run reduction is linear in accumulated units/source. No generated executable behavior exists at this stage.

## Performance evidence

Measured the 429,984-byte `pa2/tests/700-hard-string-concat.t` through the rebuilt `dev/posttoken` in eight serial `/usr/bin/time` runs. Observed wall time was 0.26–0.51 s and maximum RSS was 10,880–11,176 KiB. `size dev/posttoken` reported a 164,258-byte text section. These are descriptive single-machine repeated observations, not a frozen A/B comparison or an optimization claim; no A/B result is asserted. PA2 produces no generated program, so generated-program runtime and generated-code-size acceptance are not applicable. The stage-scoped tool evidence shows the large concatenation fixture completes without a timeout or memory-growth defect; no stricter self-imposed timing threshold is introduced.

## Validation

- `make test-report-through-pa2` — pass, **83/83** (PA1 57/57; PA2 26/26).
- `perl scripts/cppgm_file_audit.pl --stage pa2 --paths dev/src` — pass; one existing complexity advisory at `dev/src/preprocess/pp_tokenizer.cpp:825` (scan nesting depth 7), not a failure.
- `git diff --check` — pass before commit.
- Tests, references and comparison rules are unchanged.
