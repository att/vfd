
# apply all dpdk<yymm>-* patches where yymm is given on the command line.

function usage
{
	echo "usage: $0 [-n] yymm"
	echo "   where yymm is the year/month of the dpdk release"
	echo "   -n is no execute mode"
}

VFD_PATH=$PWD

forreal=""
verify=0
action="appling: "
past_action="applied"

while [[ $1 == -* ]]
do
	case $1 in 
		-n) 	forreal="echo would execute: ";;

		-s)		
				action="summarise: "
				check="--check"
				past_action="summarised"
				summary="--stat --summary"
				;;

		-V)		verify=1;;
		-v)		verbose="-v";;

		*)	usage
			exit 1
			;;
	esac

	shift
done


if [[ -z $1 ]]
then
	usage
	exit 1
fi

patches=$( ls dpdk$1-* 2>&1 )

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

		if [[ -n $check ]]
		then
			git apply  $check --ignore-whitespace $VFD_PATH/$patch	>/dev/null 2>&1
			if (( $? != 0 ))
			then
				echo " ### Patch already applied, or is not valid for this version: $patch"
			fi
		fi

		if [[ -z $forreal ]]
		then
			echo "$action $patch"
		fi
		$forreal git apply $verbose $summary --ignore-whitespace $VFD_PATH/$patch	# if summary is on, apply is off
	)
	if (( $? != 0 ))
	then
		echo "abort: error applying patch $patch"
		exit 1
	fi

	echo "patch successfully $past_action"
done
