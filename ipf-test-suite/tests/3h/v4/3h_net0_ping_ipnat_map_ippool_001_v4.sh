#!/bin/ksh


gen_ipf_conf() {
	generate_pass_rules
	generate_test_hdr
	return 0;
}

gen_ipnat_conf() {
	cat <<__EOF__
map ${SUT_NET1_IFP_NAME} from pool/nattest to any -> ${NET1_FAKE_ADDR_V4}
__EOF__
	return 0;
}

gen_ippool_conf() {
	cat <<__EOF__
pool nat/tree (name nattest;) { ${SENDER_NET0_ADDR_V4}; };
__EOF__
	return 0;
}

do_test() {
	ping_test ${SENDER_CTL_HOSTNAME} ${RECEIVER_NET1_ADDR_V4} small pass
	return $?;
}

do_tune() {
	return 0;
}

do_verify() {
	verify_srcdst_1 ${NET1_FAKE_ADDR_V4} ${RECEIVER_NET1_ADDR_V4} echo
	count=$?
	count=$((count))
	if [[ $count -ne 6 ]] ; then
		print - "-- ERROR matching packets ${count} != 6"
		return 1
	fi
	print - "-- OK ${count} packets found"
	return 0;
}
