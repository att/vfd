#!/usr/bin/env ksh

# : vi ts=4 sw=4 noet :

#	Mnemonic:	cfh_tests.ksh
#	Abstract:	This script runs through a bunch of vf configuration file handling
#				tests. The tests verify that VFd does the right thing with regard
#				to where configuration files are read from, moved to, and whether
#				or not they are used to delete existing VF configurations. These
#				tests do not verify that the configurations were correctly read,
#				nor do they verify that the VFs configured operate as expected; 
#				there are other tests which do those things. The majority of the 
#				tests here were created to verify changes for issues 263 and 262.
#
#				Environment:
#				VFd should NOT be running when this script is started; the script
#				will abort if it is. 
#
#				The script creates a config directory in /tmp to avoid stomping on
#				what might be an otherwise real VFd environment in /var/lib and /var
#				log. The directory will be deleted unless the -k option is given on
#				the command line (mostly for script debugging, but also for manual
#				verification of log output etc.).
#
#	Author:		E. Scott Daniels
#	Date:		19 February 2018
# ---------------------------------------------------------------------------------

# generate the main config for starting vfd
# log dir and config dir are the only real important things here.
function gen_main_cfg {
	vfd_conf=$wdir/etc/vfd.cfg
	verbose "create config: $vfd_conf"
	cat <<endKat >$vfd_conf

 {   
  "log_dir":      "$wdir/log/vfd",
  "config_dir":   "$wdir/lib/vfd/config",

  "log_keep":     10,
  "log_level":    1,
  "init_log_level": 3,
  "dpdk_log_level": 1,
  "dpdk_init_log_level": 2,
  "fifo": "$wdir/lib/vfd/request",
  "cpu_mask":      "0x01",
  "default_mtu":   1500,
  "enable_qos":   false,
  "enable_flowcontrol": false,
     
  "pciids": [ 
    { "id": "$pci_address", 
      "promiscuous": false,
      "mtu": 9240,
      "hw_strip_crc":   true,
      "enable_loopback": true,
      "pf_driver": "igb-uio",
      "vf_driver": "vfio-pci",
      "vf_oversubscription": true,
      "tclasses": [
        { "name": "best effort",
          "pri": 0,
          "latency": false,
          "lsp": false,
          "bsp": false,
          "max_bw": 100,
          "min_bw": 10 },
        { "name": "realtime",
          "pri": 1,
          "latency": false,
          "lsp": false,
          "bsp": false,
          "max_bw": 100,
          "min_bw": 40 },
        { "name": "voice",
          "pri": 2,
          "latency": false,
          "lsp": false,
          "bsp": false,
          "max_bw": 100,
          "min_bw": 40 },
        { "name": "control",
          "pri": 3,
          "latency": false,
          "lsp": false,
          "bsp": false,
          "max_bw": 100,
          "min_bw": 10 }
     ],
 
     "bw_grps": { "bwg0": [0],
                  "bwg1": [1, 2],
                  "bwg2": [3]
                }
	}
  ]
 }
endKat

}

#
# generate a VF configuration file with name $1 and the VF id $2 to stdout. The caller
# must redirect because for some tests we need these generated to the live directory.
#
function gen_vf_config {
	cat <<endKat 
    {
     "name":             "${1:-unnamed}"
     "pciid":            "$pci_address",
     "vfid":             ${2:-1},
  
     "strip_stag":       true,
     "allow_bcast":      true,
     "allow_mcast":      true,
     "allow_un_ucast":   false,
     "vlan_anti_spoof":  true,
     "mac_anti_spoof":   true,
     "vlans":            [ 22, 21 ],
     "macs":             [ "fa:ce:ed:09:01:01" ],
     "queues": [
         { "priority": 0, "share": "10%" },
         { "priority": 1, "share": "10%" },
         { "priority": 2, "share": "10%" },
         { "priority": 3, "share": "10%" },
     ]
   }
endKat
}

#
#	Various message functions that add the current id and may suppress the message depending
#	on the verbosity setting.
#
function verbose {
	if (( ! verbose_lvl ))				# for now, all or nothing
	then
		return
	fi

	echo "[INFO] $id: $@" >&2
}

function report_err {
	(( errors++ ))
	if (( verbose_lvl ))
	then
		verbose "last output capture from iplex (may not be related to this failure)"
		if [[ -f /tmp/PID$$.out ]]
		then
			cat /tmp/PID$$.out
		fi
	fi
	echo "[FAIL] $id: $desc" >&2
	echo "[FAIL] $id: $@" >&2
}

