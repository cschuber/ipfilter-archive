no_base_ruleset=1
capture_net0=0
capture_net1=0
capture_ipmon=0
capture_sender=0
capture_receiver=0
preserve_net0=0
preserve_net1=0
preserve_ipmon=0
preserve_sender=0
preserve_receiver=0

gen_ipf_conf() {
	cat 1h/v4/1h_ipf_parse_014_v4.data
	return 0;
}

gen_ipnat_conf() {
	return 1;
}

gen_ippool_conf() {
	return 1;
}

do_test() {
	count_ipf_rules 2>&1
	active=$?
	if [[ $active = -1 ]] ; then
		print - "-- ERROR cannot count ipf rules"
		return 1
	fi
	if [[ $active = 0 ]] ; then
		print - "-- ERROR no rules loaded prior to flush"
		return 1
	fi
	${BIN_IPF} -Fa 2>&1
	count_ipf_rules 2>&1
	active=$?
	active=$((active))
	if [[ $active != 0 ]] ; then
		print - "-- ERROR $active rules remaining after flush"
		return 1;
	fi
	return 0
}

do_tune() {
	return 0;
}

do_verify() {
	return 0;
}
