// Example: Demonstrating TinyLang's Import System (like Python!)

// Import custom libraries - you can import multiple files!
import "math_lib.tl";
import "string_lib.tl";

print("=== TinyLang Import System Demo ===");
print("");

// Using imported math functions
print("Math Library Functions:");
int num = 5;
print("Factorial of " + num + " = " + factorial(num));
print("Square of " + num + " = " + square(num));
print("Cube of " + num + " = " + cube(num));

int x = 10;
int y = 3;
print("");
print("Math operations on " + x + " and " + y + ":");
int sum = add(x, y);
int diff = subtract(x, y);
int prod = multiply(x, y);
print("  Add: " + sum);
print("  Subtract: " + diff);
print("  Multiply: " + prod);

print("");
print("Power calculations:");
int pow1 = power(2, 10);
int pow2 = power(3, 4);
print("  2^10 = " + pow1);
print("  3^4 = " + pow2);

// Combine functions creatively
print("");
print("=== Creative Combinations ===");
int base = 5;
int exponent = 3;
int result = power(base, exponent);
print("Calculating " + base + "^" + exponent + " = " + result);

int doubled = multiply(result, 2);
print("Doubled: " + doubled);

int squared_doubled = square(doubled);
print("Squared: " + squared_doubled);

// Using imported string functions
print("");
print("=== String Library Demo ===");
int dummy1 = greet("TinyLang Users");
int dummy2 = sayGoodbye("Bugs");

print("");
int dummy3 = repeat("Imported functions work!", 2);

print("");
print("SUCCESS! Multi-import system working!");
print("You can import multiple libraries, just like Python!");
