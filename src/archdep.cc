#include "parathon.h"
#include "util.h"
#include "archdep.h"
#include <dwarfpp/regs.hpp>
#include <cassert>
#include <strings.h>
#include "ast.h"

using std::vector;
using std::pair;
using std::make_pair;
using std::shared_ptr;
using dwarf::spec::base_type_die;
using dwarf::spec::structure_type_die;
using dwarf::spec::class_type_die;
using dwarf::spec::union_type_die;
using dwarf::spec::subprogram_die;
using namespace dwarf::lib;

void
write_entry_point_and_fp_locations(
	FunctionDefinition *ast,
	std::shared_ptr<dwarf::encap::subprogram_die> subp, /* determines ABI */
	void *dest, /* dest */
	size_t sz, /* size of buffer */
	size_t *out_sz /* #bytes written */
)
{
	// write location descriptions for fp
	unsigned registers_used;
	write_fp_location_descriptions(subp, &registers_used);
	
	// write entry point with fake AST pointer
	size_t spare_out_sz; // we need to know the size, whether or not the caller does
	if (!out_sz) out_sz = &spare_out_sz;
	write_basic_entry_point(subp, registers_used, dest, sz, out_sz);

	// generate a frame base
	write_frame_base(subp, (Dwarf_Addr) dest, (Dwarf_Addr)((char*) dest + *out_sz));
	
	// update fake AST pointer
	update_fake_ast_pointer(dest, out_sz ? *out_sz : spare_out_sz, ast);
}

void 
update_fake_ast_pointer(void *dest, size_t sz, FunctionDefinition *ast)
{
	#ifdef __x86__
	static void *const MARKER_ADDRESS = reinterpret_cast<void*>(0x12345678U);
	#else
	#ifdef __x86_64__
	static void *const MARKER_ADDRESS = reinterpret_cast<void*>(0x123456789abcdef0UL);
	#else
	#error "Did not recognise architecture."
	#endif
	#endif
    // find the AST node pointer in the instruction sequence
    Statement** searchp = reinterpret_cast<Statement**>(dest); 
    while ((char*) searchp <= (char*) dest + sz - sizeof (Statement*)) 
    {
        // debugging
    	std::cerr << "Bytes: " << std::hex;
		for (int i = 0; i < sizeof (unw_word_t); i++)
		{
        	std::cerr << (unw_word_t) *reinterpret_cast<unsigned char*>(searchp+i) << " ";
		}
		std::cerr << std::dec << std::endl;

    	if (*reinterpret_cast<unw_word_t*>(searchp) == 
		     reinterpret_cast<unw_word_t>(MARKER_ADDRESS))
        {
            // overwrite the AST node pointer with the appropriate one
            *searchp = ast->suite;
            std::cerr << "Installed AST node pointer for entry point " << ast->name
    	        << ", AST at " << (void*) ast->suite << std::endl;
			searchp++; // advance by a pointer's worth
        }
    	// advance by a byte's worth
    	else searchp = (Statement**) (((char*) searchp) + 1); 
	}
}
#ifdef __x86__
void *ideal_entry_point(...) __attribute__((optimize(0)));
void *ideal_entry_point(...)
{
	// call through an immediately-addressed object's vtable.
    return reinterpret_cast<Statement*>(MARKER_ADDRESS)->evaluate().i_ptr;
}

void end_ideal_entry_point(void);
void end_ideal_entry_point(void) {}

#else
#ifdef __x86_64__
/* For x86_64 we don't use a compiler-generated entry point template directly,
 * because we want to give the regparm arguments a stack location that we can
 * express in DWARF, meaning we want to push them *before* the compiler 
 * adjusts %rsp for frame-local storage. 
 *
 * Instead, we code directly in chunks of assembly (which was generated by running
 * g++ on the ideal entry point :-). */

unsigned char prologue[] = { 0x55, /* push   %rbp */
                     0x48, 0x89, 0xe5, /* mov    %rsp,%rbp */ };
