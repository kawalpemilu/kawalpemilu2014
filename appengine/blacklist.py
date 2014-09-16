# Blacklist for unwelcome referer or origin.
BLACKLIST = []

def is_blacklisted(request):
    if (request.referer):
        for entry in BLACKLIST:
            if (request.referer.find(entry) > -1):
                return True
    else:
        return False