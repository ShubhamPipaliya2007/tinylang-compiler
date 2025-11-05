// String Library - String utility functions

ComeAndDo greet(string name) {
    print("Hello, " + name + "!");
    return 0;
}

ComeAndDo sayGoodbye(string name) {
    print("Goodbye, " + name + "!");
    return 0;
}

ComeAndDo makeMessage(string prefix, string suffix) {
    return prefix + " " + suffix;
}

ComeAndDo repeat(string msg, int times) {
    int i = 0;
    while (i < times) {
        print(msg);
        i = i + 1;
    }
    return 0;
}
