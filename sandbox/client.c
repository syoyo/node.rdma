#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#include <assert.h>

#include <uv.h>

static int close_cb_called;
static int exit_cb_called;
static int on_read_cb_called;
static int after_write_cb_called;

static char exepath[1024];
static size_t exepath_size = 1024;
static char* args[3];

static uv_process_options_t options;
static uv_loop_t* loop;
uv_pipe_t out, in;
uv_pipe_t cm_pipe;

#define OUTPUT_SIZE 1024
static char output[OUTPUT_SIZE];
static int output_used;

#define COUNTOF(a) (sizeof(a) / sizeof(a[0]))

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;


static void close_cb(uv_handle_t* handle) {
  printf("close_cb\n");
  close_cb_called++;
}

static void exit_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
  assert(exit_status == 0);
  assert(term_signal == 0);
  uv_close((uv_handle_t*)process, close_cb);
  uv_close((uv_handle_t*)&in, close_cb);
  uv_close((uv_handle_t*)&out, close_cb);
}

static uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = output + output_used;
  buf.len = OUTPUT_SIZE - output_used;
  return buf;
}


static void after_write(uv_write_t* req, int status) {
  write_req_t* wr;

  if (status) {
    uv_err_t err = uv_last_error(loop);
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    assert(0);
  }

  wr = (write_req_t*) req;

  /* Free the read/write buffer and the request */
  free(wr);

  after_write_cb_called++;
}



static void init_process_options(char* test, uv_exit_cb exit_cb)
{
    int r = uv_exepath(exepath, &exepath_size);
    assert(r == 0);
    exepath[exepath_size] = '\0';

    args[0] = exepath;
    args[1] = test;
    args[2] = NULL;

    options.file    = exepath;
    options.args    = args;
    options.exit_cb = exit_cb;
}

static void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  write_req_t* write_req;
  int r;
  uv_err_t err = uv_last_error(uv_default_loop());

  assert(nread > 0 || err.code == UV_EOF);

  if (nread > 0) {
    output_used += nread;
    if (output_used == 12) {
      assert(memcmp("hello world\n", output, 12) == 0);
      write_req = (write_req_t*)malloc(sizeof(*write_req));
      write_req->buf = uv_buf_init(output, output_used);
      r = uv_write(&write_req->req, (uv_stream_t*)&in, &write_req->buf, 1, after_write);
      assert(r == 0);
    }
  }

  on_read_cb_called++;
}

static void on_cm_event_read(uv_pipe_t* cm, ssize_t nread, uv_buf_t buf, uv_handle_type pending) {
    printf("on_cm_event_read\n");

  write_req_t* write_req;
  int r;
  uv_err_t err = uv_last_error(uv_default_loop());

  assert(nread > 0 || err.code == UV_EOF);
  printf("nread = %ld\n", nread);

  if (nread > 0) {
    output_used += nread;
    if (output_used == 12) {
      assert(memcmp("hello world\n", output, 12) == 0);
      write_req = (write_req_t*)malloc(sizeof(*write_req));
      write_req->buf = uv_buf_init(output, output_used);
      r = uv_write(&write_req->req, (uv_stream_t*)&in, &write_req->buf, 1, after_write);
      assert(r == 0);
    }
  }

  on_read_cb_called++;
}

static uv_pipe_t stdin_pipe;
static uv_pipe_t stdout_pipe;
static int on_pipe_read_called;
static int after_write_called;

static void after_pipe_write(uv_write_t* req, int status) {
  assert(status == 0);
  after_write_called++;
}

void on_pipe_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  assert(nread > 0);
  assert(memcmp("hello world\n", buf.base, nread) == 0);
  on_pipe_read_called++;

  free(buf.base);

  uv_close((uv_handle_t*)&stdin_pipe, close_cb);
  uv_close((uv_handle_t*)&stdout_pipe, close_cb);
}


static uv_buf_t on_pipe_read_alloc(uv_handle_t* handle,
    size_t suggested_size) {
  printf("sz = %ld\n", suggested_size);
  uv_buf_t buf;
  buf.base = (char*)malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}

