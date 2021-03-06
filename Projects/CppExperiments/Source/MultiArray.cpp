



/*
Maybe, we make the storage layout user adjustable? Modifications that would be necessarry in order
to let the first index have a stride of 1 instead of the last (resulting in column-major storage in
case of 2D arrays): 

the loop in updateStrides would have to become:
int i = rank-1; int s = 1; while(i >= 0) { strides[i] = s; s *= shape[rank-i-1]; --i; }

the ()-operator would become:
return data[ flatIndex((int)shape.size()-1, i, rest...) ];

index computaion in flatIndex(int depth, int i, Rest... rest) would become:
return flatIndex(depth, i) + flatIndex(depth-1, rest...);



implement it in a similar way as rsMatrix - factor out class MultiArrayView
maybe make a class MultiIndex such that we can write things like
for(MultiIndex i = {0,0,0}; i < {2,4,3}; i++) A(i) = ... // A is MultiArray
the operator ++ must be implemented such that it counts up the last index, then wraps back back
to zero while incrementing second-to-last, etc.

overload the () operator of rsMultiArray, so it can take an MultiIndex, a vector of int (reference)
maybe a tuple of ints,a c-array of indices - so the class should support various syntaxes for
indexing elements - we will then need various versions of the flatIndex() function as well


operations: outer-product (tensor-product?), inner product, contraction (with respect to a pair
of indices)...but maybe these should be in the Math section whereas MutiArray itself should go
into the Data section

use 3D arrays to simulate the wave equation in rooms and tubes (cylidrical coordinates)...although, 
the probelm is rotationally symmetric, so a 2D model (in r,z) should suffice, phi is not needed 
- the dependence on r is probably weak as well - but just for proof of concept
-implement functions that approximate the Laplacian numerically in cartesian and cyclindrical
coordinates...how about 4D arrays for spacetime simulations?


*/