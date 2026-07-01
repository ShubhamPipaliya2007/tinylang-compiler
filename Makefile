CXX      = g++
CXXFLAGS = -std=c++17 -Wall \
           -Icompiler/frontend \
           -Icompiler/middleend \
           -Icompiler/backend \
           -Icompiler/common \
           -Iruntime/vm \
           -Iruntime/heap

SRC = compiler/cli/main.cpp \
      compiler/frontend/lexer.cpp \
      compiler/frontend/parser.cpp \
      compiler/frontend/semantic.cpp \
      compiler/middleend/irgen.cpp \
      compiler/middleend/iropt.cpp \
      compiler/middleend/cfg.cpp \
      compiler/middleend/tirgen.cpp \
      compiler/common/tir.cpp \
      compiler/backend/bytecode.cpp \
      compiler/backend/llvmgen.cpp \
      runtime/vm/irvm.cpp \
      runtime/vm/tirvm.cpp

HEADERS = compiler/frontend/lexer.hpp \
          compiler/frontend/parser.hpp \
          compiler/frontend/ast.hpp \
          compiler/frontend/semantic.hpp \
          compiler/common/ir.hpp \
          compiler/common/tir.hpp \
          compiler/middleend/irgen.hpp \
          compiler/middleend/iropt.hpp \
          compiler/middleend/cfg.hpp \
          compiler/middleend/tirgen.hpp \
          compiler/backend/bytecode.hpp \
          compiler/backend/llvmgen.hpp \
          runtime/heap/object.hpp \
          runtime/vm/irvm.hpp \
          runtime/vm/tirvm.hpp

TARGET  = tinylang
TESTDIR = tests
EXDIR   = examples

.PHONY: all clean test examples

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

test: $(TARGET)
	@echo "=== Semantic error tests ==="
	@./$(TARGET) $(TESTDIR)/semantic/test_semantic_errors.tl 2>&1 || true
	@echo "=== Integration: import ==="
	@./$(TARGET) $(EXDIR)/test_import.tl
	@echo "=== Integration: multi-import ==="
	@./$(TARGET) $(EXDIR)/test_multi_import.tl
	@echo "=== Integration: sample ==="
	@./$(TARGET) $(EXDIR)/sample.tl

examples: $(TARGET)
	@for f in $(EXDIR)/*.tl; do \
	    echo "--- $$f ---"; ./$(TARGET) $$f || true; \
	done

clean:
	rm -f $(TARGET)