static int stdio_over_pipes_helper() {
  /* Write several buffers to test that the write order is preserved. */
  char* buffers[] = {
    "he",
    "ll",
    "o ",
    "wo",
    "rl",
    "d",
    "\n"
  };

  uv_write_t write_req[COUNTOF(buffers)];
  uv_buf_t buf[COUNTOF(buffers)];
  int r, i;
  uv_loop_t* loop = uv_default_loop();
  
  assert(UV_NAMED_PIPE == uv_guess_handle(0));
  assert(UV_NAMED_PIPE == uv_guess_handle(1));

  r = uv_pipe_init(loop, &stdin_pipe, 0);
  assert(r == 0);
  r = uv_pipe_init(loop, &stdout_pipe, 0);
  assert(r == 0);

  uv_pipe_open(&stdin_pipe, 0);
  uv_pipe_open(&stdout_pipe, 1);

  /* Unref both stdio handles to make sure that all writes complete. */
  uv_unref(loop);
  uv_unref(loop);

  for (i = 0; i < COUNTOF(buffers); i++) {
    buf[i] = uv_buf_init((char*)buffers[i], strlen(buffers[i]));
  }

  for (i = 0; i < COUNTOF(buffers); i++) {
    r = uv_write(&write_req[i], (uv_stream_t*)&stdout_pipe, &buf[i], 1,
      after_pipe_write);
    assert(r == 0);
  }

  uv_run(loop);

  assert(after_write_called == 7);
  assert(on_pipe_read_called == 0);
  assert(close_cb_called == 0);

  uv_ref(loop);
  uv_ref(loop);

  r = uv_read_start((uv_stream_t*)&stdin_pipe, on_pipe_read_alloc,
    on_pipe_read);
  assert(r == 0);

  uv_run(loop);

  assert(after_write_called == 7);
  assert(on_pipe_read_called == 1);
  assert(close_cb_called == 2);

  return 0;
}

static uv_buf_t on_cm_event_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = output + output_used;
  buf.len = OUTPUT_SIZE - output_used;
  return buf;
}

static void on_cm_event(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  printf("on_cm_event\n");
  assert(nread > 0);
  //assert(memcmp("hello world\n", buf.base, nread) == 0);
  //on_pipe_read_called++;

  //free(buf.base);

  uv_close((uv_handle_t*)&cm_pipe, close_cb);
}

struct pdata {
    uint64_t buf_va;
    uint32_t buf_rkey;
};

