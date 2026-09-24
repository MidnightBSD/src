#!/bin/sh

atf_test_case dmi_id cleanup
dmi_id_head()
{
	atf_set "descr" "Verify Linux-compatible DMI identification files"
	atf_set "require.user" "root"
}

dmi_id_body()
{
	atf_require_prog kenv
	atf_require_prog kldload
	atf_require_prog kldstat
	atf_require_prog mount
	atf_require_prog stat
	atf_require_prog umount

	if ! kldstat -q -m linsysfs; then
		atf_check -s exit:0 kldload linsysfs
		touch linsysfs_loaded
	fi

	mkdir mnt
	atf_check -s exit:0 mount -t linsysfs linsysfs "$(pwd)/mnt"
	touch linsysfs_mounted

	found=0
	while read -r name key; do
		if value=$(kenv "${key}" 2>/dev/null); then
			found=1
			atf_check -s exit:0 -o inline:"${value}\n" \
			    cat "mnt/class/dmi/id/${name}"
			atf_check -s exit:0 -o inline:"444\n" \
			    stat -f '%Lp' "mnt/class/dmi/id/${name}"
		elif [ -e "mnt/class/dmi/id/${name}" ]; then
			atf_fail "${name} exists without ${key}"
		fi
	done <<-EOF
	sys_vendor smbios.system.maker
	product_name smbios.system.product
	board_vendor smbios.planar.maker
	board_name smbios.planar.product
	bios_vendor smbios.bios.vendor
	bios_version smbios.bios.version
	chassis_vendor smbios.chassis.maker
	EOF

	if [ "${found}" -eq 0 ]; then
		atf_skip "no SMBIOS identification values are available"
	fi
}

dmi_id_cleanup()
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
	atf_add_test_case dmi_id
}
