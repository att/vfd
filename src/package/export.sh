#!/bin/bash

function usage
{
	echo "$argv0 [-b] [-B branch] [-d export-dir] [-p prefix-name] [-s src_dir] package-name version"
	echo "  -b turns off the auto build before export"
	echo "  if -s is not listed, then we expect VFD_SRC in the environment"
}

function cleanup
{
	trap - EXIT
	rm -f /tmp/PID$$.*
	exit $1
}

function build_it
{
	(
		set -e
        cd $src_dir/src/lib
        git clone https://github.com/att/dpdk.git -b vf-agent-dpdk
		cd $src_dir/src/lib/dpdk
		echo "building dpdk...."
		make install T=x86_64-native-linuxapp-gcc
		cd $src_dir/src
		export RTE_SDK=$PWD/lib/dpdk
		export RTE_TARGET=x86_64-native-linuxapp-gcc
		echo "building vfd...."
		cd $src_dir/src/lib
		make jsmn
		cd jsmn && make
		cd $src_dir/src/vfd
		if [[ ! -a vfdlib.h ]]
		then
			ln -s $src_dir/src/lib/vfdlib.h vfdlib.h
		fi
		make

		cd build/app
		./vfd -? | grep "^vfd" >/dev/null
	)

	if (( $? != 0 ))
	then
		echo "[FAIL] build failed"
		exit 1
	fi
}

function verbose
{
	if (( chatty ))
	then
		echo "[OK]	$@"
	fi
}

ex_root=/tmp/${LOGNAME:=$USER}/export
argv0="${0##*/}"
dir=""
compress_options=""
chatty=0
rebuild=0			# -r sets to mark as a rebuild of a previous package so that last ver is not updated
build=1				# force a build of binaries before exporting, -b turns off
prefix=attlr			# we bang this together with package name ($1) to generate something like attlrqlite (use -p to change)
branch="master"		# set with -B or we'll use the package name as the default -B not given
src_dir=${VFD_SRC}
confirm=0
pkg_dir=$PWD		# for references back to pwd after switching to source dir

while [[ $1 == -* ]]
do
	case $1 in
		-b)		build=0;;
		-B)		branch=$2; shift;;
		-c)		compress_options=$2; shift;;
		-C)		confirm=1;;
		-d)		dir=$2; shift;;
		-p)		prefix=$2; shift;;
		-r)		rebuild=1;;
		-s)		src_dir=$2; shift;;
		-v)		chatty=1;;
		-\?)	usage; exit 0;;
		*)		echo "unrecognized parameter: $1"
				usage
				exit 1
				;;
	esac
	shift
done

if [[ -z $1 ]]
then
	echo "[FAIL] missing package name as first parameter (e.g) vfd"
	usage
	cleanup 1
fi

if [[ -z $2 ]]
then
	echo "[FAIL] missing version number as second parameter"
	echo "last version was: "
	cat last_export_ver.$1 2>/dev/null  # CHECK LATER
	usage
	cleanup 1
fi

if [[ -z $src_dir || ! -d $src_dir ]]
then
	echo "[FAIL] source directory (${src_dir:-missing}) does not exist"
	cleanup 1
fi

pkg_name=$1
ver="$2"
name_ver=${pkg_name}_${ver}
pkg_list=${pkg_name}.exlist

if (( confirm ))
then
	echo "pwd:		$PWD"
	echo "branch: 	$branch"
	echo "src:		$src_dir"
	echo "pkg:		$pkg_name"
	echo "ver:		$ver"
	echo "list:		$pkg_list"
	echo "target:	${dir:-$ex_root}/$name_ver"
	if (( rebuild ))
	then
		echo "rebuild: true"
	else
		echo "rebuild: false"
	fi

	printf "continue? [Y|n]"
	read ans
	if [[ ${ans:-Y} == 'n'* ]]
	then
		cleanup 0
	fi
fi

if [[ ! -r $pkg_list ]]
then
	echo "[FAIL] unable to find export list: $pkg_list"
	cleanup 1
fi
sed 's/#.*//; /^$/d' $pkg_list >/tmp/PID$$.data		# snarf the list and strip comments/blank lines (before switch to src dir)

if ! cd $src_dir
then
	echo "[FAIL] unable to swtich to src dir: $src_dir"
	cleanup 1
fi

# commit_id=$( git log   --pretty="%H %cd"  ${branch:-master} |head -1 )
if ! git checkout ${branch:-nosuchthing}
then
	echo "[FAIL] unable to checkout branch: $branch"
	cleanup 1
fi

if (( ! rebuild ))
then
	echo $ver >$pkg_dir/last_export_ver.$1
fi

if (( build ))
then
	build_it
fi

if [[ -z $dir ]]
then
	dir=${ex_root}/${name_ver}
fi

if [[ ! -d $dir ]]
then
	if ! mkdir -p $dir
	then
		echo "[FAIL] cannot make export dir: $dir"
		cleanup 1
	else
		verbose "made export dir: $dir"
	fi
fi

verbose "populating....."

typeset -A seen

trap "echo something failed!!!" EXIT
set -e
compress=""
mode=""
while read src target mode compress junk
do
	if [[ -z ${seen[${target%/*}]} ]]
	then
		verbose "ensuring $dir/${target%/*} exists"
		mkdir -p $dir/${target%/*}
		seen[${target%/*}]="true"
	fi

	if [[ ! -f $src ]]
	then
		for x in .ksh .sh .bsh .py
		do
			if [[ -f "$src$x" ]]
			then
				if [[ $target == *"/" ]]
				then
					target+="${src##*/}"
				fi
				src+=$x
				break
			fi
		done
	fi
	verbose "$src -> $dir/${target} (${mode:-755}, ${compress:-no-compression})"
	if cp ${src} ${dir}/${target}
	then
		if [[ -z $mode || $mode == "-" ]]
		then
			mode="755"
		fi
		if [[ $target == *"/" ]]
		then
			ctarget=$dir/$target/${src##*/}
		else
			ctarget=$dir/$target
		fi

		chmod $mode $ctarget
		if [[ -n $compress ]]
		then
			echo "compressing: $compress $compress_options $ctarget"
			$compress $compress_options $ctarget
		fi
	fi
done </tmp/PID$$.data
verbose ""

if ! cd $dir
then
	echo "[FAIL] cannot cd to export dir ($dir) to create tar file"
	cleanup 1
fi

echo "creating tgz file...."
mkdir -p $ex_root/bundle/
tar -cf - . | gzip >/$ex_root/bundle/${prefix}${pkg_name}-${ver}.tar.gz
trap - EXIT
echo "packaged ready for deb build in: $ex_root/bundle/${prefix}${pkg_name}-${ver}.tar.gz"
cleanup 0