unsigned char adjust_sp[] = { 0x48, 0x83, 0xec, 0x20  /* sub    $0x20,%rsp */ };
unsigned char save_regparm1[] = { 0x57 /* push   %rdi */ };
unsigned char save_regparm2[] = { 0x56 /* push   %rsi */ }; 
unsigned char save_regparm3[] = { 0x52 /* push   %rdx */ };
unsigned char save_regparm4[] = { 0x51 /* push   %rcx */ };
unsigned char save_regparm5[] = { 0x41, 0x50 /* push   %r8 */ };
unsigned char save_regparm6[] = { 0x41, 0x51 /* push   %r9 */ };
unsigned char body[] = { 0x48, 0xb8, 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
		/*  movabs $0x123456789abcdef0,%rax */ 
				0x48, 0x8b, 0x00, /* mov    (%rax),%rax */
				0x48, 0x83, 0xc0, 0x08, /* add    $0x8,%rax */
				0x48, 0x8b, 0x10, /* mov    (%rax),%rdx */
				0x48, 0x89, 0xe0, /* mov %rsp,%rax ; was: 0x48, 0x8d, 0x45, 0xe0, lea    -0x20(%rbp),%rax */
				0x48, 0xbe, 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
		/* movabs $0x123456789abcdef0,%rsi */
				0x48, 0x89, 0xc7, /* mov    %rax,%rdi */
				0xff, 0xd2, /* callq  *%rdx */
				0x48, 0x8b, 0x44, 0x24, 0x08 /* mov    0x8(%rsp),%rax; was: 0x48, 0x8b, 0x45, 0xe8  mov    -0x18(%rbp),%rax */ 
};
unsigned char epilogue[] = {0xc9, /* leaveq */
				0xc3 /* retq */
};

void assembly_test() __attribute__((optimize(0)));
void assembly_test()
{
	__asm__ ("mov %rsp,%rax \n");
	// rsp is rbp - 0x20
	// so -0x18(%rbp) is 0x8(%rsp)
	__asm__ ("mov 0x8(%rsp),%rax \n");
}

const unsigned max_entry_point_size = 
	sizeof prologue
	+ sizeof adjust_sp
	+ sizeof save_regparm1
	+ sizeof save_regparm2
	+ sizeof save_regparm3
	+ sizeof save_regparm4
	+ sizeof save_regparm5
	+ sizeof save_regparm6
	+ sizeof body
	+ sizeof epilogue;
#else
#error "Did not recognise architecture."
#endif
#endif

void 
write_basic_entry_point(
	std::shared_ptr<dwarf::spec::subprogram_die> subp, 
	unsigned registers_used,
	void *dest, 
	size_t sz, 
	size_t *out_sz)
{
	// new method: use the appropriate ideal entry point 
#ifdef __x86__
	unsigned char *buf = (unsigned char*)ideal_entry_point;
    size_t entry_point_size = (char*)ideal_entry_point_end - (char*)ideal_entry_point;
#else
#ifdef __x86_64__
	unsigned char buf[max_entry_point_size];
	unsigned char *pos = &buf[0];
	bzero(&buf, max_entry_point_size);
	size_t entry_point_size; 
	
	// copy prologue and sp adjustment
#define COPY_CHUNK(c) do { memcpy(pos, (c), sizeof (c)); pos += sizeof (c); } while (0)
	COPY_CHUNK(prologue);
	// copy regparm pushes
	assert(registers_used >= 0 && registers_used < 6);
	if (registers_used >= 1) COPY_CHUNK(save_regparm1);
	if (registers_used >= 2) COPY_CHUNK(save_regparm2);
	if (registers_used >= 3) COPY_CHUNK(save_regparm3);
	if (registers_used >= 4) COPY_CHUNK(save_regparm4);
	if (registers_used >= 5) COPY_CHUNK(save_regparm5);
	if (registers_used == 6) COPY_CHUNK(save_regparm6);
	
	// copy sp adjustment, body and epilogue
	COPY_CHUNK(adjust_sp);
	COPY_CHUNK(body);
	COPY_CHUNK(epilogue);
#undef COPY_CHUNK

#else
#error "Did not recognise architecture"
#endif
#endif

    // blat the bytes
	*out_sz = pos - &buf[0];
    memcpy(dest, (void*) buf, *out_sz);

}


