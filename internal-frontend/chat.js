var util = require('./util');
var express = require('express');
var passport = require('passport');
var FacebookStrategy = require('passport-facebook').Strategy;
var app = express();
var bodyParser = require('body-parser');

var whitelist = {"123":true,};
var session = {};

app.set('view engine', 'ejs');

// app.use(express.static('public'));
app.use(express.cookieParser());
app.use(bodyParser.urlencoded({ extended: false }));
app.use(express.session({ secret: 'J' }));
app.use(passport.initialize());
app.use(passport.session());
app.use(app.router);

// route middleware to make sure a user is logged in
function isLoggedIn(req, res, next) {
  if (req.isAuthenticated()) return next();
  res.redirect('/chat/auth/facebook');
}

function isAuthorized(req, res, next) {
  if (req.isAuthenticated()) {
    if (whitelist[req.user.id]) return next();
    res.render('unauthorized', req.user);
  } else {
    res.redirect('/chat/auth/facebook');
  }
}

passport.serializeUser(function(user, done) {
  // console.log(user);

  if (!rooms['kpo'].whos_here.hasOwnProperty(user.id)) {
    session_new_user(user, 'kpo');
    session_login(user);
    console.log("New USER: " + JSON.stringify(user));
  }

  session[user.id] = user;
  done(null, user.id);
});

passport.deserializeUser(function(id, done) {
  done(null, session[id]);
});

passport.use(new FacebookStrategy({
    clientID: 11,
    clientSecret: '8',
    callbackURL: "http://another-host/chat/auth/facebook/callback"
  },
  function(accessToken, refreshToken, profile, done) {
    var fbid = profile.id;
    var fbname = profile.displayName;
    done(null, { id: fbid, name: fbname });
    // User.findOrCreate(..., function(err, user) {
    //   if (err) { return done(err); }
    //   done(null, user);
    // });
  }
));

// Redirect the user to Facebook for authentication.  When complete,
// Facebook will redirect the user back to the application at
//     /chat/auth/facebook/callback
app.get('/chat/auth/facebook', passport.authenticate('facebook'));

// Facebook will redirect the user to this URL after approval.  Finish the
// authentication process by attempting to obtain an access token.  If
// access was granted, the user will be logged in.  Otherwise,
// authentication has failed.
app.get('/chat/auth/facebook/callback', 
  passport.authenticate('facebook', { successRedirect: '/chat',
                                      failureRedirect: '/chat/login' }));

// app.get('/enter', isLoggedIn, function (req, res) {
//   res.render('enter', {});
// });

var config = { first_poll: { kpo: 0 }, };

function render_index(res, params) { res.render('chat.ejs', {}); }

app.get('/chat', isAuthorized, function (req, res) { render_index(res, {error_message: ''}, config); });

app.get('/chat/state', isAuthorized, function (req, res) {
  res.render('state', { session: session, whitelist: whitelist, rooms: rooms });
});

app.get('/chat/ok', isAuthorized, function (req, res) { res.send('{}'); });
app.get('/chat/nok', function (req, res) { res.send('{ "error": "login" }'); });

app.get('/chat/auth/facebook/login/:id', function(req,res,next) {
  passport.authenticate('facebook', {
    callbackURL: '/chat/auth/facebook/login_callback/' + req.params.id,
  })(req,res,next);
});

app.get('/chat/auth/facebook/login_callback/:id', function(req,res,next) {
  passport.authenticate('facebook', {
    callbackURL: '/chat/auth/facebook/login_callback/' + req.params.id,
    successRedirect: '/chat/ok',
    failureRedirect: '/chat/nok',
  })(req,res,next);
});

app.get('/chat/whitelist/:fbid', isLoggedIn, function (req, res) {
  if (req.user.id == '123') {
    whitelist[req.params.fbid] = true;
    res.send('ok');
  } else {
    res.send('fail');
  }
});

app.listen(8082);

app.get('/chat/whos_here/:room', isAuthorized, function (req, res) {
  // res.send(session_whos_here(req.params.room));
  if (rooms[req.params.room]) {
    res.send(rooms[req.params.room].whos_here);
  } else {
    res.send('{}');
  }
});

app.get('/chat/chats/:room', isAuthorized, function (req, res) {
  get_chats(req.params.room, function (chats) { res.send(chats); });
});

