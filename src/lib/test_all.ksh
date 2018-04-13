#!/usr/bin/env ksh

#	Abstract:	This script will run all of the tests and create a single
#				log file in the current directory with output from each.
#				If the -l option is given, then we assume that we are doing
#				valgrind leak/memory check analysis and will expect to find
#				a working copy of valgrind either via the PATH or in $HOME.
#				When running valgrnd, we only check for errors that it reports
#				and don't look for specific errors in the output of the tests,
#				so to cover everything possible, run twice; once with -l and
#				once without.
#	Date:		27 February 2018
#	Author:		E. Scott Daniels
#


# Look for errors in valgrind output. if not running valgrind, then check the 
# return code passed in. Reports over all success/failure to tty.
function check4err {

	typeset caller_rc=${1:-0}
	if [[ -z $valgrind ]]	# if not valgrind we'll assume the test rc was passed as an error indicator
	then
		if (( caller_rc == 0 ))
		then
			echo " [OK]"
		else
			echo " bad rc: $caller_rc [FAIL]"
		fi
		return
	fi

	# if caller set a value for ignore conditionals, then n errors is acceptable.
	#
	if grep -q "ERROR SUMMARY: 0 errors" /tmp/PID$$.out		# even if ignore is set, answer might be 0, so check that first
	then
		echo "[OK]"
	else
		if (( ignore_conditionals > 0 ))
		then
			if grep -q "ERROR SUMMARY: $ignore_conditionals errors" /tmp/PID$$.out
			then
				echo "[OK]"
			else
				echo "[FAIL]"
			fi
		else
			echo "[FAIL]"
		fi
	fi

	ignore_conditionals=0
}

# ---- special case tests ---------------------

# json for jwrapper tests, written to a file and removed at end
json='{
		"active_patient": true,
		"last_visit": "2015/02/03",
			
		"patient_info": {
			"first_name": "Fred",
			"last_name": "Flintsone",
			"dob": "1986/04/30",
			"sex": "M",
			"weight_kilo": 65,
			"drug_alergies": [ "asprin","darvaset" ]
	}

	"Contact_info": {
		"name": "Wilma", "relation": "wife", "phone": "972.612.8918"
	}
}'
echo "$json" >x.json

function jwrapper1 {
	$valgrind jwrapper_test x.json patient_info.dob "patient_info.drug_alergies" 1 >/tmp/PID$$.out 2>&1
	rc=$?

	if [[ -z $valgrind ]]			# test output for expected strings
	then
		if ! grep -q darvaset /tmp/PID$$.out
		then
			rc=1
		fi
		if ! grep -q "1986/04/30" /tmp/PID$$.out
		then
			rc=1
		fi
	fi

	return $rc
}

function jwrapper2 {
	$valgrind jwrapper_test x.json "last_visit" >/tmp/PID$$.out 2>&1
	rc=$?

	if [[ -z $valgrind ]]			# test output for expected strings
	then
		if ! grep -q "2015/02/03" /tmp/PID$$.out
		then
			rc=1
		fi
	fi
	return $rc
}

function jwrapper3 {
	$valgrind jwrapper_test x.json "patient_info.weight_kilo" >/tmp/PID$$.out 2>&1
	rc=$?

	if [[ -z $valgrind ]]			# test output for expected strings
	then
		if ! grep -q "65.00" /tmp/PID$$.out
		then
			rc=1
		fi
	fi
	return $rc
}

# create some dummy files and ensure that we can move/arrange them as needed
function file_sys {
	ps -elf >delete_file			# file with some meat to verify copy
	m5b=$( md5sum <delete_file )
	touch move_file
	touch backup_file

	dir2=$PWD/filesys_test_dir2 
	$valgrind filesys_test delete_file move_file filesys_test_dir1 $dir2 backup_file  >/tmp/PID$$.out 2>&1
	rc=$?

	if [[ -e $dir2/delete_file ]]
	then
		echo "[FAIL] unlink on copy file still existed in $dir2" >>/tmp/PID$$.out 2>&1
		rc=1
		ls -al $dir2 >>/tmp/PID$$.out 2>&1
	fi

	m5a=$( md5sum <$dir2/copied_file )
	if [[ "$m5a" != "$m5b" ]]
	then
		echo "[FAIL] copied file didn't have good checksum" >>/tmp/PID$$.out 2>&1
		rc=1
	fi

	rm -fr filesys_test_dir* backup_file-
	return $rc
}

