#include "net.h"

#define NET_TIMEOUT_MS 15000
#define INBOX_SIZE 1024
#define OUTBOX_SIZE 128
#define REQ_TABLE 1
#define REQ_SUBMIT 2
#define RETRY_MS 2000   /* a send that did not reach the phone gets one more try before "No phone" */

static NetCallback s_cb;
static int32_t s_seq;
static AppTimer *s_timer;
static NetReply s_reply;
static struct { int req; Difficulty d; Best b; char name[NAME_LEN + 1]; bool retried; } s_req;
static AppTimer *s_retry_timer;
static bool s_js_ready;   /* the phone said READY: until then a request waits instead of failing */
static bool s_waiting;    /* a request is held for READY */

static void prv_stop_timers(void) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  if (s_retry_timer) { app_timer_cancel(s_retry_timer); s_retry_timer = NULL; }
}

static void prv_finish(NetStatus status) {
  prv_stop_timers();
  NetCallback cb = s_cb;
  s_cb = NULL;
  s_reply.status = status;
  if (cb) cb(&s_reply);
}

static void prv_timeout(void *ctx) {
  s_timer = NULL;
  prv_finish(NET_NO_PHONE);
}

static int32_t prv_int(DictionaryIterator *it, uint32_t key) {
  Tuple *t = dict_find(it, key);
  if (!t) return 0;
  if (t->type == TUPLE_CSTRING) return (int32_t)atoi(t->value->cstring);
  switch (t->length) {
    case 1: return t->type == TUPLE_INT ? t->value->int8 : t->value->uint8;
    case 2: return t->type == TUPLE_INT ? t->value->int16 : t->value->uint16;
    default: return t->value->int32;
  }
}

static const char *prv_str(DictionaryIterator *it, uint32_t key) {
  Tuple *t = dict_find(it, key);
  return t && t->type == TUPLE_CSTRING ? t->value->cstring : "";
}

static bool prv_write(void);
static void prv_send_failed(void);

static void prv_inbox(DictionaryIterator *it, void *ctx) {
  if (dict_find(it, MESSAGE_KEY_READY)) {
    s_js_ready = true;
    if (s_cb && s_waiting) {
      s_waiting = false;
      if (!prv_write()) prv_send_failed();
    }
    return;
  }
  if (!s_cb || prv_int(it, MESSAGE_KEY_SEQ) != s_seq) return;   /* late answer to an old request */
  memset(&s_reply, 0, sizeof(s_reply));
  int32_t status = prv_int(it, MESSAGE_KEY_STATUS);
  if (status != 0) {
    prv_finish(status == 1 ? NET_NO_INTERNET : NET_SERVER);
    return;
  }
  s_reply.rank = (uint32_t)prv_int(it, MESSAGE_KEY_RANK);
  s_reply.total = (uint32_t)prv_int(it, MESSAGE_KEY_TOTAL);
  s_reply.n_rows = board_parse_rows(prv_str(it, MESSAGE_KEY_ROWS), s_reply.rows, BOARD_MAX_ROWS);
  const char *me = prv_str(it, MESSAGE_KEY_ME);
  s_reply.has_me = me[0] && board_parse_row(me, strlen(me), &s_reply.me);
  s_reply.online = (Best){ (uint16_t)prv_int(it, MESSAGE_KEY_ON_NIGHTS), (uint32_t)prv_int(it, MESSAGE_KEY_ON_SCORE) };
  prv_finish(NET_OK);
}

void net_cancel(void) {
  prv_stop_timers();
  s_cb = NULL;
  s_waiting = false;
}

static bool prv_write(void) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return false;
  dict_write_int32(it, MESSAGE_KEY_REQ, s_req.req);
  dict_write_int32(it, MESSAGE_KEY_SEQ, s_seq);
  dict_write_int32(it, MESSAGE_KEY_DIFF, (int32_t)s_req.d);
  dict_write_cstring(it, MESSAGE_KEY_NAME, s_req.name);
  if (s_req.req == REQ_SUBMIT) {
    dict_write_int32(it, MESSAGE_KEY_NIGHTS, s_req.b.nights);
    dict_write_int32(it, MESSAGE_KEY_SCORE, (int32_t)s_req.b.score);
  }
  return app_message_outbox_send() == APP_MSG_OK;
}

static void prv_retry(void *ctx) {
  s_retry_timer = NULL;
  if (s_cb && !prv_write()) prv_finish(NET_NO_PHONE);
}

/* A send that did not reach the phone: try once more shortly, then give up. */
static void prv_send_failed(void) {
  if (s_req.retried) { prv_finish(NET_NO_PHONE); return; }
  s_req.retried = true;
  s_retry_timer = app_timer_register(RETRY_MS, prv_retry, NULL);
}

static void prv_send(int req, Difficulty d, Best b, const char *name, NetCallback cb) {
  net_cancel();
  s_cb = cb;
  s_seq++;
  memset(&s_reply, 0, sizeof(s_reply));
  s_req.req = req; s_req.d = d; s_req.b = b; s_req.retried = false;
  strncpy(s_req.name, name ? name : "", NAME_LEN);
  s_req.name[NAME_LEN] = '\0';
  s_timer = app_timer_register(NET_TIMEOUT_MS, prv_timeout, NULL);
  s_waiting = !s_js_ready;
  if (!s_waiting && !prv_write()) prv_send_failed();
}

static void prv_outbox_failed(DictionaryIterator *it, AppMessageResult reason, void *ctx) {
  if (s_cb && prv_int(it, MESSAGE_KEY_SEQ) == s_seq) prv_send_failed();
}

void net_init(void) {
  app_message_register_inbox_received(prv_inbox);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
}

void net_request_table(Difficulty d, const char *name, NetCallback cb) {
  prv_send(REQ_TABLE, d, (Best){ 0, 0 }, name, cb);
}

void net_request_submit(Difficulty d, Best b, const char *name, NetCallback cb) {
  prv_send(REQ_SUBMIT, d, b, name, cb);
}

const char *net_status_text(NetStatus s) {
  switch (s) {
    case NET_NO_PHONE:    return "No phone connection";
    case NET_NO_INTERNET: return "No internet";
    case NET_SERVER:      return "Server error";
    default:              return NULL;
  }
}
