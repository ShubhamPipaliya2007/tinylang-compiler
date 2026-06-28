// Math Library - Basic mathematical functions

ComeAndDo square(int x) {
    return x * x;
}

ComeAndDo cube(int x) {
    return x * x * x;
}

ComeAndDo add(int a, int b) {
    return a + b;
}

ComeAndDo subtract(int a, int b) {
    return a - b;
}

ComeAndDo multiply(int a, int b) {
    return a * b;
}

ComeAndDo max(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

ComeAndDo min(int a, int b) {
    if (a < b) {
        return a;
    }
    return b;
}

ComeAndDo abs(int x) {
    if (x < 0) {
        return -x;
    }
    return x;
}

ComeAndDo factorial(int n) {
    if (n == 0) {
        return 1;
    }
    int result = 1;
    int i = 1;
    for (i = 1; i < n + 1; i = i + 1) {
        result = result * i;
    }
    return result;
}

ComeAndDo power(int base, int exp) {
    if (exp == 0) {
        return 1;
    }
    int result = 1;
    int i = 0;
    while (i < exp) {
        result = result * base;
        i = i + 1;
    }
    return result;
}
