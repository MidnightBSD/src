#!/bin/sh

atf_test_case raw_dmi cleanup
raw_dmi_head()
{
	atf_set "descr" "Verify Linux-compatible raw SMBIOS table files"
	atf_set "require.user" "root"
}

raw_dmi_body()
{
	atf_require_prog awk
	atf_require_prog cmp
	atf_require_prog dd
	atf_require_prog kldload
	atf_require_prog kldstat
	atf_require_prog mount
	atf_require_prog od
	atf_require_prog stat
	atf_require_prog umount

	if ! kldstat -q -m linsysfs; then
		atf_check -s exit:0 kldload linsysfs
		touch linsysfs_loaded
	fi

	mkdir mnt
	atf_check -s exit:0 mount -t linsysfs linsysfs "$(pwd)/mnt"
	touch linsysfs_mounted

	eps="mnt/firmware/dmi/tables/smbios_entry_point"
	dmi="mnt/firmware/dmi/tables/DMI"
	if [ ! -e "${eps}" ] && [ ! -e "${dmi}" ]; then
		atf_skip "no raw SMBIOS tables are available"
	fi
	atf_check -s exit:0 -o inline:"400\n" stat -f '%Lp' "${eps}"
	atf_check -s exit:0 -o inline:"400\n" stat -f '%Lp' "${dmi}"
	atf_check -s exit:0 -o match:'^[1-9][0-9]*$' stat -f '%z' "${eps}"
	atf_check -s exit:0 -o match:'^[1-9][0-9]*$' stat -f '%z' "${dmi}"

	atf_check -s exit:0 -e ignore dd if="${eps}" of=eps bs=1
	atf_check -s exit:0 -e ignore dd if="${dmi}" of=dmi bs=1
	if [ ! -s eps ] || [ ! -s dmi ]; then
		atf_fail "raw SMBIOS files must not be empty"
	fi

	anchor=$(dd if=eps bs=1 count=4 2>/dev/null)
	if [ "${anchor}" != "_SM_" ]; then
		anchor=$(dd if=eps bs=1 count=5 2>/dev/null)
		if [ "${anchor}" != "_SM3_" ]; then
			atf_fail "invalid SMBIOS entry point signature: ${anchor}"
		fi
	fi
	checksum=$(od -An -tu1 eps | awk '
	    { for (i = 1; i <= NF; i++) sum += $i }
	    END { print sum % 256 }')
	if [ "${checksum}" -ne 0 ]; then
		atf_fail "invalid SMBIOS entry point checksum"
	fi

	atf_check -s exit:0 -e ignore dd if=eps of=expected bs=1 skip=4 count=8
	atf_check -s exit:0 -e ignore dd if="${eps}" of=actual bs=1 skip=4 count=8
	atf_check -s exit:0 cmp expected actual
}

raw_dmi_cleanup()
{
	if [ -f linsysfs_mounted ]; then
		umount "$(pwd)/mnt" >/dev/null 2>&1 || true
	fi
	if [ -f linsysfs_loaded ]; then
		kldunload linsysfs >/dev/null 2>&1 || true
	fi
}

atf_init_test_cases()
{
	atf_add_test_case raw_dmi
}
