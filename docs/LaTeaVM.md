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

