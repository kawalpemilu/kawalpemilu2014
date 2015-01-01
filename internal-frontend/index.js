var http = require('http');
var express = require('express');
var passport = require('passport');
var FacebookStrategy = require('passport-facebook').Strategy;

var app = express();

app.set('view engine', 'ejs');

app.configure(function() {
  // app.use(express.static('public'));
  app.use(express.cookieParser());
  // app.use(express.bodyParser());
  app.use(express.session({ secret: 'J' }));
  app.use(passport.initialize());
  app.use(passport.session());
  app.use(app.router);
});

// route middleware to make sure a user is logged in
function isLoggedIn(req, res, next) {
  if (req.isAuthenticated()) return next();
  res.redirect('/auth/facebook');
}

function isAuthorized(req, res, next) {
  if (req.isAuthenticated()) {
    if (whitelist[req.user.id]) return next();
    res.render('unauthorized', req.user);
  } else {
    res.redirect('/auth/facebook');
  }
}

var session = {};
var users = {
  '123': { id: '123', name: 'Felix Halim' },
};

passport.serializeUser(function(user, done) {
  console.log(user);
  session[user.id] = user;
  done(null, user.id);
});

passport.deserializeUser(function(id, done) {
  done(null, session[id]);
});

passport.use(new FacebookStrategy({
    clientID: 11,
    clientSecret: '8',
    callbackURL: "http://the-host/auth/facebook/callback"
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
//     /auth/facebook/callback
app.get('/auth/facebook', passport.authenticate('facebook'));

// Facebook will redirect the user to this URL after approval.  Finish the
// authentication process by attempting to obtain an access token.  If
// access was granted, the user will be logged in.  Otherwise,
// authentication has failed.
app.get('/auth/facebook/callback', 
  passport.authenticate('facebook', { successRedirect: '/',
                                      failureRedirect: '/login' }));

// app.get('/enter', isLoggedIn, function (req, res) {
//   res.render('enter', {});
// });

app.get('/submit_tps/:idx/:prabowo/:jokowi/:valid/:invalid', isAuthorized, function (req, res) {
  submit_tps(req.params.idx, req.params.prabowo, req.params.jokowi, req.params.valid, req.params.invalid, req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/submit_tps_rel/:parent/:tps_no/:prabowo/:jokowi/:valid/:invalid', isAuthorized, function (req, res) {
  submit_tps_rel(req.params.parent, req.params.tps_no, req.params.prabowo, req.params.jokowi, req.params.valid, req.params.invalid, req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/submit_tps3_rel/:parent/:tps_no/:a1/:a2/:a3/:a4/:b1/:b2/:b3/:b4/:ii1/:ii2/:ii3/:ii4', isAuthorized, function (req, res) {
  submit_tps3_rel(req.params.parent, req.params.tps_no,
      req.params.a1, req.params.a2, req.params.a3, req.params.a4,
      req.params.b1, req.params.b2, req.params.b3, req.params.b4,
      req.params.ii1, req.params.ii2, req.params.ii3, req.params.ii4,
      req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/fix_error3/:parent/:tps_no/:a1/:a2/:a3/:a4/:b1/:b2/:b3/:b4/:ii1/:ii2/:ii3/:ii4', isAuthorized, function (req, res) {
  fix_error3(req.params.parent, req.params.tps_no,
      req.params.a1, req.params.a2, req.params.a3, req.params.a4,
      req.params.b1, req.params.b2, req.params.b3, req.params.b4,
      req.params.ii1, req.params.ii2, req.params.ii3, req.params.ii4,
      req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/fix_error/:parent/:tps_no/:prabowo/:jokowi/:valid/:invalid', isAuthorized, function (req, res) {
  fix_error(req.params.parent, req.params.tps_no, req.params.prabowo, req.params.jokowi, req.params.valid, req.params.invalid, req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/report/:parent/:tps', isAuthorized, function (req, res) {
  report(req.params.parent, req.params.tps, req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/report3/:parent/:tps', isAuthorized, function (req, res) {
  report3(req.params.parent, req.params.tps, req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/report_h3/:parent/:tps', isAuthorized, function (req, res) {
  report_h3(req.params.parent, req.params.tps, req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/report_h4/:parent/:tps', isAuthorized, function (req, res) {
  report_h4(req.params.parent, req.params.tps, req.user.id, function (data) {
    res.send(data);
  });
});

app.get('/whitelist/:fbid', isLoggedIn, function (req, res) {
  if (req.user.id == '123') {
    // Super admin >:)
    whitelist[req.params.fbid] = true;
    res.send('ok');
  } else {
    res.send('fail');
  }
});

app.get('/', isAuthorized, function (req, res) { res.render('index', {}); });

app.get('/login', function (req, res) { res.send('<a href="/auth/facebook">login</a>'); });

app.listen(8080);



function xhr(path, cb) {
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
}

function report(parent, tps, fbid, cb) {
  xhr('/api/report/' + parent + '/' + tps + '/' + fbid + '/1234', cb); };

function report3(parent, tps, fbid, cb) {
  xhr('/api/report3/' + parent + '/' + tps + '/' + fbid + '/1234', cb); };

function report_h3(parent, tps, fbid, cb) {
  xhr('/api/report_h3/' + parent + '/' + tps + '/' + fbid + '/1234', cb); };

function report_h4(parent, tps, fbid, cb) {
  xhr('/api/report_h4/' + parent + '/' + tps + '/' + fbid + '/1234', cb); };

function submit_tps(idx, prabowo, jokowi, valid, invalid, fbid, cb) {
  xhr('/api/submit_tps/' + idx + '/' + prabowo + '/' + jokowi + '/' + valid + '/' + invalid +
    '/' + fbid + '/1234', cb); };

function submit_tps_rel(parent, tps_no, prabowo, jokowi, valid, invalid, fbid, cb) {
  xhr('/api/submit_tps_rel/' + parent + '/' + tps_no + '/' + prabowo + '/' + jokowi + '/' + valid + '/' + invalid +
    '/' + fbid + '/1234', cb); };

function submit_tps3_rel(parent, tps_no, a1, a2, a3, a4, b1, b2, b3, b4, ii1, ii2, ii3, ii4, fbid, cb) {
  xhr('/api/submit_tps3_rel/' + parent + '/' + tps_no +
    '/' + a1 + '/' + a2 + '/' + a3 + '/' + a4 +
    '/' + b1 + '/' + b2 + '/' + b3 + '/' + b4 +
    '/' + ii1 + '/' + ii2 + '/' + ii3 + '/' + ii4 +
    '/' + fbid + '/1234', cb); };

function fix_error(parent, tps_no, prabowo, jokowi, valid, invalid, fbid, cb) {
  xhr('/api/fix_error/' + parent + '/' + tps_no + '/' + prabowo + '/' + jokowi + '/' + valid + '/' + invalid +
    '/' + fbid + '/1234', cb); };

function fix_error3(parent, tps_no, a1, a2, a3, a4, b1, b2, b3, b4, ii1, ii2, ii3, ii4, fbid, cb) {
  xhr('/api/fix_error3/' + parent + '/' + tps_no +
    '/' + a1 + '/' + a2 + '/' + a3 + '/' + a4 +
    '/' + b1 + '/' + b2 + '/' + b3 + '/' + b4 +
    '/' + ii1 + '/' + ii2 + '/' + ii3 + '/' + ii4 +
    '/' + fbid + '/1234', cb); };

var whitelist = {
  "123":true,
};