app.get('/chat/poll/:ids', isAuthorized, function (req, res) {
  var ids = JSON.parse(req.params.ids);
  var room = ids.hasOwnProperty('kpo') ? 'kpo' : false;
  if (!room) return res.send('invalid room');

  session_get_user(req.user.id, room, req.ip, req.get('referer'), req.get('user-agent'));
  util.longpoll(ids, function (msgs) {
    res.send({ id: req.user.id, msgs: msgs });
  });
});

function post_chat(room, ip, uid, uname, msg, cb) {
  util.chat_post(room, ip, uid, uname, msg, function(res) {
    console.log('chat_post[%s] = ip:%s, uid:%s, uname:%s, msg:%s', room, ip, uid, uname, msg);
    var token = { post_ts: new Date().getTime(), userid: uid, uname: uname, message: msg };
    util.chat_message(room, token);
    if (cb) cb(res, token);
  });
}

// TODO: cleanup
var last_chat_post = '';
app.post('/chat/post', isAuthorized, function (req, res) {
  var user = {
    id: req.user.id,
    name: req.user.name,
    room: 'kpo',
  };
  if (!user.id || !user.room || !req.body.text) { // If user has id then he/she has logged in.
    res.send('need login');
    return post_chat(user.room, req.ip, 0, '- ? -', req.body.text);
  }
  var token = user.room + ';' + req.ip + ';' + user.id + ';' + user.name + ';' + req.body.text;
  if (token == last_chat_post) {
    console.error('duplicate post: ' + token);
    res.send('ok');
  } else {
    last_chat_post = token;
    post_chat(user.room, req.ip, user.id, user.name, req.body.text, function (ok, token) {
      res.send(ok ? 'ok' : 'fail');
    });
  }
});








function ChatRoom(max_messages) {
  this.whos_here = {                   // Filtered list of connected users.
    'server' : new Date().getTime(),   // The start time of the server.
    'count' : 0,                       // The number of users.
  };
}

ChatRoom.prototype.new_user = function () { this.whos_here.count++; };

ChatRoom.prototype.delete_user = function () { this.whos_here.count--; };

ChatRoom.prototype.login = function (user) {
  if (!this.whos_here.hasOwnProperty(user.id)) {
    this.whos_here[user.id] = user;
    user.count = 0;
  }
  user.count++;
};

// Returns the number of logged in sessions remaining for the specified id.
ChatRoom.prototype.logout = function (id, delay_delete) {
  if (!this.whos_here.hasOwnProperty(id)) return 0;
  if (--this.whos_here[id].count == 0) {
    // Cleanup if the user does not re-connect in delay_delete seconds.
    setTimeout(remove_id(this.whos_here, id), delay_delete);
  }
  return this.whos_here[id].count;
};

function remove_id(whos_here, id) {
  return function () {
    if (whos_here[id] && whos_here[id].count == 0)
      delete whos_here[id];
  }
}

var rooms = {
  'kpo': new ChatRoom(200),
};

// Guarantees that newly added user has a valid room.
function session_new_user(user, room) {
  user.room = room;
  if (!rooms[user.room]) { console.error('Invalid room: ' + JSON.stringify(user)); return false; }
  user.since = user.last_contact = new Date().getTime();
  rooms[user.room].new_user();
  return user;
}

function session_whos_here(room) {
  var ret = {};
  if (!rooms[room]) return ret;
  if (!rooms[room].whos_here) return ret;
  var w = rooms[room].whos_here;
  var now = new Date().getTime();
  for (var id in w) if (w.hasOwnProperty(id)) {
    if (id == 'count' || id == 'server') continue;
    var u = w[id];
    if (now - u.last_contact < 3 * 60000) {
      ret[id] = { id: u.id, name: u.name, since: u.since, count: u.count };
    } else {
      // console.log("too long " + JSON.stringify(u));
    }
  }
  ret.server = w.server;
  ret.count = w.count;
  // console.log("wh = " + JSON.stringify(ret));
  return ret;
}

// Accessing this method will update the user's last_contact.
function session_get_user(id, room, ip, referer, ua) {
  if (rooms[room] && rooms[room].whos_here.hasOwnProperty(id)) {
    rooms[room].whos_here[id].last_contact = new Date().getTime();
    // console.log('touch ' + JSON.stringify(rooms[room].whos_here[id]));
  }
  // console.log('tocing ' + room + ' ' + rooms[room].whos_here[id]);
}

function session_login(user) {
  rooms[user.room].login(user); // The room is guaranteed to be valid.
}

function session_logout(user) {
  rooms[user.room].logout(user.id, 1); // The room is guaranteed to be valid.
}

function session_admin_state() {
  return { session: session, rooms: rooms };
}
