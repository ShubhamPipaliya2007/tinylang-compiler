// Example: Basic class definition and usage in TinyLang
class Person {
    string name;
    int age;

    ComeAndDo greet() {
        print("Hello, my name is " + name + ", I am " + age + " years old.");
    }
}

Person p;
p.name = "Alice";
p.age = 30;
p.greet();
