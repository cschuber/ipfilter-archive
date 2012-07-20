
SUT_CTL_HOSTNAME=s10u7-vbox; export SUT_CTL_HOSTNAME
SENDER_CTL_HOSTNAME=netbsd-vbox; export SENDER_CTL_HOSTNAME
RECEIVER_CTL_HOSTNAME=freebsd-vbox; export RECEIVER_CTL_HOSTNAME
SUT_CTL_IFP_NAME=e1000g1; export SUT_CTL_IFP_NAME
SENDER_CTL_IFP_NAME=wm1; export SENDER_CTL_IFP_NAME
RECEIVER_CTL_IFP_NAME=em1; export RECEIVER_CTL_IFP_NAME
SENDER_CTL_IFP_NAME=wm1; export SENDER_CTL_IFP_NAME
SENDER_NET0_IFP_NAME=wm2; export SENDER_NET0_IFP_NAME
SENDER_NET1_IFP_NAME=SETME; export SENDER_NET1_IFP_NAME
SUT_CTL_IFP_NAME=e1000g1; export SUT_CTL_IFP_NAME
SUT_NET0_IFP_NAME=e1000g2; export SUT_NET0_IFP_NAME
SUT_NET1_IFP_NAME=e1000g3; export SUT_NET1_IFP_NAME
SUT_NET0_ADDR_V4=192.168.100.2; export SUT_NET0_ADDR_V4
SUT_NET1_ADDR_V4=192.168.101.130; export SUT_NET1_ADDR_V4
SUT_NET0_ADDR_V6=192:168:100::2; export SUT_NET0_ADDR_V6
SUT_NET1_ADDR_V6=192:168:101::130; export SUT_NET1_ADDR_V6
RECEIVER_CTL_IFP_NAME=em1; export RECEIVER_CTL_IFP_NAME
RECEIVER_NET0_IFP_NAME=SETME; export RECEIVER_NET0_IFP_NAME
RECEIVER_NET1_IFP_NAME=em3; export RECEIVER_NET1_IFP_NAME
SENDER_NET0_ADDR_V4=192.168.100.3; export SENDER_NET0_ADDR_V4
SENDER_NET0_ADDR_V6=192:168:100::3; export SENDER_NET0_ADDR_V6
RECEIVER_NET1_ADDR_V4=192.168.101.131; export RECEIVER_NET1_ADDR_V4
RECEIVER_NET1_ADDR_V6=192:168:101::131; export RECEIVER_NET1_ADDR_V6
LOG0_FILE=/var/tmp/ipf_test/tmp/tcpdump.e1000g2; export LOG0_FILE
LOG1_FILE=/var/tmp/ipf_test/tmp/tcpdump.e1000g3; export LOG1_FILE
LOGS_FILE=/var/tmp/ipf_test/tmp/tcpdump.wm2; export LOGS_FILE
LOGR_FILE=/var/tmp/ipf_test/tmp/tcpdump.em3; export LOGR_FILE
RRCP=rcp; export RRCP
RRSH=rsh; export RRSH
BIN_IPF=/usr/sbin/ipf; export BIN_IPF
BIN_IPFSTAT=/usr/sbin/ipfstat; export BIN_IPFSTAT
BIN_IPNAT=/usr/sbin/ipnat; export BIN_IPNAT
BIN_IPPOOL=/usr/sbin/ippool; export BIN_IPPOOL
PING_TRIES=3; export PING_TRIES
PING_SIZE_LARGE=2000; export PING_SIZE_LARGE
PING_SIZE_SMALL=200; export PING_SIZE_SMALL
TEST_IPF_CONF=ipf_test.conf; export TEST_IPF_CONF
TEST_IPNAT_CONF=ipnat_test.conf; export TEST_IPNAT_CONF
TEST_IPPOOL_CONF=ippool_test.conf; export TEST_IPPOOL_CONF
IPF_LOG_DIR=/var/tmp/ipf_test/log/2012_07_18_2204; export IPF_LOG_DIR
IPF_LOG_FILE=/var/tmp/ipf_test/log/2012_07_18_2204/_ftp.2363.log; export IPF_LOG_FILE
IPF_TMP_DIR=/var/tmp/ipf_test/tmp; export IPF_TMP_DIR
IPF_BIN_DIR=/var/tmp/ipf_test/bin; export IPF_BIN_DIR
IPF_LIB_DIR=/var/tmp/ipf_test/lib; export IPF_LIB_DIR
IPF_VAR_DIR=/var/tmp/ipf_test; export IPF_VAR_DIR
TCP_TIMEOUT=6; export TCP_TIMEOUT
FTP_PATH=/pub/test_data.txt; export FTP_PATH
RCMD_USER=root; export RCMD_USER
SUT_NET0_ADDR_V4_A1=192.168.100.18; export SUT_NET0_ADDR_V4_A1
SUT_NET0_ADDR_V4_A2=192.168.100.34; export SUT_NET0_ADDR_V4_A2
SUT_NET0_ADDR_V4_A3=192.168.100.50; export SUT_NET0_ADDR_V4_A3
SUT_NET0_ADDR_V4_A4=192.168.100.66; export SUT_NET0_ADDR_V4_A4
SUT_NET0_ADDR_V4_A5=192.168.100.82; export SUT_NET0_ADDR_V4_A5
SUT_NET0_ADDR_V4_A6=192.168.100.98; export SUT_NET0_ADDR_V4_A6
SUT_NET0_ADDR_V4_A7=192.168.100.114; export SUT_NET0_ADDR_V4_A7
SUT_NET0_ADDR_V6_A1=192:168:100::18; export SUT_NET0_ADDR_V6_A1
SUT_NET0_ADDR_V6_A2=192:168:100::34; export SUT_NET0_ADDR_V6_A2
SUT_NET0_ADDR_V6_A3=192:168:100::50; export SUT_NET0_ADDR_V6_A3
SUT_NET0_ADDR_V6_A4=192:168:100::66; export SUT_NET0_ADDR_V6_A4
SUT_NET0_ADDR_V6_A5=192:168:100::82; export SUT_NET0_ADDR_V6_A5
SUT_NET0_ADDR_V6_A6=192:168:100::98; export SUT_NET0_ADDR_V6_A6
SUT_NET0_ADDR_V6_A7=192:168:100::114; export SUT_NET0_ADDR_V6_A7
SUT_NET1_ADDR_V4_A1=192.168.101.146; export SUT_NET1_ADDR_V4_A1
SUT_NET1_ADDR_V4_A2=192.168.101.162; export SUT_NET1_ADDR_V4_A2
SUT_NET1_ADDR_V4_A3=192.168.101.178; export SUT_NET1_ADDR_V4_A3
SUT_NET1_ADDR_V4_A4=192.168.101.194; export SUT_NET1_ADDR_V4_A4
SUT_NET1_ADDR_V4_A5=192.168.101.210; export SUT_NET1_ADDR_V4_A5
SUT_NET1_ADDR_V4_A6=192.168.101.226; export SUT_NET1_ADDR_V4_A6
SUT_NET1_ADDR_V4_A7=192.168.101.242; export SUT_NET1_ADDR_V4_A7
SUT_NET1_ADDR_V6_A1=192:168:101::146; export SUT_NET1_ADDR_V6_A1
SUT_NET1_ADDR_V6_A2=192:168:101::162; export SUT_NET1_ADDR_V6_A2
SUT_NET1_ADDR_V6_A3=192:168:101::178; export SUT_NET1_ADDR_V6_A3
SUT_NET1_ADDR_V6_A4=192:168:101::194; export SUT_NET1_ADDR_V6_A4
SUT_NET1_ADDR_V6_A5=192:168:101::210; export SUT_NET1_ADDR_V6_A5
SUT_NET1_ADDR_V6_A6=192:168:101::226; export SUT_NET1_ADDR_V6_A6
SUT_NET1_ADDR_V6_A7=192:168:101::242; export SUT_NET1_ADDR_V6_A7
SENDER_NET0_ADDR_V4_A1=192.168.100.19; export SENDER_NET0_ADDR_V4_A1
SENDER_NET0_ADDR_V4_A2=192.168.100.35; export SENDER_NET0_ADDR_V4_A2
SENDER_NET0_ADDR_V4_A3=192.168.100.51; export SENDER_NET0_ADDR_V4_A3
SENDER_NET0_ADDR_V4_A4=192.168.100.67; export SENDER_NET0_ADDR_V4_A4
SENDER_NET0_ADDR_V4_A5=192.168.100.83; export SENDER_NET0_ADDR_V4_A5
SENDER_NET0_ADDR_V4_A6=192.168.100.99; export SENDER_NET0_ADDR_V4_A6
SENDER_NET0_ADDR_V4_A7=192.168.100.115; export SENDER_NET0_ADDR_V4_A7
SENDER_NET0_ADDR_V6_A1=192:168:100::19; export SENDER_NET0_ADDR_V6_A1
SENDER_NET0_ADDR_V6_A2=192:168:100::35; export SENDER_NET0_ADDR_V6_A2
SENDER_NET0_ADDR_V6_A3=192:168:100::51; export SENDER_NET0_ADDR_V6_A3
SENDER_NET0_ADDR_V6_A4=192:168:100::67; export SENDER_NET0_ADDR_V6_A4
SENDER_NET0_ADDR_V6_A5=192:168:100::83; export SENDER_NET0_ADDR_V6_A5
SENDER_NET0_ADDR_V6_A6=192:168:100::99; export SENDER_NET0_ADDR_V6_A6
SENDER_NET0_ADDR_V6_A7=192:168:100::115; export SENDER_NET0_ADDR_V6_A7
RECEIVER_NET1_ADDR_V4_A1=192.168.101.147; export RECEIVER_NET1_ADDR_V4_A1
RECEIVER_NET1_ADDR_V4_A2=192.168.101.163; export RECEIVER_NET1_ADDR_V4_A2
RECEIVER_NET1_ADDR_V4_A3=192.168.101.179; export RECEIVER_NET1_ADDR_V4_A3
RECEIVER_NET1_ADDR_V4_A4=192.168.101.195; export RECEIVER_NET1_ADDR_V4_A4
RECEIVER_NET1_ADDR_V4_A5=192.168.101.211; export RECEIVER_NET1_ADDR_V4_A5
RECEIVER_NET1_ADDR_V4_A6=192.168.101.227; export RECEIVER_NET1_ADDR_V4_A6
RECEIVER_NET1_ADDR_V4_A7=192.168.101.243; export RECEIVER_NET1_ADDR_V4_A7
RECEIVER_NET1_ADDR_V6_A1=192:168:101::147; export RECEIVER_NET1_ADDR_V6_A1
RECEIVER_NET1_ADDR_V6_A2=192:168:101::163; export RECEIVER_NET1_ADDR_V6_A2
RECEIVER_NET1_ADDR_V6_A3=192:168:101::179; export RECEIVER_NET1_ADDR_V6_A3
RECEIVER_NET1_ADDR_V6_A4=192:168:101::195; export RECEIVER_NET1_ADDR_V6_A4
RECEIVER_NET1_ADDR_V6_A5=192:168:101::211; export RECEIVER_NET1_ADDR_V6_A5
RECEIVER_NET1_ADDR_V6_A6=192:168:101::227; export RECEIVER_NET1_ADDR_V6_A6
RECEIVER_NET1_ADDR_V6_A7=192:168:101::243; export RECEIVER_NET1_ADDR_V6_A7
NET0_NET_V4=192.168.100; export NET0_NET_V4
NET0_NETMASK_V4=255.255.255.0; export NET0_NETMASK_V4
NET0_NET_V6=192:168:100; export NET0_NET_V6
NET0_NETMASK_V6=48; export NET0_NETMASK_V6
NET1_NET_V4=192.168.101; export NET1_NET_V4
NET1_NETMASK_V4=255.255.255.0; export NET1_NETMASK_V4
NET1_NET_V6=192:168:101; export NET1_NET_V6
NET1_NETMASK_V6=48; export NET1_NETMASK_V6
NET0_FAKE_ADDR_V4=1.1.1.1; export NET0_FAKE_ADDR_V4
NET0_FAKE_NET_V4=1.1.1.0; export NET0_FAKE_NET_V4
NET0_FAKE_NETMASK_V4=255.255.255.0; export NET0_FAKE_NETMASK_V4
NET1_FAKE_ADDR_V4=2.2.2.2; export NET1_FAKE_ADDR_V4
NET1_FAKE_NET_V4=2.2.2.0; export NET1_FAKE_NET_V4
NET1_FAKE_NETMASK_V4=255.255.255.0; export NET1_FAKE_NETMASK_V4
NET0_FAKE_ADDR_V6=1:1:1::1; export NET0_FAKE_ADDR_V6
NET0_FAKE_NET_V6=1:1:1::0; export NET0_FAKE_NET_V6
NET0_FAKE_NETMASK_V6=48; export NET0_FAKE_NETMASK_V6
NET1_FAKE_ADDR_V6=2:2:2::2; export NET1_FAKE_ADDR_V6
NET1_FAKE_NET_V6=2:2:2::0; export NET1_FAKE_NET_V6
NET1_FAKE_NETMASK_V6=48; export NET1_FAKE_NETMASK_V6
SENDER_TUNNEL_ADDR_V4=10.0.0.3; export SENDER_TUNNEL_ADDR_V4
SENDER_TUNNEL_ADDR_V6=10::3; export SENDER_TUNNEL_ADDR_V6
RECEIVER_TUNNEL_ADDR_V4=10.0.0.131; export RECEIVER_TUNNEL_ADDR_V4
RECEIVER_TUNNEL_ADDR_V6=10::131; export RECEIVER_TUNNEL_ADDR_V6
TUNNEL_NET_V4=10.0.0; export TUNNEL_NET_V4
TUNNEL_NET_V6=10; export TUNNEL_NET_V6
