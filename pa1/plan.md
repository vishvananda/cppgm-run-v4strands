# PA1 handoff plan

- Stage base commit: `e50e87639188c5da678ab00d4d927c5d57396c65`
- Last reviewed commit: `e50e87639188c5da678ab00d4d927c5d57396c65`
- Design/spec alignment: implement PA1 phases 1–3 (UTF-8/UCN/trigraph/splice/newline; comments and token grammar; header-name context) behind `pptoken`, without changing downstream compiler architecture or harness/reference coverage.
- Initial failures: all 54 required fixtures fail identically (`EXIT_NOT_IMPLEMENTED`). Owner: `PPTokenizer::process`/PA1 tokenizer entry; data flow: stdin bytes → translation phases → token events → debug stream; intended O(input code points + emitted spelling bytes), O(input) immutable source plus bounded token scratch. Implement feature clusters together, then resolve remaining mismatches by owning lexical rule.
- Performance evidence: baseline placeholder provides no meaningful correctness/runtime comparison; collect compiler build latency/peak RSS and tool text size after implementation, plus tokenization throughput/runtime and RSS on fixed inputs. PA1 emits tokens, not executables; generated-program runtime/size are N/A. No optimization claim planned.
- Handoff ledger: implementation groups open (translation phases/UTF-8, token grammar/context, malformed-input status); independent full-stage audit remains Ralph's responsibility and is not waived. Keep all existing fixtures/comparison rules.
