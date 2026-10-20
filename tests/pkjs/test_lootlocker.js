// Node tests for src/pkjs/lootlocker.js: `node tests/pkjs/test_lootlocker.js`.
var assert = require('assert');
var ll = require('../../src/pkjs/lootlocker.js');

var failed = 0, passed = 0;
function test(name, fn) {
  try { fn(); passed++; } catch (e) { failed++; console.log('FAIL ' + name + ': ' + e.message); }
}

function memStore(init) {
  var d = Object.assign({}, init || {});
  return { getItem: function (k) { return k in d ? d[k] : null; },
           setItem: function (k, v) { d[k] = String(v); }, data: d };
}

// routes: { 'GET /leaderboards/normal/list': fn(call) -> [status, json] | 'NETERR' }
function fakeHttp(routes) {
  var log = [];
  var http = function (method, url, headers, body, cb) {
    var path = url.replace(/^https:\/\/[^/]+\/game/, '').replace(/\?.*$/, '');
    var call = { method: method, path: path, url: url, headers: headers, body: body };
    log.push(call);
    var r = routes[method + ' ' + path];
    if (!r) return cb(null, 404, {});
    var res = r(call);
    if (res === 'NETERR') return cb(new Error('net'));
    cb(null, res[0], res[1]);
  };
  http.log = log;
  return http;
}

var config = { gameKey: 'dev_key', domain: 'api.lootlocker.io', gameVersion: '1.4.0' };

function item(rank, name, nights, score, id) {
  return { rank: rank, score: ll.pack(nights, score), member_id: String(id || 100 + rank),
           player: { name: name, id: id || 100 + rank, public_uid: 'X' } };
}
function top10() {
  var items = [];
  for (var i = 1; i <= 10; i++) items.push(item(i, 'P' + i, 30 - i, 1000));
  return items;
}
function baseRoutes(extra) {
  var r = {
    'POST /v2/session/guest': function () { return [200, { session_token: 'tok', player_id: 777 }]; },
    'PATCH /player/name': function () { return [200, { name: 'x' }]; },
    'GET /leaderboards/normal/list': function () {
      return [200, { items: top10(), pagination: { total: 230, next_cursor: 10, previous_cursor: null } }];
    },
    'GET /leaderboards/normal/member/777': function () {
      return [200, { rank: 14, score: ll.pack(12, 3400), member_id: '777', player: { name: 'DIM', id: 777 } }];
    }
  };
  for (var k in extra) r[k] = extra[k];
  return r;
}
function client(http, store) {
  return ll.createClient({ http: http, store: store || memStore(), config: config, playerIdentifier: 'pid-1' });
}
function sync(fn) { var out; fn(function (r) { out = r; }); assert.ok(out, 'callback not called synchronously'); return out; }

test('pack and unpack', function () {
  assert.strictEqual(ll.pack(12, 3400), 12003400);
  assert.strictEqual(ll.pack(1, 5000000), 1999999);
  assert.deepStrictEqual(ll.unpack(12003400), { nights: 12, score: 3400 });
});

