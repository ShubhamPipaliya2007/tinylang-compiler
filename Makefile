CXX = g++
CXXFLAGS = -std=c++17 -Wall

SRC     = main.cpp lexer.cpp parser.cpp semantic.cpp irgen.cpp irvm.cpp
HEADERS = lexer.hpp parser.hpp ast.hpp semantic.hpp ir.hpp irgen.hpp irvm.hpp
TARGET  = tinylang

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
