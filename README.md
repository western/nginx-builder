# nginx-builder
Last Nginx with steroids

based on https://gist.github.com/western/c04efe49745f24874c43

## fixed versions
* nginx 1.16.1

* github.com/openresty/memc-nginx-module.git
v0.19
* github.com/chaoslawful/lua-nginx-module.git
v0.10.15
* github.com/simplresty/ngx_devel_kit.git
v0.3.1
* github.com/openresty/redis2-nginx-module.git
v0.15
* github.com/openresty/echo-nginx-module.git
v0.61
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
* https://agentzh.org/misc/nginx/drizzle7-2011.07.21.tar.gz
drizzle7-2011.07.21.tar.gz
* github.com/openresty/ngx_postgres.git
1.0
* github.com/openresty/luajit2.git
v2.1-20190912
* github.com/openresty/lua-resty-core.git
v0.1.17
* github.com/openresty/lua-resty-lrucache.git
v0.09
* github.com/openresty/lua-cjson.git
2.1.0.7
* github.com/openresty/lua-resty-redis.git
v0.27
* github.com/cloudflare/lua-resty-cookie.git
v0.1.0
* github.com/openresty/lua-resty-mysql.git
v0.21
* github.com/openresty/lua-ssl-nginx-module.git
v0.01rc3

## new modules

* drizzle-nginx-module.git/
* drizzle7-2011.07.21/
* nginx-1.16.1.tar.gz
* ngx_postgres.git/
* opt/lua-resty-mysql.git/
* opt/lua-ssl-nginx-module.git/
* replace-filter-nginx-module.git/
* sregex.git

## synopsis
* download release of package
* unpack
* edit ngx_install script for IS_LOCAL variable
* run ./ngx_install
