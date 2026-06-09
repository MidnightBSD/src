: ${CPUFREQ:=cpufreq}

require_cpu_freq()
{

	if ! sysctl -n "dev.cpu.$1.freq" >/dev/null 2>&1; then
		atf_skip "dev.cpu.$1.freq is unavailable"
	fi
}

require_cpu_freqs()
{
	ncpu=$(sysctl -n hw.ncpu) || atf_skip "hw.ncpu is unavailable"
	cpu=0
	while [ "${cpu}" -lt "${ncpu}" ]; do
		require_cpu_freq "${cpu}"
		cpu=$((cpu + 1))
	done
}

atf_test_case default_cpu0
default_cpu0_head()
{
	atf_set "descr" "Default to reporting CPU 0"
}
default_cpu0_body()
{

	require_cpu_freq 0
	atf_check -s exit:0 -e empty \
	    -o match:"^CPU 0 frequency: [0-9]+ MHz$" \
	    "${CPUFREQ}"
}

atf_test_case specific_cpu
specific_cpu_head()
{
	atf_set "descr" "Report the selected CPU"
}
specific_cpu_body()
{

	require_cpu_freq 1
	atf_check -s exit:0 -e empty \
	    -o match:"^CPU 1 frequency: [0-9]+ MHz$" \
	    "${CPUFREQ}" -c 1
}

atf_test_case average_cpu
average_cpu_head()
{
	atf_set "descr" "Report the average CPU frequency"
}
average_cpu_body()
{

	require_cpu_freqs
	atf_check -s exit:0 -e empty \
	    -o match:"^Average CPU frequency: [0-9]+ MHz$" \
	    "${CPUFREQ}" -m
}

atf_test_case verbose_specific_cpu
verbose_specific_cpu_head()
{
	atf_set "descr" "Report frequency levels for the selected CPU"
}
verbose_specific_cpu_body()
{

	require_cpu_freq 0
	if ! sysctl -n dev.cpu.0.freq_levels >/dev/null 2>&1; then
		atf_skip "dev.cpu.0.freq_levels is unavailable"
	fi
	atf_check -s exit:0 -e empty \
	    -o match:"^CPU 0 frequency: [0-9]+ MHz" \
	    -o match:"Possible frequencies: " \
	    "${CPUFREQ}" -c 0 -v
}

atf_test_case invalid_cpu
invalid_cpu_head()
{
	atf_set "descr" "Reject invalid CPU arguments"
}
invalid_cpu_body()
{

	atf_check -s exit:1 -o empty -e match:"cpu is invalid: bad" \
	    "${CPUFREQ}" -c bad
}

atf_test_case average_conflicts_with_cpu
average_conflicts_with_cpu_head()
{
	atf_set "descr" "Reject average and CPU selection together"
}
average_conflicts_with_cpu_body()
{

	atf_check -s exit:1 -o empty \
	    -e inline:"usage: cpufreq [-c cpu] [-m] [-v]\n" \
	    "${CPUFREQ}" -m -c 0
}

atf_test_case extra_operand
extra_operand_head()
{
	atf_set "descr" "Reject extra operands"
}
extra_operand_body()
{

	atf_check -s exit:1 -o empty \
	    -e inline:"usage: cpufreq [-c cpu] [-m] [-v]\n" \
	    "${CPUFREQ}" extra
}

atf_init_test_cases()
{

	atf_add_test_case default_cpu0
	atf_add_test_case specific_cpu
	atf_add_test_case average_cpu
	atf_add_test_case verbose_specific_cpu
	atf_add_test_case invalid_cpu
	atf_add_test_case average_conflicts_with_cpu
	atf_add_test_case extra_operand
}
