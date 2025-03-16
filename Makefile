ROOT=.
MANUAL_PATH=$(ROOT)/manual
BUILD_DIR=$(ROOT)/build
OBJECT_DIR=$(BUILD_DIR)/objects
BIN_DIR=$(BUILD_DIR)/bin
SRC=$(ROOT)/src
COMPONENTS= io \
	formats \
	messages \
	interfaces \
	connector \
	node \
	config \
	controller \
	segment \
	proxy
VPATH=$(addprefix $(SRC)/,$(COMPONENTS))
CPPFLAGS = -Wall \
	-Wpedantic \
	-O3 \
	-flto
COMMON = poller \
	sockbuf \
	xmsg \
	messages \
	sockstream \
	interfaces \
	connectors \
	node \
	config
CONTROLLER := $(COMMON) \
	controller_connector \
	controller_marshaller
SEGMENT := $(COMMON) \
	segment_connector \
	segment_marshaller
PROXY := $(COMMON) \
	proxy_connector \
	proxy_marshaller 
EXECUTABLES := controller \
	segment \
	proxy

export TEXINPUTS=$(MANUAL_PATH):

.PHONY: manual cleanmanual all clean
all: $(addprefix $(BIN_DIR)/, $(EXECUTABLES))
	
$(BIN_DIR)/controller: $(addsuffix .o,$(addprefix $(OBJECT_DIR)/, $(CONTROLLER)))
	mkdir -p $(BIN_DIR) && $(CXX) $(CPPFLAGS) -D COMPILE_CONTROLLER $(SRC)/main.cpp -o $@ $^

$(BIN_DIR)/segment: $(addsuffix .o,$(addprefix $(OBJECT_DIR)/, $(SEGMENT)))
	mkdir -p $(BIN_DIR) && $(CXX) $(CPPFLAGS) -D COMPILE_SEGMENT $(SRC)/main.cpp -o $@ $^

$(BIN_DIR)/proxy: $(addsuffix .o,$(addprefix $(OBJECT_DIR)/, $(PROXY)))
	mkdir -p $(BIN_DIR) && $(CXX) $(CPPFLAGS) -D COMPILE_PROXY $(SRC)/main.cpp -o $@ $^

$(OBJECT_DIR)/%.o: %.cpp
	mkdir -p $(OBJECT_DIR) && $(CXX) -c $(CPPFLAGS) $< -o $@
	
manual:
	mkdir -p $(MANUAL_PATH)/build/chapters && \
	pdflatex -output-directory=$(MANUAL_PATH)/build -interaction=nonstopmode $(MANUAL_PATH)/manual.tex && \
	mkdir -p $(BUILD_DIR)/manual && \
	mv $(MANUAL_PATH)/build/manual.pdf $(BUILD_DIR)/manual/manual.pdf

clean:
	rm -r $(OBJECT_DIR) && rm $(addprefix $(BIN_DIR)/, $(EXECUTABLES))
	
cleanmanual:
	rm -r $(MANUAL_PATH)/build && rm -r $(BUILD_DIR)/manual
	
