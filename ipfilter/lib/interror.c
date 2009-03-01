/*
 * Copyright (C) 2007 by Darren Reed
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"
#include <fcntl.h>
#include <sys/ioctl.h>

typedef	struct	{
	int	iee_number;
	char	*iee_text;
} ipf_error_entry_t;

static ipf_error_entry_t *find_error __P((int));

#define	IPF_NUM_ERRORS	428

/*
 * NO REUSE OF NUMBERS!
 *
 * IF YOU WANT TO ADD AN ERROR TO THIS TABLE, _ADD_ A NEW NUMBER.
 * DO _NOT_ USE AN EMPTY NUMBER OR FILL IN A GAP.
 */
static ipf_error_entry_t ipf_errors[IPF_NUM_ERRORS] = {
	{	1,	"auth table locked/full" },
	{	2,	"" },
	{	3,	"copyinptr received bad address" },
	{	4,	"copyoutptr received bad address" },
	{	5,	"" },
	{	6,	"cannot load a rule with FR_T_BUILTIN flag set" },
	{	7,	"internal rule without FR_T_BUILDINT flag set" },
	{	8,	"no data provided with filter rule" },
	{	9,	"invalid ioctl for rule" },
	{	10,	"rule protocol is not 4 or 6" },
	{	11,	"cannot find rule function" },
	{	12,	"cannot find rule group" },
	{	13,	"group in/out does not match rule in/out" },
	{	14,	"rule without in/out does not belong to a group" },
	{	15,	"cannot determine where to append rule" },
	{	16,	"malloc for rule data failed" },
	{	17,	"copyin for rule data failed" },
	{	18,	"" },
	{	19,	"zero data size for BPF rule" },
	{	20,	"BPF validation failed" },
	{	21,	"incorrect data size for IPF rule" },
	{	22,	"'keep state' rule included 'with oow'" },
	{	23,	"bad interface index with dynamic source address" },
	{	24,	"bad interface index with dynamic dest. address" },
	{	25,	"match array verif failed for filter rule" },
	{	26,	"bad filter rule type" },
	{	27,	"rule not found for zero'stats" },
	{	28,	"copyout failed for zero'ing stats" },
	{	29,	"rule not found for removing" },
	{	30,	"cannot remove internal rule" },
	{	31,	"rule in use" },
	{	32,	"rule already exists" },
	{	33,	"no memory for another rule" },
	{	34,	"could not find function" },
	{	35,	"copyout failed for resolving function name -> addr" },
	{	36,	"copyout failed for resolving function addr -> name" },
	{	37,	"function name/addr resolving search failed" },
	{	38,	"group map cannot find it's hash table" },
	{	39,	"group map hash-table in/out do not match rule" },
	{	40,	"bcopyout failed for SIOCIPFINTERROR" },
	{	41,	"" },
	{	42,	"ipfilter not enabled for NAT ioctl" },
	{	43,	"ipfilter not enabled for state ioctl" },
	{	44,	"ipfilter not enabled for auth ioctl" },
	{	45,	"ipfilter not enbaled for sync ioctl" },
	{	46,	"ipfilter not enabled for scan ioctl" },
	{	47,	"ipfilter not enabled for lookup ioctl" },
	{	48,	"unrecognised device minor number for ioctl" },
	{	49,	"unrecognised object type for copying in ipfobj" },
	{	50,	"mismatching object type for copying in ipfobj" },
	{	51,	"object size too small for copying in ipfobj" },
	{	52,	"object size mismatch for copying in ipfobj" },
	{	53,	"compat object size too small for copying in ipfobj" },
	{	54,	"compat object size mismatch for copying in ipfobj" },
	{	55,	"error doing copyin of data for in ipfobj" },
	{	56,	"unrecognised object type for size copy in ipfobj" },
	{	57,	"object size too small for size copy in ipfobj" },
	{	58,	"mismatching object type for size copy in ipfobj" },
	{	59,	"object size mismatch for size copy in ipfobj" },
	{	60,	"compat object size mismatch for size copy in ipfobj" },
	{	61,	"error doing size copyin of data for in ipfobj" },
	{	62,	"bad object type for size copy out ipfobj" },
	{	63,	"mismatching object type for size copy out ipfobj" },
	{	64,	"object size mismatch for size copy out ipfobj" },
	{	65,	"compat object size wrong for size copy out ipfobj" },
	{	66,	"error doing size copyout of data for out ipfobj" },
	{	67,	"unrecognised object type for copying out ipfobj" },
	{	68,	"mismatching object type for copying out ipfobj" },
	{	69,	"object size too small for copying out ipfobj" },
	{	70,	"object size mismatch for copying out ipfobj" },
	{	71,	"compat object size too small for copying out ipfobj" },
	{	72,	"compat object size mismatch for copying out ipfobj" },
	{	73,	"error doing copyout of data for out ipfobj" },
	{	74,	"attempt to add existing tunable name" },
	{	75,	"cannot find tunable name to delete" },
	{	76,	"internal data too big for next tunable" },
	{	77,	"could not find tunable" },
	{	78,	"tunable can only be changed when ipfilter disabled" },
	{	79,	"new tunable value outside accepted range" },
	{	80,	"ipftune called for unrecognised ioctl" },
	{	81,	"" },
	{	82,	"could not find token to delete" },
	{	83,	"" },
	{	84,	"attempt to get next rule when no more exist" },
	{	85,	"value for iri_inout outside accepted range" },
	{	86,	"value for iri_active outside accepted range" },
	{	87,	"value for iri_nrules is 0" },
	{	88,	"NULL pointer specified for where to copy rule to" },
	{	89,	"copyout of rule failed" },
	{	90,	"copyout of rule data section failed" },
	{	91,	"could not get token for rule iteration" },
	{	92,	"unrecognised generic iterator" },
	{	93,	"could not find token for generic iterator" },
	{	94,	"need write permissions to disable/enable ipfilter" },
	{	95,	"error copying in enable/disable value" },
	{	96,	"need write permissions to set ipf tunable" },
	{	97,	"need write permissions to set ipf flags" },
	{	98,	"error doing copyin of ipf flags" },
	{	99,	"error doing copyout of ipf flags" },
	{	100,	"need write permissions to add another rule" },
	{	101,	"need write permissions to insert another rule" },
	{	102,	"need write permissions to swap active rule set" },
	{	103,	"error copying out current active rule set" },
	{	104,	"need write permissions to zero ipf stats" },
	{	105,	"need write permissions to flush ipf v4 rules" },
	{	106,	"error copying out v4 flush results" },
	{	107,	"error copying in v4 flush command" },
	{	108,	"need write permissions to flush ipf v6 rules" },
	{	109,	"error copying out v6 flush results" },
	{	110,	"error copying in v6 flush command" },
	{	111,	"error copying in new lock state for ipfilter" },
	{	112,	"need write permissions to flush ipf logs" },
	{	113,	"error copying out results of log flush" },
	{	114,	"need write permissions to resync ipf" },
	{	115,	"unrecognised ipf ioctl" },
	{	116,	"error copying in match array" },
	{	117,	"match array type is not IPFOBJ_IPFEXPR" },
	{	118,	"bad size for match array" },
	{	119,	"cannot allocate memory for match aray" },
	{	120,	"error copying in match array" },
	{	121,	"error verifying contents of match array" },
	{	122,	"need write permissions to set ipf lock status" },
	{	123,	"error copying in data for function resolution" },
	{	124,	"error copying in ipfobj structure" },
	{	125,	"error copying in ipfobj structure" },
	{	126,	"error copying in ipfobj structure" },
	{	127,	"error copying in ipfobj structure" },
	{	128,	"no memory for filter rule comment" },
	{	129,	"error copying in filter rule comment" },
	{	130,	"error copying out filter rule comment" },
	{	131,	"no memory for new rule alloc buffer" },
	{	132,	"cannot find source lookup pool" },
	{	133,	"unknown source address type" },
	{	134,	"cannot find destination lookup pool" },
	{	135,	"unknown destination address type" },
	{	136,	"icmp head group name index incorrect" },
	{	137,	"group head name index incorrect" },
	{	138,	"group name index incorrect" },
	{	139,	"to interface name index incorrect" },
	{	140,	"dup-to interface name index incorrect" },
	{	141,	"reply-to interface name index incorrect" },
	{	142,	"could not initialise call now function" },
	{	143,	"could not initialise call function" },
/* -------------------------------------------------------------------------- */
	{	10001,	"could not find token for auth iterator" },
	{	10002,	"write permissions require to add/remove auth rule" },
	{	10003,	"need write permissions to set auth lock" },
	{	10004,	"error copying out results of auth flush" },
	{	10005,	"unknown auth ioctl" },
	{	10006,	"can only append or remove preauth rules" },
	{	10007,	"NULL pointers passed in for preauth remove" },
	{	10008,	"preauth rule not found to remove" },
	{	10009,	"could not malloc memory for preauth entry" },
	{	10010,	"unrecognised preauth rule ioctl command" },
	{	10011,	"iterator data supplied with NULL pointer" },
	{	10012,	"unknown auth iterator type" },
	{	10013,	"iterator error copying out auth data" },
	{	10014,	"sleep waiting for auth packet interrupted" },
	{	10015,	"bad index supplied in auth reply" },
	{	10016,	"error injecting outbound packet back into kernel" },
	{	10017,	"error injecting inbound packet back into kernel" },
	{	10018,	"could not attempt to inject packet back into kernel" },
	{	10019,	"packet id does not match" },
/* -------------------------------------------------------------------------- */
	{	20001,	"invalid frag token data pointer supplied" },
	{	20002,	"error copying out frag token data" },
/* -------------------------------------------------------------------------- */
	{	30001,	"incorrect object size to get hash table stats" },
	{	30002,	"could not malloc memory for new hash table" },
	{	30003,	"error coping in hash table structure" },
	{	30004,	"hash table already exists" },
	{	30005,	"mismach between new hash table and operation unit" },
	{	30006,	"could not malloc memory for hash table base" },
	{	30007,	"could not find hash table" },
	{	30008,	"mismatch between hash table and operation unit" },
	{	30009,	"could not find hash table for iterators next node" },
	{	30010,	"unknown iterator tpe" },
	{	30011,	"iterator error copying out hash table" },
	{	30012,	"iterator error copying out hash table entry" },
	{	30013,	"error copying out hash table statistics" },
	{	30014,	"table node delete structure wrong size" },
	{	30015,	"error copying in node to delete" },
	{	30016,	"table to delete node from does not exist" },
	{	30017,	"could not find table to remove node from" },
	{	30018,	"table node add structure wrong size" },
	{	30019,	"error copying in node to add" },
	{	30020,	"could not find table to add node to" },
	{	30021,	"node already exists in the table" },
	{	30022,	"could not find node to delete in table" },
	{	30023,	"uid mismatch on node to delete" },
/* -------------------------------------------------------------------------- */
	{	40001,	"invalid minor device numebr for log read" },
	{	40002,	"read size too small" },
	{	40003,	"interrupted waiting for log data to read" },
	{	40004,	"interrupted waiting for log data to read" },
	{	40005,	"read size too large" },
	{	40006,	"uiomove for read operation failed" },
/* -------------------------------------------------------------------------- */
	{	50001,	"unknown lookup ioctl" },
	{	50002,	"error copying in object data for add node" },
	{	50003,	"invalid unit for lookup add node" },
	{	50004,	"incorrect size for adding a pool node" },
	{	50005,	"error copying in pool node structure" },
	{	50006,	"mismatch in pool node address/mask families" },
	{	50007,	"could not find pool name" },
	{	50008,	"node already exists in pool" },
	{	50009,	"incorrect size for adding a hash node" },
	{	50010,	"error copying in hash node structure" },
	{	50011,	"could not find hash table name" },
	{	50012,	"unrecognised object type for lookup add node" },
	{	50013,	"invalid unit for lookup delete node" },
	{	50014,	"incorrect size for deleting a pool node" },
	{	50015,	"error copying in pool node structure" },
	{	50016,	"could not find pool name" },
	{	50017,	"could not find pool node" },
	{	50018,	"incorrect size for removing a hash node" },
	{	50019,	"error copying in hash node structure" },
	{	50020,	"could not find hash table name" },
	{	50021,	"unrecognised object type for lookup delete node" },
	{	50022,	"error copying in add table data" },
	{	50023,	"invalid unit for lookup add table" },
	{	50024,	"pool name already exists" },
	{	50025,	"hash table name already exists" },
	{	50026,	"unrecognised object type for lookup add table" },
	{	50027,	"error copying table data back out" },
	{	50028,	"error copying in remove table data" },
	{	50029,	"invalid unit for lookup remove table" },
	{	50030,	"unrecognised object type for lookup remove table" },
	{	50031,	"error copying in lookup stats structure" },
	{	50032,	"invalid unit for lookup stats" },
	{	50033,	"unrecognised object type for lookup stats" },
	{	50034,	"error copying in flush lookup data" },
	{	50035,	"invalid unit for lookup flush" },
	{	50036,	"incorrect table type for lookup flush" },
	{	50037,	"error copying out lookup flush results" },
	{	50038,	"invalid unit for lookup iterator" },
	{	50039,	"invalid unit for lookup iterator" },
	{	50040,	"could not find token for lookup iterator" },
	{	50041,	"unrecognised object type for lookup interator" },
	{	50042,	"error copying in lookup delete node operation" },
/* -------------------------------------------------------------------------- */
	{	60001,	"insufficient privilege for NAT write operation" },
	{	60002,	"need write permissions to flush NAT logs" },
	{	60003,	"need write permissions to turn NAT logging on/off" },
	{	60004,	"error copying out current NAT log setting" },
	{	60005,	"error copying out bytes waiting to be read in NAT \
log" },
	{	60006,	"need write permissions to add NAT rule" },
	{	60007,	"NAT rule already exists" },
	{	60008,	"could not allocate memory for NAT rule" },
	{	60009,	"need write permissions to remove NAT rule" },
	{	60010,	"NAT rule could not be found" },
	{	60011,	"could not find NAT entry for redirect lookup" },
	{	60012,	"need write permissions to flush NAT table" },
	{	60013,	"error copying in NAT flush command" },
	{	60014,	"need write permissions to do matching NAT flush" },
	{	60015,	"need write permissions to set NAT lock" },
	{	60016,	"need write permissions to add entry to NAT table" },
	{	60017,	"NAT not locked for size retrieval" },
	{	60018,	"NAT not locked for fetching NAT table entry" },
	{	60019,	"error copying in NAT token data for deletion" },
	{	60020,	"unknown NAT ioctl" },
	{	60021,	"cannot add encapsulation rule for TCP/UDP" },
	{	60022,	"resolving proxy name in NAT rule failed" },
	{	60023,	"only reply age specified in NAT rule" },
	{	60024,	"error doing copyin to determine NAT entry size" },
	{	60025,	"error copying out NAT size of 0" },
	{	60026,	"NAT entry not found" },
	{	60027,	"error doing copyout of NAT entry size" },
	{	60028,	"invalid data size for getting NAT entry" },
	{	60029,	"could not malloc temporary space for NAT entry" },
	{	60030,	"no NAT table entries present" },
	{	60031,	"NAT entry to get next from not found" },
	{	60032,	"not enough space for proxy structure" },
	{	60033,	"not enough space for private proxy data" },
	{	60034,	"NAT entry size is too large" },
	{	60035,	"could not malloc memory for NAT entry sratch space" },
	{	60036,	"" },
	{	60037,	"could not malloc memory for NAT entry" },
	{	60038,	"could not malloc memory for NAT entry rule" },
	{	60039,	"could not resolve NAT entry rule's proxy" },
	{	60040,	"cannot add outbound duplicate NAT entry" },
	{	60041,	"cannot add inbound duplicate NAT entry" },
	{	60042,	"cannot add NAT entry that is neither IN nor OUT" },
	{	60043,	"could not malloc memory for NAT proxy data" },
	{	60044,	"proxy data size too big" },
	{	60045,	"could not malloc proxy private data for NAT entry" },
	{	60046,	"could not malloc memory for new NAT filter rule" },
	{	60047,	"could not find existing filter rule for NAT entry" },
	{	60048,	"insertion into NAT table failed" },
	{	60049,	"iterator error copying out hostmap data" },
	{	60050,	"iterator error copying out NAT rule data" },
	{	60051,	"iterator error copying out NAT entry data" },
	{	60052,	"iterator data supplied with NULL pointer" },
	{	60053,	"unknown NAT iterator type" },
	{	60054,	"unknwon next address type" },
	{	60055,	"iterator suppled with unknown type for get-next" },
	{	60056,	"unknown lookup group for next address" },
	{	60057,	"error copying out NAT log flush results" },
	{	60058,	"bucket table type is incorrect" },
	{	60059,	"error copying out NAT bucket table" },
	{	60060,	"function not found for lookup" },
	{	60061,	"address family not supported with SIOCSTPUT" },
	{	60062,	"unknown timeout name" },
	{	60063,	"cannot allocate new inbound NAT entry table" },
	{	60064,	"cannot allocate new outbound NAT entry table" },
	{	60065,	"cannot allocate new inbound NAT bucketlen table" },
	{	60066,	"cannot allocate new outbound NAT bucketlen table" },
	{	60067,	"cannot allocate new NAT rules table" },
	{	60068,	"cannot allocate new NAT hostmap table" },
	{	60069,	"new source lookup type is not dstlist" },
	{	60070,	"cannot allocate NAT rule scratch space" },
	{	60071,	"new destination lookup type is not dstlist" },
/* -------------------------------------------------------------------------- */
	{	70001,	"incorrect object size to get pool stats" },
	{	70002,	"could not malloc memory for new pool node" },
	{	70003,	"invalid addresss length for new pool node" },
	{	70004,	"invalid mask length for new pool node" },
	{	70005,	"error adding node to pool" },
	{	70006,	"pool already exists" },
	{	70007,	"could not malloc memory for new pool" },
	{	70008,	"could not allocate radix tree for new pool" },
	{	70009,	"could not find pool" },
	{	70010,	"unknown pool name for iteration" },
	{	70011,	"unknown pool iterator" },
	{	70012,	"error copying out pool head" },
	{	70013,	"error copying out pool node" },
	{	70014,	"add node size incorrect" },
	{	70015,	"error copying in pool node" },
	{	70016,	"node address/mask family mismatch" },
	{	70017,	"cannot find pool for node" },
	{	70018,	"node entry already present in pool" },
	{	70019,	"delete node size incorrect" },
	{	70020,	"error copying in node to delete" },
	{	70021,	"cannot find pool to delete node from" },
	{	70022,	"cannot find node to delete in pool" },
	{	70023,	"pool name already exists" },
	{	70024,	"uid mismatch for node removal" },
	{	70025,	"stats device unit is invalid" },
	{	70026,	"error copying out statistics" },
/* -------------------------------------------------------------------------- */
	{	80001,	"could not find proxy" },
	{	80002,	"proxy does not support control operations" },
	{	80003,	"could not allocate data to hold proxy operation" },
	{	80004,	"unknown proxy ioctl" },
	{	80005,	"could not copyin proxy control structure" },
	{	80006,	"DNS proxy could not find rule to delete" },
	{	80007,	"DNS proxy found existing matching rule" },
	{	80008,	"DNS proxy could not allocate memory for new rule" },
	{	80009,	"DNS proxy unknown command request" },
/* -------------------------------------------------------------------------- */
	{	90001,	"could not malloc space for new scan structure" },
	{	90002,	"scan tag already exists" },
	{	90003,	"scan structure in use" },
	{	90004,	"could not find matching scan tag for filter rule" },
	{	90005,	"could not copyout scan statistics" },
/* -------------------------------------------------------------------------- */
	{	100001,	"cannot find matching state entry to remove" },
	{	100002,	"error copying in v4 state flush command" },
	{	100003,	"error copying out v4 state flush results" },
	{	100004,	"error copying in v6 state flush command" },
	{	100005,	"error copying out v6 state flush results" },
	{	100006,	"" },
	{	100007,	"" },
	{	100008,	"need write permissions to flush state log" },
	{	100009,	"erorr copyout results of flushing state log" },
	{	100010,	"need write permissions to turn state logging on/off" },
	{	100011,	"error copying in new state logging state" },
	{	100012,	"error copying out current state logging state" },
	{	100013,	"error copying out bytes waiting to be read in state \
log" },
	{	100014,	"need write permissions to set state lock" },
	{	100015,	"need write permissions to add entry to state table" },
	{	100016,	"state not locked for size retrieval" },
	{	100017,	"error copying out hash table bucket lengths" },
	{	100018,	"could not find token for state iterator" },
	{	100019,	"error copying in state token data for deletion" },
	{	100020,	"unknown state ioctl" },
	{	100021,	"no state table entries present" },
	{	100022,	"state entry to get next from not found" },
	{	100023,	"could not malloc memory for state entry" },
	{	100024,	"could not malloc memory for state entry rule" },
	{	100025,	"could not copy back state entry to user space" },
	{	100026,	"iterator data supplied with NULL pointer" },
	{	100027,	"iterator supplied with 0 item count" },
	{	100028,	"iterator type is incorrect" },
	{	100029,	"invalid state token data pointer supplied" },
	{	100030,	"error copying out next state entry" },
	{	100031,	"unrecognised table request" },
	{	100032,	"error copying out bucket length data" },
	{	100033,	"could not find existing filter rule for state entry" },
	{	100034,	"could not find timeout name" },
	{	100035, "could not allocate new state table" },
	{	100036, "could not allocate new state bucket length table" },
/* -------------------------------------------------------------------------- */
	{	110001,	"sync write header magic number is incorrect" },
	{	110002,	"sync write header protocol is incorrect" },
	{	110003,	"sync write header command is incorrect" },
	{	110004,	"sync write header table number is incorrect" },
	{	110005,	"data structure too small for sync write operation" },
	{	110006,	"zero length data with sync write header" },
	{	110007,	"insufficient data for sync write" },
	{	110008,	"bad sync read size" },
	{	110009,	"interrupted sync read (solaris)" },
	{	110010,	"interrupted sync read (hpux)" },
	{	110011,	"interrupted sync read (osf)" },
	{	110012,	"interrupted sync read" },
	{	110013,	"could not malloc memory for sync'd state" },
	{	110014,	"could not malloc memory for sync-state list item" },
	{	110015,	"sync update could not find state" },
	{	110016,	"unrecognised sync state command" },
	{	110017,	"could not malloc memory for new sync'd NAT entry" },
	{	110018,	"could not malloc memory for sync-NAT list item" },
	{	110019,	"sync update could not find NAT entry" },
	{	110020,	"unrecognised sync NAT command" },
	{	110021,	"ioctls are not handled with sync" },
/* -------------------------------------------------------------------------- */
	{	120001,	"null data pointer for iterator" },
	{	120002,	"unit outside of acceptable range" },
	{	120003,	"unknown iterator subtype" },
	{	120004,	"cannot find dest. list for iteration" },
	{	120005,	"error copying out destination iteration list" },
	{	120006,	"error copying out destination iteration node" },
	{	120007,	"wrong size for frdest_t structure" },
	{	120008,	"cannot allocate memory for new destination node" },
	{	120009,	"error copying in destination node to add" },
	{	120010,	"could not find destination list to add node to" },
	{	120011,	"error copying in destination node to remove" },
	{	120012,	"could not find dest. list to remove node from" },
	{	120013,	"destination list already exists" },
	{	120014,	"could not allocate new destination table" },
	{	120015,	"could not find destination list to remove" },
	{	120016,	"destination list cannot be removed - it is busy" },
	{	120017,	"error copying in names for destination" },
	{	120018,	"destination name is too long/short" },
	{	120019,	"unrecognised address family in destination" },
	{	120020,	"statistics not yet supported for dest. lists" },
	{	120021,	"error copying in new destination table" },
	{	120022,	"cannot allocate memory for node table" },
	{	120023,	"stats object size is incorrect for dest. lists" },
	{	120024,	"stats device unit is invalid for dest. lists" },
	{	120025,	"error copying out dest. list statistics" },
/* -------------------------------------------------------------------------- */
};


