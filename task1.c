/**********************************************************************************************

At the first looking at the problem, the thought comes to using the algorithm 
“searching for the desired value in a sorted array,”. But this will require 
additional memory and time to sort the array. 

If we need to avoid the consumption of additional memory and optimize calculations, 
we should use bit logic and specifically the XOR operation.

Here is the truth table:

x |	y | x ^ y
---------------
0 |	0 |   0
0 |	1 |   1
1 |	0 |   1
1 |	1 |   0

And some useful properties:

x ^ x = 0
x ^ 0 = x
x ^ y = y ^ x

So:

x ^ x ^ x = x ^ 0 = x

And:

a ^ b ^ c ^ a ^ b  =  a ^ a ^ b ^ b ^ c  =  0 ^ 0 ^ c  =  c

If we have a sequence of XOR operations a ^ b ^ c ^ ..., we can remove all pairs of 
duplicate values from it and this will not affect the result.

All other elements cancel each other out because they occur exactly twice.

**********************************************************************************************/

//
//
//
int32_t findUnique(int32_t *a, int32_t n) 
{
  int32_t result = 0;

  for(int32_t i = 0; i < n; i++) 
  {
    result ^= a[i];
  }

  return result;
}

//
//
//
void main(void)
{
  int32_t array[] = {2, 4, 6, 8, 10, 6, 4, 2, 8};

  int32_t n = sizeof(array) / sizeof(array[0]);
  int32_t res = findUnique(array, n);
}