/*
 * addr.cc
 *
 * address processing functions, including longest prefix match
 *
 * Copyright (c) 2021 Cisco Systems, Inc. All rights reserved.  License at
 * https://github.com/cisco/mercury/blob/master/LICENSE
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <locale.h>
#include <string>
#include "addr.h"
#include "archive.h"

#include "lctrie/lctrie.h"
#include "lctrie/lctrie_bgp.h"



subnet_data::~subnet_data() {
    if (ipv4_subnet_trie.root) {
        //
        // TBD: this free ought to be in lct_tree()
        //
        free(ipv4_subnet_trie.root);
    }
    lct_free(&ipv4_subnet_trie);
    if (ipv4_subnet_array) {
        free(ipv4_subnet_array);
    }
    if (prefix) {
        free(prefix);
    }
}

uint32_t subnet_data::get_asn_info(const char* dst_ip) const {
    uint32_t ipv4_addr;

    if (inet_pton(AF_INET, dst_ip, &ipv4_addr) != 1) {
        return 0;
    }

    lct_subnet_t *subnet = lct_find(&ipv4_subnet_trie, ntohl(ipv4_addr));
    if (subnet == NULL) {
        return 0;
    }
    if (subnet->info.type == IP_SUBNET_BGP) {
        return subnet->info.bgp.asn;
    }

    return 0;
}

int subnet_data::process_line(std::string &line_str) {

    // set the prefix[num] to the subnet and ASN found in line
    if (lct_subnet_set_from_string(&prefix[num], line_str.c_str()) != 0) {
        fprintf(stderr, "error: could not parse subnet string '%s'\n", line_str.c_str());
        return -1;  // failure
    }
    num++;
    return 0;       // success
}

void subnet_data::process_final() {

    // validate subnet prefixes against their netmasks
    // and sort the resulting array
    subnet_mask(prefix, num);
    qsort(prefix, num, sizeof(lct_subnet<ipv4_addr_t>), subnet_cmp<ipv4_addr_t>);

    // de-duplicate subnets and shrink the buffer down to its
    // actual size and split into prefixes and bases
    num -= subnet_dedup(prefix, num);
    lct_subnet_t *tmp = (lct_subnet_t *)realloc(prefix, num * sizeof(lct_subnet_t));
    if (tmp != NULL) {
        prefix = tmp;
    } else {
        return;  // TODO: leak check
    }

    // allocate a buffer for the IP stats
    lct_ip_stats_t *stats = (lct_ip_stats_t *) calloc(num, sizeof(lct_ip_stats_t));
    if (!stats) {
        return;  // TODO: leak check
    }

    // count which subnets are prefixes of other subnets
    subnet_prefix(prefix, stats, num);
    free(stats);

    // we're storing twice as many subnets as necessary for easy
    // iteration over the entire sorted subnet list.
    for (int i = 0; i < num; i++) {
        // quick error check on the optimized prefix indexes
        uint32_t prfx;
        prfx = prefix[i].prefix;
        if (prfx != IP_PREFIX_NIL && prefix[prfx].type == IP_PREFIX_FULL) {
            /* error: optimized subnet index points to a full prefix */
            return;  // TODO: leak check
        }
    }

    // actually build the trie and get the trie node count for statistics printing
    memset(&ipv4_subnet_trie, 0, sizeof(lct<ipv4_addr_t>));
    lct_build(&ipv4_subnet_trie, prefix, num);

    // set subnet array to actual value; after this, the subnet_data
    // object is ready for use
    //
    ipv4_subnet_array = prefix;
    prefix = nullptr;           // to avoid free(prefix)
}


