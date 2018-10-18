#!/usr/bin/env ksh

#	Mnemonic:	verify_patches.ksh
#	Abstract:	This scirpt will suss out the dpdk version based on the current branch
#				checked out in RTE_SDK and then look to see if we have any patches that
#				are not applied. If all are applied, then we exit good.
#	Auhtor:		E. Scott Daniels
#	Date:		18 October 2018
# ---------------------------------------------------------------------------------------

function usage
{
	echo "usage: $0 [-b] [-d patch_dir] [-f] [-v] "
	echo "-b causes final message to be printed in a brief good/bad format"
	echo "-f forces the exit code to be 0 even if unapplied patches were found"
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

# ---------------------------------------------------------------------------------

VFD_PATH=$PWD

force=0
patch_dir=.
summary="--stat --summary"		# git options
verbose=0
brief=0							# final message is just good/bad and no text

while [[ $1 == -* ]]
do
	case $1 in 
		-b)		brief=1;;
		-d)		VFD_PATH=$2; shift;;
		-f)		force=1;;

		-v)		verbose=1;;

		*)	
			echo "option $1 was not recognised"
			usage
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


version=$( dpdk_ver )				# get the dpdk version 
if (( verbose ))
then
	echo "looking for patches of the form dpdk$version" >&2
fi


patches=$( ls $VFD_PATH/dpdk$version-* 2>/dev/null )

if [[ -z $patches ]]
then
	if (( brief ))
	then
		echo "0/0"
	else
		echo "there are no patches for dpdk$version"
	fi
	exit 0
fi

tried=0
bad=0
for patch in $patches
do
	(( tried++ )) 
	(
		rc=0
		cd $RTE_SDK

		git apply  -v --check  --ignore-whitespace $patch 2>&1|awk '/does not apply/ { found=1 } END { exit( !(found+0) ) }'
		if (( $? == 0 ))
		then
			if (( verbose )) 
			then
				echo "[OK]    DPDK patch was already applied, or is not valid for this version: $patch" >&2
			fi
		else
			rc=1
			echo " ### ERROR ### patch NOT applied to DPDK version: $patch" >&2
		fi

		exit $rc		# a return from the subshell, not from the overall script!
	)
	(( bad += $? ))
done

if (( brief ))
then
	echo "$(( tried - bad ))/$bad"
else
	if (( verbose ))
	then
		echo "$(( tried - bad ))  dpdk ($version) patches were applied, $bad patches were not applied"
	fi
fi

if (( force ))		# make may force a good return so that it can just capture the verbose good/bad count string and continue
then
	exit 0
fi
exit $(( bad > 0 ))
