# $FreeBSD: stable/11/tools/tools/locale/tools/extract-colldef.awk 298116 2016-04-16 17:36:02Z bapt $

BEGIN {
	print "# Warning: Do not edit. This is automatically extracted"
	print "# from CLDR project data, obtained from http://cldr.unicode.org/"
	print "# -----------------------------------------------------------------------------"
}
$1 == "comment_char" { print $0 }
$1 == "escape_char" { print $0 }
$1 == "LC_COLLATE" {
	print $0
	while (getline line) {
		print line
		if (line == "END LC_COLLATE") {
			break
		}
	}
}
