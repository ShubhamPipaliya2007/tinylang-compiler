CXX = g++
CXXFLAGS = -std=c++17 -Wall

SRC     = main.cpp lexer.cpp parser.cpp semantic.cpp irgen.cpp irvm.cpp iropt.cpp cfg.cpp bytecode.cpp
HEADERS = lexer.hpp parser.hpp ast.hpp semantic.hpp ir.hpp irgen.hpp irvm.hpp iropt.hpp cfg.hpp bytecode.hpp
TARGET  = tinylang

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