int
main(
    int argc,
    char **argv)
{
  int r;
  uv_process_t process;

    if (argc < 2) {
        printf("Needs hostname\n");
        exit(-1);
    }

    //if (argc > 1) {
    //    fprintf(stderr, "fork\n");
    //    return stdio_over_pipes_helper();
    //} else {
    //    fprintf(stderr, "main\n");
    //}

    struct pdata                server_pdata;
    struct rdma_event_channel*  cm_channel;
    struct rdma_cm_id*          cm_id;
    struct rdma_cm_event*       event;
    struct rdma_conn_param      conn_param = {};

    struct ibv_pd*              pd;
    struct ibv_comp_channel*    comp_chan;
    struct ibv_cq*              cq;
    struct ibv_cq*              evt_cq;
    struct ibv_mr*              mr;
    struct ibv_qp_init_attr     qp_attr = {};
    struct ibv_sge              sge;
    struct ibv_send_wr          send_wr = {};
    struct ibv_send_wr*         bad_send_wr;
    struct ibv_recv_wr          recv_wr = {};
    struct ibv_recv_wr*         bad_recv_wr;
    struct ibv_wc               wc;
    void*                       cq_context;

    struct addrinfo *res, *t;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    int n;
    uint32_t* buf;
    int err;

#if 0
    // UV
    loop = uv_default_loop();

    uv_pipe_init(loop, &in, 0);
    uv_pipe_open(&in, 0);

    r = uv_read_start((uv_stream_t*) &in, on_pipe_read_alloc, on_pipe_read);
    assert(r == 0);

    r = uv_run(uv_default_loop());
    assert(r == 0);

    printf("UV test done\n");
    exit(0);
#endif

    cm_channel = rdma_create_event_channel();
    assert(cm_channel);

    err = rdma_create_id(cm_channel, &cm_id, NULL, RDMA_PS_TCP);
    assert(!err);

    n = getaddrinfo(argv[1], "20079", &hints, &res);
    assert(n == 0);

    for (t = res; t; t = t->ai_next) {
        err = rdma_resolve_addr(cm_id, NULL, t->ai_addr, 500);
        if (!err) break;
    }
    assert(!err);

    //r = uv_pipe_init(loop, &cm_pipe, 0);
    //assert(r == 0);
    //uv_pipe_open(&cm_pipe, cm_channel->fd);

    //r = uv_read_start((uv_stream_t*)&cm_pipe, on_cm_event_alloc, on_cm_event);

    printf("get_cm_event\n");
    err = rdma_get_cm_event(cm_channel, &event);
    assert(!err);
    printf("get_cm_event ok\n");

    printf("event = %d(0x%08x)\n", event->event, event->event);
    assert(event->event == RDMA_CM_EVENT_ADDR_RESOLVED);

    printf("ack\n");
    rdma_ack_cm_event(event);
    printf("ack ok\n");

    printf("resolve route\n");
    err = rdma_resolve_route(cm_id, 500);
    assert(!err);
    printf("resolve route ok\n");

    printf("get_cm_event\n");
    err = rdma_get_cm_event(cm_channel, &event);
    assert(!err);
    printf("get_cm_event ok\n");

    printf("event = %d(0x%08x)\n", event->event, event->event);
    assert(event->event == RDMA_CM_EVENT_ROUTE_RESOLVED);

    printf("ack\n");
    rdma_ack_cm_event(event);
    printf("ack ok\n");

    pd = ibv_alloc_pd(cm_id->verbs);
    assert(pd);
    printf("pd ok\n");

    comp_chan = ibv_create_comp_channel(cm_id->verbs);
    assert(comp_chan);
    printf("create comp_chan ok\n");

    cq = ibv_create_cq(cm_id->verbs, 2, NULL, comp_chan, 0);
    assert(cq);

    err = ibv_req_notify_cq(cq, 0);
    assert(!err);
    printf("req_notity_cqok\n");

    buf = calloc(2, sizeof(uint32_t));
    assert(buf);

    mr = ibv_reg_mr(pd, buf, 2 * sizeof(uint32_t), IBV_ACCESS_LOCAL_WRITE);
    assert(mr);

    qp_attr.cap.max_send_wr     = 2;
    qp_attr.cap.max_send_sge    = 1;
    qp_attr.cap.max_recv_wr     = 1;
    qp_attr.cap.max_recv_sge    = 1;
    qp_attr.send_cq             = cq;
    qp_attr.recv_cq             = cq;
    qp_attr.qp_type             = IBV_QPT_RC;

    err = rdma_create_qp(cm_id, pd, &qp_attr);
    assert(!err);
    printf("create qp ok\n");

    conn_param.initiator_depth = 1;
    conn_param.retry_count = 7;

    // connect to server
    err = rdma_connect(cm_id, &conn_param);
    assert(!err);
    printf("connect ok\n");

    err = rdma_get_cm_event(cm_channel, &event);
    assert(!err);

    printf("ev = %d\n", event->event);
    assert(event->event == RDMA_CM_EVENT_ESTABLISHED);
    
    memcpy(&server_pdata, event->param.conn.private_data, sizeof server_pdata);
    rdma_ack_cm_event(event);

    printf("estabished\n");

    // prepost receive
    sge.addr = (uintptr_t)buf;
    sge.length = sizeof(uint32_t);
    sge.lkey = mr->lkey;
    
    recv_wr.wr_id = 0;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;

    err = ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr);
    assert(!err);

    printf("post recv ok\n");

    int val0 = 3;
    int val1 = 5;

    buf[0] = 3;
    buf[1] = 5;
    printf("%d + %d = ", buf[0], buf[1]);
    buf[0] = htonl(buf[0]);
    buf[1] = htonl(buf[1]);

    sge.addr = (uintptr_t) buf;
    sge.length = sizeof (uint32_t);
    sge.lkey = mr->lkey;

    send_wr.wr_id = 1;
    send_wr.opcode = IBV_WR_RDMA_WRITE;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    send_wr.wr.rdma.rkey = ntohl(server_pdata.buf_rkey);
    send_wr.wr.rdma.remote_addr = ntohll(server_pdata.buf_va);

    err = ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr);
    assert(!err);

    printf("post send ok\n");

    sge.addr = (uintptr_t) buf + sizeof(uint32_t);
    sge.length = sizeof (uint32_t);
    sge.lkey = mr->lkey;

    send_wr.wr_id = 2;
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;

    err = ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr);
    assert(!err);

    printf("post send 2 ok\n");

    // wait for receive completion.

    while (1) {
        printf("while...\n");
        err = ibv_get_cq_event(comp_chan, &evt_cq, &cq_context);
        assert(!err);

        err = ibv_req_notify_cq(cq, 0);
        assert(!err);

        int num = ibv_poll_cq(cq, 1, &wc);
        assert(num == 1);

        assert(wc.status == IBV_WC_SUCCESS);
    
        if (wc.wr_id == 0) {
            printf("ret %d\n", ntohl(buf[0]));
            break;
        }
    }
    
    printf("OK\n");
    exit(0);


  init_process_options("stdio_over_pipes_helper", exit_cb);

  uv_pipe_init(loop, &out, 0);
  options.stdout_stream = &out;
  uv_pipe_init(loop, &in, 0);
  options.stdin_stream = &in;

  r = uv_spawn(loop, &process, options);
  assert(r == 0);

  r = uv_read2_start((uv_stream_t*) &in, on_cm_event_alloc, on_cm_event_read);
  assert(r == 0);

  r = uv_run(uv_default_loop());
  assert(r == 0);

  assert(on_read_cb_called > 1);
  assert(after_write_cb_called == 1);
  assert(exit_cb_called == 1);
  assert(close_cb_called == 3);
  assert(memcmp("hello world\n", output, 12) == 0);
  assert(output_used == 12);

  return 0;
}
