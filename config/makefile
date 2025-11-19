#
# QScanner/src/config/makefile
#
TARGET_LIB = libconfig.a
#
SRC_FILES = AnalysisConfig.cxx
DEP_FILES = AnalysisConfig.h
OBJ_FILES = $(SRC_FILES:.cxx=.o)
#
all:	$(TARGET_LIB)

%.o:	%.cxx %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET_LIB):	$(OBJ_FILES) $(DEP_FILES)
		@rm -f $@
		$(AR) cr $@ $(OBJ_FILES)
		mkdir -p $(LIBDIR)
		cp $@ $(LIBDIR)

.PHONY:	clean
clean:
	@rm -f $(OBJ_FILES) $(TARGET_LIB)
