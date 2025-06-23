// Example: Basic class definition and usage in TinyLang
class Person {
    string name;
    int age;

    ComeAndDo init(string n, int a) {
        name = n;
        age = a;
    }

    ComeAndDo greet() {
        print("Hello, my name is " + name + ", I am " + age + " years old.");
    }
}

Person p("Alice", 15);
p.greet();
