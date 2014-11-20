How to publish the extract:
---------------------------
1. Dump C1 database
2. While waiting, copy index.dabcppln.rXXX.raw.html to index.dabcppln.rYYY.raw.html
   where YYY = XXX + 1
3. When dump C1 is finished, run
     python combine.py dumpName rYYY dumps_timestamp
4. For sanity sake, load the resulting index.dabcppln.rYYY.html on the browser first!
5. Then zip index.dabcppln.rYYY.html
6. Check the zip file MD5 sum
7. Upload to mega.co.nz and mediafire.com
   - Use kp email address, with pass of kp + " Mega" or " Mediafire"
8. Change index.html of kp with the right links + sum + timestamp (plus 7 hours!)
9. Deploy index.html
