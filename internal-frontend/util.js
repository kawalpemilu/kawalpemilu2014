var http = require('http');

function escape_html(text) {
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/\\/g, '&#92;')
    .replace(/\//g, '&#47;')
    .replace(/'/g, '&#39;');
}

exports.safe_json = function (text) {
  return '"' + escape_html(JSON.stringify(text)) + '"';
};

exports.xhr = function (path, cb) {
  var options = {
    hostname: '127.0.0.1',
    port: 8000,
    method: 'GET',
    path: escape(path)
  };

  console.log('xhr: ' + path);
  var req = http.request(options, function(res) {
    res.setEncoding('utf8');
    var s = '';
    res.on('data', function (chunk) { s += chunk; });
    res.on('end', function () {
      if (!cb) {
        console.log('NULL cb, ' + s.substring(0,100));
        return;
      }
      try {
        cb(JSON.parse(s));
      } catch (e) {
        console.error('XHR: ' + path + '\nERR: ' + e + "\nSNIP: " + s.substring(0, 50));
        cb({});
      }
      cb = null;
    });
  });

  req.on('error', function(e) {
    console.log('problem with request: ' + e.message);
    if (cb) { cb({}); cb = null; }
  });

  req.end();
};



var mysql      = require('mysql');
var pool = mysql.createPool({
  connectionLimit : 10,
  host     : 'zzz',
  user     : 'zzz',
  password : 'zzz',
  database : 'zzz',
});

function query(sql, values, cb) {
  pool.getConnection(function(err, connection) {
    if (err) {
      console.error('Connection error: ' + err);
      return cb([], []);
    }
    connection.query(sql, values, function(err, rows, fields) {
      if (err) {
        console.error('SQL error: ' + sql
          + '\nValues: ' + JSON.stringify(values) + '\n' + err);
        cb([], []);
      } else {
        cb(rows, fields);
      }
      connection.release();
    });
  });
}

exports.chat_get = function (room, min_id, limit, cb) {
  query('\
    SELECT id, UNIX_TIMESTAMP(post_ts) * 1000 as post_ts, userid, uname, message \
      FROM chat_messages WHERE room = ? AND id >= ? \
     ORDER BY id DESC LIMIT ?',
     [room, min_id, Math.max(1, Math.min(500, limit))], cb);
};

exports.chat_post = function (room, ip_address, userid, uname, message, cb) {
  query('INSERT INTO chat_messages(room, ip_address, userid, uname, message)\
    VALUES (?, ?, ?, ?, ?)',
    [room, ip_address, userid, uname, message], cb);
};




function Queue(type_max_lengths) {
  this.type_max_lengths = type_max_lengths;
  this.queue = {};
  for (var id in type_max_lengths)
    if (type_max_lengths.hasOwnProperty(id))
      this.queue[id] = [];
}

// Get messages with id at least the specified id.
Queue.prototype.get = function (ids) {
  var ret = {}, cnt = 0;
  for (var id in ids) if (ids.hasOwnProperty(id)) {
    var arr = this.queue[id];
    if (arr) {
      var a = [];
      var min_id = ids[id];
      for (var i = arr.length - 1; i >= 0; i--) {
        if (arr[i].id <= min_id) break;
        // console.log(i + ' ' + arr[i].id + ' ' + min_id);
        a.push(arr[i]);
      }
      a.reverse();
      cnt += a.length;
      ret[id] = a;
    } else {
      throw new Error('unknown get id ' + id);
    }
  }
  return [cnt, ret];
};

Queue.prototype.add = function (type, msg) {
  if (!msg.id) {
    throw new Error('add without id');
  } else if (this.type_max_lengths[type]) {
    this.queue[type].push(msg);
    if (this.queue[type].length > this.type_max_lengths[type])
      this.queue[type].shift();
  } else {
    throw new Error('type ' + type + ' does not exists.');
  }
};


var queue_config = { kpo: 500, };
var queue = new Queue(queue_config);
var waiters = [];

function message(type, msg) {
  queue.add(type, msg);
  var length = 0;
  for (var i = 0; i < waiters.length; i++) {
    var res = queue.get(waiters[i].ids);
    if (res[0] > 0) {
      waiters[i].cb(res[1]);
    } else if (length < i) {
      waiters[length++] = waiters[i];
    } else {
      length++;
    }
  }
  waiters.length = length;
}

exports.chat_get('kpo', 0, queue_config.kpo, function (res) {
  console.log('chat_get[%s] = %d', 'kpo', res.length);
  for (var i = res.length - 1; i >= 0; i--) chat_message('kpo', res[i]);
});

exports.longpoll = function longpoll(ids, cb) {
  var res = queue.get(ids);
  if (!cb) return res[1]; // This is synchronous call.
  if (res[0] > 0 || waiters.length > 1000) return cb(res[1]);
  waiters.push({ids:ids, cb:cb, since: new Date().getTime() });
};

(function cleanup() {
  var now = new Date().getTime();
  while (waiters.length > 0 && now - waiters[0].since > 55000) {
    waiters[0].cb(queue.get(waiters[0].ids)[1]);
    waiters.shift();
  }
  setTimeout(cleanup, 1000);
})();

var chat_id_setter = (function () {
  var last_id = new Date().getTime();
  return function (msg) {
    msg.id = last_id++;
    return msg;
  };
})();

function chat_message(room, msg) {
  message(room, chat_id_setter(msg));
}

exports.chat_message = chat_message;
