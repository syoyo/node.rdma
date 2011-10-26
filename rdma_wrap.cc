// 
// Copyright 2011 Light Transport Entertainment, Inc.
//
// Licensed under BSD license
//

// node.js
#include <v8.h>
#include <node.h>

// C/C++
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// RDMA CM
#include <rdma/rdma_cma.h>

using namespace v8;


typedef struct
{
    struct ibv_context*         ctx;            ///< Context
    struct ibv_pd*              pd;             ///< Protection Domain
    struct ibv_cq*              cq;             ///< Completion Queue
    struct ibv_comp_channel*    comp_channel;   ///< Completion Event Channel
} RDMAContext;

typedef struct
{
    bool                        connected;

    struct rdma_cm_id*          id;
    struct ibv_qp*              qp;

    struct ibv_mr*              recv_mr;
    struct ibv_mr*              send_mr;
    struct ibv_mr*              rdma_local_mr;
    struct ibv_mr*              rdma_remote_mr;

    struct ibv_mr               peer_mr;

} RDMAConnection;

static void BuildRDMAContext(struct ibv_context* verbs)
{
    RDMAContext* ctx = (RDMAContext*)malloc(sizeof(RDMAContext));
    ctx->ctx    = verbs;

    ctx->pd     = ibv_alloc_pd(ctx->ctx);
    if (!ctx->pd) {
        fprintf(stderr, "Failed to operate ibv_alloc_pd()\n");
        exit(-1);
    }

    ctx->comp_channel = ibv_create_comp_channel(ctx->ctx);
    if (!ctx->comp_channel) {
        fprintf(stderr, "Failed to operate ibv_create_comp_channel()\n");
        exit(-1);
    }

    int cqe = 10;   // # of completion queue elements. This could be arbitrary
    int comp_vector = 0;
    ctx->cq = ibv_create_cq(ctx->ctx, cqe, NULL, ctx->comp_channel, comp_vector);
    if (!ctx->cq) {
        fprintf(stderr, "Failed to operate ibv_create_qp()\n");
        exit(-1);
    }

    if (ibv_req_notify_cq(ctx->cq, 0)) {
        fprintf(stderr, "Failed to operate ibv_req_notify_cq()\n");
        exit(-1);
    }

    if (ibv_req_notify_cq(ctx->cq, 0)) {
        fprintf(stderr, "Failed to operate ibv_req_notify_cq()\n");
        exit(-1);
    }
}

static void Connection(struct rdma_cm_id* id)
{
    RDMAConnection* conn;
    struct ibv_qp_init_attr qp_attr;

    BuildRDMAContext(id->verbs);
    
}

class EioData
{
public:
    EioData();
    ~EioData();
};

extern "C" void init(Handle<Object> target) {
    // @todo
}
