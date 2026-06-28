CXX = g++
CXXFLAGS = -std=c++17 -Wall

SRC     = main.cpp lexer.cpp parser.cpp semantic.cpp irgen.cpp irvm.cpp iropt.cpp
HEADERS = lexer.hpp parser.hpp ast.hpp semantic.hpp ir.hpp irgen.hpp irvm.hpp iropt.hpp
TARGET  = tinylang

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
