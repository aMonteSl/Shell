#!/bin/sh

usage(){
	echo "usage: indent *.[ch]" 1>&2
	exit 1
}

dos2unixall(){
	for i in "$@"; do
		echo "warning: "$i" is a windows file, converting" 1>&2
		sed -E -i 's/\r//g' "$i"
	done
}

case $# in
0)
	usage
esac

if od -c "$@" |egrep '\\r' >/dev/null 2>&1; then
	dos2unixall "$@"
fi

uncomment() {
	gcc -fpreprocessed -dD -E "$@"
}

types() {
	uncomment "$1" | fgrep typedef  | fgrep struct | sed -E 's/.*[ \t]([a-zA-Z_]+) *[;}].*/\1/g'| sort -u
}

typesall() {
	for i in "$@"; do
		types "$i"
	done
}

for i in "$@"; do
	BADTYPEDEF=false
	ts=`typesall "$@"`
	if echo $ts|egrep 'struct|{' > /dev/null 2>&1 ; then
		echo Warning: separate typedef from type definition 1>&2
		egrep -n 'typedef.*struct' $i /dev/null  |sed -E 's/^/\t/' 2>&1
		BADTYPEDEF=true
	fi
	tparam=""
	for t in $ts; do
		tparam="$tparam "-T$t
	done
	if ! echo $i|grep '.*\.[hc]' > /dev/null 2>&1; then
		echo "$i" is not a C file 1>&2;
		usage
	fi
	if ! test -f $i > /dev/null 2>&1; then
		echo "$i does not exist or is not a file" 1>&2;
		usage
	fi
	if ! file "$i" |grep 'C source' > /dev/null 2>&1; then
		echo "$i does not contain C code" 1>&2;
		usage
	fi
	if [ $BADTYPEDEF = false ]; then
		#indent twice to undo trashy formatting
		indent -par -hnl -nsc -bl -ts8 -i8 -ut -bad -bap -nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4  -cli0 -d0 -di1 -nfc1 -ip0 -l80 -lp -npcs -nprs -psl -sai -saf -saw -ncs -sob -nfca -cp33 -ss -il1 -cdw "$i"> /dev/null 2>&1
		indent $tparam -par -hnl -nsc -bl -ts8 -i8 -ut -bad -bap -nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4  -cli0 -d0 -di1 -nfc1 -ip0 -l80 -lp -npcs -nprs -psl -sai -saf -saw -ncs -sob -nfca -cp33 -ss -il1 -cdw "$i"
	else
		#indent twice to undo trashy formatting
		indent -par -hnl -nsc -bl -ts8 -i8 -ut -bad -bap -nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4  -cli0 -d0 -di1 -nfc1 -ip0 -l80 -lp -npcs -nprs -psl -sai -saf -saw -ncs -sob -nfca -cp33 -ss -il1 -cdw "$i"> /dev/null 2>&1
		indent -par -hnl -nsc -bl -ts8 -i8 -ut -bad -bap -nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4  -cli0 -d0 -di1 -nfc1 -ip0 -l80 -lp -npcs -nprs -psl -sai -saf -saw -ncs -sob -nfca -cp33 -ss -il1 -cdw "$i"
	fi
	rm -f "$i"~
done
