#!/bin/ksh

gen_ipf_conf() {
	generate_block_rules
	generate_test_hdr
	cat << __EOF__
pass in on ${SUT_NET0_IFP_NAME} proto tcp from any to ${RECEIVER_NET1_ADDR_V4} port != 5058
pass out on ${SUT_NET0_IFP_NAME} proto tcp from ${RECEIVER_NET1_ADDR_V4} port != 5058 to ${SENDER_NET0_ADDR_V4}
pass out on ${SUT_NET1_IFP_NAME} proto tcp from any to ${RECEIVER_NET1_ADDR_V4} port != 5058
pass in on ${SUT_NET1_IFP_NAME} proto tcp from ${RECEIVER_NET1_ADDR_V4} port != 5058 to ${SENDER_NET0_ADDR_V4}
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
	start_tcp_server ${RECEIVER_CTL_HOSTNAME} ${RECEIVER_NET1_ADDR_V4} 5059
	sleep 1
	tcp_test ${SENDER_CTL_HOSTNAME} ${RECEIVER_NET1_ADDR_V4} 5059 pass
	ret=$?
	stop_tcp_server ${RECEIVER_CTL_HOSTNAME} 1
	ret=$((ret + $?))
	return $ret;
}

do_tune() {
	return 0;
}

do_verify() {
	return 0;
}
