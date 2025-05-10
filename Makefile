# Define the target executable name
TARGET = ngTerm

# Define the source files
SOURCES = main.cpp Connections.cpp

# Define the C++ compiler to use
CXX = g++

# Get compiler flags (include paths) from pkg-config for GTKmm and VTE
GTK_CFLAGS = $(shell pkg-config --cflags gtkmm-3.0 vte-2.91)

# Get linker flags (library paths and library names) from pkg-config for GTKmm and VTE
GTK_LIBS = $(shell pkg-config --libs gtkmm-3.0 vte-2.91)

# Define general CXXFLAGS
# -std=c++17: Specify C++17 standard for filesystem support
# -Wall: Enable all common warnings (good practice for development)
# -Wno-deprecated-declarations: Suppress warnings about deprecated declarations
CXXFLAGS = -std=c++17 -Wall -Wno-deprecated-declarations $(GTK_CFLAGS)

# Define linker flags to include filesystem library
LDFLAGS = -lstdc++fs

# Default target (builds the executable)
all: $(TARGET)

# Rule to build the executable from the source files
$(TARGET): $(SOURCES)
	$(CXX) $(SOURCES) -o $@ $(CXXFLAGS) $(LDFLAGS) $(GTK_LIBS)

# Clean rule: removes the compiled executable and any other build artifacts
clean:
	rm -f $(TARGET)