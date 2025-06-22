// ... existing code ...
// Test for global and local variable scoping
int x = 10;
print(x); // Should print 100

ComeAndDo testScope() {
    int x = 42 + 10;
    print(x); // Should print 42 (local)
}

testScope();
print(x); // Should print 10 (global)
// ... existing code ...