void 
write_frame_base(
	std::shared_ptr<dwarf::encap::subprogram_die> subp, Dwarf_Addr lopc, Dwarf_Addr hipc)
{
    // give it a frame_base? YES, just make it ebp (no lexical blocks to worry about)
#ifdef __x86__
    /* These are x86-specific, assuming that an entry point has this prologue:
    
       0:   55                      push   %ebp
       1:   89 e5                   mov    %esp,%ebp
       3:   83 ec 10                sub    $0x10,%esp # <-- here 0x10 is just size of locals, 16-byte rounded

       and this epilogue:

      10:   c9                      leave  
      11:   c3                      ret
    */
	// for x86, frame is "based" at the initial $esp + one word,
	// i.e. the address of the last argument pushed onto the stack
	// since initial $esp holds the return address
	
    Dwarf_Unsigned opcodes1[] = { DW_OP_breg4, sizeof (unw_word_t) };     // vaddr 0x0..0x1
    Dwarf_Unsigned opcodes2[] = { DW_OP_breg4, 2 * sizeof (unw_word_t) }; // vaddr 0x1..0x3
    Dwarf_Unsigned opcodes3[] = { DW_OP_breg5, 2 * sizeof (unw_word_t) }; // vaddr 0x3..LAST+1
#else
#ifdef __x86_64__
/*   0x00002aaaaf640000:  push   %rbp
   0x00002aaaaf640001:  mov    %rsp,%rbp
   0x00002aaaaf640004:  sub    $0x20,%rsp
 */
 	// for x86_64, we decree that the frame is "based" at initial $rsp + 1 word again
 
    Dwarf_Unsigned opcodes1[] = { DW_OP_breg7, sizeof (unw_word_t) };     // vaddr 0x0..0x1
    Dwarf_Unsigned opcodes2[] = { DW_OP_breg7, 2 * sizeof (unw_word_t) }; // vaddr 0x1..0x3
    Dwarf_Unsigned opcodes3[] = { DW_OP_breg6, 2 * sizeof (unw_word_t) }; // vaddr 0x3..LAST+1
#else
#error "Did not recognise architecture."
#endif
#endif

    encap::loc_expr expr1(opcodes1, //expr1.lopc = 0; expr1.hipc = 1;
		(Dwarf_Unsigned)lopc, (Dwarf_Unsigned)(lopc + 1)); 
    encap::loc_expr expr2(opcodes2,//expr2.lopc = 1; expr2.hipc = 3;
		(Dwarf_Unsigned)(lopc + 1), (Dwarf_Unsigned)(lopc + 3)); 
    encap::loc_expr expr3(opcodes3, /*sizeof prologue_bytes + sizeof call_bytes + sizeof epilogue_bytes + 1*/ // <-- +1 is wrong here 
		(Dwarf_Unsigned)(lopc + 3), (Dwarf_Unsigned)hipc);
    
    encap::loclist loc(expr1); loc.push_back(expr2); loc.push_back(expr3);
    subp->set_frame_base(loc);
}
	