test('table logs in, sets name, returns top 10 and own row', function () {
  var http = fakeHttp(baseRoutes()), store = memStore();
  var r = sync(function (cb) { client(http, store).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 0);
  assert.strictEqual(r.total, 230);
  assert.strictEqual(r.rows.length, 10);
  assert.strictEqual(r.rows[0], '1|P1|29|1000');
  assert.strictEqual(r.me, '14|DIM|12|3400');
  var login = http.log[0];
  assert.strictEqual(login.path, '/v2/session/guest');
  assert.strictEqual(login.body.game_key, 'dev_key');
  assert.strictEqual(login.body.player_identifier, 'pid-1');
  assert.strictEqual(http.log[1].method, 'PATCH');
  assert.strictEqual(http.log[1].body.name, 'DIM');
  assert.strictEqual(http.log[1].headers['x-session-token'], 'tok');
  assert.ok(/[?&]count=10\b/.test(http.log[2].url));
});

test('second table with same name skips login and rename', function () {
  var http = fakeHttp(baseRoutes()), store = memStore();
  sync(function (cb) { client(http, store).table(1, 'DIM', cb); });
  var n = http.log.length;
  sync(function (cb) { client(http, store).table(1, 'DIM', cb); });
  var later = http.log.slice(n).map(function (c) { return c.method + ' ' + c.path; });
  assert.deepStrictEqual(later, ['GET /leaderboards/normal/list', 'GET /leaderboards/normal/member/777']);
});

test('empty name is never sent', function () {
  var http = fakeHttp(baseRoutes());
  sync(function (cb) { client(http).table(1, '', cb); });
  assert.ok(!http.log.some(function (c) { return c.method === 'PATCH'; }));
});

test('player not on the board has no own row', function () {
  var http = fakeHttp(baseRoutes({
    'GET /leaderboards/normal/member/777': function () { return [200, { rank: 0, score: 0, member_id: '777' }]; }
  }));
  var r = sync(function (cb) { client(http).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 0);
  assert.strictEqual(r.me, '');
});

test('expired session: one relogin and retry', function () {
  var lists = 0;
  var http = fakeHttp(baseRoutes({
    'GET /leaderboards/normal/list': function () {
      lists++;
      return lists === 1 ? [401, {}] : [200, { items: [], pagination: { total: 0 } }];
    }
  }));
  var store = memStore({ ll_session: 'old', ll_player_id: '777', ll_name: 'DIM' });
  var r = sync(function (cb) { client(http, store).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 0);
  assert.strictEqual(lists, 2);
  assert.strictEqual(http.log.filter(function (c) { return c.path === '/v2/session/guest'; }).length, 1);
});

// What LootLocker really answers for an expired or ended session (checked against the live API).
test('expired session answered with 403: one relogin and retry', function () {
  var lists = 0;
  var http = fakeHttp(baseRoutes({
    'GET /leaderboards/normal/list': function () {
      lists++;
      return lists === 1 ? [403, { message: 'invalid session token' }] : [200, { items: [], pagination: { total: 0 } }];
    }
  }));
  var store = memStore({ ll_session: 'old', ll_player_id: '777', ll_name: 'DIM' });
  var r = sync(function (cb) { client(http, store).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 0);
  assert.strictEqual(lists, 2);
  assert.strictEqual(http.log.filter(function (c) { return c.path === '/v2/session/guest'; }).length, 1);
});

test('relogin as another player sends the name again', function () {
  var lists = 0;
  var http = fakeHttp(baseRoutes({
    'POST /v2/session/guest': function () { return [200, { session_token: 'tok2', player_id: 888 }]; },
    'GET /leaderboards/normal/list': function () {
      lists++;
      return lists === 1 ? [403, { message: 'invalid session token' }] : [200, { items: [], pagination: { total: 0 } }];
    }
  }));
  var store = memStore({ ll_session: 'old', ll_player_id: '777', ll_name: 'DIM' });
  var r = sync(function (cb) { client(http, store).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 0);
  var renames = http.log.filter(function (c) { return c.path === '/player/name'; });
  assert.strictEqual(renames.length, 1);
  assert.strictEqual(renames[0].body.name, 'DIM');
});

test('submit retried after relogin as another player: renamed first, sent as the new player', function () {
  var subs = 0;
  var http = fakeHttp(baseRoutes({
    'POST /v2/session/guest': function () { return [200, { session_token: 'tok2', player_id: 888 }]; },
    'POST /leaderboards/normal/submit': function () {
      subs++;
      return subs === 1 ? [403, { message: 'invalid session token' }] : [200, {}];
    },
    'GET /leaderboards/normal/member/888': function () { return [200, { rank: 1, score: 12003400, member_id: '888' }]; },
    'GET /leaderboards/normal/list': function () { return [200, { items: [item(1, 'DIM', 12, 3400, 888)], pagination: { total: 1 } }]; }
  }));
  var store = memStore({ ll_session: 'old', ll_player_id: '777', ll_name: 'DIM' });
  var r = sync(function (cb) { client(http, store).submit(1, 12, 3400, 'DIM', cb); });
  assert.strictEqual(r.status, 0);
  var paths = http.log.map(function (c) { return c.method + ' ' + c.path; });
  var rename = paths.indexOf('PATCH /player/name'), retry = paths.lastIndexOf('POST /leaderboards/normal/submit');
  assert.ok(rename >= 0 && rename < retry, 'renamed before the retried submit: ' + paths.join(', '));
  assert.strictEqual(http.log[retry].body.member_id, '888');
  assert.strictEqual(store.data.ll_name, 'DIM');
});

test('403 on everything after relogin: server error, no rename loop', function () {
  var http = fakeHttp(baseRoutes({
    'POST /v2/session/guest': function () { return [200, { session_token: 'tok2', player_id: 888 }]; },
    'PATCH /player/name': function () { return [403, {}]; },
    'GET /leaderboards/normal/list': function () { return [403, {}]; }
  }));
  var store = memStore({ ll_session: 'old', ll_player_id: '777', ll_name: 'DIM' });
  var r = sync(function (cb) { client(http, store).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 2);
  assert.ok(http.log.length < 10, http.log.length + ' requests');
});

test('401 twice is a server error, no login loop', function () {
  var http = fakeHttp(baseRoutes({ 'GET /leaderboards/normal/list': function () { return [401, {}]; } }));
  var r = sync(function (cb) { client(http).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 2);
  assert.ok(http.log.filter(function (c) { return c.path === '/v2/session/guest'; }).length <= 2);
});

test('network error is no internet, 500 is server error', function () {
  var r1 = sync(function (cb) {
    client(fakeHttp(baseRoutes({ 'POST /v2/session/guest': function () { return 'NETERR'; } }))).table(1, 'A', cb);
  });
  assert.strictEqual(r1.status, 1);
  var r2 = sync(function (cb) {
    client(fakeHttp(baseRoutes({ 'GET /leaderboards/normal/list': function () { return [500, {}]; } }))).table(1, 'A', cb);
  });
  assert.strictEqual(r2.status, 2);
});

test('missing game key is a server error without network', function () {
  var http = fakeHttp(baseRoutes());
  var c = ll.createClient({ http: http, store: memStore(), config: { gameKey: '', domain: 'x' }, playerIdentifier: 'p' });
  var r = sync(function (cb) { c.table(1, 'A', cb); });
  assert.strictEqual(r.status, 2);
  assert.strictEqual(http.log.length, 0);
});

test('submit sends packed score and reports rank, neighbours, online best', function () {
  var http = fakeHttp(baseRoutes({
    'POST /leaderboards/normal/submit': function () { return [200, { rank: 14, score: 12003400, member_id: '777' }]; },
    'GET /leaderboards/normal/member/777': function () {
      return [200, { rank: 9, score: 13000100, member_id: '777', player: { name: 'DIM', id: 777 } }];
    },
    'GET /leaderboards/normal/list': function () {
      var items = [];
      for (var i = 5; i <= 13; i++) items.push(i === 9 ? item(9, 'DIM', 13, 100, 777) : item(i, 'P' + i, 30 - i, 50));
      return [200, { items: items, pagination: { total: 230 } }];
    }
  }));
  var r = sync(function (cb) { client(http).submit(1, 12, 3400, 'DIM', cb); });
  var sub = http.log.filter(function (c) { return c.path === '/leaderboards/normal/submit'; })[0];
  assert.strictEqual(sub.body.score, 12003400);
  assert.strictEqual(sub.body.member_id, '777');
  assert.strictEqual(r.status, 0);
  assert.strictEqual(r.rank, 9);
  assert.strictEqual(r.total, 230);
  assert.deepStrictEqual(r.online, { nights: 13, score: 100 });
  assert.deepStrictEqual(r.rows.map(function (s) { return +s.split('|')[0]; }), [7, 8, 9, 10, 11]);
});

test('submit near the top asks from the first row', function () {
  var http = fakeHttp(baseRoutes({
    'POST /leaderboards/easy/submit': function () { return [200, {}]; },
    'GET /leaderboards/easy/member/777': function () { return [200, { rank: 1, score: 5000010, member_id: '777' }]; },
    'GET /leaderboards/easy/list': function () {
      return [200, { items: [item(1, 'DIM', 5, 10, 777), item(2, 'B', 4, 1)], pagination: { total: 2 } }];
    }
  }));
  var r = sync(function (cb) { client(http).submit(0, 5, 10, 'DIM', cb); });
  var list = http.log.filter(function (c) { return c.path === '/leaderboards/easy/list'; })[0];
  assert.ok(!/after=/.test(list.url));
  assert.strictEqual(r.rank, 1);
  assert.deepStrictEqual(r.rows, ['1|DIM|5|10', '2|B|4|1']);
});

test('separators in names become spaces', function () {
  var http = fakeHttp(baseRoutes({
    'GET /leaderboards/normal/list': function () {
      return [200, { items: [item(1, 'A|B', 3, 3), item(2, 'C\nD', 2, 2)], pagination: { total: 2 } }];
    }
  }));
  var r = sync(function (cb) { client(http).table(1, 'DIM', cb); });
  assert.deepStrictEqual(r.rows, ['1|A B|3|3', '2|C D|2|2']);
});

test('player with no score yet: member 404 means no own row', function () {
  var http = fakeHttp(baseRoutes({
    'GET /leaderboards/normal/member/777': function () { return [404, { message: 'not found' }]; }
  }));
  var r = sync(function (cb) { client(http).table(1, 'DIM', cb); });
  assert.strictEqual(r.status, 0);
  assert.strictEqual(r.me, '');
  assert.strictEqual(r.rows.length, 10);
});

test('names from the server are cut to the watch alphabet and 6 characters', function () {
  var long = 'Ünïcode-NAME-way-too-long-' + new Array(100).join('x');
  var http = fakeHttp(baseRoutes({
    'GET /leaderboards/normal/list': function () {
      return [200, { items: [item(1, long, 3, 3), item(2, 'ab|c', 2, 2)], pagination: { total: 2 } }];
    }
  }));
  var r = sync(function (cb) { client(http).table(1, 'DIM', cb); });
  assert.deepStrictEqual(r.rows, ['1|N CODE|3|3', '2|AB C|2|2']);
});

test('domain pasted as a full URL still builds the right address', function () {
  var http = fakeHttp(baseRoutes());
  var c = ll.createClient({ http: http, store: memStore(),
    config: { gameKey: 'k', domain: 'https://abc.api.lootlocker.com/', gameVersion: '1' }, playerIdentifier: 'p' });
  sync(function (cb) { c.table(1, '', cb); });
  assert.strictEqual(http.log[0].url, 'https://abc.api.lootlocker.com/game/v2/session/guest');
});

console.log(passed + ' passed, ' + failed + ' failed');
process.exit(failed ? 1 : 0);
