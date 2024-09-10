PY_FILES := $(wildcard *.py)

C_FILES := $(PY_FILES:.py=.c)
COMPILED_FILES := $(PY_FILES:.py=.exe)
PYTHON_INCLUDE := $(shell python3 -c "import sysconfig; print(sysconfig.get_path('include'))")
PYTHON_INCLUDE := $(shell python3 -c "import sysconfig; print(sysconfig.get_path('include'))")

all: $(COMPILED_FILES)
	echo $(PYTHON_INCLUDE)
	
%.exe: %.c
	cc -O2 -I$(PYTHON_INCLUDE) -shared -LC:\Users\user\AppData\Local\Programs\Python\Python312 -lpython312 -o $@ $< $(LDFLAGS)

%.c: %.py
	cython --embed -3 -o $@ $<
