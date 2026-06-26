CXX = g++
CXXFLAGS = -std=c++17 -Wall

SRC = main.cpp lexer.cpp parser.cpp codegen.cpp semantic.cpp
HEADERS = lexer.hpp parser.hpp ast.hpp semantic.hpp
TARGET = tinylang.exe

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	del /Q $(TARGET)
