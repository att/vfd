
# the mkfile is still the file of record as mk uses nicer syntax and allows 
# complete shell recipes rather than one-liners. This will do a basic build
# of the library and maybe the test scripts (eventually).

MKSHELL = ksh

jsmn_lib = -L jsmn -ljsmn
vfd_lib = -L . -lvfd

cflags = -I jsmn -g
CC = gcc $(cflags)
cc = gcc $(cflags)

binaries = jwrapper_test parm_file_test list_test fifo_test bleat_test id_mgr_test 

all: jsmn libvfd.a

lib = libvfd.a
lib_src = jwrapper jw_xapi symtab config ng_flowmgr fifo list_files bleat hot_plug id_mgr filesys
$(lib): $(lib_src:=.o)
	ar r $(lib) $^


# --------- tests ----------------------------------------------------
jwrapper_test: jwrapper_test.c 
	$(cc) $(cflags) jwrapper_test.c  -o jwrapper_test $(vfd_lib) $(jsmn_lib)

parm_file_test:	parm_file_test.c $(lib)
	$(cc) $(cflags) parm_file_test.c -o parm_file_test -L. -lvfd $(jsmn_lib)
	
vf_config_test:	vf_config_test.c $(lib)
	$(cc) $(cflags) vf_config_test.c -o vf_config_test -L. -lvfd $(jsmn_lib)

fifo_test:	fifo_test.c $(lib)
	$(cc) $(cflags) fifo_test.c -o fifo_test -L. -lvfd $(jsmn_lib)

bleat_test:	bleat_test.c $(lib)
	$(cc) $(cflags) bleat_test.c -o bleat_test -L. -lvfd $(jsmn_lib)

list_test:	list_test.c $(lib)
	$(cc) $(cflags) list_test.c -o list_test -L. -lvfd $(jsmn_lib)

hot_plug:	hot_plug.c $(lib)
	$(cc) $(cflags) hot_plug.c -o hot_plug -L. -lvfd $(jsmn_lib)

id_mgr_test::   id_mgr_test.c $lib
	$cc $cflags id_mgr_test.c -o id_mgr_test -L. -lvfd $jsmn_lib



tests: $(binaries)

nuke:
	rm -f *.o *.a $(binaries)


# ------ clone (if needed) and update and build jsmn -------
jsmn::
	@if [ ! -d jsmn ] ;\
	then \
		git clone https://github.com/zserge/jsmn ;\
	fi 
	cd jsmn; git pull; make

