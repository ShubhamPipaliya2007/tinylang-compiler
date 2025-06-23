// Example: Basic class definition and usage in TinyLang
class Person {
    string name;
    int age;

    ComeAndDo greet() {
        print("Hello, my name is " + name + ", I am " + age + " years old.");
    }
}

Person p[2];
p[0].name = "Alice";
p[0].age = 30;
p[0].greet();

p[1].name = "Bob";
p[1].age = 25;
p[1].greet();
