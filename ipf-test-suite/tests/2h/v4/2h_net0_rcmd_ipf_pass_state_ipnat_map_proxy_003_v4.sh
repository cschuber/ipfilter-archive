#!/bin/ksh

capture_net1=0;
preserve_net1=0;

gen_ipf_conf() {
	generate_block_rules
	generate_test_hdr
        cat <<__EOF__
pass out quick on ${SUT_NET0_IFP_NAME} proto tcp from ${SUT_NET0_ADDR_V4} to ${SENDER_NET0_ADDR_V4} port = shell flags S keep state
__EOF__
	return 0;
}

gen_ipnat_conf() {
	cat <<__EOF__
map ${SUT_NET0_IFP_NAME} ${SUT_NET0_ADDR_V4} -> ${NET0_FAKE_ADDR_V4} proxy port 514 rcmd/tcp
__EOF__
	return 0;
}

gen_ippool_conf() {
	return 1;
}

do_test() {
	rcmd_test ${SUT_CTL_HOSTNAME} ${SENDER_NET0_ADDR_V4} pass
	return $?;
}

do_tune() {
	return 0;
}

do_verify() {
	count_logged_nat_sessions
	created=$?
	if [[ $created -lt 2 ]] ; then
		print - "-- ERROR created count less than 2 ($created)"
		return 1;
	fi
	print - "-- OK created $created NAT sessions"
	return 0;
}
