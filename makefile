PROJECTDIR	:= .
SRCDIR		:= src
HDIR		:= h
OUTPUTDIR	:= out

TARGETNAME	:= assembler

SRCPATH		:= $(PROJECTDIR)/$(SRCDIR)
HPATH		:= $(PROJECTDIR)/$(HDIR)
OUTPUTPATH	:= $(PROJECTDIR)/$(OUTPUTDIR)

CC			:= g++
CCFLAGS		:= -m32 -std=c++11

SRC			:= $(wildcard $(SRCPATH)/*.cpp)
H			:= $(wildcard $(HPATH)/*h)
TARGET		:= $(OUTPUTPATH)/$(TARGETNAME)

$(TARGET): $(SRC) $(H)
	@mkdir -p $(OUTPUTPATH)
	@$(CC) $(CCFLAGS) -o $(TARGET) -I$(HPATH) $(SRC)

all: $(TARGET)

clean:
	@rm -rf $(OUTPUTPATH)

.PHONY: clean