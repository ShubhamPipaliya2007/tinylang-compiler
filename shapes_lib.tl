// Shapes Library - Classes for different shapes

class Rectangle {
    int width;
    int height;

    ComeAndDo init(int w, int h) {
        this.width = w;
        this.height = h;
    }

    ComeAndDo area() {
        return this.width * this.height;
    }

    ComeAndDo perimeter() {
        return 2 * (this.width + this.height);
    }

    ComeAndDo display() {
        print("Rectangle: width=" + this.width + ", height=" + this.height);
    }
}

class Circle {
    int radius;

    ComeAndDo init(int r) {
        this.radius = r;
    }

    ComeAndDo area() {
        // Approximation: π ≈ 3
        return 3 * this.radius * this.radius;
    }

    ComeAndDo circumference() {
        return 2 * 3 * this.radius;
    }

    ComeAndDo display() {
        print("Circle: radius=" + this.radius);
    }
}

class Square : Rectangle {
    ComeAndDo init(int side) {
        this.width = side;
        this.height = side;
    }

    ComeAndDo display() {
        print("Square: side=" + this.width);
    }
}
