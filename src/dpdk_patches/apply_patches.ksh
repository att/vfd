#!/usr/bin/env ksh

#	Mnemonic:	apply_patches.ksh
#	Abstract:	Apply all dpdk<yymm>-* patches where yymm is given on the command line.
#				Any matching patch will be tested to see if it's been applied, and if
#				not an attempt will be made to apply it.  This script assumes that the
#				DPDK source is in the directory referenced by the RTE_SDK environment var.
#	Auhtor:		E. Scott Daniels
#	Date:		2 February 2018
# ---------------------------------------------------------------------------------------

function usage
{
	echo "usage: $0 [-d patch_dir] [-f] [-n] [-s] [-v] {yymm | vyy.mm[_rcxx]}"
	echo "   where yymm is the year/month of the dpdk release"
	echo "   -n is no execute mode"
}

# flips to the dpdk directory and attemptes to figure out what the version is based
# on the current branch name. We assume that the branche (e.g. v18.02) is checked
# out for build.
#
function dpdk_ver {
	(
		cd $RTE_SDK; 
		git branch -l|grep "*"| awk '
			{ 
				gsub( ")", "" ); 
				gsub( "v", "" ); 
				gsub( "\\.", "" );  
				print $NF 
			}' 
	)
}

VFD_PATH=$PWD

vlevel=0
forreal=""
check=0
verify=0
action="appling: "
past_action="applied"
patch_dir=.
force=0

while [[ $1 == -* ]]
do
	case $1 in 
		-d)		VFD_PATH=$2; shift;;
		-f)		force=1;;
		-n) 	forreal="echo would execute: ";;

		-s)		
				action="summarise: "
				check=1
				past_action="summarised"
				summary="--stat --summary"
				;;

		-V)		verify=1;;
		-v)		vlevel=1; verbose="-v";;

		*)	usage
			exit 1
			;;
	esac

	shift
done

if [[ ! -d $VFD_PATH ]]
then
	echo "patch directory smells: $VFD_PATH"
	exit 1
fi

if [[ ! -d ${RTE_SDK:-no-such-directory} ]]
then
	echo "cannot find dpdk directory: ${RTE_SDK:-RTE_SDK is not set}"
	exit 1
fi


if [[ -n $1 ]] 						# version given
	if [[ $1 == "v"*"."* ]]				# something like v18.02 given
	then
		version=${1//[v.]/}
		version=${version%%_*}			# chop _rc2 or something similar
	else
		version=$1
	fi
	dver=$( dpdk_ver )
	if [[ $dver != $version ]]
	then
		echo "### ERROR ### version from command line ($version) does NOT match checked out version in RTE_SDK ($dver)"
		if (( ! force ))
		then
			exit 1
		fi
	
		echo "### WARN ###  force mode enabled, continuing even though in error state"
	fi
else
	version=$( dpdk_ver )			# just use what we suss out
	if [[ $version == "master" ]] 	# this seems wrong unless -f is on
	then
		echo "### ERROR ### dpdk branch in RTE_SDK is master and this seems wrong"
		if (( ! force ))
		then
			exit 1
		fi
		echo "### WARN ###  force mode enabled, continuing even though in error state"
fi



patches=$( ls $VFD_PATH/dpdk$version-* 2>/dev/null )

if [[ -z $patches ]]
then
	echo "there are no patches for dpdk$1"
	exit 0
fi

if [[ ! -d ${RTE_SDK} ]]
then
	echo "abort: cannot find rte sdk directory: ${RTE_SDK:-undefined}"
	echo "ensure RTE_SDK is defined and exported"
	exit 1
fi

log=/tmp/PID$$.log				# so we can know whta $$ is since generated in subshell
for patch in $patches
do
	echo ""

	if (( verify ))				# prompt user to let them pick patches to apply from the list
	then
		printf "verify: apply $patch ?"
		read ans
		if [[ $ans != y* ]]
		then
			continue
		fi
	fi

	(
		cd $RTE_SDK

		if (( check ))
		then
			git apply  -v --check  --ignore-whitespace $patch 2>&1|awk '/does not apply/ { found=1 } END { exit( !(found+0) ) }'
			if (( $? == 0 ))
			then
				echo "[OK]    DPDK patch already applied, or is not valid for this version: $patch"
			else
				if (( vlevel ))
				then
					echo "[INFO] patch is needed: $patch"
				fi
			fi
		fi

		if [[ -z $forreal ]]
		then
			echo "$action $patch"
		fi
		$forreal git apply $verbose $summary --ignore-whitespace $patch	>$log 2>&1 # if summary is on, apply is off
	)
	if (( $? != 0 ))
	then
		echo "abort: error applying patch $patch"
		echo ""
		if [[ -f $log ]]
		then
			cat $log
		fi
		exit 1
	fi

	if (( vlevel ))
	then
		if [[ -f $log ]]
		then
			cat $log
		fi
	fi


	if [[ -z $forreal ]]
	then
		echo "patch successfully $past_action"
	fi
done

rm -f $log