function report_warn {
	(( warns++ ))
	echo "[WARN] $id: $@" >&2
}

function report_ok {
	echo "[OK]   $id: $@" >&2
}

#
# returns good if vfd is running, and bad (!0) if vfd is not running.
#
function is_vfd_running {
	
	c=$( ps -elf|egrep -v "grep|${argv0##*/}" |grep -c ${binary##*/} )
	if(( c ))
	then
		return 0
	fi

	return 1
}

#
# ensure that all directories provided as parms exist.
#
function ensure_d {
	for d in "$@"
	do
		if [[ ! -d $d ]]
		then
			if ! mkdir -p $d
			then
				report_err "abort unable to create directory: $d"
				exit 1
			fi
		fi
	done
}

#
#	 Nuke the work directory and rebuild it. Log directory is kept
#	 as we create logs with test suffixes and want them all hanging
#	 about at the end if -k was given.
#
function reset_env {
	if [[ $wdir != "/tmp/vfd_"* ]]
	then
		report_err "refusing to nuke the directory: $wdir"
		exit 1
	fi

	rm -fr $wdir/lib
	rm -fr $wdir/etc
	rm -fr $wdir/config
	rm -fr /tmp/PID$$.*

	ensure_d $wdir $wdir/etc/vfd $wdir/log/vfd $wdir/lib/vfd/config  $wdir/lib/vfd/config_live
}

#
# ditch all of our working environment
#
function destroy_env {
	if [[ $wdir != "/tmp/vfd_"* ]]
	then
		report_err "refusing to destroy the directory: $wdir"
		exit 1
	fi

	if [[ ! -d $wdir ]]
	then
		verbose "destroy found nothing to trash"
		return
	fi
	
	rm -fr $wdir
	rm -fr /tmp/PID$$.*
}

#
#	start VFd stashing away the PID. The tests should set the id variable which we will
#	use in the log string.
#
function start_vfd {
	if is_vfd_running				# if test failed, it might still be running; stop it now
	then
		verbose "restarting VFd ($binary) log=$wdir/log/vfd.$id.log"
		stop_vfd
	else
		verbose "starting VFd ($binary) log=$wdir/log/vfd.$id.log"
	fi

	if [[ ! -f $vfd_conf ]]
	then
		report_err "abort: can't find vfd.cfg in $vfd_conf"
		exit 1
	fi

	sudo $binary -f -p $vfd_conf >$wdir/log/vfd.$id.log 2>&1 &
	vfd_pid=$!
	typeset count=0

	sleep 5														# let the jello stop wiggling
	if ! is_vfd_running
	then
		report_err "abort: unable to start VFd $(binary)"
		exit 1
	fi

	verbose "waiting for port ready message in log"
	while ! grep -q "is ready" $wdir/log/vfd.$id.log 2>/dev/null		# wait for up to 2 minutes for it to come ready
	do
		(( count++ ))
		if (( count > 120 ))
		then
			report_err "abort: VFd seems to have started, but no port ready messages found in log: $wdir/log/vfd.$1.log"
			exit 1
		else
			if (( count % 10 == 0 ))
			then
				verbose "still waiting, will abort in $(( 120 - count )) seconds"
			fi
		fi

		sleep 1
	done

	verbose "vfd started successfully ($vfd_pid)"
}

#
#	Assuming we have a pid, kill -15 and verify that it's not still runing; abort if it cannot be stopped.
function stop_vfd {
	if [[ -z $vfd_pid ]]
	then
		return
	fi

	verbose "stopping VFd"
	sudo kill -15 $vfd_pid
	vfd_pid=""

	sleep 2
	if is_vfd_running
	then
		report_err "abort: VFd still is running after attempting to kill (pid=%vfd_pid)"
		exit 1
	fi
}


#
#	Adds a vf config ($2) using iplex. 
#
function add_vf {
	if ! sudo iplex --conf=$vfd_conf add $1 >/tmp/PID$$.out 2>&1
	then
		return 1
	fi

	if grep -q "ERR" /tmp/PID$$.out		# iplex might return success regardless of the content, so check content too
	then
		return 1
	fi

	if ! is_in_log "vf added: vf added successfully: .*$2"			# vfd verbose >1 must be enabled for this message to appear
	then
		rport_error "add reported successful by iplex, but no log message"
		return 1
	fi

	return 0
}


#
#	Deletes a vf config ($2) using iplex. 
#
function del_vf {
	if ! sudo iplex --conf=$vfd_conf delete $1 >/tmp/PID$$.out 2>&1
	then
		return 1
	fi

	if grep -q "ERR" /tmp/PID$$.out		# iplex might return success regardless of the content, so check content too
	then
		return 1
	fi

	return 0
}


function usage {
	echo ""
	echo "usage: $argv0 [-b bin-dir(s)] [-k] [-p pci-address] [-v] [-w test-list]"
	echo ""
	echo "   valid tests for -w:  test_g1 test_i262_1 test_i263_1 test_i263_2 test_i263_3 test_i263_4 test_start test_runnin"
}

#
# search the log generated by test $1 for the string $2
#
function is_in_log {
	
	#set -x
	#grep  "${1:-nosearchstringgiven}" $wdir/log/vfd.${id:-no_test}.log
	#echo ">>>> $?"
	#set +x
	grep -q "${1:-nosearchstringgiven}" $wdir/log/vfd.${id:-no_test}.log
	return $?
}

# ------- tests --------------------------------------------------------------------

#	General Test 1
#	Ensure that a configuration which specifies the same PF/VF combination is NOT
#	added and is NOT moved to the live directory
#
function test_g1 {
	id="g1"
	desc="ensure dup add VF/PF is rejected"
	verbose "starting: $desc"

	reset_env
	gen_main_cfg
	start_vfd

	gen_vf_config foo_bar 1 >$wdir/lib/vfd/config/foo_bar.json
	if ! add_vf foo_bar
	then
		report_err "cannot add foo_bar vf config"
		return 1
	fi

	gen_vf_config bar_bar 1 >$wdir/lib/vfd/config/bar_bar.json			# same vf/pf as foo_bar
	if add_vf bar_bar
	then
		report_err "config with duplicate pf/vf was added"
		return 1
	fi

	report_ok "test passed: $desc"
}

#	Issue 262, test 1
#	Ensure that deleting a VF using a configuration with incorrect name does not
#	cause any change to NIC configuration. 
#
#	1) Create a config, foo_bar.json, with the name "foo_bar"
#	2) iplex add foo_bar
#	3) Verify PF/VF configured with foo_bar (no error reported and with show)
#	4) Create a config, bar_bar.json with the name "bar_bar", AND with the
#	   same PF/VF combination used in foo_bar.json; forcing it to be in the
#	   live directory.
#	5) iplex delete bar_bar
#	6) Verify:
#		a) error reported by VFd on iplex command
#		b) show all still reports the PF/VF combination in the list
#		c) there is an error message in the log indicating that the names
#		   did not match and no action was taken.
#
#	Note: 
#		Tthis test is a bit contrived as it should not be possible to have a
#		config file in the live directory that wasn't successfully added.
#
#		This test also verifies one of the changes from issue 263 -- that the
#		config file is moved to the _live directory after it is accepted.
#
function test_i262_1 {
	id="test_i262_1"

	desc="ensure del of vf with incorrect name in config is rejected"
	verbose "starting"
	reset_env					# reset the environment
	gen_main_cfg				# build the main config for this test

	start_vfd

	gen_vf_config foo_bar 1 >$wdir/lib/vfd/config/foo_bar.json
	if ! add_vf foo_bar
	then
		report_err "can't add vf config"
		return 1
	fi

	if [[ ! -f $wdir/lib/vfd/config_live/foo_bar.json ]]						# sub test -- vfd should move successful adds to live dir
	then
		desc="ensure config moved to live directory after successful add"
		report_err "foo_bar configuration was not moved to live directory after add"
		ls -al $wdir/lib/vfd/config_live/*
		return 1
	fi

	desc="ensure del of vf with incorrect name in config fails"
	gen_vf_config bar_bar 1 >$wdir/lib/vfd/config_live/bar_bar.json				# force into live directory
	if sudo iplex --conf=$vfd_conf delete bar_bar >/tmp/PID$$.out 2>&1			# iplex should end with a good rc; check for err
	then
		if ! grep -q "ERROR" /tmp/PID$$.out
		then
			report_err "delete of bar_bar config was reported as being successful"
			return 1
		fi
	else
		report_err "abort: iplex reported error trying to delete misnamed config; this is not expected"
		return 1
	fi
	report_ok "vfd rejected delete of mis-named config"
	
	if [[ -f $wdir/lib/vfd/config_live/bar_bar.json ]]		# vfd should also delete the file
	then
		report_err "delete was rejected, but offending config (bar_bar) was not moved out of live directory"
		find $wdir/lib/vfd -ls
		return 1
	fi

	if [[ ! -f $wdir/lib/vfd/config_live/foo_bar.json ]]		# the original config should still be there
	then
		report_err "delete was rejected, but valid config (foo_bar) is now missing"
		return 1
	fi

	if ! is_in_log "name in config did not match name"
	then
		report_warn "delete was rejected, but there isn't a related message in the log"
	fi

	count=0
	sudo iplex --conf=$vfd_conf show all >/tmp/PID$$.out
	egrep "^vf[ \t]+1[ \t]+" /tmp/PID$$.out | while read buf 
	do
		(( count++ ))
	done
	if (( ! count ))
	then
		report_err "did not find expected vf (1) in show all output"
		return 1
	fi

	stop_vfd
	report_ok "test passed: $desc" 

	return 0
}

#	Issue 263
#	Test 1 - VFd creates the live config directory on start when missing
#	1) Remove /var/lib/vfd/config_live 
#	2) Start VFd
#	3) Verify that /var/lib/vfd/config_live is created
#
function test_i263_1 {
	id="test_i263_1"

	desc="ensure live config directory is created"
	verbose "starting"
	reset_env					# reset the environment
	gen_main_cfg				# build the main config for this test

	rm -fr $wdir/lib/vfd/config_live		# VFd should create if missing

	start_vfd

	if [[ ! -d $wdir/lib/vfd/config_live ]]
	then
		report_err "vfd did not create the live config directory as expected: $wdir/lib/vfd/config_live "
		return 1
	fi

	stop_vfd
	report_ok "test passed: $desc"

	return 0
}

#	Issue 263
#	Test2 - VFd tags config file with 'error' suffix on add error
#	1) Create VF configuration file foo_bar.json  and add it.
#	2) Create VF configuration bar_bar with the same PF/VF as foo_bar
#	2) iplex add bar_bar
#	3) Verify:
#		a) add command failed
#		b) bar_bar.json is renamed bar_bar.json.error
#		c) error file is still in the /var/lib/vfd/config directory
#		   and NOT in the live directory.
#
function test_i263_2 {
	id="test_i263_2"

	desc="config file moved to *.error if add fails"
	verbose "starting"
	reset_env					# reset the environment
	gen_main_cfg				# build the main config for this test

	start_vfd
	gen_vf_config foo_bar 1 >$wdir/lib/vfd/config/foo_bar.json
	if ! add_vf foo_bar
	then
		report_err "unable to add configuration foo_bar.json"
		return 1
	fi

	gen_vf_config bar_bar 1 >$wdir/lib/vfd/config/bar_bar.json		# duplicate pf/vf values
	if add_vf bar_bar												# iplex should report error
	then
		report_err "attempt to add config with duplicate vf/pf did not fail as expected"
		return 1
	fi

	if [[ ! -f $wdir/lib/vfd/config/bar_bar.json.error ]]
	then
		report_err "expected error configuration file to be renamed with .error suffix"
		return 1
	fi

	if [[ -f $wdir/lib/vfd/config_live/bar_bar.* ]]
	then
		report_err "unexpected file found in live directory with prefix bar_bar"
		return 1
	fi

	stop_vfd
	report_ok "test passed: $desc"

	return 0
}
	

# Issue 263
#	Test3 - VFd removes config from live directory on delete
#	1) Create and then add foo_bar.json
#	2) iplex delete foo_bar
#	3) Verify:
#		a) foo_bar.json does NOT exist in the live configuration directory.
#		b) Log messages indicate that the VF was deleted.
#		c) Output from iplex show does not list the PF/VF combination.
#		d) foo_bar.json- does NOT exist in the config directory.
#
function test_i263_3 {
	id="test_i263_3"

	desc="config file removed from live directory when deleted"
	verbose "starting"
	reset_env					# reset the environment
	gen_main_cfg				# build the main config for this test

	start_vfd
	gen_vf_config foo_bar 1 >$wdir/lib/vfd/config/foo_bar.json
	if ! add_vf foo_bar
	then
		report_err "unable to add a configuration for the test"
		return 1
	fi

	if ! del_vf foo_bar
	then
		report_err "unable to delete a configuration for test"
		return 1
	fi

	if [[ -f $wdir/var/lib/config_live/foo_bar.json ]]
	then
		report_err "json file still in live directory after delete: $wdir/var/lib/config_live/foo_bar.json"
		return 1
	fi

	if [[ -f $wdir/var/lib/config/foo_bar.json- ]]
	then
		report_err "json- file still in config directory after delete: $wdir/var/lib/config/foo_bar.json-"
		return 1
	fi

	# future: look show all output to ensure that the vf is gone
	stop_vfd
	report_ok "test passed: $desc"
	
	return 0
}

# Issue 263
# Test6 - VFd reads configurations only from live directory when starting
#	1) Ensure VFd not running; stop if it is
#	2) Create /var/lib/vfd/config_lib/foo_bar.json 
#	3) Create /var/lib/vfd/config/bar_bar.json
#	4) Start Vfd
#	5) Verify:
#		a) the VF/PF specified in foo_bar.json is active (iplex show 
#		   output and/or log messages indicating it was read and added).
#		b) The VF/PF combination specified in bar_bar.json does NOT
#		   appear in iplex show output or log messages.
#
function test_i263_4 {
	id="test_i263_4"

	desc="only files in live config are read at start"
	verbose "starting"
	reset_env					# reset the environment
	gen_main_cfg				# build the main config for this test

	stop_vfd					# ensure it is not running

	gen_vf_config foo_bar 1 >$wdir/lib/vfd/config_live/foo_bar.json			# force into live directory
	gen_vf_config bar_bar 1 >$wdir/lib/vfd/config/bar_bar.json

	start_vfd

	# future -- iplex show ensure just 1 vf

	if is_in_log "bar_bar"
	then
		report_err "found reference to bar_bar named vf when it should not have been used"
		return 1
	fi

	stop_vfd
	report_ok "test passed: $desc"

	return 0
}


# just test the starting of vfd with this script
#
function test_start {
	id="test_start"
	oldv=$verbose_lvl
	verbose_lvl=1
	verbose "starting vfd"
	reset_env					# must have environment for log files etc
	gen_main_cfg
	start_vfd

	verbose "sleeping $delay seconds before stopping vfd"
	sleep $delay

	verbose "stopping vfd"
	stop_vfd

	verbose_lvl=oldv
}

# just test the verify is up function
function test_running {
	id="test_running"
	oldv=$verbose_lvl
	verbose_lvl=1
	if is_vfd_running 
	then
		verbose "vfd is running"
	else
		verbose "vfd is not running"
	fi

	verbose_lvl=$oldv		# restore on off chance this isnt only test
}

# ---------------------------------------------------------------------------------


delay=5
argv0=$0
keep=0
errors=0							# number of errors we detected
warns=0
wdir=/tmp/vfd_cfh_tests
pci_address="0000:08:00.0"
bin_dir="../../vfd/build/app:../../system"			# where vfd lives; assume build environment
verbose_lvl=0
what=" test_g1 test_i262_1 test_i263_1 test_i263_1 test_i263_2 test_i263_3 test_i263_4"		# what tests to execute
id="main"
binary="vfd"						# -B allows full path to a test binary
vfd_conf="missing"					# gen_main_cfg will set so that if we need to alter based on test we can

while [[ $1 == -* ]]
do
	case $1 in 
		-B)		binary=$2; shift;;				# binary to use
		-b)		bin_dir="$2"; shift;;
		-k) 	keep=1;;
		-p)		pci_address="$2"; shift;;

		-v) 	verbose_lvl=1;;
		-w) 	what="$2"; shift;;

		-\?)	usage
				exit 0
				;;

		*)		echo "unrecognised option $1"
				usage
				exit 1
				;;
	esac

	shift
done

PATH=$PATH:$bin_dir

if ! which $binary >/dev/null 2>&1 
then
	echo "abort: vfd ($binary) is not in the current path: $PATH"
	exit 1
fi

if [[ $what != "test_running" ]]		# ok for it to be running given this test :)
then
	if is_vfd_running
	then
		echo "abort: it seems that VFd is running; must be stopped to run this script"
		exit 1
	fi
fi

destroy_env					# at the very start, ensure everything is nuked off


verbose "running the following tests: $what"
for t in $what
do
	verbose "------------------------------------------------------------------"
	$t
done

stop_vfd					# ensure it goes away

id="main"
if ! (( errors || keep ))
then
	verbose "destroying the environment"
	destroy_env
fi

rm -f /tmp/PID$$.*			# clean out any tmp files

exit $(( !! $errors  ))


