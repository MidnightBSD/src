# $FreeBSD: release/7.0.0/tools/regression/bin/sh/regress.sh 151797 2005-10-28 14:02:42Z stefanf $

COUNTER=1

do_test() {
	local c
	c=${COUNTER}
	COUNTER=$((COUNTER+1))
	sh $1 > tmp.stdout 2> tmp.stderr
	if [ $? -ne $2 ]; then
		echo "not ok ${c} - ${1} # wrong exit status"
		rm tmp.stdout tmp.stderr
		return
	fi
	for i in stdout stderr; do
		if [ -f ${1}.${i} ]; then
			if ! cmp -s tmp.${i} ${1}.${i}; then
				echo "not ok ${c} - ${1} # wrong output on ${i}"
				rm tmp.stdout tmp.stderr
				return
			fi
		elif [ -s tmp.${i} ]; then
			echo "not ok ${c} - ${1} # wrong output on ${i}"
			rm tmp.stdout tmp.stderr
			return
		fi
	done
	echo "ok ${c} - ${1}"
	rm tmp.stdout tmp.stderr
}

TESTS=$(find -Es . -regex ".*\.[0-9]+")
printf "1..%d\n" $(echo ${TESTS} | wc -w)

for i in ${TESTS} ; do
	do_test ${i} ${i##*.}
done
