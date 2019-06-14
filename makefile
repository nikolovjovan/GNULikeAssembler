PROJECTDIR		:= .
SRCDIR			:= src
HDIR			:= h
OUTPUTDIR		:= out

TARGETNAME		:= assembler

SRCPATH			:= $(PROJECTDIR)/$(SRCDIR)
HPATH			:= $(PROJECTDIR)/$(HDIR)
OUTPUTPATH		:= $(PROJECTDIR)/$(OUTPUTDIR)

CC				:= g++
CCFLAGS			:= -m32 -std=c++11

SRC				:= $(wildcard $(SRCPATH)/*.cpp)
H				:= $(wildcard $(HPATH)/*h)
TARGET			:= $(OUTPUTPATH)/$(TARGETNAME)
TARGETSTATIC	:= $(OUTPUTPATH)/$(TARGETNAME)_static
TARGETDEBUG		:= $(OUTPUTPATH)/$(TARGETNAME)_debug

$(TARGET): $(SRC) $(H)
	@mkdir -p $(OUTPUTPATH)
	@$(CC) $(CCFLAGS) -o $(TARGET) -I$(HPATH) $(SRC)

$(TARGETSTATIC): $(SRC) $(H)
	@mkdir -p $(OUTPUTPATH)
	@$(CC) $(CCFLAGS) -static -o $(TARGETSTATIC) -I$(HPATH) $(SRC)

$(TARGETDEBUG): $(SRC) $(H)
	@mkdir -p $(OUTPUTPATH)
	@$(CC) $(CCFLAGS) -g -o $(TARGETDEBUG) -I$(HPATH) $(SRC)

all: $(TARGET)

static: $(TARGETSTATIC)

debug: $(TARGETDEBUG)

clean:
	@rm -rf $(OUTPUTPATH)

.PHONY: all, debug, clean