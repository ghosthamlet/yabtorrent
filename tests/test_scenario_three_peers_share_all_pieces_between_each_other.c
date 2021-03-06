
/**
 * Copyright (c) 2011, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. 
 *
 * @file
 * @author  Willem Thiart himself@willemthiart.com
 * @version 0.1
 */

#include <CuTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <assert.h>

#include "bt.h"
#include "network_adapter.h"
#include "network_adapter_mock.h"
#include "mock_torrent.h"
#include "mock_client.h"

#include "bt_piece_db.h"
#include "bt_diskmem.h"
#include "config.h"
#include "linked_list_hashmap.h"
#include "bipbuffer.h"
#include "asprintf.h"

#include <fcntl.h>
#include <sys/time.h>

/** a have message has to be sent by client B, for client A and C to finish */
void TestBT_Peer_three_share_all_pieces_between_each_other(CuTest * tc)
{
    int ii;
    client_t* a, *b, *c;
    hashmap_iterator_t iter;
    void* mt;
    char *addr;

    clients_setup();
    mt = mocktorrent_new(3,5);
    a = mock_client_setup(5);
    b = mock_client_setup(5);
    c = mock_client_setup(5);

    for (
        hashmap_iterator(clients_get(), &iter);
        hashmap_iterator_has_next(clients_get(), &iter);
        )
    {
        char hash[21];
        void* bt, *cfg;
        client_t* cli;

        cli = hashmap_iterator_next_value(clients_get(), &iter);
        bt = cli->bt;
        cfg = bt_dm_get_config(bt);
        /* default configuration for clients */
        config_set(cfg, "npieces", "3");
        config_set(cfg, "piece_length", "5");
        config_set(cfg, "infohash", "00000000000000000000");
        /* add files/pieces */
        bt_piecedb_increase_piece_space(bt_dm_get_piecedb(bt),20);
        bt_piecedb_add_with_hash_and_size(bt_dm_get_piecedb(bt),
                mocktorrent_get_piece_sha1(mt,hash,0), 5);
        bt_piecedb_add_with_hash_and_size(bt_dm_get_piecedb(bt),
                mocktorrent_get_piece_sha1(mt,hash,1), 5);
        bt_piecedb_add_with_hash_and_size(bt_dm_get_piecedb(bt),
                mocktorrent_get_piece_sha1(mt,hash,2), 5);
    }

    /* write blocks to client A & B */
    {
        void* data;
        bt_block_t blk;

        blk.piece_idx = 0;
        blk.offset = 0;
        blk.len = 5;

        data = mocktorrent_get_data(mt,0);
        bt_diskmem_write_block(
                bt_piecedb_get_diskstorage(bt_dm_get_piecedb(a->bt)),
                NULL, &blk, data);

        blk.piece_idx = 1;
        data = mocktorrent_get_data(mt,1);
        bt_diskmem_write_block(
                bt_piecedb_get_diskstorage(bt_dm_get_piecedb(b->bt)),
                NULL, &blk, data);

        blk.piece_idx = 2;
        data = mocktorrent_get_data(mt,2);
        bt_diskmem_write_block(
                bt_piecedb_get_diskstorage(bt_dm_get_piecedb(c->bt)),
                NULL, &blk, data);
    }

    bt_dm_check_pieces(a->bt);
    bt_dm_check_pieces(b->bt);
    bt_dm_check_pieces(c->bt);

    /* A connects to B */
    asprintf(&addr,"%p", b);
    client_add_peer(a,NULL,0,addr,strlen(addr),0);
    /* B connects to C */
    asprintf(&addr,"%p", c);
    client_add_peer(b,NULL,0,addr,strlen(addr),0);

    for (ii=0; ii<10; ii++)
    {
#if 0 /* debugging */
        printf("\nStep %d:\n", ii+1);
#endif

        bt_dm_periodic(a->bt, NULL);
        bt_dm_periodic(b->bt, NULL);
        bt_dm_periodic(c->bt, NULL);

        network_poll(a->bt, (void*)&a, 0,
                bt_dm_dispatch_from_buffer,
                mock_on_connect);

        network_poll(b->bt, (void*)&b, 0,
                bt_dm_dispatch_from_buffer,
                mock_on_connect);

        network_poll(c->bt, (void*)&c, 0,
                bt_dm_dispatch_from_buffer,
                mock_on_connect);

//        __print_client_contents();
    }

//    bt_piecedb_print_pieces_downloaded(bt_dm_get_piecedb(a->bt));
//    bt_piecedb_print_pieces_downloaded(bt_dm_get_piecedb(b->bt));
//    bt_piecedb_print_pieces_downloaded(bt_dm_get_piecedb(c->bt));

    /* empty jobs */
    bt_dm_periodic(a->bt, NULL);
    bt_dm_periodic(b->bt, NULL);
    bt_dm_periodic(c->bt, NULL);

    CuAssertTrue(tc, 1 == bt_piecedb_all_pieces_are_complete(bt_dm_get_piecedb(a->bt)));
    CuAssertTrue(tc, 1 == bt_piecedb_all_pieces_are_complete(bt_dm_get_piecedb(b->bt)));
    CuAssertTrue(tc, 1 == bt_piecedb_all_pieces_are_complete(bt_dm_get_piecedb(c->bt)));
}

