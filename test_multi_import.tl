// Test multiple imports
import "math_lib.tl";
import "string_lib.tl";

print("=== Testing Multiple Imports ===");
print("");

// Math tests
print("Math Library:");
int squared = square(7);
print("7 squared = " + squared);

int sum = add(15, 25);
print("15 + 25 = " + sum);

// String tests
print("");
print("String Library:");
greet("World");
sayGoodbye("Alice");

string combined = makeMessage("Hello", "TinyLang!");
print(combined);

print("");
repeat("Success!", 2);

print("");
print("Multiple imports working perfectly!");
