#pragma once
#include <pebble.h>
#include "../logic/types.h"
#include "../logic/board.h"

/* Leaderboard requests to the phone (src/pkjs). One at a time: a new one replaces the last. */
typedef enum { NET_OK, NET_NO_PHONE, NET_NO_INTERNET, NET_SERVER } NetStatus;

typedef struct {
  NetStatus status;
  uint32_t rank, total;
  BoardRow rows[BOARD_MAX_ROWS];
  int n_rows;
  BoardRow me;
  bool has_me;
  Best online;   /* the player's best the board kept (submit only) */
} NetReply;

typedef void (*NetCallback)(const NetReply *r);

void net_init(void);
void net_request_table(Difficulty d, const char *name, NetCallback cb);
void net_request_submit(Difficulty d, Best b, const char *name, NetCallback cb);
void net_cancel(void);   /* the screen is going away: its callback is not called any more */
/* "No phone connection" / "No internet" / "Server error"; NULL for NET_OK. */
const char *net_status_text(NetStatus s);
