#include "config.h"

#define RENEPAY_UNITTEST // logs are written in /tmp/debug.txt
#include "../payment.c"
#include "../flow.c"
#include "../uncertainty.c"
#include "../mcf.c"

#include <ccan/array_size/array_size.h>
#include <ccan/read_write_all/read_write_all.h>
#include <common/bigsize.h>
#include <common/channel_id.h>
#include <common/gossip_store.h>
#include <common/node_id.h>
#include <common/setup.h>
#include <common/wireaddr.h>
#include <stdio.h>
#include <wire/onion_wiregen.h>

/* AUTOGENERATED MOCKS START */
/* Generated stub for disabledmap_add_channel */
void disabledmap_add_channel(struct disabledmap *p UNNEEDED,
			     struct short_channel_id_dir scidd UNNEEDED)
{ fprintf(stderr, "disabledmap_add_channel called!\n"); abort(); }
/* Generated stub for disabledmap_add_node */
void disabledmap_add_node(struct disabledmap *p UNNEEDED, struct node_id node UNNEEDED)
{ fprintf(stderr, "disabledmap_add_node called!\n"); abort(); }
/* Generated stub for disabledmap_channel_is_warned */
bool disabledmap_channel_is_warned(struct disabledmap *p UNNEEDED,
				   struct short_channel_id_dir scidd UNNEEDED)
{ fprintf(stderr, "disabledmap_channel_is_warned called!\n"); abort(); }
/* Generated stub for disabledmap_new */
struct disabledmap *disabledmap_new(const tal_t *ctx UNNEEDED)
{ fprintf(stderr, "disabledmap_new called!\n"); abort(); }
/* Generated stub for disabledmap_reset */
void disabledmap_reset(struct disabledmap *p UNNEEDED)
{ fprintf(stderr, "disabledmap_reset called!\n"); abort(); }
/* Generated stub for disabledmap_warn_channel */
void disabledmap_warn_channel(struct disabledmap *p UNNEEDED,
			      struct short_channel_id_dir scidd UNNEEDED)
{ fprintf(stderr, "disabledmap_warn_channel called!\n"); abort(); }
/* Generated stub for json_add_payment */
void json_add_payment(struct json_stream *s UNNEEDED, const struct payment *payment UNNEEDED)
{ fprintf(stderr, "json_add_payment called!\n"); abort(); }
/* Generated stub for new_routetracker */
struct routetracker *new_routetracker(const tal_t *ctx UNNEEDED, struct payment *payment UNNEEDED)
{ fprintf(stderr, "new_routetracker called!\n"); abort(); }
/* Generated stub for pay_plugin */
struct pay_plugin *pay_plugin;
/* Generated stub for routetracker_cleanup */
void routetracker_cleanup(struct routetracker *routetracker UNNEEDED)
{ fprintf(stderr, "routetracker_cleanup called!\n"); abort(); }
/* Generated stub for sciddir_or_pubkey_from_node_id */
bool sciddir_or_pubkey_from_node_id(struct sciddir_or_pubkey *sciddpk UNNEEDED,
				    const struct node_id *node_id UNNEEDED)
{ fprintf(stderr, "sciddir_or_pubkey_from_node_id called!\n"); abort(); }
/* AUTOGENERATED MOCKS END */

static u8 empty_map[] = {
	0
};

static const char* print_flows(
		const tal_t *ctx,
		const char *desc,
		const struct gossmap *gossmap,
		struct chan_extra_map* chan_extra_map,
		struct flow **flows)
{
	tal_t *this_ctx = tal(ctx,tal_t);
	double tot_prob =
	    flowset_probability(tmpctx, flows, gossmap, chan_extra_map, false, NULL);
	assert(tot_prob>=0);
	char *buff = tal_fmt(ctx,"%s: %zu subflows, prob %2lf\n", desc, tal_count(flows),tot_prob);
	for (size_t i = 0; i < tal_count(flows); i++) {
		struct amount_msat fee, delivered;
		tal_append_fmt(&buff,"   ");
		for (size_t j = 0; j < tal_count(flows[i]->path); j++) {
			struct short_channel_id scid
				= gossmap_chan_scid(gossmap,
						    flows[i]->path[j]);
			tal_append_fmt(&buff,"%s%s", j ? "->" : "",
			       fmt_short_channel_id(this_ctx, scid));
		}
		delivered = flows[i]->amount;
		if (!flow_fee(&fee, flows[i]))
		{
			abort();
		}
		tal_append_fmt(&buff," prob %.2f, %s delivered with fee %s\n",
		       flows[i]->success_prob,
		       fmt_amount_msat(this_ctx, delivered),
		       fmt_amount_msat(this_ctx, fee));
	}

	tal_free(this_ctx);
	return buff;
}

static void remove_file(char *fname) { assert(!remove(fname)); }

