// Test for global and local variable scoping
int x = 10;
print(x); // Should print 10

ComeAndDo testScope() {
    int x = 42 + 10;
    print(x); // Should print 52 (local)
}

testScope();
print(x); // Should print 10 (global)

// Logical operators test
int a = 5;
int b = 10;
int c = 0;

print(a > 0 && b < 20);
print(a > 10 || b < 20);
print(!(a > 10));
print(a > 0 && c > 0);
print(a > 10 || c > 0);
print(!c);