void write_fp_location_descriptions(
	std::shared_ptr<dwarf::encap::subprogram_die> subp,
	unsigned *out_registers_used)
{
	// compute location descriptions -- ABI may depend on types
	unsigned argpos = 0;
	vector< encap::loclist > v;
#ifdef __x86_64__
	unsigned registers_used = 0;
	unsigned stack_bytes_allocated = 0;
#endif
	
	for (auto i_arg = subp->formal_parameter_children_begin();
		i_arg != subp->formal_parameter_children_end(); ++i_arg, ++argpos)
	{
#ifdef __x86__
		unsigned long offset = 0;
		unsigned long increment_in_bytes = sizeof (void*);
		assert(v.size() == argpos);
		v.push_back(
			encap::loclist(
				encap::loc_expr(
					{ DW_OP_fbreg, offset + argpos * increment_in_bytes },
					0, std::numeric_limits<Dwarf_Addr>::max()
				)
			)
		);
#else
#ifdef __x86_64__
		// FIXME: sadly, the switch statement below should instead be
		// a dynamic_pointer_cast if--else--else, so that we can use
		// the abstract superclasses like with_data_members.
		auto t = (*i_arg)->get_type();
		auto ct = t->get_concrete_type();
		/*
		INTEGER This class consists of integral types that fit into one of the general
		purpose registers.
		SSE The class consists of types that fit into a vector register.
		SSEUP The class consists of types that fit into a vector register and can be passed
		and returned in the upper bytes of it.
		X87, X87UP These classes consists of types that will be returned via the x87
		FPU.
		COMPLEX_X87 This class consists of types that will be returned via the x87
		FPU.
		NO_CLASS This class is used as initializer in the algorithms. It will be used for
		padding and empty structures and unions.
		*/
		enum 
		{
			INTEGER, 
			/* INTEGER This class consists of integral types that fit into one of the general
			   purpose registers. */
			MEMORY, 
			/* MEMORY This class consists of types that will be passed and returned in mem-
			   ory via the stack. */		 
			SSE, /* SSE The class consists of types that fit into a vector register. */
			SSEUP, 
			/* SSEUP The class consists of types that fit into a vector register and can be passed
			   and returned in the upper bytes of it. */
			X87, X87UP, 
			/* X87, X87UP These classes consists of types that will be returned via the x87 FPU. */
			COMPLEX_X87, /* This class consists of types that will be returned via the x87 FPU. */
			NO_CLASS
			/* NO_CLASS This class is used as initializer in the algorithms. It will be used for
			   padding and empty structures and unions. */
		} cls = NO_CLASS;
		switch(ct->get_tag())
		{
			case DW_TAG_pointer_type:
			case DW_TAG_reference_type:
			/* 
				Arguments of types (signed and unsigned) _Bool, char, short, int,
				long, long long, and pointers are in the INTEGER class.
			*/
				cls = INTEGER;
				break;

			case DW_TAG_base_type: {

			/*
				Arguments of types (signed and unsigned) _Bool, char, short, int,
				long, long long, and pointers are in the INTEGER class.

				Arguments of types float, double, _Decimal32, _Decimal64 and
				__m64 are in class SSE.
				Arguments of types __float128, _Decimal128 and __m128 are split
				into two halves. The least significant ones belong to class SSE, the most
				significant one to class SSEUP.
				 Arguments of type __m256 are split into four eightbyte chunks. The least
				significant one belongs to class SSE and all the others to class SSEUP.
				The 64-bit mantissa of arguments of type long double belongs to class
				X87, the 16-bit exponent plus 6 bytes of padding belongs to class X87UP.
				Arguments of type __int128 offer the same operations as INTEGERs,
				yet they do not fit into one general purpose register but require two registers.
				For classification purposes __int128 is treated as if it were implemented
				as:
				typedef struct {
				long low, high;
				} __int128;
				with the exception that arguments of type __int128 that are stored in
				memory must be aligned on a 16-byte boundary.
				 Arguments of complex T where T is one of the types float or double
				are treated as if they are implemented as:
				struct complexT {
				T real;
				T imag;
				};
				A variable of type complex long double is classified as type COM-
				PLEX_X87.
			*/
				auto bt = dynamic_pointer_cast<spec::base_type_die>(ct);
				if (*bt->get_byte_size() <= 8 /* this is x86_64-only, so no need to be portable */
					&& (bt->get_encoding() == DW_ATE_signed
					||  bt->get_encoding() == DW_ATE_unsigned 
					||  bt->get_encoding() == DW_ATE_signed_char
					||  bt->get_encoding() == DW_ATE_unsigned_char
					||  bt->get_encoding() == DW_ATE_boolean
					))
				{
					cls = INTEGER;
				}
				else assert(false); // no support for other cases yet!
			}
			break;
			case DW_TAG_structure_type:
			case DW_TAG_class_type:
			case DW_TAG_union_type:
				assert(false); // no support yet!
				break;
		
		/* 
			The classification of aggregate (structures and arrays) and union types works
			as follows:

			1. If the size of an object is larger than four eightbytes, or it contains unaligned
			fields, it has class MEMORY (footnote 10). Footnote 10: The post merger clean up described 
			later ensures that, for the processors that do not support the __m256 type, if the 
			size of an object is larger than two eightbytes and the first eightbyte is not
			SSE or any other eightbyte is not SSEUP, it still has class MEMORY. This in turn 
			ensures that for processors that do support the __m256 type, if the size of an object 
			is four eightbytes and the first eightbyte is SSE and all other eightbytes are SSEUP, 
			it can be passed in a register.

			2. If a C++ object has either a non-trivial copy constructor or a non-trivial
			destructor (footnote 11), it is passed by invisible reference (the object is replaced
			in the 	parameter list by a pointer that has class INTEGER) (footnote 12).
			Footnote 11: A de/constructor is trivial if it is an implicitly-declared default 
			de/constructor and if:
			- its class has no virtual functions and no virtual base classes, and
			- all the direct base classes of its class have trivial de/constructors, and
			- for all the nonstatic data members of its class that are of class type (or array thereof), each
			such class has a trivial de/constructor.)
			Footnote 12:
			An object with either a non-trivial copy constructor or a non-trivial destructor 
			cannot be passed by value because such objects must have well defined addresses. 
			Similar issues apply when returning an object from a function.)
			3. If the size of the aggregate exceeds a single eightbyte, each is classified
			separately. Each eightbyte gets initialized to class NO_CLASS.
			4. Each field of an object is classified recursively so that always two fields are
			considered. The resulting class is calculated according to the classes of the
			fields in the eightbyte:
			(a) If both classes are equal, this is the resulting class.
			(b) If one of the classes is NO_CLASS, the resulting class is the other
			class.
			(c) If one of the classes is MEMORY, the result is the MEMORY class.
			(d) If one of the classes is INTEGER, the result is the INTEGER.
			(e) If one of the classes is X87, X87UP, COMPLEX_X87 class, MEM-
			ORY is used as class.
			(f) Otherwise class SSE is used.
			5. Then a post merger cleanup is done:
			(a) If one of the classes is MEMORY, the whole argument is passed in
			memory.
			(b) If X87UP is not preceded by X87, the whole argument is passed in
			memory.
			(c) If the size of the aggregate exceeds two eightbytes and the first eight-
			byte isn't SSE or any other eightbyte isn't SSEUP, the whole argument
			is passed in memory.
			19
			(d) If SSEUP is not preceded by SSE or SSEUP, it is converted to SSE.
		*/
			default: assert(false);
		} // end switch
		assert(cls != NO_CLASS);
	/*
		Once arguments are classified, the registers get assigned (in left-to-right
		order) for passing as follows:
	*/
		assert(ct->calculate_byte_size());
		unsigned byte_size = *ct->calculate_byte_size();
		switch(cls)
		{
			case MEMORY:
			pass_on_stack: {
				/*  1. If the class is MEMORY, pass the argument on the stack. */
				Dwarf_Unsigned opcodes[] = { DW_OP_fbreg, stack_bytes_allocated };
				v.push_back(encap::loclist(encap::loc_expr(opcodes, 
					0, std::numeric_limits<Dwarf_Addr>::max())));
				// stack argument n is at %rbp + 8n + 16...
				// ... so make sure we align byte_size to 8 bytes
				stack_bytes_allocated += (byte_size % 8 == 0) ? byte_size 
					: ((byte_size / 8) + 1) * 8;
				} break;
			case INTEGER:
				/* 2. If the class is INTEGER, the next available register of the sequence %rdi,
				%rsi, %rdx, %rcx, %r8 and %r9 is used (footnote 13).
				*/
				assert(byte_size <= 8); // FIXME: handle multi-word args as described....
				if (registers_used < 6) 
				{
					switch(registers_used)
					{ 
						/* HACK: we'd rather write DW_OP_reg ## DWARF_X86_64_RDI etc. */
#define PUSH_REG_LOC(i, pushed_bytes, r) \
{ \
	std::vector<encap::loc_expr> ll; \
	Dwarf_Unsigned reg_opcodes[] = { (Dwarf_Unsigned) DW_OP_reg0 + (Dwarf_Unsigned) (r) }; \
	ll.push_back(encap::loc_expr( \
	                 reg_opcodes, /* HACK: would prefer DW_OP_reg ## DWARF_X86_64_RDI etc. */ \
	                 subp->get_low_pc()->addr /* lopc */, \
	                 subp->get_low_pc()->addr + sizeof prologue + pushed_bytes)); \
	Dwarf_Unsigned stack_opcodes[] = { DW_OP_fbreg, (-8) * ((i) + 3) };  \
	/* the three slots skipped over here are for                --^
	 * the possibly-imaginary first argument (fbreg + 0 is first stack arg),
	 * return addr,
	 * and saved $rbp */ \
	ll.push_back(encap::loc_expr( \
	                 stack_opcodes, \
	                 subp->get_low_pc()->addr + sizeof prologue + pushed_bytes, \
	                 subp->get_low_pc()->addr + max_entry_point_size \
	               )); \
	v.push_back(ll); }

/* Explanation of this macro: our arg is in the register up to a certain 
 * point, but then our entry point 
 * will push it onto the stack. We need to encode this. For this, we
 * really need the total number of regparm arguments, and/or the total
 * length of the entry point. However, as a HACK so we don't need to
 * iterate through all args *again*, we size all entry points by the biggest,
 * and rely on the push instructions being at similar offsets across all
 * template entry points.  */
// 						case 0: {
// 	std::vector<encap::loc_expr> ll;
// 	Dwarf_Unsigned reg_opcodes[] = { DW_OP_reg0 + DWARF_X86_64_RDI };
// 	ll.push_back(encap::loc_expr( \
// 	                 reg_opcodes, /* HACK: would prefer DW_OP_reg ## DWARF_X86_64_RDI etc. */ \
// 	                 subp->get_low_pc()->addr /* lopc */, \
// 	                 subp->get_low_pc()->addr + PROLOG_SIZE + PUSH1_CUMUL_SIZE));
// 	Dwarf_Unsigned stack_opcodes[] =  { DW_OP_fbreg, (-8) * ((0) + 1) };
// 	ll.push_back(encap::loc_expr( \
// 	                 stack_opcodes, \
// 	                 subp->get_low_pc()->addr + PROLOG_SIZE + PUSH1_CUMUL_SIZE, \
// 	                 /*(char*)&end_ideal_entry_point_regparm6 - (char*)&ideal_entry_point_regparm6)*/ \
// 	                 subp->get_high_pc()->addr)); 
// 	v.push_back(encap::loclist(ll));
// 	              }
// 				  break;
						
						case 0: PUSH_REG_LOC(0, sizeof save_regparm1, DWARF_X86_64_RDI); break;
						
						case 1: PUSH_REG_LOC(1, sizeof save_regparm1 + sizeof save_regparm2, 
							DWARF_X86_64_RSI); break;
						case 2: PUSH_REG_LOC(2, 
							sizeof save_regparm1 + sizeof save_regparm2 + sizeof save_regparm3, 
							DWARF_X86_64_RDX); break;
						case 3: PUSH_REG_LOC(3, 
							sizeof save_regparm1 + sizeof save_regparm2 + sizeof save_regparm3
							+ sizeof save_regparm4, DWARF_X86_64_RCX); break;
						case 4: PUSH_REG_LOC(4, 
							sizeof save_regparm1 + sizeof save_regparm2 + sizeof save_regparm3
							+ sizeof save_regparm4 + sizeof save_regparm5, DWARF_X86_64_R8); break;
						case 5: PUSH_REG_LOC(5, 
							sizeof save_regparm1 + sizeof save_regparm2 + sizeof save_regparm3
							+ sizeof save_regparm4 + sizeof save_regparm5 + sizeof save_regparm6,
							DWARF_X86_64_R9); break;
						default: assert(false);
					}
					++registers_used;
				} else goto pass_on_stack;
				break;
				/*
				When a value of type _Bool is returned or passed in a register or on the stack,
				bit 0 contains the truth value and bits 1 to 7 shall be zero (footnote 14).
				*/
				/* If there are no registers available for any eightbyte of an argument, the whole
				argument is passed on the stack. If registers have already been assigned for some
				eightbytes of such an argument, the assignments get reverted.
				*/
				/* 
				Once registers are assigned, the arguments passed in memory are pushed on
				the stack in reversed (right-to-left (footnote 15)) order.
				*/
				break;
			case SSE:
			/* 3. If the class is SSE, the next available vector register is used, the registers
				are taken in the order from %xmm0 to %xmm7.
			*/
				assert(false);
				break;
			case SSEUP:
				/* 4. If the class is SSEUP, the eightbyte is passed in the next available eightbyte
				chunk of the last used vector register. */
				assert(false);
				break;
			case X87:
			case X87UP:
			case COMPLEX_X87:
				/* 5. If the class is X87, X87UP or COMPLEX_X87, it is passed in memory. */
				assert(false);
				break;
			default:
				assert(false);
				break;
		} // end switch cls
		/* 
		Once registers are assigned, the arguments passed in memory are pushed on
		the stack in reversed (right-to-left (footnote 15)) order.
		*/
#else
#error "Did not recognise architecture."
#endif
#endif
	} // end for i_arg
	/*
		For calls that may call functions that use varargs or stdargs (prototype-less
		calls or calls to functions containing ellipsis (...) in the declaration) %al
		(footnote 16) is used
		as hidden argument to specify the number of vector registers used. The contents
		of %al do not need to match exactly the number of registers, but must be an upper
		bound on the number of vector registers used and is in the range 0--8 inclusive.
		
		Footnote 16: Note that the rest of %rax is undefined, only the contents of %al
		is defined.
		
		When passing __m256 arguments to functions that use varargs or stdarg,
		function prototypes must be provided. Otherwise, the run-time behavior is un-
		defined.
	*/
#ifdef __x86_64__
	if (subp && subp->unspecified_parameters_children_begin()
			!= subp->unspecified_parameters_children_end())
	{
		assert(false);	
	}
#endif
	// now iterate through the fps adding the location descriptions
	int i = 0;
	for (auto i_fp = subp->formal_parameter_children_begin();
		i_fp != subp->formal_parameter_children_end();
		++i_fp, ++i)
	{
		dynamic_pointer_cast<encap::formal_parameter_die>(*i_fp)->set_location(
			v.at(i));
	}
	
	*out_registers_used = registers_used;
}	
