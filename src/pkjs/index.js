// Phone side of the leaderboard: relays the watch's requests to LootLocker.
// Nothing goes online on `ready`; only a request from the watch (a player action) does.
var ll = require('./lootlocker');
var config = require('./lootlocker_config');

var REQ_TABLE = 1, REQ_SUBMIT = 2;
var HTTP_TIMEOUT_MS = 12000;   // under the watch's 15 s, so the watch hears "No internet"

function http(method, url, headers, body, cb) {
  var xhr = new XMLHttpRequest(), done = false;
  function finish(err) {
    if (done) return;
    done = true;
    if (err || !xhr.status) return cb(new Error('network'));
    var json = null;
    try { json = JSON.parse(xhr.responseText); } catch (e) { /* non-JSON error page */ }
    cb(null, xhr.status, json);
  }
  xhr.open(method, url, true);
  xhr.timeout = HTTP_TIMEOUT_MS;
  for (var h in headers) xhr.setRequestHeader(h, headers[h]);
  xhr.onload = function () { finish(false); };
  xhr.onerror = xhr.ontimeout = xhr.onabort = function () { finish(true); };
  xhr.send(body ? JSON.stringify(body) : null);
}

function playerIdentifier() {
  var token = '';
  try { token = Pebble.getAccountToken() || ''; } catch (e) { /* not offered by this phone app */ }
  if (token) return token;
  var id = localStorage.getItem('ll_pid');
  if (!id) {
    id = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
      var r = Math.random() * 16 | 0;
      return (c === 'x' ? r : (r & 3 | 8)).toString(16);
    });
    localStorage.setItem('ll_pid', id);
  }
  return id;
}

var client = null;
function getClient() {
  if (!client) client = ll.createClient({ http: http, store: localStorage, config: config,
                                          playerIdentifier: playerIdentifier() });
  return client;
}

function reply(seq, r) {
  var msg = { SEQ: seq, STATUS: r.status };
  if (r.status === 0) {
    msg.TOTAL = r.total || 0;
    msg.ROWS = (r.rows || []).join('\n');
    if (r.me !== undefined) msg.ME = r.me;
    if (r.rank !== undefined) msg.RANK = r.rank;
    if (r.online) { msg.ON_NIGHTS = r.online.nights; msg.ON_SCORE = r.online.score; }
  }
  Pebble.sendAppMessage(msg, null, function () { console.log('leaderboard reply not delivered'); });
}

// Local only: tells the watch it can send requests now. No network here.
// Retried a few times: the watch holds every request until READY arrives.
var READY_TRIES = 5, READY_RETRY_MS = 1000;
function sendReady(left) {
  Pebble.sendAppMessage({ READY: 1 }, null, function () {
    if (left > 1) setTimeout(function () { sendReady(left - 1); }, READY_RETRY_MS);
  });
}
Pebble.addEventListener('ready', function () { sendReady(READY_TRIES); });

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload, seq = p.SEQ, name = p.NAME || '';
  var done = function (r) { reply(seq, r); };
  if (p.REQ === REQ_TABLE) getClient().table(p.DIFF, name, done);
  else if (p.REQ === REQ_SUBMIT) getClient().submit(p.DIFF, p.NIGHTS, p.SCORE, name, done);
});
