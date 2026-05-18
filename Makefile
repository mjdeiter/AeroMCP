CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall \
            -I../fan-watch/imgui \
            -I../fan-watch/imgui/backends \
            -I/usr/include/nlohmann \
            $(shell pkg-config --cflags glfw3)

LDFLAGS  := $(shell pkg-config --libs glfw3) \
            $(shell pkg-config --libs libcurl) \
            $(shell pkg-config --libs sqlite3) \
            -lGL -ldl -lpthread

IMGUI_DIR := ../fan-watch/imgui
IMGUI_SRC := $(IMGUI_DIR)/imgui.cpp \
             $(IMGUI_DIR)/imgui_draw.cpp \
             $(IMGUI_DIR)/imgui_tables.cpp \
             $(IMGUI_DIR)/imgui_widgets.cpp \
             $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
             $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

SRC      := src/main.cpp src/gemini.cpp src/db.cpp src/executor.cpp
OBJ      := $(SRC:.cpp=.o) $(IMGUI_SRC:.cpp=.o)
TARGET   := AeroMCP

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
