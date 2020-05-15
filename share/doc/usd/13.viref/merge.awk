# $FreeBSD: stable/11/share/doc/usd/13.viref/merge.awk 284619 2015-06-20 00:13:28Z bapt $
#	@(#)merge.awk	8.3 (Berkeley) 5/25/94
#
# merge index entries into one line per label
$1 == prev {
	printf ", %s", $2;
	next;
}
{
	if (NR != 1)
		printf "\n";
	printf "%s \t%s", $1, $2;
	prev = $1;
}
END {
	printf "\n"
}
