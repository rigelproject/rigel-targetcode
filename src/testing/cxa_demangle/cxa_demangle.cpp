//#include <cxa_demangle.hpp>
//#include <iostream>
//#include <cassert>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <typeinfo>
#include "test.h"

namespace bla
{
	struct a
	{
		int b;
		int c;
	};
}

int
main(int argc, char *argv[])
{
	  bla::a a_example;
    fprintf(stderr, "%s\n", __cxa_demangle_gnu3(typeid(a_example).name()));
		fprintf(stderr, "%s\n", __cxa_demangle_gnu3("_ZGVN5libcw24_GLOBAL__N_cbll.cc0ZhUKa23compiler_bug_workaroundISt6vectorINS_13omanip_id_tctINS_5debug32memblk_types_manipulator_data_ctEEESaIS6_EEE3idsE"));
		fprintf(stderr, "should have been:\n%s\n", "guard variable for libcw::(anonymous namespace)::compiler_bug_workaround<std::vector<libcw::omanip_id_tct<libcw::debug::memblk_types_manipulator_data_ct>, std::allocator<libcw::omanip_id_tct<libcw::debug::memblk_types_manipulator_data_ct> > > >::ids");

    //assert(!strcmp(more::cxa_demangle(typeid(a).name()), "main::cpp_demangled_struct_name");
    return 0;
}

