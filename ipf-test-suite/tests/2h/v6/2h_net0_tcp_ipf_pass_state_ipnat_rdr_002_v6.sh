#!/bin/ksh
# ni8
capture_net1=0;
preserve_net1=0;

gen_ipf_conf() {
	generate_block_rules
	generate_test_hdr
	cat << __EOF__
pass in on ${SUT_NET0_IFP_NAME} inet6 proto tcp from any to any port = 5550 flags S keep state
__EOF__
	return 0;
}

gen_ipnat_conf() {
	cat <<__EOF__
rdr ${SUT_NET0_IFP_NAME} ${NET0_FAKE_ADDR_V6} port 1000:2000 -> ${SUT_NET0_ADDR_V6} port 5050 tcp
__EOF__
	return 0;
}

gen_ippool_conf() {
	return 1;
}

do_test() {
	start_tcp_server ${SUT_CTL_HOSTNAME} ${SUT_NET0_ADDR_V6} 5550
	sleep 3
	tcp_test ${SENDER_CTL_HOSTNAME} ${NET0_FAKE_ADDR_V6} 1500 pass
	ret=$?
	stop_tcp_server ${SUT_CTL_HOSTNAME} 1
	ret=$((ret + $?))
	return $ret;
}

do_tune() {
	return 0;
}

do_verify() {
	return 0;
}
