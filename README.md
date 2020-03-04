# nginx-builder
Latest Nginx with steroids

based on https://gist.github.com/western/c04efe49745f24874c43

## synopsis
* download release of package https://github.com/western/nginx-builder/releases
* tar xf nginx-builder-x.x.tar.gz
* ./ngx_install

## new

* rds-csv-nginx-module
* stream-lua-nginx-module
* njs

## fixed versions
* nginx 1.16.1

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
* github.com/openresty/sregex.git
v0.0.1
* github.com/openresty/replace-filter-nginx-module.git
v0.01rc5
* github.com/openresty/drizzle-nginx-module.git
v0.1.11
* github.com/openresty/rds-json-nginx-module.git
v0.15
* github.com/openresty/rds-csv-nginx-module.git
v0.09
* github.com/openresty/ngx_postgres.git
1.0
* github.com/openresty/stream-lua-nginx-module.git
v0.0.8rc2
* github.com/nginx/njs.git
0.3.9
* github.com/openresty/luajit2.git
v2.1-20200102
* github.com/openresty/lua-resty-core.git
v0.1.18rc1
* github.com/openresty/lua-resty-lrucache.git
v0.10rc1
* github.com/openresty/lua-cjson.git
2.1.0.7
* github.com/openresty/lua-resty-redis.git
v0.28rc1
* github.com/cloudflare/lua-resty-cookie.git
v0.1.0
* github.com/openresty/lua-resty-mysql.git
v0.21
* github.com/openresty/lua-ssl-nginx-module.git
v0.01rc3
* openresty.org/download/drizzle7-2011.07.21.tar.gz
drizzle7-2011.07.21.tar.gz
* github.com/openssl/openssl/archive/OpenSSL_1_1_1d.tar.gz
1.1.1d

## generate ssl keys

openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout localhost.key -out localhost.crt

### make Diffie Hellman key

openssl dhparam -out dhparam.pem 4096

