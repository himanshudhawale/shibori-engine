# Adaptive Selection Policy

## 1. Purpose

Adaptive selection chooses a reversible encoding chain and optional byte codec
for each column chunk. It must improve a declared objective without violating
correctness, resource limits, reproducibility, or caller restrictions.

Adaptive does not mean unbounded trial and error. Every decision occurs under
explicit analysis time, candidate count, sample bytes, scratch memory, and
minimum-benefit limits.

## 2. Policy Inputs

```text
Policy
  goal
  target_block_bytes
  maximum_block_rows
  analysis_byte_budget
  analysis_time_budget
  candidate_count_limit
  memory_budget
  worker_limit
  minimum_savings_bytes
  minimum_savings_ratio
  permitted_encodings
  permitted_codecs
  required_capabilities
  deterministic
```

Hard limits always override goal-specific preferences.

## 3. Initial Goals

### Balanced

Optimizes total storage while penalizing encode and decode CPU. It is the
default for backup and archive workflows without a stronger preference.

### Maximum ratio

Spends a larger but still bounded analysis and encode budget to minimize
complete serialized bytes. Decode memory and safety limits remain hard.

### Fast decode

Prioritizes decode throughput, low scratch memory, and simple random block
access. It accepts larger output when the predicted decode benefit exceeds the
configured weight.

Goals are named profiles over a common scoring model, not unrelated algorithms.

## 4. Decision Scope

Format 1 selects independently per column chunk. This permits heterogeneous
types and distributions within a block.

Block-level decisions still coordinate:

- total memory reservations;
- worker scheduling;
- shared codec dictionary eligibility;
- minimum block savings;
- deterministic publication order.

Cross-column transforms are deferred because they complicate projection,
parallelism, schema evolution, and independent verification.

## 5. Analysis Levels

### Level 0: structural

Available during validation:

- type and width;
- row and non-null counts;
- canonical bytes;
- validity bytes;
- minimum possible metadata.

### Level 1: bounded sample

Collected from deterministic positions or prefixes under sample limits:

- estimated distinct ratio;
- delta and second-delta widths;
- run transitions;
- value lengths;
- byte compressibility probes.

Sampling positions are deterministic for deterministic mode.

### Level 2: complete lightweight scan

Collected when affordable or required:

- exact minimum and maximum;
- exact monotonicity;
- complete run count;
- complete bit-width histogram;
- exact dictionary under entry and byte caps.

### Level 3: candidate trial

Runs selected candidate transforms or codecs on a bounded sample or complete
chunk. Trial output is discarded unless the planner explicitly transfers a
complete deterministic result into execution.

No correctness precondition relies solely on Level 1.

## 6. Candidate Generation

Candidate generation proceeds as follows:

1. Add raw canonical storage.
2. Add permitted byte-codec candidates for canonical bytes.
3. Query the registry for templates compatible with the logical type.
4. Reject templates violating caller allowlists or required capabilities.
5. Use structural facts to reject impossible candidates.
6. Use observed statistics to rank remaining candidates.
7. Retain at most the configured candidate count.
8. Estimate or trial candidates within remaining analysis budgets.

Templates define legal stage order. The planner never combines arbitrary
registered stages.

## 7. Pruning Rules

A candidate is rejected before trial when:

- its descriptor does not accept the type or physical representation;
- required parameters cannot be represented;
- worst-case memory exceeds available budget;
- worst-case output exceeds format or caller limits;
- its minimum metadata exceeds raw size minus minimum savings;
- required deterministic behavior is unavailable;
- chain depth exceeds the format or policy limit;
- a required plugin or codec version is absent;
- complete-scan prerequisites cannot fit the analysis budget;
- policy explicitly prohibits a stage.

Pruning reasons are retained for explainability.

## 8. Cost Model

The planner normalizes costs relative to raw canonical storage:

```text
size_cost   = candidate_bytes / raw_complete_bytes
encode_cost = estimated_encode_ns / raw_copy_ns
decode_cost = estimated_decode_ns / raw_read_ns
memory_cost = peak_scratch_bytes / operation_memory_budget
```

A profile computes:

```text
score =
    size_weight   * size_cost +
    encode_weight * encode_cost +
    decode_weight * decode_cost +
    memory_weight * memory_cost +
    uncertainty_penalty
```

Lower is better. Hard limits and minimum savings are checked before scoring.

The initial weight values are implementation defaults documented with benchmark
evidence; they are not serialized as format semantics.

## 9. Estimation

Estimates come from:

- exact serialized results for cheap complete transforms;
- trial compression of deterministic samples;
- measured stage models calibrated by Shibori Bench;
- conservative descriptor bounds when no model exists.

Each estimate records confidence:

- `exact`;
- `measured_sample`;
- `modeled`;
- `upper_bound`.

Lower confidence increases the uncertainty penalty. A candidate with unknown
decode cost cannot win `fast-decode` unless the caller explicitly allows it.

## 10. Minimum Benefit

A non-raw candidate is eligible only when:

```text
raw_complete_bytes - candidate_complete_bytes
    >= minimum_savings_bytes
```

and:

