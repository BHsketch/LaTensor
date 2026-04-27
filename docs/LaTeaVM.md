# The Polyhedral Route
## Shape Problem
Apparently polyhedral optimizers like polly will isolate, not rectangular, but *convex* iteration spaces --- where the iteration variables are affine functions of the iteration variables surrounding them. So as a first option we could trivially filter out only rectangular iteration spaces, but if we wanted to deal with other spaces as well we can:
1. split them up into rectangular and non-rectangular regions. The problem here is that if the split contains multiple rectangular regions, TVM will have to tune for all of them separately which will lead to very long compilation times.
2. split them up, but make sure all rectangular regions are made up of tiles of a single size (so as to avoid multiple tuning sessions): Here the tuning will have to be done for only one tile, but it might not work great in general if these tiles are too small etc.
3. masking: create a rectangular bounding box around the iteration space, then add a conditional within the loop body that skips computation on whatever is out of bounds for us.
### Masking
Let's say we go the masking route. Here, too we need to be careful on how to generate loops. Consider the following two versions of an element-wise update:
![[Pasted image 20260423162323.png]]

In the first version, we have an `if vj<vi` which prevents TVM from vectorizing the statements that follow. Ideally though, one could have done redundant computations to get any parallelism that it can, and then discard values we don't need. The latter is what is done in the second version using the `T.if_then_else()` construct.

### Considerations when translating 
Here we focus on how to filter Polyhedral-safe loops into TVM-safe ones,
since the latter will be a subset of the former.
- multi writes in a single block
- conditionals -> if_then_else
- scalars, temps -> need buffer
- iteration rectangularity
- function calls must inline
- aliasing must be enforced
- loop canonicalization

## The overall flow
- we put c(++) code into llvm to make ir
- run opt to do: inlining
- we run the scop detection on it
  - we get loop nests list to get nice loops
- run polly and export JSCoP.
- filter JSCoP files to get super nice loops
- write a simple parser and translator to convert JSCoP into TVM code

# JSCoP to TVM:
## Interpreting schedules:
### Case 1: Input is a naive matmul, but polly tiles internally.

We'll get a domain and schedule of this form:
```
"domain": "{ Stmt2[i0, i1, i2] : 0 <= i0 <= 127 and 0 <= i1 <= 127 and 0 <= i2 <= 127 }",
"name": "Stmt2",
"schedule": "{ Stmt2[i0, i1, i2] -> [o0, o1, o2, i0 - 32o0, i1 - 32o1, i2 - 32o2] : -31 + i0 <= 32o0 <= i0 and -31 + i1 <= 32o1 <= i1 and -31 + i2 <= 32o2 <= i2 }"
```
The domain just specifies the iteration space over which we evaluate the operation. The "actual" iteration space though can traverse the domain in any order using any number of iteration variables --- in tiled code, a loop with three nests becomes a loop with six nests. The schedule simply does the job of mapping the variables in the three-nest into the ones in the six-nest using expressions and constraints. The first three dimensions o0, o1 and o2 are fresh variables rather than being expressions in terms of i0, i1 and i2, because the internal representation does not support the "//" operation we would need to represent the same. We can ignore such quirks for the time being but it's good to be mindful about how they affect the JSCoP file.

### Case 2: Input is a manually tiled matmul of this form: 
```
extern "C" void matmul_tiled(const float* A, const float* B, float* C, int N) {
    N = 128;
    const int T = 32;  // tile size
    for (int ii = 0; ii < N; ii += T) {
        for (int jj = 0; jj < N; jj += T) {
            for (int kk = 0; kk < N; kk += T) {
                // Mini-kernel: T x T x T multiply on the current tile
                for (int i = ii; i < ii + T; i++) {
                    for (int j = jj; j < jj + T; j++) {
                        for (int k = kk; k < kk + T; k++) {
                            C[i * N + j] += A[i * N + k] * B[k * N + j];
                        }
        ..........
}
```
Here, even though tiling is effectively being done in the same way, we get the following JSCoP format:

```
   "statements": [
      {
         "accesses": [
            {
               "kind": "write",
               "relation": "{ Stmt5[i0, i1, i2, i3, i4, i5] -> MemRef0[4096i0 + 32i1 + 128i3 + i4] }"
            },
            {
               "kind": "read",
               "relation": "{ Stmt5[i0, i1, i2, i3, i4, i5] -> MemRef0[4096i0 + 32i1 + 128i3 + i4] }"
            },
            {
               "kind": "read",
               "relation": "{ Stmt5[i0, i1, i2, i3, i4, i5] -> MemRef2[4096i0 + 32i2 + 128i3 + i5] }"
            },
            {
               "kind": "read",
               "relation": "{ Stmt5[i0, i1, i2, i3, i4, i5] -> MemRef3[32i1 + 4096i2 + i4 + 128i5] }"
            }
         ],
         "domain": "{ Stmt5[i0, i1, i2, i3, i4, i5] : 0 <= i0 <= 3 and 0 <= i1 <= 3 and 0 <= i2 <= 3 and 0 <= i3 <= 31 and 0 <= i4 <= 31 and 0 <= i5 <= 31 }",
         "name": "Stmt5",
         "schedule": "{ Stmt5[i0, i1, i2, i3, i4, i5] -> [i0, i1, i2, i3, i4, i5] }"
      }
   ]

```
We would like to point out two interesting aspects of this output:
1. We can see that all the iteration variables have been automatically cannonicalized by Polly: For instance, here `i0` and `i3` in the JSoC map to `ii` and `i` in the input C++. We can see that i0 iterates from 0 to 4 with steps of 1 instead of from 0 to 128 with steps of 32. Similarly, i3 iterates from 0 to 32 rather than from some ii to ii+32. **This is helpful**, since TVM would expect cannonical loops.
2. The domain is no longer 3D. We're now dealing with a 6D domain, with 6D schedule variables mapping trivially to the domain variables. i.e. the high level information of "i0 and i3 were actually obtained by splitting a single input domain axis" is lost. As a result, converting this to TVM would lead to a "baked in" tiled schedule that it might not be able to tune over. This may or may not lead to TVM not being able to perform some additional fancy autoscheduling such as fusing or reordering these splitted axes. **This is unhelpful** because TVM expects simple programs that it later does composable optimizations on.

#### A little more discussion on the latter concern
What does it really mean to have a "baked in" schedule and how would one go about recovering the high level information from it? The point of scheduling is to change the order in which one visits each dimension of a domain. If N dimensions of the domain map to M dimensions of the schedule, where N < M, then the binding expressions in TVM tell us: "this input array that seems to be M dimensional, is actually N dimensional, and you can access it by just N variables $v_0$ through $v_{N-1}$ where $v_i$ is computed by some expression containing potentially multiple variables from the M-dimensional schedule. 

Why does it matter what shape we "see" as long as the buffer is 1-dimensional in both cases? Isn't it merely a matter of substituting complex expressions with some dummy variables for the sake of readability? Well maybe we want to start with this perspective instead: if recovering high level information of "this was actually tiled from a lower-dimensional domain" was trivial, then we'd be golden. Why is this not trivial? 

### In summary
For any user-written code that is already partially scheduled, optimally tuning it in TVM would require that we first recover necessary high-level information, and give TVM the right distinction between what belongs to the domain and what belongs to the schedule.
