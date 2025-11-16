CXX = g++
CXXFLAGS = -std=c++23 -O2 -Wall -Wextra -pthread
TARGET = async_server
SOURCES = main.cpp server.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean install uninstall package

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

install: $(TARGET)
	install -d /usr/bin
	install -m 755 $(TARGET) /usr/bin/
	if [ -f "async-server.service" ]; then \
		install -d /etc/systemd/system; \
		install -m 644 async-server.service /etc/systemd/system/; \
		systemctl daemon-reload; \
		echo "Service installed successfully."; \
	else \
		echo "Warning: async-server.service not found, skipping service installation"; \
	fi

uninstall:
	-systemctl stop async-server 2>/dev/null || true
	-systemctl disable async-server 2>/dev/null || true
	rm -f /usr/bin/$(TARGET)
	rm -f /etc/systemd/system/async-server.service
	systemctl daemon-reload
	@echo "Service uninstalled"

package: $(TARGET)
	@if [ ! -f "async-server.service" ] || [ ! -f "control" ]; then \
		echo "Error: Required files for packaging not found"; \
		exit 1; \
	fi
	@echo "Creating package structure..."
	mkdir -p pkg/usr/bin
	mkdir -p pkg/usr/lib/systemd/system
	mkdir -p pkg/DEBIAN
	cp $(TARGET) pkg/usr/bin/
	cp async-server.service pkg/usr/lib/systemd/system/
	cp control pkg/DEBIAN/
	@echo "Building DEB package..."
	dpkg-deb --build pkg async-server_1.0_amd64.deb
	rm -rf pkg
	@echo "Package created: async-server_1.0_amd64.deb"

check-control:
	@echo "Checking control file format..."
	@if ! tail -1 control | grep -q .; then \
		echo "Error: control file must end with empty line"; \
		echo "Fixing control file..."; \
		echo "" >> control; \
	fi
	@echo "Control file format is OK"