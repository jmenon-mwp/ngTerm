# Define the target executable name
TARGET = ngTerm

# Define the source files
SOURCES = main.cpp Connections.cpp Folders.cpp Ssh.cpp Config.cpp

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

# Define linker flags to include filesystem library and libuuid
LDFLAGS = -lstdc++fs -luuid

# Default target (builds the executable)
all: $(TARGET)

# Rule to generate icondata.h from PNG files
# Rule to generate icondata.h from PNG files
icondata.h: images/*.png
	@echo "Generating icondata.h..."
	@echo "// Generated icon data - DO NOT EDIT" > $@
	@echo "" >> $@
	@echo "#include <gdk/gdk.h>" >> $@
	@echo "" >> $@
	@for file in $^; do \
		base=$$(basename "$${file}" .png | sed 's/^images_//') && \
		xxd  -n $${base}_png -i "$${file}" >> $@; \
		echo "" >> $@; \
	done


# Rule to build the executable from the source files
$(TARGET): $(SOURCES) icondata.h
	$(CXX) $(SOURCES) -o $@ $(CXXFLAGS) $(LDFLAGS) $(GTK_LIBS)

# Clean target to remove generated files
clean:
	rm -f $(TARGET) icondata.h

.PHONY: clean
