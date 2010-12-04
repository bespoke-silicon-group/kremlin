# C
%.o: %.c

# C++
%.o: %.cpp
%.o: %.cxx
%.o: %.cc
%.o: %.C

# Pascal
%.o: %.p

# Fortran
%.o: %.r
%.o: %.F
%.o: %.f

%.f: %.r
%.f: %.F

# Modula-2
%.sym: %.def

# Assembler
#%.s: %.o

# Linking
%: %.o
%: %.o *.c

# Yacc
%.c: %.y

# Lex
%.c: %.l
%.r: %.l

# Lint
%.ln: %.c

# TeX and Web
%.dvi: %.tex
%.tex: %.web
%.tex: %.w
%.tex: %.ch
%.p: %.web
%.c: %.w
%.c: %.ch

# Texinfo and Info
%.dvi: %.texinfo
%.dvi: %.texi
%.dvi: %.txinfo
%.info: %.texinfo
%.info: %.texi
%.info: %.txinfo

# RCS
%: %,v
%: RCS/%,v

