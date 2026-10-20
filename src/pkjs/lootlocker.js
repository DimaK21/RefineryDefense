// LootLocker client for the leaderboard. No Pebble APIs here: index.js passes in the HTTP
// function and the storage, so tests/pkjs/test_lootlocker.js can drive it from node.
// Reply status: 0 ok, 1 no internet, 2 server error (same codes as the watch's STATUS key).

var BOARD_KEYS = ['easy', 'normal', 'hard'];   // index = the watch's Difficulty
var SCORE_CAP = 999999;
var TOP_COUNT = 10;
var AROUND = 2;   // rows above and below the player after a submit

// Nights rank first, points break ties: one number LootLocker can sort.
function pack(nights, score) { return nights * 1000000 + Math.min(score, SCORE_CAP); }
function unpack(n) { return { nights: Math.floor(n / 1000000), score: n % 1000000 }; }

// Only what the watch can draw and store: A-Z, 0-9, space, at most 6. Anything else becomes a
// space, so a hostile or foreign name can neither break the row format nor overflow the inbox.
function cleanName(s) {
  return String(s || '').toUpperCase().replace(/[^A-Z0-9 ]/g, ' ').trim().slice(0, 6).trim();
}
function row(it) {
  var b = unpack(it.score || 0);
  return it.rank + '|' + cleanName(it.player && it.player.name) + '|' + b.nights + '|' + b.score;
}

function createClient(opts) {
  var http = opts.http, store = opts.store, config = opts.config;
  // The console shows the API address as a URL; accept it pasted whole or as a bare host.
  var host = String(config.domain || '').replace(/^https?:\/\//, '').replace(/\/+$/, '');
  var base = 'https://' + host + '/game';

  function login(cb) {
    http('POST', base + '/v2/session/guest', { 'Content-Type': 'application/json' },
      { game_key: config.gameKey, game_version: config.gameVersion, player_identifier: opts.playerIdentifier },
      function (err, status, json) {
        if (err) return cb(1);
        if (status < 200 || status >= 300 || !json || !json.session_token) return cb(2);
        store.setItem('ll_session', json.session_token);
        store.setItem('ll_player_id', String(json.player_id));
        cb(0);
      });
  }

  // One authenticated request; a 401 or 403 logs in again and retries once. cb(status, json)
  // LootLocker answers an expired session with 403 "invalid session token", not 401.
  // `empty` lists HTTP codes that count as success with no body.
  function call(method, path, body, cb, empty) {
    var relogged = false;
    function attempt() {
      var headers = { 'Content-Type': 'application/json', 'LL-Version': '2021-03-01',
                      'x-session-token': store.getItem('ll_session') || '' };
      http(method, base + path, headers, body, function (err, status, json) {
        if (err) return cb(1);
        if ((status === 401 || status === 403) && !relogged) {
          relogged = true;
          return login(function (s) { return s ? cb(s) : attempt(); });
        }
        if (empty && empty.indexOf(status) >= 0) return cb(0, {});
        if (status < 200 || status >= 300) return cb(2);
        cb(0, json || {});
      });
    }
    if (store.getItem('ll_session')) attempt();
    else {
      relogged = true;   // a fresh session that still gets 401 will not get better
      login(function (s) { return s ? cb(s) : attempt(); });
    }
  }

  // Renames only when the name differs from the last one the server accepted.
  // A refused name (e.g. taken) does not block the request it rides along with.
  function syncName(name, cb) {
    if (!name || store.getItem('ll_name') === name) return cb(0);
    call('PATCH', '/player/name', { name: name }, function (s) {
      if (s === 1) return cb(1);
      if (s === 0) store.setItem('ll_name', name);
      cb(0);
    });
  }

  // A player who never sent may come back as 404 instead of rank 0: both mean "not on the board".
  function member(key, cb) {
    call('GET', '/leaderboards/' + key + '/member/' + store.getItem('ll_player_id'), null, cb, [404]);
  }

  function total(list) { return (list.pagination && list.pagination.total) || 0; }

  function table(diff, name, cb) {
    var key = BOARD_KEYS[diff];
    if (!config.gameKey || !key) return cb({ status: 2 });
    syncName(name, function (s) {
      if (s) return cb({ status: s });
      call('GET', '/leaderboards/' + key + '/list?count=' + TOP_COUNT, null, function (s, list) {
        if (s) return cb({ status: s });
        member(key, function (s, me) {
          if (s) return cb({ status: s });
          cb({ status: 0, total: total(list), rows: (list.items || []).map(row),
               me: me && me.rank > 0 ? row(me) : '' });
        });
      });
    });
  }

  function submit(diff, nights, score, name, cb) {
    var key = BOARD_KEYS[diff];
    if (!config.gameKey || !key) return cb({ status: 2 });
    syncName(name, function (s) {
      if (s) return cb({ status: s });
      call('POST', '/leaderboards/' + key + '/submit',
        { member_id: store.getItem('ll_player_id'), score: pack(nights, score) }, function (s) {
          if (s) return cb({ status: s });
          // The member entry holds what the board kept, which beats this run if the server knew better.
          member(key, function (s, me) {
            if (s) return cb({ status: s });
            var rank = me.rank || 0, first = Math.max(rank - AROUND, 1);
            // `after` is an offset whose exact base the docs leave open: ask a little wider, then filter.
            var q = first <= 2 ? '?count=' + (rank + AROUND)
                               : '?count=' + (2 * AROUND + 3) + '&after=' + (first - 2);
            call('GET', '/leaderboards/' + key + '/list' + q, null, function (s, list) {
              if (s) return cb({ status: s });
              var rows = (list.items || []).filter(function (it) {
                return it.rank >= first && it.rank <= rank + AROUND;
              }).map(row);
              cb({ status: 0, rank: rank, total: total(list), rows: rows, online: unpack(me.score || 0) });
            });
          });
        });
    });
  }

  return { table: table, submit: submit };
}

module.exports = { pack: pack, unpack: unpack, createClient: createClient };
