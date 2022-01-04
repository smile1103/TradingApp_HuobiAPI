import hmac
import hashlib
import json
import requests
import time

API_KEY = "NfMdDn3wpZWKL4EJdnW4xg"
SECRET_KEY = "dnopcDjFWzYVkTZdThSUdt"

n = int(time.time()* 1000)
req = {
 "id": n,
 "method": "private/create-order",
 "api_key" : API_KEY,
 "params": {
   "instrument_name": "BTC_USDT",
   "side": "BUY",
   "type": "LIMIT",
   "price": 31100.12,
   "quantity": 0.0001,
   "client_oid": "my_order_0002",
   "time_in_force": "GOOD_TILL_CANCEL",
   "exec_inst": "POST_ONLY"
 },
 "nonce": n
}
# First ensure the params are alphabetically sorted by key
paramString = ""

if "params" in req:
  for key in sorted(req['params']):
    paramString += key
    paramString += str(req['params'][key])

sigPayload = req['method'] + str(req['id']) + req['api_key'] + paramString + str(req['nonce'])
print(sigPayload)
req['sig'] = hmac.new(
  bytes(str(SECRET_KEY), 'utf-8'),
  msg=bytes(sigPayload, 'utf-8'),
  digestmod=hashlib.sha256
).hexdigest()
print(req)
res = requests.post('https://api.crypto.com/v2/private/create-order', json=req)
print(res.request.headers)
print(res.request.body)
print(res.json())