int main(int argc, char *argv[])
{
	int fd;
	char *gossfile;
	struct gossmap *gossmap;
	struct node_id l1, l2, l3, l4;
	struct short_channel_id scid12, scid13, scid24, scid34;
	struct gossmap_localmods *mods;
	struct chan_extra_map *chan_extra_map;

	char *errmsg;

	common_setup(argv[0]);

	fd = tmpdir_mkstemp(tmpctx, "run-not_mcf-diamond.XXXXXX", &gossfile);
	tal_add_destructor(gossfile, remove_file);
	assert(write_all(fd, empty_map, sizeof(empty_map)));

	gossmap = gossmap_load(tmpctx, gossfile, NULL, NULL);
	assert(gossmap);

	/* These are in ascending order, for easy direction setting */
	assert(node_id_from_hexstr("022d223620a359a47ff7f7ac447c85c46c923da53389221a0054c11c1e3ca31d59", 66, &l1));
	assert(node_id_from_hexstr("0266e4598d1d3c415f572a8488830b60f7e744ed9235eb0b1ba93283b315c03518", 66, &l2));
	assert(node_id_from_hexstr("035d2b1192dfba134e10e540875d366ebc8bc353d5aa766b80c090b39c3a5d885d", 66, &l3));
	assert(node_id_from_hexstr("0382ce59ebf18be7d84677c2e35f23294b9992ceca95491fcf8a56c6cb2d9de199", 66, &l4));
	assert(short_channel_id_from_str("1x2x0", 5, &scid12));
	assert(short_channel_id_from_str("1x3x0", 5, &scid13));
	assert(short_channel_id_from_str("2x4x0", 5, &scid24));
	assert(short_channel_id_from_str("3x4x0", 5, &scid34));

	mods = gossmap_localmods_new(tmpctx);

	/* 1->2->4 has capacity 10k sat, 1->3->4 has capacity 5k sat (lower fee!) */
	assert(gossmap_local_addchan(mods, &l1, &l2, scid12, AMOUNT_MSAT(10000000), NULL));
	assert(gossmap_local_setchan(mods, scid12,
				     /*htlc_min=*/ AMOUNT_MSAT(0),
				     /*htlc_max=*/ AMOUNT_MSAT(10000000),
				     /*base_fee=*/ AMOUNT_MSAT(0),
				     /*ppm_fee =*/ 1001,
				     /* delay  =*/ 5,
				     /* enabled=*/ true,
				     /* dir    =*/ 0));
	assert(gossmap_local_addchan(mods, &l2, &l4, scid24, AMOUNT_MSAT(10000000), NULL));
	assert(gossmap_local_setchan(mods, scid24,
				     AMOUNT_MSAT(0),
				     AMOUNT_MSAT(10000000),
				     AMOUNT_MSAT(0), 1002, 5,
				     true,
				     0));
	assert(gossmap_local_addchan(mods, &l1, &l3, scid13, AMOUNT_MSAT(5000000), NULL));
	assert(gossmap_local_setchan(mods, scid13,
				     AMOUNT_MSAT(0),
				     AMOUNT_MSAT(5000000),
				     AMOUNT_MSAT(0), 503, 5,
				     true,
				     0));
	assert(gossmap_local_addchan(mods, &l3, &l4, scid34, AMOUNT_MSAT(5000000), NULL));
	assert(gossmap_local_setchan(mods, scid34,
				     AMOUNT_MSAT(0),
				     AMOUNT_MSAT(5000000),
				     AMOUNT_MSAT(0), 504, 5,
				     true,
				     0));

	gossmap_apply_localmods(gossmap, mods);
	chan_extra_map = tal(tmpctx, struct chan_extra_map);
	chan_extra_map_init(chan_extra_map);
	/* The local chans have no "capacity", so set them manually. */
	new_chan_extra(chan_extra_map,
			    scid12,
			    AMOUNT_MSAT(10000000));
	new_chan_extra(chan_extra_map,
			    scid24,
			    AMOUNT_MSAT(10000000));
	new_chan_extra(chan_extra_map,
			    scid13,
			    AMOUNT_MSAT(5000000));
	new_chan_extra(chan_extra_map,
			    scid34,
			    AMOUNT_MSAT(5000000));

	bitmap *disabled = tal_arrz(
	    tmpctx, bitmap, 2 * BITMAP_NWORDS(gossmap_max_chan_idx(gossmap)));

	struct flow **flows;
	flows = minflow(tmpctx, gossmap,
			gossmap_find_node(gossmap, &l1),
			gossmap_find_node(gossmap, &l4),
			chan_extra_map,
			disabled,
			/* Half the capacity */
			AMOUNT_MSAT(1000000), // 1000 sats
			 /* max_fee = */ AMOUNT_MSAT(10000), // 10 sats
			 /* min probability = */ 0.8, // 80%
			 /* base probability = */ 1.0,
			 /* delay fee factor = */ 0,
			 /* base fee penalty */ 0,
			 /* prob cost factor = */ 1,
			 &errmsg);
	if (!flows) {
		printf("Minflow has failed with: %s", errmsg);
		assert(0 && "minflow failed");
	}

	printf("%s\n",
		print_flows(tmpctx,"Simple minflow", gossmap,chan_extra_map, flows));

	common_shutdown();
}