# list files that have the desired prefix
function pfx_files {
	mkdir pfiles
	touch pfiles/foo_foo
	touch pfiles/foo_bar
	touch pfiles/bar_foo
	touch pfiles/bar_bar
	rc=$?

	$valgrind pfx_list_test pfiles foo_ >/tmp/PID$$.out 2>&1

	rm -fr pfiles
	return $rc
}

# list files that have the desired file extension/suffix (.foo)
function suffix_files {
	mkdir pfiles
	touch pfiles/foo.foo
	touch pfiles/foo.bar
	touch pfiles/bar.foo
	touch pfiles/bar.bar

	$valgrind list_test pfiles foo >/tmp/PID$$.out 2>&1
	rc=$?

	rm -fr pfiles
	return $rc
}

#
#	when not in leak analysis, we'll validate that the captured messages do not 
#	contain the 'should NOT be' phrase which would indicate a log message was 
#	written when it should not have been.  We also must ignore one valgrind
#	conditional jump message as there is a C library function in the time module
#	which does not completely initialise a target string buffer and valgrind gets
#	its knickers all twisted over it.
#
function bleat {
	rm -f foo.log*				# ditch any pre existing log files

	printf "(test will take about 5 minutes) "
	$valgrind bleat_test 300 >/tmp/PID$$.out 2>&1
	rc=$?

	if [[ -z $valgrind ]]		# scan output to validate further if not doing leak analysis
	then
		c=$( ls foo.log* |wc -l )
		if (( c == 3 ))
		then
			if grep -q "NOT" /tmp/PID$$.out foo.log*		# messages in any log with NOT are bad
			then
				echo "[FAIL] NOT found in one or more logs" >>$log
				rc=1
			fi
		else
			echo "[FAIL] expected 3 log files, found less" >>$log
			rc=1
		fi
	fi

	if (( rc == 0 ))		# keep logs only on failure
	then
		rm -f foo.log*
	fi

	ignore_conditionals=1	# number to ignore
	return $rc
}
		

PATH=.:$PATH

# ---------------------------------------------------------------------
ignore_conditionals=0

if [[ $1 == -l ]]			# enable leak checking (valgrind)
then
	valgrind=$( which  valgrind >/dev/null  )
	if [[ -z $valgrind ]]
	then
		valgrind=$( find $HOME -type f -name valgrind | head -1 )
		if [[ -z $valgrind ]]
		then
			echo "cannot find valgrind to run"
			exit 1
		fi
	fi

	shift
fi

log=${1:-all_tests.log}
>$log


# tests that can be run directly with valgrind
for x in id_mgr_test "vf_config_test vf_test.cfg" "parm_file_test parm_test.cfg" fifo_test
do
	printf "running %-20s"  "${x%% *}"
	printf "\n----- %s -----\n" "$x" >>$log 

	$valgrind $x >/tmp/PID$$.out  2>&1
	check4err $?

	cat /tmp/PID$$.out >>$log
done


# tests that need some special setup  or other considerations before starting with valgrind
# so we kick the speicialised function from above rather than the binary.
# run bleat last as it takes 5 minutes.
for x in file_sys  jwrapper1 jwrapper2 jwrapper3 pfx_files suffix_files bleat
do
	printf "running %-20s"  "${x%% *}"
	printf "\n----- %s-----\n" "$x" >>$log 

	$x
	check4err $?
	cat /tmp/PID$$.out >>$log
done

# ---- cleanup ----------------------------------
rm -f x.json
rm -f /tmp/PID$$.out		# our log capture
exit

