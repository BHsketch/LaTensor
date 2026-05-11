# The Polyhedral Route
## Story
At some point, polyhedral transformations were done through pattern-matching over ASTs. The developers of Polly realized, however, that LLVM does a good job of lowering complicated syntax into simpler, more general IR, and also does a lot of heavy analyses that are important for polyhedral optimizations. Polly leverages these to specialize these generic transformations for their use-case which was polyhedral optimizations. In order to translate cpp to TVM, hence, which is arguably a more specific problem than what polly achieves, we use Polly itself as a base similar to how Polly uses LLVM: (1) Polly does a lot of heavy-lifting for us in terms of scop detection and (2) from what we understand, TVM semantics are (almost) a subset of what polyhedral frameworks consider valid.

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
What does it really mean to have a "baked in" schedule and how would one go about recovering the high level information from it? 

The point of scheduling is to change the order in which one visits each dimension of a domain. 
Normally we say "I want to perform the same computation for all points in a certain domain; here is the domain, now find the best way to iterate over it". The way we define this domain is that we give ranges for some variables, and the lexicographical order of the values taken by this list of variables forms the "default" iteration order that we must improve. 
When we pre-optimize something, what we're basically doing is defining a different set of variables whose lexicographical order take us through the same space but along a different path. A pre-tiled matmul for instance will have six loop variables, and incrementing them in the natural manner, with the provided memory access expressions, will take you on a journey through the 3D domain one small cube at a time. Both are different default traversal patterns that can be converted to each other using a sequence of primitive loop transformations: namely loop fusion, loop splitting, and loop reordering. Given this fact, it would seem that both of these orderings are equally good "default" orderings for TVM to begin with. So why would it be a problem if we have a pre-tiled matmul supplied to TVM?

Well, if TVM were exploring the exhaustive search space of possible schedules for something, it would probably find the best schedule in both cases and we wouldn't have to worry. However, what TVM really does is a guided search over the space of transformations that _start with your default configuration and then try to improve it_. Two things that affect what schedules are finally found in both cases depend on:
1. The ease with TVM can go from the simple to the tiled version, vs the tiled to the simple version --- the search space one would have to explore to reach from A to B might be different than the one needed to reach from B to A.
2. What TVM things are "good" transformations given a starting point --- If TVM thinks splitting is worth exploring rather than fusion, then it'll take the already tiled loop and try to tile it even more; rather than turning it into a simple matmul, followed by trying an alternative first-level tiling size.

Ease of conversion: For loops with no loop carried dependencies it doesn't really matter how one arranges these iterations and the case is boring to analyze. So for now let's say our loop has loop carried dependencies (In a matmul, the sum requires partial results). For a loop carried dependency of level K, splitting a loop at level K-k, and then transferring it inside of level K is different from sending a loop from within level K outside, then fusing it with some other loop. One might be harder than the other.

### In summary
For any user-written code that is already partially scheduled, optimally tuning it in TVM would require that we first recover necessary high-level information, and give TVM the right distinction between what belongs to the domain and what belongs to the schedule.

## Finding the operation
While the JSCoP gives us the distinction between the program and the schedule, it doesn't give us the actual operation being performed. Generally, our hypothesis is that we'll need a lot of additional information from the source code, along with the polly results in order to generate the final TVM, which means instead of running polly as a stand-alone tool, it might be better to call it as a plugin from a general lowering pipeline. As a result ... \<write more about the plugin approach\> 

## Detecting Reductions
Apparently, there's some analysis theory dedicated to detecting reductions (check Allen and Kennedy, Section 5.6).
<img width="1047" height="625" alt="image" src="https://github.com/user-attachments/assets/be51d4ff-cc75-4e6e-adc4-119eb794673c" />

The full text can be found in the book by Allen and Kennedy (Optimizing Compilers for Modern Architectures), but the summary is that there are three properties of a computation that can serve as necessary and sufficient conditions for being a reduction:
1. They reduce the elements of some vector or array dimension down to one element. This property can be checked by looking at the dependence graph for the corresponding statement. The condition is true if the dependence graph has all three kinds of dependencies:
   - an output dependence, because we always aggregate in the same final scalar variable
   - a true dependence, because the partially accumulated result is read in subsequent iterations
   - an antidependence, because the partial results are rewritten
2. Only the final result of the reduction is used later
3. There is no variation inside the intermediate accumulation. That is, the reduction operates on the vector and nothing else.

Particularly, if a computation follows all three rules, it's definitely a reduction. If it follows the first and third rules, but not the second, (i.e. if the intermediate results are used as well), it's no longer a reduction, but a scan. TODO: What computation is represented by only not following the third rule?

The key observation here is that it's not that TVM is incapable of representing these patterns, but that to infer which _kind_ of reduction we need in our generated TVM code, we will have to do some additional analysis on the JSCoP by looking at all the reads and writes it has detected inside the SCoP, forming a dependence graph out of them, and seeing what constraints from above are satisfied/violated.

### Detection of Pure reductions
These are reductions where all three conditions are met. Detecting these is simple beacause Polly tells us of their existence through "reduction types" --- a non-null reduction type would mean polly detected a reduction and told us the reduction operator being used.

### Detection of Scans
These are reductions that violate the second condition. In these cases, polly does not tell us that an operation is a scan; hence, we must detect it ourselves by reconstructing the dependence graph from the JSCoP. Luckily, TVM provides some functionality to reconstruct dependency graphs as well, so our job is reduced to just verifying that they follow the required pattern.
