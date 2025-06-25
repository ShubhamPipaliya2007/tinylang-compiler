// Example: Inheritance in TinyLang
class Person {
    string name;
    int age;

    ComeAndDo greet() {
        print("Hello, my name is " + name + ", I am " + age + " years old.");
    }
}

class Student : Person {
    int studentId;
    ComeAndDo greet() {
        print("Hi, I'm student " + name + " (ID: " + studentId + ")");
        super.greet();
    }
}

Person p[2];
p[0].name = "Alice";
p[0].age = 30;
p[0].greet();

p[1].name = "Bob";
p[1].age = 25;
p[1].greet();

Student s;
s.name = "Charlie";
s.age = 20;
s.studentId = 12345;
s.greet(); // Should use Student's greet()