// TBD: should these functions be retained, or rewritten?
//
#if 0
template <typename T>
int
read_prefix_table_from_archive(class encrypted_compressed_archive &archive, // const char *archive_name,
                               lct_subnet<T> prefix[],
                               size_t prefix_size) {

    (void)prefix_size;   // TBD: add defensive check
    int num = 0;

    // open the archive for reading
    // class compressed_archive archive{archive_name};
    const class archive_node *entry = archive.get_next_entry();
    if (entry == nullptr) {
        fprintf(stderr, "error: could not read archive\n");
        return -1;
    }

    // validate and parse each line of input
    while (entry != nullptr) {
        if (entry->is_regular_file()) {
            std::string line_str;

            std::string name = entry->get_name();
            if (name == "pyasn.db") {
                while (archive.getline(line_str)) {

                    // set the prefix[num] to the subnet and ASN found in line
                    if (lct_subnet_set_from_string(&prefix[num], line_str.c_str()) != 0) {
                        fprintf(stderr, "error: could not parse subnet string '%s'\n", line_str.c_str());
                        return -1;
                    }
                    num++;
                }
            }
        }
        entry = archive.get_next_entry();
    }

    return num;
}



/*
 * lct_init_from_file(lct, filename) initializes the lctrie lct by
 * reading data from the file filename.  On success, the location of
 * the subnet array allocated by this function is returned; on error,
 * NULL is returned, and the caller should use errno/perror to
 * determine the cause.
 */
lct_subnet_t *lct_init_from_file(lct<ipv4_addr_t> *trie,
                                 encrypted_compressed_archive &archive) { // char *filename) {
  int num = 0;
  uint32_t prefix;
  lct_subnet_t *p;
  lct_subnet_t *tmp = NULL;
  lct_ip_stats_t *stats = NULL;

  // we need this to get thousands separators
  setlocale(LC_NUMERIC, "");

  if (!(p = (lct_subnet_t *)calloc(sizeof(lct_subnet_t), BGP_MAX_ENTRIES))) {
      return NULL;  /* could not allocate subnet input buffer */
  }

  // start with the RFC 1918 and 3927 private and link local
  // subnets as a basis for any table set
  num += init_private_subnets(&p[num], BGP_MAX_ENTRIES);

  // fill up the rest of the array with reserved IP subnets
  num += init_special_subnets(&p[num], BGP_MAX_ENTRIES);

  // read in the ASN prefixes
  int rc;
#if 1
  if (0 > (rc = read_prefix_table_from_archive(archive, &p[num], BGP_MAX_ENTRIES - num))) {
      goto bail; /* could not read prefix file */
  }
#else
  if (0 > (rc = read_prefix_table(filename, &p[num], BGP_MAX_ENTRIES - num))) {
      goto bail; /* could not read prefix file */
  }
#endif
  num += rc;

  // validate subnet prefixes against their netmasks
  // and sort the resulting array
  subnet_mask(p, num);
  qsort(p, num, sizeof(lct_subnet<ipv4_addr_t>), subnet_cmp<ipv4_addr_t>);

  // de-duplicate subnets and shrink the buffer down to its
  // actual size and split into prefixes and bases
  num -= subnet_dedup(p, num);
  tmp = (lct_subnet_t *)realloc(p, num * sizeof(lct_subnet_t));
  if (tmp != NULL) {
      p = tmp;
  } else {
      goto bail;
  }

  // allocate a buffer for the IP stats
  stats = (lct_ip_stats_t *) calloc(num, sizeof(lct_ip_stats_t));
  if (!stats) {
      goto bail; /* "could not allocate prefix statistics buffer */
  }

  // count which subnets are prefixes of other subnets
  subnet_prefix(p, stats, num);
  free(stats);

  // we're storing twice as many subnets as necessary for easy
  // iteration over the entire sorted subnet list.
  for (int i = 0; i < num; i++) {
    // quick error check on the optimized prefix indexes
    prefix = p[i].prefix;
    if (prefix != IP_PREFIX_NIL && p[prefix].type == IP_PREFIX_FULL) {
        goto bail; /* error: optimized subnet index points to a full prefix */
    }
  }

  // actually build the trie and get the trie node count for statistics printing
  memset(trie, 0, sizeof(lct<ipv4_addr_t>));
  lct_build(trie, p, num);

  return p;

 bail:   /* handle errors by freeing memory as needed */

  free(p);
  return NULL;
}


//////

subnet_data::subnet_data(encrypted_compressed_archive &archive) {
    ipv4_subnet_array = lct_init_from_file(&ipv4_subnet_trie, archive);
    if (ipv4_subnet_array == NULL) {
        throw "error: could not initialize subnet_data";
    }
}

#endif
