# ============================================================
#  Makefile – Ubuntu Store
#  Yêu cầu: g++ >= 11, GTK3, pkg-config
# ============================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 \
             $(shell pkg-config --cflags gtk+-3.0) \
             -Iinclude
LDFLAGS  := $(shell pkg-config --libs gtk+-3.0) -lstdc++fs

TARGET   := ubuntu-store
BUILDDIR := build
SRCDIR   := src

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all clean run install-deps

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo ""
	@echo "✔  Build thành công: ./$(TARGET)"
	@echo "   Chạy: make run"

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

# Cài đặt dependencies trên Ubuntu/Debian
install-deps:
	sudo apt-get install -y libgtk-3-dev pkg-config build-essential
