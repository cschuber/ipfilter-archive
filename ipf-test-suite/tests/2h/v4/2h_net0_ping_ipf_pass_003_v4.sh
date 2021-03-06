capture_net1=0;
preserve_net1=0;

gen_ipf_conf() {
	generate_block_rules
	generate_test_hdr
	cat << __EOF__
pass in on ${SUT_NET0_IFP_NAME} proto icmp from any to ${SUT_NET0_ADDR_V4}/${NET0_NETMASK_V4}
pass out on ${SUT_NET0_IFP_NAME} proto icmp from ${SUT_NET0_ADDR_V4}/${NET0_NETMASK_V4} to any
__EOF__
	return 0;
}

gen_ipnat_conf() {
	return 1;
}

gen_ippool_conf() {
	return 1;
}

do_test() {
	ping_test ${SENDER_CTL_HOSTNAME} ${SUT_NET0_ADDR_V4} small pass
	return $?;
}

do_tune() {
	return 0;
}

do_verify() {
	return 2;
}
