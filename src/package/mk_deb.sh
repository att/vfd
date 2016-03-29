#!/bin/bash

function clean_house
{
	trap - EXIT
	echo "something failed or you aborted"
	rm -fr /tmp/PID$$.*
	exit 1
}

function clean_exit
{
	trap - EXIT
	rm -fr /tmp/PID$$.*
	exit ${1:0}
}

function verbose
{
	if (( chatty > 0 ))
	then
		echo "$@" >&2
	fi
}

function help
{
	usage
	cat <<endKat
	VFD is a daemon which is used to configures NIC VF's
endKat
}

function usage
{
	echo ""
	echo "version 1.0"
	echo "usage: $argv0 [-a arch] [-p] [-u 'name <id@domain>'] [-v] <pkg-name> <version> [cleanup]"
	echo "	cleanup causes a cleanup of the named version and no building"
	echo "	-a arch allows the architecture to be specified amd64 is the default"
	echo "	-p      prompt; don't assume certain things"
	echo "	-r n    supplies the revsion number"
	echo "	-v      verbose mode"
	echo "	-u str  name and email address to be put into change (default: $who)"
}

function build_mail_str
{
	typeset w="${WHOIAM:-$user <$user@att.com>}"			# default

	if [[ -z $WHOIAM ]]				# let user override, but pull from git if we can
	then
		git config --list| grep user|sed 's/=/ /;' |while read what data
		do
			case $what in
				user.email)	ge="$data";;
				user.name)	gn="$data";;
			esac
		done
	
		if [[ -n $gn && -n $ge ]]
		then
			w="$gn <$ge>"
		fi
	fi

	echo "$w"
}

user=${LOGNAME:-$USER}
argv0=$0
arch="amd64"
confirm=0
rev=1							# deb revision number
chatty=0
who=$( build_mail_str )			# sets who

while [[ $1 == -* ]]
do
	case $1 in
		-a)	arch="$2"; shift;;
		-p)	confirm=1;;
		-r)	rev=$2; shift;;
		-u)	who="$2"; shift;;
		-v) chatty=1;;
		-\?) help
			exit 0
			;;
		*)	echo "unrecognised option $1"
			usage
			exit 1
	esac

	shift
done

pkg_name="$1"
full_pkg_name="attlr$1"
ver=$2
cleanup=$3

if [[ -z $pkg_name || -z $ver ]]
then
	echo "missing positional parameters  [FAIL]"
	usage
	exit 1
fi

src_dir=$PWD
if [[ ! -d ${pkg_name}_debian ]]
then
	echo "cannot find debian files directory in $src_dir: ${pkg_name}_debian  [FAIL]"
	exit 1
fi

bundle_dir=/tmp/$user/export/bundle 
if [[ ! -d $bundle_dir ]]
then
	echo "cannot find bundle directory:  /tmp/$user/export/bundle  [FAIL]"
	exit 1
fi

set -e									# lazy way out
trap "clean_house" 1 2 3 15 EXIT 		# purge everything on failure or exit

cd $bundle_dir 
rtar=$bundle_dir/${full_pkg_name}-${ver}.tar.gz		# rtar? original script pulled it from a remote machine :)
release=$bundle_dir/${full_pkg_name}_${ver}			# where we'll build the directory to make the release

if [[ -d $release  ]]
then
	set +e
	if ! rm -fr $release- $release
	then
		echo "unable to remove previous export directories: $release- $release"
		printf "try as root? [y|N]"
		read ans
		if [[ ${ans:-n} == "n"* ]]
		then
			clean_house 0
		fi
	fi

	set -e
	sudo rm -fr $release- $release
fi

if [[ $cleanup == "cleanup" ]]			# only invoked to clean 
then
	trap - EXIT
	exit 0
fi

if [[ -e $rtar ]]
then
	echo "using $rtar"
else
	echo "cannot find: $rtar"
	clean_exit 1
fi

verbose "making release directory $release"
mkdir -p $release
cd $release
rm -fr lib
verbose "unpack tar"
gzip -dc $rtar | tar -xf -

find . -type f |xargs md5sum >/tmp/PID$$.md5 		# capture md5sum before we add DEBIAN directory

verbose "populate DEBIAN directory"
(
	mkdir -p DEBIAN
	ls $src_dir/${pkg_name}_debian/[a-z]* | while read f
	do
		if [[ ${f##*/} == "control" ]]
		then
			sed "s/<VER>/$ver/" $f >DEBIAN/${f##*/} 
		else
			cp $f DEBIAN/
		fi
	done
)
cp /tmp/PID$$.md5 DEBIAN/md5sums

edit=1
if [[ -f ../changelog.$pkg_name.$ver ]]						# if one from a previous run, don't make them edit it again
then
	cp ../changelog.$pkg_name.$ver DEBIAN/changelog

	if (( confirm ))
	then
		printf "previous change log used; edit it? [Yn]"
		read ans
	else
		ans=n
	fi
	if [[ $ans == "n" ]]
	then
		edit=0
	fi
else
	# CAUTION -- the change log parser is way too sensitive -- leading spaces tabs seem to matter
	date=$(date -R)
	cat <<endKat >DEBIAN/changelog
${full_pkg_name} (${ver}-1) UNRELEASED; urgency=low

  * Bug Fixes. (Closes: #xxx)

 -- $who $date
endKat

	echo ""
	echo "a new change log was created, you must edit it"
	printf "press return to edit "
	read foo
fi

if (( edit ))
then
	vi DEBIAN/changelog
	cp DEBIAN/changelog ../changelog.$pkg_name.$ver
fi


if (( confirm ))
then
	printf "run debuild [yN]? "
	read ans
else
	ans=y
fi
if [[ $ans == "y"* ]]
then
	cd ..
	find $release | sudo xargs chown root:root 						# files in package must be owned by root
	deb_name=${full_pkg_name}_${ver}-${rev}_$arch.deb
	set -x
	dpkg-deb -b  $release $deb_name
	set +x
	find $release | sudo xargs chown $user:users

	echo "deb file created: $bundle_dir/$deb_name"
else
	echo "make any changes needed and then run 'dpkg-deb -b  $release ${full_pkg_name}_${ver}_$arch.deb'"
fi

trap - EXIT
exit 0
