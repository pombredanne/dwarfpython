OBJS := parathon.l.o parathon.yy.o $(patsubst %.cc,%.o,$(wildcard Parathon*.cc))
OBJS += die_wrapper.o buffer.o ast.o builtins.o runtime/list.o closures.o
CFLAGS := -I${HOME}/work/devel/dwarf/libdwarfpp/include
LIBS += -lfl -lreadline -lffi -ldwarf -lelf -lgc

default: autogen ctags parathon

autogen:
	python ast/autogen.py ast > /tmp/aaa

parathon.l.cc: parathon.l
	flex "$<" > "$@"
	
ctags:
	ctags -R --c++-kinds=+p --fields=+iaS --extra=+q .

#env.Autogen(['ast.h', 'parathon.yy'], ['ast/autogen.py', Glob('ast/*.cc')])
#env.Ctags(target='ctags', source=['parathon'])

runtime: $(patsubst%.c,%.so,$(wildcard runtime/*.c))

parathon.l.cc:
	flex -t --header-file=parathon.l.h parathon.l > parathon.l.cc

parathon: 
	 g++ -o parathon \
	 main.o \
	 parathon.l.o parathon.yy.o \
	 ParathonException.o ParathonFunction.o ParathonValue.o ParathonContext.o ParathonAssigner.o \
	 die_wrapper.o \
	 ast/CompLtEq.o ast/ValueString.o ast/DottedAsNames.o ast/OpBnot.o ast/ParameterListNonArgs.o \
	 ast/BreakStatement.o ast/TestPhraseBracketed.o ast/OpPlus.o ast/CompGtEq.o ast/ParameterListArgs.o \
	 ast/OpLeftShift.o ast/TestStatement.o ast/TestPhrase.o ast/AndPhrase.o ast/SingleInput.o \
	 ast/TestListNonTrailing.o ast/ComparisonList.o ast/OpMod.o ast/CompIn.o ast/AssignStatement.o \
	 ast/OpDivide.o ast/TupleMaker.o ast/CompNotIs.o ast/AndExpression.o ast/ParameterPhrase.o \
	 ast/ArithOperator.o ast/OpMult.o ast/Suite.o ast/ComparisonOperator.o ast/FunctionDefinition.o \
	 ast/CompLt.o ast/Expression.o ast/TestListNonEmpty.o ast/TestList.o ast/PassStatement.o \
	 ast/IfStatement.o ast/CompGt.o ast/UnaryPhrase.o ast/OpBand.o ast/OpBor.o ast/WhileStatement.o \
	 ast/ListMaker.o ast/DeleteStatement.o ast/OpBxor.o ast/TestPhraseOptional.o ast/ImportAsNames.o \
	 ast/MultOperator.o ast/CompNotIn.o ast/ValueFloat.o ast/OpPositive.o ast/ReturnStatement.o \
	 ast/ContinueStatement.o ast/ParameterListKwargs.o ast/CompNotEq.o ast/BasePhrase.o \
	 ast/DottedName.o ast/PrintStatement.o ast/NotPhrase.o ast/OrPhrase.o ast/OpTrunc.o ast/SliceList.o \
	 ast/GetAttr.o ast/OpPower.o ast/GlobalStatement.o ast/FunctionCall.o ast/OpMinus.o \
	 ast/SmallStatement.o ast/CompIs.o ast/ReprPhrase.o ast/InlineIfPhrase.o ast/SimpleStatement.o \
	 ast/OpNegative.o ast/CompEq.o ast/SlicePhrase.o ast/ArithExpression.o ast/MultExpression.o \
	 ast/ComparisonPhrase.o ast/OpRightShift.o ast/UnaryOperator.o ast/NamePhrase.o \
	 ast/ImportStatement.o ast/ValueInt.o ast/SuiteBody.o ast/XorExpression.o ast/ShiftOperator.o \
	 ast/GetItem.o ast/FactExpression.o ast/ParameterList.o ast/ShiftExpression.o \
	 buffer.o \
	 ast.o \
	 builtins.o \
	 dwarfpp/encap.o dwarfpp/lib.o dwarfpp/spec.o \
	 /auto/homes/srk31/work/devel/dwarfpython/contrib/libsrk31c++/indenting_ostream.o \
	 runtime/list.o \
	 closures.o \
	 -lfl -lreadline -lffi -ldwarf -lelf -lgc \
	 -L${HOME}/scratch/opt/lib -L${HOME}/opt/lib

runtime/libbuiltin_types.so:
	cd runtime; gcc -g3 -shared -o libbuiltin_types.so builtin_types.c	 

runtime/libstring.so:
	cd runtime; gcc -g3 -shared -o libstring.so string.c

runtime/libint.so:
	cd runtime; gcc -g3 -shared -o libint.so int.c
	
runtime/libfloat.so:
	cd runtime; gcc -g3 -shared -o libfloat.so float.c

