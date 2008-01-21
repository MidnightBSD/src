: check-env? ( addr len addr len -- flag )
	getenv
	dup -1 <> if
		compare 0= if
			true exit
		then
	else
		drop
		2drop
	then
	false
;

: is-vm? ( -- flag )
	s" QEMU  " s" hint.acpi.0.oem" check-env? if
		true exit
	then
	s" VBOX  " s" hint.acpi.0.oem" check-env? if
		true exit
	then
	s" VMware, Inc." s" smbios.system.maker" check-env? if
		true exit
	then
	s" Parallels Software International Inc." s" smbios.bios.vendor"
	    check-env? if
		true exit
	then
	false
;

: setup-vm
	is-vm? if
		s" 100" s" hz" setenv
	then
;
