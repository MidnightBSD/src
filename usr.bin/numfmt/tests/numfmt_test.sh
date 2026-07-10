#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case to_si
to_si_head()
{
	atf_set "descr" "scale numbers to SI suffixes"
}
to_si_body()
{
	atf_check -s exit:0 -e empty -o "inline:1.5k\n" \
	    numfmt --to=si 1500
}

atf_test_case to_iec_i
to_iec_i_head()
{
	atf_set "descr" "scale numbers to IEC-i suffixes"
}
to_iec_i_body()
{
	atf_check -s exit:0 -e empty -o "inline:1.5Ki\n" \
	    numfmt --to=iec-i 1536
}

atf_test_case from_iec
from_iec_head()
{
	atf_set "descr" "scale input from IEC suffixes"
}
from_iec_body()
{
	atf_check -s exit:0 -e empty -o "inline:1024\n" \
	    numfmt --from=iec 1K
}

atf_test_case field
field_head()
{
	atf_set "descr" "convert a selected input field"
}
field_body()
{
	printf "name 1500 bytes\n" >input
	printf "name 1.5k bytes\n" >expected

	atf_check -s exit:0 -e empty -o file:expected \
	    numfmt --field=2 --to=si <input
}

atf_test_case header
header_head()
{
	atf_set "descr" "preserve header lines"
}
header_body()
{
	printf "SIZE\n1500\n" >input
	printf "SIZE\n1.5k\n" >expected

	atf_check -s exit:0 -e empty -o file:expected \
	    numfmt --header --to=si <input
}

atf_test_case suffix
suffix_head()
{
	atf_set "descr" "strip input suffix and append output suffix"
}
suffix_body()
{
	atf_check -s exit:0 -e empty -o "inline:1.5kB\n" \
	    numfmt --suffix=B --to=si 1500B
}

atf_init_test_cases()
{
	atf_add_test_case to_si
	atf_add_test_case to_iec_i
	atf_add_test_case from_iec
	atf_add_test_case field
	atf_add_test_case header
	atf_add_test_case suffix
}
