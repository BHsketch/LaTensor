# Knowledge notes: loop-nest extraction for TVM offload

Background reference for the `latensor` tool. Goal of the tool: given arbitrary C++, identify loop-nest subregions that can be safely translated to TVM TIR, let TVM meta_schedule tune them, and re-emit the C++ with calls into the generated `.so`. These notes cover (1) how to approach the "what is safe to extract" problem, (2) what polyhedral optimizers actually do and how they relate to TVM, and (3) the concrete gap between "is a SCoP" and "TVM will handle it correctly and fast."

---

## 1. Approach: how to detect TVM-safe loop nests

Three candidate directions were on the table:

1. Build on LLVM's existing loop distribution / related passes.
2. Enumerate TVM's assumptions from scratch and detect them on the AST.
3. Use LLVM Polly.

**Recommendation: hybrid of (2) and (3).** Use Polly's SCoP definition as the *specification* of "what's safely TVM-compatible," but keep detection at the Clang AST level where the existing plugin already lives. Reason: the end goal is to rewrite C++ source to call a generated library, and Polly operates on LLVM IR — bridging IR-level analysis back to source edits is painful, whereas SCoP criteria (affine bounds, affine subscripts, no aliasing, side-effect-free bodies, statically analyzable control flow) translate cleanly into a checklist you can enforce on the AST.

**Tradeoff vs. pure option (2):** you reinvent some affine-analysis machinery that Polly/isl already have (loop-bound normalization, dependence vectors for splitting). That's fine for matmul-shaped kernels but gets painful for real-world messy loops with triangular bounds, strided accesses, or conditional stores.

**Why not LLVM loop distribution (option 1):** it splits loops to enable vectorization, not to extract "maximally affine" subregions, and its output is still LLVM IR. Treat it as prior art to read rather than something to build on.

### Concrete starting points

- **Polly's SCoP detection criteria** — `llvm/lib/Analysis/ScopDetection.cpp` in the Polly tree. Clearest written-down list of "what a polyhedral tool considers safe." TVM's assumptions are a strict subset.
- **TVM TIR block semantics** — `T.reads` / `T.writes` / `T.axis.remap("SSR", ...)` encode exactly the spatial-vs-reduction split the current plugin is already detecting. Reading the TIR "block" spec and working backward answers: what must be true of a C++ loop for those annotations to be derivable?
- **Dependence analysis gap in the current plugin** — the matcher flags loop vars but doesn't reason about cross-iteration dependences on array writes (e.g., `C[j][k] += ...` inside an `i`-loop has a loop-carried dep on `i` only if the same `(j,k)` is written twice). `clang::ASTMatchers` alone won't get this; walk subscripts and check affine form yourself, or lower a single function to LLVM IR and borrow `DependenceAnalysis` for just that function.

---

## 2. What polyhedral optimizers do, and how they relate to TVM

### The polyhedral model

A loop nest is modeled as a set of integer points in Z^n. Three objects:

1. **Iteration domain** — every dynamic instance of every statement, expressed as the integer points of a polyhedron defined by affine inequalities on the loop variables. `for i in [0,N): for j in [0,i):` becomes `{(i,j) | 0 ≤ j < i < N}`.
2. **Access functions** — each array subscript is an affine function of the iteration point. `A[2*i+j]` → `f(i,j) = 2i+j`.
3. **Schedule** — an affine map from iteration points to a logical execution time. The original program's schedule is lexicographic order. "Optimizing the loop" means finding a *different* affine schedule that (a) respects all dependences and (b) has better properties (parallelism, locality, tileability).

Given that representation, exact dependence analysis reduces to integer linear programming (solved by libraries like `isl`), and transformations like tiling, skewing, interchange, fusion, distribution, and parallelization all become algebraic manipulations of the schedule. Code generation (via `isl`/CLooG) emits loops from the transformed schedule.

Famous instances: **Pluto** (classic academic optimizer), **Polly** (LLVM), **PPCG** (GPU-targeted), **MLIR affine dialect**, **Tiramisu**.

### Relation to TVM

Substantial overlap in goals (tile, reorder, parallelize dense loop nests for locality and throughput). TVM's TIR is designed to be polyhedral-friendly — the `T.axis.remap("SSR", ...)` / `T.reads` / `T.writes` annotations exist precisely so affine analysis is trivial. Philosophies diverge sharply on two axes:

**Analytical vs. search.** Pluto/Polly pick *one* schedule by solving a cost-model ILP. TVM's meta_schedule *samples thousands* of schedules, measures them on real hardware, and trains an ML cost model. On modern machines analytical cost models tend to lose to search — a big part of why TVM exists.

**General-purpose vs. DSL.** Polyhedral tools extract affine regions from arbitrary C/Fortran/LLVM-IR and must gracefully reject non-affine code. TVM is a compiler for a DSL you *write in*; it assumes the input already fits the model. TVM also ships a large operator library, quantization story, and multi-backend codegen (CPU/GPU/NPU/edge) that polyhedral tools generally don't.

### Implication for this project

The set "loop nests TVM can meaningfully optimize" is very close to "SCoPs in the polyhedral sense." That's not a coincidence — TVM TIR is essentially a restricted polyhedral IR with explicit reduction axes. So:

- Polly's **SCoP detection** is almost exactly the "is this safe to hand to TVM?" predicate.
- Polly's **dependence analysis** is what you need to decide whether a subregion can be split off without changing semantics.
- What Polly does *next* — picking an analytical schedule — is the step you'd replace with "hand it to TVM and let meta_schedule search."

