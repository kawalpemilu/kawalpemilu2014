from google.appengine.ext import ndb
from google.appengine.api import urlfetch
from google.appengine.api import memcache
from datetime import datetime
from datetime import timedelta
from blacklist import is_blacklisted
import webapp2

URL_BASE = 'http://dummy.kawalpemilu.org'
CACHE_TIMEOUT = 300
USE_DB = False

class Cache(ndb.Model):
    path = ndb.StringProperty(indexed=True)
    content = ndb.StringProperty(indexed=False)
    date = ndb.DateTimeProperty(auto_now_add=True, indexed=False)

class MainPage(webapp2.RequestHandler):
    def return_error(self):
        self.response.headers['Content-Type'] = 'application/json'
        self.response.out.write('Error')
        
    def return_response(self, content):
        self.response.headers['Content-Type'] = 'application/json'
        self.response.headers['Access-Control-Allow-Origin'] = '*'
        self.response.out.write(content)
        
    def get(self, path):
        if (is_blacklisted(self.request)):
            return self.return_error();
        if (len(path)>20) or (path[:14] != '/api/children/' and path[:9] != '/api/tps/'):
            return self.return_error()

        expiry_boundary = datetime.now() - timedelta(seconds=CACHE_TIMEOUT)

        # Memcache
        memcache_client = memcache.Client();
        memcache_entry = memcache_client.get(path)
        if memcache_entry is not None:
            self.response.headers['X-Cache'] = 'HIT-M'
            return self.return_response(memcache_entry)

        # Check in DB
        if USE_DB:
            cache_entries = Cache.query(Cache.path==path).fetch(1)
            cache_entry = None
            if (len(cache_entries) == 1) and (cache_entries[0].date >= expiry_boundary):
                cache_entry = cache_entries[0]
                self.response.headers['X-Cache'] = 'HIT-D'
                self.return_response(cache_entries[0].content)
                memcache_client.set(path, cache_entry.content,
                                    (cache_entries[0].date + timedelta(seconds=CACHE_TIMEOUT) - datetime.now()).seconds)
                return;
            
        # Go fetch!
        url = URL_BASE + path
        result = urlfetch.fetch(url)
        if result.status_code == 200:
            self.response.headers['Content-Type'] = 'application/json'
            self.response.headers['Access-Control-Allow-Origin'] = '*'
            self.response.out.write(result.content)
            if USE_DB:
                # Save to DB
                if not cache_entry:
                    cache_entry = Cache(path=path)
                cache_entry.content = result.content
                cache_entry.put()
            memcache_client.set(path, result.content, CACHE_TIMEOUT)
            
        else:
            return self.return_error()
 
app = webapp2.WSGIApplication([('(.*)', MainPage)], debug=True)
