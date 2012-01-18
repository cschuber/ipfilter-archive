/*
 * Copyright (C) 2011 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include <sys/types.h>

#include "ip_compat.h"
#include "ip_fil.h"
#ifdef RDX_DEBUG
# include <arpa/inet.h>
# include <stddef.h>
# include <stdlib.h>
# include <stdio.h>
#endif
#include "netinet/radix_ipf.h"

#define	ADF_OFF	offsetof(addrfamily_t, adf_addr)
#define	ADF_OFF_BITS	(ADF_OFF << 3)

static void buildnodes(addrfamily_t *, addrfamily_t *, ipf_rdx_node_t n[2]);
static int count_mask_bits(addrfamily_t *, u_32_t **);
static ipf_rdx_node_t *ipf_rx_find_addr(ipf_rdx_node_t *, u_32_t *);
static ipf_rdx_node_t *ipf_rx_insert(ipf_rdx_head_t *, ipf_rdx_node_t n[2],
				     int *);
static ipf_rdx_node_t *ipf_rx_match(ipf_rdx_head_t *, addrfamily_t *);
static void ipf_rx_attach_mask(ipf_rdx_node_t *, ipf_rdx_mask_t *);

/*
 * Foreword.
 * ---------
 * The code in this file has been written to target using the addrfamily_t
 * data structure to house the address information and no other. Thus there
 * are certain aspects of thise code (such as offsets to the address itself)
 * that are hard coded here whilst they might be more variable elsewhere.
 * Similarly, this code enforces no maximum key length as that's implied by
 * all keys needing to be stored in addrfamily_t.
 */

/* ------------------------------------------------------------------------ */
/* Function:    count_mask_bits                                             */
/* Returns:     number of consecutive bits starting at "mask".              */
/*                                                                          */
/* Count the number of bits set in the address section of addrfamily_t and  */
/* return both that number and a pointer to the last word with a bit set if */
/* lastp is not NULL. The bit count is performed using network byte order   */
/* as the guide for which bit is the most significant bit.                  */
/* ------------------------------------------------------------------------ */
static int
count_mask_bits(mask, lastp)
	addrfamily_t *mask;
	u_32_t **lastp;
{
	u_32_t *mp = (u_32_t *)&mask->adf_addr;
	u_32_t m;
	int count = 0;
	int mlen;

	mlen = mask->adf_len - offsetof(addrfamily_t, adf_addr);
	for (mlen = mask->adf_len; mlen > 0; mlen -= 4, mp++) {
		if ((m = ntohl(*mp)) == 0)
			break;
		if (lastp != NULL)
			*lastp = mp;
		for (; m & 0x80000000; m <<= 1)
			count++;
	}

	return count;
}


