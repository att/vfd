#mk; is better than make, but if you insist this might work too

libs = -L../lib -lvfd 
vreq_req:	vreq.c ../lib/libvfd.a
	gcc -I ../lib vreq.c -o vfd_req $(libs)

clean::
	rm -f *.o vreq

nuke::
	rm -f *.o vreq