static ipf_error_entry_t *
find_error(errnum)
	int errnum;
{
	ipf_error_entry_t *ie;

	int l = -1, r = IPF_NUM_ERRORS + 1, step;
	step = (r - l) / 2;;

	while (step != 0) {
		ie = ipf_errors + l + step;
		if (ie->iee_number == errnum)
			return ie;
		step = l + step;
		if (ie->iee_number > errnum)
			r = step;
		else
			l = step;
		step = (r - l) / 2;;
	}

	return NULL;
}

char *
ipf_geterror(fd, func)
	int fd;
	ioctlfunc_t *func;
{
	static char text[80];
	ipf_error_entry_t *ie;
	int errnum;

	if ((*func)(fd, SIOCIPFINTERROR, &errnum) == 0) {

		ie = find_error(errnum);
		if (ie != NULL)
			return ie->iee_text;
		sprintf(text, "unknown error %d", errnum);
	} else {
		sprintf(text, "retrieving error number failed (%d)", errno);
	}
	return text;
}


char *
ipf_strerror(errnum)
	int errnum;
{
	static char text[80];
	ipf_error_entry_t *ie;


	ie = find_error(errnum);
	if (ie != NULL)
		return ie->iee_text;

	sprintf(text, "unknown error %d", errnum);
	return text;
}
