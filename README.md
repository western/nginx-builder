# Further than nginx

Latest Nginx with steroids

based on https://gist.github.com/western/c04efe49745f24874c43

## synopsis

* download release of package https://github.com/western/nginx-builder/releases
* tar xf nginx-builder-x.x.tar.gz
* ./install

## command only

* wget https://raw.githubusercontent.com/western/nginx-builder/dev/nginx_builder
* chmod +x nginx_builder
* make code fit
```bash
IS_LOCAL=1|0 - const for locate folder to build
IS_PAUSED=1|0 - program wait for info board
IS_GET_ONLY=1|0 - do make and compile after download
```
* ./nginx_builder

## fixed versions

* nginx 1.18.0
* openresty.org/download/drizzle7-2011.07.21.tar.gz
drizzle7-2011.07.21.tar.gz
* github.com/openssl/openssl/archive/OpenSSL_1_1_1g.tar.gz
1.1.1g

## new

* nginx-ts-module
* wrk2
* lua-nginx-module up to v0.10.16rc5

## sources

* github.com/openresty/sregex.git
v0.0.1
* github.com/giltene/wrk2.git

* github.com/openresty/memc-nginx-module.git
v0.19
* github.com/openresty/lua-nginx-module.git
v0.10.16rc5
* github.com/simplresty/ngx_devel_kit.git
v0.3.1
* github.com/openresty/redis2-nginx-module.git
v0.15
* github.com/openresty/echo-nginx-module.git
v0.62rc1
* github.com/calio/form-input-nginx-module.git
v0.12
* github.com/openresty/set-misc-nginx-module.git
v0.32
* github.com/Austinb/nginx-upload-module.git
2.2.0
* github.com/FRiCKLE/ngx_cache_purge.git
2.3
* github.com/openresty/headers-more-nginx-module.git
v0.33
* github.com/nbs-system/naxsi.git
0.56
* github.com/SpiderLabs/ModSecurity-nginx.git
v1.0.1
* github.com/openresty/replace-filter-nginx-module.git
v0.01rc5
* github.com/openresty/rds-json-nginx-module.git
v0.15
* github.com/openresty/rds-csv-nginx-module.git
v0.09
* github.com/openresty/drizzle-nginx-module.git
v0.1.11
* github.com/openresty/ngx_postgres.git
1.0
* github.com/nginx/njs.git
0.4.0
* github.com/openresty/stream-lua-nginx-module.git
v0.0.7
* github.com/openresty/xss-nginx-module.git
v0.06
* github.com/arut/nginx-rtmp-module.git
v1.2.1
* github.com/arut/nginx-ts-module.git
v0.1.1
* github.com/openresty/luajit2.git
v2.1-20200102
* github.com/openresty/lua-resty-core.git
v0.1.17
* github.com/openresty/lua-resty-lrucache.git
v0.09
* github.com/openresty/lua-cjson.git
2.1.0.7
* github.com/openresty/lua-resty-redis.git
v0.28rc1
* github.com/cloudflare/lua-resty-cookie.git
v0.1.0
* github.com/openresty/lua-resty-mysql.git
v0.22
* github.com/openresty/lua-ssl-nginx-module.git
v0.01rc3
* github.com/openresty/lua-resty-signal.git
v0.02
* github.com/openresty/lua-tablepool.git
v0.01
* github.com/openresty/lua-resty-shell.git
v0.02
* github.com/openresty/lua-resty-limit-traffic.git
v0.06
* github.com/openresty/lua-resty-lock.git
v0.08
* github.com/openresty/lua-resty-string.git
v0.12rc1
* github.com/openresty/lua-resty-upload.git
v0.10
* github.com/openresty/lua-resty-websocket.git
v0.07
* github.com/openresty/lua-resty-upstream-healthcheck.git
v0.06


## generate ssl keys

openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout localhost.key -out localhost.crt

### make Diffie Hellman key

openssl dhparam -out dhparam.pem 4096