Framing: **use polyhedral extraction as the frontend, use TVM meta_schedule as the backend scheduler.** Novelty is the glue (re-emitting C++ with calls into the generated `.so`).

---

## 3. Corner cases: where SCoP ≠ TVM-compatible

Gaps fall into three buckets: correctness hazards (silent wrong answers), performance cliffs (TVM accepts it but can't tune it well), and glue work (expressible, but translation required).

### 3.1 Correctness hazards — SCoP says "safe" but handing to TVM changes observable behavior

**Pointer aliasing.** Polly emits *runtime alias checks* and falls back to the original loop if they fail. TVM has no such fallback — the `tirx.noalias` attribute is a *promise* you make. If the extracted region has `float* A, float* B, float* C` parameters from an outer C++ function, emit the alias check in your glue code and only dispatch to the TVM kernel when `A`, `B`, `C` are disjoint. Get this wrong → silent memory corruption.

**Floating-point reassociation.** Polyhedral tools and TVM both freely reorder and parallelize reductions. IEEE 754 addition is *not* associative; a sum computed by a tuned TVM kernel is not bit-identical to the source. Fine for ML, potentially wrong for scientific/financial code. Need a policy: refuse FP reductions, require a `#pragma` opt-in, or document the divergence.

**Reduction forms TVM recognizes.** TVM's combiner set is essentially `{sum, prod, max, min, and, or}` out of the box (and `te.comm_reducer` for registered custom ones). The current Clang matcher accepts any of `+= -= *= /= %=`. Of those, `-=` and `/=` are *not* associative/commutative and should not be handed to TVM's reduction machinery as-is — `sum -= x` is fine (rewrite as `sum += -x`), but `/=` in a reduction is a trap.

**Integer overflow and signedness.** SCoP doesn't care about dtype; TVM does. Signed overflow is UB in C++, wraps in TVM. If the source relied on trap-on-overflow via `-ftrapv`, that's lost. `long double`, `__int128`, bitfields aren't representable.

**Volatile, atomic, `fenv` state.** SCoP detection should reject volatile (Polly does) — mirror this. TVM-generated code does not preserve floating-point environment state (rounding mode, exceptions).

### 3.2 Performance cliffs — TVM accepts it, meta_schedule tunes poorly

**Conditionals inside the body.** SCoPs allow affine `if`. TIR can express it with `T.if_then_else`, but meta_schedule's tuning templates are built around dense rectangular loops; conditional-heavy bodies lose most of the tiling benefit and often run slower than the naive C++.

**Parametric shapes.** SCoPs naturally parameterize over `N`. TVM's meta_schedule tunes far better with *concrete* sizes than with symbolic ones — the tuning signal weakens and vectorization/tensorization templates become conservative. Decision required: one `.so` per size (like the current `unified/` setup), or one shape-polymorphic `.so` (cheaper to ship, noticeably slower). No good middle ground without shape-bucketing.

**Non-contiguous strides.** SCoP analysis is agnostic to strides. TVM supports strided tensors, but the good tuning templates (vectorize, tensorize) assume contiguity. A nest over a column slice of a row-major array will run but won't be fast.

**Modulo / floor-div in subscripts.** Polyhedral tools handle these (semi-affine). TVM accepts them; meta_schedule's tile/vectorize templates don't optimize them well.

**Multi-statement SCoPs with different domains.** Polly happily optimizes a SCoP containing several statements with different iteration domains (and can fuse them). TVM prefers one block per output tensor. Extracting a multi-statement SCoP wholesale may produce a TIR module meta_schedule can't do much with.

### 3.3 Glue work — translation is straightforward but non-trivial

**Flat vs. multi-dim indexing.** `A[i*N+j]` and `A[i][j]` are both affine in SCoP. TVM wants a 2D `T.Buffer((N, N), ...)`. The translator has to recover the original shape from flat indexing (fine for `i*N+j` patterns; painful when stride variables aren't obvious).

**Parameters vs. induction variables.** SCoPs distinguish program parameters (loop-invariant symbols) from induction variables. TIR distinguishes buffer shape variables, function arguments, and loop iters. Mapping is mostly mechanical but must be made explicit.

**Spatial vs. reduction axis assignment.** The existing plugin already does a version of this. The SCoP view is cleaner: an induction variable is a reduction axis iff it does not appear in the write's access function but appears in a read's. Handles cases like `C[i] += A[i][k] * B[k]` correctly.

**Data-dependent loop bounds.** `for (i=0; i < *ptr; ++i)` — Polly can treat `*ptr` as a parameter specialized at region entry. TVM wants the value as a shape argument. Doable, but requires hoisting the read, guarding against concurrent modification, and passing the value in.

**Dynamic output sizes.** Filter/gather patterns (`if (pred) out[j++] = x[i]`) are not SCoPs in Polly's default mode and definitely not TIR. Reject early.

### 3.4 Suggested priorities

Harden **correctness first**:

1. The aliasing-check-and-fallback pattern is the single most important piece of glue. Without it, the tool silently corrupts memory on real-world code.
2. FP-reassociation policy (opt-in via pragma or comment directive) is the next correctness gate.
3. Performance cliffs are all discoverable empirically — measure the TVM kernel against the original and fall back when it's slower.

**Concrete suggestion:** build a small corpus of "near-SCoP" C++ loop nests exercising each hazard (aliasing, FP reduction, conditionals, parametric size, strided access, multi-statement) and use it as a regression suite from day one. It pays for itself the first time a "fix" silently breaks another case.