/* ------------------------------------------------------------------------ */
/* Function:    buildnodes                                                  */
/* Returns:     Nil                                                         */
/* Parameters:  addr(I)  - network address for this radix node              */
/*              mask(I)  - netmask associated with the above address        */
/*              nodes(O) - pair of ipf_rdx_node_t's to initialise with data */
/*                         associated with addr and mask.                   */
/*                                                                          */
/* Initialise the fields in a pair of radix tree nodes according to the     */
/* data supplied in the paramters "addr" and "mask". It is expected that    */
/* "mask" will contain a consecutive string of bits set. Masks with gaps in */
/* the middle are not handled by this implementation.                       */
/* ------------------------------------------------------------------------ */
static void
buildnodes(addr, mask, nodes)
	addrfamily_t *addr, *mask;
	ipf_rdx_node_t nodes[2];
{
	u_32_t maskbits;
	u_32_t lastbits;
	u_32_t lastmask;
	u_32_t *last;
	int masklen;

	last = NULL;
	maskbits = count_mask_bits(mask, &last);
	if (last == NULL) {
		masklen = 0;
		lastmask = 0;
	} else {
		masklen = last - (u_32_t *)mask;
		lastmask = *last;
	}
	lastbits = maskbits & 0x1f;

	bzero(&nodes[0], sizeof(ipf_rdx_node_t) * 2);
	nodes[0].maskbitcount = maskbits;
	nodes[0].index = -1 - (ADF_OFF_BITS + maskbits);
	nodes[0].addrkey = (u_32_t *)addr;
	nodes[0].maskkey = (u_32_t *)mask;
	nodes[0].addroff = nodes[0].addrkey + masklen;
	nodes[0].maskoff = nodes[0].maskkey + masklen;
	nodes[0].parent = &nodes[1];
	nodes[0].offset = masklen;
	nodes[0].lastmask = lastmask;
	nodes[1].offset = masklen;
	nodes[1].left = &nodes[0];
	nodes[1].maskbitcount = maskbits;
#ifdef RDX_DEBUG
	(void) strcpy(nodes[0].name, "_BUILD.0");
	(void) strcpy(nodes[1].name, "_BUILD.1");
#endif
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_find_addr                                            */
/* Returns:     ipf_rdx_node_t * - pointer to a node in the radix tree.     */
/* Parameters:  tree(I)  - pointer to first right node in tree to search    */
/*              addr(I)  - pointer to address to match                      */
/*                                                                          */
/* Walk the radix tree given by "tree", looking for a leaf node that is a   */
/* match for the address given by "addr".                                   */
/* ------------------------------------------------------------------------ */
static ipf_rdx_node_t *
ipf_rx_find_addr(tree, addr)
	ipf_rdx_node_t *tree;
	u_32_t *addr;
{
	ipf_rdx_node_t *cur;

	for (cur = tree; cur->index >= 0;) {
		if (cur->bitmask & addr[cur->offset]) {
			cur = cur->right;
		} else {
			cur = cur->left;
		}
	}

	return (cur);
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_match                                                */
/* Returns:     ipf_rdx_node_t * - NULL on error, else pointer to the node  */
/*                                 added to the tree.                       */
/* Paramters:   head(I)  - pointer to tree head to search                   */
/*              addr(I)  - pointer to address to find                       */
/*                                                                          */
/* Search the radix tree for the best match to the address pointed to by    */
/* "addr" and return a pointer to that node. This search will not match the */
/* address information stored in either of the root leaves as neither of    */
/* them are considered to be part of the tree of data being stored.         */
/* ------------------------------------------------------------------------ */
static ipf_rdx_node_t *
ipf_rx_match(head, addr)
	ipf_rdx_head_t *head;
	addrfamily_t *addr;
{
	ipf_rdx_mask_t *masknode;
	ipf_rdx_node_t *prev;
	ipf_rdx_node_t *node;
	ipf_rdx_node_t *cur;
	u_32_t *data;
	u_32_t *mask;
	u_32_t *key;
	u_32_t *end;
	int len;
	int i;

	len = addr->adf_len;
	end = (u_32_t *)((u_char *)addr + len);
	node = ipf_rx_find_addr(head->root, (u_32_t *)addr);

	/*
	 * Search the dupkey list for a potential match.
	 */
	for (cur = node; (cur != NULL) && (cur->root == 0); cur = cur->dupkey) {
		data = cur[0].addroff;
		mask = cur[0].maskoff;
		key = (u_32_t *)addr + cur[0].offset;
		for (; key < end; data++, key++, mask++)
			if ((*key & *mask) != *data)
				break;
		if ((end == key) && (cur->root == 0))
			return (cur);	/* Equal keys */
	}
	prev = node->parent;
	key = (u_32_t *)addr;

	for (node = prev; node->root == 0; node = node->parent) {
		/*
		 * We know that the node hasn't matched so therefore only
		 * the entries in the mask list are searched, not the top
		 * node nor the dupkey list.
		 */
		masknode = node->masks;
		for (; masknode != NULL; masknode = masknode->next) {
			if (masknode->maskbitcount > node->maskbitcount)
				continue;
			cur = masknode->node;
			for (i = ADF_OFF >> 2; i <= node->offset; i++) {
				if ((key[i] & masknode->mask[i]) ==
				    cur->addrkey[i])
					return (cur);
			}
		}
	}

	return NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_lookup                                               */
/* Returns:     ipf_rdx_node_t * - NULL on error, else pointer to the node  */
/*                                 added to the tree.                       */
/* Paramters:   head(I)  - pointer to tree head to search                   */
/*              addr(I)  - address part of the key to match                 */
/*              mask(I)  - netmask part of the key to match                 */
/*                                                                          */
/* ipf_rx_lookup searches for an exact match on (addr,mask). The intention  */
/* is to see if a given key is in the tree, not to see if a route exists.   */
/* ------------------------------------------------------------------------ */
ipf_rdx_node_t *
ipf_rx_lookup(head, addr, mask)
	ipf_rdx_head_t *head;
	addrfamily_t *addr, *mask;
{
	ipf_rdx_node_t *found;
	ipf_rdx_node_t *node;
	u_32_t *akey;
	int count;

	found = ipf_rx_find_addr(head->root, (u_32_t *)addr);
	if (found->root == 1)
		return NULL;

	/*
	 * It is possible to find a matching address in the tree but for the
	 * netmask to not match. If the netmask does not match and there is
	 * no list of alternatives present at dupkey, return a failure.
	 */
	count = count_mask_bits(mask, NULL);
	if (count != found->maskbitcount && found->dupkey == NULL)
		return (NULL);

	akey = (u_32_t *)addr;
	if ((found->addrkey[found->offset] & found->maskkey[found->offset]) !=
	    akey[found->offset])
		return NULL;

	if (found->dupkey != NULL) {
		node = found;
		while (node != NULL && node->maskbitcount != count)
			node = node->dupkey;
		if (node == NULL)
			return (NULL);
		found = node;
	}
	return found;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_attach_mask                                          */
/* Returns:     Nil                                                         */
/* Parameters:  node(I)  - pointer to a radix tree node                     */
/*              mask(I)  - pointer to mask structure to add                 */
/*                                                                          */
/* Add the netmask to the given node in an ordering where the most specific */
/* netmask is at the top of the list.                                       */
/* ------------------------------------------------------------------------ */
static void
ipf_rx_attach_mask(node, mask)
	ipf_rdx_node_t *node;
	ipf_rdx_mask_t *mask;
{
	ipf_rdx_mask_t **pm;
	ipf_rdx_mask_t *m;

	for (pm = &node->masks; (m = *pm) != NULL; pm = &m->next)
		if (m->maskbitcount < mask->maskbitcount)
			break;
	mask->next = *pm;
	*pm = mask;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_insert                                               */
/* Returns:     ipf_rdx_node_t * - NULL on error, else pointer to the node  */
/*                                 added to the tree.                       */
/* Paramters:   head(I)  - pointer to tree head to add nodes to             */
/*              nodes(I) - pointer to radix nodes to be added               */
/*              dup(O)   - set to 1 if node is a duplicate, else 0.         */
/*                                                                          */
/* Add the new radix tree entry that owns nodes[] to the tree given by head.*/
/* If there is already a matching key in the table, "dup" will be set to 1  */
/* and the existing node pointer returned if there is a complete key match. */
/* A complete key match is a matching of all key data that is presented by  */
/* by the netmask.                                                          */
/* ------------------------------------------------------------------------ */
static ipf_rdx_node_t *
ipf_rx_insert(head, nodes, dup)
	ipf_rdx_head_t *head;
	ipf_rdx_node_t nodes[2];
	int *dup;
{
	ipf_rdx_mask_t **pmask;
	ipf_rdx_node_t *node;
	ipf_rdx_node_t *prev;
	ipf_rdx_mask_t *mask;
	ipf_rdx_node_t *cur;
	u_32_t nodemask;
	u_32_t *addr;
	u_32_t *data;
	int nodebits;
	u_32_t *key;
	u_32_t *end;
	u_32_t bits;
	int nodekey;
	int nodeoff;
	int nlen;
	int len;

	addr = nodes[0].addrkey;

	node = ipf_rx_find_addr(head->root, addr);
	len = ((addrfamily_t *)addr)->adf_len;
	key = (u_32_t *)&((addrfamily_t *)addr)->adf_addr;
	data= (u_32_t *)&((addrfamily_t *)node->addrkey)->adf_addr;
	end = (u_32_t *)((u_char *)addr + len);
	for (; key < end; data++, key++)
		if (*key != *data)
			break;
	if (end == data) {
		*dup = 1;
		return (node);	/* Equal keys */
	}
	*dup = 0;

	bits = (ntohl(*data) ^ ntohl(*key));
	for (nlen = 0; bits != 0; nlen++) {
		if ((bits & 0x80000000) != 0)
			break;
		bits <<= 1;
	}
	nlen += ADF_OFF_BITS;
	nodes[1].index = nlen;
	nodes[1].bitmask = htonl(0x80000000 >> (nlen & 0x1f));

	/*
	 * Walk through the tree and look for the correct place to attach
	 * this node. ipf_rx_fin_addr is not used here because the place
	 * to attach this node may be an internal node (same key, different
	 * netmask.) Additionally, the depth of the search is forcibly limited
	 * here to not exceed the netmask, so that a short netmask will be
	 * added higher up the tree even if there are lower branches.
	 */
	cur = head->root;
	key = nodes[0].addrkey;
	do {
		prev = cur;
		if (key[cur->offset] & cur->bitmask) {
			cur = cur->right;
		} else {
			cur = cur->left;
		}
	} while (nlen > (unsigned)cur->index);

	if ((key[prev->offset] & prev->bitmask) == 0) {
		prev->left = &nodes[1];
	} else {
		prev->right = &nodes[1];
	}
	cur->parent = &nodes[1];
	nodes[1].parent = prev;
	if ((key[cur->offset] & nodes[1].bitmask) == 0) {
		nodes[1].right = cur;
	} else {
		nodes[1].right = &nodes[0];
		nodes[1].left = cur;
	}

	nodeoff = nodes[0].offset;
	nodekey = nodes[0].addrkey[nodeoff];
	nodemask = nodes[0].lastmask;
	nodebits = nodes[0].maskbitcount;
	prev = NULL;
	/*
	 * Find the node up the tree with the largest pattern that still
	 * matches the node being inserted to see if this mask can be
	 * moved there.
	 */
	for (cur = nodes[1].parent; cur->root == 0; cur = cur->parent) {
		if (cur->maskbitcount <= nodebits)
			break;
		if (((cur - 1)->addrkey[nodeoff] & nodemask) != nodekey)
			break;
		prev = cur;
	}

	KMALLOC(mask, ipf_rdx_mask_t *);
	if (mask == NULL)
		return NULL;
	bzero(mask, sizeof(*mask));
	mask->next = NULL;
	mask->node = &nodes[0];
	mask->maskbitcount = nodebits;
	mask->mask = nodes[0].maskkey;
	nodes[0].mymask = mask;

	if (prev != NULL) {
		ipf_rdx_mask_t *m;

		for (pmask = &prev->masks; (m = *pmask) != NULL;
		     pmask = &m->next) {
			if (m->maskbitcount < nodebits)
				break;
		}
	} else {
		/*
		 * No higher up nodes qualify, so attach mask locally.
		 */
		pmask = &nodes[0].masks;
	}
	mask->next = *pmask;
	*pmask = mask;

	/*
	 * Search the mask list on each child to see if there are any masks
	 * there that can be moved up to this newly inserted node.
	 */
	cur = nodes[1].right;
	if (cur->root == 0) {
		for (pmask = &cur->masks; (mask = *pmask) != NULL; ) {
			if (mask->maskbitcount < nodebits) {
				*pmask = mask->next;
				ipf_rx_attach_mask(&nodes[0], mask);
			} else {
				pmask = &mask->next;
			}
		}
	}
	cur = nodes[1].left;
	if (cur->root == 0 && cur != &nodes[0]) {
		for (pmask = &cur->masks; (mask = *pmask) != NULL; ) {
			if (mask->maskbitcount < nodebits) {
				*pmask = mask->next;
				ipf_rx_attach_mask(&nodes[0], mask);
			} else {
				pmask = &mask->next;
			}
		}
	}
	return (&nodes[0]);
}

/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_addroute                                             */
/* Returns:     ipf_rdx_node_t * - NULL on error, else pointer to the node  */
/*                                 added to the tree.                       */
/* Paramters:   head(I)  - pointer to tree head to search                   */
/*              addr(I)  - address portion of "route" to add                */
/*              mask(I)  - netmask portion of "route" to add                */
/*              nodes(I) - radix tree data nodes inside allocate structure  */
/*                                                                          */
/* Attempt to add a node to the radix tree. The key for the node is the     */
/* (addr,mask). No memory allocation for the radix nodes themselves is      */
/* performed here, the data structure that this radix node is being used to */
/* find is expected to house the node data itself however the call to       */
/* ipf_rx_insert() will attempt to allocate memory in order for netmask to  */
/* be promoted further up the tree.                                         */
/* In this case, the ip_pool_node_t structure from ip_pool.h contains both  */
/* the key material (addr,mask) and the radix tree nodes[].                 */
/*                                                                          */
/* The mechanics of inserting the node into the tree is handled by the      */
/* function ipf_rx_insert() above. Here, the code deals with the case       */
/* where the data to be inserted is a duplicate.                            */
/* ------------------------------------------------------------------------ */
ipf_rdx_node_t *
ipf_rx_addroute(head, addr, mask, nodes)
	ipf_rdx_head_t *head;
	addrfamily_t *addr, *mask;
	ipf_rdx_node_t *nodes;
{
	ipf_rdx_node_t *node;
	ipf_rdx_node_t *prev;
	ipf_rdx_node_t *x;
	int dup;

	buildnodes(addr, mask, nodes);
	x = ipf_rx_insert(head, nodes, &dup);
	if (x == NULL)
		return NULL;

	if (dup == 1) {
		node = &nodes[0];
		prev = NULL;
		/*
		 * The duplicate list is kept sorted with the longest
		 * mask at the top, meaning that the most specific entry
		 * in the listis found first. This list thus allows for
		 * duplicates such as 128.128.0.0/32 and 128.128.0.0/16.
		 */
		while ((x != NULL) && (x->maskbitcount > node->maskbitcount)) {
			prev = x;
			x = x->dupkey;
		}

		/*
		 * Is it a complete duplicate? If so, return NULL and
		 * fail the insert. Otherwise, insert it into the list
		 * of netmasks active for this key.
		 */
		if ((x != NULL) && (x->maskbitcount == node->maskbitcount))
			return (NULL);

		if (prev != NULL) {
			nodes[0].dupkey = x;
			prev->dupkey = &nodes[0];
			nodes[0].parent = prev;
			if (x != NULL)
				x->parent = &nodes[0];
		} else {
			nodes[0].dupkey = x->dupkey;
			prev = x->parent;
			nodes[0].parent = prev;
			x->parent = &nodes[0];
			if (prev->left == x)
				prev->left = &nodes[0];
			else
				prev->right = &nodes[0];
		}
	}

	return &nodes[0];
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_delete                                               */
/* Returns:     ipf_rdx_node_t * - NULL on error, else node removed from    */
/*                                 the tree.                                */
/* Paramters:   head(I)  - pointer to tree head to search                   */
/*              addr(I)  - pointer to the address part of the key           */
/*              mask(I)  - pointer to the netmask part of the key           */
/*                                                                          */
/* Search for an entry in the radix tree that is an exact match for (addr,  */
/* mask) and remove it if it exists. In the case where (addr,mask) is a not */
/* a unique key, the tree structure itself is not changed - only the list   */
/* of duplicate keys.                                                       */
/* ------------------------------------------------------------------------ */
ipf_rdx_node_t *
ipf_rx_delete(head, addr, mask)
        ipf_rdx_head_t *head;
        addrfamily_t *addr, *mask;
{
	ipf_rdx_mask_t **pmask;
	ipf_rdx_node_t *parent;
	ipf_rdx_node_t *found;
	ipf_rdx_node_t *prev;
	ipf_rdx_node_t *node;
	ipf_rdx_node_t *cur;
	ipf_rdx_mask_t *m;
	int count;

	found = ipf_rx_find_addr(head->root, (u_32_t *)addr);
	if (found->root == 1)
		return NULL;
	count = count_mask_bits(mask, NULL);
	parent = found->parent;
	if (found->dupkey != NULL) {
		node = found;
		while (node != NULL && node->maskbitcount != count)
			node = node->dupkey;
		if (node == NULL)
			return (NULL);
		if (node != found) {
			/*
			 * Remove from the dupkey list. Here, "parent" is
			 * the previous node on the list (rather than tree)
			 * and "dupkey" is the next node on the list.
			 */
			parent = node->parent;
			parent->dupkey = node->dupkey;
			node->dupkey->parent = parent;
		} else {
			/*
			 * 
			 * When removing the top node of the dupkey list,
			 * the pointers at the top of the list that point
			 * to other tree nodes need to be preserved and
			 * any children must have their parent updated.
			 */
			node = node->dupkey;
			node->parent = found->parent;
			node->right = found->right;
			node->left = found->left;
			found->right->parent = node;
			found->left->parent = node;
			if (parent->left == found)
				parent->left = node;
			else
				parent->right= node;
		}
	} else {
		if (count != found->maskbitcount)
			return (NULL);
		/*
		 * Remove the node from the tree and reconnect the subtree
		 * below.
		 */
		/*
		 * If there is a tree to the left, look for something to
		 * attach in place of "found".
		 */
		prev = found + 1;
		if (parent != found + 1) {
			cur = parent->parent;
			if (cur->right == parent) {
				if (prev->right != parent)
					prev->right->parent = parent;
				if (cur != prev) {
					if (parent->left != parent - 1) {
						cur->right = parent->left;
						parent->left->parent = cur;
					} else {
						cur->right = parent - 1;
						(parent - 1)->parent = cur;
					}
				}

				if (cur != prev) {
					if (parent->left == found)
						(parent - 1)->parent = parent;
				}
				if (prev->parent->right == prev) {
					prev->parent->right = parent;
				} else {
					prev->parent->left = parent;
				}
				if (prev->left->index > 0) {
					prev->left->parent = parent;
					if (parent->left != found)
						parent->right = parent->left;
					parent->left = prev->left;
				}
				if (prev->right->index > 0) {
					if (prev->right != parent) {
						prev->right->parent = parent;
						parent->right = prev->right;
					} else if (parent->left->index < 0) {
						parent->right = parent - 1;
					}
				} else if (parent->right == found) {
					parent->right = parent - 1;
				}
				parent->parent = prev->parent;
			} else {
				parent->left = parent - 1;

				if (cur->parent->right == cur)
					cur->parent->right = parent;
				else
					cur->parent->left = parent;
				cur->right->parent = parent;
				parent->parent = cur->parent;
				parent->right = cur->right;
			}
			parent->bitmask = prev->bitmask;
			parent->offset = prev->offset;
			parent->index = prev->index;
		} else {
			/*
			 * We found an edge node.
			 */
			cur = parent->parent;
			if (cur->left == parent) {
				if (parent->left == found) {
					cur->left = parent->right;
					parent->right->parent = cur;
				} else {
					cur->left = parent->left;
					parent->left->parent = cur;
				}
			} else {
				if (parent->right != found) {
					cur->right = parent->right;
					parent->right->parent = cur;
				} else {
					cur->right = parent->left;
					prev->left->parent = cur;
				}
			}
		}
	}

	/*
	 * Remove mask associated with this node.
	 */
	for (cur = parent; cur->root == 0; cur = cur->parent) {
		ipf_rdx_mask_t **pm;

		if (cur->maskbitcount <= found->maskbitcount)
			break;
		if (((cur - 1)->addrkey[found->offset] & found->bitmask) !=
		    found->addrkey[found->offset])
			break;
		for (pm = &cur->masks; (m = *pm) != NULL; )
			if (m->node == cur) {
				*pm = m->next;
				break;
			} else {
				pm = &m->next;
			}
	}

	/*
	 * Masks that have been brought up to this node from below need to
	 * be sent back down.
	 */
	for (pmask = &parent->masks; (m = *pmask) != NULL; ) {
		*pmask = m->next;
		cur = m->node;
		if (cur == found)
			continue;
		if (found->addrkey[cur->offset] & cur->lastmask) {
			ipf_rx_attach_mask(parent->right, m);
		} else if (parent->left != found) {
			ipf_rx_attach_mask(parent->left, m);
		}
	}

	return (found);
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_walktree                                             */
/* Returns:     Nil                                                         */
/* Paramters:   head(I)   - pointer to tree head to search                  */
/*              walker(I) - function to call for each node in the tree      */
/*              arg(I)    - parameter to pass to walker, in addition to the */
/*                          node pointer                                    */
/*                                                                          */
/* A standard tree walking function except that it is iterative, rather     */
/* than recursive and tracks the next node in case the "walker" function    */
/* should happen to delete and free the current node. It thus goes without  */
/* saying that the "walker" function is not permitted to cause any change   */
/* in the validity of the data found at either the left or right child.     */
/* ------------------------------------------------------------------------ */
void
ipf_rx_walktree(head, walker, arg)
	ipf_rdx_head_t *head;
	radix_walk_func_t walker;
	void *arg;
{
	ipf_rdx_node_t *next;
	ipf_rdx_node_t *node = head->root;
	ipf_rdx_node_t *base;

	while (node->index >= 0)
		node = node->left;

	for (;;) {
		base = node;
		while ((node->parent->right == node) && (node->root == 0))
			node = node->parent;

		for (node = node->parent->right; node->index >= 0; )
			node = node->left;
		next = node;

		for (node = base; node != NULL; node = base) {
			base = node->dupkey;
			if (node->root == 0)
				walker(node, arg);
		}
		node = next;
		if (node->root)
			return;
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_rx_inithead                                             */
/* Returns:     int       - 0 = success, else failure                       */
/* Paramters:   softr(I)  - pointer to radix context                        */
/*              headp(O)  - location for where to store allocated tree head */
/*                                                                          */
/* This function allocates and initialises a radix tree head structure.     */
/* As a traditional radix tree, node 0 is used as the "0" sentinel and node */
/* "2" is used as the all ones sentinel, leaving node "1" as the root from  */
/* which the tree is hung with node "0" on its left and node "2" to the     */
/* right. The context, "softr", is used here to provide a common source of  */
/* the zeroes and ones data rather than have one per head.                  */
/* ------------------------------------------------------------------------ */
int
ipf_rx_inithead(softr, headp)
	radix_softc_t *softr;
	ipf_rdx_head_t **headp;
{
	ipf_rdx_head_t *ptr;
	ipf_rdx_node_t *node;

	KMALLOC(ptr, ipf_rdx_head_t *);
	*headp = ptr;
	if (ptr == NULL)
		return -1;
	bzero(ptr, sizeof(*ptr));
	node = ptr->nodes;
	ptr->root = node + 1;
	node[0].index = -1 - ADF_OFF_BITS;
	node[1].index = ADF_OFF_BITS;
	node[2].index = -1 - ADF_OFF_BITS;
	node[0].parent = node + 1;
	node[1].parent = node + 1;
	node[2].parent = node + 1;
	node[1].bitmask = htonl(0x80000000);
	node[0].root = 1;
	node[1].root = 1;
	node[2].root = 1;
	node[0].offset = ADF_OFF_BITS >> 5;
	node[1].offset = ADF_OFF_BITS >> 5;
	node[2].offset = ADF_OFF_BITS >> 5;
	node[1].left = &node[0];
	node[1].right = &node[2];
	node[0].addrkey = (u_32_t *)softr->zeros;
	node[2].addrkey = (u_32_t *)softr->ones;
#ifdef RDX_DEBUG
	(void) strcpy(node[0].name, "0_ROOT");
	(void) strcpy(node[1].name, "1_ROOT");
	(void) strcpy(node[2].name, "2_ROOT");
#endif

	ptr->addaddr = ipf_rx_addroute;
	ptr->deladdr = ipf_rx_delete;
	ptr->lookup = ipf_rx_lookup;
	ptr->matchaddr = ipf_rx_match;
	ptr->walktree = ipf_rx_walktree;
	return 0;
}

void
ipf_rx_freehead(head)
	ipf_rdx_head_t *head;
{
	KFREE(head);
}

void *
ipf_rx_create()
{
	radix_softc_t *softr;

	KMALLOC(softr, radix_softc_t *);
	if (softr == NULL)
		return NULL;
	bzero((char *)softr, sizeof(*softr));

	return softr;
}

int
ipf_rx_init(ctx)
	void *ctx;
{
	radix_softc_t *softr = ctx;

	KMALLOCS(softr->zeros, u_char *, 3 * sizeof(addrfamily_t));
	if (softr->zeros == NULL)
		return (-1);
	softr->ones = softr->zeros + sizeof(addrfamily_t);
	memset(softr->ones, 0xff, sizeof(addrfamily_t));
	return (0);
}

void
ipf_rx_destroy(ctx)
	void *ctx;
{
	radix_softc_t *softr = ctx;

	KFREES(softr->zeros, 3 * sizeof(addrfamily_t));
	KFREE(softr);
}

/* ====================================================================== */

#ifdef RDX_DEBUG

#define	NAME(x)	((x)->index < 0 ? (x)->name : (x)->name)
#define	GNAME(y)	((y) == NULL ? "NULL" : NAME(y))

typedef struct myst {
	struct ipf_rdx_node nodes[2];
	addrfamily_t	dst;
	addrfamily_t	mask;
	struct myst	*next;
	int		printed;
} myst_t;

static int nodecount = 0;
myst_t *myst_top = NULL;

void add_addr(ipf_rdx_head_t *, int , int);
void checktree(ipf_rdx_head_t *);
void delete_addr(ipf_rdx_head_t *rnh, int item);
void dumptree(ipf_rdx_head_t *rnh);
void nodeprinter(ipf_rdx_node_t *, void *);
void printroots(ipf_rdx_head_t *);
void random_add(ipf_rdx_head_t *);
void random_delete(ipf_rdx_head_t *);
void test_addr(ipf_rdx_head_t *rnh, int pref, u_32_t addr, int);

static void
ipf_rx_freenode(node, arg)
	ipf_rdx_node_t *node;
	void *arg;
{
	ipf_rdx_head_t *head = arg;
	ipf_rdx_node_t *rv;
	myst_t *stp;

	stp = (myst_t *)node;
	rv = ipf_rx_delete(head, &stp->dst, &stp->mask);
	if (rv != NULL) {
		free(rv);
	}
}

void
nodeprinter(node, arg)
	ipf_rdx_node_t *node;
	void *arg;
{
	myst_t *stp = (myst_t *)node;

	printf("Node %-9.9s L %-9.9s R %-9.9s P %9.9s/%-9.9s %s/%d\n",
		node[0].name,
		GNAME(node[1].left), GNAME(node[1].right),
		GNAME(node[0].parent), GNAME(node[1].parent),
		inet_ntoa(stp->dst.adf_addr.in4), node[0].maskbitcount);
}

void
printnode(stp)
	myst_t *stp;
{
	ipf_rdx_node_t *node = &stp->nodes[0];

	if (stp->nodes[0].index > 0)
		stp = (myst_t *)&stp->nodes[-1];

	printf("Node %-9.9s ", node[0].name);
	printf("L %-9.9s ", GNAME(node[1].left));
	printf("R %-9.9s ", GNAME(node[1].right));
	printf("P %9.9s", GNAME(node[0].parent));
	printf("/%-9.9s ", GNAME(node[1].parent));
	printf("%s\n", inet_ntoa(stp->dst.adf_addr.in4));
}

char *ttable[22][3] = {
	{ "127.192.0.0",	"255.255.255.0",	"d" },
	{ "127.128.0.0",	"255.255.255.0",	"d" },
	{ "127.96.0.0",		"255.255.255.0",	"d" },
	{ "127.80.0.0",		"255.255.255.0",	"d" },
	{ "127.72.0.0",		"255.255.255.0",	"d" },
	{ "127.64.0.0",		"255.255.255.0",	"d" },
	{ "127.56.0.0",		"255.255.255.0",	"d" },
	{ "127.48.0.0",		"255.255.255.0",	"d" },
	{ "127.40.0.0",		"255.255.255.0",	"d" },
	{ "127.32.0.0",		"255.255.255.0",	"d" },
	{ "127.24.0.0",		"255.255.255.0",	"d" },
	{ "127.16.0.0",		"255.255.255.0",	"d" },
	{ "127.8.0.0",		"255.255.255.0",	"d" },
	{ "124.0.0.0",		"255.0.0.0",		"d" },
	{ "125.0.0.0",		"255.0.0.0",		"d" },
	{ "126.0.0.0",		"255.0.0.0",		"d" },
	{ "127.0.0.0",		"255.0.0.0",		"d" },
	{ "10.0.0.0",		"255.0.0.0",		"d" },
	{ "128.250.0.0",	"255.255.0.0",		"d" },
	{ "192.168.0.0",	"255.255.0.0",		"d" },
	{ "192.168.1.0",	"255.255.255.0",	"d" },
	{ NULL, NULL }
};

char *mtable[15][1] = {
	{ "9.0.0.0" },
	{ "9.0.0.1" },
	{ "11.0.0.0" },
	{ "11.0.0.1" },
	{ "127.0.0.1" },
	{ "127.0.1.0" },
	{ "255.255.255.0" },
	{ "126.0.0.1" },
	{ "128.251.0.0" },
	{ "128.251.0.1" },
	{ "128.251.255.255" },
	{ "129.250.0.0" },
	{ "129.250.0.1" },
	{ "192.168.255.255" },
	{ NULL }
};

int forder[22] = {
	14, 13, 12,  5, 10,  3, 19,  7,  4, 20,  8,
	 2, 17,  9, 16, 11, 15,  1,  6, 18,  0, 21
};

void
printroots(rnh)
	ipf_rdx_head_t *rnh;
{
	printf("Root.0.%s b %3d p %-9.9s l %-9.9s r %-9.9s\n",
		GNAME(&rnh->nodes[0]),
		rnh->nodes[0].index, GNAME(rnh->nodes[0].parent),
		GNAME(rnh->nodes[0].left), GNAME(rnh->nodes[0].right));
	printf("Root.1.%s b %3d p %-9.9s l %-9.9s r %-9.9s\n",
		GNAME(&rnh->nodes[1]),
		rnh->nodes[1].index, GNAME(rnh->nodes[1].parent),
		GNAME(rnh->nodes[1].left), GNAME(rnh->nodes[1].right));
	printf("Root.2.%s b %3d p %-9.9s l %-9.9s r %-9.9s\n",
		GNAME(&rnh->nodes[2]),
		rnh->nodes[2].index, GNAME(rnh->nodes[2].parent),
		GNAME(rnh->nodes[2].left), GNAME(rnh->nodes[2].right));
}

int
main(int argc, char *argv[])
{
	ipf_rdx_head_t *rnh;
	radix_softc_t *ctx;
	int j;
	int i;

	rnh = NULL;

	ctx = ipf_rx_create();
	ipf_rx_init(ctx);
	ipf_rx_inithead(ctx, &rnh);

	printf("=== ADD-0 ===\n");
	for (i = 0; ttable[i][0] != NULL; i++) {
		add_addr(rnh, i, i);
		checktree(rnh);
	}
	ipf_rx_walktree(rnh, nodeprinter, NULL);
	printf("=== DELETE-0 ===\n");
	for (i = 0; ttable[i][0] != NULL; i++) {
		delete_addr(rnh, i);
		ipf_rx_walktree(rnh, nodeprinter, NULL);
	}
	printf("=== ADD-1 ===\n");
	for (i = 0; ttable[i][0] != NULL; i++) {
		add_addr(rnh, i, forder[i]);
		checktree(rnh);
	}
	printroots(rnh);
	dumptree(rnh);
	ipf_rx_walktree(rnh, nodeprinter, NULL);
	printf("=== TEST-1 ===\n");
	for (i = 0; ttable[i][0] != NULL; i++) {
		test_addr(rnh, i, inet_addr(ttable[i][0]), -1);
	}

	printf("=== TEST-2 ===\n");
	for (i = 0; mtable[i][0] != NULL; i++) {
		test_addr(rnh, i, inet_addr(mtable[i][0]), -1);
	}
	printf("=== DELETE-1 ===\n");
	for (i = 0; ttable[i][0] != NULL; i++) {
		if (ttable[i][2][0] != 'd')
			continue;
		delete_addr(rnh, i);
		for (j = 0; ttable[j][0] != NULL; j++) {
			test_addr(rnh, i, inet_addr(ttable[j][0]), 3);
		}
		ipf_rx_walktree(rnh, nodeprinter, NULL);
	}

	printroots(rnh);
	dumptree(rnh);

	printf("=== ADD-2 ===\n");
	random_add(rnh);
	checktree(rnh);
	printroots(rnh);
	dumptree(rnh);
	ipf_rx_walktree(rnh, nodeprinter, NULL);
	printf("=== DELETE-2 ===\n");
	random_delete(rnh);
	checktree(rnh);
	printroots(rnh);
	dumptree(rnh);

	ipf_rx_walktree(rnh, ipf_rx_freenode, rnh);

	return 0;
}

void
dumptree(rnh)
	ipf_rdx_head_t *rnh;
{
	myst_t *stp;

	printf("VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV\n");
	for (stp = myst_top; stp; stp = stp->next)
		printnode(stp);
	printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
}

void
test_addr(rnh, pref, addr, limit)
	ipf_rdx_head_t *rnh;
	int pref;
	u_32_t addr;
{
	static int extras[14] = { 0, -1, 1, 3, 5, 8, 9,
				  15, 16, 19, 255, 256, 65535, 65536
	};
	ipf_rdx_node_t *rn;
	addrfamily_t af;
	myst_t *stp;
	int i;

	memset(&af, 0, sizeof(af));
	af.adf_len = sizeof(af.adf_addr);
	if (limit < 0 || limit > 14)
		limit = 14;

	for (i = 0; i < limit; i++) {
		af.adf_addr.in4.s_addr = htonl(ntohl(addr) + extras[i]);
		printf("%d.%d.LOOKUP(%s)", pref, i, inet_ntoa(af.adf_addr.in4));
		rn = ipf_rx_match(rnh, &af);
		stp = (myst_t *)rn;
		printf(" = %s (%s/%d)\n", GNAME(rn),
			rn ? inet_ntoa(stp->dst.adf_addr.in4) : "NULL",
			rn ? rn->maskbitcount : 0);
	}
}

void
delete_addr(rnh, item)
	ipf_rdx_head_t *rnh;
	int item;
{
	ipf_rdx_node_t *rn;
	addrfamily_t mask;
	addrfamily_t af;
	myst_t **pstp;
	myst_t *stp;

	memset(&af, 0, sizeof(af));
	memset(&mask, 0, sizeof(mask));
	af.adf_family = AF_INET;
	af.adf_len = sizeof(af.adf_addr);
	mask.adf_len = sizeof(mask.adf_addr);

	af.adf_addr.in4.s_addr = inet_addr(ttable[item][0]);
	mask.adf_addr.in4.s_addr = inet_addr(ttable[item][1]);
	printf("DELETE(%s)\n", inet_ntoa(af.adf_addr.in4));
	rn = ipf_rx_delete(rnh, &af, &mask);
	printf("%d.delete(%s) = %s\n", item,
	       inet_ntoa(af.adf_addr.in4), GNAME(rn));

	for (pstp = &myst_top; (stp = *pstp) != NULL; pstp = &stp->next)
		if (stp == (myst_t *)rn)
			break;
	*pstp = stp->next;
	free(stp);
	nodecount--;
	checktree(rnh);
}

void
add_addr(rnh, n, item)
	ipf_rdx_head_t *rnh;
	int n, item;
{
	ipf_rdx_node_t *rn;
	myst_t *stp;

	stp = calloc(1, sizeof(*stp));
	rn = (ipf_rdx_node_t *)stp;
	stp->dst.adf_family = AF_INET;
	stp->dst.adf_len = sizeof(stp->dst.adf_addr);
	stp->dst.adf_addr.in4.s_addr = inet_addr(ttable[item][0]);
	memset(&stp->mask, 0xff, offsetof(addrfamily_t, adf_addr));
	stp->mask.adf_len = sizeof(stp->mask.adf_addr);
	stp->mask.adf_addr.in4.s_addr = inet_addr(ttable[item][1]);
	stp->next = myst_top;
	myst_top = stp;
	(void) sprintf(rn[0].name, "_BORN.0");
	(void) sprintf(rn[1].name, "_BORN.1");
	rn = ipf_rx_addroute(rnh, &stp->dst, &stp->mask, stp->nodes);
	(void) sprintf(rn[0].name, "%d_NODE.0", item);
	(void) sprintf(rn[1].name, "%d_NODE.1", item);
	printf("ADD %d/%d %s/%s\n", n, item, rn[0].name, rn[1].name);
	nodecount++;
	checktree(rnh);
}

void
checktree(ipf_rdx_head_t *head)
{
	myst_t *s1;
	ipf_rdx_node_t *rn;

	if (nodecount <= 1)
		return;

	for (s1 = myst_top; s1 != NULL; s1 = s1->next) {
		int fault = 0;
		rn = &s1->nodes[1];
		if (rn->right->parent != rn)
			fault |= 1;
		if (rn->left->parent != rn)
			fault |= 2;
		if (rn->parent->left != rn && rn->parent->right != rn)
			fault |= 4;
		if (fault != 0) {
			printf("FAULT %#x %s\n", fault, rn->name);
			printroots(head);
			dumptree(head);
			ipf_rx_walktree(head, nodeprinter, NULL);
		}
	}
}

int *
randomize(int *pnitems)
{
	int *order;
	int nitems;
	int choice;
	int j;
	int i;

	nitems = sizeof(ttable) / sizeof(ttable[0]);
	*pnitems = nitems;
	order = calloc(nitems, sizeof(*order));
	srandom(getpid() * time(NULL));
	memset(order, 0xff, nitems * sizeof(*order));
	order[21] = 21;
	for (i = 0; i < nitems - 1; i++) {
		do {
			choice = rand() % (nitems - 1);
			for (j = 0; j < nitems; j++)
				if (order[j] == choice)
					break;
		} while (j != nitems);
		order[i] = choice;
	}

	return order;
}

void
random_add(rnh)
	ipf_rdx_head_t *rnh;
{
	int *order;
	int nitems;
	int i;

	order = randomize(&nitems);

	for (i = 0; i < nitems - 1; i++) {
		add_addr(rnh, i, order[i]);
		checktree(rnh);
	}
}

void
random_delete(rnh)
	ipf_rdx_head_t *rnh;
{
	int *order;
	int nitems;
	int i;

	order = randomize(&nitems);

	for (i = 0; i < nitems - 1; i++) {
		delete_addr(rnh, i);
		checktree(rnh);
	}
}
#endif /* RDX_DEBUG */
