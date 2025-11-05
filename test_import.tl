// Simple test of import functionality
import "math_lib.tl";

print("Testing imported math functions:");

int x = 5;
print("Square of 5: " + square(x));

int result = add(10, 20);
print("10 + 20 = " + result);

print("Factorial of 5: " + factorial(5));
print("Power 2^8 = " + power(2, 8));

print("Import test successful!");