```text
1 - candidate_complete_bytes / raw_complete_bytes
    >= minimum_savings_ratio
```

Profiles may use different defaults. Both values are caller-overridable within
safe configuration bounds.

For very small chunks, raw normally wins because directory and parameter
overhead dominates.

## 11. Trial Sampling

Samples must preserve enough local structure for the candidate:

- delta and XOR candidates use contiguous ranges;
- dictionary candidates include deterministic spread positions;
- byte codecs receive framed sample segments, not concatenated bytes that could
  create artificial adjacency;
- null bitmaps sample corresponding row positions.

The explanation records sample rows and bytes. The planner cannot extrapolate a
sample compressed size without including full-chunk fixed overhead.

## 12. Decision Determinism

Deterministic mode fixes:

- sample positions;
- candidate enumeration and tie order;
- measurement-independent scoring inputs;
- dictionary insertion order;
- codec implementation and settings;
- floating-point-free or exactly specified score comparisons.

Wall-clock trial timings cannot influence deterministic selection. Deterministic
profiles use benchmark-calibrated static cost classes and measured output bytes.

Non-deterministic mode may incorporate local timing, but the container remains
fully portable and records the actual selected chain.

## 13. Tie Breaking

After hard limits and scores, ties resolve in this order:

1. fewer complete serialized bytes;
2. lower declared decode cost class;
3. lower declared scratch memory;
4. fewer transformation stages;
5. lower codec identifier;
6. lexicographically lower encoding identifier sequence.

Tie rules are stable within an engine policy version.

## 14. Policy Version

The engine reports:

- policy name;
- policy algorithm version;
- effective limits;
- selected candidate;
- explanation.

The policy version is not required to decode a container. It exists for
reproducibility and benchmark comparison.

Defaults may evolve between engine releases. A caller requiring repeatable
selection pins both engine and policy versions or uses deterministic explicit
chains.

## 15. Explanation Model

```text
DecisionExplanation
  field_id
  block_id
  policy_name
  policy_version
  observed_statistics
  sample_description
  candidates[]
  selected_candidate
  fallback_reason?
```

Each candidate includes:

- stage identifiers and parameters;
- eligibility;
- rejection reason when ineligible;
- estimate values and confidence;
- trial bytes when measured;
- hard-limit consumption;
- normalized costs and final score.

Explanations omit actual field values and dictionary contents.

## 16. Fallback Reasons

Raw selection is normal and carries one primary reason:

- `best_score`;
- `minimum_savings_not_met`;
- `incompressible_sample`;
- `chunk_too_small`;
- `analysis_budget_exhausted`;
- `memory_budget`;
- `no_compatible_candidate`;
- `caller_restriction`;
- `determinism_requirement`.

Raw selection is not reported as a warning unless the caller required
compression.

## 17. Budget Enforcement

Analysis has a child reservation within the operation budget. Candidate tasks
must acquire reservations before allocating dictionaries, trial buffers, or
codec contexts.

Budget exhaustion:

- rejects an optional candidate when a safe candidate remains;
- stops further trials when the analysis budget is exhausted;
- never removes raw;
- fails the operation only when a caller-required candidate or guarantee cannot
  be evaluated.

Time budgets are cooperative and checked at bounded work intervals. They are
not hard real-time guarantees.

## 18. Learning and Feedback

Format 1 policies are local and deterministic by default. The engine does not
silently train global models from user data.

Future learned models must be:

- explicitly enabled;
- versioned and inspectable;
- bounded by the same safety rules;
- excluded from deterministic mode unless the exact model is pinned;
- benchmarked against simpler policies;
- prevented from emitting unsupported chains.

Actual candidate outcomes may be returned to the embedding application for
external analysis without retaining source values.

## 19. Edge Cases

### All null

Store row count and validity efficiently; no value encoding is needed.

### Constant non-null

Frame-of-reference with bit width zero or RLE may win. Complete metadata cost is
compared with a compressed raw chunk.

### Already compressed binary

Entropy probes and codec trials should select raw when savings thresholds fail.

### Mixed distributions

Selection is per block, so later blocks may use different chains. The engine
does not split a caller block into hidden sub-blocks in the MVP.

### Tiny chunks

Avoid analysis beyond structural facts when maximum possible savings cannot
cover minimum overhead.

### Highly skewed strings

Dictionary caps prevent a few huge distinct values from consuming the operation
budget.

### Adversarial input

Analysis tables, runs, and samples have explicit caps. Hash collision behavior
must remain bounded or use hardened hashing.

## 20. Validation

Policy tests include:

1. Golden decisions for fixed statistics, policies, and registry capabilities.
2. Property tests that selected chains are compatible and reversible.
3. Budget tests proving candidate count, memory, and sample limits.
4. Tests that raw remains available under every standard policy.
5. Deterministic comparisons across worker counts and repeated runs.
6. Threshold tests immediately above and below minimum savings.
7. Tie-break tests for every ordering rule.
8. Explanation completeness and value-redaction tests.
9. Benchmarks covering favorable and hostile distributions per candidate.
10. Differential comparisons between estimates and actual outcomes to detect
    cost-model drift.
