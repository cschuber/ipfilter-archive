gen_ipf_conf() {
	generate_block_rules
	generate_test_hdr
	cat << __EOF__
pass in on ${SUT_NET0_IFP_NAME} proto icmp from any to any icmp-type echo
pass out on ${SUT_NET0_IFP_NAME} proto icmp from any to any icmp-type echorep
pass out on ${SUT_NET1_IFP_NAME} proto icmp from any to any icmp-type echo
pass in on ${SUT_NET1_IFP_NAME} proto icmp from any to any icmp-type echorep
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
	ping_test ${SENDER_CTL_HOSTNAME} ${RECEIVER_NET1_ADDR_V4} small pass
	return $?;
}

do_tune() {
	return 0;
}

do_verify() {
	return 0;
}
