CXX      = g++
CXXFLAGS = -std=c++17 -Wall \
           -Ilanguage/lexer \
           -Ilanguage/parser \
           -Ilanguage/semantic \
           -Ilanguage/optimizer \
           -Ilanguage/runtime

SRC = language/main.cpp \
      language/lexer/lexer.cpp \
      language/parser/parser.cpp \
      language/semantic/semantic.cpp \
      language/optimizer/irgen.cpp \
      language/optimizer/iropt.cpp \
      language/optimizer/cfg.cpp \
      language/optimizer/bytecode.cpp \
      language/runtime/irvm.cpp

HEADERS = language/lexer/lexer.hpp \
          language/parser/parser.hpp \
          language/parser/ast.hpp \
          language/semantic/semantic.hpp \
          language/optimizer/ir.hpp \
          language/optimizer/irgen.hpp \
          language/optimizer/iropt.hpp \
          language/optimizer/cfg.hpp \
          language/optimizer/bytecode.hpp \
          language/runtime/irvm.hpp

TARGET = tinylang

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
